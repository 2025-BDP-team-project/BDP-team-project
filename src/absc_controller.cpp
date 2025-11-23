// absc_controller.cpp
#include "absc_controller.hpp"
#include <algorithm>

// 쿨다운: 200ms
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
    // 쿨다운 "프레임" 수 = sampleRate * (쿨다운 시간 초)
    // 예) 48kHz, 200ms → 48000 * 0.2 = 9600 frames
    cooldownFrames_ = static_cast<int>(
        static_cast<double>(sampleRate_) * DEFAULT_COOLDOWN_MS / 1000.0
    );

    // 기본 block 후보 (필요하면 setBlockSizeOptions()로 덮어쓰기)
    smallBlock_      = 128;
    normalBlock_     = 256;
    largeBlock_      = 512;
    currentBlockSize_ = normalBlock_;
}

void AbscController::setBlockSizeOptions(int small, int normal, int large)
{
    smallBlock_  = small;
    normalBlock_ = normal;
    largeBlock_  = large;
    currentBlockSize_ = normalBlock_; // 초기값은 Normal
}

// cb_ms 이동 평균 (O(1) 갱신)
void AbscController::updateMean(double new_cb_ms)
{
    cbHistory_.push_back(new_cb_ms);
    runningSum_ += new_cb_ms;

    if (static_cast<int>(cbHistory_.size()) > historySize_) {
        runningSum_ -= cbHistory_.front();
        cbHistory_.pop_front();
    }

    if (cbHistory_.empty()) {
        cbMean_ = 0.0;
    } else {
        cbMean_ = runningSum_ / static_cast<double>(cbHistory_.size());
    }
}

// fast/slow path를 모두 포함한 block_size 전환 로직
void AbscController::updateBlockSize(double period_ms,
                                     bool fastOverload,
                                     bool fastUnderload)
{
    int nextSize = currentBlockSize_;

    // -------- Fast-path: 급격한 오버로드/언더로드 대응 --------
    if (fastOverload) {
        // 즉시 최소 block으로 점프 (안정성 최우선)
        nextSize = smallBlock_;
    }
    else if (fastUnderload) {
        // 즉시 최대 block으로 점프 (효율 최우선)
        nextSize = largeBlock_;
    }
    else {
        // -------- Slow-path: 완만한 단계적 전환 --------
        // cbMean_ > upperThreshold * period → 더 작은 block 쪽으로 한 단계
        if (cbMean_ > period_ms * upperThreshold_) {
            if (currentBlockSize_ == largeBlock_) {
                nextSize = normalBlock_;
            } else if (currentBlockSize_ == normalBlock_) {
                nextSize = smallBlock_;
            }
            // 이미 small이면 유지
        }
        // cbMean_ < lowerThreshold * period → 더 큰 block 쪽으로 한 단계
        else if (cbMean_ < period_ms * lowerThreshold_) {
            if (currentBlockSize_ == smallBlock_) {
                nextSize = normalBlock_;
            } else if (currentBlockSize_ == normalBlock_) {
                nextSize = largeBlock_;
            }
            // 이미 large이면 유지
        }
        // 그 사이 구간이면 block size 유지 (히스테리시스 영역)
    }

    if (nextSize != currentBlockSize_) {
        currentBlockSize_  = nextSize;
        framesSinceSwitch_ = 0; // 전환 직후 쿨다운 리셋
    }
}

int AbscController::onCallbackEnd(double cb_ms, int hostFrames)
{
    // 1) cb_ms 이동 평균 갱신
    updateMean(cb_ms);

    // 2) 데드라인(주기) 계산: hostFrames / sampleRate * 1000
    //    예) 256 frame @ 48kHz → 5.333... ms
    double period_ms = (static_cast<double>(hostFrames) /
                        static_cast<double>(sampleRate_)) * 1000.0;

    // 3) Fast-path 조건 계산
    //    - cbMean_ > period_ms * fastOverloadFactor_   → 데드라인 초과 + 여유 없음 (위험)
    //    - cbMean_ < period_ms * fastUnderloadFactor_  → 너무 여유로움 (공격적 확대 가능)
    bool fastOverload  = (cbMean_ > period_ms * fastOverloadFactor_);
    bool fastUnderload = (cbMean_ < period_ms * fastUnderloadFactor_);

    // 4) fast-path는 쿨다운 상관 없이 즉시 반응 (안정성/효율 우선)
    if (fastOverload || fastUnderload) {
        updateBlockSize(period_ms, fastOverload, fastUnderload);
    }
    // 5) fast-path가 아닌 경우에만, 쿨다운 이후 slow-path 적용
    else if (framesSinceSwitch_ >= cooldownFrames_) {
        updateBlockSize(period_ms, /*fastOverload=*/false, /*fastUnderload=*/false);
    }

    // 6) 이번 콜백에서 처리된 "host 프레임 수"만큼 시간 경과 누적
    framesSinceSwitch_ += hostFrames;

    return currentBlockSize_;
}
