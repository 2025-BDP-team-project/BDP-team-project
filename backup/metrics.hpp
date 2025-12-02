#pragma once
#include <fstream>

class Metrics {
public:
    Metrics(int sampleRate, int hostFrames) : sr_(sampleRate), hostFrames_(hostFrames) {}

    // 파일 스트림을 보유하므로 복사/이동을 명시적으로 금지하여 수명 관리를 단순화
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;
    Metrics(Metrics&&) = delete;
    Metrics& operator=(Metrics&&) = delete;

    void openCSV(const std::string& path);
    
    // ABSC 상태(cb_mean, switchReason)까지 포함하여 한 콜백당 한 줄 기록
    void onCallbackEnd(double cb_ms,
                       bool underrun,
                       int blockSize,
                       bool activeWork,
                       double cb_mean,
                       int switchReasonCode);
                       
    void closeCSV();
private:
    int sr_, hostFrames_;
    std::ofstream csv_;
    long long frameIndex_ = 0;
    double periodMs_      = 0.0; // 주기(Deadline) 저장용
};
