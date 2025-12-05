
#pragma once
#include <algorithm>
#include <atomic>
#include <vector>
#include <cstring>
#include "dsp_ops.hpp"

class LockFreeRing {
public:
    LockFreeRing() = default;

    explicit LockFreeRing(size_t capacity) { reset(capacity); }

    void reset(size_t capacity) {
        capacity_ = nextPow2(std::max<size_t>(1, capacity));
        mask_ = capacity_ - 1;
        buffer_.assign(capacity_, 0.0f);
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    size_t capacity() const { return capacity_; }

    size_t size() const {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);
        return h - t;
    }

    size_t push(const float* src, size_t count) {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_acquire);
        size_t freeSpace = capacity_ - (h - t);
        size_t toWrite = std::min(freeSpace, count);

        size_t idx = h & mask_;
        size_t firstPart = std::min(toWrite, capacity_ - idx);
        std::memcpy(buffer_.data() + idx, src, firstPart * sizeof(float));
        if (toWrite > firstPart) {
            std::memcpy(buffer_.data(), src + firstPart, (toWrite - firstPart) * sizeof(float));
        }

        head_.store(h + toWrite, std::memory_order_release);
        return toWrite;
    }

    size_t pop(float* dst, size_t count) {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t avail = h - t;
        size_t toRead = std::min(avail, count);

        size_t idx = t & mask_;
        size_t firstPart = std::min(toRead, capacity_ - idx);
        std::memcpy(dst, buffer_.data() + idx, firstPart * sizeof(float));
        if (toRead > firstPart) {
            std::memcpy(dst + firstPart, buffer_.data(), (toRead - firstPart) * sizeof(float));
        }

        tail_.store(t + toRead, std::memory_order_release);
        return toRead;
    }

private:
    static size_t nextPow2(size_t v) {
        if (v == 0) return 1;
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        if constexpr (sizeof(size_t) == 8) {
            v |= v >> 32;
        }
        return v + 1;
    }

    std::vector<float> buffer_;
    size_t capacity_ = 0;
    size_t mask_ = 0;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
};

class FrameRouter {
public:
    FrameRouter(int sampleRate, int channels);
    void setInternalBlockSize(int n);
    void setOverlap(float ov);
    int  currentBlockSize() const { return internalBlock_; }
    void pushInput(const float* interleaved, size_t frames);
    void processPendingBlocks(DspOps& dsp);
    void pullOutput(float* interleaved, size_t frames);
private:
    int sr_, ch_;
    int internalBlock_ = 256;
    float overlap_ = 0.5f;
    LockFreeRing inRing_, outRing_;
    std::vector<float> blockIn_, blockOut_;
    void writeToOutRing(const float* src, size_t frames);
    size_t readFromOutRing(float* dst, size_t frames);
    size_t availableInFrames() const;
    void initRingBuffers();
};
