#include "dsp_ops.hpp"
#include <cstring>
#include <cmath> // CPU 시간을 소모하는 수학 함수를 사용하기 위해 추가

// CPU 부하를 인위적으로 늘리는 반복 횟수 (이 숫자를 조정하세요: 800부터 시작 권장)
static constexpr int LOAD_ITERATIONS = 50000; 

DspOps::DspOps(int channels) : ch_(channels), z_(channels, 0.0f) {}

void DspOps::processBlock(const float* inInterleaved, float* outInterleaved, int frames) {
    
    // 1. [순수한 CPU 부하 생성] - DSP 상태를 오염시키지 않으면서 시간 소비
    for (int load_i = 0; load_i < LOAD_ITERATIONS; ++load_i) { 
        for (int c = 0; c < ch_; ++c) {
            // 부동 소수점 연산을 통해 CPU 시간 소비
            float dummy = std::sin(z_[c]) * std::cos(z_[c]); 
            z_[c] += dummy * 0.00001f; 
        }
    }

    // 2. [원래의 Low-pass DSP 로직 실행] (단 한 번만)
    for (int n = 0; n < frames; ++n) {
        for (int c = 0; c < ch_; ++c) {
            float x = inInterleaved[n * ch_ + c];
            
            // 1-pole lowpass 계산
            z_[c] = z_[c] + alpha_ * (x - z_[c]);
            
            outInterleaved[n * ch_ + c] = z_[c]; 
        }
    }
}