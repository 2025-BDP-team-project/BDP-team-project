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

// 언더런 감지를 위한 데드라인 설정 (48kHz, 256버퍼 기준 5.333ms)
static constexpr double DEADLINE_MS = 5.333;
static constexpr double UNDERRUN_THRESHOLD_MS = 0.95 * DEADLINE_MS; 

bool run_experiment(const std::string& inPath, const std::string& outPath, const std::string& metricsPath, 
                    int hostFrames, int initialInternalBlock, float overlap, bool enableAbsc) {
    
    std::cout << "\n--- 실험 시작: " << (enableAbsc ? "Adaptive (ABSC)" : "Fixed (Baseline)") << " 모드 ---\n";

    FileSource src;
    if (!src.open(inPath)) return false;
    if (!src.open(inPath)) return false; // 안정적인 파일 열기를 위한 재시도

    FileSink sink;
    if (!sink.open(outPath, src.sampleRate(), src.channels())) return false;

    FrameRouter router(src.sampleRate(), src.channels());
    router.setInternalBlockSize(initialInternalBlock);
    router.setOverlap(overlap);

    // DSP 초기화: 정규화된 부하 생성을 위해 전체 프레임 수 전달
    DspOps dsp(src.channels());
    dsp.setTotalFrames(src.frames());

    Metrics metrics(src.sampleRate(), hostFrames);
    metrics.openCSV(metricsPath);

    AbscController controller(src.sampleRate());
    if (enableAbsc) {
        // [설정] 128 블록부터 시작하여 Low Latency 성능 확보 도전
        controller.setBlockSizeOptions(128, initialInternalBlock, 512);
        router.setInternalBlockSize(controller.currentBlockSize());
    }

    std::vector<float> inBuf(hostFrames * src.channels());
    std::vector<float> outBuf(hostFrames * src.channels());
    size_t processed = 0;
    size_t totalFrames = src.frames();
    
    while (processed < totalFrames) {
        size_t need = hostFrames;
        size_t got = src.readFrames(inBuf.data(), need);
        if (got < need) std::fill(inBuf.begin() + got*src.channels(), inBuf.end(), 0.0f);

        auto t0 = std::chrono::high_resolution_clock::now();
        
        // 파이프라인 처리
        router.pushInput(inBuf.data(), hostFrames);
        router.processPendingBlocks(dsp);
        router.pullOutput(outBuf.data(), hostFrames);
        
        auto t1 = std::chrono::high_resolution_clock::now();
        double cb_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        
        bool underrun = cb_ms > UNDERRUN_THRESHOLD_MS; 

        // [중요] 언더런 발생 시 강제 묵음 처리 (Dropout Simulation)
        // 실제 리얼타임 환경처럼 소리가 끊기는 현상을 재현하기 위함
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
        std::cout << "사용법: ./absc_offline <input.wav>\n"; 
        return 1; 
    }
    std::string inPath = argv[1];
    
    // 입력 파일명을 기반으로 결과 파일명 자동 생성
    std::string baseName = "result";
    size_t lastSlash = inPath.find_last_of("/\\");
    size_t lastDot = inPath.find_last_of(".");
    if (lastDot != std::string::npos) {
        size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
        baseName = inPath.substr(start, lastDot - start);
    }
    
    std::string outFixed = baseName + "_fixed.wav";
    std::string outAdaptive = baseName + "_adaptive.wav";

    // 두 가지 모드 실험 실행
    run_experiment(inPath, outFixed, "metrics_fixed_high_load.csv", 256, 256, 0.5f, false);
    run_experiment(inPath, outAdaptive, "metrics_adaptive.csv", 256, 256, 0.5f, true);
    
    return 0;
}