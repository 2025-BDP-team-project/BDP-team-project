// src/dsp_ops.cpp
#include "dsp_ops.hpp"
#include <cmath>
#include <algorithm>

namespace {
    constexpr float kEpsilon = 1e-12f;
    constexpr float kPi      = 3.14159265358979323846f;

    float dbToLin(float db) {
        return std::pow(10.0f, db / 20.0f);
    }

    float timeToCoeff(float timeSec, double fs) {
        if (timeSec <= 0.0)
            return 0.0f;
        float x = std::exp(-1.0f / static_cast<float>(fs * timeSec));
        return x;
    }
}

// ===================== ctor / setup =====================

DspOps::DspOps(int numChannels, double sampleRate)
    : ch_(numChannels),
      fs_(sampleRate),
      inputGain_(1.0f),
      outputGain_(1.0f),
      chorusMaxDelaySamples_(0),
      chorusWriteIndex_(0),
      delaySamples_(0),
      delayFeedback_(0.35f),
      delayMix_(0.25f),
      delayIndex_(0)
{
    if (ch_ <= 0) ch_ = 1;
    if (fs_ <= 0.0) fs_ = 48000.0;

    hpf_x1_.assign(ch_, 0.0f);
    hpf_y1_.assign(ch_, 0.0f);
    lpf_y1_.assign(ch_, 0.0f);
    compEnv_.assign(ch_, 0.0f);

    setupFilters();
    setupCompressor();
    setupFuzz();
    setupChorus();
    setupDelay();
}

void DspOps::setupFilters()
{
    // HPF: fc = 80 Hz (1차, simple high-pass)
    float fc_hpf = 80.0f;
    float theta = 2.0f * kPi * fc_hpf / static_cast<float>(fs_);
    float gamma = std::cos(theta) / (1.0f + std::sin(theta));
    hpf_a0_ = (1.0f + gamma) * 0.5f;
    hpf_a1_ = -hpf_a0_;
    hpf_b1_ = gamma;

    // LPF: fc = 12 kHz (1차, simple low-pass)
    float fc_lpf = 12000.0f;
    float x = std::exp(-2.0f * kPi * fc_lpf / static_cast<float>(fs_));
    lpf_a0_ = 1.0f - x;
    lpf_b1_ = x;
}

void DspOps::setupCompressor()
{
    // 대략적인 컴프 설정 (적당히 공격적)
    float thresholdDb = -18.0f;
    compRatio_        = 4.0f;
    float makeupDb    = 4.0f;
    float attackMs    = 5.0f;
    float releaseMs   = 80.0f;

    compThresholdLin_  = dbToLin(thresholdDb);
    compMakeupGain_    = dbToLin(makeupDb);
    compAttackCoeff_   = timeToCoeff(attackMs  / 1000.0f, fs_);
    compReleaseCoeff_  = timeToCoeff(releaseMs / 1000.0f, fs_);
}

void DspOps::setupFuzz()
{
    // 퍼즈는 pre-gain을 크게 주고, soft tanh 계열로 찌그러뜨리기
    fuzzPreGain_ = dbToLin(20.0f); // +20 dB
    fuzzMix_     = 0.7f;           // wet 70%
}

void DspOps::setupChorus()
{
    // 코러스 최대 딜레이: 약 30ms 정도
    float maxDelayMs = 30.0f;
    chorusMaxDelaySamples_ = static_cast<int>(fs_ * (maxDelayMs / 1000.0));
    if (chorusMaxDelaySamples_ < 1) chorusMaxDelaySamples_ = 1;

    chorusBuffer_.assign(chorusMaxDelaySamples_ * ch_, 0.0f);
    chorusWriteIndex_ = 0;

    // 2-voice chorus 설정
    chorusVoices_[0].baseDelayMs = 12.0f;
    chorusVoices_[0].depthMs     = 3.0f;
    chorusVoices_[0].lfoRateHz   = 0.25f;
    chorusVoices_[0].lfoPhase    = 0.0f;

    chorusVoices_[1].baseDelayMs = 18.0f;
    chorusVoices_[1].depthMs     = 5.0f;
    chorusVoices_[1].lfoRateHz   = 0.37f;
    chorusVoices_[1].lfoPhase    = 0.25f; // 다른 시작위상

    chorusMix_ = 0.35f; // dry/wet
}

void DspOps::setupDelay()
{
    // 약 400ms 딜레이
    double delaySec = 0.4;
    delaySamples_   = static_cast<int>(fs_ * delaySec);
    if (delaySamples_ < 1) delaySamples_ = 1;

    delayBuffer_.assign(delaySamples_ * ch_, 0.0f);
    delayIndex_ = 0;
}

void DspOps::reset()
{
    std::fill(hpf_x1_.begin(), hpf_x1_.end(), 0.0f);
    std::fill(hpf_y1_.begin(), hpf_y1_.end(), 0.0f);
    std::fill(lpf_y1_.begin(), lpf_y1_.end(), 0.0f);
    std::fill(compEnv_.begin(), compEnv_.end(), 0.0f);
    std::fill(chorusBuffer_.begin(), chorusBuffer_.end(), 0.0f);
    std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.0f);
    delayIndex_ = 0;
    chorusWriteIndex_ = 0;
}

// ===================== helpers =====================

float DspOps::softClip(float x)
{
    // 부드러운 soft clip
    float ax = std::fabs(x);
    return x / (1.0f + ax);
}

float DspOps::fastTanh(float x)
{
    // 퍼즈용 tanh 근사 (빠르긴 한데 여기선 그냥 std::tanh 써도 괜찮음)
    // return std::tanh(x);
    // 간단한 근사: x * (27 + x^2) / (27 + 9x^2)
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// ===================== main processing =====================

void DspOps::processBlock(const float* in, float* out, int numFrames)
{
    if (!in || !out || numFrames <= 0)
        return;

    for (int n = 0; n < numFrames; ++n)
    {
        // Long delay index: frame 기준
        int delayFrameIdx = delayIndex_;
        if (++delayIndex_ >= delaySamples_)
            delayIndex_ = 0;

        // Chorus write index: frame 기준
        int chorusFrameIdx = chorusWriteIndex_;
        if (++chorusWriteIndex_ >= chorusMaxDelaySamples_)
            chorusWriteIndex_ = 0;

        // LFO phase 증가 (sample-based)
        float dt = 1.0f / static_cast<float>(fs_);
        for (int v = 0; v < 2; ++v)
        {
            ChorusVoice& voice = chorusVoices_[v];
            voice.lfoPhase += voice.lfoRateHz * dt;
            if (voice.lfoPhase >= 1.0f)
                voice.lfoPhase -= 1.0f;
        }

        for (int c = 0; c < ch_; ++c)
        {
            int idx = n * ch_ + c;
            float x = in[idx];

            // 1) Input gain
            x *= inputGain_;

            // 2) HPF (1차)
            {
                float y = hpf_a0_ * x + hpf_a1_ * hpf_x1_[c] + hpf_b1_ * hpf_y1_[c];
                hpf_x1_[c] = x;
                hpf_y1_[c] = y;
                x = y;
            }

            // 3) LPF (1차)
            {
                float y = lpf_a0_ * x + lpf_b1_ * lpf_y1_[c];
                lpf_y1_[c] = y;
                x = y;
            }

            // 4) Compressor
            {
                float absx = std::fabs(x) + kEpsilon;
                float env  = compEnv_[c];

                if (absx > env)
                {
                    env = compAttackCoeff_ * env + (1.0f - compAttackCoeff_) * absx;
                }
                else
                {
                    env = compReleaseCoeff_ * env + (1.0f - compReleaseCoeff_) * absx;
                }

                compEnv_[c] = env;

                float gain = 1.0f;
                if (env > compThresholdLin_)
                {
                    float over = env / compThresholdLin_;
                    float g = std::pow(over, (1.0f / compRatio_ - 1.0f));
                    gain = g;
                }
                x *= (gain * compMakeupGain_);
            }

            // 5) Fuzz (pre-gain + waveshaper, dry/wet)
            {
                float dry = x;
                float driven = x * fuzzPreGain_;
                float shaped = fastTanh(driven);
                x = dry * (1.0f - fuzzMix_) + shaped * fuzzMix_;
            }

            // 6) Chorus (2-voice, modulated delay, per-channel)
            float chorusOut = 0.0f;
            {
                // write current sample into chorus buffer
                int wIdx = chorusFrameIdx * ch_ + c;
                chorusBuffer_[wIdx] = x;

                float accum = 0.0f;

                for (int v = 0; v < 2; ++v)
                {
                    const ChorusVoice& voice = chorusVoices_[v];

                    float lfo = std::sin(voice.lfoPhase * 2.0f * kPi); // -1..1
                    float delayMs = voice.baseDelayMs + voice.depthMs * lfo;
                    if (delayMs < 0.0f) delayMs = 0.0f;

                    float delaySamples = static_cast<float>(fs_) * (delayMs / 1000.0f);
                    if (delaySamples > static_cast<float>(chorusMaxDelaySamples_ - 1))
                        delaySamples = static_cast<float>(chorusMaxDelaySamples_ - 1);

                    // read position = writeIndex - delaySamples
                    float readPos = static_cast<float>(chorusFrameIdx) - delaySamples;
                    while (readPos < 0.0f)
                        readPos += static_cast<float>(chorusMaxDelaySamples_);

                    int idx0 = static_cast<int>(readPos);
                    int idx1 = idx0 + 1;
                    if (idx1 >= chorusMaxDelaySamples_)
                        idx1 = 0;

                    float frac = readPos - static_cast<float>(idx0);

                    int r0 = idx0 * ch_ + c;
                    int r1 = idx1 * ch_ + c;

                    float s0 = chorusBuffer_[r0];
                    float s1 = chorusBuffer_[r1];

                    float sample = s0 + (s1 - s0) * frac;
                    accum += sample;
                }

                chorusOut = accum * 0.5f; // average voices
                float dry = x;
                x = dry * (1.0f - chorusMix_) + chorusOut * chorusMix_;
            }

            // 7) Long delay (feedback + dry/wet)
            {
                int dIdx = delayFrameIdx * ch_ + c;
                float delayed = delayBuffer_[dIdx];

                float writeVal = x + delayed * delayFeedback_;
                delayBuffer_[dIdx] = writeVal;

                float dry = x;
                float wet = delayed;
                x = dry * (1.0f - delayMix_) + wet * delayMix_;
            }

            // 8) Soft clipping + Output gain
            x = softClip(x);
            x *= outputGain_;

            out[idx] = x;
        }
    }
}
