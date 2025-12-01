#include "absc_controller.hpp"
#include <algorithm>
#include <numeric>

// 48kHz 기준 상수 정의
static constexpr double DEADLINE_MS = 5.333; 
static constexpr int DEFAULT_COOLDOWN_MS = 200; 
static constexpr int SAMPLE_RATE_48K = 48000;

AbscController::AbscController(int sampleRate, int historySize) 
    : historySize_(historySize), 
      currentBlockSize_(256) {
    
    // 쿨다운 프레임 계산 (초 -> 프레임)
    cooldownFrames_ = (sampleRate > 0) 
        ? (sampleRate * DEFAULT_COOLDOWN_MS / 1000) 
        : (SAMPLE_RATE_48K * DEFAULT_COOLDOWN_MS / 1000); 
}

void AbscController::setBlockSizeOptions(int small, int normal, int large) {
    smallBlock_ = small;
    normalBlock_ = normal;
    largeBlock_ = large;
    currentBlockSize_ = normalBlock_; 
}

void AbscController::updateMean(double new_cb_ms) {
    cbHistory_.push_back(new_cb_ms);
    if (cbHistory_.size() > (size_t)historySize_) {
        cbHistory_.pop_front();
    }
    if (cbHistory_.empty()) {
        cbMean_ = 0.0;
    } else {
        double sum = std::accumulate(cbHistory_.begin(), cbHistory_.end(), 0.0);
        cbMean_ = sum / cbHistory_.size();
    }
}

void AbscController::updateBlockSize(double target_ms) {
    int nextSize = currentBlockSize_;
    
    // 128 -> 256 -> 512 순서로 이동합니다.
    
    // 1. 부하 상승 (Scale Up)
    if (cbMean_ > target_ms * upperThreshold_) {
        if (currentBlockSize_ == smallBlock_) {
            nextSize = normalBlock_; // 128 -> 256
        } else if (currentBlockSize_ == normalBlock_) {
            nextSize = largeBlock_;  // 256 -> 512
        }
    }
    // 2. 부하 하강 (Scale Down)
    else if (cbMean_ < target_ms * lowerThreshold_) {
        if (currentBlockSize_ == largeBlock_) {
            nextSize = normalBlock_; // 512 -> 256
        } else if (currentBlockSize_ == normalBlock_) {
            nextSize = smallBlock_;  // 256 -> 128
        }
    }
    if (nextSize != currentBlockSize_) {
        currentBlockSize_ = nextSize;
        framesSinceSwitch_ = 0;
    }

}

int AbscController::onCallbackEnd(double cb_ms) {
    updateMean(cb_ms);

    // 쿨다운 기간이 지났을 때만 블록 크기 변경 시도
    if (framesSinceSwitch_ >= cooldownFrames_) {
        updateBlockSize(DEADLINE_MS);
    }

    framesSinceSwitch_ += currentBlockSize_; 
    return currentBlockSize_;
}