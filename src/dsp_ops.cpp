#include "dsp_ops.hpp"
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <algorithm>

DspOps::DspOps(int channels) : ch_(channels), z_(channels, 0.0f) {}

void DspOps::processBlock(const float* inInterleaved, float* outInterleaved, int frames) {
    
    // 1. 진행률(Progress) 계산 (0.0 ~ 1.0)
    double progress = 0.0;
    if (totalFrames_ > 0) {
        progress = (double)processedFrames_ / (double)totalFrames_;
    }

    // 2. 부하 계수 생성 (Sine Wave)
    // 전체 파일 길이에 맞춰 정확히 4번 출렁이도록 파동을 만듭니다.
    float phase = (float)(progress * 4.0 * 2.0 * 3.1415926535);
    float loadFactor = (std::sin(phase) + 1.0f) * 0.5f; 

    // 3. 목표 지연 시간(Target Delay) 설정
    // 평소(0.5ms): 128 블록(한계 2.6ms)도 여유롭게 처리 가능 -> Low Latency 달성
    // 피크(9.0ms): 256 블록(한계 5.3ms)은 감당 불가 -> Fixed 모드 붕괴
    //             512 블록(한계 10.6ms)은 생존 가능 -> Adaptive 모드 생존
    double targetDelayMs = 0.5 + (loadFactor * 8.5);

    if (simulateLoad_) {
        // 4. 부하 시뮬레이션 (Busy Wait)
        // 컴퓨터 사양과 무관하게 항상 일정한 CPU 부하를 발생시킵니다.
        auto start = std::chrono::high_resolution_clock::now();
        while (true) {
            volatile float dummy = 0.0f;
            for(int i=0; i<100; ++i) dummy += 1.0f; // 컴파일러 최적화 방지

            auto now = std::chrono::high_resolution_clock::now();
            // 목표 시간이 될 때까지 루프를 돌며 CPU를 점유합니다.
            if (std::chrono::duration<double, std::milli>(now - start).count() >= targetDelayMs) {
                break;
            }
        }
    }

    auto processRange = [&](int chBegin, int chEnd) {
        for (int n = 0; n < frames; ++n) {
            for (int c = chBegin; c < chEnd; ++c) {
                float x = inInterleaved[n * ch_ + c];
                z_[c] = z_[c] + alpha_ * (x - z_[c]);
                outInterleaved[n * ch_ + c] = z_[c];
            }
        }
    };

    if (workerThreads_ <= 1 || ch_ == 1) {
        processRange(0, ch_);
    } else {
        int threads = std::min(workerThreads_, ch_);
        int base = ch_ / threads;
        int extra = ch_ % threads;
        std::vector<std::thread> pool;
        int startCh = 0;
        for (int t = 0; t < threads; ++t) {
            int count = base + (t < extra ? 1 : 0);
            int endCh = startCh + count;
            if (count <= 0) break;

            if (t + 1 == threads) {
                processRange(startCh, endCh);
            } else {
                pool.emplace_back(processRange, startCh, endCh);
            }
            startCh = endCh;
        }
        for (auto& th : pool) th.join();
    }

    processedFrames_ += frames;
}