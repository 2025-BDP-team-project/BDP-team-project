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

// Underrun 감지 임계치 설정 (5.333ms의 95%)
static constexpr double DEADLINE_MS = 5.333;
static constexpr double UNDERRUN_THRESHOLD_MS = 0.95 * DEADLINE_MS; 

// 전체 실험 루프를 함수로 분리 (모드와 파일명을 인수로 받음)
bool run_experiment(
    const std::string& inPath, 
    const std::string& outPath, 
    const std::string& metricsPath, // CSV 파일명
    int hostFrames, 
    int initialInternalBlock, 
    float overlap,
    bool enableAbsc // ABSC 활성화 여부
) {
    std::cout << "\n--- Running Experiment: " << (enableAbsc ? "Adaptive (ABSC)" : "Fixed (Baseline)") << " Mode ---\n";

    FileSource src;
    if (!src.open(inPath)) { std::fprintf(stderr, "Failed to open input: %s\n", inPath.c_str()); return false; }
    const int sampleRate = src.sampleRate();
    const int channels   = src.channels();

    FileSink sink;
    if (!sink.open(outPath, sampleRate, channels)) { std::fprintf(stderr, "Failed to open output: %s\n", outPath.c_str()); return false; }

    FrameRouter router(sampleRate, channels);
    router.setInternalBlockSize(initialInternalBlock);
    router.setOverlap(overlap);

    DspOps dsp(channels, sampleRate);
    Metrics metrics(sampleRate, hostFrames);
    metrics.openCSV(metricsPath);

    // ABSC Controller 초기화 (필요할 때만 사용)
    AbscController controller(sampleRate);
    if (enableAbsc) {
        controller.setBlockSizeOptions(128, initialInternalBlock, 512);
        router.setInternalBlockSize(controller.currentBlockSize());
    }

    std::vector<float> inBuf(hostFrames * channels);
    std::vector<float> outBuf(hostFrames * channels);

    size_t totalFrames = src.frames();
    size_t processed = 0;
    
    // [Fixed 모드는 입력 WAV를 처음부터 다시 읽어야 하므로 파일을 닫았다가 다시 엽니다]
    src.close();
    if (!src.open(inPath)) { std::fprintf(stderr, "Failed to reopen input: %s\n", inPath.c_str()); return false; }


    auto wallStart = std::chrono::high_resolution_clock::now();
    while (processed < totalFrames) {
        size_t need = hostFrames;
        size_t got = src.readFrames(inBuf.data(), need);
        if (got < need) std::fill(inBuf.begin() + got*channels, inBuf.end(), 0.0f);

        auto t0 = std::chrono::high_resolution_clock::now();
        
        router.pushInput(inBuf.data(), hostFrames);
        router.processPendingBlocks(dsp);
        router.pullOutput(outBuf.data(), hostFrames);
        
        auto t1 = std::chrono::high_resolution_clock::now();
        
        double cb_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        bool underrun = cb_ms > UNDERRUN_THRESHOLD_MS; 

        sink.writeFrames(outBuf.data(), hostFrames);

        int recordedBlockSize = router.currentBlockSize();
        metrics.onCallbackEnd(cb_ms, underrun, recordedBlockSize);

        // [ABSC 피드백 루프 - 활성화된 경우에만 작동]
        // (수정)
        if (enableAbsc) {
            int nextInternalBlockSize = controller.onCallbackEnd(cb_ms, hostFrames);
            router.setInternalBlockSize(nextInternalBlockSize);
        }

        // ------------------------------------
        
        processed += hostFrames;
    }
    auto wallEnd = std::chrono::high_resolution_clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(wallEnd - wallStart).count();

    metrics.closeCSV();
    
    std::printf("Experiment Done. Output: %s, Metrics: %s\n", outPath.c_str(), metricsPath.c_str());
    std::printf("Total frames processed: %zd, Wall time: %.2f ms\n", processed, wall_ms);

    return true;
}


int main(int argc, char** argv) {
    // ... (기존 인자 파싱) ...
    if (argc < 3) { /* ... (Usage 출력) ... */ return 1; }
    const std::string inPath = argv[1];
    const int hostFrames = (argc >= 4) ? std::atoi(argv[3]) : 256;
    const int initialInternalBlock = (argc >= 5) ? std::atoi(argv[4]) : 256;
    const float overlap = (argc >= 6) ? std::atof(argv[5]) : 0.5f;

    // --- [실험 1: Fixed (Baseline) 모드 실행] ---
    if (!run_experiment(
        inPath, 
        "../assets/out_fixed_high_load.wav", // Fixed 모드 출력 WAV
        "metrics_fixed_high_load.csv",       // Fixed 모드 CSV
        hostFrames, 
        initialInternalBlock, 
        overlap,
        false // ABSC 비활성화
    )) return 4;


    // --- [실험 2: Adaptive (ABSC) 모드 실행] ---
    if (!run_experiment(
        inPath, 
        "../assets/out_adaptive.wav",      // Adaptive 모드 출력 WAV
        "metrics_adaptive.csv",            // Adaptive 모드 CSV
        hostFrames, 
        initialInternalBlock, 
        overlap,
        true // ABSC 활성화
    )) return 5;
    
    return 0;
}