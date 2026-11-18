#include "nowplayingpage.h"

#include "mediastate.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QProgressBar>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int kCoverSize = 168;

QString formatTime(qint64 ms) {
    const qint64 total = std::max<qint64>(ms, 0) / 1000;
    return QStringLiteral("%1:%2")
        .arg(total / 60)
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}
} // namespace

NowPlayingPage::NowPlayingPage(const MediaState *media, QWidget *parent)
    : QWidget(parent), m_media(media) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(tr("Now playing"));
    title->setObjectName("pageTitle");
    m_chip = new QLabel;
    m_chip->setObjectName("statusChip");
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_chip);
    root->addLayout(header);

    m_device = new QLabel;
    m_device->setObjectName("muted");
    root->addWidget(m_device);
    root->addSpacing(8);

    auto *card = new QFrame;
    card->setObjectName("card");
    auto *cardLayout = new QHBoxLayout(card);
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

    root->addWidget(card);
    root->addStretch();

    connect(m_media, &MediaState::stateChanged, this, &NowPlayingPage::refreshState);
    connect(m_media, &MediaState::deviceChanged, this, &NowPlayingPage::refreshState);
    connect(m_media, &MediaState::trackChanged, this, &NowPlayingPage::refreshTrack);
    connect(m_media, &MediaState::coverChanged, this, &NowPlayingPage::refreshCover);
    connect(m_media, &MediaState::positionChanged, this, &NowPlayingPage::refreshPosition);

    refreshState();
    refreshTrack();
    refreshCover();
    refreshPosition();
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
    m_title->setText(hasTrack ? m_media->title() : tr("Nothing playing"));
    m_artist->setText(m_media->artist());
    m_album->setText(m_media->album());
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

void NowPlayingPage::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    refreshPosition();
}
