#include "metrics.hpp"
#include <iomanip>

Metrics::Metrics(int sampleRate, int hostFrames)
    : sr_(sampleRate > 0 ? sampleRate : 48000),
      hostFrames_(hostFrames > 0 ? hostFrames : 256),
      periodMs_(0.0),
      frameIndex_(0)
{
    // 데드라인(ms) = hostFrames / sampleRate * 1000
    periodMs_ = (static_cast<double>(hostFrames_) /
                 static_cast<double>(sr_)) * 1000.0;
}

void Metrics::openCSV(const std::string& path) {
    csv_.open(path, std::ios::out | std::ios::trunc);

    // 확장된 메트릭 필드 헤더
    csv_ << "frame_index,"         // 전체 오디오 프레임 인덱스(x축)
         << "cb_ms,"               // 콜백 처리 시간(ms)
         << "underrun,"            // 언더런 여부(0/1)
         << "block_size,"          // 내부 block_size
         << "period_ms,"           // 데드라인(호스트 버퍼 주기)
         << "slack_ms,"            // 여유 시간 = period_ms - cb_ms
         << "cpu_util,"            // cb_ms / period_ms (0~1 이상)
         << "cpu_percent,"         // cpu_util * 100
         << "buffer_latency_ms"    // block_size 기반 이론 버퍼 레이턴시
         << "\n";
}

void Metrics::onCallbackEnd(double cb_ms, bool underrun, int blockSize) {
    if (!csv_.is_open())
        return;

    // 데드라인 대비 여유/부족
    double slack_ms = periodMs_ - cb_ms;

    // 데드라인 대비 콜백 점유 비율
    double cpu_util = (periodMs_ > 0.0) ? (cb_ms / periodMs_) : 0.0;
    double cpu_percent = cpu_util * 100.0;

    // 현재 block_size 기준 버퍼 레이턴시 (internal block에 해당하는 시간)
    double buffer_latency_ms =
        (static_cast<double>(blockSize) / static_cast<double>(sr_)) * 1000.0;

    csv_ << frameIndex_ << ","
         << std::fixed << std::setprecision(6) << cb_ms << ","
         << (underrun ? 1 : 0) << ","
         << blockSize << ","
         << std::setprecision(6) << periodMs_ << ","
         << std::setprecision(6) << slack_ms << ","
         << std::setprecision(6) << cpu_util << ","
         << std::setprecision(3) << cpu_percent << ","
         << std::setprecision(6) << buffer_latency_ms
         << "\n";

    // 다음 콜백의 frame_index = 현재 hostFrames만큼 증가
    frameIndex_ += hostFrames_;
}

void Metrics::closeCSV() {
    if (csv_.is_open())
        csv_.close();
}
