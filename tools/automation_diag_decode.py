#!/usr/bin/env python3
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 AgingGliders

"""Decode the v1.54/v1.55 automation field-diagnostic acceleration overlay.

Diagnostic firmware modifies only the SD-bound copy of selected 0x70 samples.
Recorder-side automation always consumes the original corrected acceleration.
The overlay is an exactly reversible ternary code spread across X/Y/Z:

    trit 0: recorded_axis = original_axis - 20001 mg
    trit 1: recorded_axis = original_axis             (no offset)
    trit 2: recorded_axis = original_axis + 20001 mg

The clean accelerometer is physically bounded by the +/-8 g sensor. +/-10 g is
reserved as the clean-data band. The three trits encode one wire event code:

    code = tx + 3*ty + 9*tz

Code 13 == (1,1,1) == no event, so ordinary samples are completely unchanged.
Diagnostic event selection is performed after the preceding SD-ring push. An
event therefore normally appears about one 20 Hz sample (50 ms) later. If
several events occur in one cycle they are queued and emitted one per sample.

Outputs:
  <input>_clean.bin             normal SLM stream with overlay removed
  <input>_automation_diag.csv   diagnostic event timeline

The clean binary is checksum-correct and can be passed to the normal SLM tools.
"""

from __future__ import annotations

import argparse
import csv
import struct
from pathlib import Path

SYNC = 0x55
BLOCK_ACCEL = 0x70
BLOCK_STATUS = 0x71
BLOCK_CAL = 0x72
SIZE_ACCEL = 13
SIZE_STATUS = 13
SIZE_CAL = 252

CLEAN_AXIS_LIMIT_MG = 10000
AXIS_OFFSET_MG = 20001
NO_EVENT_CODE = 13

EVENTS = {
    0: "AUTO_START_MOTION_POLICY",
    1: "AUTO_START_ATTITUDE_POLICY",
    2: "MOTION_START_ON",
    3: "MOTION_START_OFF",
    4: "ATTITUDE_START_ON",
    5: "ATTITUDE_START_OFF",
    6: "MOTION_STOP_ON",
    7: "MOTION_STOP_OFF",
    8: "HIRMS_ABOVE_0P050",
    9: "HIRMS_BELOW_0P050",
    10: "PRIMARY_CONFIRMING",
    11: "POSSIBLE_FLIGHT",
    12: "FLIGHT_SEEN_PRIMARY",
    13: "NONE",
    14: "FLIGHT_SEEN_SECONDARY",
    15: "HIRMS_EVENT_START",
    16: "HIRMS_EVENT_3S",
    17: "HIRMS_EVENT_SAW_FLIGHT_FG",
    18: "HIRMS_EVENT_END_VALID",
    19: "HIRMS_EVENT_END_INVALID",
    20: "LANDING_EVENT_EXPIRED",
    21: "FG_LOW_START",
    22: "FG_LOW_RESET",
    23: "GROUND_CANDIDATE",
    24: "GROUND_CANCELLED",
    25: "FLIGHT_END_CONFIRMED",
    26: "EXTENDED",
}

EXTENDED_EVENTS = {
    0: "DIAG_QUEUE_OVERFLOW",
    1: "AUTO_WIFI_SELECTED_ON",
    2: "AUTO_WIFI_SELECTED_OFF",
    3: "WIFI_REQUESTED_ON",
    4: "WIFI_REQUESTED_OFF",
    5: "WIFI_AP_STARTED",
    6: "WIFI_AP_STOPPED",
    7: "AUTO_RECORD_SELECTED_ON",
    8: "AUTO_RECORD_SELECTED_OFF",
    9: "AUTO_DELETE_SELECTED_ON",
    10: "AUTO_DELETE_SELECTED_OFF",
    11: "USB_PRESENT",
    12: "USB_ABSENT",
    13: "STATE_READY",
    14: "STATE_STARTING",
    15: "STATE_RECORDING",
    16: "STATE_STOPPING",
    17: "STATE_ERROR",
    18: "STATE_OFF",
    19: "MANUAL_RECORD_START",
    20: "GROUND_10S",
    21: "GROUND_25S",
    22: "GROUND_40S",
    23: "VIRTUAL_AUTO_SESSION_START",
    24: "VIRTUAL_AUTO_STOP_NO_FLIGHT",
    25: "VIRTUAL_AUTO_STOP_FLIGHT_END",
    26: "EXTENDED_2",
}

EXTENDED2_EVENTS = {
    0: "OBSERVE_ONLY_ACTIVE",
    1: "WOULD_AUTO_WIFI_ON",
    2: "WOULD_AUTO_WIFI_OFF_MOTION",
    3: "WOULD_AUTO_WIFI_OFF_RECORDING",
    4: "WOULD_AUTO_WIFI_OFF_SELECTED",
    5: "WOULD_AUTO_DELETE",
    6: "VIRTUAL_READY",
    7: "VIRTUAL_RECORDING",
    8: "AUTO_WIFI_QUIET_5S",
}


def checksum(block_without_checksum: bytes) -> int:
    return sum(block_without_checksum) & 0xFF


def decode_axis(value: int) -> tuple[int, int]:
    if value < -CLEAN_AXIS_LIMIT_MG:
        raw = value + AXIS_OFFSET_MG
        trit = 0
    elif value > CLEAN_AXIS_LIMIT_MG:
        raw = value - AXIS_OFFSET_MG
        trit = 2
    else:
        raw = value
        trit = 1

    if not -CLEAN_AXIS_LIMIT_MG <= raw <= CLEAN_AXIS_LIMIT_MG:
        raise ValueError(
            f"diagnostic axis decode outside clean band: stored={value}, restored={raw}"
        )
    return raw, trit


def decode_file(src: Path, clean_dst: Path, events_dst: Path) -> tuple[int, int]:
    data = src.read_bytes()
    out = bytearray()
    events: list[dict[str, object]] = []
    pos = 0
    accel_index = 0
    extended_prefix = None
    extended2_prefix = None

    while pos < len(data):
        if pos + 2 > len(data) or data[pos] != SYNC:
            raise ValueError(f"0x{pos:08X}: invalid block sync/header")

        bid = data[pos + 1]
        if bid == BLOCK_ACCEL:
            size = SIZE_ACCEL
        elif bid == BLOCK_STATUS:
            size = SIZE_STATUS
        elif bid == BLOCK_CAL:
            if pos + 4 > len(data):
                raise ValueError(f"0x{pos:08X}: truncated calibration header")
            size = struct.unpack_from("<H", data, pos + 2)[0]
            if size != SIZE_CAL:
                raise ValueError(
                    f"0x{pos:08X}: calibration size {size}, expected {SIZE_CAL}"
                )
        else:
            raise ValueError(f"0x{pos:08X}: unknown block id 0x{bid:02X}")

        if pos + size > len(data):
            raise ValueError(f"0x{pos:08X}: truncated block id 0x{bid:02X}")

        block = data[pos : pos + size]
        if checksum(block[:-1]) != block[-1]:
            raise ValueError(f"0x{pos:08X}: checksum failure")

        if bid != BLOCK_ACCEL:
            out.extend(block)
            pos += size
            continue

        sync, block_id, ts_ms, ax, ay, az, _ = struct.unpack("<BBihhhB", block)
        clean_ax, tx = decode_axis(ax)
        clean_ay, ty = decode_axis(ay)
        clean_az, tz = decode_axis(az)
        event_code = tx + 3 * ty + 9 * tz

        clean_without_checksum = struct.pack(
            "<BBihhh", sync, block_id, ts_ms, clean_ax, clean_ay, clean_az
        )
        out.extend(clean_without_checksum)
        out.append(checksum(clean_without_checksum))

        event_name = None
        event_code_text = None
        extended_nominal_event_time_ms = None
        if extended2_prefix is not None:
            prefix_sample_index, prefix_ts_ms = extended2_prefix
            event_name = EXTENDED2_EVENTS.get(event_code, f"EXT2_UNKNOWN_{event_code}")
            event_code_text = f"26:26:{event_code}"
            extended_nominal_event_time_ms = prefix_ts_ms - 50
            extended2_prefix = None
        elif extended_prefix is not None:
            prefix_sample_index, prefix_ts_ms = extended_prefix
            if event_code == 26:
                extended2_prefix = (prefix_sample_index, prefix_ts_ms)
            else:
                event_name = EXTENDED_EVENTS.get(event_code, f"EXT_UNKNOWN_{event_code}")
                event_code_text = f"26:{event_code}"
                extended_nominal_event_time_ms = prefix_ts_ms - 50
            extended_prefix = None
        elif event_code == 26:
            extended_prefix = (accel_index, ts_ms)
        elif event_code != NO_EVENT_CODE:
            event_name = EVENTS.get(event_code, f"UNKNOWN_{event_code}")
            event_code_text = str(event_code)

        if event_name is not None:
            events.append(
                {
                    "sample_index": accel_index,
                    "stored_time_ms": ts_ms,
                    "nominal_event_time_ms": (
                        extended_nominal_event_time_ms
                        if extended_nominal_event_time_ms is not None
                        else ts_ms - 50
                    ),
                    "event_code": event_code_text,
                    "event": event_name,
                    "stored_ax_mg": ax,
                    "stored_ay_mg": ay,
                    "stored_az_mg": az,
                    "clean_ax_mg": clean_ax,
                    "clean_ay_mg": clean_ay,
                    "clean_az_mg": clean_az,
                }
            )

        accel_index += 1
        pos += size

    if extended_prefix is not None:
        print("warning: file ends after EXTENDED prefix without payload")
    if extended2_prefix is not None:
        print("warning: file ends after EXTENDED_2 prefix without payload")

    clean_dst.write_bytes(out)
    with events_dst.open("w", newline="", encoding="utf-8") as f:
        fieldnames = [
            "sample_index",
            "stored_time_ms",
            "nominal_event_time_ms",
            "event_code",
            "event",
            "stored_ax_mg",
            "stored_ay_mg",
            "stored_az_mg",
            "clean_ax_mg",
            "clean_ay_mg",
            "clean_az_mg",
        ]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(events)

    return accel_index, len(events)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="diagnostic SLM .bin file")
    parser.add_argument("--clean-output", type=Path, default=None)
    parser.add_argument("--events-output", type=Path, default=None)
    args = parser.parse_args()

    src = args.input
    clean_dst = args.clean_output or src.with_name(src.stem + "_clean.bin")
    events_dst = args.events_output or src.with_name(src.stem + "_automation_diag.csv")

    samples, events = decode_file(src, clean_dst, events_dst)
    print(f"input: {src}")
    print(f"acceleration samples: {samples}")
    print(f"diagnostic events: {events}")
    print(f"clean binary: {clean_dst}")
    print(f"event CSV: {events_dst}")
    print("event timestamps are normally delayed by one 50 ms sample;")
    print("simultaneous events may be delayed further by the diagnostic queue.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
