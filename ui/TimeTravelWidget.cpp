#include "TimeTravelWidget.hpp"
#include <QHBoxLayout>
#include <QStyle>

namespace edb_next {

TimeTravelWidget::TimeTravelWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

TimeTravelWidget::~TimeTravelWidget() = default;

void TimeTravelWidget::setupUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    // Modern styling for TTD bar
    setStyleSheet(QStringLiteral(
        "TimeTravelWidget { background-color: #252526; border-top: 1px solid #3e3e42; }"
        "QPushButton { background: #333337; color: #d4d4d4; border: 1px solid #3e3e42; padding: 3px 8px; border-radius: 2px; font-size: 11px; }"
        "QPushButton:hover { background: #3e3e42; color: #ffffff; }"
        "QPushButton:disabled { color: #666666; background: #252526; }"
        "QSlider::groove:horizontal { height: 4px; background: #3e3e42; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #007acc; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #0098ff; width: 12px; margin-top: -4px; margin-bottom: -4px; border-radius: 6px; }"
    ));

    btnRevContinue_ = new QPushButton(QStringLiteral("⏮ Rev Cont"), this);
    btnRevContinue_->setToolTip(QStringLiteral("Reverse Continue to previous breakpoint (Ctrl+Shift+F9)"));
    btnRevContinue_->setEnabled(false);
    connect(btnRevContinue_, &QPushButton::clicked, this, &TimeTravelWidget::reverseContinueRequested);
    layout->addWidget(btnRevContinue_);

    btnStepBack_ = new QPushButton(QStringLiteral("◀ Step Back"), this);
    btnStepBack_->setToolTip(QStringLiteral("Step Back one instruction (Ctrl+F7)"));
    btnStepBack_->setEnabled(false);
    connect(btnStepBack_, &QPushButton::clicked, this, &TimeTravelWidget::stepBackRequested);
    layout->addWidget(btnStepBack_);

    btnStepForward_ = new QPushButton(QStringLiteral("▶ Step Fwd"), this);
    btnStepForward_->setToolTip(QStringLiteral("Step Forward in timeline (Ctrl+F8)"));
    btnStepForward_->setEnabled(false);
    connect(btnStepForward_, &QPushButton::clicked, this, &TimeTravelWidget::stepForwardRequested);
    layout->addWidget(btnStepForward_);

    slider_ = new QSlider(Qt::Horizontal, this);
    slider_->setRange(0, 0);
    slider_->setValue(0);
    slider_->setEnabled(false);
    connect(slider_, &QSlider::valueChanged, this, &TimeTravelWidget::onSliderValueChanged);
    layout->addWidget(slider_, 1);

    statusLabel_ = new QLabel(QStringLiteral("TTD: 0 frames (Live)"), this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #858585; font-size: 11px; font-family: monospace;"));
    layout->addWidget(statusLabel_);

    btnLive_ = new QPushButton(QStringLiteral("Live 🔴"), this);
    btnLive_->setToolTip(QStringLiteral("Jump back to live execution head"));
    btnLive_->setEnabled(false);
    connect(btnLive_, &QPushButton::clicked, this, &TimeTravelWidget::liveResumeRequested);
    layout->addWidget(btnLive_);
}

void TimeTravelWidget::updateTimeline(const TimeTravelEngine& engine) {
    isUpdating_ = true;

    size_t count = engine.frameCount();
    size_t cur = engine.currentFrameIndex();
    bool isReplaying = engine.isReplaying();

    if (count > 0) {
        slider_->setEnabled(true);
        slider_->setRange(0, static_cast<int>(count - 1));
        slider_->setValue(static_cast<int>(cur));

        btnStepBack_->setEnabled(engine.canStepBack());
        btnRevContinue_->setEnabled(engine.canStepBack());
        btnStepForward_->setEnabled(engine.canStepForward());
        btnLive_->setEnabled(isReplaying);

        if (isReplaying) {
            statusLabel_->setText(QString::asprintf("TTD: Frame %zu/%zu [REPLAY] @ 0x%016llx",
                                                   cur + 1, count,
                                                   static_cast<unsigned long long>(engine.timeline()[cur].rip.value())));
            statusLabel_->setStyleSheet(QStringLiteral("color: #ce9178; font-weight: bold; font-size: 11px; font-family: monospace;"));
        } else {
            statusLabel_->setText(QString::asprintf("TTD: Frame %zu/%zu [LIVE HEAD]", count, count));
            statusLabel_->setStyleSheet(QStringLiteral("color: #4ec9b0; font-size: 11px; font-family: monospace;"));
        }
    } else {
        slider_->setEnabled(false);
        slider_->setRange(0, 0);
        btnStepBack_->setEnabled(false);
        btnRevContinue_->setEnabled(false);
        btnStepForward_->setEnabled(false);
        btnLive_->setEnabled(false);
        statusLabel_->setText(QStringLiteral("TTD: 0 frames (Live)"));
        statusLabel_->setStyleSheet(QStringLiteral("color: #858585; font-size: 11px; font-family: monospace;"));
    }

    isUpdating_ = false;
}

void TimeTravelWidget::onSliderValueChanged(int value) {
    if (isUpdating_) return;
    if (value >= 0) {
        Q_EMIT seekFrameRequested(static_cast<size_t>(value));
    }
}

} // namespace edb_next
