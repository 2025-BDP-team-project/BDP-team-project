// src/dsp_ops.hpp
#pragma once

#include <vector>

class DspOps
{
public:
    DspOps(int numChannels, double sampleRate);

    void reset();

    // in/out: interleaved float (frames x channels)
    // globalFrameIndex: 이 블록의 첫 샘플(frame)의 "전체 스트림 기준" 인덱스
    void processBlock(const float* in, float* out, int numFrames, long long globalFrameIndex);

private:
    int    ch_;
    double fs_;

    // === 필터 상태 (채널별) ===
    std::vector<float> hpf_x1_;
    std::vector<float> hpf_y1_;
    std::vector<float> lpf_y1_;

    float hpf_a0_;
    float hpf_b1_;
    float lpf_a0_;
    float lpf_b1_;

    // === 입출력 게인 ===
    float inputGain_;
    float outputGain_;

    // === FFT 버퍼 (알고리즘 레이턴시 관찰용 스펙트럴 스테이지) ===
    struct Complex {
        float re;
        float im;
    };
    std::vector<Complex> fftBuf_;
    int fftSize_;

    // ===== Helpers =====
    void setupFilters(double sampleRate);

    // 간단한 radix-2 FFT
    void fft(Complex* data, int n, bool inverse);

    static float softClip(float x);
};
