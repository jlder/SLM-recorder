// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/tasks/ui_task.cpp
 * @brief LVGL user-interface task and screen construction/update logic.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

/*******************************************************************************
 * USER INTERFACE v1.11
 * 
 * Unified helpers, battery enhancements, bug fixes
 * 
 * Version: 1.11
 * Changes:
 * - Unified button/label helpers (one function each)
 * - Battery helper moved to ui_helpers
 * - Battery shows percentage & charging indicator
 * - Battery moved up 25px for better visibility
 * - Main screen now uses helpers
 * - Bug fixes: recording button state, status font
 ******************************************************************************/

#include <Arduino.h>
#include "src/services/task_helpers.h"

// Forward declarations
void initUI();
void updateUI();
void syncUIToSystemState();

#include "src/tasks/ui_task.h"
#include "src/global.h"
#include "config.h"
#include "src/tasks/ui_helpers.h"
#include "src/tasks/state_task.h"
#include "src/tasks/web_task.h"
#include "src/drivers/display_driver.h"
#include "src/services/touch_service.h"
#include "src/services/settings_store.h"
#include "src/services/datetime_service.h"
#include "src/services/button_hold_helpers.h"
#include "src/drivers/rtc_driver.h"
#include <stdio.h>
#include <lvgl.h>
// UI must NOT touch I2C directly. Use the shared date/time cache for RTC values.
/**
 * Handles ui rtc from status for the local UI, display, or user interaction
 * path.
 *
 * Inputs: `dt`.
 * Returns: None.
 */
static void ui_rtc_from_status(rtc_datetime_t* dt)
{
  if (dt == nullptr) return;
  if (!datetime_service_get(dt)) {
    dt->year = 2000; dt->month = 1; dt->day = 1;
    dt->hour = 0; dt->min = 0; dt->sec = 0;
  }
}


// UI background asset
#include "src/assets/SLM_206_v4.h"
// Task handle (registered for stack telemetry)

extern lv_obj_t *reg_rollers[5];
extern lv_obj_t *pwd_rollers[8];

#include "Arduino_GFX_Library.h"
#include "src/services/ui_message.h"

#define UI_DRAW_BUF_LINES     40u
#define UI_DRAW_BUF_MAX_WIDTH 410u

static volatile bool s_touch_activity_detected = false;
static uint32_t s_display_last_activity_ms = 0u;
static bool s_display_dimmed = false;
static bool s_display_prev_usb_valid = false;
static bool s_display_prev_usb_present = false;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================
void createMainScreen(lv_style_t &style_huge, lv_style_t &style_large);
void createMenuScreen(lv_style_t &style_huge);
void createSettingsMenuScreen(lv_style_t &style_huge);
void createSetDateScreen(lv_style_t &style_huge, lv_style_t &style_large);
void createSetTimeScreen(lv_style_t &style_huge, lv_style_t &style_large);
void createSetRegScreen(lv_style_t &style_huge, lv_style_t &style_large);
void createSetWifiPwdScreen(lv_style_t &style_huge, lv_style_t &style_large);
void createStandbyScreen(lv_style_t &style_huge);
void createLowBatteryScreen(lv_style_t &style_huge);

void wifi_btn_cb(lv_event_t * e);
void record_btn_event_cb(lv_event_t * e);
void save_date_cb(lv_event_t * e);
void save_time_cb(lv_event_t * e);
void save_reg_cb(lv_event_t * e);
void save_wifi_pwd_cb(lv_event_t * e);

/**
 * Convert a registration character to its roller index performs the ui task
 * operation represented by this function and keeps the module state consistent
 * with recorder ownership rules.
 *
 * Inputs: `c`.
 * Returns: Roller index for the character, or zero for unsupported characters.
 */
static int reg_char_to_index(char c){
    if(c=='_') return 0;
    if(c>='0' && c<='9') return 1 + (c - '0');         // '_' then digits
    if(c>='A' && c<='Z') return 11 + (c - 'A');        // '_' + 10 digits => 11
    return 0;
}

/**
 * Convert a Wi-Fi password character to its roller index performs the ui task
 * operation represented by this function and keeps the module state consistent
 * with recorder ownership rules.
 *
 * Inputs: `c`.
 * Returns: Roller index for the character, or zero for unsupported characters.
 */
static int pwd_char_to_index(char c){
    if(c>='0' && c<='9') return (c - '0');             // digits first
    if(c>='a' && c<='z') return 10 + (c - 'a');        // then lowercase
    if(c>='A' && c<='Z') return 36 + (c - 'A');        // then uppercase (10+26=36)
    return 0;
}

/**
 * Loads stored/default values into the relevant UI controls so the operator
 * starts from the current configuration.
 *
 * Inputs: None.
 * Returns: None.
 */
static void load_registration_into_rollers(void){
    settings_t s;
    if(!settings_get(&s)){
        // fallback to blanks
        for(int i=0;i<5;i++) lv_roller_set_selected(reg_rollers[i], 0, LV_ANIM_OFF);
        return;
    }
    // Ensure at least 5 chars; pad with '_'
    char reg5[5];
    for(int i=0;i<5;i++){
        char c = s.registration[i];
        if(c=='\0') c = '_';
        reg5[i]=c;
    }
    for(int i=0;i<5;i++){
        lv_roller_set_selected(reg_rollers[i], reg_char_to_index(reg5[i]), LV_ANIM_OFF);
    }
}

/**
 * Loads stored/default values into the relevant UI controls so the operator
 * starts from the current configuration.
 *
 * Inputs: None.
 * Returns: None.
 */
static void load_wifi_pwd_into_rollers(void){
    settings_t s;
    if(!settings_get(&s)){
        for(int i=0;i<8;i++) lv_roller_set_selected(pwd_rollers[i], 0, LV_ANIM_OFF);
        return;
    }
    char pwd8[8];
    for(int i=0;i<8;i++){
        char c = s.wifi_password[i];
        if(c=='\0') c = '0';
        pwd8[i]=c;
    }
    for(int i=0;i<8;i++){
        lv_roller_set_selected(pwd_rollers[i], pwd_char_to_index(pwd8[i]), LV_ANIM_OFF);
    }
}

// =============================================================================
// GLOBAL UI STATE
// =============================================================================

static Arduino_GFX *gfx = NULL;


// =============================================================================

uint32_t screenWidth, screenHeight;
lv_display_t *disp;
static lv_color_t s_disp_draw_buf_mem[UI_DRAW_BUF_MAX_WIDTH * UI_DRAW_BUF_LINES];
static lv_color_t *disp_draw_buf = s_disp_draw_buf_mem;
lv_obj_t *main_screen = NULL;
lv_obj_t *menu_screen = NULL;
lv_obj_t *settings_menu_screen = NULL;
lv_obj_t *set_date_screen = NULL;
lv_obj_t *set_time_screen = NULL;
lv_obj_t *set_reg_screen = NULL;
lv_obj_t *set_wifi_pwd_screen = NULL;
lv_obj_t *standby_screen = NULL;
lv_obj_t *low_battery_screen = NULL;

lv_obj_t *lbl_main_time = NULL;
lv_obj_t *lbl_main_date = NULL;
lv_obj_t *lbl_main_version = NULL;
lv_obj_t *lbl_status = NULL;
lv_obj_t *btn_main_menu = NULL;

// Battery graphic - NEW: structure-based
static BatteryGraphic battery_graphic;

// Main-screen WiFi status graphic.  The object is created once with the main
// screen and shown only while the local Web/WiFi access point is requested.
static lv_obj_t *obj_main_wifi = NULL;

// MENU button layout.  Four equal-height buttons are spaced by one constant
// vertical step so the gaps remain even and easy to adjust.
static const int16_t MENU_BUTTON_FIRST_Y = 80;
static const int16_t MENU_BUTTON_STEP_Y  = 105;

lv_obj_t *y_roller = NULL;
lv_obj_t *mo_roller = NULL;
lv_obj_t *d_roller = NULL;
lv_obj_t *h_roller = NULL;
lv_obj_t *m_roller = NULL;
lv_obj_t *reg_rollers[5] = {NULL};
lv_obj_t *pwd_rollers[8] = {NULL};

lv_obj_t *btn_wifi_label = NULL;
lv_obj_t *btn_wifi = NULL;
lv_obj_t *btn_record = NULL;
lv_obj_t *btn_record_label = NULL;
lv_obj_t *btn_set = NULL;
lv_obj_t *btn_settings_date = NULL;
lv_obj_t *btn_settings_time = NULL;
lv_obj_t *btn_settings_reg = NULL;
lv_obj_t *btn_settings_wifi = NULL;


// UI START/STOP RECORD button hold state.  The touch button deliberately uses
// a longer local hold time than the physical RECORD button to reduce accidental
// starts from the menu page.
static const uint32_t UI_RECORD_HOLD_MS = 1000u;
static bool s_ui_record_btn_pressed = false;
static bool s_ui_record_btn_consumed = false;
static uint32_t s_ui_record_btn_press_ms = 0u;

// =============================================================================
// LVGL CALLBACKS
// =============================================================================

/**
 * My disp flush performs the ui task operation represented by this function
 * and keeps the module state consistent with recorder ownership rules.
 *
 * Inputs: `disp`, `area`, `px_map`.
 * Returns: None.
 */
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    lv_disp_flush_ready(disp);
}

/**
 * Handles ui touch activity consume for the local UI, display, or user
 * interaction path.
 *
 * Inputs: None.
 * Returns: `true` if touch activity was detected since the last call; otherwise `false`.
 */
static bool ui_touch_activity_consume_(void){
    const bool detected = s_touch_activity_detected;
    s_touch_activity_detected = false;
    return detected;
}

/**
 * Display wake helper records recent activity, restores full brightness, and
 * returns from standby to the main UI.
 *
 * Inputs: None.
 * Returns: None.
 */
static void ui_display_wake_(uint32_t now){
    s_display_last_activity_ms = now;

    if(s_display_dimmed){
        display_brightness_set(DISPLAY_BRIGHTNESS_ACTIVE);
        s_display_dimmed = false;

        if(main_screen != NULL){
            lv_scr_load(main_screen);
        }
    }
}

/**
 * Display standby service enters the black standby screen from the main page,
 * detects wake conditions, and restores the active UI when needed.
 *
 * Inputs: None.
 * Returns: None.
 */
static void ui_display_standby_service_(void){
    const uint32_t now = (uint32_t)millis();
    const system_status_t st = state_task_get_status();

    const bool touch_activity = ui_touch_activity_consume_();
    const bool button_activity = power_button_pressed() || record_button_pressed();

    const bool message_or_error =
        (st.last_error != 0) ||
        ((st.message_id != MSG_NONE) &&
         (st.message_id != MSG_READY) &&
         (st.message_id != MSG_RECORDING) &&
         (st.message_id != MSG_SD_FULL_FILES));

    const bool usb_inserted =
        st.usb_present_valid &&
        st.usb_present &&
        ((!s_display_prev_usb_valid) || (!s_display_prev_usb_present));

    if(st.usb_present_valid){
        s_display_prev_usb_valid = true;
        s_display_prev_usb_present = st.usb_present;
    }else{
        s_display_prev_usb_valid = false;
        s_display_prev_usb_present = false;
    }

    const bool recorder_state_allows_standby =
        (st.state == ST_READY) ||
        (st.state == ST_RECORDING);

    // Standby may be entered only from the main display. Once already in
    // standby, the standby screen itself is allowed to remain active until a
    // normal wake condition occurs.
    const lv_obj_t *active_screen = lv_scr_act();
    const bool screen_allows_standby =
        (active_screen == main_screen) ||
        (s_display_dimmed && (active_screen == standby_screen));

    const bool standby_allowed =
        recorder_state_allows_standby && screen_allows_standby;

    if((!standby_allowed) ||
       touch_activity ||
       button_activity ||
       message_or_error ||
       usb_inserted){
        ui_display_wake_(now);
        return;
    }

    if(!s_display_dimmed &&
       ((now - s_display_last_activity_ms) >= DISPLAY_DIM_TIMEOUT_MS)){
        if(standby_screen != NULL){
            lv_scr_load(standby_screen);
        }
        display_brightness_set(DISPLAY_BRIGHTNESS_DIMMED);
        s_display_dimmed = true;
    }
}

/**
 * LVGL touch callback reads the touch-service snapshot, converts it to LVGL
 * input data, and records touch activity for standby wake.
 *
 * Inputs: `indev`, `data`.
 * Returns: None.
 */
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    touch_snapshot_t s = touch_service_get_snapshot();
    if (!s.valid || !s.pressed) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    data->state = LV_INDEV_STATE_PR;
    data->point.x = s.x;
    data->point.y = s.y;
    s_touch_activity_detected = true;
}

// =============================================================================
// UI HELPER FUNCTIONS
// =============================================================================

/**
 * Apply global background performs the ui task operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `screen`.
 * Returns: None.
 */
void apply_global_background(lv_obj_t * screen) {
    lv_obj_set_style_bg_img_src(screen, &SLM_206_v4, 0);
}

/**
 * Creates the main-screen Wi-Fi graphic shown while Web/Wi-Fi access is active.
 *
 * The graphic uses LVGL's built-in Wi-Fi symbol instead of drawing arcs
 * manually.  It is placed slightly to the right and lower than the top-left
 * corner so it remains clear of the rounded display edge.
 *
 * Inputs: `parent`.
 * Returns: None.
 */
static void create_wifi_graphic_(lv_obj_t *parent) {
    obj_main_wifi = lv_label_create(parent);
    lv_label_set_text(obj_main_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(obj_main_wifi, FONT_HUGE, 0);
    lv_obj_set_style_text_color(obj_main_wifi, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align(obj_main_wifi, LV_ALIGN_TOP_LEFT, 48, 34);
    lv_obj_add_flag(obj_main_wifi, LV_OBJ_FLAG_HIDDEN);
}

/**
 * Shows or hides the main-screen WiFi graphic according to Web/WiFi state.
 *
 * Inputs: `visible`.
 * Returns: None.
 */
static void ui_wifi_graphic_set_visible_(bool visible) {
    if(obj_main_wifi == NULL){
        return;
    }

    if(visible){
        lv_obj_clear_flag(obj_main_wifi, LV_OBJ_FLAG_HIDDEN);
    }else{
        lv_obj_add_flag(obj_main_wifi, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * Updates or evaluates disable button state used to qualify physical button
 * gestures without blocking the state machine.
 *
 * Inputs: `btn`.
 * Returns: None.
 */
void disable_button(lv_obj_t * btn) {
    lv_obj_add_state(btn, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREY), 0);
}

/**
 * Updates or evaluates enable button state used to qualify physical button
 * gestures without blocking the state machine.
 *
 * Inputs: `btn`, `color`.
 * Returns: None.
 */
void enable_button(lv_obj_t * btn, lv_color_t color) {
    lv_obj_clear_state(btn, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(btn, color, 0);
}

/**
 * Handles settings action required for the local UI, display, or user
 * interaction path.
 *
 * Inputs: `msg`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
static bool settings_action_required_(msg_id_t msg){
    return (msg == MSG_SETTINGS_LOCKED);
}

/**
 * Handles calibration action required for the local UI, display, or user
 * interaction path.
 *
 * Inputs: `msg`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
static bool calibration_action_required_(msg_id_t msg){
    return (msg == MSG_ACCEL_CALIBRATION_REQUIRED) ||
           (msg == MSG_INSTALLATION_CALIBRATION_REQUIRED) ||
           (msg == MSG_CALIBRATION_FAULT);
}

/**
 * Handles SD maintenance action required for the local UI, display, or user
 * interaction path.
 *
 * Inputs: `msg`.
 * Returns: `true` when SD file-count maintenance is needed; otherwise `false`.
 */
static bool sd_maintenance_action_required_(msg_id_t msg){
    return (msg == MSG_SD_FULL_FILES);
}

/**
 * Handles setup action required for the local UI, display, or user interaction
 * path.
 *
 * Inputs: `msg`.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
static bool setup_action_required_(msg_id_t msg){
    return settings_action_required_(msg) ||
           calibration_action_required_(msg) ||
           sd_maintenance_action_required_(msg);
}

/**
 * Reports whether all local settings required before WiFi/calibration are
 * complete.
 *
 * Inputs: None.
 * Returns: `true` when settings are complete; otherwise `false`.
 */
static bool local_settings_complete_(void){
    settings_t s = {};
    return settings_get(&s) && settings_is_complete(&s);
}

/**
 * Reports whether the local START/STOP RECORD button should request STOP.
 *
 * Inputs: `st`.
 * Returns: `true` while a recording session is active or transitioning.
 */
static bool ui_record_button_shows_stop_(const system_status_t& st){
    return (st.state == ST_RECORDING) ||
           (st.state == ST_STARTING) ||
           (st.state == ST_STOPPING);
}

/**
 * Reports whether the UI START RECORD action is currently allowed.
 *
 * The State task accepts a record-start request in ST_READY only when the
 * user-visible message is MSG_READY.  Other READY messages indicate that a
 * setup, calibration, or SD maintenance condition still blocks recording.
 *
 * Inputs: `st`.
 * Returns: `true` only when a START RECORD request can be consumed.
 */
static bool ui_record_start_allowed_(const system_status_t& st){
    return (st.state == ST_READY) && (st.message_id == MSG_READY);
}

/**
 * Reports whether the local START/STOP RECORD button may be pressed.
 *
 * Inputs: `st`.
 * Returns: `true` only when the State task can consume the matching request.
 */
static bool ui_record_button_enabled_(const system_status_t& st){
    return ui_record_start_allowed_(st) || (st.state == ST_RECORDING);
}

/**
 * Reset the local hold detector used by the START/STOP RECORD menu button.
 *
 * Inputs: None.
 * Returns: None.
 */
static void ui_record_button_hold_reset_(void){
    s_ui_record_btn_pressed = false;
    s_ui_record_btn_consumed = false;
    s_ui_record_btn_press_ms = 0u;
}

/**
 * Service the local START/STOP RECORD long-press detector.
 *
 * The LVGL event callback only records press/release state.  This periodic
 * service checks the elapsed press time against UI_RECORD_HOLD_MS and sends
 * exactly one State-task request per press.
 *
 * Inputs: `st`.
 * Returns: None.
 */
static void ui_record_button_hold_service_(const system_status_t& st){
    if((!s_ui_record_btn_pressed) || s_ui_record_btn_consumed){
        return;
    }

    if(((uint32_t)millis() - s_ui_record_btn_press_ms) < UI_RECORD_HOLD_MS){
        return;
    }

    // Only READY and RECORDING consume UI record commands.  Transient states
    // disable the button and ignore any stale touch hold.
    if(st.state == ST_READY){
        state_task_request_record_start();
        s_ui_record_btn_consumed = true;
    } else if(st.state == ST_RECORDING){
        state_task_request_record_stop();
        s_ui_record_btn_consumed = true;
    } else {
        ui_record_button_hold_reset_();
    }
}


/**
 * Refreshes SETTINGS and setting-specific button colors so missing setup items
 * remain orange and completed items turn blue.
 *
 * Inputs: None.
 * Returns: None.
 */
static void refreshSettingsButtons(void) {
    settings_t s = {};
    const bool have_settings = settings_get(&s);
    const bool reg_ok = have_settings && (s.registration[0] != '\0');
    const bool wifi_ok = have_settings && (s.wifi_password[0] != '\0');
    const bool date_ok = have_settings && s.date_set;
    const bool time_ok = have_settings && s.time_set;
    const bool complete = reg_ok && wifi_ok && date_ok && time_ok;

    const lv_color_t blue = lv_palette_main(LV_PALETTE_BLUE);
    const lv_color_t amber = lv_palette_main(LV_PALETTE_AMBER);

    // Highlight the SETTINGS entry until all required items are saved.
    if (btn_set && !lv_obj_has_state(btn_set, LV_STATE_DISABLED)) {
        enable_button(btn_set, complete ? blue : amber);
    }

    // Highlight each missing settings item directly in the settings menu.
    if (btn_settings_date) enable_button(btn_settings_date, date_ok ? blue : amber);
    if (btn_settings_time) enable_button(btn_settings_time, time_ok ? blue : amber);
    if (btn_settings_reg) enable_button(btn_settings_reg, reg_ok ? blue : amber);
    if (btn_settings_wifi) enable_button(btn_settings_wifi, wifi_ok ? blue : amber);
}

// =============================================================================
// SCREEN: MAIN (NOW USING UNIFIED HELPERS!)
// =============================================================================

/**
 * Creates the main recorder screen containing time, date, menu access, version
 * text, battery graphic, and status/message area.
 *
 * Inputs: `style_huge`, `style_large`.
 * Returns: None.
 */
void createMainScreen(lv_style_t &style_huge, lv_style_t &style_large) {
    main_screen = lv_obj_create(NULL);
    apply_global_background(main_screen);
    
    // Time label - using unified helper
    lbl_main_time = createLabel(main_screen, "", NULL, FONT_HUGE,
        LV_ALIGN_TOP_MID, 0, 10, 380, LV_TEXT_ALIGN_CENTER);
    
    // Date label - using unified helper
    lbl_main_date = createLabel(main_screen, "", NULL, FONT_MEDIUM,
        LV_ALIGN_TOP_MID, 0, 65, 380, LV_TEXT_ALIGN_CENTER);

    create_wifi_graphic_(main_screen);
    
    // Menu button - using unified helper (was 8 lines, now 1!)
    btn_main_menu = createButton(main_screen, "MENU", &style_huge, NULL,
        BTN_MAIN_WIDTH, BTN_MAIN_HEIGHT,
        LV_ALIGN_TOP_MID, 0, 105,
        [](lv_event_t*e){ lv_scr_load(menu_screen); });

    // Hardware/software version displayed under the MENU button.
    lbl_main_version = createLabel(main_screen, RECORDER_VERSION_TEXT, NULL, FONT_SMALL,
        LV_ALIGN_TOP_MID, 0, 190, 380, LV_TEXT_ALIGN_CENTER);
    
    // Battery graphic - NEW: using helper with percentage & charging
    createBatteryGraphic(main_screen, 30, -95, &battery_graphic);
    
    // Status label - using unified helper with letter spacing
    lbl_status = createLabel(main_screen, "", NULL, FONT_LARGE,
        LV_ALIGN_BOTTOM_MID, 0, -25, 380, LV_TEXT_ALIGN_CENTER, 2);
}


// =============================================================================
// SCREEN: STANDBY
// =============================================================================

/**
 * Creates the black AMOLED standby screen with dimmed wake instruction text
 * used to reduce display power.
 *
 * Inputs: `style_large`.
 * Returns: None.
 */
void createStandbyScreen(lv_style_t &style_huge) {
    standby_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(standby_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(standby_screen, LV_OPA_COVER, 0);

    lv_obj_t *label = lv_label_create(standby_screen);
    lv_label_set_text(label, "TOUCH TO ACTIVATE");
    lv_obj_add_style(label, &style_huge, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_opa(label, LV_OPA_80, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

/**
 * Creates the full-screen low-battery notice shown before automatic shutdown.
 *
 * Inputs: `style_huge`.
 * Returns: None.
 */
void createLowBatteryScreen(lv_style_t &style_huge) {
    low_battery_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(low_battery_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(low_battery_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(low_battery_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(low_battery_screen);
    lv_label_set_text(label, "BATTERY LOW\nRECHARGE WITH USB");
    lv_obj_add_style(label, &style_huge, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label, 390);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

// =============================================================================
// SCREEN: MENU
// =============================================================================

/**
 * Creates the local MENU page with START/STOP RECORD, WiFi, SETTINGS, and
 * BACK actions.  BACK only returns to MAIN; Web/WiFi remains controlled by
 * the START/STOP WIFI button.
 *
 * Inputs: `style_huge`.
 * Returns: None.
 */
void createMenuScreen(lv_style_t &style_huge) {
    menu_screen = lv_obj_create(NULL);
    
    createScreenTitle(menu_screen, "MENU", 20);
    
    btn_record = createMenuButton(menu_screen, "START RECORD", &style_huge, MENU_BUTTON_FIRST_Y, NULL);
    btn_record_label = lv_obj_get_child(btn_record, 0);
    lv_obj_add_event_cb(btn_record, record_btn_event_cb, LV_EVENT_ALL, NULL);

    btn_wifi = createMenuButton(menu_screen, "START WIFI", &style_huge, MENU_BUTTON_FIRST_Y + MENU_BUTTON_STEP_Y, wifi_btn_cb);
    btn_wifi_label = lv_obj_get_child(btn_wifi, 0);
    
    btn_set = createMenuButton(menu_screen, "SETTINGS", &style_huge, MENU_BUTTON_FIRST_Y + (2 * MENU_BUTTON_STEP_Y), 
        [](lv_event_t* e){ lv_scr_load(settings_menu_screen); });
    
    // BACK button - use createButton directly (needs green color)
    createButton(menu_screen, "BACK", &style_huge, NULL,
        BTN_MENU_WIDTH, BTN_MENU_HEIGHT,
        LV_ALIGN_TOP_MID, 0, MENU_BUTTON_FIRST_Y + (3 * MENU_BUTTON_STEP_Y),
        [](lv_event_t*e){
            (void)e;
            lv_scr_load(main_screen);
        },
        lv_palette_main(LV_PALETTE_GREEN));
}

// =============================================================================
// SCREEN: SETTINGS MENU
// =============================================================================

/**
 * Creates the SETTINGS menu and connects each setting button to the
 * corresponding editor page with preloaded values.
 *
 * Inputs: `style_huge`.
 * Returns: None.
 */
void createSettingsMenuScreen(lv_style_t &style_huge) {
    settings_menu_screen = lv_obj_create(NULL);
    
    createScreenTitle(settings_menu_screen, "SETTINGS", 20);
    
    btn_settings_date = createMenuButton(settings_menu_screen, "DATE", &style_huge, 70,
        [](lv_event_t* e){ 
            rtc_datetime_t dt = {};
            (void)ui_rtc_from_status(&dt);
            int y = (int)dt.year - 2000;
            if (y < 0) y = 0;
            if (y > 99) y = 99;
            lv_roller_set_selected(y_roller, y, LV_ANIM_OFF);
            lv_roller_set_selected(mo_roller, (int)dt.month - 1, LV_ANIM_OFF);
            lv_roller_set_selected(d_roller, (int)dt.day - 1, LV_ANIM_OFF);
            lv_scr_load(set_date_screen);
        });
    
    btn_settings_time = createMenuButton(settings_menu_screen, "TIME", &style_huge, 160,
        [](lv_event_t* e){ 
            rtc_datetime_t dt = {};
            (void)ui_rtc_from_status(&dt);
            lv_roller_set_selected(h_roller, (int)dt.hour, LV_ANIM_OFF);
            lv_roller_set_selected(m_roller, (int)dt.min, LV_ANIM_OFF);
            lv_scr_load(set_time_screen);
        });
    
    btn_settings_reg = createMenuButton(settings_menu_screen, "REGISTRATION", &style_huge, 250,
        [](lv_event_t* e){
            (void)e;
            load_registration_into_rollers();
            lv_scr_load(set_reg_screen);
        });
    
    btn_settings_wifi = createMenuButton(settings_menu_screen, "WIFI PWD", &style_huge, 340,
        [](lv_event_t* e){
            (void)e;
            load_wifi_pwd_into_rollers();
            lv_scr_load(set_wifi_pwd_screen);
        });
    
    // BACK button - use createButton directly (needs green color)
    createButton(settings_menu_screen, "BACK", &style_huge, NULL,
        BTN_MENU_WIDTH, BTN_MENU_HEIGHT,
        LV_ALIGN_TOP_MID, 0, 430,
        [](lv_event_t*e){ lv_scr_load(menu_screen); },
        lv_palette_main(LV_PALETTE_GREEN));
}

// =============================================================================
// SCREEN: SET DATE
// =============================================================================

/**
 * Creates and configures the Set Date Screen UI objects, labels, callbacks,
 * and initial styling.
 *
 * Inputs: `style_huge`, `style_large`.
 * Returns: None.
 */
void createSetDateScreen(lv_style_t &style_huge, lv_style_t &style_large) {
    set_date_screen = lv_obj_create(NULL);
    
    createScreenTitle(set_date_screen, "SET DATE", 20);

    // Maximize roller size: 1 row up to 5 rollers, 2 rows up to 10 rollers.
    static const RollerType kTypes[3] = { ROLLER_YEAR, ROLLER_MONTH, ROLLER_DAY };
    ui_roller_grid_layout_t lay = ui_calc_roller_grid(set_date_screen,
                                                     3,
                                                     /*top_y*/110,
                                                     /*bottom_reserved*/(BTN_ACTION_HEIGHT + 70),
                                                     /*side_margin*/20,
                                                     /*col_gap*/10,
                                                     /*row_gap*/10);

    // Column labels (same width as rollers, centered text).
    (void)createLabel(set_date_screen, "Year", &style_large, NULL,
                      LV_ALIGN_TOP_LEFT,
                      lay.start_x + 0 * (lay.roller_w + lay.col_gap),
                      70, lay.roller_w, LV_TEXT_ALIGN_CENTER);
    (void)createLabel(set_date_screen, "Month", &style_large, NULL,
                      LV_ALIGN_TOP_LEFT,
                      lay.start_x + 1 * (lay.roller_w + lay.col_gap),
                      70, lay.roller_w, LV_TEXT_ALIGN_CENTER);
    (void)createLabel(set_date_screen, "Day", &style_large, NULL,
                      LV_ALIGN_TOP_LEFT,
                      lay.start_x + 2 * (lay.roller_w + lay.col_gap),
                      70, lay.roller_w, LV_TEXT_ALIGN_CENTER);

    lv_obj_t* rollers[3] = {0};
    ui_create_typed_roller_grid(set_date_screen, rollers, 3, kTypes, &style_huge, &lay, nullptr);
    y_roller  = rollers[0];
    mo_roller = rollers[1];
    d_roller  = rollers[2];
    
    createActionButton(set_date_screen, "SAVE", &style_huge, 
        LV_ALIGN_BOTTOM_LEFT, 30, -30, save_date_cb,
        lv_palette_main(LV_PALETTE_BLUE));
    createActionButton(set_date_screen, "BACK", &style_huge,
        LV_ALIGN_BOTTOM_RIGHT, -30, -30,
        [](lv_event_t*e){ lv_scr_load(settings_menu_screen); },
        lv_palette_main(LV_PALETTE_GREEN));
}

// =============================================================================
// SCREEN: SET TIME
// =============================================================================

/**
 * Creates and configures the Set Time Screen UI objects, labels, callbacks,
 * and initial styling.
 *
 * Inputs: `style_huge`, `style_large`.
 * Returns: None.
 */
void createSetTimeScreen(lv_style_t &style_huge, lv_style_t &style_large) {
    set_time_screen = lv_obj_create(NULL);
    
    createScreenTitle(set_time_screen, "SET TIME", 20);

    static const RollerType kTypes[2] = { ROLLER_HOUR, ROLLER_MINUTE };
    ui_roller_grid_layout_t lay = ui_calc_roller_grid(set_time_screen,
                                                     2,
                                                     /*top_y*/120,
                                                     /*bottom_reserved*/(BTN_ACTION_HEIGHT + 70),
                                                     /*side_margin*/30,
                                                     /*col_gap*/20,
                                                     /*row_gap*/10);

    (void)createLabel(set_time_screen, "Hour", &style_large, NULL,
                      LV_ALIGN_TOP_LEFT,
                      lay.start_x + 0 * (lay.roller_w + lay.col_gap),
                      70, lay.roller_w, LV_TEXT_ALIGN_CENTER);
    (void)createLabel(set_time_screen, "Minute", &style_large, NULL,
                      LV_ALIGN_TOP_LEFT,
                      lay.start_x + 1 * (lay.roller_w + lay.col_gap),
                      70, lay.roller_w, LV_TEXT_ALIGN_CENTER);

    lv_obj_t* rollers[2] = {0};
    ui_create_typed_roller_grid(set_time_screen, rollers, 2, kTypes, &style_huge, &lay, nullptr);
    h_roller = rollers[0];
    m_roller = rollers[1];
    
    createActionButton(set_time_screen, "SAVE", &style_huge, 
        LV_ALIGN_BOTTOM_LEFT, 30, -30, save_time_cb,
        lv_palette_main(LV_PALETTE_BLUE));
    createActionButton(set_time_screen, "BACK", &style_huge,
        LV_ALIGN_BOTTOM_RIGHT, -30, -30,
        [](lv_event_t*e){ lv_scr_load(settings_menu_screen); },
        lv_palette_main(LV_PALETTE_GREEN));
}

// =============================================================================
// SCREEN: SET REGISTRATION
// =============================================================================

/**
 * Creates and configures the Set Reg Screen UI objects, labels, callbacks, and
 * initial styling.
 *
 * Inputs: `style_huge`, `style_large`.
 * Returns: None.
 */
void createSetRegScreen(lv_style_t &style_huge, lv_style_t &style_large) {
    set_reg_screen = lv_obj_create(NULL);
    
    createScreenTitle(set_reg_screen, "SET REGISTRATION", 20);
    
    lv_obj_t *subtitle = createLabel(set_reg_screen, "Aircraft Registration (5 chars)",
        NULL, FONT_SMALL, LV_ALIGN_TOP_MID, 0, 60, 0, LV_TEXT_ALIGN_CENTER);
    
    static const RollerType kTypes[5] = { ROLLER_REG_CHAR, ROLLER_REG_CHAR, ROLLER_REG_CHAR, ROLLER_REG_CHAR, ROLLER_REG_CHAR };
    ui_roller_grid_layout_t lay = ui_calc_roller_grid(set_reg_screen,
                                                     5,
                                                     /*top_y*/100,
                                                     /*bottom_reserved*/(BTN_ACTION_HEIGHT + 70),
                                                     /*side_margin*/15,
                                                     /*col_gap*/8,
                                                     /*row_gap*/8);
    ui_create_typed_roller_grid(set_reg_screen, reg_rollers, 5, kTypes, &style_huge, &lay, nullptr);
    
    createActionButton(set_reg_screen, "SAVE", &style_huge, 
        LV_ALIGN_BOTTOM_LEFT, 30, -30, save_reg_cb,
        lv_palette_main(LV_PALETTE_BLUE));
    createActionButton(set_reg_screen, "BACK", &style_huge,
        LV_ALIGN_BOTTOM_RIGHT, -30, -30,
        [](lv_event_t*e){ lv_scr_load(settings_menu_screen); },
        lv_palette_main(LV_PALETTE_GREEN));
}

// =============================================================================
// SCREEN: SET WIFI PASSWORD
// =============================================================================

/**
 * Creates and configures the Set Wifi Pwd Screen UI objects, labels,
 * callbacks, and initial styling.
 *
 * Inputs: `style_huge`, `style_large`.
 * Returns: None.
 */
void createSetWifiPwdScreen(lv_style_t &style_huge, lv_style_t &style_large) {
    set_wifi_pwd_screen = lv_obj_create(NULL);
    
    lv_obj_t *title = createLabel(set_wifi_pwd_screen, "SET WIFI PASSWORD",
        NULL, &lv_font_montserrat_28, LV_ALIGN_TOP_MID, 0, 15, 0, LV_TEXT_ALIGN_CENTER);
    
    lv_obj_t *subtitle = createLabel(set_wifi_pwd_screen, "8 characters minimum",
        NULL, FONT_SMALL, LV_ALIGN_TOP_MID, 0, 50, 0, LV_TEXT_ALIGN_CENTER);
    
    static const RollerType kTypes[8] = { ROLLER_PWD_CHAR, ROLLER_PWD_CHAR, ROLLER_PWD_CHAR, ROLLER_PWD_CHAR,
                                          ROLLER_PWD_CHAR, ROLLER_PWD_CHAR, ROLLER_PWD_CHAR, ROLLER_PWD_CHAR };
    ui_roller_grid_layout_t lay = ui_calc_roller_grid(set_wifi_pwd_screen,
                                                     8,
                                                     /*top_y*/85,
                                                     /*bottom_reserved*/(BTN_ACTION_HEIGHT + 70),
                                                     /*side_margin*/12,
                                                     /*col_gap*/8,
                                                     /*row_gap*/10);
    ui_create_typed_roller_grid(set_wifi_pwd_screen, pwd_rollers, 8, kTypes, &style_huge, &lay, nullptr);
    
    createActionButton(set_wifi_pwd_screen, "SAVE", &style_huge, 
        LV_ALIGN_BOTTOM_LEFT, 30, -30, save_wifi_pwd_cb,
        lv_palette_main(LV_PALETTE_BLUE));
    createActionButton(set_wifi_pwd_screen, "BACK", &style_huge,
        LV_ALIGN_BOTTOM_RIGHT, -30, -30,
        [](lv_event_t*e){ lv_scr_load(settings_menu_screen); },
        lv_palette_main(LV_PALETTE_GREEN));
}

// =============================================================================
// INITIALIZATION
// =============================================================================

/**
 * Init UI performs the ui task operation represented by this function and
 * keeps the module state consistent with recorder ownership rules.
 *
 * Inputs: None.
 * Returns: None.
 */
void initUI() {
    gfx = display_driver_get_gfx();

    if (!gfx) {
        while (true) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    
        s_display_last_activity_ms = (uint32_t)millis();
    s_display_dimmed = false;
    display_brightness_set(DISPLAY_BRIGHTNESS_ACTIVE);

lv_init();
    lv_tick_set_cb([](){ return (uint32_t)millis(); });
    
    screenWidth = gfx->width();
    screenHeight = gfx->height();

    // LVGL draw buffer is statically allocated (no heap).
    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, disp_draw_buf, NULL, 
        screenWidth * 40 * sizeof(lv_color_t), 
        LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    
    static lv_style_t style_huge;
    lv_style_init(&style_huge);
    lv_style_set_text_font(&style_huge, &lv_font_montserrat_34);
    
    static lv_style_t style_large;
    lv_style_init(&style_large);
    lv_style_set_text_font(&style_large, &lv_font_montserrat_24);
    
    createMainScreen(style_huge, style_large);
    createMenuScreen(style_huge);
    createSettingsMenuScreen(style_huge);
    createSetDateScreen(style_huge, style_large);
    createSetTimeScreen(style_huge, style_large);
    createSetRegScreen(style_huge, style_large);
    createSetWifiPwdScreen(style_huge, style_large);
    createStandbyScreen(style_huge);
    createLowBatteryScreen(style_huge);
    
    // Initialize status manager
    statusManager.setLabel(lbl_status);
    
    // Initialize error manager (after StatusManager)
// errorManager init omitted (baseline uses state_task status)

    lv_scr_load(main_screen);
    syncUIToSystemState();
    
}

// =============================================================================
// SYNC UI TO SYSTEM STATE
// =============================================================================

/**
 * Synchronizes local UI widgets, navigation forcing, button colors, WiFi
 * labels, battery graphic, and status message with the latest recorder status.
 *
 * Inputs: None.
 * Returns: None.
 */
void syncUIToSystemState() {
    system_status_t st = state_task_get_status();

    if((st.message_id == MSG_LOW_BATT) && (low_battery_screen != NULL)){
        if(lv_scr_act() != low_battery_screen){
            lv_scr_load(low_battery_screen);
        }
        if(btn_main_menu){
            disable_button(btn_main_menu);
        }
        return;
    }

    bool ready = (st.state == ST_READY);
    bool recording = (st.state == ST_RECORDING);
    bool recording_like = ui_record_button_shows_stop_(st);

    // Generic rule: errors and transient states still force the main screen.
    // ST_RECORDING is the exception: MENU remains accessible so the operator
    // can use the local STOP RECORD button.
    bool non_ready_message = (st.message_id != MSG_NONE) &&
                             (st.message_id != MSG_READY) &&
                             !((st.state == ST_RECORDING) && (st.message_id == MSG_RECORDING));
    const ui_message_info_t *msginfo = ui_message_get(st.message_id);

    // Setup/maintenance-lock messages are allowed to keep MENU/WiFi access
    // while READY. Recording remains blocked by state_task; this only avoids
    // trapping the operator on the main page when setup, calibration, or
    // SD file-count maintenance is required.
    const bool menu_access_lock_ready =
        ready &&
        ((st.message_id == MSG_SETTINGS_LOCKED) ||
         (st.message_id == MSG_ACCEL_CALIBRATION_REQUIRED) ||
         (st.message_id == MSG_INSTALLATION_CALIBRATION_REQUIRED) ||
         (st.message_id == MSG_CALIBRATION_FAULT) ||
         (st.message_id == MSG_SD_FULL_FILES));

    if (menu_access_lock_ready) {
        non_ready_message = false;
    }

    bool force_main = ((!ready) && (!recording)) ||
                      (st.last_error != 0) ||
                      non_ready_message ||
                      (msginfo && msginfo->force_main);
    if (menu_access_lock_ready) {
        force_main = false;
    }

    // During recording the operator may use only MAIN and MENU.  If recording
    // starts from hardware while a deeper settings page is open, return to MAIN.
    if(recording &&
       (lv_scr_act() != main_screen) &&
       (lv_scr_act() != menu_screen) &&
       main_screen){
        lv_scr_load(main_screen);
    } else if (force_main && lv_scr_act() != main_screen && main_screen) {
        lv_scr_load(main_screen);
    }

    const bool settings_action_required = settings_action_required_(st.message_id);
    const bool calibration_action_required = calibration_action_required_(st.message_id);
    const bool sd_maintenance_action_required = sd_maintenance_action_required_(st.message_id);
    const bool setup_action_required = setup_action_required_(st.message_id);

    const lv_color_t blue = lv_palette_main(LV_PALETTE_BLUE);
    const lv_color_t amber = lv_palette_main(LV_PALETTE_AMBER);

    // MENU button: enabled in READY and RECORDING.  During RECORDING, MENU is
    // needed to reach STOP RECORD; deeper maintenance/settings actions are
    // disabled on the menu page.
    if (btn_main_menu) {
        if (ready || recording) enable_button(btn_main_menu, setup_action_required ? amber : blue);
        else disable_button(btn_main_menu);
    }

    const bool wifi_active = web_task_is_enabled();
    ui_wifi_graphic_set_visible_(wifi_active);

    // START/STOP RECORD follows the same gates as the hardware RECORD button.
    // START RECORD is blue only when the State task would accept a start
    // request; otherwise it is disabled and gray.  STOP RECORD remains red
    // while recording is active.
    if(btn_record_label){
        lv_label_set_text(btn_record_label, recording_like ? "STOP RECORD" : "START RECORD");
    }
    if(btn_record){
        if(ui_record_button_enabled_(st)) enable_button(btn_record, recording ? lv_palette_main(LV_PALETTE_RED) : blue);
        else disable_button(btn_record);
    }

    // Update WiFi button label if exists
    if (btn_wifi_label) {
        lv_label_set_text(btn_wifi_label, wifi_active ? "STOP WIFI" : "START WIFI");
    }

    // START WIFI is disabled while settings are incomplete. The WiFi password
    // is part of the required settings, so calibration/Web access is only
    // offered after the settings path has been completed.
    if (btn_wifi) {
        if (recording || settings_action_required) {
            disable_button(btn_wifi);
        } else {
            enable_button(btn_wifi,
                          (calibration_action_required || sd_maintenance_action_required)
                              ? amber
                              : blue);
        }
    }

    // SETTINGS button (on menu screen): disabled while WiFi/Webserver is active.
    // When settings are required, orange guides the operator to SETTINGS.
    if (btn_set) {
        if (recording || wifi_active) disable_button(btn_set);
        else enable_button(btn_set, settings_action_required ? amber : blue);
    }

    refreshSettingsButtons();
}


// =============================================================================
// UPDATE UI
// =============================================================================

/**
 * Updates the visible main-screen time, date, battery, and status fields while
 * the display is active.
 *
 * Inputs: None.
 * Returns: None.
 */
void updateUI() {
    // Pull everything needed from State Task (UI owns no hardware/I2C).
    system_status_t st = state_task_get_status();

    // Service the local long-press START/STOP RECORD button before rendering
    // state-dependent labels and colors.
    ui_record_button_hold_service_(st);

    // Time/date
    char timeStr[16];
    char dateStr[24];

    rtc_datetime_t dt_now = {};
    if(datetime_service_get(&dt_now)){
        snprintf(timeStr, sizeof(timeStr), "%02u:%02u:%02u",
                 (unsigned)dt_now.hour, (unsigned)dt_now.min, (unsigned)dt_now.sec);
        snprintf(dateStr, sizeof(dateStr), "%04u-%02u-%02u",
                 (unsigned)dt_now.year, (unsigned)dt_now.month, (unsigned)dt_now.day);
    }else{
        snprintf(timeStr, sizeof(timeStr), "--:--:--");
        snprintf(dateStr, sizeof(dateStr), "---- -- --");
    }

    if (lbl_main_time) lv_label_set_text(lbl_main_time, timeStr);
    if (lbl_main_date) lv_label_set_text(lbl_main_date, dateStr);

    // Battery graphic: UI reads state snapshot only.
    const int batt_for_ui = st.battery_percent_valid ? (int)st.battery_percent : -1;
    updateBatteryGraphic(&battery_graphic, batt_for_ui, st.usb_present);

    // Status message is exported by state_task as the single UI source of truth.
    const msg_id_t msg_id = st.message_id;

    if (lbl_status) {
        const ui_message_info_t *mi = ui_message_get(msg_id);
        const char *msg_txt = (mi && mi->text) ? mi->text : "";
        lv_label_set_text(lbl_status, msg_txt);

        // Color is table-driven (severity is still available for logic if needed).
        if (mi) {
            switch (mi->color) {
                case UI_COLOR_RED:   lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF0000), 0); break;
                case UI_COLOR_AMBER: lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFFA500), 0); break;
                case UI_COLOR_GREEN: lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x00FF00), 0); break;
                default:             lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFFFFFF), 0); break;
            }
        }
    }

    // Enforce screen-lock rules and update menu-button availability.
    // During RECORDING, MAIN and MENU remain available so the operator can
    // stop recording from the UI; WiFi and Settings are disabled on MENU.
    syncUIToSystemState();
}


// =============================================================================
// BUTTON CALLBACKS
// =============================================================================

/**
 * START/STOP RECORD button event callback records press/release state.
 *
 * The actual start/stop request is generated by ui_record_button_hold_service_()
 * after the button has remained pressed for UI_RECORD_HOLD_MS.  Keeping the
 * timing check in the periodic UI service avoids relying on LVGL long-press
 * default timing.
 *
 * Inputs: `e`.
 * Returns: None.
 */
void record_btn_event_cb(lv_event_t * e) {
    const lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_PRESSED){
        s_ui_record_btn_pressed = true;
        s_ui_record_btn_consumed = false;
        s_ui_record_btn_press_ms = (uint32_t)millis();
        return;
    }

    if((code == LV_EVENT_RELEASED) || (code == LV_EVENT_PRESS_LOST)){
        ui_record_button_hold_reset_();
    }
}

/**
 * WiFi button callback toggles the recorder access point and web server used
 * for file management and calibration support.  After a START or STOP request
 * the UI returns to MAIN so the operator immediately sees the WiFi indicator
 * state next to the time/date field.
 *
 * Inputs: `e`.
 * Returns: None.
 */
void wifi_btn_cb(lv_event_t * e) {
    (void)e;

    bool changed = false;

    if(web_task_is_enabled()){
        web_task_set_enabled(false);
        changed = true;
    }else if(local_settings_complete_()){
        web_task_set_enabled(true);
        changed = true;
    }

    syncUIToSystemState();

    if(changed && (main_screen != NULL)){
        lv_scr_load(main_screen);
    }
}

/**
 * Date save callback stores the selected date, updates the RTC/date-time
 * cache, and refreshes settings button state.
 *
 * Inputs: `e`.
 * Returns: None.
 */
void save_date_cb(lv_event_t * e) {
    (void)e;

    // Rollers: y_roller, mo_roller, d_roller
    // Year roller represents 00-99 (mapped to years 2000-2099).
    rtc_datetime_t dt = {};
    (void)ui_rtc_from_status(&dt);

    uint16_t year  = (uint16_t)lv_roller_get_selected(y_roller) + 2000;
    uint8_t  month = (uint8_t)(lv_roller_get_selected(mo_roller) + 1);
    uint8_t  day   = (uint8_t)(lv_roller_get_selected(d_roller) + 1);

    dt.year  = year;
    dt.month = month;
    dt.day   = day;

    if(datetime_service_set(&dt)){
        (void)settings_set_date_set(true);
    }

    // Return to settings menu after saving
    lv_scr_load(settings_menu_screen);
    syncUIToSystemState();
}




/**
 * Time save callback stores the selected time, updates the RTC/date-time
 * cache, and refreshes settings button state.
 *
 * Inputs: `e`.
 * Returns: None.
 */
void save_time_cb(lv_event_t * e) {
    (void)e;

    // Prototype rollers: h_roller, m_roller
    rtc_datetime_t dt = {};
    (void)ui_rtc_from_status(&dt);

    uint8_t hour = (uint8_t)lv_roller_get_selected(h_roller);
    uint8_t min  = (uint8_t)lv_roller_get_selected(m_roller);

    dt.hour = hour;
    dt.min  = min;
    dt.sec  = 0;

    if(datetime_service_set(&dt)){
        (void)settings_set_time_set(true);
    }

    // Return to settings menu after saving
    lv_scr_load(settings_menu_screen);
    syncUIToSystemState();
}




/**
 * Registration save callback stores the selected glider registration in NVS
 * and refreshes settings button state.
 *
 * Inputs: `e`.
 * Returns: None.
 */
void save_reg_cb(lv_event_t * e) {
    // Store the registration through the settings store abstraction
    // (keeps the on-flash representation aligned with the prototype).
    static const char kRegOpts[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char reg[6] = {0};
    for(int i = 0; i < 5; i++) {
        int idx = (int)lv_roller_get_selected(reg_rollers[i]);
        if(idx < 0 || idx >= (int)(sizeof(kRegOpts)-1)) idx = 0;
        reg[i] = kRegOpts[idx];
    }
    reg[5] = '\0';

    // Persist settings directly via settings_store (updates RAM cache + NVS).
    (void)settings_set_registration(reg);
    syncUIToSystemState();

    lv_scr_load(settings_menu_screen);
}

/**
 * WiFi password save callback stores the selected access-point password in NVS
 * and refreshes settings button state.
 *
 * Inputs: `e`.
 * Returns: None.
 */
void save_wifi_pwd_cb(lv_event_t * e) {
    const char* pwd_chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char password[9];
    
    for(int i = 0; i < 8; i++) {
        int idx = lv_roller_get_selected(pwd_rollers[i]);
        password[i] = pwd_chars[idx];
    }
    password[8] = '\0';

    // Persist settings directly via settings_store (updates RAM cache + NVS).
    (void)settings_set_wifi_password(password);

    // Never print secrets to the serial console.
// message set omitted

    lv_scr_load(settings_menu_screen);
}


/**
 * UI task loop owns LVGL updates, display standby/wake processing, touch
 * rendering, and periodic synchronization of the visible UI with recorder
 * status.
 *
 * Inputs: `arg`.
 * Returns: None.
 */
static void ui_task_loop(void *arg){
    (void)arg;
    initUI();
    for(;;){
        if(s_display_dimmed){
            // In standby, run LVGL first so touch is sampled, then process wake
            // in the same loop. This avoids an extra standby-period delay.
            lv_timer_handler();
            ui_display_standby_service_();
        }else{
            updateUI();
            // When active, run standby/wake service before lv_timer_handler()
            // so a transition to the standby screen is flushed immediately,
            // avoiding a dimmed normal UI frame.
            ui_display_standby_service_();
            lv_timer_handler();
        }
        vTaskDelay(pdMS_TO_TICKS(s_display_dimmed ? 200 : 20)); // ~50 Hz active, ~5 Hz standby
    }
}

/**
 * Initializes ui task init state or hardware resources and prepares the module
 * for later recorder operation.
 *
 * Inputs: None.
 * Returns: None.
 */
void ui_task_init(void){
    const BaseType_t ok = xTaskCreatePinnedToCore(
        ui_task_loop,
        "ui_task",
        CFG_UI_TASK_STACK_WORDS,
        NULL,
        CFG_UI_TASK_PRIO,
        nullptr,
        CFG_UI_TASK_CORE);
    if(ok != pdPASS){
        task_create_failed_reboot("ui_task");
    }
}

