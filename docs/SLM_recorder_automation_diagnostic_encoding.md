<!--
SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
Copyright (c) 2026 AgingGliders
-->

# Automation Diagnostic Event Encoding

## 1. Purpose

This document specifies the reversible acceleration overlay used by the SLM Recorder automation field-diagnostic firmware to record automation events inside the normal 20 Hz `0x70` acceleration stream.

It defines:

- why the diagnostic event is carried by temporary offsets applied to the SD-bound acceleration sample;
- the ternary/base-3 encoding used across `Nx`, `Ny`, and `Nz`;
- the exact correspondence between wire codes and automation events;
- the extended-event mechanism;
- queue and timing behavior;
- exact restoration of the original acceleration sample;
- the mandatory rule that the overlay shall be removed before any file-based acceleration or flight analysis.

The automation algorithm itself is defined and justified in `06_automation_methodology.md`. The implementation-oriented decoder note remains in `../tools/AUTOMATION_DIAGNOSTIC_OVERLAY.md`.

The encoding described here applies to the v1.54 field-validation build with:

```text
AUTOMATION_DIAGNOSTIC_OVERLAY_ENABLED = 1
AUTOMATION_DIAGNOSTIC_OBSERVE_ONLY = 1
AUTOMATION_DIAGNOSTIC_FORCE_ALL_ON = 1
```

## 2. Design objective

The field-validation build needs to preserve a time-ordered trace of decisions made by AUTO RECORDING, AUTO WIFI, and AUTO DELETE while the recorder is used normally.

Adding variable diagnostic processing, file writes, serial logging, or another asynchronous telemetry path in front of the 20 Hz recording path could affect acquisition timing. The selected solution therefore reuses the existing acceleration record itself:

1. automation signal processing consumes the original corrected acceleration sample;
2. a precomputed diagnostic offset is applied only to the copy that is about to be written to the recording ring;
3. the normal `0x70` record is written with its normal checksum;
4. after the current ring push, automation events are compared and queued for later samples.

The overlay is deliberately outside the physical accelerometer range and can therefore be removed exactly.

## 3. Reserved acceleration bands

The QMI8658 is configured for approximately +/-8 g. The diagnostic format reserves +/-10,000 mg as the clean-data band and uses:

```text
AUTOMATION_DIAGNOSTIC_CLEAN_AXIS_LIMIT_MG = 10000
AUTOMATION_DIAGNOSTIC_AXIS_OFFSET_MG      = 20001
```

Each axis can therefore be in one of three unambiguous bands:

| Trit | Stored-axis operation | Stored value indicates |
|---:|---|---|
| 0 | `stored = original - 20001 mg` | stored axis `< -10000 mg` |
| 1 | `stored = original` | `-10000 mg <= stored <= +10000 mg` |
| 2 | `stored = original + 20001 mg` | stored axis `> +10000 mg` |

The offset is **20,001 mg rather than 20,000 mg** so the encoded bands cannot touch the clean band at exactly +/-10,000 mg. For example, a clean `+10000 mg` sample encoded with trit 0 becomes `-10001 mg`, and a clean `-10000 mg` sample encoded with trit 2 becomes `+10001 mg`.

The firmware also checks at compile time that:

- the encoded bands do not overlap; and
- the largest encoded value still fits in signed 16-bit storage.

## 4. Three-axis ternary encoding

Each of the three acceleration axes carries one ternary digit, or **trit**:

```text
Nx -> tx
Ny -> ty
Nz -> tz
```

For a wire code `c` in the range 0..26:

```text
tx = c mod 3
ty = floor(c / 3) mod 3
tz = floor(c / 9) mod 3
```

The decoder reconstructs the wire code as:

```text
c = tx + 3*ty + 9*tz
```

Three trits provide:

```text
3 x 3 x 3 = 27 wire codes
```

so one acceleration sample can carry one code from 0 through 26.

The axis offsets are:

```text
dNx = (tx - 1) * 20001 mg
dNy = (ty - 1) * 20001 mg
dNz = (tz - 1) * 20001 mg
```

### 4.1 Code 13 is deliberately `NONE`

Code 13 decomposes to:

```text
13 = 1 + 3*1 + 9*1
(tx,ty,tz) = (1,1,1)
```

Trit 1 means zero offset. Therefore code 13 changes no axis:

```text
dNx = 0
dNy = 0
dNz = 0
```

This is the normal `NONE` code. Acceleration samples carrying no diagnostic event remain unchanged apart from their ordinary recording checksum.

## 5. Direct event map

The direct codes 0..25 carry automation detector events. Code 26 is reserved as an extension prefix.

Offsets in the table are in mg and are added to the original SD-bound sample.

| Code | Trits `(tx,ty,tz)` | dNx | dNy | dNz | Event |
|---:|---|---:|---:|---:|---|
| 0 | (0,0,0) | -20001 | -20001 | -20001 | `AUTO_START_MOTION_POLICY` |
| 1 | (1,0,0) | 0 | -20001 | -20001 | `AUTO_START_ATTITUDE_POLICY` |
| 2 | (2,0,0) | +20001 | -20001 | -20001 | `MOTION_START_ON` |
| 3 | (0,1,0) | -20001 | 0 | -20001 | `MOTION_START_OFF` |
| 4 | (1,1,0) | 0 | 0 | -20001 | `ATTITUDE_START_ON` |
| 5 | (2,1,0) | +20001 | 0 | -20001 | `ATTITUDE_START_OFF` |
| 6 | (0,2,0) | -20001 | +20001 | -20001 | `MOTION_STOP_ON` |
| 7 | (1,2,0) | 0 | +20001 | -20001 | `MOTION_STOP_OFF` |
| 8 | (2,2,0) | +20001 | +20001 | -20001 | `HIRMS_ABOVE_0P050` |
| 9 | (0,0,1) | -20001 | -20001 | 0 | `HIRMS_BELOW_0P050` |
| 10 | (1,0,1) | 0 | -20001 | 0 | `PRIMARY_CONFIRMING` |
| 11 | (2,0,1) | +20001 | -20001 | 0 | `POSSIBLE_FLIGHT` |
| 12 | (0,1,1) | -20001 | 0 | 0 | `FLIGHT_SEEN_PRIMARY` |
| 13 | (1,1,1) | 0 | 0 | 0 | `NONE` |
| 14 | (2,1,1) | +20001 | 0 | 0 | `FLIGHT_SEEN_SECONDARY` |
| 15 | (0,2,1) | -20001 | +20001 | 0 | `HIRMS_EVENT_START` |
| 16 | (1,2,1) | 0 | +20001 | 0 | `HIRMS_EVENT_3S` |
| 17 | (2,2,1) | +20001 | +20001 | 0 | `HIRMS_EVENT_SAW_FLIGHT_FG` |
| 18 | (0,0,2) | -20001 | -20001 | +20001 | `HIRMS_EVENT_END_VALID` |
| 19 | (1,0,2) | 0 | -20001 | +20001 | `HIRMS_EVENT_END_INVALID` |
| 20 | (2,0,2) | +20001 | -20001 | +20001 | `LANDING_EVENT_EXPIRED` |
| 21 | (0,1,2) | -20001 | 0 | +20001 | `FG_LOW_START` |
| 22 | (1,1,2) | 0 | 0 | +20001 | `FG_LOW_RESET` |
| 23 | (2,1,2) | +20001 | 0 | +20001 | `GROUND_CANDIDATE` |
| 24 | (0,2,2) | -20001 | +20001 | +20001 | `GROUND_CANCELLED` |
| 25 | (1,2,2) | 0 | +20001 | +20001 | `FLIGHT_END_CONFIRMED` |
| 26 | (2,2,2) | +20001 | +20001 | +20001 | `EXTENDED prefix` |

## 6. Worked example

Assume the original corrected acceleration sample is:

```text
Nx =  +120 mg
Ny =   -35 mg
Nz = +1010 mg
```

and the event to record is code 12, `FLIGHT_SEEN_PRIMARY`.

Code 12 decomposes to:

```text
12 = 0 + 3*1 + 9*1
(tx,ty,tz) = (0,1,1)
```

The SD-bound sample becomes:

```text
Nx = +120 - 20001 = -19881 mg
Ny =  -35             =    -35 mg
Nz = +1010            =  +1010 mg
```

The decoder later observes:

```text
Nx < -10000 -> tx = 0; restore Nx by adding 20001 mg
Ny clean     -> ty = 1; Ny is unchanged
Nz clean     -> tz = 1; Nz is unchanged
```

It then reconstructs:

```text
code = 0 + 3*1 + 9*1 = 12
```

and restores the exact original acceleration sample:

```text
(+120, -35, +1010) mg
```

The event and the original acceleration therefore coexist in the same stored `0x70` record without loss of acceleration information.

## 7. Extended events

Twenty-six direct event values are not enough for all field diagnostics. Wire code 26 is therefore an escape prefix.

A first-level extended event occupies two consecutive acceleration samples:

```text
26 : payload
```

The first sample carries code 26 and the next sample carries the payload code.

| Wire encoding | Extended event |
|---|---|
| `26:0` | `DIAG_QUEUE_OVERFLOW` |
| `26:1` | `AUTO_WIFI_SELECTED_ON` |
| `26:2` | `AUTO_WIFI_SELECTED_OFF` |
| `26:3` | `WIFI_REQUESTED_ON` |
| `26:4` | `WIFI_REQUESTED_OFF` |
| `26:5` | `WIFI_AP_STARTED` |
| `26:6` | `WIFI_AP_STOPPED` |
| `26:7` | `AUTO_RECORD_SELECTED_ON` |
| `26:8` | `AUTO_RECORD_SELECTED_OFF` |
| `26:9` | `AUTO_DELETE_SELECTED_ON` |
| `26:10` | `AUTO_DELETE_SELECTED_OFF` |
| `26:11` | `USB_PRESENT` |
| `26:12` | `USB_ABSENT` |
| `26:13` | `STATE_READY` |
| `26:14` | `STATE_STARTING` |
| `26:15` | `STATE_RECORDING` |
| `26:16` | `STATE_STOPPING` |
| `26:17` | `STATE_ERROR` |
| `26:18` | `STATE_OFF` |
| `26:19` | `MANUAL_RECORD_START` |
| `26:20` | `GROUND_10S` |
| `26:21` | `GROUND_25S` |
| `26:22` | `GROUND_40S` |
| `26:23` | `VIRTUAL_AUTO_SESSION_START` |
| `26:24` | `VIRTUAL_AUTO_STOP_NO_FLIGHT` |
| `26:25` | `VIRTUAL_AUTO_STOP_FLIGHT_END` |
| `26:26` | `EXTENDED_2 prefix` |

Payload 26 is itself another escape code.

## 8. Second-level extended events

Observe-only virtual-policy events use three consecutive wire codes:

```text
26 : 26 : payload
```

| Wire encoding | Event |
|---|---|
| `26:26:0` | `OBSERVE_ONLY_ACTIVE` |
| `26:26:1` | `WOULD_AUTO_WIFI_ON` |
| `26:26:2` | `WOULD_AUTO_WIFI_OFF_MOTION` |
| `26:26:3` | `WOULD_AUTO_WIFI_OFF_RECORDING` |
| `26:26:4` | `WOULD_AUTO_WIFI_OFF_SELECTED` |
| `26:26:5` | `WOULD_AUTO_DELETE` |
| `26:26:6` | `VIRTUAL_READY` |
| `26:26:7` | `VIRTUAL_RECORDING` |
| `26:26:8` | `AUTO_WIFI_QUIET_5S` |

The hierarchy can therefore be read as:

```text
0..25            direct event
26:x             first-level extended event
26:26:x          second-level extended event
```

## 9. Timing and queue behavior

The overlay is intentionally not computed in front of the current sample's recording-ring push.

For each 20 Hz State-task cycle, the relevant ordering is:

```text
acquire/correct acceleration
        |
automation signal processing uses untouched acceleration
        |
apply already-prepared diagnostic offset to SD-bound copy
        |
ring_buffer_push()
        |
compare automation states/events
        |
queue newly detected events
        |
prepare overlay for a later sample
```

Consequences:

- a newly detected direct event normally appears one 20 Hz sample, approximately **50 ms**, after the event was detected;
- if several events are generated in one State-task cycle, they are queued and emitted one wire code per subsequent acceleration sample;
- a first-level extended event requires two wire samples;
- a second-level extended event requires three wire samples;
- queueing preserves event order, but stored sample timestamps are transport timestamps rather than exact detector-transition timestamps.

The diagnostic queue depth is 64 wire codes. `DIAG_QUEUE_OVERFLOW` exists to make loss of queued telemetry visible if that capacity is exceeded.

`tools/automation_diag_decode.py` reports both:

```text
stored_time_ms
nominal_event_time_ms
```

For an ordinary direct event, `nominal_event_time_ms` is normally `stored_time_ms - 50 ms`. For an extended event, nominal timing is referenced to the first extension-prefix sample rather than to the final payload sample.

The event trace shall therefore be interpreted primarily as an **ordered diagnostic trace**. Exact sub-queue transition timing shall be derived from the detector logic and surrounding samples when needed.

## 10. Exact acceleration restoration

Every axis shall be restored independently before the sample is supplied to normal acceleration processing.

The decoding rule is:

```text
if stored_axis < -10000 mg:
    trit = 0
    original_axis = stored_axis + 20001 mg

else if stored_axis > +10000 mg:
    trit = 2
    original_axis = stored_axis - 20001 mg

else:
    trit = 1
    original_axis = stored_axis
```

After restoration, the decoder shall verify:

```text
-10000 mg <= original_axis <= +10000 mg
```

for all three axes. A restored value outside that range indicates an invalid/corrupt diagnostic sample or an incompatible format and shall not silently be accepted as clean acceleration.

After the three trits are recovered, the wire code is:

```text
code = tx + 3*ty + 9*tz
```

For a cleaned SLM binary, the `0x70` checksum shall be recalculated after restoring `Nx`, `Ny`, and `Nz`.

## 11. Mandatory rule for file-based analysis

**The diagnostic overlay is a transport encoding, not physical acceleration. It shall be removed before any algorithm interprets the stored acceleration values.**

This requirement applies to every consumer of a diagnostic `.bin` file, including:

- recorder Web-page flight analysis;
- Bridge-triggered processing that uses the recorder's file-analysis code;
- Python SLM flight/EFI analysis;
- plotting or CSV export tools;
- any future post-recording C++ analysis that re-reads acceleration from SD.

The required processing order is:

```text
read and validate 0x70 record/checksum
        |
detect diagnostic bands and restore Nx/Ny/Nz
        |
interpret/decode diagnostic event if required
        |
provide CLEAN Nx/Ny/Nz to all normal analysis
        |
HIRMS / LOWRMS / FlightGround / flight segmentation / K&R / EFI / plots
```

A consumer that does not need the event trace may ignore the event code after restoring the axes. It shall **not** skip acceleration restoration.

### 11.1 v1.54 integration finding

Field validation with `FCJAF_20260823_3.bin` exposed this requirement in the recorder Web analysis path. The live automation detector was correct because it consumed untouched acceleration before the overlay was applied, but the post-recording Web decoder read the encoded SD values directly.

When the encoded values were passed to the normal HIRMS/LOWRMS analysis as if they were physical acceleration, the artificial approximately +/-20 g steps corrupted flight detection. Removing the overlay first restored the expected two-flight result.

This finding does **not** invalidate the diagnostic recording. The original acceleration is still recoverable exactly. It identifies a decoder integration requirement: any recorder-side file decoder used during diagnostic firmware operation shall perform the restoration in this section before flight analysis.

## 12. Separation from live automation behavior

The encoding does not feed back into the automation detectors.

The intended v1.54 flow is:

```text
corrected physical acceleration
        +--------------------------+
        |                          |
        v                          v
live automation logic       SD-bound sample copy
(untouched sample)                 |
                                   + diagnostic offset
                                   |
                                   v
                              recording ring / SD
```

Therefore:

- AUTO START, `flight_seen`, landing detection, virtual AUTO DELETE, and virtual AUTO WIFI decisions are based on clean live acceleration;
- diagnostic marker magnitude does not affect those live decisions;
- only consumers that later re-read the encoded file need explicit restoration.

## 13. Compatibility and format identification

For the v1.54 field-validation build the physical accelerometer range makes the diagnostic bands self-identifying at the individual-axis level: valid physical samples cannot normally exceed the reserved +/-10 g clean band.

Nevertheless, the overlay is a diagnostic firmware convention rather than a permanent extension of the generic SLM `0x70` format. Software that writes, archives, or exchanges clean production SLM data should continue to treat the restored acceleration values as the canonical acceleration stream.

When diagnostic decoding is enabled, values outside the clean band shall be treated as overlay candidates only according to the exact rules in this document. Unknown extended payloads should be reported as unknown diagnostic events while the acceleration itself is still restored normally.

## 14. Reference implementation

The reference implementation for removal of the overlay and extraction of the event CSV is:

```text
tools/automation_diag_decode.py
```

The firmware-side encoder is implemented in:

```text
src/tasks/state_task.cpp
```

with constants in:

```text
config.h
```

The encoder and decoder shall use the same values for:

```text
clean axis limit = 10000 mg
axis offset      = 20001 mg
NONE code        = 13
EXTENDED code    = 26
```

Any change to those constants, the ternary mapping, event tables, or extension hierarchy is a diagnostic format change and shall be updated consistently in firmware, decoder tooling, and this document.

## 15. Validation expectations

A diagnostic-format validation should demonstrate at least:

1. ordinary no-event samples remain unchanged;
2. every direct code 0..26 can be encoded and decoded;
3. first- and second-level extended events are decoded in order;
4. restored acceleration exactly matches the original acceleration for every encoded axis combination within the clean band;
5. cleaned `0x70` checksums are valid;
6. decoding a cleaned file a second time produces no diagnostic events and leaves the clean binary unchanged;
7. diagnostic queue overflow, if deliberately provoked, is visible as `DIAG_QUEUE_OVERFLOW`;
8. normal file-based flight analysis gives the same result from the restored diagnostic file as from the equivalent clean acceleration stream.

The most important acceptance criterion is that instrumentation shall remain observational: after restoration, the diagnostic mechanism shall not alter the physical acceleration history used by the normal SLM analysis chain.
