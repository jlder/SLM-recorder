#!/usr/bin/env python3
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 AgingGliders

"""
Validate SLM recorder binary files.

Checks:
- block sequence and checksums;
- 0x72 calibration block before first 0x70 acceleration block;
- 0x70 acceleration timestamp intervals for 20 Hz rate / jitter review;
- final 0x71 status block when present.

Usage:
    python tools/validate_recording.py path/to/file.bin
"""

from __future__ import annotations

import argparse
import math
import struct
from pathlib import Path

SYNC = 0x55
BLOCK_ACCEL = 0x70
BLOCK_STATUS = 0x71
BLOCK_CAL = 0x72

SIZE_ACCEL = 13
SIZE_STATUS = 13
SIZE_CAL = 184


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


def validate(path: Path) -> int:
    data = path.read_bytes()
    pos = 0
    errors: list[str] = []
    warnings: list[str] = []
    accel_ts: list[int] = []
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
    args = ap.parse_args()
    return validate(args.recording)


if __name__ == "__main__":
    raise SystemExit(main())
