#pragma once
#include <vector>
#include "dsp_ops.hpp"

class FrameRouter {
public:
    FrameRouter(int sampleRate, int channels);
    void setInternalBlockSize(int n);
    void setOverlap(float ov);
    int  currentBlockSize() const { return internalBlock_; }
    void pushInput(const float* interleaved, size_t frames);
    void processPendingBlocks(DspOps& dsp, long long globalFrameIndex);
    void pullOutput(float* interleaved, size_t frames);
private:
    int sr_, ch_;
    int internalBlock_ = 256;
    float overlap_ = 0.5f;
    std::vector<float> inRing_, outRing_, blockIn_, blockOut_;
    size_t inSize_ = 0, outSize_ = 0;
    void writeToOutRing(const float* src, size_t frames);
    size_t readFromOutRing(float* dst, size_t frames);
};
