<!--
SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
Copyright (c) 2026 AgingGliders
-->

# Automation Development Methodology

## 1. Purpose

This document records the methodology used to develop and freeze the recorder automatic-operation logic for:

- AUTO RECORDING;
- AUTO WIFI;
- AUTO DELETE.

It explains how the algorithms were derived from recorded SLM data, how offline flight-analysis behavior was separated from behavior that can be implemented causally in the recorder, how nuisance sessions were treated, and how the final logic is being field-validated in recorder v1.55.

This document owns the **derivation rationale and development evidence** for the automation logic. The normative operational requirements remain in `01_recorder_requirements.md`; architecture ownership remains in `02_recorder_architecture.md`; implemented state behavior remains in `03_state_machine_behavior_review.md`; and validation procedures remain in `05_lightweight_validation_strategy.md` and `VALIDATION.md`.

## 2. Development objective

The automation was not developed as a replacement for the existing post-processing flight-analysis algorithm. The objective was to obtain a simple real-time recorder policy that can:

1. start an automatic logical recording session before a real flight is missed;
2. distinguish a real flight from ground-only vibration or other nuisance activity;
3. keep a confirmed flight session active until a causal landing/ground sequence has been confirmed;
4. identify automatically started sessions containing no flight so they can be deleted;
5. derive a simple WiFi policy from the same automatic READY/RECORDING activity state.

The development deliberately accepts that short nuisance AUTO sessions may occur. Once AUTO DELETE can reliably identify a session in which no flight was seen, minimizing the number of nuisance files is secondary to maintaining reliable flight coverage.

The original 300 s / five-minute no-flight quiet delay was introduced partly to avoid creating many short files when intermittent vibration occurred. During development this rationale was re-examined: if the flight/no-flight classification is correct, additional short pseudo-sessions are not by themselves a failure. The 300 s value is retained in the frozen v1.55 logic, but it is treated as a nuisance-session management parameter rather than as part of flight detection.

## 3. Data basis

Development and historical replay used real recorder data rather than synthetic threshold-only test signals. The data set was progressively enlarged and included:

- the FCJAF files available on the official server;
- FCFAG 2026 recordings;
- recovered FCFAG 2025 recordings;
- recovered FCASH recordings;
- difficult calm-flight, multi-flight, ground-vibration, and borderline landing cases examined during development.

The frozen historical replay currently contains 190 cases with evaluable pre-roll AUTO START evidence. Flight end is evaluable in 189 of those cases. `FCJAF_20260627_3.bin` is retained as a right-censored case because the historical recording ends at landing and does not contain the post-landing interval required to confirm 50 s of GROUND.

The historical data were used both to understand failure modes and to select/freeze the logic. They are therefore **development/replay evidence**, not an independent prospective validation population. Recorder v1.55 continues the observe-only field-validation stage on new recordings.

## 4. Starting point: offline flight analysis is not directly portable

The existing file post-processor was used as a reference for understanding useful signals, but its event timing cannot be copied directly into recorder firmware because important parts of the offline process are non-causal.

The post-processing chain uses forward/reverse filtering and centered processing. In particular, its smoothed FlightGround transition benefits from samples occurring after the transition being located. The offline process can also search around a detected transition for a HIRMS peak using future data.

A focused comparison on 11 difficult landings showed that normalization was not the main reason the offline algorithm performed better:

| Variant | Landing result |
|---|---:|
| Offline, full-file normalized HIRMS gate | 11/11 |
| Offline, fixed HIRMS = 0.050 g gate | 11/11 |
| Causal detector with full-file oracle normalization | 8/11 |
| Causal detector with fixed HIRMS = 0.050 g gate | 8/11 |

The decisive ablation was the final FlightGround smoothing. Even when HIRMS/LOWRMS filtering and RMS calculations were made causal, restoring only the zero-phase FlightGround low-pass recovered 11/11 landings in that subset. With a causal final FlightGround filter, performance fell to 8/11 or 9/11 depending on the other processing choices.

This led to two design rules for the recorder automation:

- use only trailing/causal information that is actually available at the current 20 Hz sample;
- do not transplant the offline `FlightGround <= 0` landing condition into the real-time detector, because a causal positive-valued low-pass signal can asymptotically approach zero without crossing it promptly.

The final real-time flight-end detector therefore uses an ordered sequence of finite thresholds and confirmation times rather than trying to reproduce the offline zero-phase transition exactly.

## 5. AUTO RECORDING methodology

### 5.1 Continuous processing

Automatic-operation signals are evaluated continuously from corrected, installation-aligned 20 Hz acceleration whenever normal acquisition is available. HIRMS, LOWRMS, motion, and attitude-filter histories are not restarted merely because a logical AUTO session starts or stops.

Only session evidence and the virtual session state are reset. This avoids filter warm-up artifacts at an automatic recording boundary.

### 5.2 AUTO START: two independent evidence paths

AUTO START was developed to avoid depending on a single type of pre-flight activity. The frozen logic accepts either of two independent causal paths.

**Motion path**

- calculate the 2.0 s trailing three-axis RMS as `sqrt(var(Ax)+var(Ay)+var(Az))`;
- require RMS >= 0.020 g continuously for 1.0 s.

**Attitude-change path**

- run independent first-order causal 0.10 Hz high-pass filters on installation-aligned `Nx` and `Ny`;
- require `max(|HP(Nx)|, |HP(Ny)|) >= 0.020 g` continuously for 1.0 s.

The second path complements vibration/motion evidence with a slow attitude-change indicator and does not require a stored reference attitude.

After the thresholds and timings were frozen, historical replay gave **190/190 pre-roll AUTO START coverage** on the current development set.

### 5.3 Logical AUTO session before flight confirmation

An AUTO START creates a logical automatic session. At this point the recorder has evidence of relevant activity but has not yet established that a flight occurred.

Before `flight_seen`:

- continuous motion quiet for 300 s ends the logical no-flight session;
- ending a no-flight session does not imply that a flight was missed; it is specifically the point at which AUTO DELETE becomes eligible in production mode.

Once `flight_seen` is latched, motion quiet is no longer allowed to end the session. A confirmed flight can end only through the ordered flight-end logic.

## 6. Flight-presence methodology

The existing HIRMS/LOWRMS family of signals was retained because it had already proved useful in SLM flight/ground analysis, but it was reformulated for causal continuous operation.

The frozen signals are:

- HIRMS: 4 s trailing RMS after a fourth-order 3 Hz high-pass;
- LOWRMS: 10 s trailing RMS after a fourth-order 0.25 Hz high-pass followed by a fourth-order 3 Hz low-pass.

Two `flight_seen` paths are retained because a logical AUTO session can begin at different points relative to the takeoff sequence.

### 6.1 Primary path

1. `HIRMS >= 0.050 g` latches HIRMS evidence;
2. after that evidence exists, `LOWRMS - HIRMS >= 0.120 g` continuously for 4 s sets `flight_seen`.

This requires both a sufficiently strong high-frequency event and subsequent low-frequency flight dominance.

### 6.2 Late-start path

1. `LOWRMS >= 0.050 g` while `HIRMS < 0.050 g` continuously for 5 s latches `possible_flight`;
2. a later new HIRMS upward crossing from below 0.050 g to at least 0.050 g sets `flight_seen`.

This path preserves coverage when the automatic logical session begins after the normal primary evidence ordering has already partly occurred.

The two paths share the same continuous filter histories. Only their session evidence is reset at a new logical AUTO session.

## 7. Flight-end methodology

### 7.1 Why an ordered causal detector was selected

The difficult historical cases showed that a single causal threshold derived from the offline FlightGround transition was not sufficiently reliable. The final detector therefore looks for a physically ordered pattern around landing rather than for one instantaneous state transition.

After `flight_seen`, the detector maintains flight-local HIRMS minimum/maximum values and computes normalized FlightGround:

`FG = max(0, (LOWRMS - HIRMS) / (Hmax - Hmin))`

with the implemented small-range guard.

### 7.2 Frozen ordered sequence

A valid flight end requires the following sequence:

1. HIRMS >= 0.050 g continuously for at least 3 s;
2. during that same HIRMS event, `FG >= 0.10` is observed at least once;
3. HIRMS subsequently falls below 0.050 g;
4. within the following 25 s, `FG <= 0.020` continuously for 2 s creates a GROUND candidate;
5. if `FG >= 0.10` before confirmation, GROUND is cancelled and the logical detector returns to FLIGHT;
6. 50 s continuous GROUND confirms flight end.

This structure provides an explicit causal order: landing-like HIRMS activity, evidence that the system was still in the flight regime during that activity, decay of HIRMS, transition to low FlightGround, and finally a sustained ground confirmation.

### 7.3 Historical replay result

With the logic frozen, the current historical set gives:

- **189/189 evaluable flight-end confirmations**;
- **zero premature flight ends** in the evaluated set;
- **zero missed flight-end confirmations** in the evaluated set;
- `FCJAF_20260627_3.bin` excluded from the denominator only because the source file is right-censored at landing.

These results freeze the algorithm for field evaluation; they do not replace prospective field validation because the historical recordings were also used during development.

## 8. AUTO DELETE methodology

AUTO DELETE is deliberately downstream of flight classification. It is not a separate vibration classifier.

In production actuation mode, a file is eligible for automatic deletion only when all of the following are true:

- the recording was automatically started;
- the logical session ended through the pre-flight/no-flight quiet path;
- `flight_seen` was never latched.

When eligible, both the `.bin` file and its matching `.sha` are removed through the normal STOPPING/SD close path. A manually started recording is never AUTO DELETE eligible.

This policy is what allows AUTO START to favor coverage over aggressive suppression of short nuisance sessions. A false AUTO START can produce an extra temporary session; it should not lose a real flight. If no flight evidence appears, AUTO DELETE can remove the nuisance recording afterward.

In v1.55 observe-only validation, the same decision is evaluated and logged as `WOULD_AUTO_DELETE`, but no physical file is deleted.

## 9. AUTO WIFI methodology

AUTO WIFI does not introduce another independent flight classifier. Its intended state is derived from the same virtual automatic-operation activity used by AUTO RECORDING:

- quiet virtual READY: WiFi would be ON;
- confirmed motion or an active virtual/real AUTO recording: WiFi would be OFF;
- after return to READY conditions, 5 s continuous quiet: WiFi would return ON.

The short 5 s quiet period is a WiFi availability delay, not the 300 s no-flight recording timeout. The two timers serve different purposes.

In v1.55, AUTO WIFI is simulated only. `WOULD_AUTO_WIFI_*` events are logged while actual manual WiFi request and AP transitions are logged separately. This permits the intended automatic policy to be compared with real Web/AP behavior without automation changing the recorder during the test.

## 10. v1.55 prospective field-validation method

### 10.1 Why all automations are forced ON

For the field-validation build, AUTO RECORDING, AUTO WIFI, and AUTO DELETE are forced logically ON from boot. Their stored NVS selections are ignored as runtime gates but are not rewritten.

This ensures every field recording exercises all three automatic policies and avoids an invalid test caused by one automation inadvertently being left disabled.

### 10.2 Why v1.55 is observe-only

The purpose of v1.55 is to observe the frozen automation on real new operating days before allowing it to control the recorder.

The operator manually starts one long morning-to-evening physical recording and manually stops it at the end of the desired test period. Inside that physical file:

- AUTO START opens virtual sessions;
- no-flight timeout and `flight_seen` are evaluated;
- landing and flight-end logic are evaluated;
- AUTO DELETE decisions are evaluated;
- AUTO WIFI decisions are evaluated;
- none of those automation decisions can physically start/stop recording, change WiFi, or delete files.

Manual controls, recorder faults, power controls, and the <=5% battery shutdown remain real.

This separates **detector observation** from **actuator effect**: an automation error can be seen in the diagnostic trace without altering the recording that is needed to analyze the error.

### 10.3 Reversible diagnostic trace

Diagnostic events are encoded only into the SD-bound copy of `Nx/Ny/Nz`. The acceleration used by recorder signal processing remains untouched.

The QMI8658 operates within +/-8 g. The diagnostic overlay uses independent ternary axis offsets of -20.001 g, 0, and +20.001 g, placing the encoded values outside the normal sensor range. `tools/automation_diag_decode.py` removes the overlay, restores the original milli-g acceleration and 0x70 checksums, and emits the event stream as CSV.

A controlled round-trip test has verified that removing the overlay can restore the original binary byte-for-byte.

The first prospective v1.54 field file (`FCJAF_20260823_3.bin`) also exposed an integration issue: the recorder Web post-processing path initially analyzed the encoded SD values directly. The diagnostic offsets therefore appeared as approximately +/-20 g acceleration steps and corrupted HIRMS/LOWRMS segmentation even though the live automation detector itself remained correct. v1.55 fixes only this analysis boundary: the Web SLM decoder now restores the diagnostic overlay after stored checksum validation and before any flight-analysis filtering. The automation algorithm and overlay encoding remain frozen and unchanged.

### 10.4 Timing/jitter boundary

The diagnostic instrumentation was arranged so it cannot add variable automation work to the acquisition-to-recording critical path.

Before `ring_buffer_push()`, the only diagnostic work is applying three already-precomputed integer offsets to the SD-bound sample copy. Event comparisons, virtual policy evaluation, queueing, detector resets, and preparation of later overlay values occur after the current ring push.

Consequently, a newly detected event normally appears at least one 20 Hz sample (50 ms) later in the diagnostic stream. If several events occur in one State-task cycle, they are emitted over later samples in preserved order. The event CSV should therefore be interpreted as an ordered diagnostic trace, not as exact same-cycle event timestamps.

## 11. Evidence hierarchy and freeze rule

The automation evidence is intentionally separated into three levels:

1. **Algorithm-development evidence** — detailed review of known difficult flights and nuisance cases used to identify failure modes and select the logic.
2. **Frozen historical replay** — the selected logic is rerun without per-file changes over the accumulated historical set, currently giving 190/190 pre-roll AUTO START coverage and 189/189 evaluable flight ends with zero premature and zero missed confirmations.
3. **Prospective field validation** — v1.55 records new operating days with the logic forced ON but observe-only, so the frozen decisions can be compared with real pilot/aircraft operation without automation affecting the recorder.

The v1.55 field campaign is therefore not intended to retune the detector continuously. New failures should first be preserved and explained from the diagnostic trace. Any later threshold or sequence change creates a new algorithm revision and requires the relevant historical replay and field-validation evidence to be reconsidered.

## 12. Relationship between the three automations

The final methodology intentionally keeps the three functions coupled only through a small set of semantic results:

```text
continuous acceleration
        |
        +--> motion / attitude AUTO START
                    |
                    v
            logical AUTO session
                    |
          HIRMS / LOWRMS evidence
                    |
          +---------+----------+
          |                    |
   no flight seen          flight_seen
   + 300 s quiet               |
          |               ordered landing
          |               + 50 s GROUND
          |                    |
          v                    v
  no-flight AUTO stop     flight AUTO stop
          |                    |
          v                    |
   AUTO DELETE eligible        |
                               |
quiet READY <------------------+
    |
    +--> AUTO WIFI ON
motion / virtual recording --> AUTO WIFI OFF
READY + 5 s quiet -----------> AUTO WIFI ON
```

AUTO DELETE therefore depends only on the result of an automatically started session that never reached `flight_seen`. AUTO WIFI depends on the virtual READY/activity state. Neither function modifies the underlying flight detector.

This separation is intended to keep later policy changes to deletion or WiFi from changing the evidence used to identify a flight.
