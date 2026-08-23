// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/audio_alert_service.h
 * @brief High-level repeating error-alert service.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/** Create the dormant low-priority audio alert task. */
void audio_alert_service_init(void);

/** Enable or disable error alerting and identify the active error instance. */
void audio_alert_service_set_error(bool active, uint32_t error_key);

/** Silence the current error alert until the error changes or clears. */
void audio_alert_service_acknowledge(void);

/** Enable or disable the repeating USB-power-loss alert. */
void audio_alert_service_set_usb_power_loss(bool active);
