# Validation — Recorder v1.41

1. Compile and install Recorder v1.41.
2. Connect with Bridge v0.3.36.
3. Confirm the recorder home page shows File Management, Logbook, and Maintenance.
4. Press each button and confirm its page opens.
5. Confirm Return works from each page.
6. Open Maintenance, unlock it, and confirm Report Management, Firmware Update, and About open normally.
7. Confirm calibration reports can still be listed and downloaded.
8. Confirm processing and `/api/archive` behavior remain unchanged.

Static check performed: the corrected JavaScript fragment passes `node --check`.
