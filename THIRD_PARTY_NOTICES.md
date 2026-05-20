# Third-Party Notices

This project uses third-party libraries and board support packages that are
licensed separately from the AgingGliders recorder application code.

The AgingGliders project license applies only to project-owned source files.
Third-party libraries remain under their own licenses.

Known third-party components used by this recorder firmware release are:

| Component | Author / Publisher | License |
|---|---|---|
| Async TCP / AsyncTCP | ESP32Async / me-no-dev lineage | LGPL-3.0 |
| ESPAsync WebServer | ESP32Async / me-no-dev lineage | LGPL-3.0 |
| GFX Library for Arduino / Arduino_GFX | Moon On Our Nation; includes Adafruit-derived code | BSD-2-Clause |
| SensorLib | Lewis He | MIT |
| XPowersLib / XPowerLib | Lewis He | MIT |
| LVGL | LVGL project / Gabor Kiss-Vamosi | MIT |

## Notes

- The AgingGliders project license does not relicense third-party libraries.
- The GFX Library license text found for this dependency contains the two
  redistribution clauses and warranty disclaimer, but does not contain the BSD
  non-endorsement clause. It is therefore recorded here as `BSD-2-Clause`.
- The LGPL libraries used by this firmware were not modified for this project.
- Exact dependency versions and source locations are recorded in
  `THIRD_PARTY_VERSIONS.md`.
- Arduino_DriveBus is not used by this firmware release; FT3168 touch
  access is implemented directly in project-owned code.
