
#include "frame_router.hpp"
#include <algorithm>
#include <cstring>
FrameRouter::FrameRouter(int sampleRate, int channels) : sr_(sampleRate), ch_(channels) {
    blockIn_.resize(internalBlock_ * ch_);
    blockOut_.resize(internalBlock_ * ch_);
    initRingBuffers();
}
void FrameRouter::setInternalBlockSize(int n) {
    if (n <= 0) return; internalBlock_ = n;
    blockIn_.assign(internalBlock_ * ch_, 0.0f);
    blockOut_.assign(internalBlock_ * ch_, 0.0f);
}
void FrameRouter::setOverlap(float ov) { overlap_ = std::max(0.0f, std::min(0.95f, ov)); }
void FrameRouter::pushInput(const float* interleaved, size_t frames) {
    size_t totalSamples = frames * ch_;
    size_t written = inRing_.push(interleaved, totalSamples);
    if (written < totalSamples) {
        // Ring capacity is intentionally oversized to avoid overflow in offline use.
        // If it still happens, fall back to synchronous handling of the remaining samples.
        const float* remain = interleaved + written;
        size_t remaining = totalSamples - written;
        while (remaining > 0) {
            size_t advance = inRing_.push(remain, remaining);
            if (advance == 0) break;
            remain += advance;
            remaining -= advance;
        }
    }
}
void FrameRouter::processPendingBlocks(DspOps& dsp) {
    const size_t blockSamples = static_cast<size_t>(internalBlock_) * ch_;
    while (availableInFrames() >= static_cast<size_t>(internalBlock_)) {
        size_t popped = inRing_.pop(blockIn_.data(), blockSamples);
        if (popped < blockSamples) {
            break;
        }
        dsp.processBlock(blockIn_.data(), blockOut_.data(), internalBlock_);
        writeToOutRing(blockOut_.data(), internalBlock_);
    }
}
void FrameRouter::pullOutput(float* interleaved, size_t frames) {
    size_t got = readFromOutRing(interleaved, frames);
    if (got < frames) {
        size_t remain = frames - got;
        if (got > 0) {
            float* last = interleaved + (got-1)*ch_;
            for (size_t i=0;i<remain;i++) std::memcpy(interleaved + (got+i)*ch_, last, ch_*sizeof(float));
        } else {
            std::memset(interleaved, 0, frames * ch_ * sizeof(float));
        }
    }
}
void FrameRouter::writeToOutRing(const float* src, size_t frames) {
    size_t samples = frames * ch_;
    size_t written = outRing_.push(src, samples);
    if (written < samples) {
        const float* remain = src + written;
        size_t remaining = samples - written;
        while (remaining > 0) {
            size_t advance = outRing_.push(remain, remaining);
            if (advance == 0) break;
            remain += advance;
            remaining -= advance;
        }
    }
}
size_t FrameRouter::readFromOutRing(float* dst, size_t frames) {
    size_t requestedSamples = frames * ch_;
    size_t popped = outRing_.pop(dst, requestedSamples);
    return popped / ch_;
}

size_t FrameRouter::availableInFrames() const {
    return inRing_.size() / static_cast<size_t>(ch_);
}

void FrameRouter::initRingBuffers() {
    const size_t safetyFrames = std::max(4096, sr_); // at least ~1s of audio or 4096 frames
    const size_t ringCapacitySamples = static_cast<size_t>(safetyFrames) * ch_ * 4; // generous headroom for overlap
    inRing_.reset(ringCapacitySamples);
    outRing_.reset(ringCapacitySamples);
}
