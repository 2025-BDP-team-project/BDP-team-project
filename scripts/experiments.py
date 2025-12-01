import pandas as pd
import matplotlib.pyplot as plt

# 1. 데이터 로드
try:
    df_fixed = pd.read_csv('metrics_fixed_high_load.csv')
    df_adaptive = pd.read_csv('metrics_adaptive.csv')
except FileNotFoundError:
    print("오류: CSV 파일을 찾을 수 없습니다. 시뮬레이션을 먼저 실행해주세요.")
    exit()

# 2. 그래프 스타일 설정
plt.style.use('default')
plt.rcParams['font.size'] = 12
plt.rcParams['axes.grid'] = True
plt.rcParams['grid.alpha'] = 0.5
plt.rcParams['lines.linewidth'] = 2.0

# ==========================================
# [Graph 1] 부하 반응 비교 (Processing Time)
# 고정형 vs 적응형의 처리 시간 변화 비교
# ==========================================
plt.figure(figsize=(12, 6))

plt.plot(df_fixed['frame_index'], df_fixed['cb_ms'].rolling(50).mean(), 
         label='Fixed Mode (Baseline)', color='salmon', alpha=0.7)
plt.plot(df_adaptive['frame_index'], df_adaptive['cb_ms'].rolling(50).mean(), 
         label='Adaptive Mode (Ours)', color='royalblue')

plt.axhline(5.333, color='gray', linestyle='--', label='Deadline (5.3ms)')

plt.title('Performance Comparison: Processing Time', fontsize=16, weight='bold')
plt.xlabel('Time (Frame Index)')
plt.ylabel('Processing Time (ms)')
plt.legend(loc='upper left')
plt.tight_layout()
plt.savefig('Graph_1_ProcessingTime.png', dpi=150)
print("저장 완료: Graph_1_ProcessingTime.png")
plt.close()


# ==========================================
# [Graph 2] 적응형 동작 분석 (Block Size)
# 부하에 따른 블록 크기 변화 (계단형 그래프)
# ==========================================
plt.figure(figsize=(12, 5))

plt.step(df_adaptive['frame_index'], df_adaptive['block_size'], 
         label='Adaptive Block Size', color='green', where='post', linewidth=2.5)

plt.title('Adaptive Behavior: Block Size Adjustment', fontsize=16, weight='bold')
plt.xlabel('Time (Frame Index)')
plt.ylabel('Block Size (frames)')
plt.ylim(0, 600) # 128, 256, 512가 잘 보이도록 범위 설정
plt.legend(loc='upper left')
plt.tight_layout()
plt.savefig('Graph_2_BlockSize.png', dpi=150)
print("저장 완료: Graph_2_BlockSize.png")
plt.close()


# ==========================================
# [Graph 3] 결과 증명 (Cumulative Underruns)
# 누적 끊김 횟수 비교 (최종 성적표)
# ==========================================
plt.figure(figsize=(12, 6))

plt.plot(df_fixed['frame_index'], df_fixed['underrun'].cumsum(), 
         label='Fixed Mode (Crashes)', color='red', linewidth=2.5)
plt.plot(df_adaptive['frame_index'], df_adaptive['underrun'].cumsum(), 
         label='Adaptive Mode (Stable)', color='green', linewidth=2.5)

# 최종 횟수 텍스트 표시
fixed_fail = df_fixed['underrun'].sum()
adaptive_fail = df_adaptive['underrun'].sum()
plt.text(df_fixed['frame_index'].iloc[-1], fixed_fail, f' {fixed_fail}', 
         color='red', va='center', weight='bold', fontsize=14)
plt.text(df_adaptive['frame_index'].iloc[-1], adaptive_fail, f' {adaptive_fail}', 
         color='green', va='bottom', weight='bold', fontsize=14)

plt.title('Reliability Result: Cumulative Failures', fontsize=16, weight='bold')
plt.xlabel('Time (Frame Index)')
plt.ylabel('Total Dropouts')
plt.legend(loc='upper left')
plt.tight_layout()
plt.savefig('Graph_3_Reliability.png', dpi=150)
print("저장 완료: Graph_3_Reliability.png")
plt.close()