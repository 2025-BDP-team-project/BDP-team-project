## 🚀 변경 사항
## 1. src/dsp_ops.hpp 및 src/dsp_ops.cpp (부하 생성기)
   
목적: PC 성능과 무관하게 일관된 부하 패턴을 생성하도록 코드를 시뮬레이션 전용 엔진으로 교체했습니다.

dsp_ops.hpp (추가),totalFrames_ 변수와 setTotalFrames 함수를 추가하여 DSP가 전체 파일 길이를 알 수 있게 함.

dsp_ops.cpp (주요 변경),수학 연산 루프를 제거하고 std::chrono 기반의 시간 목표 Busy Wait 로직으로 교체.

dsp_ops.cpp (튜닝),"부하 패턴을 파일 진행률에 맞춰 정확히 4회 출렁이도록 정규화하고, 피크 부하를 8.5ms로 고정하여 Fixed 모드의 실패를 유도."

---

## 2. src/absc_controller.hpp 및 src/absc_controller.cpp (제어 로직)
   
목적: 초저지연(128) 모드를 안전하게 사용하고, 위기 시 확실한 방어(512)를 보장하는 하이브리드 로직을 적용했습니다.

absc_controller.hpp,"upperThreshold_를 0.30으로, lowerThreshold_를 0.15로 튜닝하여 128 Block Size에 대한 감도를 극도로 높임."

absc_controller.cpp (Up Logic),위험 감지 시 256을 거치지 않고 **128에서 512로 즉시 점프(Jump)**하도록 로직 수정. (최대 안전 확보)

absc_controller.cpp (Down Logic),복귀 시에는 512 → 256 → 128의 계단식(Step-wise) 복귀를 통해 시스템의 안정성과 유연성을 확보함.

---

## 3. src/main.cpp (메인 시뮬레이션 환경)
   
목적: 시뮬레이션 환경을 설정하고, 청각적인 증명을 위한 기능을 추가했습니다.

DSP 초기화,dsp.setTotalFrames(src.frames());를 호출하여 DSP 엔진에 파일 길이를 전달.

결과 파일명,입력 파일명을 기반으로 [input_name]_fixed.wav와 같이 자동으로 파일명을 생성하도록 변경.

Auditory Proof (추가),if (underrun) 조건문 아래에 std::fill(outBuf...) 묵음 처리 로직을 추가하여 Fixed Mode 실패 시 뚝뚝 끊기는 소리를 재생하도록 함.

---

## 4. scripts/experiments.py (시각화)
   
목적: 발표 가독성을 극대화하기 위해 그래프 형태를 분리하고 단순화했습니다.

그래프 분리,"하나의 이미지에 2~4개 그래프를 합치지 않고, 3개의 독립된 PNG 파일(ProcessingTime, BlockSize, Reliability)로 생성."

스타일/언어,Seaborn 스타일과 영문 표준 용어를 사용한 깔끔한 레이블링을 적용하여 전문적인 느낌을 주도록 통일함.

# ABSC Offline Harness – README

Adaptive Block Size Control (ABSC)를 구현하기 위한 **오프라인 실험 환경**입니다.  
동일한 WAV 입력을 블록 기반 파이프라인으로 처리하고, 각 “가상 콜백”의 처리시간과 상태를 기록하여 **고정형 vs 적응형(ABSC)** 을 공정하게 비교할 수 있도록 설계했습니다.

---

## ✅ 목표

- **공정한 실험:** 동일 입력(WAV) 기반, 동일 조건으로 반복 가능한 결과 확보  
- **구조적 베이스라인:** 이후 ABSC 알고리즘과 JUCE 실시간으로 이식 가능  
- **지표 기반 평가:** 처리시간(cb_ms), underrun 등 핵심 지표를 자동 기록/시각화  

---

## 📦 저장소 구조

absc_offline_harness/<br/>
├── CMakeLists.txt <br/>
├── README.md<br/>
├── assets/<br/>
│   └── input_48k.wav          # 테스트용 오디오 입력 파일 (48kHz 권장)<br/>
├── scripts/<br/>
│   └── experiments.py         # metrics.csv → 그래프(cb_ms.png) 생성<br/>
└── src/<br/>
├── main.cpp               # 오프라인 가상 콜백 루프<br/>
├── file_source.{hpp,cpp}  # libsndfile로 WAV 입력<br/>
├── file_sink.{hpp,cpp}    # libsndfile로 WAV 출력<br/>
├── frame_router.{hpp,cpp} # 내부 block 단위 분할/합치기 (ABSC 제어 대상)<br/>
├── dsp_ops.{hpp,cpp}      # 임시 DSP(1-pole low-pass)<br/>
└── metrics.{hpp,cpp}      # 처리시간/underrun/block_size 로그 → CSV<br/>


---


## 🛠️ 빌드 환경 구축 & 빌드 시퀀스

### 0️⃣ 필수 요소
- C++17 컴파일러 (Clang, GCC, MSVC)
- CMake ≥ 3.15
- libsndfile (WAV I/O)
- pkg-config (권장, macOS/Linux)

---

### 1️⃣ macOS (Apple Silicon/Intel, Homebrew)

```bash
brew update
brew install cmake libsndfile pkg-config
```


빌드:
```
cd absc_offline_harness
mkdir -p build && cd build
cmake ..
cmake --build . -j
```


링크 에러 발생 시:
```
pkg-config --libs sndfile 로 -L/opt/homebrew/lib -lsndfile 가 출력되는지 확인.
필요 시:

export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
```



### 2) Ubuntu/Debian
```
sudo apt update
sudo apt install -y build-essential cmake pkg-config libsndfile1-dev

cd absc_offline_harness
mkdir -p build && cd build
cmake ..
cmake --build . -j
```
### 3) Windows (MSVC + vcpkg 권장)
```
# vcpkg 설치 후
vcpkg install libsndfile

# CMake 호출 시 툴체인 지정
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg.cmake
cmake --build build --config Release
```

---

## ▶️ 실행 & 플로팅
	
1.	입력 WAV 준비 (권장: 48kHz 스테레오)
assets/input_48k.wav 로 두세요.

2.	실행 (오프라인 가상 콜백 루프)
```
cd build
./absc_offline ../assets/input_48k.wav ../assets/out_fixed.wav 256 256 0.5
# 인자: <입력.wav> <출력.wav> [hostFrames=256] [internalBlock=256] [overlap=0.5]
```
•	hostFrames: 장치 콜백 크기를 가상으로 고정 에뮬레이션
	•	internalBlock: 내부 처리 블록 크기 (나중에 ABSC가 동적으로 제어)
	•	결과물: metrics.csv, out_fixed.wav

3.	그래프 생성

```
pip install pandas matplotlib   # 또는 conda install pandas matplotlib -y

python3 ../scripts/experiments.py
# → build/cb_ms.png 생성 (콜백 처리시간의 rolling mean)
```

---

## 🧩 코드 구조 & 각 모듈 역할

각 모듈은 ABSC 오프라인 하네스의 핵심 구성 요소로, 입력 → 처리 → 출력 및 메트릭 수집 파이프라인을 담당합니다.

| 모듈 | 경로 | 설명 |
|------|------|------|
| **main** | `src/main.cpp` | 가상 콜백 루프. hostFrames 단위로 입력을 읽고 내부 block 단위로 처리 후 출력. |
| **FileSource** | `src/file_source.*` | libsndfile로 WAV를 interleaved float로 읽음. |
| **FileSink** | `src/file_sink.*` | 처리 결과를 WAV로 저장 (청감 비교/검증 가능). |
| **FrameRouter** | `src/frame_router.*` | 내부 blockSize 단위로 데이터를 분할/병합. ABSC 제어 핵심 대상. |
| **DspOps** | `src/dsp_ops.*` | 1차 Low-pass (baseline용 placeholder). 이후 실제 DSP 체인으로 대체 예정. |
| **Metrics** | `src/metrics.*` | 각 콜백의 처리시간(cb_ms), underrun, block_size를 CSV로 기록. |
| **experiments** | `scripts/experiments.py` | metrics.csv를 통계·그래프로 분석 (cb_ms.png 생성). |

> 💡 **참고:**  
> 현재 FrameRouter는 오프라인용 FIFO 구조이며,  
> 실시간 환경으로 이식 시에는 RT-safe(락-프리 SPSC / AbstractFifo 래핑)로 교체 필요.

---

### 📊 우리가 평가하는 지표(왜 이걸 쓰는가?)

| 지표 | 의미 | 왜 중요한가? | ABSC에서의 역할 |
|------|------|---------------|----------------|
| **frame_index** | “가상 콜백”의 순번(시간 인덱스) | 시간에 따른 상태 변화를 분석하기 위한 기준축 | block_size 전환 시점, 스파이크 위치 등 변곡점 식별 |
| **cb_ms** | 콜백 1회 처리시간(ms) | 실시간 데드라인 여유도를 직접 반영 | ABSC의 1차 피드백: 평균/추세가 오르면 block_size 확대 등 조치 |
| **underrun** | 버퍼 언더런(끊김) 발생 여부 | 청감 품질과 1:1 대응되는 실패 지표 | fail-safe: 발생 전에 예방하도록 제어 |
| **block_size** | 내부 처리 블록 크기 | 지연 vs CPU 효율의 핵심 스위치 | 제어 변수: adaptive 전환의 기록 및 효과 검증 |

**한 줄 요약:**  
`cb_ms` = 성능(속도), `underrun` = 안정성(신뢰성), `block_size` = 제어 레버, `frame_index` = 시간축 기준.

---

### 🔍 Baseline 결과 해석 (예시)

- 입력 길이: **3,180,544 프레임 (≈ 72.12초 @ 48 kHz)**  
- 설정: `hostFrames=256`, `internalBlock=256`, `overlap=0.5`  
- 평균 cb_ms: **≈ 0.014 ms (매우 낮고 안정적)**  
- 표준편차: **≈ 0.0011 ms (작은 변동)**  
- 최대 스파이크: **≈ 0.0566 ms (드묾)**  
- underrun: **0 (끊김 없음)**  

**의미:**  
파이프라인은 안정적이며, 이후 ABSC 적용 시 부하 변화 상황에서  
`cb_ms` 안정화·`underrun` 방지 효과를 검증할 **참조선(reference)** 으로 적합합니다.

---

### 🧭 앞으로의 방향성 (로드맵)

1. **ABSC Controller 모듈 추가**  
   - 50–100ms 주기로 `cb_ms` 이동평균과 `underrun`을 읽어 `block_size(128 / 256 / 512)`를  
     **히스테리시스 + 쿨다운** 방식으로 전환  
   - 파일: `absc_controller.{hpp,cpp}`  
     *(입력: 지표 / 출력: 다음 block_size)*  

2. **Adaptive 모드 실험**  
   - 고정형 vs ABSC 비교: 평균, 중앙값, 95·99p `cb_ms`, `underrun` 비율, 전환 빈도 / 핀퐁 여부  
   - 그래프: `cb_ms` 타임라인 + `block_size` 타임라인을 **겹쳐 adaptive 반응 시각화**
