// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file config.h
 * @brief Project-wide configuration constants for timing, buffers, task sizing, and SD/file limits.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

/*******************************************************************************
 * CONFIGURATION & CONSTANTS
 ******************************************************************************/

#pragma once

#include <Arduino.h>

// Hardware pins
#define GPIO_RECORD_SWITCH  0
#define GPIO_POWER_BUTTON   10

// Acceleration recording
#define ACCEL_READ_MAX_TRIES    3

// PMU / touch / RTC retries
#define PMU_BATT_PCT_MAX_RETRIES 3
#define PMU_USB_MAX_RETRIES      3
#define PMU_BATT_LOW_THRESHOLD_PCT 5u
#define TOUCH_READ_MAX_RETRIES   3
#define RTC_READ_MAX_RETRIES     3
#define TOUCH_INIT_POST_WRITE_DELAY_MS 50u
#define TOUCH_INIT_RETRY_DELAY_MS      250u
#define TOUCH_INIT_ATTEMPTS            4u

// Packet format
#define PACKET_SYNC_BYTE     0x55
#define PACKET_TYPE_ACCEL    0x70
#define PACKET_TYPE_STATUS   0x71
#define PACKET_TYPE_CALIBRATION 0x72

// WiFi / web
#define AP_SSID_PREFIX               "SLM2-"
#define WEB_SERVER_PORT              80
#define AP_IP_ADDRESS                IPAddress(192, 168, 4, 1)
#define AP_GATEWAY                   IPAddress(192, 168, 4, 1)
#define AP_SUBNET                    IPAddress(255, 255, 255, 0)
#define WEB_SINGLE_CLIENT_TIMEOUT_MS 60000
#define WEB_SD_BUSY_STALE_MS         30000u
// WiFi is normally a USB-powered maintenance mode. Recorder calibration is
// the only supported battery-powered Web workflow. If WiFi remains enabled on
// battery while recorder calibration is not active, shut the recorder down
// after this grace period so an unattended recorder cannot discharge itself.
#define WIFI_BATTERY_IDLE_SHUTDOWN_MS 180000u


// Browser-side flight-time analysis parameters.
// These values are embedded in the served JavaScript flight-analysis page at
// compile time by html_interface.h / web_ui/12_script_flight_decode.inc.
#define FLIGHT_ANALYSIS_FS_HZ                         20.0
#define FLIGHT_ANALYSIS_HIRMS_WINDOW_S                 4.0
#define FLIGHT_ANALYSIS_LOWRMS_WINDOW_S               10.0
#define FLIGHT_ANALYSIS_FLIGHTGROUND_LPF_PERIOD_S     20.0
#define FLIGHT_ANALYSIS_FLIGHTGROUND_THRESHOLD         0.05
#define FLIGHT_ANALYSIS_FLIGHTGROUND_HYSTERESIS        0.10
#define FLIGHT_ANALYSIS_SEARCH_WINDOW_S               80.0
#define FLIGHT_ANALYSIS_TO_ROLL_START_THR              0.10
#define FLIGHT_ANALYSIS_TO_ROLL_END_THR                0.25
#define FLIGHT_ANALYSIS_LDG_ROLL_START_THR             0.25
#define FLIGHT_ANALYSIS_LDG_ROLL_END_THR               0.10
#define FLIGHT_ANALYSIS_MIN_FILE_S                    30.0
#define FLIGHT_ANALYSIS_BUTTER_Q1                      0.541196100146
#define FLIGHT_ANALYSIS_BUTTER_Q2                      1.306562964876
// Landing validation gate used by the browser-side flight-time analysis.
// A FlightGround flight->ground transition is accepted only when normalized
// high-frequency RMS has exceeded the configured peak threshold within the
// preceding age window. This prevents calm in-flight FlightGround crossings
// from being interpreted as landings while still accepting touchdown/rollout
// vibration peaks that occur shortly before the detected transition. Set
// FLIGHT_ANALYSIS_LDG_HIRMS_GATE_ENABLED to 0 to use the FlightGround-only
// transition logic.
#define FLIGHT_ANALYSIS_LDG_HIRMS_GATE_ENABLED         1
#define FLIGHT_ANALYSIS_LDG_HIRMS_PEAK_MIN_NORM        0.20
#define FLIGHT_ANALYSIS_LDG_HIRMS_PEAK_MAX_AGE_S      20.0
// Takeoff validation gate used by the browser-side flight-time analysis.
// A FlightGround ground->flight transition is accepted only when normalized
// high-frequency RMS has exceeded the configured peak threshold within the
// preceding age window. This confirms rolling vibration before accepting a
// takeoff after quiet/edge FlightGround excursions.
#define FLIGHT_ANALYSIS_TO_HIRMS_GATE_ENABLED          1
#define FLIGHT_ANALYSIS_TO_HIRMS_PEAK_MIN_NORM         0.20
#define FLIGHT_ANALYSIS_TO_HIRMS_PEAK_MAX_AGE_S       30.0

// Transition hardening for browser-side flight-time analysis.
// The recorder is assumed to start on the ground. Ground->flight requests during
// the startup settling time are ignored; strict takeoff transition requires the
// signal to have been observed below the ON threshold before a later upward
// crossing is accepted. The confirmation time rejects short bounces; accepted
// transitions are back-filled to the first candidate sample by the browser logic.
#define FLIGHT_ANALYSIS_TO_STARTUP_IGNORE_S            5.0
#define FLIGHT_ANALYSIS_STRICT_TAKEOFF_TRANSITION_ENABLED 1
#define FLIGHT_ANALYSIS_TRANSITION_CONFIRM_S           2.0

// Automatic operation. Automation is opt-in and is reset to OFF whenever the
// firmware version or this settings schema changes. The automatic-start
// detectors are independent of the live flight-presence/flight-end detector.
#define AUTOMATION_SETTINGS_SCHEMA_VERSION              1u
#define AUTO_RECORD_MOTION_WINDOW_S                      2.0
#define AUTO_RECORD_MOTION_THRESHOLD_G                   0.020
#define AUTO_RECORD_START_CONFIRM_S                      1.0
#define AUTO_RECORD_STOP_QUIET_S                       300.0

// Additional low-frequency attitude-change trigger used by AUTO RECORDING.
// Two independent first-order causal high-pass filters run continuously on
// installation-aligned Nx and Ny. Either axis may satisfy the common 1 s
// automatic-start confirmation above.
#define AUTO_RECORD_ATTITUDE_HP_HZ                        0.10
#define AUTO_RECORD_ATTITUDE_THRESHOLD_G                  0.020

// Live flight-presence detector. HIRMS/LOWRMS filters run continuously at
// 20 Hz and are not reset at a recording boundary. LOWRMS uses the validated
// 0.25 Hz high-pass followed by the existing 3 Hz low-pass.
#define AUTO_FLIGHT_LOWRMS_HP_HZ                         0.25
#define AUTO_FLIGHT_HIRMS_G                              0.050
#define AUTO_FLIGHT_PRIMARY_DELTA_G                      0.120
#define AUTO_FLIGHT_PRIMARY_CONFIRM_S                    4.0
#define AUTO_FLIGHT_LATE_LOWRMS_G                        0.050
#define AUTO_FLIGHT_LATE_CONFIRM_S                       5.0

// Automatic flight-end detector. It reuses the causal HIRMS/LOWRMS signals
// above and runs only after flight_seen has latched for the current recording.
// FlightGround is normalized by the running flight-local HIRMS range.
#define AUTO_FLIGHT_END_HIRMS_EVENT_CONFIRM_S            3.0
#define AUTO_FLIGHT_END_FG_FLIGHT_NORM                   0.100
#define AUTO_FLIGHT_END_EVENT_WINDOW_S                  25.0
#define AUTO_FLIGHT_END_FG_GROUND_NORM                   0.020
#define AUTO_FLIGHT_END_FG_CONFIRM_S                     2.0
#define AUTO_FLIGHT_END_GROUND_CONFIRM_S                50.0
#define AUTO_FLIGHT_END_HIRMS_RANGE_MIN_G                0.001

// Field diagnostic overlay for AUTO RECORD validation. When enabled, only the
// SD-bound copy of selected acceleration samples is modified. Recorder-side
// signal processing always consumes the untouched corrected sample. Diagnostic
// event encoding is computed after the recording-ring push and therefore appears
// in a later sample (normally one 50 ms cycle later).
//
// The QMI8658 is configured for +/-8 g. A +/-10 g clean-axis bound therefore
// leaves room for an exactly reversible three-band encoding: diagnostic trits
// add -20.001 g, 0 g, or +20.001 g independently to X/Y/Z. The three trits
// encode one wire event code in the range 0..26; code 13 is NO EVENT and leaves
// all axes unchanged. tools/automation_diag_decode.py removes the overlay.
#define AUTOMATION_DIAGNOSTIC_OVERLAY_ENABLED             1
// Dedicated field-test mode: detector/policy decisions are logged, but AUTO
// RECORD / AUTO WIFI / AUTO DELETE are not allowed to actuate recorder state,
// WiFi, or file deletion.
#define AUTOMATION_DIAGNOSTIC_OBSERVE_ONLY                 1
// v1.54 field-validation build: all three automation functions are forced
// logically ON from boot. Their decisions are evaluated/logged only; the
// observe-only guard still prevents recorder/WiFi/file actuation. Stored NVS
// selections are not rewritten and are ignored while this diagnostic mode is on.
#define AUTOMATION_DIAGNOSTIC_FORCE_ALL_ON                  1
#define AUTOMATION_DIAGNOSTIC_AUTO_WIFI_QUIET_S            5.0
#define AUTOMATION_DIAGNOSTIC_CLEAN_AXIS_LIMIT_MG     10000
#define AUTOMATION_DIAGNOSTIC_AXIS_OFFSET_MG          20001
#define AUTOMATION_DIAGNOSTIC_QUEUE_DEPTH                64u


// Software watchdog
#define WATCHDOG_TIMEOUT_MS             3000u
#define WATCHDOG_CHECK_PERIOD_MS        1000u
#define WATCHDOG_PREFS_NAMESPACE        "slm-fault"
#define WATCHDOG_PREFS_KEY              "wdg"

// User-visible button timing
#define POWER_CLEAR_HOLD_MS     150
#define POWER_SHUTDOWN_HOLD_MS  2000
#define RECORD_START_HOLD_MS    500
#define RECORD_STOP_HOLD_MS     2000

// Display brightness management
#define DISPLAY_BRIGHTNESS_ACTIVE      255u
#define DISPLAY_BRIGHTNESS_DIMMED      128u
#define DISPLAY_DIM_TIMEOUT_MS         10000u

// Accelerometer calibration
#define CALIBRATION_PREFS_NAMESPACE        "slm-cal"
// Version of the calibration metadata block written to recording files (0x72).
// This is a file-format version, not an NVS storage-schema version.
#define CALIBRATION_RECORD_VERSION         3u
// NVS storage-schema versions for split calibration records.
// IMPORTANT: if the packed NVS payload, key meaning, checksum coverage, or
// interpretation of one of these stored records changes, bump the matching
// storage version and update the load/reject/migration handling in
// calibration_store.cpp. Do not change the storage layout without changing
// the corresponding storage version.
#define CALIBRATION_SENSOR_STORAGE_VERSION  2u
#define CALIBRATION_INSTALL_STORAGE_VERSION 1u
#define CALIBRATION_GRAVITY_MG             1000.0f
#define CALIBRATION_VALIDITY_MONTHS        12u
#define CALIBRATION_FACE_GRAVITY_TOL_PCT   10.0f
#define INSTALLATION_GRAVITY_TOL_PCT        10.0f
#define CALIBRATION_SAMPLE_PERIOD_MS       50u
// Number of samples in the calibration stability window.
// Requirement: must be greater than 0 because mean/stddev computation divides by this value.
#define CALIBRATION_WINDOW_SAMPLE_COUNT    40u
#define CALIBRATION_STABILITY_STDDEV_MAX_MG 2.5f
#define CALIBRATION_STABILITY_STDDEV_MIN_MG 0.05f
#define CALIBRATION_GAIN_MIN               0.8f
#define CALIBRATION_GAIN_MAX               1.2f
#define CALIBRATION_OFFSET_ABS_MAX_MG      200.0f
#define CALIBRATION_GAIN_DELTA_MAX          0.05f
#define CALIBRATION_OFFSET_DELTA_MAX_MG     50.0f
#define CALIBRATION_TEMP_MIN_C              25.0f
#define CALIBRATION_TEMP_MAX_C              55.0f
#define CALIBRATION_TEMP_MAX_SPAN_C         3.0f


// Error audio alert. Audio hardware remains disabled outside ST_ERROR.
#define AUDIO_ALERT_BEEP_COUNT          3u
#define AUDIO_ALERT_BEEP_MS             250u
#define AUDIO_ALERT_GAP_MS              250u
// Silent warm-up required for equal first and subsequent beeps.
#define AUDIO_ALERT_PREROLL_SILENCE_MS  300u
#define AUDIO_ALERT_TRAILING_SILENCE_MS 100u
#define AUDIO_ALERT_REPEAT_MS           4000u
// Persistent USB-power-loss warning used while AUTO RECORDING waits in READY.
#define AUDIO_USB_LOSS_BEEP_MS          100u
#define AUDIO_USB_LOSS_GAP_MS           100u
#define AUDIO_USB_LOSS_REPEAT_MS        2000u
#define AUDIO_ALERT_TONE_HZ             1000u
#define AUDIO_ALERT_AMPLITUDE           7000
#define AUDIO_ALERT_VOLUME_PERCENT      60u
#define AUDIO_ALERT_AMP_SETTLE_MS       20u

// Product version displayed on the device main screen.
// Hardware version identifies the recorder hardware configuration.
// Software version identifies the firmware build.
#define RECORDER_HARDWARE_VERSION      "1.00"
#define RECORDER_SOFTWARE_VERSION      "1.54"

// Storage / SD
#define PREFS_NAMESPACE              "slm-data"
#define FILENAME_MAX_LENGTH          64
#define SD_MAX_RECORD_FILES          50

// SD file-management sizing. Keep these derived from the filename/file-count
// policy so the web API, SD request buffers, and storage list buffers stay in sync.
#define SD_MOUNT_PREFIX              "/sdcard"
#define SD_STORAGE_PATH_MAX          ((sizeof(SD_MOUNT_PREFIX) - 1u) + 1u + FILENAME_MAX_LENGTH + 1u)
#define SD_FILE_LIST_JSON_ENTRY_MAX  (FILENAME_MAX_LENGTH + 64u)
#define SD_FILE_LIST_JSON_MAX        (2u + (SD_MAX_RECORD_FILES * (SD_FILE_LIST_JSON_ENTRY_MAX + 1u)) + 1u)
#define SD_FILE_OP_TIMEOUT_MS        2000u
#define SD_SHA_VERIFY_TIMEOUT_MS      60000u
#define FLIGHT_LOG_TEXT_MAX_BYTES    4096u

#define SD_IO_FAIL_LIMIT             3u
#define SD_WRITE_RETRY_MAX           3u
#define SD_RECORD_START_MIN_FREE_MB   500u
#define SD_RECORD_LOW_FREE_MB         250u
#define SD_RECORD_FLUSH_PERIOD_MS    500u
#define SD_TASK_PERIOD_MS            50u
#define SD_TASK_FILE_OP_PERIOD_MS    1u
#define SD_IDLE_REPROBE_PERIOD_MS    500u
#define SD_ERROR_REPROBE_PERIOD_MS   500u

// Hardware addresses
#define AXP2101_ADDRESS         AXP2101_SLAVE_ADDRESS
#define FT3168_ADDRESS          0x38u
