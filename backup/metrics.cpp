#include "metrics.hpp"
#include <iomanip>

void Metrics::openCSV(const std::string& path) {
    csv_.open(path, std::ios::out | std::ios::trunc);
    // 수정: 확장된 헤더 (absc_mean, switch_reason 추가)
    csv_ << "frame_index,cb_ms,underrun,block_size,active_work,period_ms,cb_mean,block_switch_reason\n";
    
    // periodMs 미리 계산 (hostFrames / sampleRate * 1000)
    if (sr_ > 0) {
        periodMs_ = (static_cast<double>(hostFrames_) / static_cast<double>(sr_)) * 1000.0;
    }
}

void Metrics::onCallbackEnd(double cb_ms, 
                            bool underrun, 
                            int blockSize, 
                            bool activeWork, 
                            double cb_mean, 
                            int switchReasonCode) 
{
    if (!csv_.is_open()) return;

    csv_ << frameIndex_ << "," 
         << std::fixed << std::setprecision(6) << cb_ms << ","
         << (underrun ? 1 : 0) << "," 
         << blockSize << ","
         << (activeWork ? 1 : 0) << ","
         << std::fixed << std::setprecision(6) << periodMs_ << ","
         << std::fixed << std::setprecision(6) << cb_mean << ","
         << switchReasonCode << "\n";
         
    frameIndex_ += hostFrames_;
}

void Metrics::closeCSV() { if (csv_.is_open()) csv_.close(); }