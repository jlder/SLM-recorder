// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/audio_driver.h
 * @brief Hardware-abstraction interface for the ES8311 speaker output.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/** Initialize the audio hardware on first use. Failure is non-fatal. */
bool audio_driver_init(void);

/** Enable or disable the codec output and speaker amplifier. */
bool audio_driver_set_enabled(bool enabled);

/** Write interleaved stereo 16-bit PCM frames to the I2S output. */
bool audio_driver_write_frames(const int16_t *frames, size_t frame_count);
