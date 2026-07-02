// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/tasks/ui_helpers.h
 * @brief Public UI helper types and lookup API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

/*******************************************************************************
 * UI HELPERS v2 - UNIFIED & SIMPLIFIED
 *
 * Single button helper, single label helper - parameterized for flexibility
 * Type-safe roller creation with constants
 *
 * Version: 1.11
 * Date: 2026-01-24
 ******************************************************************************/

#ifndef UI_HELPERS_H
#define UI_HELPERS_H

#include <lvgl.h>
#include <Preferences.h>
#include "src/ui/ui_definitions.h"

extern Preferences prefs;

// =============================================================================
// ROLLER TYPE ENUMERATION
// =============================================================================

typedef enum {
    // Numeric ranges
    ROLLER_NUM,           // 0-9
    ROLLER_YEAR,          // 00-99 (mapped to years 2000-2099)
    ROLLER_MONTH,         // 01-12
    ROLLER_DAY,           // 01-31
    ROLLER_HOUR,          // 00-23
    ROLLER_MINUTE,        // 00-59
    ROLLER_SECOND,        // 00-59

    // Character sets
    ROLLER_ALPHA_UPPER,   // A-Z
    ROLLER_ALPHA_LOWER,   // a-z
    ROLLER_ALPHA_MIXED,   // A-Z + a-z

    // Application-specific
    ROLLER_REG_CHAR,      // Space + 0-9 + A-Z
    ROLLER_PWD_CHAR,      // 0-9 + a-z + A-Z
    ROLLER_ALPHANUM,      // Space + 0-9 + A-Z + a-z

    ROLLER_CUSTOM
} RollerType;

typedef struct {
    lv_roller_mode_t default_mode;
    uint16_t recommended_width;
    uint16_t recommended_height;
    const char* description;
} RollerTypeSpec;

// =============================================================================
// UNIFIED BUTTON HELPER
// =============================================================================

/**
 * Create a button with label - ONE FUNCTION FOR ALL BUTTONS
 *
 * @param parent Parent screen
 * @param text Button label text
 * @param text_style Text style (or use font directly with NULL style)
 * @param text_font Font (used if text_style is NULL)
 * @param width Button width in pixels
 * @param height Button height in pixels
 * @param alignment Button alignment
 * @param x_offset X position offset
 * @param y_offset Y position offset
 * @param callback Click callback (NULL if set later)
 * @param bg_color Background color
 * @param enabled Initial enabled state
 * @return Button object pointer
 *
 * Examples:
 *   // Action button (160×70)
 *   createButton(screen, "SAVE", NULL, FONT_LARGE, 160, 70,
 *                LV_ALIGN_BOTTOM_LEFT, 30, -30, save_cb);
 *
 *   // Menu button (340×80)
 *   createButton(screen, "SETTINGS", &style_huge, NULL, 340, 80,
 *                LV_ALIGN_TOP_MID, 0, 170, settings_cb);
 *
 *   // Main MENU button (200×80)
 *   createButton(screen, "MENU", &style_huge, NULL, 200, 80,
 *                LV_ALIGN_TOP_MID, 0, 140, menu_cb);
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
    lv_event_cb_t callback = nullptr,
    lv_color_t bg_color = lv_palette_main(LV_PALETTE_BLUE),
    bool enabled = true
);

// =============================================================================
// UNIFIED LABEL HELPER
// =============================================================================

/**
 * Create a label - ONE FUNCTION FOR ALL LABELS
 *
 * @param parent Parent screen
 * @param text Label text
 * @param text_style Text style (or NULL to use font directly)
 * @param text_font Font (used if text_style is NULL)
 * @param alignment Label alignment
 * @param x_offset X position offset
 * @param y_offset Y position offset
 * @param width Label width (0 = auto)
 * @param text_align Text alignment (LV_TEXT_ALIGN_LEFT/CENTER/RIGHT)
 * @return Label object pointer
 *
 * Examples:
 *   // Title (centered, 32pt)
 *   createLabel(screen, "SET DATE", NULL, FONT_MEDIUM,
 *               LV_ALIGN_TOP_MID, 0, 20, 0, LV_TEXT_ALIGN_CENTER);
 *
 *   // Field label (smaller, 24pt)
 *   createLabel(screen, "Year", &style_large, NULL,
 *               LV_ALIGN_TOP_MID, -130, 70, 0, LV_TEXT_ALIGN_CENTER);
 *
 *   // Status (large, 36pt, centered, wide)
 *   createLabel(screen, "", NULL, FONT_LARGE,
 *               LV_ALIGN_BOTTOM_MID, 0, -25, 380, LV_TEXT_ALIGN_CENTER);
 */
lv_obj_t* createLabel(
    lv_obj_t* parent,
    const char* text,
    lv_style_t* text_style,
    const lv_font_t* text_font,
    lv_align_t alignment,
    int16_t x_offset,
    int16_t y_offset,
    int16_t width = 0,
    lv_text_align_t text_align = LV_TEXT_ALIGN_LEFT,
    int16_t letter_spacing = 0
);

// =============================================================================
// ROLLER FUNCTIONS
// =============================================================================

/**
 * @brief Get Roller Options.
 *
 * Inputs: `type`, `custom_options`.
 * Returns: Pointer to the requested object or string; may be `nullptr` when unavailable.
 */
const char* getRollerOptions(RollerType type, const char* custom_options = nullptr);
/**
 * @brief Get Roller Type Spec.
 *
 * Inputs: `type`.
 * Returns: Roller metadata for the requested type.
 */
RollerTypeSpec getRollerTypeSpec(RollerType type);

lv_obj_t* createTypedRoller(
    lv_obj_t* parent,
    RollerType type,
    lv_style_t* style,
    lv_align_t alignment,
    int16_t x_offset,
    int16_t y_offset,
    uint16_t initial_selection = 0
);

lv_obj_t* createTypedRoller(
    lv_obj_t* parent,
    RollerType type,
    lv_style_t* style,
    int16_t width,
    int16_t height,
    lv_align_t alignment,
    int16_t x_offset,
    int16_t y_offset,
    uint16_t initial_selection = 0
);

void createTypedRollerArray(
    lv_obj_t* parent,
    lv_obj_t* roller_array[],
    int count,
    RollerType type,
    lv_style_t* style,
    int16_t spacing,
    int16_t start_x,
    int16_t y_position,
    const char* pref_key_prefix = nullptr,
    int row_break = 0
);

// =============================================================================
// GENERIC ROLLER GRID BUILDER (maximize roller size)
// =============================================================================

// Layout for a roller grid. Rows/cols are derived from count:
// - 1 row up to 5 rollers
// - 2 rows up to 10 rollers
typedef struct {
    int rows;
    int cols;
    int16_t start_x;
    int16_t top_y;
    int16_t roller_w;
    int16_t roller_h;
    int16_t col_gap;
    int16_t row_gap;
} ui_roller_grid_layout_t;

// Compute the largest possible equal-size roller grid within the parent.
ui_roller_grid_layout_t ui_calc_roller_grid(
    lv_obj_t* parent,
    int roller_count,
    int16_t top_y,
    int16_t bottom_reserved,
    int16_t side_margin,
    int16_t col_gap,
    int16_t row_gap
);

// Create a grid of typed rollers with maximum size. Types are per-roller.
// initial_sel may be NULL.
void ui_create_typed_roller_grid(
    lv_obj_t* parent,
    lv_obj_t* rollers_out[],
    int roller_count,
    const RollerType types[],
    lv_style_t* style,
    const ui_roller_grid_layout_t* layout,
    const uint16_t* initial_sel
);

// =============================================================================
// BATTERY GRAPHIC COMPONENT
// =============================================================================

/**
 * Battery graphic objects (managed internally)
 */
typedef struct {
    lv_obj_t* container;        // Main container
    lv_obj_t* bars[4];          // Battery level bars (top to bottom)
    lv_obj_t* lbl_percent;      // Percentage label below battery
    lv_obj_t* lbl_charging;     // Charging indicator (⚡)
    bool initialized;
} BatteryGraphic;

/**
 * Create battery graphic component
 *
 * Creates a vertical battery indicator with:
 * - 4-bar level indicator (color coded)
 * - Percentage text below
 * - Lightning symbol when charging
 *
 * @param parent Parent screen
 * @param x_offset X position from LV_ALIGN_BOTTOM_LEFT
 * @param y_offset Y position from LV_ALIGN_BOTTOM_LEFT (negative = up from bottom)
 * @param battery Output structure (store for later updates)
 *
 * @req UI-BATT-001 Battery graphic shall display level with 4 bars
 * @req UI-BATT-002 Battery graphic shall show percentage text
 * @req UI-BATT-003 Battery graphic shall indicate charging state
 *
 * Example:
 *   static BatteryGraphic battery_graphic;
 *   createBatteryGraphic(main_screen, 30, -95, &battery_graphic);
 *   // Position: 30px from left, 95px up from bottom
 */
void createBatteryGraphic(
    lv_obj_t* parent,
    int16_t x_offset,
    int16_t y_offset,
    BatteryGraphic* battery
);

/**
 * Update battery graphic display
 *
 * Updates bars, percentage, and charging indicator based on current state
 *
 * Color coding:
 * - 76-100%: Green (4 bars)
 * - 51-75%: Yellow (3 bars)
 * - 26-50%: Orange (2 bars)
 * - 0-25%: Red (1 bar)
 * - Charging: Purple lightning indicator; battery bars keep level color
 *
 * @param battery Battery graphic structure
 * @param battery_percent Battery percentage (0-100)
 * @param is_charging True if charging detected
 *
 * Example:
 *   int level = safeBatteryPercent();
 *   bool charging = safeIsVbusIn();
 *   updateBatteryGraphic(&battery_graphic, level, charging);
 */
void updateBatteryGraphic(
    BatteryGraphic* battery,
    int battery_percent,
    bool is_charging
);

// =============================================================================
// STATUS MESSAGE MANAGER
// =============================================================================

/**
 * Status message priorities (higher number = higher priority)
 * Lower priority messages cannot override higher priority messages
 */
enum StatusPriority {
    STATUS_PRIO_NONE = 0,          // Empty/idle
    STATUS_PRIO_INFO = 1,          // WiFi on, Settings needed
    STATUS_PRIO_NOTIFICATION = 2,  // Temporary messages (saved, etc.)
    STATUS_PRIO_WARNING = 3,       // Starting, Stopping
    STATUS_PRIO_ERROR = 4,         // Error states
    STATUS_PRIO_CRITICAL = 5,      // Recording (most important)
    STATUS_PRIO_SYSTEM = 6         // System messages (shutdown) - cannot be overridden
};

/**
 * Status Message Manager - Centralized status message handling with priorities
 *
 * Features:
 * - Priority-based message system
 * - Automatic expiry for temporary messages
 * - Blinking support
 * - Color coding
 * - Thread-safe (LVGL is single-threaded)
 *
 * Example:
 *   StatusManager statusMgr;
 *   statusMgr.setLabel(lbl_status);
 *
 *   // Temporary notification (2 seconds)
 *   statusMgr.setStatus(STATUS_PRIO_NOTIFICATION, "Date saved",
 *                       lv_palette_main(LV_PALETTE_DEEP_PURPLE), false, 2000);
 *
 *   // Persistent state (until cleared)
 *   statusMgr.setStatus(STATUS_PRIO_CRITICAL, "RECORDING",
 *                       lv_palette_main(LV_PALETTE_RED), true);
 *
 *   // In main loop
 *   statusMgr.update(blink_state);
 */
class StatusManager {
private:
    lv_obj_t* lbl_status;

    // Current status
    StatusPriority current_priority;
    char current_message[32];
    lv_color_t current_color;
    bool current_blink;
    uint32_t expiry_time;
    bool show_immediately;  // NEW: Force show on next update (fixes blink timing)

public:
    StatusManager();

    /**
     * Set the status label to manage
     * Call once during initialization
     */
    void setLabel(lv_obj_t* label);

    /**
     * Set status message with priority
     * Only updates if priority >= current priority or current has expired
     *
     * @param priority Message priority
     * @param msg Message text (max 31 chars)
     * @param color Text color
     * @param blink True to blink the message
     * @param duration_ms Duration in milliseconds (0 = permanent until cleared)
     */
    void setStatus(StatusPriority priority, const char* msg,
                   lv_color_t color, bool blink = false, uint32_t duration_ms = 0);

    /**
     * Update display (call from main loop)
     * Handles blinking and expiry
     *
     * @param blink_state Current blink state (true/false, toggles each second)
     */
    void update(bool blink_state);

    /**
     * Clear status (only if priority matches or is lower)
     *
     * @param priority Priority to clear
     */
    void clear(StatusPriority priority);

    /**
     * Force clear (clears regardless of priority)
     * Use with caution!
     */
    void forceClear();

    /**
     * Get current priority (for external checks)
     */
    StatusPriority getPriority() const { return current_priority; }

    /**
     * Check if status is active
     */
    bool isActive() const { return current_priority > STATUS_PRIO_NONE; }
};

// Global status manager instance
extern StatusManager statusManager;

#endif // UI_HELPERS_H
