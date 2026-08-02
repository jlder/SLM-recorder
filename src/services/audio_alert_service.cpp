// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/audio_alert_service.cpp
 * @brief Repeating three-beep error alert isolated from recorder-critical tasks.
 */

#include "src/services/audio_alert_service.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"
#include "src/global.h"
#include "src/drivers/audio_driver.h"
#include "src/services/audio_tone_helpers.h"
#include "src/services/task_helpers.h"

namespace {

static TaskHandle_t s_task = nullptr;
static portMUX_TYPE s_alert_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_active = false;
static bool s_acknowledged = false;
static uint32_t s_error_key = 0u;

static const uint32_t AUDIO_SAMPLE_RATE_HZ = 16000u;
static const size_t AUDIO_CHUNK_FRAMES = 160u; // 10 ms at 16 kHz
static int16_t s_pcm[AUDIO_CHUNK_FRAMES * 2u];

struct alert_snapshot_t {
  bool active;
  bool acknowledged;
  uint32_t error_key;
};

static alert_snapshot_t snapshot_(void){
  alert_snapshot_t out;
  portENTER_CRITICAL(&s_alert_mux);
  out.active = s_active;
  out.acknowledged = s_acknowledged;
  out.error_key = s_error_key;
  portEXIT_CRITICAL(&s_alert_mux);
  return out;
}

static bool alert_should_play_(uint32_t error_key){
  const alert_snapshot_t current = snapshot_();
  return current.active && !current.acknowledged && (current.error_key == error_key);
}

static bool write_duration_(bool tone, uint32_t duration_ms, uint32_t error_key, uint32_t *phase){
  uint32_t remaining_ms = duration_ms;

  while(remaining_ms > 0u){
    if(!alert_should_play_(error_key)){
      return false;
    }

    if(tone){
      audio_tone_fill_square(s_pcm,
                             AUDIO_CHUNK_FRAMES,
                             AUDIO_SAMPLE_RATE_HZ,
                             AUDIO_ALERT_TONE_HZ,
                             AUDIO_ALERT_AMPLITUDE,
                             phase);
    } else {
      audio_tone_fill_silence(s_pcm, AUDIO_CHUNK_FRAMES);
    }

    if(!audio_driver_write_frames(s_pcm, AUDIO_CHUNK_FRAMES)){
      return false;
    }

    // Pace PCM delivery at the actual playback rate. Without this delay,
    // the I2S DMA queue can accumulate several tone/silence segments and
    // the audible pattern no longer matches the requested timing.
    vTaskDelay(pdMS_TO_TICKS(10u));
    remaining_ms = (remaining_ms > 10u) ? (remaining_ms - 10u) : 0u;
  }

  return true;
}

static void wait_interruptible_(uint32_t wait_ms, uint32_t error_key){
  uint32_t elapsed_ms = 0u;
  while((elapsed_ms < wait_ms) && alert_should_play_(error_key)){
    vTaskDelay(pdMS_TO_TICKS(50u));
    elapsed_ms += 50u;
  }
}

static void audio_alert_task_(void *arg){
  (void)arg;

  for(;;){
    const alert_snapshot_t current = snapshot_();
    if(!current.active || current.acknowledged){
      (void)audio_driver_set_enabled(false);
      vTaskDelay(pdMS_TO_TICKS(100u));
      continue;
    }

    if(!audio_driver_set_enabled(true)){
      // Audio is auxiliary. Fail silently and avoid repeated initialization
      // attempts or any impact on recorder-critical processing.
      audio_alert_service_acknowledge();
      continue;
    }

    const uint32_t cycle_start_ms = millis();
    uint32_t phase = 0u;

    // Prime the amplifier, codec playback path, and I2S DMA with silence.
    // This is an intentional hardware warm-up, not part of the audible pattern.
    // Without it, the first tone can sound shorter or different while the
    // playback path is still settling.
    bool complete = write_duration_(false,
                                    AUDIO_ALERT_PREROLL_SILENCE_MS,
                                    current.error_key,
                                    &phase);

    for(uint32_t beep = 0u; complete && (beep < AUDIO_ALERT_BEEP_COUNT); beep++){
      if(!write_duration_(true, AUDIO_ALERT_BEEP_MS, current.error_key, &phase)){
        complete = false;
        break;
      }

      if((beep + 1u) < AUDIO_ALERT_BEEP_COUNT){
        if(!write_duration_(false, AUDIO_ALERT_GAP_MS, current.error_key, &phase)){
          complete = false;
          break;
        }
      }
    }

    if(complete){
      complete = write_duration_(false,
                                 AUDIO_ALERT_TRAILING_SILENCE_MS,
                                 current.error_key,
                                 &phase);
    }

    (void)audio_driver_set_enabled(false);

    if(!complete){
      continue;
    }

    const uint32_t elapsed_ms = millis() - cycle_start_ms;
    if(elapsed_ms < AUDIO_ALERT_REPEAT_MS){
      wait_interruptible_(AUDIO_ALERT_REPEAT_MS - elapsed_ms, current.error_key);
    }
  }
}

} // namespace

void audio_alert_service_init(void){
  if(s_task != nullptr){
    return;
  }

  const BaseType_t ok = xTaskCreatePinnedToCore(audio_alert_task_,
                                                "audio_alert",
                                                CFG_AUDIO_TASK_STACK_WORDS,
                                                nullptr,
                                                CFG_AUDIO_TASK_PRIO,
                                                &s_task,
                                                CFG_AUDIO_TASK_CORE);
  if(ok != pdPASS){
    s_task = nullptr;
    // Auxiliary feature creation failure shall not reboot the recorder.
  }
}

void audio_alert_service_set_error(bool active, uint32_t error_key){
  portENTER_CRITICAL(&s_alert_mux);

  if(!active){
    s_active = false;
    s_acknowledged = false;
    s_error_key = 0u;
  } else {
    if(!s_active || (s_error_key != error_key)){
      s_acknowledged = false;
    }
    s_active = true;
    s_error_key = error_key;
  }

  portEXIT_CRITICAL(&s_alert_mux);
}

void audio_alert_service_acknowledge(void){
  portENTER_CRITICAL(&s_alert_mux);
  if(s_active){
    s_acknowledged = true;
  }
  portEXIT_CRITICAL(&s_alert_mux);
}
