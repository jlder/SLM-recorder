# SLM Recorder v1.41 corrective delta

Fixes a recorder web-interface JavaScript regression introduced during removal of the calibration-report archive action.

The obsolete promise `.catch(...)` tail remained in `15_script_archive_status_diag.inc` after its parent function was removed. This caused a JavaScript syntax error, preventing all menu click handlers from loading. The File Management, Logbook, and Maintenance buttons therefore appeared but did nothing.

No Bridge change is required.
