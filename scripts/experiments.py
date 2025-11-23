#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

# 1. 비교할 CSV 파일 이름
FILE_FIXED    = 'metrics_fixed_high_load.csv'
FILE_ADAPTIVE = 'metrics_adaptive.csv'


def load_metrics(path: Path) -> pd.DataFrame:
    if not path.exists():
        raise FileNotFoundError(f"[!] CSV not found: {path}")

    df = pd.read_csv(path)

    # 최소 필드 체크
    required = ["frame_index", "cb_ms", "underrun", "block_size"]
    for c in required:
        if c not in df.columns:
            raise ValueError(f"Required column '{c}' not found in {path}")

    # period_ms / slack_ms / cpu_util / cpu_percent / buffer_latency_ms
    # 없으면 가능한 한 계산해서 채움 (구버전 호환용)
    if "period_ms" not in df.columns:
        # 이전 버전에서 데드라인 정보가 없으면 cb_ms 상위값으로 대략 추정
        approx_period = df["cb_ms"].quantile(0.99)
        df["period_ms"] = approx_period

    if "slack_ms" not in df.columns:
        df["slack_ms"] = df["period_ms"] - df["cb_ms"]

    if "cpu_util" not in df.columns:
        df["cpu_util"] = df["cb_ms"] / df["period_ms"]

    if "cpu_percent" not in df.columns:
        df["cpu_percent"] = df["cpu_util"] * 100.0

    if "buffer_latency_ms" not in df.columns:
        df["buffer_latency_ms"] = np.nan

    # jitter: cb_ms - period_ms (signed)
    df["jitter_ms"] = df["cb_ms"] - df["period_ms"]
    df["abs_jitter_ms"] = df["jitter_ms"].abs()

    return df


def summarize(name: str, df: pd.DataFrame):
    print(f"\n==================== {name} ====================")

    print("=== Callback Time (cb_ms) ===")
    print(df["cb_ms"].describe(percentiles=[0.5, 0.9, 0.95, 0.99]))
    print()

    print("=== CPU Utilization (cb_ms / period_ms) ===")
    print(df["cpu_util"].describe(percentiles=[0.5, 0.9, 0.95, 0.99]))
    print(f"mean CPU%: {df['cpu_percent'].mean():.3f}%")
    print(f"max  CPU%: {df['cpu_percent'].max():.3f}%")
    print()

    print("=== Slack (period_ms - cb_ms) ===")
    print(df["slack_ms"].describe(percentiles=[0.5, 0.1, 0.05]))
    print()

    underrun_count = int(df["underrun"].sum())
    underrun_ratio = underrun_count / len(df)
    print("=== Underrun ===")
    print(f"count = {underrun_count} / {len(df)} "
          f"({underrun_ratio*100:.4f}%)")
    print()

    peak_jitter = df["abs_jitter_ms"].max()
    print("=== Peak Jitter ===")
    print(f"max |cb_ms - period_ms| = {peak_jitter:.6f} ms")


def plot_cb_ms_overview(df_fixed, df_adaptive, out_prefix: Path):
    """전체 cb_ms time-series + fixed vs adaptive 비교"""
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.plot(df_fixed["frame_index"], df_fixed["cb_ms"],
            label="fixed cb_ms", linewidth=0.7)
    ax.plot(df_adaptive["frame_index"], df_adaptive["cb_ms"],
            label="adaptive cb_ms", linewidth=0.7, alpha=0.8)
    ax.set_xlabel("frame_index")
    ax.set_ylabel("cb_ms (ms)")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right")
    ax.set_title("Callback time (ms) - Fixed vs Adaptive (overview)")

    fig.tight_layout()
    fig.savefig(out_prefix.with_name(out_prefix.name + "_cb_overview.png"),
                dpi=150)
    plt.close(fig)


def plot_cb_ms_zoom(df_fixed, df_adaptive, out_prefix: Path):
    """cb_ms zoom: 상위 5% 스파이크 제외하고 typical 구간 비교"""
    cb_all = pd.concat([df_fixed["cb_ms"], df_adaptive["cb_ms"]],
                       ignore_index=True)
    p95 = cb_all.quantile(0.95)
    y_max = p95 * 1.2 if p95 > 0 else cb_all.max() * 1.2

    fig, ax = plt.subplots(figsize=(10, 4))
    ax.plot(df_fixed["frame_index"], df_fixed["cb_ms"],
            linewidth=0.7, label="fixed cb_ms")
    ax.plot(df_adaptive["frame_index"], df_adaptive["cb_ms"],
            linewidth=0.7, alpha=0.8, label="adaptive cb_ms")

    ax.set_xlabel("frame_index")
    ax.set_ylabel("cb_ms (ms)")
    ax.set_ylim(0, y_max)
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right")
    ax.set_title(f"Callback time (zoomed up to ~p95={p95:.6f} ms)")

    fig.tight_layout()
    fig.savefig(out_prefix.with_name(out_prefix.name + "_cb_zoom.png"),
                dpi=150)
    plt.close(fig)


def plot_block_size(df_fixed, df_adaptive, out_prefix: Path):
    """block_size step plot (adaptive의 전환 패턴 확인용)"""
    fig, ax = plt.subplots(figsize=(10, 4))
    ax.step(df_fixed["frame_index"], df_fixed["block_size"],
            where="post", linewidth=0.7, label="fixed block_size")
    ax.step(df_adaptive["frame_index"], df_adaptive["block_size"],
            where="post", linewidth=0.7, alpha=0.8,
            label="adaptive block_size")
    ax.set_xlabel("frame_index")
    ax.set_ylabel("block_size")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right")
    ax.set_title("Block size (fixed vs adaptive)")

    fig.tight_layout()
    fig.savefig(out_prefix.with_name(out_prefix.name + "_blocksize.png"),
                dpi=150)
    plt.close(fig)


def plot_cpu_percent(df_fixed, df_adaptive, out_prefix: Path):
    """CPU% time-series 비교 (cb_ms / period_ms 기반)"""
    fig, ax = plt.subplots(figsize=(10, 4))
    ax.plot(df_fixed["frame_index"], df_fixed["cpu_percent"],
            linewidth=0.7, label="fixed CPU%")
    ax.plot(df_adaptive["frame_index"], df_adaptive["cpu_percent"],
            linewidth=0.7, alpha=0.8, label="adaptive CPU%")
    ax.set_xlabel("frame_index")
    ax.set_ylabel("CPU (%)")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right")
    ax.set_title("CPU utilization (fixed vs adaptive)")

    fig.tight_layout()
    fig.savefig(out_prefix.with_name(out_prefix.name + "_cpu_percent.png"),
                dpi=150)
    plt.close(fig)


def plot_slack_jitter(df: pd.DataFrame, name: str, out_prefix: Path):
    """slack_ms & jitter_ms time-series (모드별)"""
    fig, ax = plt.subplots(figsize=(10, 4))
    ax.plot(df["frame_index"], df["slack_ms"],
            linewidth=0.7, label=f"{name} slack_ms")
    ax.plot(df["frame_index"], df["jitter_ms"],
            linewidth=0.7, label=f"{name} jitter_ms")
    ax.axhline(0.0, color="black", linewidth=0.8, alpha=0.5)
    ax.set_xlabel("frame_index")
    ax.set_ylabel("ms")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right")
    ax.set_title(f"Slack & Jitter ({name})")

    fig.tight_layout()
    fig.savefig(out_prefix.with_name(out_prefix.name + f"_{name}_slack_jitter.png"),
                dpi=150)
    plt.close(fig)


def main():
    fixed_path    = Path(FILE_FIXED)
    adaptive_path = Path(FILE_ADAPTIVE)

    df_fixed    = load_metrics(fixed_path)
    df_adaptive = load_metrics(adaptive_path)

    # 요약 통계 출력 (터미널)
    summarize("FIXED", df_fixed)
    summarize("ADAPTIVE", df_adaptive)

    out_prefix = Path("metrics_compare")

    # 그래프 생성
    plot_cb_ms_overview(df_fixed, df_adaptive, out_prefix)
    plot_cb_ms_zoom(df_fixed, df_adaptive, out_prefix)
    plot_block_size(df_fixed, df_adaptive, out_prefix)
    plot_cpu_percent(df_fixed, df_adaptive, out_prefix)
    plot_slack_jitter(df_fixed,    "fixed",    out_prefix)
    plot_slack_jitter(df_adaptive, "adaptive", out_prefix)

    print("\n[+] Saved plots (in current directory):")
    print("  - metrics_compare_cb_overview.png")
    print("  - metrics_compare_cb_zoom.png")
    print("  - metrics_compare_blocksize.png")
    print("  - metrics_compare_cpu_percent.png")
    print("  - metrics_compare_fixed_slack_jitter.png")
    print("  - metrics_compare_adaptive_slack_jitter.png")


if __name__ == "__main__":
    main()
