#!/usr/bin/env python3
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 AgingGliders

"""
Validate SLM recorder binary files.

Checks:
- block sequence and checksums;
- 0x72 calibration block before first 0x70 acceleration block;
- 0x70 acceleration timestamp intervals for 20 Hz rate / jitter review;
- final 0x71 status block when present;
- optional graph of recorded X/Y/Z g-load versus time.

The graph opens by default for interactive checks. Use --no-plot for automated
validation runs where a plot window would block the script.

Usage:
    python tools/validate_recording.py path/to/file.bin
    python tools/validate_recording.py path/to/file.bin --plot-output recording_g_load.png
    python tools/validate_recording.py path/to/file.bin --no-plot

After installation calibration, if the recorder is stable and has not moved,
the g-load plot should show approximately X = 0 g, Y = 0 g, and Z = +1 g.
"""

from __future__ import annotations

import argparse
import math
import struct
from pathlib import Path
from typing import Optional

SYNC = 0x55
BLOCK_ACCEL = 0x70
BLOCK_STATUS = 0x71
BLOCK_CAL = 0x72

SIZE_ACCEL = 13
SIZE_STATUS = 13
SIZE_CAL = 252


def checksum_ok(block: bytes) -> bool:
    return (sum(block[:-1]) & 0xFF) == block[-1]


def parse_accel(block: bytes) -> dict:
    sync, bid, ts_ms, ax, ay, az, csum = struct.unpack("<BBihhhB", block)
    return {"sync": sync, "id": bid, "ts_ms": ts_ms, "ax": ax, "ay": ay, "az": az, "checksum": csum}


def parse_status(block: bytes) -> dict:
    sync, bid, overflow, r1, r2, r3, r4, csum = struct.unpack("<BBHHHHHB", block)
    return {"sync": sync, "id": bid, "overflow": overflow, "checksum": csum}


def parse_calibration(block: bytes) -> dict:
    header_fmt = "<BBHIHBBBBB"
    header_size = struct.calcsize(header_fmt)
    values = struct.unpack_from(header_fmt, block, 0)
    sync, bid, size, version, year, month, day, hour, minute, second = values

    offset = header_size
    gains = struct.unpack_from("<fff", block, offset)
    offset += 12
    offsets = struct.unpack_from("<fff", block, offset)
    offset += 12

    face_mean = []
    for _ in range(6):
        face_mean.append(struct.unpack_from("<fff", block, offset))
        offset += 12

    face_stddev = []
    for _ in range(6):
        face_stddev.append(struct.unpack_from("<fff", block, offset))
        offset += 12

    installation_valid = struct.unpack_from("<B", block, offset)[0] != 0
    offset += 1
    inst_year, inst_month, inst_day, inst_hour, inst_minute, inst_second = struct.unpack_from("<HBBBBB", block, offset)
    offset += struct.calcsize("<HBBBBB")
    installation_mean = struct.unpack_from("<fff", block, offset)
    offset += 12
    installation_stddev = struct.unpack_from("<fff", block, offset)
    offset += 12
    installation_matrix = struct.unpack_from("<fffffffff", block, offset)
    offset += 36

    return {
        "sync": sync,
        "id": bid,
        "size": size,
        "version": version,
        "timestamp": f"{year:04d}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}:{second:02d}",
        "gain": gains,
        "offset_mg": offsets,
        "face_mean_mg": face_mean,
        "face_stddev_mg": face_stddev,
        "installation_valid": installation_valid,
        "installation_timestamp": f"{inst_year:04d}-{inst_month:02d}-{inst_day:02d} {inst_hour:02d}:{inst_minute:02d}:{inst_second:02d}",
        "installation_mean_mg": installation_mean,
        "installation_stddev_mg": installation_stddev,
        "installation_matrix": installation_matrix,
    }


def describe_intervals(ts_values: list[int]) -> dict:
    if len(ts_values) < 2:
        return {}

    intervals = [b - a for a, b in zip(ts_values[:-1], ts_values[1:])]
    avg = sum(intervals) / len(intervals)
    variance = sum((x - avg) ** 2 for x in intervals) / len(intervals)
    return {
        "count": len(intervals),
        "avg_ms": avg,
        "min_ms": min(intervals),
        "max_ms": max(intervals),
        "stddev_ms": math.sqrt(variance),
    }




def display_g_load_graph(accel_samples: list[dict], output: Optional[Path] = None) -> None:
    """Display or save a simple X/Y/Z g-load graph from 0x70 blocks.

    Acceleration samples are stored in signed milli-g. The graph converts them
    to g and uses time relative to the first recorded acceleration timestamp.
    After installation calibration, a stable recorder that has not moved should
    show X ~= 0 g, Y ~= 0 g, and Z ~= +1 g.
    """
    if not accel_samples:
        print("g-load graph: skipped, no acceleration samples")
        return

    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("g-load graph: skipped, matplotlib is not installed")
        print("install with: python -m pip install matplotlib")
        return

    t0_ms = accel_samples[0]["ts_ms"]
    time_s = [(s["ts_ms"] - t0_ms) / 1000.0 for s in accel_samples]
    x_g = [s["ax"] / 1000.0 for s in accel_samples]
    y_g = [s["ay"] / 1000.0 for s in accel_samples]
    z_g = [s["az"] / 1000.0 for s in accel_samples]

    plt.figure()
    plt.plot(time_s, x_g, label="X")
    plt.plot(time_s, y_g, label="Y")
    plt.plot(time_s, z_g, label="Z")
    plt.axhline(0.0, linewidth=0.8, linestyle="--")
    plt.axhline(1.0, linewidth=0.8, linestyle="--")
    plt.xlabel("Time since first sample (s)")
    plt.ylabel("g-load (g)")
    plt.title("SLM recorder g-load: X/Y/Z")
    plt.legend()
    plt.grid(True, linewidth=0.3)
    plt.tight_layout()

    if output is not None:
        plt.savefig(output, dpi=150)
        print(f"g-load graph saved: {output}")
    else:
        print("g-load graph: close the plot window to finish")
        plt.show()

def validate(path: Path, show_plot: bool = True, plot_output: Optional[Path] = None) -> int:
    data = path.read_bytes()
    pos = 0
    errors: list[str] = []
    warnings: list[str] = []
    accel_ts: list[int] = []
    accel_samples: list[dict] = []
    counts = {BLOCK_ACCEL: 0, BLOCK_STATUS: 0, BLOCK_CAL: 0}
    first_accel_pos = None
    cal_info = None

    while pos < len(data):
        if data[pos] != SYNC:
            errors.append(f"0x{pos:08X}: expected sync 0x55, got 0x{data[pos]:02X}")
            break

        if pos + 2 > len(data):
            errors.append(f"0x{pos:08X}: truncated block header")
            break

        bid = data[pos + 1]
        if bid == BLOCK_ACCEL:
            size = SIZE_ACCEL
        elif bid == BLOCK_STATUS:
            size = SIZE_STATUS
        elif bid == BLOCK_CAL:
            if pos + SIZE_CAL > len(data):
                errors.append(f"0x{pos:08X}: truncated calibration block")
                break
            size_field = struct.unpack_from("<H", data, pos + 2)[0]
            size = size_field
            if size != SIZE_CAL:
                errors.append(f"0x{pos:08X}: calibration size {size}, expected {SIZE_CAL}")
                break
        else:
            errors.append(f"0x{pos:08X}: unknown block id 0x{bid:02X}")
            break

        if pos + size > len(data):
            errors.append(f"0x{pos:08X}: truncated block id 0x{bid:02X}")
            break

        block = data[pos:pos + size]
        if not checksum_ok(block):
            errors.append(f"0x{pos:08X}: checksum failure for block id 0x{bid:02X}")

        if bid == BLOCK_ACCEL:
            if first_accel_pos is None:
                first_accel_pos = pos
            accel = parse_accel(block)
            accel_ts.append(accel["ts_ms"])
            accel_samples.append(accel)
            counts[BLOCK_ACCEL] += 1
        elif bid == BLOCK_STATUS:
            counts[BLOCK_STATUS] += 1
            if pos + size != len(data):
                warnings.append(f"0x{pos:08X}: status block is not final block")
        elif bid == BLOCK_CAL:
            counts[BLOCK_CAL] += 1
            if counts[BLOCK_CAL] > 1:
                warnings.append(f"0x{pos:08X}: multiple calibration blocks")
            cal_info = parse_calibration(block)

        pos += size

    if pos != len(data):
        warnings.append(f"parsed {pos} of {len(data)} bytes")

    if counts[BLOCK_CAL] == 0:
        errors.append("missing calibration block 0x72")
    elif first_accel_pos is not None and first_accel_pos < SIZE_CAL:
        errors.append("first acceleration block appears before calibration block")

    print(f"file: {path}")
    print(f"bytes: {len(data)}")
    print(f"blocks: 0x72={counts[BLOCK_CAL]}, 0x70={counts[BLOCK_ACCEL]}, 0x71={counts[BLOCK_STATUS]}")

    if cal_info:
        print(f"calibration timestamp: {cal_info['timestamp']}")
        print(f"gain: x={cal_info['gain'][0]:.6f}, y={cal_info['gain'][1]:.6f}, z={cal_info['gain'][2]:.6f}")
        print(f"offset_mg: x={cal_info['offset_mg'][0]:.1f}, y={cal_info['offset_mg'][1]:.1f}, z={cal_info['offset_mg'][2]:.1f}")

    stats = describe_intervals(accel_ts)
    if stats:
        print("accel intervals:")
        print(f"  count={stats['count']}")
        print(f"  avg_ms={stats['avg_ms']:.3f}")
        print(f"  min_ms={stats['min_ms']}")
        print(f"  max_ms={stats['max_ms']}")
        print(f"  stddev_ms={stats['stddev_ms']:.3f}")
        if accel_ts:
            duration_s = (accel_ts[-1] - accel_ts[0]) / 1000.0
            rate = (len(accel_ts) - 1) / duration_s if duration_s > 0 else 0.0
            print(f"  derived_rate_hz={rate:.3f}")

    if warnings:
        print("warnings:")
        for w in warnings:
            print(f"  - {w}")

    if show_plot or plot_output is not None:
        display_g_load_graph(accel_samples, plot_output)

    if errors:
        print("errors:")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("result: PASS")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("recording", type=Path)
    ap.add_argument(
        "--no-plot",
        action="store_true",
        help="validate only; do not display the X/Y/Z g-load graph",
    )
    ap.add_argument(
        "--plot-output",
        type=Path,
        help="save the X/Y/Z g-load graph to a PNG file instead of only displaying it",
    )
    args = ap.parse_args()
    return validate(args.recording, show_plot=not args.no_plot, plot_output=args.plot_output)


if __name__ == "__main__":
    raise SystemExit(main())
