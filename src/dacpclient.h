#pragma once

#include <QHostAddress>
#include <cstdint>
#include <QObject>
#include <QString>
#include <QTimer>

class MediaState;
class QNetworkAccessManager;
class QSocketNotifier;

struct _DNSServiceRef_t;
struct sockaddr;

// Remote control of the connected Apple device over DACP (Digital Audio Control
// Protocol): the device advertises "_dacp._tcp" as "iTunes_Ctrl_<DACP-ID>" and
// accepts HTTP GET commands carrying the "Active-Remote" header it sent us
// during the AirPlay session. UxPlay exports both values; MediaState holds them.
//
// A command is only offered when the service was actually found. Nothing is
// pretended: until then canControl() is false and unavailableReason() says why.
class DacpClient : public QObject {
    Q_OBJECT

public:
    enum class Status { Unavailable, Resolving, Ready, Failed };
    Q_ENUM(Status)

    explicit DacpClient(const MediaState *media, QObject *parent = nullptr);
    ~DacpClient() override;

    Status status() const { return m_status; }
    bool canControl() const { return m_status == Status::Ready; }
    QString unavailableReason() const;

    void playPause();
    void nextTrack();
    void previousTrack();

signals:
    void statusChanged();
    void commandFinished(const QString &command, bool ok);

private:
    void onMediaChanged();
    void beginResolve();
    void stop();
    void setStatus(Status status);
    void startAddressLookup(const QString &hostTarget, quint16 port);
    void cleanupService(_DNSServiceRef_t *&ref, QSocketNotifier *&notifier);
    void resolved(const QString &hostTarget, quint16 port);
    void addressFound(const QHostAddress &address);
    void lookupFailed(const QString &what);
    void sendCommand(const QString &command);

    static void resolveCallback(_DNSServiceRef_t *ref, uint32_t flags,
                                uint32_t iface, int32_t error,
                                const char *fullname, const char *hosttarget,
                                uint16_t port, uint16_t txtLen,
                                const unsigned char *txt, void *context);
    static void addressCallback(_DNSServiceRef_t *ref, uint32_t flags,
                                uint32_t iface, int32_t error,
                                const char *hostname,
                                const struct sockaddr *address, uint32_t ttl,
                                void *context);

    const MediaState *m_media;
    QNetworkAccessManager *m_network;
    Status m_status = Status::Unavailable;
    QString m_dacpId;
    QString m_activeRemote;
    QString m_failure;

    QString m_hostTarget;
    quint16 m_port = 0;
    QHostAddress m_address;

    _DNSServiceRef_t *m_resolveRef = nullptr;
    QSocketNotifier *m_resolveNotifier = nullptr;
    _DNSServiceRef_t *m_addressRef = nullptr;
    QSocketNotifier *m_addressNotifier = nullptr;

    QTimer m_retry;
    int m_attempts = 0;
};
