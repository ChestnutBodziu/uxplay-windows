#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include "lyrics.h"

class QNetworkAccessManager;
class QNetworkReply;

// Looks lyrics up on LRCLIB (https://lrclib.net): open, free, no key. Only
// title, artist, album and duration leave the machine, and only when the user
// enabled lyrics. One request chain at a time; a new fetch cancels the
// previous one.
class LyricsProvider : public QObject {
    Q_OBJECT

public:
    enum class Result { Found, NotFound, Error };
    Q_ENUM(Result)

    struct Query {
        QString title;
        QString artist;
        QString album;
        qint64 durationMs = 0;
    };

    explicit LyricsProvider(QObject *parent = nullptr);

    void fetch(quint64 requestId, const Query &query);
    void cancel();

signals:
    void finished(quint64 requestId, LyricsProvider::Result result,
                  const Lyrics &lyrics);

private:
    void getExact();
    void search();
    void handleExact(QNetworkReply *reply);
    void handleSearch(QNetworkReply *reply);
    QNetworkReply *request(const QString &path,
                           const QList<QPair<QString, QString>> &parameters);
    // False when the reply is stale or failed (a 404 is not logged as an error).
    bool acceptReply(QNetworkReply *reply, const char *what);
    void finish(Result result, const Lyrics &lyrics = {});

    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    quint64 m_requestId = 0;
    Query m_query;
    bool m_active = false;
};
