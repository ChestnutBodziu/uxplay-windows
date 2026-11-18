#include "lyricsprovider.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <cmath>
#include <cstdlib>

namespace {
constexpr int kTimeoutMs = 8000;
constexpr qint64 kDurationToleranceMs = 5000;
constexpr int kMinScore = 6;

// "Song (feat. X)" -> "Song"; the exact lookup uses the original title.
QString cleanTitle(QString title) {
    static const QRegularExpression extras(
        QStringLiteral(R"(\s*[\(\[][^\)\]]*(feat|ft\.|with|remix|version|remaster)[^\)\]]*[\)\]])"),
        QRegularExpression::CaseInsensitiveOption);
    title.remove(extras);
    return title.trimmed();
}

// "A, B & C" -> "A"
QString firstArtist(const QString &artist) {
    static const QRegularExpression separators(
        QStringLiteral(R"(\s*(,|&|;| feat\.? | ft\.? | x )\s*)"),
        QRegularExpression::CaseInsensitiveOption);
    return artist.split(separators).value(0).trimmed();
}

// Lowercase, no diacritics: "Kubańczyk" == "kubanczyk".
QString normalized(const QString &text) {
    QString result;
    for (const QChar ch : text.normalized(QString::NormalizationForm_D)) {
        if (ch.category() != QChar::Mark_NonSpacing) {
            result.append(ch.toLower());
        }
    }
    return result.trimmed();
}

Lyrics lyricsFromObject(const QJsonObject &object) {
    if (object.value(QStringLiteral("instrumental")).toBool()) {
        Lyrics lyrics;
        lyrics.instrumental = true;
        lyrics.source = QStringLiteral("LRCLIB");
        return lyrics;
    }
    const QString synced = object.value(QStringLiteral("syncedLyrics")).toString();
    if (!synced.trimmed().isEmpty()) {
        Lyrics lyrics = Lyrics::fromLrc(synced);
        if (!lyrics.isEmpty()) {
            lyrics.source = QStringLiteral("LRCLIB");
            return lyrics;
        }
    }
    const QString plain = object.value(QStringLiteral("plainLyrics")).toString();
    if (!plain.trimmed().isEmpty()) {
        Lyrics lyrics = Lyrics::fromPlain(plain);
        lyrics.source = QStringLiteral("LRCLIB");
        return lyrics;
    }
    return {};
}
} // namespace

LyricsProvider::LyricsProvider(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this)) {}

QNetworkReply *LyricsProvider::request(
    const QString &path, const QList<QPair<QString, QString>> &parameters) {
    QUrl url(QStringLiteral("https://lrclib.net/api/") + path);
    QUrlQuery query;
    for (const auto &parameter : parameters) {
        query.addQueryItem(parameter.first, parameter.second);
    }
    url.setQuery(query);

    QNetworkRequest req(url);
    // LRCLIB asks clients to identify themselves.
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("uxplay-windows (https://github.com/ChestnutBodziu/uxplay-windows)"));
    req.setTransferTimeout(kTimeoutMs);
    return m_network->get(req);
}

void LyricsProvider::cancel() {
    m_active = false;
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void LyricsProvider::fetch(quint64 requestId, const Query &query) {
    cancel();
    m_requestId = requestId;
    m_query = query;
    m_active = true;
    getExact();
}

bool LyricsProvider::acceptReply(QNetworkReply *reply, const char *what) {
    reply->deleteLater();
    if (!m_active || reply != m_reply) {
        return false; // cancelled or superseded
    }
    m_reply = nullptr;
    if (reply->error() != QNetworkReply::NoError) {
        // A 404 just means "no exact match"; not worth a log line.
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 404) {
            qInfo().noquote() << "[lyrics]" << what << "error:" << reply->errorString();
        }
        return false;
    }
    return true;
}

void LyricsProvider::finish(Result result, const Lyrics &lyrics) {
    m_active = false;
    qInfo() << "[lyrics] result:" << result << "lines:" << lyrics.lines.size()
            << (lyrics.synced ? "synced" : "plain") << "source:" << lyrics.source;
    emit finished(m_requestId, result, lyrics);
}

void LyricsProvider::getExact() {
    QList<QPair<QString, QString>> parameters = {
        {QStringLiteral("track_name"), m_query.title},
        {QStringLiteral("artist_name"), m_query.artist},
    };
    if (!m_query.album.isEmpty()) {
        parameters.append({QStringLiteral("album_name"), m_query.album});
    }
    if (m_query.durationMs > 0) {
        parameters.append({QStringLiteral("duration"),
                           QString::number(std::lround(m_query.durationMs / 1000.0))});
    }
    qInfo().noquote() << "[lyrics] lookup:" << m_query.title << "|" << m_query.artist;
    m_reply = request(QStringLiteral("get"), parameters);
    QNetworkReply *reply = m_reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleExact(reply); });
}

void LyricsProvider::handleExact(QNetworkReply *reply) {
    const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (!acceptReply(reply, "lrclib")) {
        if (m_active && http == 404) {
            search(); // not an error: the exact lookup simply has no match
        } else if (m_active) {
            finish(Result::Error);
        }
        return;
    }
    const QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
    const Lyrics lyrics = lyricsFromObject(object);
    if (lyrics.isEmpty() && !lyrics.instrumental) {
        search();
        return;
    }
    finish(Result::Found, lyrics);
}

void LyricsProvider::search() {
    const QString query = (cleanTitle(m_query.title) + QLatin1Char(' ') +
                           firstArtist(m_query.artist)).trimmed();
    qInfo().noquote() << "[lyrics] searching:" << query;
    m_reply = request(QStringLiteral("search"), {{QStringLiteral("q"), query}});
    QNetworkReply *reply = m_reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleSearch(reply); });
}

void LyricsProvider::handleSearch(QNetworkReply *reply) {
    if (!acceptReply(reply, "lrclib search")) {
        if (m_active) {
            finish(Result::Error);
        }
        return;
    }
    const QJsonArray results = QJsonDocument::fromJson(reply->readAll()).array();

    // Prefer synced lyrics, then a duration close to the playing track.
    const QString wantedArtist = normalized(firstArtist(m_query.artist));
    const QString wantedTitle = normalized(cleanTitle(m_query.title));
    Lyrics best;
    int bestScore = -1;
    for (const QJsonValue &value : results) {
        const QJsonObject object = value.toObject();
        const Lyrics candidate = lyricsFromObject(object);
        if (candidate.isEmpty() && !candidate.instrumental) {
            continue;
        }
        int score = 0;
        if (candidate.synced) {
            score += 2;
        }
        if (normalized(cleanTitle(object.value(QStringLiteral("trackName")).toString())) == wantedTitle) {
            score += 3;
        }
        if (normalized(object.value(QStringLiteral("artistName")).toString()).contains(wantedArtist)) {
            score += 3;
        }
        if (m_query.durationMs > 0) {
            const qint64 duration =
                qint64(object.value(QStringLiteral("duration")).toDouble() * 1000);
            if (std::llabs(duration - m_query.durationMs) <= kDurationToleranceMs) {
                score += 4;
            } else {
                score -= 3; // likely a different recording
            }
        }
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    // A match must agree on more than just a synced flag; a low score means we
    // would be showing someone else's song.
    if (bestScore >= kMinScore && (!best.isEmpty() || best.instrumental)) {
        finish(Result::Found, best);
    } else {
        qInfo() << "[lyrics] no confident match (best score" << bestScore << ")";
        finish(Result::NotFound);
    }
}
