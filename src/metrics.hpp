// src/metrics.hpp
#pragma once

#include <fstream>
#include <string>

class Metrics
{
public:
    // sampleRate, hostFrames: 호스트/인터페이스 설정
    // targetLatencyMs, maxLatencyMs: 편도 기준 레이턴시 타깃/상한 (분석용, Fixed/ABSC 공통)
    Metrics(int sampleRate,
            int hostFrames,
            double targetLatencyMs = 0.0,
            double maxLatencyMs    = 0.0);

    void openCSV(const std::string& path);

    // blockSize : internal block size (실제 사용 중인 블록 크기)
    // activeWork: 해당 콜백에서 DSP를 실제로 수행했는지 여부
    // switchReasonCode : AbscSwitchReason enum 값을 int로 기록 (Fixed는 0)
    void onCallbackEnd(double cb_ms,
                       bool   underrun,
                       int    blockSize,
                       bool   activeWork,
                       int    switchReasonCode);

    void closeCSV();

private:
    int    sr_;
    int    hostFrames_;
    double targetLatencyMs_;   // 편도 타깃 레이턴시
    double maxLatencyMs_;      // 편도 최대 허용 레이턴시

    double periodMs_;          // 호스트 콜백 주기(ms)
    long long frameIndex_;     // 콜백 기반 누적 프레임 인덱스

    // Metrics 내부에서 자체적으로 관리하는 cb_ms 이동 평균
    double cbMean_;            // cb_ms 러닝 평균
    long long cbCount_;        // 콜백 횟수

    std::ofstream csv_;
};
