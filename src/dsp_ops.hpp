#pragma once
#include <vector>

class DspOps {
public:
    explicit DspOps(int channels);
    
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

    // 진행률(Progress) 계산을 위한 변수
    long long totalFrames_ = 0;
    long long processedFrames_ = 0;
};