#include "absc_controller.hpp"
#include <algorithm>
#include <numeric>

// 48000Hz 기준, 256 frames/block 이면 5.333ms
static constexpr double DEADLINE_MS = 5.333; 
// 쿨다운: 200ms = 9600 프레임 (48k * 0.2초)
static constexpr int DEFAULT_COOLDOWN_MS = 200; 
static constexpr int SAMPLE_RATE_48K = 48000;

AbscController::AbscController(int sampleRate, int historySize) 
    : historySize_(historySize), 
      currentBlockSize_(256) {
    
    // 쿨다운 프레임 계산 (샘플 레이트 기반)
    cooldownFrames_ = (sampleRate > 0) 
        ? (sampleRate * DEFAULT_COOLDOWN_MS / 1000) 
        : (SAMPLE_RATE_48K * DEFAULT_COOLDOWN_MS / 1000); 
}

void AbscController::setBlockSizeOptions(int small, int normal, int large) {
    smallBlock_ = small;
    normalBlock_ = normal;
    largeBlock_ = large;
    currentBlockSize_ = normalBlock_; // 초기값은 Normal로 설정
}

void AbscController::updateMean(double new_cb_ms) {
    // 큐에 새로운 데이터 추가
    cbHistory_.push_back(new_cb_ms);

    // 윈도우 크기 유지 (가장 오래된 데이터 제거)
    if (cbHistory_.size() > (size_t)historySize_) {
        cbHistory_.pop_front();
    }

    // 이동 평균 재계산
    if (cbHistory_.empty()) {
        cbMean_ = 0.0;
    } else {
        double sum = std::accumulate(cbHistory_.begin(), cbHistory_.end(), 0.0);
        cbMean_ = sum / cbHistory_.size();
    }
}

void AbscController::updateBlockSize(double target_ms) {
    int nextSize = currentBlockSize_;
    
    // 1. 블록 크기 축소 (부하 높음) - Underrun 방지
    // 평균 처리 시간이 상한 임계치를 넘으면 가장 작은 블록으로 축소
    if (cbMean_ > target_ms * upperThreshold_) {
        // 현재 블록이 이미 가장 작지 않으면 축소
        if (currentBlockSize_ > smallBlock_) {
            nextSize = smallBlock_;
        }
    }
    // 2. 블록 크기 확대 (부하 낮음) - CPU 효율 개선
    // 평균 처리 시간이 하한 임계치보다 낮으면 가장 큰 블록으로 확대
    else if (cbMean_ < target_ms * lowerThreshold_) {
        // 현재 블록이 이미 가장 크지 않으면 확대
        if (currentBlockSize_ < largeBlock_) {
            nextSize = largeBlock_;
        }
    }

    // 블록 크기가 변경되면 프레임 카운터를 초기화
    if (nextSize != currentBlockSize_) {
        currentBlockSize_ = nextSize;
        framesSinceSwitch_ = 0;
    }
}

int AbscController::onCallbackEnd(double cb_ms) {
    // 1. cb_ms 이동 평균 업데이트
    updateMean(cb_ms);

    // 2. 쿨다운 검사
    // 쿨다운 기간 중이 아닐 때만 블록 크기 변경 로직 실행
    if (framesSinceSwitch_ >= cooldownFrames_) {
        updateBlockSize(DEADLINE_MS);
    }

    // 3. 경과 프레임 업데이트
    // 이 프로젝트에서는 hostFrames(256) 단위로 처리되므로, 이렇게 업데이트
    framesSinceSwitch_ += currentBlockSize_; 

    return currentBlockSize_;
}