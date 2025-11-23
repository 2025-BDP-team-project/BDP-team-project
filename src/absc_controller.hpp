// absc_controller.hpp
#pragma once
#include <deque>

class AbscController
{
public:
    // sampleRate: Hz, historySize: 이동 평균 윈도우(예: 50 콜백)
    AbscController(int sampleRate, int historySize = 50);

    // 사용 가능한 내부 block 후보 (small < normal < large 가정)
    void setBlockSizeOptions(int small, int normal, int large);

    // 콜백 1회 끝날 때 호출
    //  - cb_ms     : 이번 콜백 처리 시간(ms)
    //  - hostFrames: 이번 콜백에서 처리한 host 버퍼 크기(프레임 수)
    int onCallbackEnd(double cb_ms, int hostFrames);

    int currentBlockSize() const { return currentBlockSize_; }

private:
    // 이동 평균 갱신
    void updateMean(double new_cb_ms);

    // cbMean_ + 데드라인(period_ms)을 바탕으로 block_size 결정
    // fastOverload / fastUnderload:
    //   - true면 fast-path (즉시 small/large jump) 우선 적용
    void updateBlockSize(double period_ms,
                         bool fastOverload,
                         bool fastUnderload);

    // --- 설정 / 상태 ---
    int    sampleRate_;     // Hz
    int    historySize_;    // 이동 평균 길이
    double cbMean_ = 0.0;   // 최근 cb_ms 평균
    double runningSum_ = 0.0; // 이동 평균용 합계

    std::deque<double> cbHistory_;

    // block size 후보
    int smallBlock_  = 128;
    int normalBlock_ = 256;
    int largeBlock_  = 512;

    int currentBlockSize_ = 256;

    // --- Slow-path 히스테리시스 임계값 ---
    // cbMean > period_ms * upperThreshold_  → 더 작은 블록 쪽으로 한 단계
    // cbMean < period_ms * lowerThreshold_  → 더 큰 블록 쪽으로 한 단계
    double upperThreshold_ = 0.9; // 데드라인의 90% 이상이면 위험
    double lowerThreshold_ = 0.5; // 데드라인의 50% 미만이면 여유

    // --- Fast-path 임계값 ---
    // cbMean > period_ms * fastOverloadFactor_   → 즉시 smallest block
    // cbMean < period_ms * fastUnderloadFactor_  → 즉시 largest block
    double fastOverloadFactor_  = 1.05; // 데드라인 초과 + 여유 없이 매우 위험
    double fastUnderloadFactor_ = 0.30; // 데드라인의 30% 미만이면 너무 여유

    // 쿨다운: block_size 변경 후 최소 경과해야 하는 "오디오 프레임" 수
    int  cooldownFrames_     = 0;    // 프레임 단위(= sampleRate * ms)
    int  framesSinceSwitch_  = 0;    // 마지막 전환 이후 누적 host 프레임
};
