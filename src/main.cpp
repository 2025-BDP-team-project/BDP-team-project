// src/main.cpp
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <iostream>

// 프로젝트 헤더 파일들
#include "file_source.hpp"
#include "file_sink.hpp"
#include "frame_router.hpp"
#include "dsp_ops.hpp"
#include "metrics.hpp"
#include "absc_controller.hpp"

using Clock      = std::chrono::high_resolution_clock;
using TimePoint  = std::chrono::time_point<Clock>;
using MilliFloat = std::chrono::duration<double, std::milli>;

// 전체 실험 루프를 함수로 분리 (모드와 파일명을 인수로 받음)
bool run_experiment(
    const std::string& inPath,
    const std::string& outPath,
    const std::string& metricsPath, // CSV 파일명
    int   hostFrames,
    int   initialInternalBlock,
    float overlap,
    bool  enableAbsc // ABSC 활성화 여부
)
{
    std::cout << "\n====================================================\n";
    std::cout << (enableAbsc ? "[Experiment] Adaptive (ABSC) Mode\n"
                             : "[Experiment] Fixed (Baseline) Mode\n");
    std::cout << "  Input        : " << inPath      << "\n";
    std::cout << "  Output       : " << outPath     << "\n";
    std::cout << "  Metrics CSV  : " << metricsPath << "\n";
    std::cout << "  hostFrames   : " << hostFrames  << "\n";
    std::cout << "  initBlock    : " << initialInternalBlock << "\n";
    std::cout << "  overlap      : " << overlap     << "\n";
    std::cout << "====================================================\n";

    FileSource src;
    if (!src.open(inPath)) {
        std::fprintf(stderr, "Failed to open input: %s\n", inPath.c_str());
        return false;
    }

    const int sampleRate = src.sampleRate();
    const int channels   = src.channels();
    const long long totalFrames = src.frames();

    if (sampleRate <= 0 || channels <= 0) {
        std::fprintf(stderr, "Invalid WAV format (sampleRate=%d, channels=%d)\n",
                     sampleRate, channels);
        return false;
    }

    // 호스트 콜백 주기 및 언더런 기준
    const double periodMs = (static_cast<double>(hostFrames) /
                             static_cast<double>(sampleRate)) * 1000.0;
    const double underrunThresholdMs = 0.95 * periodMs;

    std::cout << "[Info] SampleRate=" << sampleRate
              << " Hz, Channels=" << channels << "\n";
    std::cout << "[Info] Host callback period ≈ "
              << periodMs << " ms (underrun threshold: "
              << underrunThresholdMs << " ms)\n";

    FileSink sink;
    if (!sink.open(outPath, sampleRate, channels)) {
        std::fprintf(stderr, "Failed to open output: %s\n", outPath.c_str());
        return false;
    }

    FrameRouter router(sampleRate, channels);
    router.setOverlap(overlap);

    DspOps dsp(channels, static_cast<double>(sampleRate));

    // Latency Budget 설정값 (Fixed/Adaptive 공통 분석 기준)
    const double targetLat = 10.0; // 편도 10ms 목표
    const double maxLat    = 15.0; // 편도 15ms 상한


    Metrics metrics(sampleRate, hostFrames, targetLat, maxLat);
    metrics.openCSV(metricsPath);

    AbscController controller(sampleRate);
    if (enableAbsc) {
        controller.setHostConfig(sampleRate, hostFrames);
        controller.setLatencyTargets(targetLat, maxLat);
        controller.setBlockSizeOptions(64, initialInternalBlock, 128);

        router.setInternalBlockSize(controller.currentBlockSize());
        std::cout << "[ABSC] Initial internal block size = "
                  << controller.currentBlockSize() << " frames\n";
    } else {
        router.setInternalBlockSize(initialInternalBlock);
        std::cout << "[FIXED] Internal block size = "
                  << initialInternalBlock << " frames\n";
    }

    std::vector<float> inBuf (static_cast<std::size_t>(hostFrames * channels));
    std::vector<float> outBuf(static_cast<std::size_t>(hostFrames * channels));

    long long processed        = 0;
    long long globalFrameIndex = 0;

    auto wallStart = Clock::now();

    while (processed < totalFrames) {
        std::size_t need = static_cast<std::size_t>(hostFrames);
        std::size_t got  = src.readFrames(inBuf.data(), need);

        if (got < need) {
            std::fill(inBuf.begin() + got * channels, inBuf.end(), 0.0f);
        }

        TimePoint t0 = Clock::now();

        router.pushInput(inBuf.data(), hostFrames);
        router.processPendingBlocks(dsp, globalFrameIndex);
        router.pullOutput(outBuf.data(), hostFrames);

        TimePoint t1 = Clock::now();

        double cb_ms = MilliFloat(t1 - t0).count();
        bool underrun = cb_ms > underrunThresholdMs;

        sink.writeFrames(outBuf.data(), hostFrames);

        int  recordedBlockSize = router.currentBlockSize();
        bool isDSPExecuted     = true; // 이 하네스에서는 항상 DSP 수행

        if (enableAbsc) {
            AbscSwitchReason reasonCode;
            int nextInternalBlockSize =
                controller.onCallbackEnd(cb_ms, hostFrames, underrun, reasonCode);

            metrics.onCallbackEnd(cb_ms,
                                underrun,
                                recordedBlockSize,
                                isDSPExecuted,
                                static_cast<int>(reasonCode));

            router.setInternalBlockSize(nextInternalBlockSize);
        } else {
            metrics.onCallbackEnd(cb_ms,
                                underrun,
                                recordedBlockSize,
                                isDSPExecuted,
                                0); // Fixed: switchReasonCode = 0 (NO_SWITCH)
        }


        processed        += hostFrames;
        globalFrameIndex += hostFrames;
    }

    auto wallEnd = Clock::now();
    double wall_ms = MilliFloat(wallEnd - wallStart).count();

    std::cout << "\n[Summary] "
              << (enableAbsc ? "Adaptive (ABSC)" : "Fixed (Baseline)") << " Mode\n";
    std::cout << "  Total processed frames : " << processed
              << " / " << totalFrames << "\n";
    std::cout << "  Wall-clock time        : " << wall_ms << " ms\n";

    metrics.closeCSV();
    src.close();
    sink.close();

    return true;
}

int main(int argc, char** argv)
{
    if (argc < 5) {
        std::fprintf(stderr,
            "Usage: %s <input_wav> <host_frames> <initial_internal_block> <overlap>\n"
            "  example) %s ../assets/input_48k.wav 128 96 0.5\n",
            argv[0], argv[0]);
        return 1;
    }

    std::string inPath         = argv[1];
    int   hostFrames           = std::atoi(argv[2]);
    int   initialInternalBlock = std::atoi(argv[3]);
    float overlap              = std::atof(argv[4]);

    if (hostFrames <= 0 || initialInternalBlock <= 0) {
        std::fprintf(stderr, "host_frames and initial_internal_block must be > 0\n");
        return 2;
    }

    // --- [실험 1: Fixed (Baseline) 모드 실행] ---
    if (!run_experiment(
        inPath,
        "../assets/out_fixed.wav",
        "metrics_fixed.csv",
        hostFrames,
        initialInternalBlock,
        overlap,
        false // ABSC 비활성화
    )) {
        return 3;
    }

    // --- [실험 2: Adaptive (ABSC) 모드 실행] ---
    if (!run_experiment(
        inPath,
        "../assets/out_adaptive.wav",
        "metrics_adaptive.csv",
        hostFrames,
        initialInternalBlock,
        overlap,
        true // ABSC 활성화
    )) {
        return 4;
    }

    std::cout << "\nAll experiments completed successfully.\n";
    return 0;
}
