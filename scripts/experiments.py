import pandas as pd
import matplotlib.pyplot as plt

# 1. 파일명 정의 (두 모드의 CSV 파일)
FILE_FIXED = 'metrics_fixed_high_load.csv'
FILE_ADAPTIVE = 'metrics_adaptive.csv'

# 2. 데이터 로드 및 분석
try:
    df_fixed = pd.read_csv(FILE_FIXED)
    df_adaptive = pd.read_csv(FILE_ADAPTIVE)
except FileNotFoundError as e:
    print(f"Error: One or both metrics files not found: {e}")
    print("Please ensure both metrics_fixed_high_load.csv and metrics_adaptive.csv are in the current directory.")
    exit()

# 3. 데이터 통계 출력
print("--- Fixed High Load Statistics ---")
print(df_fixed[['cb_ms', 'underrun']].describe())
print("\n--- Adaptive (ABSC) Statistics ---")
print(df_adaptive[['cb_ms', 'underrun']].describe())

# 4. 그래프 생성 (두 데이터를 한 그림에)
plt.figure(figsize=(12, 6))

# Fixed Mode (ABSC 없음) - 실패를 보여줌
df_fixed['cb_ms'].rolling(20).mean().plot(label='Fixed (ABSC Disabled)', color='red')

# Adaptive Mode (ABSC 있음) - 성공을 보여줌
df_adaptive['cb_ms'].rolling(20).mean().plot(label='Adaptive (ABSC Enabled)', color='blue')

# 5. 그래프 꾸미기
# 데드라인 (5.333ms)의 95% 임계치 (약 5.066ms)를 표시하여 위험선을 보여줌
DEADLINE_MS = 5.333
plt.axhline(DEADLINE_MS * 0.95, color='gray', linestyle='--', label='95% Deadline Threshold')

plt.title('Callback Time (ms, Rolling Mean) Comparison')
plt.xlabel('Frame Index')
plt.ylabel('Callback Time (ms)')
plt.legend()
plt.tight_layout()

# 6. 파일 저장
plt.savefig('cb_ms_comparison.png')
print('\nSaved cb_ms_comparison.png (Comparison Graph)')