import pandas as pd
import matplotlib.pyplot as plt
from matplotlib import rc
import platform

# ==========================================
# 1. 환경 설정 및 데이터 로드
# ==========================================

# 한글 폰트 설정
plt.style.use('default')
if platform.system() == 'Windows':
    rc('font', family='Malgun Gothic')
elif platform.system() == 'Darwin': # Mac
    rc('font', family='AppleGothic')
else:
    rc('font', family='NanumGothic')

plt.rcParams['axes.unicode_minus'] = False
plt.rcParams['font.size'] = 11

# 데이터 로드
try:
    df_fixed = pd.read_csv('metrics_fixed_high_load.csv')
    df_adaptive = pd.read_csv('metrics_adaptive.csv')
    print("✅ 데이터 로드 성공!")
except FileNotFoundError:
    print("❌ 오류: CSV 파일을 찾을 수 없습니다. 시뮬레이션을 먼저 실행해주세요.")
    exit()

# 파생 지표 계산
df_fixed['deadline_ms'] = 5.333
df_adaptive['deadline_ms'] = df_adaptive['block_size'] / 48000.0 * 1000.0

df_fixed['margin'] = df_fixed['deadline_ms'] - df_fixed['cb_ms']
df_adaptive['margin'] = df_adaptive['deadline_ms'] - df_adaptive['cb_ms']


# ==========================================
# 2. 텍스트 통계 리포트 (콘솔 출력용)
# ==========================================
def print_stats(name, df):
    total = len(df)
    underruns = df['underrun'].sum() # 변수명: underruns (복수형)
    fail_rate = (underruns / total) * 100
    avg_load = df['cb_ms'].mean()
    max_load = df['cb_ms'].max()
    safe_margin_avg = df['margin'].mean()
    
    print(f"\n--- [{name} 모드 분석 결과] ---")
    print(f"• 총 프레임 수: {total}")
    # [수정된 부분] {underrun} -> {underruns} 로 오타 수정 완료!
    print(f"• 오디오 끊김(Underrun): {underruns}회 ({fail_rate:.2f}%)") 
    print(f"• 평균 처리 시간: {avg_load:.4f} ms")
    print(f"• 최대 부하(Peak): {max_load:.4f} ms")
    print(f"• 평균 안전 마진: {safe_margin_avg:.4f} ms (클수록 안전)")

print_stats("Fixed (고정형)", df_fixed)
print_stats("Adaptive (적응형)", df_adaptive)

# Adaptive 블록 사용률 출력
print(f"\n--- [Adaptive 블록 사용 분포] ---")
print(df_adaptive['block_size'].value_counts().sort_index())


# ==========================================
# 3. 심층 분석 그래프 생성
# ==========================================

# [Graph 1] 부하 분포 박스 플롯
plt.figure(figsize=(10, 6))
data_to_plot = [df_fixed['cb_ms'], df_adaptive['cb_ms']]
plt.boxplot(data_to_plot, labels=['Fixed Mode', 'Adaptive Mode'], patch_artist=True,
            boxprops=dict(facecolor='#a8dadc'), medianprops=dict(color='red'))

plt.axhline(5.333, color='red', linestyle='--', label='Fixed Deadline (5.3ms)')
plt.title('CPU 부하 분포 비교 (Box Plot)', fontsize=14, weight='bold')
plt.ylabel('처리 시간 (ms)')
plt.legend()
plt.grid(True, alpha=0.3)
plt.savefig('Analysis_1_LoadBoxplot.png', dpi=150)
print("\n📊 저장 완료: Analysis_1_LoadBoxplot.png")


# [Graph 2] 안전 마진 히스토그램
plt.figure(figsize=(10, 6))
plt.hist(df_fixed['margin'], bins=50, alpha=0.6, label='Fixed Mode', color='red')
plt.hist(df_adaptive['margin'], bins=50, alpha=0.6, label='Adaptive Mode', color='green')

plt.axvline(0, color='black', linestyle='--', linewidth=2, label='Crash Boundary (0ms)')
plt.title('시스템 생존 여유(Safety Margin) 분포', fontsize=14, weight='bold')
plt.xlabel('여유 시간 (ms) [음수=사망, 양수=생존]')
plt.ylabel('빈도수')
plt.legend()
plt.grid(True, alpha=0.3)
plt.savefig('Analysis_2_SafetyMargin.png', dpi=150)
print("📊 저장 완료: Analysis_2_SafetyMargin.png")


# [Graph 3] 블록 크기 사용 비율
plt.figure(figsize=(8, 8))
counts = df_adaptive['block_size'].value_counts().sort_index()
labels = [f'Block {bs}\n({ct}회)' for bs, ct in zip(counts.index, counts.values)]
colors = ['#66bb6a', '#42a5f5', '#ffa726'] 
explode = (0.05, 0, 0) 

plt.pie(counts, labels=labels, autopct='%1.1f%%', startangle=140, 
        colors=colors, explode=explode, shadow=True, textprops={'fontsize': 12})
plt.title('Adaptive 모드 자원 활용 비율', fontsize=16, weight='bold')
plt.savefig('Analysis_3_BlockUsage.png', dpi=150)
print("📊 저장 완료: Analysis_3_BlockUsage.png")

print("\n=== 모든 분석 완료 ===")