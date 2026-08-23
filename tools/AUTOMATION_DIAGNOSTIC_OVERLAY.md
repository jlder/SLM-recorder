# v1.54/v1.55 automation field-diagnostic overlay

This diagnostic build instruments AUTO RECORD logic without feeding diagnostic
values back into recorder signal processing.

## Timing / jitter rule

The State task still performs the normal 20 Hz acquisition first. While
recording, the SD-bound sample receives only the already-precomputed diagnostic
overlay and is immediately formatted/pushed to the existing recording ring.
Diagnostic state/event comparison and selection for the next sample runs later,
after the ring push. A diagnostic event therefore normally appears 50 ms late.
If several events occur during one State-task cycle, the fixed diagnostic queue
emits one event per later sample; the CSV retains the emitted order.

## Reversible acceleration encoding

The QMI8658 is configured for +/-8 g. +/-10 g is reserved as the clean band.
For each axis a ternary digit is encoded by adding:

- trit 0: -20001 mg
- trit 1: 0 mg
- trit 2: +20001 mg

The resulting three trits encode `code = tx + 3*ty + 9*tz` (0..26).
Code 13 is `(1,1,1)` / `NONE`; normal samples therefore remain byte-for-byte
unchanged except for their normal checksum.

`tools/automation_diag_decode.py` removes the overlay exactly and recalculates
the 0x70 checksum, producing a normal clean SLM binary plus a diagnostic CSV.

## Event map

| Code | Event |
|---:|---|
| 0 | AUTO_START_MOTION_POLICY |
| 1 | AUTO_START_ATTITUDE_POLICY |
| 2 | MOTION_START_ON |
| 3 | MOTION_START_OFF |
| 4 | ATTITUDE_START_ON |
| 5 | ATTITUDE_START_OFF |
| 6 | MOTION_STOP_ON |
| 7 | MOTION_STOP_OFF |
| 8 | HIRMS_ABOVE_0P050 |
| 9 | HIRMS_BELOW_0P050 |
| 10 | PRIMARY_CONFIRMING |
| 11 | POSSIBLE_FLIGHT |
| 12 | FLIGHT_SEEN_PRIMARY |
| 13 | NONE |
| 14 | FLIGHT_SEEN_SECONDARY |
| 15 | HIRMS_EVENT_START |
| 16 | HIRMS_EVENT_3S |
| 17 | HIRMS_EVENT_SAW_FLIGHT_FG |
| 18 | HIRMS_EVENT_END_VALID |
| 19 | HIRMS_EVENT_END_INVALID |
| 20 | LANDING_EVENT_EXPIRED |
| 21 | FG_LOW_START |
| 22 | FG_LOW_RESET |
| 23 | GROUND_CANDIDATE |
| 24 | GROUND_CANCELLED |
| 25 | FLIGHT_END_CONFIRMED |
| 26 | EXTENDED prefix |

The intended field test is a manually started long recording. These dedicated
v1.54/v1.55 field-validation builds are observe-only and force AUTO RECORD, AUTO WIFI and AUTO DELETE
logically ON from boot. SETTINGS > AUTOMATION displays all three as ON with
disabled controls. Stored NVS selections are not rewritten and are ignored as
runtime gates while this build is installed. None of the automation decisions
actuate recorder state, WiFi, or file deletion. Manual controls, faults and
battery protection remain real.

## Extended events

Wire code 26 is an escape prefix. The next wire code is an extended-event
payload. This allows additional field diagnostics without changing the
reversible acceleration encoding.

| Payload | Extended event |
|---:|---|
| 0 | DIAG_QUEUE_OVERFLOW |
| 1 | AUTO_WIFI_SELECTED_ON |
| 2 | AUTO_WIFI_SELECTED_OFF |
| 3 | WIFI_REQUESTED_ON |
| 4 | WIFI_REQUESTED_OFF |
| 5 | WIFI_AP_STARTED |
| 6 | WIFI_AP_STOPPED |
| 7 | AUTO_RECORD_SELECTED_ON |
| 8 | AUTO_RECORD_SELECTED_OFF |
| 9 | AUTO_DELETE_SELECTED_ON |
| 10 | AUTO_DELETE_SELECTED_OFF |
| 11 | USB_PRESENT |
| 12 | USB_ABSENT |
| 13 | STATE_READY |
| 14 | STATE_STARTING |
| 15 | STATE_RECORDING |
| 16 | STATE_STOPPING |
| 17 | STATE_ERROR |
| 18 | STATE_OFF |
| 19 | MANUAL_RECORD_START |
| 20 | GROUND_10S |
| 21 | GROUND_25S |
| 22 | GROUND_40S |
| 23 | VIRTUAL_AUTO_SESSION_START |
| 24 | VIRTUAL_AUTO_STOP_NO_FLIGHT |
| 25 | VIRTUAL_AUTO_STOP_FLIGHT_END |

## Second-level extended events

`26:26:<payload>` carries observe-only policy events.

| Payload | Event |
|---:|---|
| 0 | OBSERVE_ONLY_ACTIVE |
| 1 | WOULD_AUTO_WIFI_ON |
| 2 | WOULD_AUTO_WIFI_OFF_MOTION |
| 3 | WOULD_AUTO_WIFI_OFF_RECORDING |
| 4 | WOULD_AUTO_WIFI_OFF_SELECTED |
| 5 | WOULD_AUTO_DELETE |
| 6 | VIRTUAL_READY |
| 7 | VIRTUAL_RECORDING |
| 8 | AUTO_WIFI_QUIET_5S |

## Manual all-day observe-only mode

All automation functions are already forced ON. The operator only needs to
manually start one physical recording in the morning and manually stop it at
the end of the desired test period. The SD file stays open throughout.

AUTO RECORD is simulated as virtual sessions inside the file: motion/attitude
START, no-flight timeout, `flight_seen`, landing sequence and flight-end STOP are
all evaluated and logged, but never change the physical recorder state. A virtual no-flight STOP also logs `WOULD_AUTO_DELETE`; the file is not deleted.

AUTO WIFI is also simulated without calling the Web/AP actuator. In virtual
READY it would be ON when quiet, turns OFF for confirmed motion or virtual
recording, and returns ON after 5 s quiet. The CSV separately retains actual
`WIFI_REQUESTED_*` / `WIFI_AP_*` transitions caused by manual or other real
operations, so intended automation policy can be compared with actual WiFi
state.

All virtual policy evaluation, detector resets and diagnostic event queuing run
after the current ring push.


## Recorder Web analysis restoration (v1.55)

v1.55 keeps the wire encoding unchanged and corrects the recorder-side file-analysis boundary. `src/tasks/web_ui/12_script_flight_decode.inc` validates each stored 0x70 checksum first, then restores each diagnostic axis using the same clean-band rule as this decoder before converting milli-g to g. HIRMS/LOWRMS and all downstream browser flight analysis therefore receive clean acceleration.

The physical `.bin` remains diagnostic and retains all event codes; only the in-memory analysis representation is restored. See `../docs/07_automation_diagnostic_encoding.md` for the normative encoding/decoding specification.
