#pragma once
#include <deque>

class AbscController {
public:
    // 생성자: 샘플 레이트와 모니터링할 윈도우 크기를 받음
    AbscController(int sampleRate, int historySize = 50);

    // 내부 블록 크기 설정 (ABSC가 제어하는 대상)
    void setBlockSizeOptions(int small, int normal, int large);

    // 콜백이 끝날 때마다 호출되어 cb_ms를 기록하고 다음 블록 크기를 결정
    int onCallbackEnd(double cb_ms);

    // 현재 블록 크기 반환
    int currentBlockSize() const { return currentBlockSize_; }
    
private:
    // 시간 지표 관련
    std::deque<double> cbHistory_; // cb_ms 기록 큐
    int historySize_;              // 이동 평균 윈도우 크기
    double cbMean_ = 0.0;          // 현재 cb_ms 이동 평균

    // 블록 크기 옵션 (128, 256, 512 등)
    int smallBlock_ = 128;
    int normalBlock_ = 256;
    int largeBlock_ = 512;

    int currentBlockSize_;         // 현재 적용 중인 블록 크기

    // 제어 관련 (쿨다운/임계치)
    long long framesSinceSwitch_ = 0; // 마지막 블록 크기 변경 후 경과 프레임
    int cooldownFrames_;             // 블록 크기 변경 후 대기할 최소 프레임 수

    // 임계치 (Thresholds)
    // 데드라인(5.333ms) 대비 비율로 설정 (예: 0.6 = 60%)
    const double upperThreshold_ = 0.60; // 이 이상이면 부하 높음 → 작게 전환
    const double lowerThreshold_ = 0.35; // 이 이하이면 부하 낮음 → 크게 전환

    // 내부 계산 함수
    void updateMean(double new_cb_ms);
    void updateBlockSize(double target_ms);
};