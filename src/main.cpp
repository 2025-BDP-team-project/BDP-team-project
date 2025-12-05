#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <iostream>

#include "file_source.hpp"
#include "file_sink.hpp"
#include "frame_router.hpp"
#include "dsp_ops.hpp"
#include "metrics.hpp"
#include "absc_controller.hpp"

// 데드라인은 main에서 계산된 값을 사용하기 위해 전역 변수 제거

bool run_experiment(const std::string& inPath, const std::string& outPath, const std::string& metricsPath,
                    int hostFrames, int initialInternalBlock, float overlap, bool enableAbsc, bool forceAbscLarge,
                    int workerThreads, bool simulateLoad) {
    
    // 호스트 프레임 기반 데드라인 계산
    double localDeadline = (double)hostFrames / 48000.0 * 1000.0;
    double localThreshold = localDeadline * 0.95;

    std::cout << "\n--- Running Experiment: "
              << (enableAbsc
                  ? (forceAbscLarge ? "Adaptive (ABSC Locked to 512)" : "Adaptive (ABSC)")
                  : "Fixed (Baseline)")
              << " | Host: " << hostFrames << " | Block: " << initialInternalBlock << " ---\n";

    FileSource src;
    if (!src.open(inPath)) return false;

    FileSink sink;
    if (!sink.open(outPath, src.sampleRate(), src.channels())) return false;

    FrameRouter router(src.sampleRate(), src.channels());
    router.setInternalBlockSize(initialInternalBlock);
    router.setOverlap(overlap);

    DspOps dsp(src.channels());
    dsp.setTotalFrames(src.frames());
    dsp.setWorkerThreads(workerThreads);
    dsp.setSimulateLoad(simulateLoad);

    Metrics metrics(src.sampleRate(), hostFrames);
    metrics.openCSV(metricsPath);

    AbscController controller(src.sampleRate());
    if (enableAbsc) {
        // [핵심 수정]
        // 사용자가 입력한 initialInternalBlock과 상관없이
        // Adaptive 모드는 무조건 [128 - 256 - 512] 구조를 가져야 합니다.
        // 시작값(current)만 입력값에 맞춰줍니다.
        controller.setBlockSizeOptions(128, 256, 512);

        // 시작 블록 크기 설정 (입력값에 맞춤, 단 범위 내에서)
        int startBlock = initialInternalBlock;
        if (startBlock < 128) startBlock = 128;
        if (startBlock > 512) startBlock = 512;
        if (forceAbscLarge) startBlock = 512;
        router.setInternalBlockSize(startBlock);
        controller.forceLargeBlock(forceAbscLarge);
    }

    std::vector<float> inBuf(hostFrames * src.channels());
    std::vector<float> outBuf(hostFrames * src.channels());
    size_t processed = 0;
    
    while (processed < src.frames()) {
        size_t need = hostFrames;
        size_t got = src.readFrames(inBuf.data(), need);
        if (got < need) std::fill(inBuf.begin() + got*src.channels(), inBuf.end(), 0.0f);

        auto t0 = std::chrono::high_resolution_clock::now();
        
        router.pushInput(inBuf.data(), hostFrames);
        router.processPendingBlocks(dsp);
        router.pullOutput(outBuf.data(), hostFrames);
        
        auto t1 = std::chrono::high_resolution_clock::now();
        double cb_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        
        // Underrun 판정
        bool underrun = cb_ms > localThreshold; 

        if (underrun) {
            std::fill(outBuf.begin(), outBuf.end(), 0.0f);
        }

        sink.writeFrames(outBuf.data(), hostFrames);
        metrics.onCallbackEnd(cb_ms, underrun, router.currentBlockSize());

        if (enableAbsc) {
            int nextBlock = controller.onCallbackEnd(cb_ms);
            router.setInternalBlockSize(nextBlock);
        }
        processed += hostFrames;
    }
    metrics.closeCSV();
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: ./absc_offline <input.wav> [HostFrames] [InternalBlock] [Overlap] [Threads] [SimulateLoad]\n";
        std::cout << " - Threads: DSP 채널 처리를 병렬화할 스레드 수 (기본 1)\n";
        std::cout << " - SimulateLoad: 1이면 기존 Busy-wait 부하 유지, 0이면 순수 DSP만 실행\n";
        return 1;
    }
    std::string inPath = argv[1];
    
    // 기본값
    int hostFrames = (argc >= 3) ? std::atoi(argv[2]) : 256;
    int internalBlock = (argc >= 4) ? std::atoi(argv[3]) : 256;
    float overlap = (argc >= 5) ? std::atof(argv[4]) : 0.5f;
    int workerThreads = (argc >= 6) ? std::atoi(argv[5]) : 1;
    bool simulateLoad = (argc >= 7) ? (std::atoi(argv[6]) != 0) : true;

    // 파일명 자동 생성
    std::string baseName = "result";
    size_t lastSlash = inPath.find_last_of("/\\");
    size_t lastDot = inPath.find_last_of(".");
    if (lastDot != std::string::npos) {
        size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
        baseName = inPath.substr(start, lastDot - start);
    }
    
    // 결과 파일명에 설정값 포함 (덮어쓰기 방지)
    std::string suffix = "_" + std::to_string(internalBlock);
    std::string outFixed = baseName + suffix + "_fixed.wav";
    std::string outAdaptive = baseName + suffix + "_adaptive.wav";
    std::string outAdaptiveLocked = baseName + suffix + "_adaptive_locked.wav";
    
    // CSV 파일명도 구분
    std::string csvFixed = "metrics_fixed_high_load.csv";       // 분석 스크립트 호환용
    std::string csvAdaptive = "metrics_adaptive.csv";           // 분석 스크립트 호환용
    std::string csvAdaptiveLocked = "metrics_adaptive_512.csv"; // ABSC를 512 고정으로 사용하는 케이스

    // 1. Fixed Mode (입력값 사용)
    run_experiment(inPath, outFixed, csvFixed, hostFrames, internalBlock, overlap, false, false, workerThreads, simulateLoad);

    // 2. Adaptive Mode (구조는 128-256-512 고정, 시작값만 입력값 사용)
    run_experiment(inPath, outAdaptive, csvAdaptive, hostFrames, internalBlock, overlap, true, false, workerThreads, simulateLoad);

    // 3. Adaptive Mode이지만, 제어 로직을 끄고 largeBlock(512)으로 고정한 케이스
    run_experiment(inPath, outAdaptiveLocked, csvAdaptiveLocked, hostFrames, internalBlock, overlap, true, true, workerThreads, simulateLoad);
    
    return 0;
}
