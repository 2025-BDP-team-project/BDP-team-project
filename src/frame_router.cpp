#include "frame_router.hpp"
#include <algorithm>
#include <cstring>

FrameRouter::FrameRouter(int sampleRate, int channels)
    : sr_(sampleRate), ch_(channels)
{
    blockIn_.resize(internalBlock_ * ch_);
    blockOut_.resize(internalBlock_ * ch_);
}

void FrameRouter::setInternalBlockSize(int n) {
    if (n <= 0) return;
    internalBlock_ = n;
    blockIn_.assign(internalBlock_ * ch_, 0.0f);
    blockOut_.assign(internalBlock_ * ch_, 0.0f);
}

void FrameRouter::setOverlap(float ov) { 
    // TODO: 현재 프레임 라우팅 경로에서는 overlap_ 값은 사용되지 않는다.
    // 향후 STFT/OLA 구조로 확장할 때를 대비해 파라미터만 보존한다.
    overlap_ = std::max(0.0f, std::min(0.95f, ov)); 
}

void FrameRouter::pushInput(const float* interleaved, size_t frames) {
    size_t old = inRing_.size();
    inRing_.resize(old + frames * ch_);
    std::memcpy(inRing_.data() + old, interleaved, frames * ch_ * sizeof(float));
    inSize_ += frames;
}

void FrameRouter::processPendingBlocks(DspOps& dsp, long long globalFrameIndex)
{
    long long frameCursor = 0;  
    // frameCursor = hostBlock 내부에서 처리한 누적 프레임 수
    // globalFrameIndex + frameCursor = 내부 블록의 전역 시작 프레임

    while (inSize_ >= static_cast<size_t>(internalBlock_)) {

        // 1) 입력 블록 추출
        std::memcpy(blockIn_.data(),
                    inRing_.data(),
                    internalBlock_ * ch_ * sizeof(float));

        // inRing 소비: 요소 개수 기준으로 이동/축소
        const size_t consume = static_cast<size_t>(internalBlock_) * static_cast<size_t>(ch_);
        if (inRing_.size() > consume) {
            std::memmove(inRing_.data(),
                         inRing_.data() + consume,
                         (inRing_.size() - consume) * sizeof(float));
        }
        inRing_.resize(inRing_.size() - consume);
        inSize_ -= internalBlock_;

        // 2) 이 내부 블록의 전역 프레임 시작 위치
        long long blockGlobalFrame = globalFrameIndex + frameCursor;

        // 3) DSP 호출 (globalFrameIndex 전달)
        dsp.processBlock(
            blockIn_.data(),
            blockOut_.data(),
            internalBlock_,
            blockGlobalFrame   // 전역 프레임 인덱스
        );

        // 4) 출력 링버퍼에 기록
        writeToOutRing(blockOut_.data(), internalBlock_);

        frameCursor += internalBlock_;
    }
}

void FrameRouter::pullOutput(float* interleaved, size_t frames) {
    size_t got = readFromOutRing(interleaved, frames);
    if (got < frames) {
        size_t remain = frames - got;
        if (got > 0) {
            float* last = interleaved + (got - 1) * ch_;
            for (size_t i = 0; i < remain; ++i) {
                std::memcpy(interleaved + (got + i) * ch_, last, ch_ * sizeof(float));
            }
        } else {
            std::memset(interleaved, 0, frames * ch_ * sizeof(float));
        }
    }
}

void FrameRouter::writeToOutRing(const float* src, size_t frames) {
    size_t old = outRing_.size();
    outRing_.resize(old + frames * ch_);
    std::memcpy(outRing_.data() + old, src, frames * ch_ * sizeof(float));
    outSize_ += frames;
}

size_t FrameRouter::readFromOutRing(float* dst, size_t frames) {
    size_t avail = std::min(outSize_, frames);
    if (avail > 0) {
        std::memcpy(dst, outRing_.data(), avail * ch_ * sizeof(float));
        size_t consume = avail * ch_;
        if (outRing_.size() > consume) {
            std::memmove(outRing_.data(),
                         outRing_.data() + consume,
                         (outRing_.size() - consume) * sizeof(float));
        }
        outRing_.resize(outRing_.size() - consume);
        outSize_ -= avail;
    }
    return avail;
}
