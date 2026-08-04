# Recorder v1.39 validation

1. Confirm `POST /api/archive?file=<name>` moves the matching `.bin`, `.sha`, and `.log` companions to `/processed`.
2. Confirm the retired archive endpoint is no longer registered.
3. Confirm calibration reports remain downloadable but no report archive action or endpoint is present.
4. Confirm File Management, Logbook, Maintenance, About, SHA verification, and calibration report pages load without JavaScript errors.
5. Confirm the Bridge v0.3.36 archives a verified upload successfully through `/api/archive`.
