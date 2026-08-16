# Validation — Recorder v1.49

1. Compile and install Recorder v1.49.
2. Connect with SLM Bridge v0.3.47.
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
17. On a recorder with no stored language key, confirm French is selected by default and the SETTINGS language button shows `ENGLISH`.
18. Press `ENGLISH` and confirm the AMOLED interface immediately changes to English and the button changes to `FRANÇAIS`; power-cycle and confirm the selection persists.
19. Select French again and confirm the AMOLED version lines show `ver. logic. 1.49` and `ver. mat. 1.00`, and the agreed abbreviated French labels fit without clipping.
20. On the main page, confirm `ENREGIST.` and all other status messages fit fully inside the bottom status area without clipping.
21. In REGLAGES/SETTINGS, confirm both `FRANÇAIS` and `ENGLISH` fit comfortably in the language button using the reduced font.
22. With WiFi active, reload the recorder Web interface and confirm all normal pages, calibration workflows, support messages, file-processing messages, and firmware-update messages follow the recorder-selected language.
23. Confirm changing recorder language does not alter required-settings state, calibration state, API reason codes, filenames, or existing calibration-report contents/parsing.
24. Confirm Bridge-originated transfer/OTA failures are rendered by the recorder page using recorder-selected language rather than a Bridge-provided human-readable sentence.
25. With SLM Bridge v0.3.47, enable recorder WiFi and connect. Confirm the recorder Web page appears normally and remains usable for at least 30 seconds while the Bridge health monitor continues to report the recorder connected.
26. Confirm the recorder Web page is self-contained: loading `/` shall not require `/api/language.js` or any other new recorder-side script/style resource. Existing API calls such as `/api/status` remain unchanged except for the added `language` field.
27. With the recorder Web page open, change the recorder language and confirm the page follows the new `fr`/`en` value on the next `/api/status` refresh without navigating away from the current page.

28. In French mode, inspect the AMOLED screens containing `RÉGLAGES`, `DÉBUT ENREG.`, `ARRÊT ENREG.`, `PRÊT`, `ÉCHEC ENREG.`, `FRANÇAIS`, `Année` and `5 caractères A-Z / 0-9`; confirm accents are rendered correctly with no missing-glyph boxes or clipping.
29. Confirm accented characters use the same Montserrat base letter shape as unaccented characters and that acute, grave, circumflex and cedilla marks remain inside the normal line height at all UI font sizes.
30. Confirm the recorder Web interface displays the same French accents correctly and that the v1.47 self-contained Web/WiFi behavior is unchanged.

31. Perform a successful phone-file OTA update and confirm the recorder returns HTTP success before Wi-Fi disappears for reboot; verify the successful response/restart transition remains reliable with the 2000 ms OTA acknowledgement grace.
32. Perform Firmware from Server through SLM Bridge v0.3.47 and confirm the same successful acknowledgement/reboot behavior.
33. Confirm support-triggered reboot behavior remains unchanged apart from using its explicit 500 ms delay.

Static checks performed for the v1.49 source delta:

- `src/tasks/web_ui/11_script_files_download.inc` and the JavaScript portion of `17_script_ota_startup_end.inc` pass `node --check` after extraction from their C++ raw strings.
- File-processing progress inspection confirms recorder transfer maps to the first 95% of each physical file contribution and the existing staged analysis callbacks map to the final 5%, with day progress weighted by file size.
- Firmware-page DOM/handler inspection confirms server and phone selections are mutually exclusive and feed one common Update Firmware action.
- Host-side state tests confirm root, leading-slash, and `/processed/` callback filenames resolve to one recorder transfer state.
- Host-side failure-event test confirms a failed transfer clears the normalized state, aborts the daily sequence, and replaces the stale `Processing.` indication with an actionable retry message.
- USB-gating UI inspection confirms the Main Menu message is driven by the same valid-and-present USB condition that enables File Management and Logbook.
- Translation-catalog validation confirms every referenced translation key exists, no duplicate keys are present, and the French catalog uses only the approved accented glyph set `À Ç É Ê à ç è é ê`.
- Host-side accent-font tests exercise all 9 accented glyphs at all 7 recorder UI sizes (63 combinations), confirm ASCII falls through to the original LVGL Montserrat font, and verify guard bytes around the A8 render buffer remain intact.
- Host syntax compilation passes for `src/ui/slm_fonts.cpp`; the complete generated UTF-8 Web page contains the accented catalog, remains self-contained, and all inline JavaScript passes `node --check`.
- AMOLED status-catalog inspection confirms all main-screen English/French status messages are 14 characters or fewer after the display-fit abbreviations.
- Extracted Web JavaScript passes `node --check`; the obsolete browser sample-period formatter/calculation is absent.
- Host syntax checks pass for `language.cpp`, `settings_store.cpp`, and `ui_message.cpp` using minimal Arduino/Preferences stubs.
- Source inspection confirms recorder Web UI code no longer consumes `detail.message` from Bridge transfer/OTA events.
- Host compilation of `html_interface.h` confirms the complete self-contained Web page is a valid C++ string object; the resulting page contains no external `<script src>` or stylesheet resource.
- Extracted JavaScript from the complete generated HTML passes `node --check`; all 248 statically referenced Web translation keys resolve in the 329-entry central catalog.
- Source comparison confirms `ensure_server_ready_()`, `start_ap_and_server()`, `stop_ap_and_server()`, `web_task_set_enabled()`, `web_task_loop()`, and `state_task.cpp` WiFi/shutdown logic are unchanged from the v1.45 baseline.
