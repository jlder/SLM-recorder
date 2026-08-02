// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

#pragma once

#define XPOWERS_CHIP_AXP2101

#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 11
#define LCD_CS 12
#define LCD_RESET 8
#define LCD_EN 13
#define LCD_WIDTH 410
#define LCD_HEIGHT 502

// TOUCH
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 38
#define TP_RESET 9


// AUDIO - ES8311 codec and speaker amplifier
#define AUDIO_I2S_MCLK 41
#define AUDIO_I2S_BCLK 45
#define AUDIO_I2S_DOUT 40
#define AUDIO_I2S_WS   42
#define AUDIO_I2S_DIN  16
#define AUDIO_AMP_ENABLE 46

// SD
const int SDMMC_CLK = 2;
const int SDMMC_CMD = 1;
const int SDMMC_DATA = 3;
const int SDMMC_CS = 17;