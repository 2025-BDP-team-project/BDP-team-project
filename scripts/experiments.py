#!/usr/bin/env python3
import argparse
from pathlib import Path
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

# 기본 설정값
DEFAULT_FILE_FIXED    = "metrics_fixed_high_load.csv"
DEFAULT_FILE_ADAPTIVE = "metrics_adaptive.csv"
DEFAULT_OUT_DIR       = "metrics_analysis_v4"
DEFAULT_ROLLING_WIN   = 50

def load_metrics(path: Path) -> pd.DataFrame:
    if not path.exists():
        raise FileNotFoundError(f"[!] CSV not found: {path}")

    df = pd.read_csv(path)
    df.columns = df.columns.str.strip()

    # 필수 컬럼 체크 (업데이트된 헤더 반영)
    required = {"frame_index", "cb_ms", "underrun", "block_size"}
    if not required.issubset(df.columns):
        raise ValueError(f"[!] Missing columns in {path}")

    # period_ms가 0이면 추정 (이전 호환성)
    if "period_ms" not in df.columns or df["period_ms"].iloc[0] == 0:
        df["period_ms"] = df["cb_ms"].quantile(0.99) * 1.5

    # 시간축 계산
    df["time_sec"] = df["frame_index"] / 48000.0 # sr 하드코딩 혹은 period_ms 역산
    
    # 이동 평균
    df["cb_ms_ma"] = df["cb_ms"].rolling(window=DEFAULT_ROLLING_WIN, min_periods=1).mean()

    return df

def summarize(label: str, df: pd.DataFrame):
    print(f"\n=== Summary: {label} ===")
    
    underrun_count = df["underrun"].sum()
    print(f"- Total Underruns: {underrun_count}")
    print(f"- Mean Callback Time: {df['cb_ms'].mean():.4f} ms")
    print(f"- P99 Callback Time:  {df['cb_ms'].quantile(0.99):.4f} ms")

    if "target_lat_ms" in df.columns:
        target = df["target_lat_ms"].iloc[0]
        max_lat = df["max_lat_ms"].iloc[0]
        
        # Budget Compliance Analysis
        # estimated_lat_ms가 target 이하인 비율
        if "estimated_lat_ms" in df.columns:
            within_target = (df["estimated_lat_ms"] <= target + 0.1).sum() # 0.1 float margin
            within_max    = (df["estimated_lat_ms"] <= max_lat + 0.1).sum()
            total = len(df)
            
            print(f"- Target Budget ({target}ms) Compliance: {within_target/total*100:.1f}%")
            print(f"- Max Budget ({max_lat}ms) Compliance:    {within_max/total*100:.1f}%")
            
            # Emergency Mode 사용 비율 (Target 초과 ~ Max 이하)
            emergency_use = within_max - within_target
            print(f"- Emergency Mode Usage: {emergency_use/total*100:.1f}%")

def plot_latency_budget(df: pd.DataFrame, out_dir: Path):
    """
    핵심 시각화: Latency Budget 준수 여부 확인
    Y축이 Latency(ms)이며, Target과 Max 라인 사이에 블록 사이즈가 어떻게 위치하는지 보여줌.
    """
    fig, ax = plt.subplots(figsize=(12, 6))
    
    t = df["time_sec"]
    lat = df["estimated_lat_ms"]
    
    # 1. 실제 Latency (Block Size 기반)
    ax.step(t, lat, where='post', label="Estimated Latency (Block Size)", color='blue', linewidth=1.5)
    
    # 2. Budget Lines
    if "target_lat_ms" in df.columns:
        target = df["target_lat_ms"].iloc[0]
        max_lat = df["max_lat_ms"].iloc[0]
        ax.axhline(target, color='green', linestyle='--', linewidth=2, label=f"Target Budget ({target}ms)")
        ax.axhline(max_lat, color='red', linestyle='--', linewidth=2, label=f"Max Budget ({max_lat}ms)")
        
        # 영역 채우기 (Target ~ Max 사이는 'Emergency Zone')
        ax.fill_between(t, target, max_lat, color='yellow', alpha=0.1, label="Emergency Zone")
        ax.fill_between(t, 0, target, color='green', alpha=0.05, label="Safe Zone")

    # 3. Underrun Markers
    underruns = df[df["underrun"] == 1]
    if not underruns.empty:
        ax.scatter(underruns["time_sec"], underruns["estimated_lat_ms"], 
                   color='red', marker='x', s=50, label="Underrun Event", zorder=5)

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Latency (ms)")
    ax.set_title("ABSC Performance: Latency Budget Compliance")
    ax.legend(loc="upper right")
    ax.grid(True, alpha=0.3)
    
    out_path = out_dir / "latency_budget_compliance.png"
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"[+] Saved {out_path}")

def plot_load_vs_response(df: pd.DataFrame, out_dir: Path):
    """
    부하(cb_ms) 변화에 따라 컨트롤러가 어떻게 반응했는지(Latency 변경) 확인
    """
    fig, ax1 = plt.subplots(figsize=(12, 6))
    
    t = df["time_sec"]
    
    # Y1: CPU Load (cb_ms)
    ax1.plot(t, df["cb_ms_ma"], color='orange', alpha=0.8, label="CPU Load (cb_ms MA)")
    ax1.set_ylabel("Processing Time (ms)", color='orange')
    ax1.tick_params(axis='y', labelcolor='orange')
    
    # Deadline 표시
    period = df["period_ms"].iloc[0]
    ax1.axhline(period, color='grey', linestyle=':', label="Host Deadline")

    # Y2: System Latency (Block Size)
    ax2 = ax1.twinx()
    ax2.step(t, df["estimated_lat_ms"], color='blue', linewidth=1.5, where='post', label="System Latency")
    ax2.set_ylabel("System Latency (ms)", color='blue')
    ax2.tick_params(axis='y', labelcolor='blue')
    
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, loc="upper left")
    
    ax1.set_title("ABSC Response: CPU Load vs System Latency")
    ax1.grid(True, alpha=0.3)
    
    out_path = out_dir / "load_vs_response.png"
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"[+] Saved {out_path}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixed", default=DEFAULT_FILE_FIXED)
    parser.add_argument("--adaptive", default=DEFAULT_FILE_ADAPTIVE)
    parser.add_argument("--out", default=DEFAULT_OUT_DIR)
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(exist_ok=True, parents=True)

    try:
        df_fixed = load_metrics(Path(args.fixed))
        df_adaptive = load_metrics(Path(args.adaptive))
    except Exception as e:
        print(f"Error loading CSV: {e}")
        return

    summarize("Fixed", df_fixed)
    summarize("Adaptive", df_adaptive)
    
    # Fixed 모드는 Latency Budget 그래프가 의미가 없으므로(고정), Adaptive만 그립니다.
    if "estimated_lat_ms" in df_adaptive.columns:
        plot_latency_budget(df_adaptive, out_dir)
        plot_load_vs_response(df_adaptive, out_dir)

    print(f"\nDone. Check {out_dir}")

if __name__ == "__main__":
    main()