// absc_controller.cpp
#include "absc_controller.hpp"
#include <algorithm>

static constexpr int DEFAULT_COOLDOWN_MS = 200;
static constexpr int SAMPLE_RATE_48K     = 48000;

AbscController::AbscController(int sampleRate, int historySize)
    : sampleRate_(sampleRate > 0 ? sampleRate : SAMPLE_RATE_48K),
      historySize_(historySize),
      cbMean_(0.0),
      runningSum_(0.0),
      cooldownFrames_(0),
      framesSinceSwitch_(0)
{
    cooldownFrames_ = static_cast<int>(
        (static_cast<double>(DEFAULT_COOLDOWN_MS) / 1000.0) *
        static_cast<double>(sampleRate_));
}

void AbscController::setBlockSizeOptions(int small, int normal, int large)
{
    smallBlock_  = std::max(16, small);
    normalBlock_ = std::max(smallBlock_, normal);
    largeBlock_  = std::max(normalBlock_, large);

    currentBlockSize_ = normalBlock_;
}

void AbscController::updateMean(double new_cb_ms)
{
    cbHistory_.push_back(new_cb_ms);
    runningSum_ += new_cb_ms;

    if (static_cast<int>(cbHistory_.size()) > historySize_) {
        runningSum_ -= cbHistory_.front();
        cbHistory_.pop_front();
    }

    if (!cbHistory_.empty()) {
        cbMean_ = runningSum_ / static_cast<double>(cbHistory_.size());
    }
}

void AbscController::updateBlockSize(double period_ms,
                                     bool fastOverload,
                                     bool fastUnderload,
                                     AbscSwitchReason& switchReasonCode)
{
    int nextSize = currentBlockSize_;
    switchReasonCode = AbscSwitchReason::NO_SWITCH; // 기본값

    // Fast-path
    if (fastOverload) {
        nextSize = smallBlock_;
        if (nextSize != currentBlockSize_) switchReasonCode = AbscSwitchReason::FAST_DOWN_OVERLOAD;
    }
    else if (fastUnderload) {
        nextSize = largeBlock_;
        if (nextSize != currentBlockSize_) switchReasonCode = AbscSwitchReason::FAST_UP_UNDERLOAD;
    }
    else {
        // Slow-path
        if (cbMean_ > period_ms * upperThreshold_) {
            if (currentBlockSize_ == largeBlock_) {
                nextSize = normalBlock_;
                switchReasonCode = AbscSwitchReason::SLOW_DOWN_OVERLOAD;
            } else if (currentBlockSize_ == normalBlock_) {
                nextSize = smallBlock_;
                switchReasonCode = AbscSwitchReason::SLOW_DOWN_OVERLOAD;
            }
        }
        else if (cbMean_ < period_ms * lowerThreshold_) {
            if (currentBlockSize_ == smallBlock_) {
                nextSize = normalBlock_;
                switchReasonCode = AbscSwitchReason::SLOW_UP_UNDERLOAD;
            } else if (currentBlockSize_ == normalBlock_) {
                nextSize = largeBlock_;
                switchReasonCode = AbscSwitchReason::SLOW_UP_UNDERLOAD;
            }
        }
    }

    if (nextSize != currentBlockSize_) {
        currentBlockSize_  = nextSize;
        framesSinceSwitch_ = 0;
    } else {
        switchReasonCode = AbscSwitchReason::NO_SWITCH; // 변경 없으면 이유 없음
    }
}

int AbscController::onCallbackEnd(double cb_ms, int hostFrames, bool underrun, AbscSwitchReason& switchReasonCode)
{
    updateMean(cb_ms);

    double period_ms = (static_cast<double>(hostFrames) /
                        static_cast<double>(sampleRate_)) * 1000.0;

    // 언더런이 발생한 경우: 우선순위 최상위 fast-path로 즉시 최소 블록으로 축소
    if (underrun) {
        int nextSize = smallBlock_;
        if (nextSize != currentBlockSize_) {
            currentBlockSize_  = nextSize;
            framesSinceSwitch_ = 0;
            switchReasonCode   = AbscSwitchReason::FAST_DOWN_UNDERRUN;
        } else {
            switchReasonCode   = AbscSwitchReason::NO_SWITCH;
        }

        framesSinceSwitch_ += hostFrames;
        return currentBlockSize_;
    }

    bool fastOverload  = (cbMean_ > period_ms * fastOverloadFactor_);
    bool fastUnderload = (cbMean_ < period_ms * fastUnderloadFactor_);

    switchReasonCode = AbscSwitchReason::NO_SWITCH;

    if (fastOverload || fastUnderload) {
        updateBlockSize(period_ms, fastOverload, fastUnderload, switchReasonCode);
    }
    else if (framesSinceSwitch_ >= cooldownFrames_) {
        updateBlockSize(period_ms, false, false, switchReasonCode);
    }

    framesSinceSwitch_ += hostFrames;
    return currentBlockSize_;
}
