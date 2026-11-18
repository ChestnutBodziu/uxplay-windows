#include "nowplayingpage.h"

#include "lyrics.h"
#include "lyricsstate.h"
#include "mediastate.h"
#include "overlaysettings.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int kCoverSize = 168;
constexpr int kCardHeight = 214;

QString formatTime(qint64 ms) {
    const qint64 total = std::max<qint64>(ms, 0) / 1000;
    return QStringLiteral("%1:%2")
        .arg(total / 60)
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}

const QColor kLineDim(0x8a, 0x8a, 0x94);
const QColor kLineCurrent(0xff, 0xff, 0xff);
const QColor kLinePlain(0xd0, 0xd0, 0xd8);
} // namespace

NowPlayingPage::NowPlayingPage(const MediaState *media, const LyricsState *lyrics,
                               const OverlaySettings *settings, QWidget *parent)
    : QWidget(parent), m_media(media), m_lyrics(lyrics), m_settings(settings) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(tr("Now playing"));
    title->setObjectName("pageTitle");
    m_chip = new QLabel;
    m_chip->setObjectName("statusChip");
    m_lyricsToggle = new QPushButton(tr("Lyrics"));
    m_lyricsToggle->setCheckable(true);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_lyricsToggle);
    header->addWidget(m_chip);
    root->addLayout(header);

    m_device = new QLabel;
    m_device->setObjectName("muted");
    root->addWidget(m_device);
    root->addSpacing(8);

    auto *card = new QFrame;
    card->setObjectName("card");
    auto *cardOuter = new QVBoxLayout(card);
    cardOuter->setContentsMargins(0, 0, 0, 0);
    m_cardStack = new QStackedWidget;
    cardOuter->addWidget(m_cardStack);

    // Page 0: cover and track info.
    auto *infoPage = new QWidget;
    auto *cardLayout = new QHBoxLayout(infoPage);
    cardLayout->setContentsMargins(18, 18, 18, 18);
    cardLayout->setSpacing(20);

    m_cover = new QLabel;
    m_cover->setObjectName("cover");
    m_cover->setFixedSize(kCoverSize, kCoverSize);
    m_cover->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_cover, 0, Qt::AlignTop);

    auto *info = new QVBoxLayout;
    m_title = new QLabel;
    m_title->setObjectName("trackTitle");
    m_title->setWordWrap(true);
    m_artist = new QLabel;
    m_album = new QLabel;
    m_album->setObjectName("muted");
    info->addWidget(m_title);
    info->addWidget(m_artist);
    info->addWidget(m_album);
    info->addStretch();

    m_progress = new QProgressBar;
    m_progress->setTextVisible(false);
    m_progress->setRange(0, 1000);
    info->addWidget(m_progress);
    auto *times = new QHBoxLayout;
    m_position = new QLabel;
    m_position->setObjectName("muted");
    m_duration = new QLabel;
    m_duration->setObjectName("muted");
    times->addWidget(m_position);
    times->addStretch();
    times->addWidget(m_duration);
    info->addLayout(times);
    cardLayout->addLayout(info, 1);
    m_cardStack->addWidget(infoPage);

    // Page 1: lyrics.
    m_lyricsList = new QListWidget;
    m_lyricsList->setFrameShape(QFrame::NoFrame);
    m_lyricsList->setSelectionMode(QAbstractItemView::NoSelection);
    m_lyricsList->setFocusPolicy(Qt::NoFocus);
    m_lyricsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_lyricsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_lyricsList->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; } "
        "QListWidget::item { padding: 4px 12px; }"));
    m_cardStack->addWidget(m_lyricsList);
    card->setMinimumHeight(kCardHeight);

    root->addWidget(card);

    m_lyricsHint = new QLabel(
        tr("Lyrics are off. Turn them on in the Overlay settings "
           "(title, artist and album are then sent to lrclib.net)."));
    m_lyricsHint->setObjectName("muted");
    m_lyricsHint->setWordWrap(true);
    root->addWidget(m_lyricsHint);
    root->addStretch();

    connect(m_media, &MediaState::stateChanged, this, &NowPlayingPage::refreshState);
    connect(m_media, &MediaState::deviceChanged, this, &NowPlayingPage::refreshState);
    connect(m_media, &MediaState::stateChanged, this, &NowPlayingPage::refreshTrack);
    connect(m_media, &MediaState::trackChanged, this, &NowPlayingPage::refreshTrack);
    connect(m_media, &MediaState::coverChanged, this, &NowPlayingPage::refreshCover);
    connect(m_media, &MediaState::positionChanged, this, &NowPlayingPage::refreshPosition);
    connect(m_lyrics, &LyricsState::changed, this, &NowPlayingPage::refreshLyrics);
    connect(m_lyrics, &LyricsState::currentLineChanged, this,
            &NowPlayingPage::refreshLyricsLine);
    connect(m_settings, &OverlaySettings::changed, this,
            &NowPlayingPage::refreshLyricsToggle);
    connect(m_lyricsToggle, &QPushButton::toggled, this, [this](bool on) {
        m_cardStack->setCurrentIndex(on ? 1 : 0);
        if (on) {
            refreshLyricsLine();
        }
    });

    refreshState();
    refreshTrack();
    refreshCover();
    refreshPosition();
    refreshLyrics();
    refreshLyricsToggle();
}

void NowPlayingPage::refreshState() {
    QString chip;
    switch (m_media->state()) {
    case MediaState::State::Disconnected:
        chip = tr("Waiting for AirPlay");
        break;
    case MediaState::State::Idle:
        chip = tr("Connected");
        break;
    case MediaState::State::Playing:
        chip = tr("Playing");
        break;
    case MediaState::State::Paused:
        chip = tr("Paused");
        break;
    }
    m_chip->setText(chip);

    if (m_media->deviceName().isEmpty()) {
        m_device->setText(tr("No device connected"));
    } else if (m_media->deviceModel().isEmpty()) {
        m_device->setText(m_media->deviceName());
    } else {
        m_device->setText(QStringLiteral("%1  ·  %2")
                              .arg(m_media->deviceName(), m_media->deviceModel()));
    }
}

void NowPlayingPage::refreshTrack() {
    const bool hasTrack = !m_media->title().isEmpty();
    const MediaState::State state = m_media->state();
    if (hasTrack) {
        m_title->setText(m_media->title());
        m_artist->setText(m_media->artist());
        m_album->setText(m_media->album());
    } else if (state == MediaState::State::Playing || state == MediaState::State::Paused) {
        // Audio is flowing without track information (e.g. video apps).
        m_title->setText(tr("Audio from device"));
        m_artist->setText(m_media->deviceName());
        m_album->setText(tr("No track information"));
    } else {
        m_title->setText(tr("Nothing playing"));
        m_artist->clear();
        m_album->clear();
    }
}

void NowPlayingPage::refreshCover() {
    const QImage &cover = m_media->coverArt();
    if (cover.isNull()) {
        m_cover->setPixmap(QPixmap());
        m_cover->setText(QStringLiteral("♪"));
        return;
    }
    const QPixmap pixmap = QPixmap::fromImage(cover).scaled(
        m_cover->size() * devicePixelRatioF(), Qt::KeepAspectRatioByExpanding,
        Qt::SmoothTransformation);
    m_cover->setText(QString());
    m_cover->setPixmap(pixmap);
    m_cover->setScaledContents(true);
}

void NowPlayingPage::refreshPosition() {
    if (!isVisible()) {
        return; // nothing to paint; the page refreshes when shown again
    }
    const qint64 duration = m_media->durationMs();
    const qint64 position = m_media->positionMs();
    m_progress->setValue(duration > 0 ? int(position * 1000 / duration) : 0);
    m_position->setText(formatTime(position));
    m_duration->setText(duration > 0 ? formatTime(duration) : QString());
}

void NowPlayingPage::refreshLyricsToggle() {
    const bool enabled = m_settings->lyricsEnabled();
    m_lyricsToggle->setVisible(enabled);
    m_lyricsHint->setVisible(!enabled);
    if (!enabled && m_lyricsToggle->isChecked()) {
        m_lyricsToggle->setChecked(false); // returns to the cover page
    }
}

void NowPlayingPage::refreshLyrics() {
    m_lyricsList->clear();
    m_highlighted = -1;

    const auto addMessage = [this](const QString &text) {
        auto *item = new QListWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        item->setForeground(kLineDim);
        item->setFlags(Qt::NoItemFlags);
        m_lyricsList->addItem(item);
    };

    switch (m_lyrics->status()) {
    case LyricsState::Status::Off:
    case LyricsState::Status::Idle:
        addMessage(tr("No lyrics to show"));
        return;
    case LyricsState::Status::Loading:
        addMessage(tr("Loading lyrics..."));
        return;
    case LyricsState::Status::NotFound:
        addMessage(tr("No lyrics found for this track"));
        return;
    case LyricsState::Status::Error:
        addMessage(tr("Lyrics could not be loaded (no connection to lrclib.net?)"));
        return;
    case LyricsState::Status::Ready:
        break;
    }

    const Lyrics &lyrics = m_lyrics->lyrics();
    if (lyrics.instrumental) {
        addMessage(tr("Instrumental"));
        return;
    }
    QFont font = m_lyricsList->font();
    font.setPointSize(12);
    for (const LyricLine &line : lyrics.lines) {
        auto *item = new QListWidgetItem(line.text);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFont(font);
        item->setForeground(lyrics.synced ? kLineDim : kLinePlain);
        item->setFlags(Qt::ItemIsEnabled);
        m_lyricsList->addItem(item);
    }
    if (!lyrics.source.isEmpty()) {
        // Credit line after the lyrics (not a lyric: index math ignores it).
        auto *credit = new QListWidgetItem(tr("Source: %1").arg(lyrics.source));
        credit->setTextAlignment(Qt::AlignCenter);
        credit->setForeground(kLineDim);
        credit->setFlags(Qt::NoItemFlags);
        QFont small = m_lyricsList->font();
        small.setPointSize(9);
        credit->setFont(small);
        m_lyricsList->addItem(credit);
    }
    refreshLyricsLine();
}

void NowPlayingPage::refreshLyricsLine() {
    if (m_lyrics->status() != LyricsState::Status::Ready || !m_lyrics->lyrics().synced) {
        return;
    }
    const int index = m_lyrics->currentIndex();
    if (index == m_highlighted || index >= m_lyricsList->count()) {
        return;
    }
    if (m_highlighted >= 0 && m_highlighted < m_lyricsList->count()) {
        QListWidgetItem *old = m_lyricsList->item(m_highlighted);
        QFont font = old->font();
        font.setBold(false);
        font.setPointSize(12);
        old->setFont(font);
        old->setForeground(kLineDim);
    }
    m_highlighted = index;
    if (index >= 0) {
        QListWidgetItem *now = m_lyricsList->item(index);
        QFont font = now->font();
        font.setBold(true);
        font.setPointSize(14);
        now->setFont(font);
        now->setForeground(kLineCurrent);
        if (m_cardStack->currentIndex() == 1) {
            m_lyricsList->scrollToItem(now, QAbstractItemView::PositionAtCenter);
        }
    }
}

void NowPlayingPage::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    refreshPosition();
}
