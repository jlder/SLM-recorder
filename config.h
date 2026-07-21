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
#define AP_SSID_PREFIX               "SLM-"
#define WEB_SERVER_PORT              80
#define AP_IP_ADDRESS                IPAddress(192, 168, 4, 1)
#define AP_GATEWAY                   IPAddress(192, 168, 4, 1)
#define AP_SUBNET                    IPAddress(255, 255, 255, 0)
#define WEB_SINGLE_CLIENT_TIMEOUT_MS 60000
#define WEB_SD_BUSY_STALE_MS         30000u


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


// Product version displayed on the device main screen.
// Hardware version identifies the recorder hardware configuration.
// Software version identifies the firmware build.
#define RECORDER_HARDWARE_VERSION      "1.00"
#define RECORDER_SOFTWARE_VERSION      "1.17"
#define RECORDER_VERSION_TEXT          "sw ver " RECORDER_SOFTWARE_VERSION "\nhw ver " RECORDER_HARDWARE_VERSION

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
