// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/ui/slm_fonts.h
 * @brief Recorder UI fonts with compact French-accent support.
 *
 * The recorder keeps LVGL's built-in Montserrat fonts for the normal ASCII
 * glyphs.  This module adds only the accented French glyphs used by the UI by
 * synthesizing the accent marks over the matching built-in Montserrat base
 * glyph at render time.  No external font file or second full alphabet is
 * stored in firmware.
 */

#pragma once

#include <lvgl.h>

extern lv_font_t slm_font_montserrat_18;
extern lv_font_t slm_font_montserrat_24;
extern lv_font_t slm_font_montserrat_30;
extern lv_font_t slm_font_montserrat_32;
extern lv_font_t slm_font_montserrat_34;
extern lv_font_t slm_font_montserrat_36;
extern lv_font_t slm_font_montserrat_48;

/** Initialize the accent-capable wrappers before creating UI objects/styles. */
void slm_fonts_init(void);
