#pragma once
#include <deque>

class AbscController {
public:
    AbscController(int sampleRate, int historySize = 50);

    void setBlockSizeOptions(int small, int normal, int large);
    int onCallbackEnd(double cb_ms);
    int currentBlockSize() const { return currentBlockSize_; }
    
private:
    std::deque<double> cbHistory_; 
    int historySize_;              
    double cbMean_ = 0.0;          

    int smallBlock_ = 128;
    int normalBlock_ = 256;
    int largeBlock_ = 512;

    int currentBlockSize_;         
    long long framesSinceSwitch_ = 0; 
    long long cooldownFrames_;             

    // [임계값 설정]
    // 128 블록의 한계(2.6ms)를 고려하여 기준을 대폭 낮췄습니다.
    const double upperThreshold_ = 0.30; // 부하가 30%만 넘어도 즉시 방어 태세 (Scale-up)
    const double lowerThreshold_ = 0.15; // 부하가 15%까지 떨어져야 복귀 (Scale-down, 핑퐁 방지)

    void updateMean(double new_cb_ms);
    void updateBlockSize(double target_ms);
};