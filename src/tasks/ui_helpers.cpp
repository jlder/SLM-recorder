// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/tasks/ui_helpers.cpp
 * @brief UI helper lookup functions for roller options and roller metadata.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

/*******************************************************************************
 * UI HELPERS v2 - UNIFIED IMPLEMENTATION
 ******************************************************************************/

#include "ui_helpers.h"
#include <Arduino.h>

// =============================================================================
// ROLLER TYPE OPTIONS (Same as before)
// =============================================================================

/**
 * Returns the requested get Roller Options information from the module state
 * or underlying driver interface.
 *
 * Inputs: `type`, `custom_options`.
 * Returns: Pointer to the requested object or string; may be `nullptr` when unavailable.
 */
const char* getRollerOptions(RollerType type, const char* custom_options) {
    static char year_opts[512] = {0};
    static char month_opts[64] = {0};
    static char day_opts[256] = {0};
    static char hour_opts[256] = {0};
    static char minute_opts[512] = {0};
    static char second_opts[512] = {0};
    static char reg_char_opts[256] = {0};
    static char pwd_char_opts[512] = {0};
    static char alphanum_opts[1024] = {0};
    static bool initialized = false;
    
    if (!initialized) {
        // ROLLER_YEAR: 00-99 (maps to years 2000-2099 in UI save logic)
        year_opts[0] = '\0';
        for (int y = 0; y <= 99; y++) {
            char buf[8];
            sprintf(buf, "%02d%s", y, (y < 99) ? "\n" : "");
            strcat(year_opts, buf);
        }
        
        strcpy(month_opts, "01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12");
        
        day_opts[0] = '\0';
        for (int d = 1; d <= 31; d++) {
            char buf[8];
            sprintf(buf, "%02d%s", d, (d < 31) ? "\n" : "");
            strcat(day_opts, buf);
        }
        
        hour_opts[0] = '\0';
        for (int h = 0; h <= 23; h++) {
            char buf[8];
            sprintf(buf, "%02d%s", h, (h < 23) ? "\n" : "");
            strcat(hour_opts, buf);
        }
        
        minute_opts[0] = '\0';
        for (int m = 0; m <= 59; m++) {
            char buf[8];
            sprintf(buf, "%02d%s", m, (m < 59) ? "\n" : "");
            strcat(minute_opts, buf);
        }
        
        strcpy(second_opts, minute_opts);
        
        strcpy(reg_char_opts, "_\n0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n"
                              "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\n"
                              "N\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ");
        
        strcpy(pwd_char_opts, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n"
                              "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nm\n"
                              "n\no\np\nq\nr\ns\nt\nu\nv\nw\nx\ny\nz\n"
                              "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\n"
                              "N\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ");
        
        strcpy(alphanum_opts, "_\n0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n"
                              "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\n"
                              "N\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ\n"
                              "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nm\n"
                              "n\no\np\nq\nr\ns\nt\nu\nv\nw\nx\ny\nz");
        
        initialized = true;
    }
    
    switch (type) {
        case ROLLER_NUM:
            return "0\n1\n2\n3\n4\n5\n6\n7\n8\n9";
        case ROLLER_YEAR:
            return year_opts;
        case ROLLER_MONTH:
            return month_opts;
        case ROLLER_DAY:
            return day_opts;
        case ROLLER_HOUR:
            return hour_opts;
        case ROLLER_MINUTE:
            return minute_opts;
        case ROLLER_SECOND:
            return second_opts;
        case ROLLER_ALPHA_UPPER:
            return "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\n"
                   "N\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ";
        case ROLLER_ALPHA_LOWER:
            return "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nm\n"
                   "n\no\np\nq\nr\ns\nt\nu\nv\nw\nx\ny\nz";
        case ROLLER_ALPHA_MIXED:
            return "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\n"
                   "N\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ\n"
                   "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nm\n"
                   "n\no\np\nq\nr\ns\nt\nu\nv\nw\nx\ny\nz";
        case ROLLER_REG_CHAR:
            return reg_char_opts;
        case ROLLER_PWD_CHAR:
            return pwd_char_opts;
        case ROLLER_ALPHANUM:
            return alphanum_opts;
        case ROLLER_CUSTOM:
            return custom_options ? custom_options : "";
        default:
            return "ERROR";
    }
}

/**
 * Returns the requested get Roller Type Spec information from the module state
 * or underlying driver interface.
 *
 * Inputs: `type`.
 * Returns: Roller metadata for the requested type.
 */
RollerTypeSpec getRollerTypeSpec(RollerType type) {
    switch (type) {
        case ROLLER_NUM:
            return {LV_ROLLER_MODE_INFINITE, 60, 150, "Single digit (0-9)"};
        case ROLLER_YEAR:
            return {LV_ROLLER_MODE_NORMAL, 90, 180, "Year (00-99)"};
        case ROLLER_MONTH:
            return {LV_ROLLER_MODE_INFINITE, 120, 180, "Month (01-12)"};
        case ROLLER_DAY:
            return {LV_ROLLER_MODE_INFINITE, 120, 180, "Day (01-31)"};
        case ROLLER_HOUR:
            return {LV_ROLLER_MODE_INFINITE, 140, 220, "Hour (00-23)"};
        case ROLLER_MINUTE:
            return {LV_ROLLER_MODE_INFINITE, 140, 220, "Minute (00-59)"};
        case ROLLER_SECOND:
            return {LV_ROLLER_MODE_INFINITE, 100, 200, "Second (00-59)"};
        case ROLLER_ALPHA_UPPER:
            return {LV_ROLLER_MODE_INFINITE, 70, 180, "Uppercase A-Z"};
        case ROLLER_ALPHA_LOWER:
            return {LV_ROLLER_MODE_INFINITE, 70, 180, "Lowercase a-z"};
        case ROLLER_ALPHA_MIXED:
            return {LV_ROLLER_MODE_INFINITE, 70, 180, "Mixed case"};
        case ROLLER_REG_CHAR:
            return {LV_ROLLER_MODE_INFINITE, 72, 200, "Registration"};
        case ROLLER_PWD_CHAR:
            return {LV_ROLLER_MODE_INFINITE, 85, 140, "Password"};
        case ROLLER_ALPHANUM:
            return {LV_ROLLER_MODE_INFINITE, 85, 180, "Alphanumeric"};
        case ROLLER_CUSTOM:
        default:
            return {LV_ROLLER_MODE_INFINITE, 100, 180, "Custom"};
    }
}

// =============================================================================
// UNIFIED BUTTON HELPER
// =============================================================================

/**
 * Creates and configures the Button UI objects, labels, callbacks, and initial
 * styling.
 *
 * Inputs: `parent`, `text`, `text_style`, `text_font`, `width`, `height`, `alignment`, `x_offset`, `y_offset`, `callback`, `bg_color`, `enabled`.
 * Returns: Requested value.
 */
lv_obj_t* createButton(
    lv_obj_t* parent,
    const char* text,
    lv_style_t* text_style,
    const lv_font_t* text_font,
    int16_t width,
    int16_t height,
    lv_align_t alignment,
    int16_t x_offset,
    int16_t y_offset,
    lv_event_cb_t callback,
    lv_color_t bg_color,
    bool enabled
) {
    // Create button
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, width, height);
    lv_obj_align(btn, alignment, x_offset, y_offset);
    
    // Set background color if not default blue
    lv_color_t default_blue = lv_palette_main(LV_PALETTE_BLUE);
    if (bg_color.blue != default_blue.blue ||
        bg_color.green != default_blue.green ||
        bg_color.red != default_blue.red) {
        lv_obj_set_style_bg_color(btn, bg_color, 0);
    }
    
    // Set enabled state
    if (!enabled) {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREY), 0);
    }
    
    // Create label
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    
    // Apply style or font
    if (text_style != nullptr) {
        lv_obj_add_style(lbl, text_style, 0);
    } else if (text_font != nullptr) {
        lv_obj_set_style_text_font(lbl, text_font, 0);
    }
    
    lv_obj_center(lbl);
    
    // Attach callback
    if (callback != nullptr) {
        lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, NULL);
    }
    
    return btn;
}

// =============================================================================
// UNIFIED LABEL HELPER
// =============================================================================

/**
 * Creates and configures the Label UI objects, labels, callbacks, and initial
 * styling.
 *
 * Inputs: `parent`, `text`, `text_style`, `text_font`, `alignment`, `x_offset`, `y_offset`, `width`, `text_align`, `letter_spacing`.
 * Returns: Requested value.
 */
lv_obj_t* createLabel(
    lv_obj_t* parent,
    const char* text,
    lv_style_t* text_style,
    const lv_font_t* text_font,
    lv_align_t alignment,
    int16_t x_offset,
    int16_t y_offset,
    int16_t width,
    lv_text_align_t text_align,
    int16_t letter_spacing
) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    
    // Apply style or font
    if (text_style != nullptr) {
        lv_obj_add_style(lbl, text_style, 0);
    } else if (text_font != nullptr) {
        lv_obj_set_style_text_font(lbl, text_font, 0);
    }
    
    // Set width if specified
    if (width > 0) {
        lv_obj_set_width(lbl, width);
    }
    
    // Set text alignment
    if (text_align != LV_TEXT_ALIGN_LEFT) {
        lv_obj_set_style_text_align(lbl, text_align, 0);
    }
    
    // Set letter spacing if specified
    if (letter_spacing > 0) {
        lv_obj_set_style_text_letter_space(lbl, letter_spacing, 0);
    }
    
    lv_obj_align(lbl, alignment, x_offset, y_offset);
    
    return lbl;
}

// =============================================================================
// ROLLER HELPERS (Same as before)
// =============================================================================

/**
 * Creates and configures the Typed Roller UI objects, labels, callbacks, and
 * initial styling.
 *
 * Inputs: `parent`, `type`, `style`, `alignment`, `x_offset`, `y_offset`, `initial_selection`.
 * Returns: Requested value.
 */
lv_obj_t* createTypedRoller(
    lv_obj_t* parent,
    RollerType type,
    lv_style_t* style,
    lv_align_t alignment,
    int16_t x_offset,
    int16_t y_offset,
    uint16_t initial_selection
) {
    RollerTypeSpec spec = getRollerTypeSpec(type);
    const char* options = getRollerOptions(type, nullptr);
    
    lv_obj_t *roller = lv_roller_create(parent);
    lv_roller_set_options(roller, options, spec.default_mode);
    lv_obj_add_style(roller, style, 0);
    lv_obj_set_size(roller, spec.recommended_width, spec.recommended_height);
    lv_obj_align(roller, alignment, x_offset, y_offset);
    
    if (initial_selection > 0) {
        lv_roller_set_selected(roller, initial_selection, LV_ANIM_OFF);
    }
    
    return roller;
}

/**
 * Creates and configures the Typed Roller UI objects, labels, callbacks, and
 * initial styling.
 *
 * Inputs: `parent`, `type`, `style`, `width`, `height`, `alignment`, `x_offset`, `y_offset`, `initial_selection`.
 * Returns: Requested value.
 */
lv_obj_t* createTypedRoller(
    lv_obj_t* parent,
    RollerType type,
    lv_style_t* style,
    int16_t width,
    int16_t height,
    lv_align_t alignment,
    int16_t x_offset,
    int16_t y_offset,
    uint16_t initial_selection
) {
    RollerTypeSpec spec = getRollerTypeSpec(type);
    const char* options = getRollerOptions(type, nullptr);
    
    lv_obj_t *roller = lv_roller_create(parent);
    lv_roller_set_options(roller, options, spec.default_mode);
    lv_obj_add_style(roller, style, 0);
    lv_obj_set_size(roller, width, height);
    lv_obj_align(roller, alignment, x_offset, y_offset);
    
    if (initial_selection > 0) {
        lv_roller_set_selected(roller, initial_selection, LV_ANIM_OFF);
    }
    
    return roller;
}

/**
 * Creates and configures the Typed Roller Array UI objects, labels, callbacks,
 * and initial styling.
 *
 * Inputs: `parent`, `roller_array[]`, `count`, `type`, `style`, `spacing`, `start_x`, `y_position`, `pref_key_prefix`, `row_break`.
 * Returns: None.
 */
void createTypedRollerArray(
    lv_obj_t* parent,
    lv_obj_t* roller_array[],
    int count,
    RollerType type,
    lv_style_t* style,
    int16_t spacing,
    int16_t start_x,
    int16_t y_position,
    const char* pref_key_prefix,
    int row_break
) {
    RollerTypeSpec spec = getRollerTypeSpec(type);
    const char* options = getRollerOptions(type, nullptr);
    
    for(int i = 0; i < count; i++) {
        roller_array[i] = lv_roller_create(parent);
        lv_roller_set_options(roller_array[i], options, spec.default_mode);
        lv_obj_add_style(roller_array[i], style, 0);
        lv_obj_set_size(roller_array[i], spec.recommended_width, spec.recommended_height);
        
        int16_t x_pos, y_pos;
        if (row_break > 0 && i >= row_break) {
            int row_index = i - row_break;
            x_pos = start_x + (row_index * spacing);
            y_pos = y_position + 150;
        } else {
            x_pos = start_x + (i * spacing);
            y_pos = y_position;
        }
        lv_obj_align(roller_array[i], LV_ALIGN_TOP_MID, x_pos, y_pos);
        
        if (pref_key_prefix != nullptr) {
            char key[16];
            sprintf(key, "%s%d", pref_key_prefix, i);
            uint16_t saved_value = prefs.getInt(key, 0);
            lv_roller_set_selected(roller_array[i], saved_value, LV_ANIM_OFF);
        }
    }
}

/**
 * Creates and configures the Typed Roller Array UI objects, labels, callbacks,
 * and initial styling.
 *
 * Inputs: `parent`, `roller_array[]`, `count`, `type`, `style`, `width`, `height`, `spacing`, `start_x`, `y_position`, `pref_key_prefix`, `row_break`.
 * Returns: None.
 */
void createTypedRollerArray(
    lv_obj_t* parent,
    lv_obj_t* roller_array[],
    int count,
    RollerType type,
    lv_style_t* style,
    int16_t width,
    int16_t height,
    int16_t spacing,
    int16_t start_x,
    int16_t y_position,
    const char* pref_key_prefix,
    int row_break
) {
    RollerTypeSpec spec = getRollerTypeSpec(type);
    const char* options = getRollerOptions(type, nullptr);
    
    for(int i = 0; i < count; i++) {
        roller_array[i] = lv_roller_create(parent);
        lv_roller_set_options(roller_array[i], options, spec.default_mode);
        lv_obj_add_style(roller_array[i], style, 0);
        lv_obj_set_size(roller_array[i], width, height);
        
        int16_t x_pos, y_pos;
        if (row_break > 0 && i >= row_break) {
            int row_index = i - row_break;
            x_pos = start_x + (row_index * spacing);
            y_pos = y_position + 150;
        } else {
            x_pos = start_x + (i * spacing);
            y_pos = y_position;
        }
        lv_obj_align(roller_array[i], LV_ALIGN_TOP_MID, x_pos, y_pos);
        
        if (pref_key_prefix != nullptr) {
            char key[16];
            sprintf(key, "%s%d", pref_key_prefix, i);
            uint16_t saved_value = prefs.getInt(key, 0);
            lv_roller_set_selected(roller_array[i], saved_value, LV_ANIM_OFF);
        }
    }
}

// =============================================================================
// GENERIC ROLLER GRID BUILDER (maximize roller size)
// =============================================================================

/**
 * Handles ui calc roller grid for the local UI, display, or user interaction
 * path.
 *
 * Inputs: `parent`, `roller_count`, `top_y`, `bottom_reserved`, `side_margin`, `col_gap`, `row_gap`.
 * Returns: Requested value.
 */
ui_roller_grid_layout_t ui_calc_roller_grid(
    lv_obj_t* parent,
    int roller_count,
    int16_t top_y,
    int16_t bottom_reserved,
    int16_t side_margin,
    int16_t col_gap,
    int16_t row_gap
) {
    ui_roller_grid_layout_t out = {};
    out.top_y = top_y;
    out.col_gap = col_gap;
    out.row_gap = row_gap;

    if (roller_count <= 0) {
        out.rows = 0;
        out.cols = 0;
        return out;
    }

    // Policy: 1 row up to 5 rollers, 2 rows up to 10 rollers.
    if (roller_count <= 5) {
        out.rows = 1;
        out.cols = roller_count;
    } else {
        out.rows = 2;
        out.cols = (roller_count + 1) / 2; // ceil
    }

    const int16_t w = (int16_t)lv_obj_get_width(parent);
    const int16_t h = (int16_t)lv_obj_get_height(parent);

    const int16_t avail_w = (int16_t)(w - (2 * side_margin) - (out.cols > 1 ? (out.cols - 1) * col_gap : 0));
    int16_t avail_h = (int16_t)(h - top_y - bottom_reserved);
    if (avail_h < 60) avail_h = 60; // sane minimum

    out.roller_w = (int16_t)(avail_w / out.cols);
    if (out.roller_w < 30) out.roller_w = 30;

    if (out.rows == 1) {
        out.roller_h = avail_h;
    } else {
        int16_t h2 = (int16_t)((avail_h - row_gap) / 2);
        if (h2 < 60) h2 = 60;
        out.roller_h = h2;
    }

    // Left-align the grid; maximize roller width by minimizing margins/gaps.
    out.start_x = side_margin;
    return out;
}

/**
 * Handles ui create typed roller grid for the local UI, display, or user
 * interaction path.
 *
 * Inputs: `parent`, `rollers_out[]`, `roller_count`, `types[]`, `style`, `layout`, `initial_sel`.
 * Returns: None.
 */
void ui_create_typed_roller_grid(
    lv_obj_t* parent,
    lv_obj_t* rollers_out[],
    int roller_count,
    const RollerType types[],
    lv_style_t* style,
    const ui_roller_grid_layout_t* layout,
    const uint16_t* initial_sel
) {
    if (!parent || !rollers_out || !types || !layout) return;
    if (roller_count <= 0) return;

    for (int i = 0; i < roller_count; i++) {
        const int row = (layout->rows > 1) ? (i / layout->cols) : 0;
        const int col = (layout->rows > 1) ? (i % layout->cols) : i;

        const int16_t x = (int16_t)(layout->start_x + col * (layout->roller_w + layout->col_gap));
        const int16_t y = (int16_t)(layout->top_y + row * (layout->roller_h + layout->row_gap));
        const uint16_t sel = (initial_sel != nullptr) ? initial_sel[i] : 0;

        rollers_out[i] = createTypedRoller(parent, types[i], style,
                                          layout->roller_w, layout->roller_h,
                                          LV_ALIGN_TOP_LEFT, x, y, sel);
    }
}

// =============================================================================
// BATTERY GRAPHIC HELPER
// =============================================================================

/**
 * Creates and configures the Battery Graphic UI objects, labels, callbacks,
 * and initial styling.
 *
 * Inputs: `parent`, `x_offset`, `y_offset`, `battery`.
 * Returns: None.
 */
void createBatteryGraphic(
    lv_obj_t* parent,
    int16_t x_offset,
    int16_t y_offset,
    BatteryGraphic* battery
) {
    // Initialize structure
    battery->initialized = true;
    
    // Create container for battery graphic (30×80 pixels)
    battery->container = lv_obj_create(parent);
    lv_obj_set_size(battery->container, 30, 80);
    lv_obj_align(battery->container, LV_ALIGN_BOTTOM_LEFT, x_offset, y_offset);
    lv_obj_set_style_bg_color(battery->container, lv_color_black(), 0);
    lv_obj_set_style_border_color(battery->container, lv_color_white(), 0);
    lv_obj_set_style_border_width(battery->container, 2, 0);
    lv_obj_set_style_pad_all(battery->container, 2, 0);
    lv_obj_clear_flag(battery->container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create battery terminal (small cap on top)
    lv_obj_t *terminal = lv_obj_create(battery->container);
    lv_obj_set_size(terminal, 12, 4);
    lv_obj_align(terminal, LV_ALIGN_TOP_MID, 0, -6);
    lv_obj_set_style_bg_color(terminal, lv_color_white(), 0);
    lv_obj_set_style_border_width(terminal, 0, 0);
    
    // Create 4 battery bars (from top to bottom)
    // NOTE: bar fill logic is handled in updateBatteryGraphic().
    for (int i = 0; i < 4; i++) {
        battery->bars[i] = lv_obj_create(battery->container);
        // Match prototype sizing
        lv_obj_set_size(battery->bars[i], 22, 15);
        lv_obj_align(battery->bars[i], LV_ALIGN_TOP_MID, 0, 2 + (i * 18));
        lv_obj_set_style_border_width(battery->bars[i], 0, 0);
        lv_obj_set_style_bg_opa(battery->bars[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(battery->bars[i], lv_color_black(), 0);
    }

// Create percentage label below battery
    battery->lbl_percent = lv_label_create(parent);
    lv_obj_set_style_text_font(battery->lbl_percent, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(battery->lbl_percent, lv_color_black(), 0);
    lv_obj_align_to(battery->lbl_percent, battery->container, 
                    LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
	lv_obj_set_width(battery->lbl_percent, 60);
    lv_label_set_text(battery->lbl_percent, "0%");
    
    // Create charging indicator (lightning symbol) above battery - initially hidden
    battery->lbl_charging = lv_label_create(parent);
    lv_obj_set_style_text_font(battery->lbl_charging, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(battery->lbl_charging, 
                                 lv_palette_main(LV_PALETTE_PURPLE), 0);
    lv_obj_align_to(battery->lbl_charging, battery->container,
                    LV_ALIGN_OUT_TOP_MID, 18, -8);
    lv_label_set_text(battery->lbl_charging, LV_SYMBOL_CHARGE);  // ⚡ symbol
	lv_obj_set_width(battery->lbl_charging, 30);
    lv_obj_add_flag(battery->lbl_charging, LV_OBJ_FLAG_HIDDEN);  // Hide initially
}

/**
 * Update battery graphic performs the ui helpers operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `battery`, `battery_percent`, `is_charging`.
 * Returns: None.
 */
void updateBatteryGraphic(
    BatteryGraphic* battery,
    int battery_percent,
    bool is_charging
) {
    if (!battery->initialized) return;

    // If battery percent is negative, treat as invalid reading.
    if (battery_percent < 0) {
        lv_label_set_text(battery->lbl_percent, "--%");
        // Hide charging indicator when data invalid.
        lv_obj_add_flag(battery->lbl_charging, LV_OBJ_FLAG_HIDDEN);
        // Turn off all bars (leave outline visible).
        for (int i = 0; i < 4; i++) {
            lv_obj_set_style_bg_opa(battery->bars[i], LV_OPA_20, 0);
        }
        return;
    }
    
    // Clamp percentage to valid range
    if (battery_percent < 0) battery_percent = 0;
    if (battery_percent > 100) battery_percent = 100;
    
    // Update percentage text
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", battery_percent);
    lv_label_set_text(battery->lbl_percent, buf);
    
    // Show/hide charging indicator
    if (is_charging) {
        lv_obj_clear_flag(battery->lbl_charging, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(battery->lbl_charging, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Determine color and bars to light based on battery percentage
    lv_color_t bar_color;
    int bars_to_light;
    
    if (battery_percent >= 76) {
        // 76-100%: All 4 bars green
        bar_color = lv_palette_main(LV_PALETTE_GREEN);
        bars_to_light = 4;
    } else if (battery_percent >= 51) {
        // 51-75%: 3 bars yellow
        bar_color = lv_palette_main(LV_PALETTE_YELLOW);
        bars_to_light = 3;
    } else if (battery_percent >= 26) {
        // 26-50%: 2 bars orange
        bar_color = lv_palette_main(LV_PALETTE_ORANGE);
        bars_to_light = 2;
    } else {
        // 0-25%: 1 bar red
        bar_color = lv_palette_main(LV_PALETTE_RED);
        bars_to_light = 1;
    }
    
    // Keep the charging symbol purple, but do not change the battery bar color while charging.
    lv_obj_set_style_text_color(battery->lbl_charging, lv_palette_main(LV_PALETTE_PURPLE), 0);
    
    // Update each bar. The graphic stores bars top->bottom as index 0..3,
    // but we want to "fill" from the bottom up.
    for (int i = 0; i < 4; i++) {
        int idx = 3 - i; // start from bottom (3) up to top (0)
        if (i < bars_to_light) {
            lv_obj_set_style_bg_opa(battery->bars[idx], LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(battery->bars[idx], bar_color, 0);
        } else {
            lv_obj_set_style_bg_opa(battery->bars[idx], LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(battery->bars[idx], lv_color_black(), 0);
        }
    }
}

// =============================================================================
// STATUS MESSAGE MANAGER
// =============================================================================

// Global instance
StatusManager statusManager;

StatusManager::StatusManager() 
    : lbl_status(nullptr), 
      current_priority(STATUS_PRIO_NONE),
      current_blink(false), 
      expiry_time(0),
      show_immediately(false) {
    current_message[0] = '\0';
    current_color = lv_color_white();
}

void StatusManager::setLabel(lv_obj_t* label) {
    lbl_status = label;
}

void StatusManager::setStatus(StatusPriority priority, const char* msg,
                               lv_color_t color, bool blink, uint32_t duration_ms) {
    if (!lbl_status) return;  // Label not set
    
    // Check if this message should override current
    bool should_update = false;
    
    if (expiry_time > 0 && millis() >= expiry_time) {
        // Current message expired, accept any priority
        should_update = true;
        current_priority = STATUS_PRIO_NONE;
    } else if (priority >= current_priority) {
        // Higher or equal priority
        should_update = true;
    }
    
    if (should_update) {
        current_priority = priority;
        strncpy(current_message, msg, sizeof(current_message) - 1);
        current_message[sizeof(current_message) - 1] = '\0';  // Ensure null termination
        current_color = color;
        current_blink = blink;
        show_immediately = true;  // NEW: Ensure message shows on next update
        
        if (duration_ms > 0) {
            expiry_time = millis() + duration_ms;
        } else {
            expiry_time = 0;  // Permanent until cleared
        }
        
        // Update display immediately
        lv_obj_set_style_text_color(lbl_status, color, 0);
        lv_label_set_text(lbl_status, msg);
    }
}

void StatusManager::update(bool blink_state) {
    if (!lbl_status) return;
    
    // Check expiry
    if (expiry_time > 0 && millis() >= expiry_time) {
        // Message expired - clear it
        current_priority = STATUS_PRIO_NONE;
        current_message[0] = '\0';
        lv_label_set_text(lbl_status, "");
        expiry_time = 0;
        show_immediately = false;
        return;
    }
    
    // Handle display based on blink state
    if (current_blink) {
        // Blinking message
        if (show_immediately) {
            // First update after setStatus - always show the message
            lv_label_set_text(lbl_status, current_message);
            show_immediately = false;
        } else {
            // Normal blink cycle - toggle between message and empty
            lv_label_set_text(lbl_status, blink_state ? current_message : "");
        }
    } else {
        // Non-blinking message - always show
        lv_label_set_text(lbl_status, current_message);
        show_immediately = false;
    }
}

void StatusManager::clear(StatusPriority priority) {
    // Only clear if priority matches current or is higher
    if (priority >= current_priority) {
        current_priority = STATUS_PRIO_NONE;
        current_message[0] = '\0';
        expiry_time = 0;
        show_immediately = false;
        if (lbl_status) {
            lv_label_set_text(lbl_status, "");
        }
    }
}

void StatusManager::forceClear() {
    current_priority = STATUS_PRIO_NONE;
    current_message[0] = '\0';
    expiry_time = 0;
    show_immediately = false;
    if (lbl_status) {
        lv_label_set_text(lbl_status, "");
    }
}
