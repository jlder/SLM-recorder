// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/audio_tone_helpers.h
 * @brief Small deterministic PCM-generation helpers for alert tones.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/** Fill an interleaved stereo buffer with a square-wave tone. */
void audio_tone_fill_square(int16_t *frames,
                            size_t frame_count,
                            uint32_t sample_rate_hz,
                            uint32_t tone_hz,
                            int16_t amplitude,
                            uint32_t *phase);

/** Fill an interleaved stereo buffer with silence. */
void audio_tone_fill_silence(int16_t *frames, size_t frame_count);
