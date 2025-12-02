// src/metrics.cpp
#include "metrics.hpp"

#include <iomanip>

// 네가 제공한 RTL 측정표 기반
static double estimateRoundTripLatencyMs_internal(int sr, int hostFrames)
{
    struct Entry {
        int sr;
        int buf;
        double rtl;
    };

    static const Entry table[] = {
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
        if (e.sr == sr && e.buf == hostFrames)
            return e.rtl;
    }

    // fallback: 호스트 버퍼 지연 × 2 (왕복)
    if (sr > 0 && hostFrames > 0) {
        double bufMs = (static_cast<double>(hostFrames) /
                        static_cast<double>(sr)) * 1000.0;
        return 2.0 * bufMs;
    }
    return 0.0;
}

Metrics::Metrics(int sampleRate,
                 int hostFrames,
                 double targetLatencyMs,
                 double maxLatencyMs)
    : sr_(sampleRate)
    , hostFrames_(hostFrames)
    , targetLatencyMs_(targetLatencyMs)
    , maxLatencyMs_(maxLatencyMs)
    , periodMs_(0.0)
    , frameIndex_(0)
    , cbMean_(0.0)
    , cbCount_(0)
{
    if (sr_ > 0 && hostFrames_ > 0) {
        periodMs_ = (static_cast<double>(hostFrames_) /
                     static_cast<double>(sr_)) * 1000.0;
    }
}

void Metrics::openCSV(const std::string& path)
{
    csv_.open(path, std::ios::out | std::ios::trunc);

    csv_ << "frame_index,"
         << "cb_ms,"
         << "underrun,"
         << "block_size,"
         << "active_work,"
         << "period_ms,"
         << "cb_mean,"
         << "load_ratio,"
         << "base_rtl_ms,"
         << "base_oneway_ms,"
         << "algo_latency_ms,"
         << "total_latency_ms,"
         << "target_latency_ms,"
         << "max_latency_ms,"
         << "latency_budget_used,"
         << "latency_margin_ms,"
         << "block_switch_reason\n";
}

void Metrics::onCallbackEnd(double cb_ms,
                            bool   underrun,
                            int    blockSize,
                            bool   activeWork,
                            int    switchReasonCode)
{
    if (!csv_.is_open())
        return;

    // 1) 내부적으로 cb_ms 평균 갱신 (Fixed/ABSC 공통)
    ++cbCount_;
    if (cbCount_ == 1) {
        cbMean_ = cb_ms;
    } else {
        // 러닝 평균
        cbMean_ += (cb_ms - cbMean_) / static_cast<double>(cbCount_);
    }

    double periodMs  = periodMs_;
    double loadRatio = 0.0;
    if (periodMs > 0.0)
        loadRatio = cbMean_ / periodMs;

    // 2) RTL 기반 base 레이턴시
    double baseRtlMs    = estimateRoundTripLatencyMs_internal(sr_, hostFrames_);
    double baseOneWayMs = 0.5 * baseRtlMs;

    // 3) internal block 기반 알고리즘 레이턴시 (FFT 그룹딜레이 근사)
    double algoLatencyMs = 0.0;
    if (blockSize > 0 && sr_ > 0) {
        algoLatencyMs = (static_cast<double>(blockSize) / 2.0)
                        / static_cast<double>(sr_) * 1000.0;
    }

    // 4) 총 편도 레이턴시
    double totalLatencyMs = baseOneWayMs + algoLatencyMs;

    // 5) 타깃 대비 사용량 / 최대 대비 마진
    double budgetUsed = 0.0;
    double marginMs   = 0.0;
    if (targetLatencyMs_ > 0.0)
        budgetUsed = totalLatencyMs / targetLatencyMs_;
    if (maxLatencyMs_ > 0.0)
        marginMs = maxLatencyMs_ - totalLatencyMs;

    csv_ << frameIndex_ << ","
         << std::fixed << std::setprecision(6) << cb_ms << ","
         << (underrun ? 1 : 0) << ","
         << blockSize << ","
         << (activeWork ? 1 : 0) << ","
         << periodMs << ","
         << cbMean_ << ","
         << loadRatio << ","
         << baseRtlMs << ","
         << baseOneWayMs << ","
         << algoLatencyMs << ","
         << totalLatencyMs << ","
         << targetLatencyMs_ << ","
         << maxLatencyMs_ << ","
         << budgetUsed << ","
         << marginMs << ","
         << switchReasonCode << "\n";

    frameIndex_ += hostFrames_;
}

void Metrics::closeCSV()
{
    if (csv_.is_open())
        csv_.close();
}
