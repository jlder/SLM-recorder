# Validation — Recorder v1.45

1. Compile and install Recorder v1.45.
2. Connect with SLM Bridge v0.3.44.
3. With USB power connected, confirm the recorder home page shows File Management, Logbook, and Maintenance active.
4. Remove USB power and confirm File Management and Logbook become grey while Maintenance remains reachable. Confirm the Main Menu displays `USB power required.` and `Connect USB power to use all recorder functions except Recorder Calibration.` Reconnect USB and confirm both the message disappears and the buttons become active after status refresh.
5. In File Management, select a recording day containing more than one physical `.bin` file and press Process.
6. Confirm the Flight Analysis status shows `Processing: N%` while the selected day is being processed. Confirm the percentage advances during both recorder transfer and browser analysis, is weighted across all physical files for the selected day, and reaches 100% only when the final file completes analysis.
7. Confirm the Process state remains locked consistently if Bridge callbacks identify a file as `name.bin`, `/name.bin`, or `/processed/name.bin`.
8. Confirm the completed day shows the final aggregate flight table, and `No flight detected` appears only when the completed day contains no flights.
9. Force or observe a transfer/analysis failure and confirm the panel changes from the active `Processing: N%` indication to an actionable failure message and the Process action becomes retryable. In SLM Bridge, confirm the message tells the operator to check Recorder and Server connections and that the file queue is clear before pressing Process again.
10. Confirm successfully processed flights are written to the companion `.log` and appear in Logbook.
11. Open Maintenance > About and confirm Restore Installation Calibration is present.
12. Using an SD card containing accepted/rejected installation-calibration reports in `/calibration_reports` and `/processed`, confirm Restore Installation Calibration selects the newest valid report matching the configured registration and restores only the installation calibration NVS record.
13. Confirm recorder calibration remains independently required on replacement hardware before recording is authorized.
14. Open Firmware Update and confirm the page consists of the information card, one **Select Firmware** card (server preferred / phone if requested by support), and one action card containing **Update Firmware** and **Return**. Confirm the restart warning states that the Wi-Fi connection will be lost and should be restarted.
15. Test both server selection and phone-file selection and confirm the common Update Firmware button installs the currently selected source only.
16. Confirm Report Management, recording SHA verification, and existing calibration functions remain unchanged.

Static checks performed for the v1.45 source delta:

- `src/tasks/web_ui/11_script_files_download.inc` and the JavaScript portion of `17_script_ota_startup_end.inc` pass `node --check` after extraction from their C++ raw strings.
- File-processing progress inspection confirms recorder transfer maps to the first 95% of each physical file contribution and the existing staged analysis callbacks map to the final 5%, with day progress weighted by file size.
- Firmware-page DOM/handler inspection confirms server and phone selections are mutually exclusive and feed one common Update Firmware action.
- Host-side state tests confirm root, leading-slash, and `/processed/` callback filenames resolve to one recorder transfer state.
- Host-side failure-event test confirms a failed transfer clears the normalized state, aborts the daily sequence, and replaces the stale `Processing.` indication with an actionable retry message.
- USB-gating UI inspection confirms the Main Menu message is driven by the same valid-and-present USB condition that enables File Management and Logbook.
