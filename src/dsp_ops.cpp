// src/dsp_ops.cpp
#include "dsp_ops.hpp"
#include <cmath>
#include <algorithm>

namespace
{
    constexpr float kPi      = 3.14159265358979323846f;

    float timeToCoeff(float timeSec, double fs) {
        if (timeSec <= 0.0)
            return 0.0f;
        float x = std::exp(-1.0f / static_cast<float>(fs * timeSec));
        return x;
    }

    // ===== 스파이크 부하 설정 =====
    // 48kHz 기준: 5초마다 한 번 스파이크
    constexpr long long SPIKE_INTERVAL_FRAMES  = 5LL * 48000LL; // 5초
    // 스파이크가 유지되는 프레임 구간(대략 2000프레임)
    constexpr int        SPIKE_DURATION_FRAMES = 2000;
    // 스파이크 강도 조절용 반복 횟수
    // 머신/최적화에 따라 cb_ms를 보면서 조정할 것
    constexpr int        SPIKE_ITERATIONS      = 20000;

    int nextPow2(int n)
    {
        int v = 1;
        while (v < n) v <<= 1;
        return v;
    }
}

// ===== DspOps 구현 =====

DspOps::DspOps(int numChannels, double sampleRate)
    : ch_(numChannels)
    , fs_(sampleRate > 0.0 ? sampleRate : 48000.0)
    , hpf_a0_(0.0f)
    , hpf_b1_(0.0f)
    , lpf_a0_(0.0f)
    , lpf_b1_(0.0f)
    , inputGain_(1.0f)
    , outputGain_(1.0f)
    , fftSize_(0)
{
    if (ch_ <= 0) ch_ = 1;
    if (fs_ <= 0.0) fs_ = 48000.0;

    hpf_x1_.assign(ch_, 0.0f);
    hpf_y1_.assign(ch_, 0.0f);
    lpf_y1_.assign(ch_, 0.0f);

    setupFilters(fs_);
}

void DspOps::setupFilters(double sampleRate)
{
    // 예시: HPF 80Hz, LPF 12kHz
    // 아주 단순한 1차 IIR 구조: y[n] = a0 * x[n] + b1 * y[n-1] 형태로 쓸 수 있도록,
    // time constant 기반 계수 설계 예시를 사용한다.
    double hpfCut = 80.0;
    double lpfCut = 12000.0;

    // HPF: 1차 하이패스로 근사 (누산기형)
    {
        double rc   = 1.0 / (2.0 * kPi * hpfCut);
        double dt   = 1.0 / sampleRate;
        double alpha = rc / (rc + dt); // HPF 유사
        hpf_b1_ = static_cast<float>(alpha);
        hpf_a0_ = static_cast<float>(1.0 - alpha);
    }

    // LPF: 1차 로우패스
    {
        double rc   = 1.0 / (2.0 * kPi * lpfCut);
        double dt   = 1.0 / sampleRate;
        double alpha = dt / (rc + dt);
        lpf_a0_ = static_cast<float>(alpha);
        lpf_b1_ = static_cast<float>(1.0 - alpha);
    }
}

void DspOps::reset()
{
    std::fill(hpf_x1_.begin(), hpf_x1_.end(), 0.0f);
    std::fill(hpf_y1_.begin(), hpf_y1_.end(), 0.0f);
    std::fill(lpf_y1_.begin(), lpf_y1_.end(), 0.0f);
}

// 간단한 radix-2 FFT
void DspOps::fft(Complex* data, int n, bool inverse)
{
    if (n <= 1) return;

    // bit-reversal
    int j = 0;
    for (int i = 1; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }

    // Cooley–Tukey
    for (int len = 2; len <= n; len <<= 1) {
        float ang = static_cast<float>((inverse ? 2.0 : -2.0) * kPi / len);
        float wlenRe = std::cos(ang);
        float wlenIm = std::sin(ang);

        for (int i = 0; i < n; i += len) {
            float wRe = 1.0f;
            float wIm = 0.0f;
            for (int j2 = 0; j2 < len / 2; ++j2) {
                int u = i + j2;
                int v = i + j2 + len / 2;
                float ur = data[u].re;
                float ui = data[u].im;
                float vr = data[v].re * wRe - data[v].im * wIm;
                float vi = data[v].re * wIm + data[v].im * wRe;

                data[u].re = ur + vr;
                data[u].im = ui + vi;
                data[v].re = ur - vr;
                data[v].im = ui - vi;

                // w *= wlen
                float tmpRe = wRe * wlenRe - wIm * wlenIm;
                float tmpIm = wRe * wlenIm + wIm * wlenRe;
                wRe = tmpRe;
                wIm = tmpIm;
            }
        }
    }

    // inverse 시 1/N 스케일은 호출부에서 처리
}

float DspOps::softClip(float x)
{
    // 간단한 소프트 클리핑
    const float threshold = 1.0f;
    if (x > threshold)      x = threshold + (x - threshold) / (1.0f + (x - threshold)*(x - threshold));
    else if (x < -threshold)x = -threshold + (x + threshold) / (1.0f + (x + threshold)*(x + threshold));
    return x;
}

void DspOps::processBlock(const float* in, float* out, int numFrames, long long globalFrameIndex)
{
    if (!in || !out || numFrames <= 0)
        return;

    // === 0. 주기적인 스파이크 구간인지 확인 ===
    // block의 "시작 프레임" 기준으로만 간단히 판단
    const long long blockStartFrame = globalFrameIndex;
    const long long frameMod        = blockStartFrame % SPIKE_INTERVAL_FRAMES;

    bool spikeActive = (frameMod < SPIKE_DURATION_FRAMES);

    if (spikeActive)
    {
        // 순수 CPU 부하: DSP 상태에 영향 주지 않는 수학 연산
        // cb_ms가 눈에 띄게 튀도록 SPIKE_ITERATIONS를 조정
        double dummySum = 0.0;
        for (int i = 0; i < SPIKE_ITERATIONS; ++i)
        {
            double x = static_cast<double>(i);
            dummySum += std::sin(x * 0.001) / std::sqrt(x + 1.0);
        }
        (void)dummySum; // 최적화 방지용
    }

    // === 1. FFT 기반 스펙트럴 스테이지 (알고리즘 레이턴시 관찰용) ===

    const int N = numFrames;
    int fftSize = nextPow2(N);
    if (fftSize != fftSize_) {
        fftBuf_.assign(static_cast<std::size_t>(fftSize), Complex{0.0f, 0.0f});
        fftSize_ = fftSize;
    }

    // 채널별 처리
    for (int ch = 0; ch < ch_; ++ch) {

        // 1-A) 입력 + 시간영역 필터(HPF/LPF) 적용 후 FFT 버퍼에 기록
        for (int n = 0; n < fftSize; ++n) {
            float x = 0.0f;
            if (n < N) {
                x = in[n * ch_ + ch];
            }

            // Input gain
            x *= inputGain_;

            // HPF
            float y_hpf = hpf_a0_ * (x - hpf_x1_[ch]) + hpf_b1_ * hpf_y1_[ch];
            hpf_x1_[ch] = x;
            hpf_y1_[ch] = y_hpf;

            // LPF
            float y_lpf = lpf_a0_ * y_hpf + lpf_b1_ * lpf_y1_[ch];
            lpf_y1_[ch] = y_lpf;

            fftBuf_[n].re = y_lpf;
            fftBuf_[n].im = 0.0f;
        }

        // 1-B) FFT
        fft(fftBuf_.data(), fftSize, /*inverse*/ false);

        // 1-C) 간단한 스펙트럴 셰이핑:
        //      0Hz에서 1.0, Nyquist에서 0.3 정도로 떨어지는 tilt EQ
        for (int k = 0; k < fftSize; ++k) {
            float normFreq = static_cast<float>(k) / static_cast<float>(fftSize);
            float gain = 1.0f - 0.7f * normFreq;
            fftBuf_[k].re *= gain;
            fftBuf_[k].im *= gain;
        }

        // 1-D) IFFT
        fft(fftBuf_.data(), fftSize, /*inverse*/ true);

        // 1-E) 실수부 + 1/N 스케일링 + 소프트 클리핑 + Output gain
        for (int n = 0; n < N; ++n) {
            float y = fftBuf_[n].re / static_cast<float>(fftSize);
            y = softClip(y);
            y *= outputGain_;
            out[n * ch_ + ch] = y;
        }
    }
}
