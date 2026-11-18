#pragma once

#include <QWidget>

class LyricsState;
class MediaState;
class OverlaySettings;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;

// "Now playing" page: AirPlay device, current media and (optionally) lyrics.
// Read-only view of MediaState and LyricsState.
class NowPlayingPage : public QWidget {
    Q_OBJECT

public:
    NowPlayingPage(const MediaState *media, const LyricsState *lyrics,
                   const OverlaySettings *settings, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void refreshState();
    void refreshTrack();
    void refreshCover();
    void refreshPosition();
    void refreshLyrics();
    void refreshLyricsLine();
    void refreshLyricsToggle();

    const MediaState *m_media;
    const LyricsState *m_lyrics;
    const OverlaySettings *m_settings;
    QLabel *m_chip;
    QLabel *m_device;
    QLabel *m_cover;
    QLabel *m_title;
    QLabel *m_artist;
    QLabel *m_album;
    QProgressBar *m_progress;
    QLabel *m_position;
    QLabel *m_duration;

    QPushButton *m_lyricsToggle;
    QLabel *m_lyricsHint;
    QStackedWidget *m_cardStack;
    QListWidget *m_lyricsList;
    int m_highlighted = -1;
};
