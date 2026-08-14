# Validation — Recorder v1.44

1. Compile and install Recorder v1.44.
2. Connect with the current SLM Bridge.
3. With USB power connected, confirm the recorder home page shows File Management, Logbook, and Maintenance active.
4. Remove USB power and confirm File Management and Logbook become grey while Maintenance remains reachable. Confirm the Main Menu displays `USB power required.` and `Connect USB power to use all recorder functions except Recorder Calibration.` Reconnect USB and confirm both the message disappears and the buttons become active after status refresh.
5. In File Management, select a recording day containing more than one physical `.bin` file and press Process.
6. Confirm the Flight Analysis status shows only `Processing.` while the selected day is still being processed.
7. Confirm the Process state remains locked consistently if Bridge callbacks identify a file as `name.bin`, `/name.bin`, or `/processed/name.bin`.
8. Confirm the completed day shows the final aggregate flight table, and `No flight detected` appears only when the completed day contains no flights.
9. Force or observe a transfer/analysis failure and confirm the panel changes from `Processing.` to an actionable failure message and the Process action becomes retryable. In SLM Bridge, confirm the message tells the operator to check Recorder and Server connections and that the file queue is clear before pressing Process again.
10. Confirm successfully processed flights are written to the companion `.log` and appear in Logbook.
11. Open Maintenance > About and confirm Restore Installation Calibration is present.
12. Using an SD card containing accepted/rejected installation-calibration reports in `/calibration_reports` and `/processed`, confirm Restore Installation Calibration selects the newest valid report matching the configured registration and restores only the installation calibration NVS record.
13. Confirm recorder calibration remains independently required on replacement hardware before recording is authorized.
14. Confirm Report Management, firmware update, recording SHA verification, and existing calibration functions remain unchanged.

Static checks performed for the v1.44 source delta:

- `src/tasks/web_ui/11_script_files_download.inc` passes `node --check` after extraction from its C++ raw string.
- Host-side state tests confirm root, leading-slash, and `/processed/` callback filenames resolve to one recorder transfer state.
- Host-side failure-event test confirms a failed transfer clears the normalized state, aborts the daily sequence, and replaces the stale `Processing.` indication with an actionable retry message.
- USB-gating UI inspection confirms the Main Menu message is driven by the same valid-and-present USB condition that enables File Management and Logbook.
