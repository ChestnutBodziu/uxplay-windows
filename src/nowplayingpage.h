#pragma once

#include <QWidget>

class MediaState;
class QLabel;
class QProgressBar;

// "Now playing" page: AirPlay device and current media. Read-only view of
// MediaState.
class NowPlayingPage : public QWidget {
    Q_OBJECT

public:
    explicit NowPlayingPage(const MediaState *media, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void refreshState();
    void refreshTrack();
    void refreshCover();
    void refreshPosition();

    const MediaState *m_media;
    QLabel *m_chip;
    QLabel *m_device;
    QLabel *m_cover;
    QLabel *m_title;
    QLabel *m_artist;
    QLabel *m_album;
    QProgressBar *m_progress;
    QLabel *m_position;
    QLabel *m_duration;
};
