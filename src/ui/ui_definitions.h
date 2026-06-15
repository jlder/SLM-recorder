// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/ui/ui_definitions.h
 * @brief UI-only compile-time definitions for layout, fonts, and buffers.
 *
 * @details Keep operator-visible behavior and system limits in config.h.  This
 * file owns UI implementation constants that are used by UI construction and
 * rendering code only.
 */

#pragma once

#include <lvgl.h>

// Display draw-buffer sizing used by ui_task.cpp.
#define UI_DRAW_BUF_LINES     40u
#define UI_DRAW_BUF_MAX_WIDTH 410u

// Standard button sizes.
#define BTN_ACTION_WIDTH      160   // SAVE, BACK, OK, CANCEL
#define BTN_ACTION_HEIGHT     70
#define BTN_MENU_WIDTH        300   // Menu items
#define BTN_MENU_HEIGHT       70
#define BTN_MAIN_WIDTH        200   // Main screen MENU button
#define BTN_MAIN_HEIGHT       80

// Font aliases used by UI helper calls.
#define FONT_HUGE             &lv_font_montserrat_48
#define FONT_LARGE            &lv_font_montserrat_36
#define FONT_MEDIUM           &lv_font_montserrat_32
#define FONT_SMALL            &lv_font_montserrat_18
