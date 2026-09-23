#pragma once

#include "Types.hpp"
#include "TimeTravelEngine.hpp"
#include <QWidget>
#include <QSlider>
#include <QPushButton>
#include <QLabel>

namespace edb_next {

class TimeTravelWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimeTravelWidget(QWidget* parent = nullptr);
    ~TimeTravelWidget() override;

    void updateTimeline(const TimeTravelEngine& engine);

Q_SIGNALS:
    void stepBackRequested();
    void stepForwardRequested();
    void reverseContinueRequested();
    void seekFrameRequested(size_t frameIndex);
    void liveResumeRequested();

private Q_SLOTS:
    void onSliderValueChanged(int value);

private:
    void setupUi();

    QPushButton* btnRevContinue_{nullptr};
    QPushButton* btnStepBack_{nullptr};
    QPushButton* btnStepForward_{nullptr};
    QPushButton* btnLive_{nullptr};
    QSlider* slider_{nullptr};
    QLabel* statusLabel_{nullptr};
    bool isUpdating_{false};
};

} // namespace edb_next
