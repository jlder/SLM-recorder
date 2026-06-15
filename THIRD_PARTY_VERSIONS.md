# Third-Party Versions

This file records the third-party libraries used by this AgingGliders recorder
firmware release.

| Component | Author / Publisher | License | Version Used | Source / Package Location |
|---|---|---|---:|---|
| Async TCP / AsyncTCP | ESP32Async / me-no-dev lineage | LGPL-3.0 | 3.4.10 | https://github.com/ESP32Async/AsyncTCP |
| ESPAsync WebServer | ESP32Async / me-no-dev lineage | LGPL-3.0 | 3.11.0 | https://github.com/ESP32Async/ESPAsyncWebServer |
| GFX Library for Arduino / Arduino_GFX | Moon On Our Nation; includes Adafruit-derived code | BSD-2-Clause | 1.6.0 | https://github.com/moononournation/Arduino_GFX |
| SensorLib | Lewis He | MIT | 0.3.1 | https://github.com/lewisxhe/SensorLib |
| XPowersLib / XPowerLib | Lewis He | MIT | 0.3.0 | https://github.com/lewisxhe/XPowersLib |
| LVGL | LVGL project / Gabor Kiss-Vamosi | MIT | 9.3.0 | https://github.com/lvgl/lvgl |

## Release Notes

- The versions above are the required library versions for this release.
- Later library versions may not be compatible without code changes.
- The LGPL libraries listed above were not modified for this project.
- Keep this file with the firmware source release so the build can be traced to
  the dependency set used.

Arduino_DriveBus is not a dependency of this release. FT3168 touch access is implemented directly using Wire/I2C in the project source.
