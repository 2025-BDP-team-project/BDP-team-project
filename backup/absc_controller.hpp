// absc_controller.hpp
#pragma once
#include <deque>

// --- ABSC 블록 전환 이유 정의 ---
enum class AbscSwitchReason : int {
    NO_SWITCH = 0,          // 전환 없음
    SLOW_DOWN_OVERLOAD = 2, // Slow-path: 부하 증가 -> 블록 축소
    SLOW_UP_UNDERLOAD  = 3, // Slow-path: 여유 -> 블록 확대
    FAST_DOWN_OVERLOAD  = 4, // Fast-path: 부하 급증 -> 즉시 축소
    FAST_UP_UNDERLOAD   = 5, // Fast-path: 매우 여유 -> 즉시 확대
    FAST_DOWN_UNDERRUN  = 6  // Fast-path: 언더런 발생 -> 강제 최소 블록
};

class AbscController
{
public:
    AbscController(int sampleRate, int historySize = 50);

    void setBlockSizeOptions(int small, int normal, int large);

    // cb_ms, hostFrames, underrun 정보를 받아 전환 이유 코드까지 반환
    int onCallbackEnd(double cb_ms, int hostFrames, bool underrun, AbscSwitchReason& switchReasonCode);

    int currentBlockSize() const { return currentBlockSize_; }
    double currentCbMean() const { return cbMean_; } // 현재 평균값 조회

private:
    void updateMean(double new_cb_ms);

    void updateBlockSize(double period_ms,
                         bool fastOverload,
                         bool fastUnderload,
                         AbscSwitchReason& switchReasonCode);

    int    sampleRate_;
    int    historySize_;
    double cbMean_ = 0.0;
    double runningSum_ = 0.0;

    std::deque<double> cbHistory_;

    int smallBlock_  = 128;
    int normalBlock_ = 256;
    int largeBlock_  = 512;

    int currentBlockSize_ = 256;

    double upperThreshold_ = 0.9;
    double lowerThreshold_ = 0.5;

    double fastOverloadFactor_  = 1.05;
    double fastUnderloadFactor_ = 0.30;

    int  cooldownFrames_     = 0;
    int  framesSinceSwitch_  = 0;
};
