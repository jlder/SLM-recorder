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
#define DISPLAY_BRIGHTNESS_DIMMED      60u
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
#define CALIBRATION_SENSOR_STORAGE_VERSION  1u
#define CALIBRATION_INSTALL_STORAGE_VERSION 1u
#define CALIBRATION_GRAVITY_MG             1000.0f
#define CALIBRATION_VALIDITY_MONTHS        12u
#define CALIBRATION_FACE_GRAVITY_TOL_PCT   10.0f
#define INSTALLATION_GRAVITY_TOL_PCT        10.0f
#define CALIBRATION_SAMPLE_PERIOD_MS       50u
// Number of samples in the calibration stability window.
// Requirement: must be greater than 0 because mean/stddev computation divides by this value.
#define CALIBRATION_WINDOW_SAMPLE_COUNT    40u
#define CALIBRATION_STABILITY_STDDEV_MAX_MG 1.5f
#define CALIBRATION_GAIN_MIN               0.8f
#define CALIBRATION_GAIN_MAX               1.2f
#define CALIBRATION_OFFSET_ABS_MAX_MG      200.0f


// Product version displayed on the device main screen.
// Hardware version identifies the recorder hardware/prototype revision.
// Software version is incremented during prototype development and before release.
#define RECORDER_HARDWARE_VERSION      "1.00"
#define RECORDER_SOFTWARE_VERSION      "1.10"
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
