#include "overlaysettingspage.h"

#include "overlaysettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
QLabel *sectionTitle(const QString &text) {
    auto *label = new QLabel(text);
    label->setObjectName("sectionTitle");
    return label;
}

QSlider *makeSlider(int min, int max) {
    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(min, max);
    slider->setMinimumWidth(220);
    return slider;
}
} // namespace

OverlaySettingsPage::OverlaySettingsPage(OverlaySettings *settings, QWidget *parent)
    : QWidget(parent), m_settings(settings) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto *title = new QLabel(tr("Floating overlay"));
    title->setObjectName("pageTitle");
    root->addWidget(title);

    root->addWidget(sectionTitle(tr("Visibility")));
    m_enabled = new QCheckBox(tr("Enable overlay"));
    m_autoShow = new QCheckBox(tr("Show automatically when playback starts"));
    m_autoHide = new QCheckBox(tr("Hide automatically after AirPlay disconnects"));
    root->addWidget(m_enabled);
    root->addWidget(m_autoShow);
    root->addWidget(m_autoHide);

    auto *delayRow = new QHBoxLayout;
    delayRow->setContentsMargins(28, 0, 0, 0);
    auto *delayLabel = new QLabel(tr("Hide after"));
    delayLabel->setObjectName("muted");
    m_hideDelay = new QSpinBox;
    m_hideDelay->setRange(OverlaySettings::kMinHideDelaySec,
                          OverlaySettings::kMaxHideDelaySec);
    m_hideDelay->setSuffix(tr(" s"));
    delayRow->addWidget(delayLabel);
    delayRow->addWidget(m_hideDelay);
    delayRow->addStretch();
    root->addLayout(delayRow);

    root->addWidget(sectionTitle(tr("Behaviour")));
    m_alwaysOnTop = new QCheckBox(tr("Always on top"));
    m_clickThrough = new QCheckBox(
        tr("Click-through (mouse clicks pass to windows below; undo from the tray menu)"));
    m_animations = new QCheckBox(tr("Animations"));
    root->addWidget(m_alwaysOnTop);
    root->addWidget(m_clickThrough);
    root->addWidget(m_animations);

    root->addWidget(sectionTitle(tr("Appearance")));
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 1);

    m_mode = new QComboBox;
    m_mode->addItem(tr("Compact"), static_cast<int>(OverlaySettings::Mode::Compact));
    m_mode->addItem(tr("Expanded"), static_cast<int>(OverlaySettings::Mode::Expanded));
    m_scale = makeSlider(OverlaySettings::kMinScale, OverlaySettings::kMaxScale);
    m_scaleValue = new QLabel;
    m_scaleValue->setMinimumWidth(48);
    m_opacity = makeSlider(OverlaySettings::kMinOpacity, OverlaySettings::kMaxOpacity);
    m_opacityValue = new QLabel;
    m_opacityValue->setMinimumWidth(48);

    grid->addWidget(new QLabel(tr("Mode")), 0, 0);
    grid->addWidget(m_mode, 0, 1, 1, 2, Qt::AlignLeft);
    grid->addWidget(new QLabel(tr("Size")), 1, 0);
    grid->addWidget(m_scale, 1, 1);
    grid->addWidget(m_scaleValue, 1, 2);
    grid->addWidget(new QLabel(tr("Opacity")), 2, 0);
    grid->addWidget(m_opacity, 2, 1);
    grid->addWidget(m_opacityValue, 2, 2);
    root->addLayout(grid);

    root->addWidget(sectionTitle(tr("Position")));
    auto *positionRow = new QHBoxLayout;
    auto *hint = new QLabel(tr("Drag the overlay with the mouse to move it. "
                               "Double-click it to switch mode."));
    hint->setObjectName("muted");
    hint->setWordWrap(true);
    auto *reset = new QPushButton(tr("Reset position"));
    positionRow->addWidget(hint, 1);
    positionRow->addWidget(reset);
    root->addLayout(positionRow);
    root->addStretch();

    connect(m_enabled, &QCheckBox::toggled, m_settings, &OverlaySettings::setEnabled);
    connect(m_autoShow, &QCheckBox::toggled, m_settings, &OverlaySettings::setAutoShow);
    connect(m_autoHide, &QCheckBox::toggled, m_settings, &OverlaySettings::setAutoHide);
    connect(m_hideDelay, &QSpinBox::valueChanged, m_settings,
            &OverlaySettings::setHideDelaySec);
    connect(m_alwaysOnTop, &QCheckBox::toggled, m_settings,
            &OverlaySettings::setAlwaysOnTop);
    connect(m_clickThrough, &QCheckBox::toggled, m_settings,
            &OverlaySettings::setClickThrough);
    connect(m_animations, &QCheckBox::toggled, m_settings,
            &OverlaySettings::setAnimations);
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_settings->setMode(static_cast<OverlaySettings::Mode>(
            m_mode->itemData(index).toInt()));
    });
    connect(m_scale, &QSlider::valueChanged, m_settings,
            &OverlaySettings::setScalePercent);
    connect(m_opacity, &QSlider::valueChanged, m_settings,
            &OverlaySettings::setOpacityPercent);
    connect(reset, &QPushButton::clicked, m_settings,
            &OverlaySettings::clearPosition);
    connect(m_settings, &OverlaySettings::changed, this, &OverlaySettingsPage::refresh);

    refresh();
}

void OverlaySettingsPage::refresh() {
    const auto setChecked = [](QCheckBox *box, bool value) {
        QSignalBlocker blocker(box);
        box->setChecked(value);
    };
    setChecked(m_enabled, m_settings->enabled());
    setChecked(m_autoShow, m_settings->autoShow());
    setChecked(m_autoHide, m_settings->autoHide());
    setChecked(m_alwaysOnTop, m_settings->alwaysOnTop());
    setChecked(m_clickThrough, m_settings->clickThrough());
    setChecked(m_animations, m_settings->animations());

    {
        QSignalBlocker blocker(m_hideDelay);
        m_hideDelay->setValue(m_settings->hideDelaySec());
    }
    m_hideDelay->setEnabled(m_settings->autoHide());
    m_autoShow->setEnabled(m_settings->enabled());
    m_autoHide->setEnabled(m_settings->enabled());
    {
        QSignalBlocker blocker(m_mode);
        m_mode->setCurrentIndex(
            m_mode->findData(static_cast<int>(m_settings->mode())));
    }
    {
        QSignalBlocker blocker(m_scale);
        m_scale->setValue(m_settings->scalePercent());
    }
    {
        QSignalBlocker blocker(m_opacity);
        m_opacity->setValue(m_settings->opacityPercent());
    }
    m_scaleValue->setText(QStringLiteral("%1%").arg(m_settings->scalePercent()));
    m_opacityValue->setText(QStringLiteral("%1%").arg(m_settings->opacityPercent()));
}
