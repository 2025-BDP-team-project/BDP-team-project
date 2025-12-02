// src/absc_controller.hpp
#pragma once

#include <deque>
#include <vector>

enum class AbscSwitchReason : int
{
    NO_SWITCH           = 0,
    FAST_UP_UNDERRUN    = 1,  // 언더런 / 데드라인 미스 → 긴급 확대
    STEP_UP_OVERLOAD    = 2,  // 부하 높음 → 한 단계 확대
    STEP_DOWN_UNDERLOAD = 3,  // 부하 낮음 → 한 단계 축소
    CRITICAL_AT_MAX     = 4   // 이미 최대 블록인데도 언더런/미스
};

class AbscController
{
public:
    AbscController(int sampleRate, int historySize = 50);

    // 호스트 버퍼/샘플레이트 설정 (RTL 기반 base_oneway_ms 계산)
    void setHostConfig(int sampleRate, int hostFrames);

    // 전체 편도 레이턴시 타깃/최댓값 (예: 10ms, 15ms)
    void setLatencyTargets(double targetMs, double maxMs);

    // 블록 후보 설정.
    // 실제 후보는 {64,96,128}로 고정하고, normal에 가장 가까운 값으로 시작.
    void setBlockSizeOptions(int small, int normal, int large);

    // 콜백 1회 종료 시 ABSC 업데이트 & 다음 블록 크기 반환
    int onCallbackEnd(double cb_ms,
                      int    hostFrames,
                      bool   underrun,
                      AbscSwitchReason& switchReasonCode);

    int    currentBlockSize() const;
    double currentCbMean()    const { return cbMean_; }

    double targetLatencyMs() const { return targetLatencyMs_; }
    double maxLatencyMs()    const { return maxLatencyMs_;    }

private:
    // ==== 내부 helper ====
    void   updateMean(double new_cb_ms);

    double estimateRoundTripLatencyMs(int sampleRate, int hostFrames) const;
    double algoLatencyMs(int blockSize) const;
    double totalLatencyMsForBlock(int blockSize) const;

    int    emergencyMaxIndex() const;  // maxLatencyMs 이하에서 가장 큰 블록
    int    targetMaxIndex() const;     // targetLatencyMs 이하에서 가장 큰 블록

private:
    int    sr_;             // 샘플레이트
    int    hostFrames_;     // 호스트 버퍼 크기
    int    historySize_;

    // cb_ms 이동평균
    double cbMean_     = 0.0;
    double runningSum_ = 0.0;
    std::deque<double> cbHistory_;

    // 내부 블록 후보 (고정: 64,96,128)
    std::vector<int> candidates_;
    int currentIndex_ = 0;      // candidates_ 인덱스

    // 부하 기준
    double upperLoadRatio_    = 0.9;   // cb_mean / period > 0.9 → 부하 높음
    double lowerLoadRatio_    = 0.4;   // cb_mean / period < 0.4 → 부하 낮음
    double fastOverloadRatio_ = 1.05;  // cb_ms > 1.05 * period → 긴급

    // 스위칭 쿨다운 (프레임)
    int    cooldownFrames_    = 0;
    int    framesSinceSwitch_ = 0;

    // 레이턴시 예산 (편도 기준, ms)
    double targetLatencyMs_ = 10.0;
    double maxLatencyMs_    = 15.0;
    double safetyMarginMs_  = 1.0;

    // 호스트 기반 편도 레이턴시 (RTL / 2)
    double baseOneWayMs_ = 0.0;
};
