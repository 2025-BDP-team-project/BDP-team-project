#pragma once
#include <fstream>
#include <string>

class Metrics {
public:
    // sampleRate: Hz (예: 48000)
    // hostFrames: 한 콜백당 호스트 버퍼 크기 (예: 256)
    Metrics(int sampleRate, int hostFrames);

    // CSV 파일 열기
    void openCSV(const std::string& path);

    // 콜백 1회 끝날 때마다 호출
    //  - cb_ms    : 콜백 처리 시간(ms, wall-clock 기준)
    //  - underrun : 언더런 발생 여부(실험에서 필요하면 true/false 전달)
    //  - blockSize: 현재 내부 block_size (ABSC가 조정한 값)
    void onCallbackEnd(double cb_ms, bool underrun, int blockSize);

    void closeCSV();

private:
    int   sr_;           // sample rate (Hz)
    int   hostFrames_;   // host buffer size (frames)
    double periodMs_;    // host buffer 1회 데드라인(ms)
    std::ofstream csv_;
    long long frameIndex_ = 0; // 전체 오디오 프레임 인덱스
};
