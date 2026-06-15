<!--
SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
Copyright (c) 2026 AgingGliders
-->

# Lightweight Validation Strategy

## 1. Purpose

This document defines a practical validation strategy for the recorder prototype. The goal is to collect lightweight evidence that operational requirements, derived software requirements, and key state-machine behavior have been reviewed and exercised, without creating a formal certification test campaign.

The project objective is not formal DO-178C certification. This strategy is intentionally limited to evidence that is practical for the current hardware prototype and source-code structure.

## 2. Scope

In scope:

- device boot to READY/setup-lock behavior;
- local UI behavior needed to operate the recorder;
- settings persistence and setup-lock behavior;
- calibration capture, storage, and recording lockout behavior;
- recording start/stop/shutdown behavior;
- SD readiness, low-space, file-count, and recovery behavior;
- acceleration raw/corrected path review;
- output file inspection;
- state-machine source review.

Out of scope:

- formal tool qualification;
- structural coverage evidence;
- automated hardware fault injection;
- complete simulation of external libraries or board-support packages;
- polished Web/UI styling validation beyond operationally relevant content and controls.

## 3. Validation Philosophy

Validation evidence should be:

- lightweight;
- repeatable where practical;
- honest about prototype limitations;
- linked to the `OP-*` and `SWR-*` requirements in `01_recorder_requirements.md`.

Preferred evidence types:

1. source review;
2. state-machine behavior review;
3. manual hardware validation;
4. Web/API/manual UI screenshots where helpful;
5. output file inspection;
6. small scripts for repeatable recorded-file checks;
7. release review notes.

Faults that are easy to reproduce on hardware may be manually tested. Faults that require special driver/library failures may be covered by source/design review unless abstraction layers are added later.

## 4. Evidence Types

### 4.1 Source and Architecture Review

Source review should confirm:

- state ownership rules;
- SD access serialization;
- settings and calibration NVS ownership;
- raw vs corrected accelerometer data paths;
- calibration lockout and calibration-fault behavior;
- SD error and recovery behavior;
- bounded buffers and configured limits;
- ignored return values are intentional;
- Web/UI support functions do not interfere with active recording.

Primary review documents:

- `01_recorder_requirements.md`;
- `02_recorder_architecture.md`;
- `03_state_machine_behavior_review.md`.

### 4.2 Manual Hardware Validation

Manual hardware validation should exercise normal and simple abnormal operations on the real recorder hardware.

Evidence may include:

- observation notes;
- screenshots/photos;
- generated files;
- short logs or copied Web JSON;
- version/zip identification.

### 4.3 Output File Inspection

Output files should be inspected for:

- expected file creation;
- expected acceleration/status blocks;
- clean close status after normal stop;
- no data after close;
- implemented calibration block `0x72`.

A small Python script may be used to validate recorded files, packet structure, counts, and checksums. This is preferred over manual binary inspection when possible.

## 5. Manual Validation Candidates

### VAL-PWR-001 — Low-power shutdown from battery

Purpose:

Confirm low-power battery protection requests shutdown when running on battery with no USB power and displays the recharge notice before power-down.

Procedure:

1. Run the recorder on battery without USB power.
2. Create or simulate low-power condition.
3. Repeat from representative states where practical: READY and RECORDING at minimum.
4. During recording, inspect that the recording file is closed.

Expected result:

- Recorder displays a black full-screen red notice: `BATTERY LOW` / `RECHARGE WITH USB`.
- Notice remains visible for approximately 10 seconds before PMU shutdown is requested.
- Recorder transitions to shutdown from battery low condition.
- If recording is active, SD close path is used before shutdown.

### VAL-BOOT-001 — Boot to READY or setup lock

Purpose:

Confirm that device boot initializes hardware and reaches READY or a setup-lock condition.

Procedure:

1. Power on the device.
2. Observe main screen background and UI elements.
3. Confirm message is one of expected startup/setup messages:
   - `READY`;
   - `NEED SETTINGS`;
   - `ACC CAL REQ`;
   - `CAL FAULT`;
   - SD-related error if SD is intentionally absent.

Expected result:

- Device reaches a stable visible state.
- MENU access is available when in READY/setup-lock state unless an active error intentionally forces the main page.

### VAL-UI-001 — Main display content

Purpose:

Confirm the main display shows operationally required information.

Procedure:

1. Boot device.
2. Observe main page.
3. Check time/date, MENU button, version text, message/error field, and battery graphic.

Expected result:

- Required fields are visible when data is available.
- Battery color follows:
  - 0-25% red;
  - 26-50% orange;
  - 51-75% yellow;
  - 76-100% green.
- USB power shows the charging/electric indicator.

### VAL-UI-002 — Display standby and wake

Purpose:

Confirm standby display reduces active UI refresh and wakes correctly.

Procedure:

1. Leave the recorder idle on the main UI longer than `DISPLAY_DIM_TIMEOUT_MS`.
2. Confirm the screen changes to a black standby display with `TOUCH TO ACTIVATE` from the main display.
3. Confirm standby does not start while on MENU, SETTINGS, or setting-edit pages.
3. Wake using touch.
4. Repeat wake using power/clear button, record button, USB insertion, and a generated message/error where practical.

Expected result:

- Before standby, confirm the active UI time continues updating during RECORDING.
- Standby display appears after timeout in READY and RECORDING.
- Standby does not remain active in BOOT, STARTING, STOPPING, ERROR, or OFF.
- Active main UI returns at full brightness on each wake condition.

### VAL-SET-001 — Settings persistence

Purpose:

Confirm required settings can be entered and stored in NVS.

Procedure:

1. Enter SETTINGS page.
2. Set date, time, registration, and WiFi password.
3. Save each setting.
4. Restart the device.
5. Re-enter settings or observe operational state.

Expected result:

- Settings remain saved after restart.
- Recording remains locked until all required settings are saved.
- When all required settings are saved, settings lock is removed unless another lock remains.

### VAL-SET-002 — Clear settings and calibration gesture

Purpose:

Confirm the combined physical-button gesture clears both settings and calibration NVS data.

Procedure:

1. Start from a device with complete settings and valid calibration.
2. In READY, hold record and power/clear simultaneously until the recorder stops.
3. Restart the recorder.
4. Observe setup/calibration lock messages and inspect Web calibration NVS values if needed.

Expected result:

- Required settings are cleared.
- Stored calibration and calibration fault latch are cleared.
- Recorder requires setup/calibration again before recording can be authorized.

### VAL-WIFI-001 — WiFi/Web access

Purpose:

Confirm START WIFI runs the Web server at the required AP address.

Procedure:

1. Enter MENU.
2. Select START WIFI.
3. Connect a remote device to the recorder WiFi.
4. Open `http://192.168.4.1/`.
5. Open `http://192.168.4.1/diag`.
6. Stop WiFi using the MENU/BACK workflow.
7. Repeat START WIFI / Web page / `/diag` for at least five cycles.

Expected result:

- Web page loads on every cycle.
- `/diag` returns valid JSON with `ok:true`, AP IP `192.168.4.1`, and a cycle count that increments across Web ON cycles.
- Heap reported by `/diag` has no steady downward trend across repeated cycles.
- File tab and Calibration tab are visible.
- Web access is unavailable or not authorized during active recording.

Validation note:

- The Web listener is intentionally not stopped with `AsyncWebServer::end()` during normal Web OFF. Repeated Web ON/OFF validation verifies the selected server-once/AP-toggle lifecycle.

### VAL-CAL-001 — Missing calibration lock

Purpose:

Confirm missing calibration prevents recording.

Procedure:

1. Use a device/NVS state with no valid calibration.
2. Boot device with settings complete.
3. Observe displayed message.
4. Attempt record start.

Expected result:

- Device displays `ACC CAL REQ`.
- MENU/WiFi access remains available.
- Recording does not start.

### VAL-CAL-002 — Calibration automatic six-face capture

Purpose:

Confirm calibration captures all six faces automatically.

Procedure:

1. Enable WiFi/Web.
2. Open Calibration tab.
3. Press Start.
4. Slowly rotate recorder through +X, -X, +Y, -Y, +Z, -Z.
5. Leave recorder still on each face until its row becomes captured.

Expected result:

- Each face row changes to captured.
- Displayed Value/Stddev are scalar values for the face axis.
- Current/last updated face is highlighted in blue.
- NVS values are shown only for comparison.

### VAL-CAL-003 — Calibration result and save

Purpose:

Confirm calibration result appears and can be saved.

Procedure:

1. Complete VAL-CAL-002.
2. Observe lower calibration result area.
3. Press Save.
4. Restart device.
5. Return to Calibration tab.

Expected result:

- Lower area shows `Calibration: Done`.
- NVS date is shown.
- X/Y/Z gain and offset are shown against NVS gain/offset.
- After save and restart, NVS date and NVS gain/offset reflect stored calibration.
- Device can reach READY if no other lock/error exists.

### VAL-CAL-004 — Calibration storage and lock removal

Purpose:

Confirm valid calibration removes calibration lock.

Procedure:

1. Complete and save a valid calibration.
2. Restart device.
3. Observe main screen.
4. Attempt to start recording when all other start conditions are satisfied.

Expected result:

- `ACC CAL REQ` is not shown.
- Recording start is authorized if SD/settings/error conditions are also satisfied.

### VAL-CAL-005 — Calibration expiration review

Purpose:

Confirm calibration expiration behavior by source review or controlled date manipulation.

Procedure:

1. Review `calibration_service` date comparison logic.
2. If practical, set RTC/date beyond configured calibration validity period.
3. Restart or refresh calibration status.

Expected result:

- Calibration older than `CALIBRATION_VALIDITY_MONTHS` is treated as expired.
- Device displays `ACC CAL REQ`.
- Recording is locked.

Note:

This may be source-review-only unless reliable date manipulation is available.

### VAL-CAL-006 — Calibration fault review

Purpose:

Confirm calibration plausibility limits protect against unacceptable calibration.

Procedure:

1. Review gain/offset limit checks.
2. If practical with controlled test hooks, inject or produce an out-of-limit calibration candidate.

Expected result:

- Calibration save is rejected.
- Calibration fault latch is set.
- Device displays `CAL FAULT`.
- The stored valid calibration is retained.
- Successful later calibration clears the fault latch.

Note:

This is currently likely source-review-only without test hooks.

### VAL-REC-001 — Recording start authorization

Purpose:

Confirm recording starts only when authorized.

Procedure:

1. Ensure settings complete.
2. Ensure valid calibration exists.
3. Ensure SD is present, has free space, and file count is below threshold.
4. Hold record button for configured start hold.
5. Repeat with one condition missing where practical.

Expected result:

- Recording starts only when all conditions are satisfied.
- Missing conditions display appropriate messages and block recording.

### VAL-REC-002 — Recording stop by record button

Purpose:

Confirm normal recording stop.

Procedure:

1. Start recording.
2. Hold record button for configured stop hold.
3. Observe stop/ready transition.
4. Inspect SD file existence.

Expected result:

- Recording stops.
- SD file is closed.
- Device returns to READY if no other error/shutdown condition exists.

### VAL-REC-003 — Recording stop by power button

Purpose:

Confirm power-long during recording closes file before shutdown.

Procedure:

1. Start recording.
2. Hold power/clear button for configured shutdown hold.
3. Observe stop/shutdown behavior.
4. Inspect SD file existence if possible.

Expected result:

- Device enters stopping path.
- SD file closes before shutdown.
- Device shuts down.

### VAL-SD-001 — Missing SD behavior

Purpose:

Confirm no-card behavior.

Procedure:

1. Remove SD card.
2. Boot or refresh READY.
3. Observe message and attempt recording.

Expected result:

- `NO SD` or applicable SD error message appears.
- Recording does not start.

### VAL-SD-002 — SD recovery clear

Purpose:

Confirm SD recovery/clear behavior.

Procedure:

1. Create an SD error condition such as no card.
2. Restore SD card/condition.
3. Observe `SD OK/CLR`.
4. Press power/clear for clear hold.

Expected result:

- Device acknowledges SD recovery.
- After clear, device returns to READY or setup-lock message such as `ACC CAL REQ` or `INST CAL REQ`.

### VAL-SD-003 — Low-space and file-count review

Purpose:

Confirm low-space and file-count start prevention.

Procedure:

1. Source-review SD low-space and file-count checks.
2. If practical, use SD media with low available space or excessive file count.
3. Attempt recording.

Expected result:

- Recording is not started.
- `SD FULL (FILES)` allows READY/Web maintenance.
- `SD LOW` does not rely on Web maintenance because archiving does not free SD memory.


### VAL-PERF-001 — 20 Hz sample-rate validation

Purpose:

Confirm that the recorder records acceleration at the required 20 Hz rate.

Procedure:

1. Record a file for a known duration.
2. Inspect the number of acceleration samples or acceleration blocks in the file.
3. Compare observed count with expected count at 20 Hz.

Expected result:

- Observed sample count is consistent with 20 Hz within the accepted tolerance.

Note:

The accepted tolerance and measurement method still need to be defined.

### VAL-PERF-002 — Acquisition jitter review

Purpose:

Confirm that acquisition timing jitter is minimized by architecture.

Procedure:

1. Review acquisition, ring-buffer, and SD-writing code.
2. Confirm acquisition is decoupled from SD writes by a ring/circular buffer.
3. If timing instrumentation is available, measure sample interval jitter during recording.

Expected result:

- Source review confirms acquisition is not directly blocked by SD writes.
- Measured jitter, if available, is within the accepted tolerance.

### VAL-FILE-001 — Recording block specification validation

Purpose:

Confirm recording file block types are implemented as specified.

Procedure:

1. Record a file.
2. Inspect file content using a script or binary decoder.
3. Confirm implemented block types:
   - `0x70` acceleration block;
   - `0x71` status/close block.
4. Confirm `0x72` calibration block appears before acceleration blocks.

Expected result:

- Implemented block types match the recording format specification.
- `0x72` validation is performed by confirming the first block is a valid calibration block before acceleration blocks. This has been validated on recorded file `FCFAG_20260517_222418.bin`.

### VAL-SD-004 — Daily recording file behavior

Purpose:

Confirm that multiple recording sessions on the same day are stored in one daily recording file.

Procedure:

1. Set a valid registration and RTC date.
2. Start and stop a recording.
3. Inspect the SD root and confirm that `/REGISTRATION_YYYYMMDD_1.bin` exists.
4. Start and stop a second recording on the same day.
5. Inspect the SD root again.
6. Decode the resulting file and confirm that both sessions are present, each with its own calibration block and close/status block sequence.

Expected result:

- The first same-day session creates `_1.bin`.
- The second same-day session renames/appends the daily file to `_2.bin`.
- No additional separate time-stamped recording file is created for the second session.
- The appended file remains decodable as a sequence of recording-session blocks.

### VAL-SD-005 — Web delete archives to processed folder

Purpose:

Confirm that Web delete preserves the file by moving it to `/processed`.

Procedure:

1. Create or identify a recording file in the SD root.
2. Enable WiFi and open the Web file-management page.
3. Request delete for the file.
4. Inspect SD card contents.

Expected result:

- `/processed` exists.
- The file is moved from SD root to `/processed`.
- The file is not shown in the active root-file list.
- If the destination name already exists, a `_N` suffix is added before the extension.

### VAL-SD-006 — SD maintenance access when recording is blocked

Purpose:

Confirm that SD max-file-count blocks recording but still allows Web file maintenance, while SD low-space blocks recording and requires SD replacement or external cleanup.

Procedure:

1. Create a root file-count condition at `SD_MAX_RECORD_FILES`, then separately create an SD low-space condition.
2. Confirm the recorder remains in READY with the applicable SD message.
3. Confirm MENU and START WIFI remain available.
4. Use the Web file-management page to archive one or more files.
5. Confirm the SD condition clears and recording can be started again when all other authorization conditions are satisfied.

Expected result:

- Recording start is blocked during the SD maintenance condition.
- MENU/WiFi/Web file maintenance remain available while not recording.
- Archiving files to `/processed` can clear the condition without removing the SD card.

### VAL-SD-007 — SD removal recovery into file-count maintenance

Purpose:

Confirm that SD removal during recording can recover into file-count
maintenance with MENU/START WIFI available.

Procedure:

1. Start with one less than the root file-count limit.
2. Start recording.
3. Remove the SD card while recording.
4. Confirm `NO SD` is displayed.
5. Reinsert the SD card.
6. Confirm `SD OK/CLR` is displayed.
7. Press power/clear to acknowledge.
8. Confirm `SD FULL (FILES)` is displayed and MENU is enabled.
9. Open MENU, start WiFi, and archive files through the Web interface.

Expected result:

- `SD FULL (FILES)` is orange/amber.
- MENU is enabled after recovery.
- START WIFI is enabled.
- Recording remains blocked until the file-count condition is cleared.

## 6. Known Validation Limitations

Validation limitations:

- Power-button start, USB start, and forced unconditional shutdown are managed by PMU hardware and should be validated as hardware behavior, not application software behavior.
- Driver/hardware fault injection is limited without abstraction/test hooks.
- Calibration fault behavior may require test hooks to validate safely.
- Calibration block `0x72` is implemented; recorded-file validation should confirm placement, checksum, and content.
- 20 Hz sample-rate and acquisition-jitter validation are performed using recorded-file timestamp differences. A first validation file showed exact 50 ms intervals and 20.000 Hz derived rate; final acceptance tolerance may still be defined.

## 7. Recommended Future Test Support

To reduce reliance on source review for fault cases, consider adding controlled test hooks or abstraction layers for:

- accelerometer read failure;
- ring-buffer overflow;
- SD write failure;
- SD low-space simulation;
- RTC date manipulation;
- calibration out-of-limit simulation;
- battery-low simulation.

These should be kept focused to avoid scope creep.


## 8. Traceability Use

Validation case identifiers in this document are referenced by the operational-to-software correspondence table in `01_recorder_requirements.md`.

The intended traceability direction is:

```text
Operational requirement -> derived software requirement -> implementation allocation -> validation case
```

Some requirements are validated by source review only because hardware fault injection or simulation hooks are not available in this baseline. Those cases are explicitly noted as limitations or test-support candidates.


A helper script is provided at `tools/validate_recording.py` to inspect recording block sequence, checksums, calibration block placement, 20 Hz timing statistics, and recorded X/Y/Z g-load. The g-load plot opens by default for interactive checks; use `--no-plot` for automated validation runs or `--plot-output <png>` to save the graph.


WiFi validation should include confirming that pressing BACK on the MENU page stops WiFi/AP support and returns to the main page. The HTTP listener intentionally remains alive internally; validation should verify externally visible AP/Web access rather than expecting the listener object to be destroyed.

## VAL-SD-LOW-002 — SD free-space threshold hysteresis

Verify that recording is blocked when SD free space is below `SD_RECORD_START_MIN_FREE_MB`.

Verify that a recording may continue after start until the lower in-recording threshold `SD_RECORD_LOW_FREE_MB` is reached.

Verify that crossing the in-recording low-space threshold causes the recorder to close the recording through the normal low-space close path and report `SD LOW`.


## VAL-CAL-INSTALL-001 — Installation calibration

Verify that recording is blocked after sensor calibration until installation calibration has been saved and that the device displays `INST CAL REQ` for this condition.

Verify that the Web installation calibration workflow accepts stable level-attitude samples, computes a matrix, and saves it to NVS.

Verify that a stable level-attitude sample reads approximately +1 g on corrected Z after the installation matrix is applied.

Verify that the 0x72 calibration block contains the saved installation calibration data.

## VAL-CAL-QUALITY-001 — Calibration rolling-window quality selection

Verify that accelerometer calibration keeps sampling a stable face after the first capture and stores an improved candidate only when the dominant face-axis noise improves. Verify that the Web page reports a simplified progress summary: validity status with NVS date when valid, session state, current face, samples processed on the current face, lowest stddev for the current face, current-face best-update count, and time since the last best update. Verify that the six-face summary shows captured faces as OK and missing faces as —, with unprocessed faces in plain text, the active face in amber only until processed, and processed faces in green. Verify that the workflow text advises leaving a face still until the last best update is more than 10 seconds old and saving when all six face values are satisfactory.

Verify that installation calibration keeps sampling after a valid candidate is found and updates the complete candidate, including the matrix, only when the quadratic stddev quality improves.

Verify that the Web UI progress counters continue to change while calibration is active, even when the best candidate is not improving.


Verify that installation calibration workflow text instructs the operator to put the glider in flight-level attitude with wings leveled following the AMM procedure, confirms sensor calibration must already be valid, advises waiting until the last best update is more than 10 seconds old, and tells the operator to save when noise is satisfactory.
