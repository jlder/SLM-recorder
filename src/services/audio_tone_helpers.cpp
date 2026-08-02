// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/audio_tone_helpers.cpp
 * @brief Deterministic PCM-generation helpers for recorder alert tones.
 */

#include "src/services/audio_tone_helpers.h"

#include <string.h>

void audio_tone_fill_square(int16_t *frames,
                            size_t frame_count,
                            uint32_t sample_rate_hz,
                            uint32_t tone_hz,
                            int16_t amplitude,
                            uint32_t *phase){
  if((frames == nullptr) || (phase == nullptr) ||
     (sample_rate_hz == 0u) || (tone_hz == 0u)){
    return;
  }

  const uint32_t period = sample_rate_hz / tone_hz;
  const uint32_t safe_period = (period < 2u) ? 2u : period;
  const uint32_t half_period = safe_period / 2u;

  for(size_t i = 0u; i < frame_count; i++){
    const int16_t sample = ((*phase % safe_period) < half_period) ? amplitude : (int16_t)-amplitude;
    frames[(i * 2u) + 0u] = sample;
    frames[(i * 2u) + 1u] = sample;
    (*phase)++;
  }
}

void audio_tone_fill_silence(int16_t *frames, size_t frame_count){
  if(frames == nullptr){
    return;
  }
  memset(frames, 0, frame_count * 2u * sizeof(int16_t));
}
