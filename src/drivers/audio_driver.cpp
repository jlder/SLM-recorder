// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/drivers/audio_driver.cpp
 * @brief ES8311 and I2S hardware abstraction used by recorder audio alerts.
 *
 * The board pin assignment and codec configuration follow the Waveshare
 * ESP32-S3-Touch-AMOLED-2.06 ES8311 Arduino example. The implementation is
 * intentionally limited to mono alert playback through stereo I2S frames.
 */

#include "src/drivers/audio_driver.h"

#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>

#include "src/board/pin_config.h"
#include "config.h"

namespace {

static const uint8_t ES8311_I2C_ADDRESS = 0x18u;
static const uint32_t AUDIO_SAMPLE_RATE_HZ = 16000u;

static I2SClass s_i2s;
static bool s_initialized = false;
static bool s_init_attempted = false;
static bool s_enabled = false;

static bool codec_write_(uint8_t reg, uint8_t value){
  Wire.beginTransmission(ES8311_I2C_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return (Wire.endTransmission() == 0u);
}

static bool codec_read_(uint8_t reg, uint8_t *value){
  if(value == nullptr){
    return false;
  }

  Wire.beginTransmission(ES8311_I2C_ADDRESS);
  Wire.write(reg);
  if(Wire.endTransmission(false) != 0u){
    return false;
  }

  if(Wire.requestFrom((int)ES8311_I2C_ADDRESS, 1) != 1){
    return false;
  }

  *value = (uint8_t)Wire.read();
  return true;
}

static bool codec_set_muted_(bool muted){
  uint8_t reg31 = 0u;
  if(!codec_read_(0x31u, &reg31)){
    return false;
  }

  if(muted){
    reg31 |= (uint8_t)((1u << 6) | (1u << 5));
  } else {
    reg31 &= (uint8_t)~((1u << 6) | (1u << 5));
  }
  return codec_write_(0x31u, reg31);
}

static bool codec_init_(void){
  // Reset and power-on sequence.
  if(!codec_write_(0x00u, 0x1Fu)) return false;
  if(!codec_write_(0x00u, 0x00u)) return false;
  if(!codec_write_(0x00u, 0x80u)) return false;

  // 16 kHz, 4.096 MHz MCLK, slave I2S, 16-bit input/output.
  if(!codec_write_(0x01u, 0x3Fu)) return false;
  if(!codec_write_(0x02u, 0x00u)) return false;
  if(!codec_write_(0x03u, 0x10u)) return false;
  if(!codec_write_(0x04u, 0x10u)) return false;
  if(!codec_write_(0x05u, 0x00u)) return false;
  if(!codec_write_(0x06u, 0x03u)) return false;
  if(!codec_write_(0x07u, 0x00u)) return false;
  if(!codec_write_(0x08u, 0xFFu)) return false;
  if(!codec_write_(0x09u, 0x0Cu)) return false;
  if(!codec_write_(0x0Au, 0x0Cu)) return false;

  // Playback path power and equalizer bypass.
  if(!codec_write_(0x0Du, 0x01u)) return false;
  if(!codec_write_(0x0Eu, 0x02u)) return false;
  if(!codec_write_(0x12u, 0x00u)) return false;
  if(!codec_write_(0x13u, 0x10u)) return false;
  if(!codec_write_(0x1Cu, 0x6Au)) return false;
  if(!codec_write_(0x37u, 0x08u)) return false;

  // Moderate alert volume. Register value follows the ES8311 0..100 mapping.
  const uint8_t volume_reg = (uint8_t)(((AUDIO_ALERT_VOLUME_PERCENT * 256u) / 100u) - 1u);
  if(!codec_write_(0x32u, volume_reg)) return false;

  return codec_set_muted_(true);
}

} // namespace

bool audio_driver_init(void){
  if(s_initialized){
    return true;
  }
  if(s_init_attempted){
    return false;
  }
  s_init_attempted = true;

  pinMode(AUDIO_AMP_ENABLE, OUTPUT);
  digitalWrite(AUDIO_AMP_ENABLE, LOW);

  s_i2s.setPins(AUDIO_I2S_MCLK,
                AUDIO_I2S_BCLK,
                AUDIO_I2S_DOUT,
                AUDIO_I2S_WS,
                AUDIO_I2S_DIN);

  if(!s_i2s.begin(I2S_MODE_STD,
                  AUDIO_SAMPLE_RATE_HZ,
                  I2S_DATA_BIT_WIDTH_16BIT,
                  I2S_SLOT_MODE_STEREO,
                  I2S_STD_SLOT_BOTH)){
    return false;
  }

  if(!codec_init_()){
    s_i2s.end();
    return false;
  }

  s_initialized = true;
  return true;
}

bool audio_driver_set_enabled(bool enabled){
  if(enabled){
    if(!audio_driver_init()){
      return false;
    }
    if(s_enabled){
      return true;
    }

    digitalWrite(AUDIO_AMP_ENABLE, HIGH);
    delay(AUDIO_ALERT_AMP_SETTLE_MS);
    if(!codec_set_muted_(false)){
      digitalWrite(AUDIO_AMP_ENABLE, LOW);
      return false;
    }
    s_enabled = true;
    return true;
  }

  if(!s_initialized){
    return true;
  }

  (void)codec_set_muted_(true);
  digitalWrite(AUDIO_AMP_ENABLE, LOW);
  s_enabled = false;
  return true;
}

bool audio_driver_write_frames(const int16_t *frames, size_t frame_count){
  if((frames == nullptr) || (frame_count == 0u) || !s_enabled){
    return false;
  }

  const size_t byte_count = frame_count * 2u * sizeof(int16_t);
  const size_t written = s_i2s.write((const uint8_t *)frames, byte_count);
  return (written == byte_count);
}
