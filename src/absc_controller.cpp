// src/absc_controller.cpp
#include "absc_controller.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int   DEFAULT_SR_48K       = 48000;
    constexpr double DEFAULT_COOLDOWN_MS = 200.0; // 0.2초
}

// ====== 생성자 ======

AbscController::AbscController(int sampleRate, int historySize)
    : sr_(sampleRate > 0 ? sampleRate : DEFAULT_SR_48K)
    , hostFrames_(0)
    , historySize_(historySize > 0 ? historySize : 50)
{
    // 내부 블록 후보: 64, 96, 128
    candidates_.clear();
    candidates_.push_back(64);
    candidates_.push_back(96);
    candidates_.push_back(128);

    // 기본 시작점을 96에 가장 가까운 후보로
    int preferred = 96;
    int bestIdx   = 0;
    int bestDiff  = std::abs(candidates_[0] - preferred);
    for (int i = 1; i < static_cast<int>(candidates_.size()); ++i) {
        int d = std::abs(candidates_[i] - preferred);
        if (d < bestDiff) {
            bestDiff = d;
            bestIdx  = i;
        }
    }
    currentIndex_ = bestIdx;

    double cooldownSec = DEFAULT_COOLDOWN_MS / 1000.0;
    cooldownFrames_    = static_cast<int>(cooldownSec * static_cast<double>(sr_));
    framesSinceSwitch_ = cooldownFrames_;
}

// ====== 설정 함수 ======

void AbscController::setHostConfig(int sampleRate, int hostFrames)
{
    if (sampleRate > 0) sr_ = sampleRate;
    hostFrames_ = hostFrames;

    if (hostFrames_ > 0 && sr_ > 0) {
        double rtlMs = estimateRoundTripLatencyMs(sr_, hostFrames_);
        baseOneWayMs_ = 0.5 * rtlMs;
    } else {
        baseOneWayMs_ = 0.0;
    }

    // 쿨다운도 재계산
    double cooldownSec = DEFAULT_COOLDOWN_MS / 1000.0;
    cooldownFrames_    = static_cast<int>(cooldownSec * static_cast<double>(sr_));
    framesSinceSwitch_ = cooldownFrames_;
}

void AbscController::setLatencyTargets(double targetMs, double maxMs)
{
    if (targetMs > 0.0)
        targetLatencyMs_ = targetMs;
    if (maxMs > targetLatencyMs_)
        maxLatencyMs_ = maxMs;
}

void AbscController::setBlockSizeOptions(int /*small*/, int normal, int /*large*/)
{
    // 후보 자체는 {64,96,128}로 고정
    if (candidates_.empty()) {
        candidates_.push_back(64);
        candidates_.push_back(96);
        candidates_.push_back(128);
    }

    std::sort(candidates_.begin(), candidates_.end());
    candidates_.erase(std::unique(candidates_.begin(), candidates_.end()),
                      candidates_.end());

    int preferred = (normal > 0 ? normal : 96);
    int bestIdx   = 0;
    int bestDiff  = std::abs(candidates_[0] - preferred);
    for (int i = 1; i < static_cast<int>(candidates_.size()); ++i) {
        int d = std::abs(candidates_[i] - preferred);
        if (d < bestDiff) {
            bestDiff = d;
            bestIdx  = i;
        }
    }
    currentIndex_ = bestIdx;
}

// ====== 기본 정보 ======

int AbscController::currentBlockSize() const
{
    if (candidates_.empty())
        return 0;
    return candidates_[currentIndex_];
}

// ====== 내부 helper 구현 ======

void AbscController::updateMean(double new_cb_ms)
{
    cbHistory_.push_back(new_cb_ms);
    runningSum_ += new_cb_ms;

    if (static_cast<int>(cbHistory_.size()) > historySize_) {
        runningSum_ -= cbHistory_.front();
        cbHistory_.pop_front();
    }

    if (!cbHistory_.empty())
        cbMean_ = runningSum_ / static_cast<double>(cbHistory_.size());
    else
        cbMean_ = 0.0;
}

// 네가 준 RTL 표를 기반으로 일부 케이스는 상수로, 나머지는 보수적 추정
double AbscController::estimateRoundTripLatencyMs(int sampleRate, int hostFrames) const
{
    struct Entry {
        int sr;
        int buf;
        double rtl;
    };

    static const Entry table[] = {
        // Zoom LiveTrak L-8 기준 예시 (질문에서 주신 값)
        {192000, 128, 4.9},
        {192000,  96, 4.5},
        {192000,  64, 4.2},
        { 96000, 128,10.4},
        { 96000,  96, 5.7},
        { 96000,  64, 5.0},
        { 48000, 128,14.5},
        { 48000,  96, 9.2},
        { 48000,  64, 7.8},
        { 44100, 128,15.1},
        { 44100,  96,13.7},
        { 44100,  64, 8.2},
    };

    for (const auto& e : table) {
        if (e.sr == sampleRate && e.buf == hostFrames)
            return e.rtl;
    }

    // fallback: 이론상 호스트 버퍼 지연의 왕복(×2)
    if (sampleRate > 0 && hostFrames > 0) {
        double bufMs = (static_cast<double>(hostFrames) /
                        static_cast<double>(sampleRate)) * 1000.0;
        return 2.0 * bufMs;
    }
    return 0.0;
}

double AbscController::algoLatencyMs(int blockSize) const
{
    if (blockSize <= 0 || sr_ <= 0)
        return 0.0;

    // FFT 그룹딜레이 ~ N/2 샘플 근사
    return (static_cast<double>(blockSize) / 2.0)
           / static_cast<double>(sr_) * 1000.0;
}

double AbscController::totalLatencyMsForBlock(int blockSize) const
{
    return baseOneWayMs_ + algoLatencyMs(blockSize);
}

int AbscController::emergencyMaxIndex() const
{
    if (candidates_.empty())
        return 0;

    int bestIdx = 0;
    for (int i = 0; i < static_cast<int>(candidates_.size()); ++i) {
        double tLat = totalLatencyMsForBlock(candidates_[i]);
        if (tLat <= maxLatencyMs_)
            bestIdx = i;
    }
    return bestIdx;
}

int AbscController::targetMaxIndex() const
{
    if (candidates_.empty())
        return 0;

    int bestIdx = 0;
    for (int i = 0; i < static_cast<int>(candidates_.size()); ++i) {
        double tLat = totalLatencyMsForBlock(candidates_[i]);
        if (tLat + safetyMarginMs_ <= targetLatencyMs_)
            bestIdx = i;
    }
    return bestIdx;
}

// ====== 메인 컨트롤 로직 ======

int AbscController::onCallbackEnd(double cb_ms,
                                  int    hostFrames,
                                  bool   underrun,
                                  AbscSwitchReason& switchReasonCode)
{
    switchReasonCode = AbscSwitchReason::NO_SWITCH;

    if (candidates_.empty())
        return 0;

    // hostFrames가 변할 수 있다면 최신 값으로 업데이트
    if (hostFrames > 0 && hostFrames != hostFrames_) {
        setHostConfig(sr_, hostFrames);
    }

    // 호스트 콜백 주기
    double periodMs = 0.0;
    if (sr_ > 0 && hostFrames_ > 0) {
        periodMs = (static_cast<double>(hostFrames_) /
                    static_cast<double>(sr_)) * 1000.0;
    }

    // cb_ms 이동평균 업데이트
    updateMean(cb_ms);

    double loadRatio = 0.0;
    if (periodMs > 0.0)
        loadRatio = cbMean_ / periodMs;

    // 각 후보에서 total latency 검사
    int emIdx = emergencyMaxIndex(); // maxLatency 이하에서 가장 큰 블록
    int tgIdx = targetMaxIndex();    // targetLatency 이하에서 가장 큰 블록

    int curIdx  = currentIndex_;
    int curSize = candidates_[curIdx];

    // 1) 언더런 / 데드라인 미스 → 최우선 (FAST-UP)
    bool deadlineMiss = (periodMs > 0.0) && (cb_ms > periodMs * fastOverloadRatio_);

    if (underrun || deadlineMiss) {
        int upIdx = emIdx;
        if (upIdx > curIdx) {
            currentIndex_      = upIdx;
            framesSinceSwitch_ = 0;
            switchReasonCode   = AbscSwitchReason::FAST_UP_UNDERRUN;
        }
        else {
            // 이미 maxLatency 내에서 가능한 가장 큰 블록인데도 문제 발생
            switchReasonCode = AbscSwitchReason::CRITICAL_AT_MAX;
        }
        return candidates_[currentIndex_];
    }

    // 2) 쿨다운: 너무 자주 스위칭하지 않도록
    if (framesSinceSwitch_ < cooldownFrames_) {
        framesSinceSwitch_ += hostFrames_;
        return curSize;
    }

    // 3) 평상시: 부하/레이턴시 트레이드오프
    int nextIdx = curIdx;

    // (a) loadRatio가 높음 → 블록 키워서 CPU 여유 확보 (단, maxLatency 안에서만)
    if (loadRatio > upperLoadRatio_) {
        int candIdx = std::min(curIdx + 1, emIdx);
        if (candIdx > curIdx) {
            nextIdx          = candIdx;
            switchReasonCode = AbscSwitchReason::STEP_UP_OVERLOAD;
        }
    }
    // (b) loadRatio가 낮고, 레이턴시 예산 여유 → 블록 줄여서 레이턴시 ↓
    else if (loadRatio < lowerLoadRatio_) {
        int candIdx = std::max(curIdx - 1, 0);
        if (candIdx < curIdx) {
            double tLat = totalLatencyMsForBlock(candidates_[candIdx]);
            if (tLat + safetyMarginMs_ <= targetLatencyMs_) {
                nextIdx          = candIdx;
                switchReasonCode = AbscSwitchReason::STEP_DOWN_UNDERLOAD;
            }
        }
    }

    if (nextIdx != curIdx) {
        currentIndex_      = nextIdx;
        framesSinceSwitch_ = 0;
    } else {
        framesSinceSwitch_ += hostFrames_;
    }

    return candidates_[currentIndex_];
}
