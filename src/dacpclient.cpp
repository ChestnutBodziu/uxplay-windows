#include "dacpclient.h"

#include "mediastate.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <dns_sd.h>

#include <QDebug>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSocketNotifier>
#include <QUrl>

namespace {
constexpr int kRetryIntervalMs = 4000;
constexpr int kMaxAttempts = 8;
constexpr int kRequestTimeoutMs = 3000;
} // namespace

DacpClient::DacpClient(const MediaState *media, QObject *parent)
    : QObject(parent), m_media(media), m_network(new QNetworkAccessManager(this)) {
    m_retry.setSingleShot(true);
    m_retry.setInterval(kRetryIntervalMs);
    connect(&m_retry, &QTimer::timeout, this, [this]() {
        if (m_status != Status::Resolving) {
            return;
        }
        if (++m_attempts >= kMaxAttempts) {
            cleanupService(m_resolveRef, m_resolveNotifier);
            cleanupService(m_addressRef, m_addressNotifier);
            qInfo() << "[dacp] remote control service not found after"
                    << m_attempts << "attempts";
            setStatus(Status::Failed);
            return;
        }
        beginResolve();
    });
    connect(m_media, &MediaState::dacpChanged, this, &DacpClient::onMediaChanged);
}

DacpClient::~DacpClient() {
    stop();
}

QString DacpClient::unavailableReason() const {
    switch (m_status) {
    case Status::Ready:
        return QString();
    case Status::Resolving:
        return tr("Looking for the device's remote control...");
    case Status::Failed:
        return tr("This device or app does not offer remote control");
    case Status::Unavailable:
        break;
    }
    return tr("Waiting for the device to offer remote control");
}

void DacpClient::setStatus(Status status) {
    if (m_status == status) {
        return;
    }
    m_status = status;
    qInfo() << "[dacp] status:" << status;
    emit statusChanged();
}

void DacpClient::onMediaChanged() {
    const QString id = m_media->dacpId();
    const QString remote = m_media->activeRemote();
    if (id.isEmpty() || remote.isEmpty()) {
        stop();
        m_dacpId.clear();
        m_activeRemote.clear();
        setStatus(Status::Unavailable);
        return;
    }
    if (id == m_dacpId && remote == m_activeRemote &&
        (m_status == Status::Resolving || m_status == Status::Ready)) {
        return;
    }
    m_dacpId = id;
    m_activeRemote = remote;
    m_attempts = 0;
    beginResolve();
}

void DacpClient::stop() {
    m_retry.stop();
    cleanupService(m_resolveRef, m_resolveNotifier);
    cleanupService(m_addressRef, m_addressNotifier);
    m_address.clear();
    m_port = 0;
}

void DacpClient::cleanupService(_DNSServiceRef_t *&ref, QSocketNotifier *&notifier) {
    if (notifier) {
        notifier->setEnabled(false);
        notifier->deleteLater();
        notifier = nullptr;
    }
    if (ref) {
        DNSServiceRefDeallocate(ref);
        ref = nullptr;
    }
}

void DacpClient::beginResolve() {
    cleanupService(m_resolveRef, m_resolveNotifier);
    cleanupService(m_addressRef, m_addressNotifier);
    m_address.clear();
    setStatus(Status::Resolving);

    // Seen in a real session: the DACP-ID header was 15 hex digits (leading zero
    // dropped) while the advertised service name has 16. Try the zero-padded
    // name first, then the id exactly as received, alternating on retries.
    const QString padded = m_dacpId.rightJustified(16, QLatin1Char('0'));
    const QString chosen = (m_attempts % 2 == 0) ? padded : m_dacpId;
    const QByteArray name = ("iTunes_Ctrl_" + chosen).toUtf8();
    qInfo().noquote() << "[dacp] resolving" << name << "attempt" << (m_attempts + 1);
    const DNSServiceErrorType error = DNSServiceResolve(
        &m_resolveRef, 0, kDNSServiceInterfaceIndexAny, name.constData(),
        "_dacp._tcp", "local.", &DacpClient::resolveCallback, this);
    if (error != kDNSServiceErr_NoError) {
        m_resolveRef = nullptr;
        qWarning() << "[dacp] DNSServiceResolve failed:" << error;
    } else {
        m_resolveNotifier = new QSocketNotifier(
            qintptr(DNSServiceRefSockFD(m_resolveRef)), QSocketNotifier::Read, this);
        connect(m_resolveNotifier, &QSocketNotifier::activated, this, [this]() {
            if (m_resolveRef) {
                DNSServiceProcessResult(m_resolveRef);
            }
        });
    }
    m_retry.start();
}

// dns_sd callbacks run inside DNSServiceProcessResult. Deallocating the ref
// from within its own callback is not allowed, so work is posted to the event loop.
void DacpClient::resolveCallback(_DNSServiceRef_t *, uint32_t, uint32_t,
                                 int32_t error, const char *, const char *hosttarget,
                                 uint16_t port, uint16_t, const unsigned char *,
                                 void *context) {
    auto *self = static_cast<DacpClient *>(context);
    if (error != kDNSServiceErr_NoError || !hosttarget) {
        const QString what = QStringLiteral("resolve error %1").arg(error);
        QMetaObject::invokeMethod(self, [self, what]() { self->lookupFailed(what); },
                                  Qt::QueuedConnection);
        return;
    }
    const QString host = QString::fromUtf8(hosttarget);
    const quint16 hostPort = ntohs(port);
    QMetaObject::invokeMethod(
        self, [self, host, hostPort]() { self->resolved(host, hostPort); },
        Qt::QueuedConnection);
}

void DacpClient::resolved(const QString &hostTarget, quint16 port) {
    if (m_status != Status::Resolving || m_addressRef) {
        return; // duplicate callback (one per interface) or stale
    }
    qInfo().noquote() << "[dacp] service at" << hostTarget << "port" << port;
    cleanupService(m_resolveRef, m_resolveNotifier);
    m_hostTarget = hostTarget;
    m_port = port;
    startAddressLookup(hostTarget, port);
}

void DacpClient::startAddressLookup(const QString &hostTarget, quint16) {
    const QByteArray host = hostTarget.toUtf8();
    const DNSServiceErrorType error = DNSServiceGetAddrInfo(
        &m_addressRef, 0, kDNSServiceInterfaceIndexAny, kDNSServiceProtocol_IPv4,
        host.constData(), &DacpClient::addressCallback, this);
    if (error != kDNSServiceErr_NoError) {
        m_addressRef = nullptr;
        lookupFailed(QStringLiteral("address lookup error %1").arg(error));
        return;
    }
    m_addressNotifier = new QSocketNotifier(
        qintptr(DNSServiceRefSockFD(m_addressRef)), QSocketNotifier::Read, this);
    connect(m_addressNotifier, &QSocketNotifier::activated, this, [this]() {
        if (m_addressRef) {
            DNSServiceProcessResult(m_addressRef);
        }
    });
}

void DacpClient::addressCallback(_DNSServiceRef_t *, uint32_t flags, uint32_t,
                                 int32_t error, const char *,
                                 const struct sockaddr *address, uint32_t,
                                 void *context) {
    auto *self = static_cast<DacpClient *>(context);
    if (error != kDNSServiceErr_NoError) {
        const QString what = QStringLiteral("address error %1").arg(error);
        QMetaObject::invokeMethod(self, [self, what]() { self->lookupFailed(what); },
                                  Qt::QueuedConnection);
        return;
    }
    if (!(flags & kDNSServiceFlagsAdd) || !address || address->sa_family != AF_INET) {
        return;
    }
    const QHostAddress host(address);
    QMetaObject::invokeMethod(self, [self, host]() { self->addressFound(host); },
                              Qt::QueuedConnection);
}

void DacpClient::addressFound(const QHostAddress &address) {
    if (m_status != Status::Resolving || !m_addressRef) {
        return;
    }
    cleanupService(m_addressRef, m_addressNotifier);
    m_address = address;
    m_retry.stop();
    qInfo().noquote() << "[dacp] device at" << address.toString() << ":" << m_port;
    setStatus(Status::Ready);
}

void DacpClient::lookupFailed(const QString &what) {
    // The retry timer re-attempts; just record the reason.
    qInfo().noquote() << "[dacp]" << what;
}

void DacpClient::sendCommand(const QString &command) {
    if (!canControl()) {
        qInfo().noquote() << "[dacp] command" << command << "ignored:"
                          << unavailableReason();
        return;
    }
    const QUrl url(QStringLiteral("http://%1:%2/ctrl-int/1/%3")
                       .arg(m_address.toString())
                       .arg(m_port)
                       .arg(command));
    QNetworkRequest request(url);
    request.setRawHeader("Active-Remote", m_activeRemote.toUtf8());
    request.setTransferTimeout(kRequestTimeoutMs);

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, command]() {
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError;
        qInfo().noquote() << "[dacp]" << command << "->"
                          << (ok ? "ok" : reply->errorString()) << "(HTTP" << http << ")";
        const QNetworkReply::NetworkError error = reply->error();
        reply->deleteLater();
        emit commandFinished(command, ok);

        // The device may have moved to another port (new playback session).
        const bool unreachable = error == QNetworkReply::ConnectionRefusedError ||
                                 error == QNetworkReply::TimeoutError ||
                                 error == QNetworkReply::HostNotFoundError ||
                                 error == QNetworkReply::RemoteHostClosedError;
        if (unreachable && m_status == Status::Ready) {
            m_attempts = 0;
            beginResolve();
        }
    });
}

void DacpClient::playPause() { sendCommand(QStringLiteral("playpause")); }
void DacpClient::nextTrack() { sendCommand(QStringLiteral("nextitem")); }
void DacpClient::previousTrack() { sendCommand(QStringLiteral("previtem")); }
