#pragma once
#include <algorithm>
#include <vector>

class DspOps {
public:
    explicit DspOps(int channels);

    void setWorkerThreads(int threads) { workerThreads_ = std::max(1, threads); }

    // Busy-wait 기반 부하 시뮬레이션을 켜거나 끕니다.
    // 병렬화 테스트 시 실제 DSP만 측정하기 위해 false로 설정할 수 있습니다.
    void setSimulateLoad(bool enable) { simulateLoad_ = enable; }
    
    // [설정] 부하 생성의 정규화를 위해 전체 파일 길이를 설정합니다.
    // 파일 길이에 상관없이 동일한 패턴의 파동을 만들기 위함입니다.
    void setTotalFrames(long long total) { 
        totalFrames_ = total; 
        processedFrames_ = 0; 
    }
    
    void processBlock(const float* inInterleaved, float* outInterleaved, int frames);

private:
    int ch_;
    std::vector<float> z_;
    float alpha_ = 0.1f;

    int workerThreads_ = 1;
    bool simulateLoad_ = true;

    // 진행률(Progress) 계산을 위한 변수
    long long totalFrames_ = 0;
    long long processedFrames_ = 0;
};