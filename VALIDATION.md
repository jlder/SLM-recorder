# Validation — Recorder v1.55 automation field-validation candidate

1. Compile and install Recorder v1.55.
2. Connect with SLM Bridge v0.3.48.
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
19. Select French again and confirm the AMOLED version lines show `ver. logic. 1.55` and `ver. mat. 1.00`, and the agreed abbreviated French labels fit without clipping.
20. On the main page, confirm `ENREGIST.` and all other status messages fit fully inside the bottom status area without clipping.
21. In REGLAGES/SETTINGS, confirm both `FRANÇAIS` and `ENGLISH` fit comfortably in the language button using the reduced font.
22. With WiFi active, reload the recorder Web interface and confirm all normal pages, calibration workflows, support messages, file-processing messages, and firmware-update messages follow the recorder-selected language.
23. Confirm changing recorder language does not alter required-settings state, calibration state, API reason codes, filenames, or existing calibration-report contents/parsing.
24. Confirm Bridge-originated transfer/OTA failures are rendered by the recorder page using recorder-selected language rather than a Bridge-provided human-readable sentence.
25. With SLM Bridge v0.3.48, enable recorder WiFi and connect. Confirm the recorder Web page appears normally and remains usable for at least 30 seconds while the Bridge health monitor continues to report the recorder connected.
26. Confirm the recorder Web page is self-contained: loading `/` shall not require `/api/language.js` or any other new recorder-side script/style resource. Existing API calls such as `/api/status` remain unchanged except for the added `language` field.
27. With the recorder Web page open, change the recorder language and confirm the page follows the new `fr`/`en` value on the next `/api/status` refresh without navigating away from the current page.

28. In French mode, inspect the AMOLED screens containing `RÉGLAGES`, `DÉBUT ENREG.`, `ARRÊT ENREG.`, `PRÊT`, `ÉCHEC ENREG.`, `FRANÇAIS`, `Année` and `5 caractères A-Z / 0-9`; confirm accents are rendered correctly with no missing-glyph boxes or clipping.
29. Confirm accented characters use the same Montserrat base letter shape as unaccented characters and that acute, grave, circumflex and cedilla marks remain inside the normal line height at all UI font sizes.
30. Confirm the recorder Web interface displays the same French accents correctly and that the v1.47 self-contained Web/WiFi behavior is unchanged.

31. Perform a successful phone-file OTA update and confirm the recorder returns HTTP success before Wi-Fi disappears for reboot; verify the successful response/restart transition remains reliable with the 2000 ms OTA acknowledgement grace.
32. Perform Firmware from Server through SLM Bridge v0.3.48 and confirm the same successful acknowledgement/reboot behavior.
33. Confirm support-triggered reboot behavior remains unchanged apart from using its explicit 500 ms delay.

34. Load the recorder Web page through SLM Bridge and confirm the three main-menu buttons appear with their labels already present; confirm no button is displayed as a plain coloured rectangle without text at any point during loading.
35. Confirm the page is served in the recorder-selected language only, that it does not visibly re-translate itself after the first `/api/status` response, and that changing the recorder language from the AMOLED while the page is open causes the page to reload in the new language.
36. Reload the page and confirm the browser revalidates with `304 Not Modified` while the firmware version and selected language are unchanged, and performs a full download after either changes.
37. Download a recording of at least 8 MB and confirm the file is byte-identical to the SD copy and passes SHA-256 verification. Repeat with the same file to confirm repeatability.
38. Confirm downloads still complete correctly if the read-ahead buffer cannot be allocated, by exercising the direct per-chunk fallback path.
39. With calibration valid and the recorder in READY, confirm no accelerometer driver write occurs while Web calibration status is polled, and that a driver update does occur after a calibration is saved, accepted, cleared, or restored.
40. Confirm recorded acceleration remains correctly calibrated after a calibration change followed by a recording, and that a recording started immediately after boot uses the stored calibration.

## v1.55 automation field-validation additions

41. Confirm SETTINGS > AUTOMATION shows AUTO RECORDING, AUTO WIFI, and AUTO DELETE all `ON` immediately after boot. Confirm the three controls are disabled/fixed in this diagnostic build and cannot be toggled OFF.
42. Confirm the forced-ON diagnostic override does not rewrite the stored NVS automation selections; it is an effective runtime override only.
43. Confirm all three automations are observe-only: automation never physically starts/stops a recording, never automatically enables/disables WiFi, and never deletes a `.bin` or `.sha`. Manual controls, faults, and <=5% battery shutdown remain effective.
44. Manually start one long recording and confirm the virtual AUTO START trace responds to either motion RMS >=0.020 g for 1 s or `max(|HP0.10Hz(Nx)|, |HP0.10Hz(Ny)|) >=0.020 g` for 1 s.
45. Confirm virtual no-flight sessions use the 300 s quiet timeout and log `WOULD_AUTO_DELETE` when they end without `flight_seen`; the physical recording remains open and retained.
46. Confirm `flight_seen` uses the continuous causal HIRMS/LOWRMS primary and secondary paths and that a virtual flight session cannot be ended by motion quiet after `flight_seen` latches.
47. Confirm the ordered landing trace: HIRMS >=0.050 g for >=3 s, FG >=0.10 during that HIRMS event, HIRMS subsequently below 0.050 g, FG <=0.020 for 2 s within 25 s, then 50 s continuous GROUND; FG >=0.10 cancels a GROUND candidate.
48. Confirm virtual AUTO WIFI logs ON in virtual READY, OFF for confirmed motion or virtual recording, and ON again after 5 s quiet, while actual manual WiFi requested/AP transitions remain separately visible in the diagnostic event CSV.
49. Confirm diagnostic overlay events are added only to the SD-bound sample copy and appear with the expected >=50 ms telemetry delay; recorder-side HIRMS/LOWRMS/motion results remain based on untouched acceleration.
50. Run `tools/automation_diag_decode.py` on a diagnostic recording. Confirm it writes both a clean `.bin` and an event CSV and that a known round-trip test restores the original binary byte-for-byte, including valid 0x70 checksums.
51. Analyze a diagnostic `.bin` directly through the recorder Web File Management/Flight Analysis path. Confirm the SLM decoder validates the stored checksum, removes diagnostic axis offsets, and supplies only restored acceleration to HIRMS/LOWRMS analysis.
52. Regression-test `FCJAF_20260823_3.bin`: confirm recorder-side analysis of the encoded file returns the same two real flights as analysis of the cleaned stream, rather than the false multi-segment result produced by v1.54 before restoration was added.
53. With USB absent, leave the UI inactive past `DISPLAY_DIM_TIMEOUT_MS` and confirm full display standby. With USB present, confirm the same timeout dims the display to approximately 50% rather than switching it off; local activity restores full brightness.
54. At battery <=5%, confirm an active manual recording closes and the recorder shuts down regardless of the observe-only automation mode.

Static checks for the v1.55 field-validation source candidate:

- `automation_service.cpp` and `state_task.cpp` pass host C++17 syntax compilation with warnings treated as errors using the project stubs.
- Historical replay of the frozen detector gives 190/190 pre-roll AUTO START coverage and 189/189 evaluable flight-end confirmations with zero premature stops and zero misses; `FCJAF_20260627_3.bin` is right-censored at landing.
- Source-path inspection confirms diagnostic event evaluation/queueing occurs after the current recording sample has been pushed; pre-push diagnostic overhead is limited to three precomputed integer additions.
- A real SLM binary instrumented with primary/extended diagnostic events decodes back to the original binary byte-for-byte.
- Observe-only guards suppress automatic physical recording start/stop, AUTO WIFI actuation, and AUTO DELETE while retaining virtual policy/event evaluation.
- The v1.55 Web SLM decoder restores diagnostic axis offsets only after stored-block checksum validation and before conversion to g/HIRMS/LOWRMS analysis.
- Host regression on `FCJAF_20260823_3.bin` confirms the v1.55 restoration rule reconstructs the same 288156 acceleration samples as the Python-cleaned diagnostic stream.

Static checks performed for the v1.51 source delta:

- Host-side tests of `sd_files_download_read()` against a stubbed SD layer confirm byte-exact reassembly at 1, 7, 1436, 2920, and 65536-byte request sizes, for file sizes that are and are not multiples of the read-ahead block, and for the no-PSRAM fallback path.
- A negative host-side test confirms that an SD read returning more than the requested length is rejected rather than accepted as valid file content.
- Preprocessor and brace/parenthesis balance checks pass on the modified `web_task.cpp`, `sd_files.cpp`, and `html_interface.h`.
- Extraction of the served Web page from its C++ raw-string segments confirms every `<script>` block parses, the translation catalog parses as JSON, and every `data-i18n` and `tr()` key resolves in both languages.
- Call-site inspection confirms every `calibration_service_refresh_status()` call is made outside the calibration-service mutex and that none of its callees take that mutex, so the added guard cannot nest.

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


## State-task ownership audit

- Verify no UI/Web source calls any `settings_set_*()` function or `settings_clear()`.
- Verify `web_task_set_enabled()` runtime callers are State task only.
- Verify `Preferences` for the recorder settings namespace is private to `settings_store.cpp`.
- Verify UI date/time/registration/language/automation actions are posted to State task request APIs.
