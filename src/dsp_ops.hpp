// src/dsp_ops.hpp
#pragma once
#include <vector>

class DspOps
{
public:
    DspOps(int numChannels, double sampleRate);

    void reset();

    // in/out: interleaved float (frames x channels)
    void processBlock(const float* in, float* out, int numFrames);

private:
    int    ch_;
    double fs_;

    // ==== Global Gains ====
    float inputGain_;
    float outputGain_;

    // ==== HPF / LPF ====
    float hpf_a0_, hpf_a1_, hpf_b1_;
    std::vector<float> hpf_x1_; // prev input
    std::vector<float> hpf_y1_; // prev output

    float lpf_a0_, lpf_b1_;
    std::vector<float> lpf_y1_; // prev output

    // ==== Compressor ====
    float compThresholdLin_;
    float compRatio_;
    float compMakeupGain_;
    float compAttackCoeff_;
    float compReleaseCoeff_;
    std::vector<float> compEnv_; // per-channel envelope

    // ==== Fuzz / Distortion ====
    float fuzzPreGain_;   // input gain before waveshaper
    float fuzzMix_;       // dry/wet mix

    // ==== Chorus ====
    struct ChorusVoice
    {
        float baseDelayMs;   // base delay time (ms)
        float depthMs;       // modulation depth (ms)
        float lfoPhase;      // current LFO phase [0, 1)
        float lfoRateHz;     // LFO rate
    };

    int   chorusMaxDelaySamples_;
    std::vector<float> chorusBuffer_;  // size = maxDelay * ch_
    int   chorusWriteIndex_;
    ChorusVoice chorusVoices_[2];      // 2-voice chorus
    float chorusMix_;

    // ==== Long Delay ====
    int   delaySamples_;
    float delayFeedback_;
    float delayMix_;
    std::vector<float> delayBuffer_; // size = delaySamples_ * ch_
    int   delayIndex_;               // frame-based write index

    // ==== Helpers ====
    void setupFilters();
    void setupCompressor();
    void setupFuzz();
    void setupChorus();
    void setupDelay();

    static float softClip(float x);
    static float fastTanh(float x);   // fuzz용 근사 tanh
};
