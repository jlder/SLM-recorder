// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/tasks/web_task.cpp
 * @brief Wi-Fi access point and HTTP server task for the recorder web interface.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/tasks/web_task.h"
#include "config.h"
#include "slm_drive_config_private.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include <WiFi.h>
#include <new>
#include <ESPAsyncWebServer.h>
#include <Update.h>

#include "src/tasks/html_interface.h"
#include "src/tasks/state_task.h"
#include "src/global.h"
#include "src/services/settings_store.h"
#include "src/services/sd_files.h" // authorization gate for file ops
#include "src/services/task_helpers.h"
#include "src/services/calibration_service.h"
#include "src/services/calibration_report_service.h"
#include "src/services/watchdog_service.h"

#include "src/tasks/sd_task.h"
// Webserver must ONLY be enabled in READY (State Task enforces this).
// This task owns WiFi/AP and HTTP access.  WiFi/AP is toggled by Web ON/OFF;
// the AsyncWebServer listener is intentionally created and started once.
//
// Important lifecycle note:
// On the selected ESP32-S3 / Arduino / AsyncWebServer / AsyncTCP stack, the
// supported Web OFF lifecycle is to disable the SoftAP and clear application
// locks/state while keeping the port-80 listener allocated and started.
static AsyncWebServer *s_server = nullptr;
static bool s_server_routes_registered = false;
static bool s_server_listener_started = false;
static uint32_t s_web_start_cycle = 0u;
// s_enabled_requested is written by other tasks and polled by web_task_loop.
// It is volatile to prevent compiler caching of the single request flag.
// WiFi/AP/HTTP server state remains owned by web_task_loop.  s_started is
// only ever read/written inside web_task_loop, so it does not need volatile.
static volatile bool s_enabled_requested = false;
static bool s_started = false;
static volatile bool s_ota_active = false;
static volatile bool s_ota_ok = false;
static volatile bool s_ota_reboot_pending = false;
static char s_ota_error[48] = "";


static IPAddress s_ap_ip = AP_IP_ADDRESS;
static IPAddress s_gateway = AP_GATEWAY;
static IPAddress s_subnet = AP_SUBNET;


// --- Web single-client policy ---
// Only one client (IP) may access SD endpoints at a time. Session expires after inactivity.
static bool s_web_client_locked = false;
static uint32_t s_web_client_ip = 0u; // IPv4 packed
static uint32_t s_web_client_last_ms = 0u;

// Calibration is a maintenance/mechanical activity. A client must unlock the
// maintenance routes before access. Authorization remains valid for the
// current recorder Web session and is cleared when Web/Wi-Fi is restarted.
static bool s_cal_client_authorized = false;
static uint32_t s_cal_client_ip = 0u;

/**
 * Ip to u32 performs the web task operation represented by this function and
 * keeps the module state consistent with recorder ownership rules.
 *
 * Inputs: `ip`.
 * Returns: Requested numeric value.
 */
static uint32_t ip_to_u32(const IPAddress& ip) {
  return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | (uint32_t)ip[3];
}

/**
 * Handles web single client allow for the Web/WiFi support path without
 * changing recorder-core state ownership.
 *
 * Inputs: `req`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool web_single_client_allow(AsyncWebServerRequest* req) {
  const uint32_t now_ms = (uint32_t)millis();
  const uint32_t ip_u32 = ip_to_u32(req->client()->remoteIP());

  if (s_web_client_locked) {
    if ((now_ms - s_web_client_last_ms) > (uint32_t)WEB_SINGLE_CLIENT_TIMEOUT_MS) {
      s_web_client_locked = false;
    }
  }

  if (!s_web_client_locked) {
    s_web_client_locked = true;
    s_web_client_ip = ip_u32;
    s_web_client_last_ms = now_ms;
    return true;
  }

  if (ip_u32 == s_web_client_ip) {
    s_web_client_last_ms = now_ms;
    return true;
  }

  return false;
}

/**
 * Return whether the calibration password matches the stored registration.
 *
 * Inputs: `password`.
 * Returns: `true` when the password matches the recorder registration.
 */
static bool cal_password_matches_(const String& password){
  settings_t st;
  if((!settings_get(&st)) || (st.registration[0] == '\0')){
    return false;
  }
  return password == String(st.registration);
}

static bool cal_support_password_matches_(const String& password){
  settings_t st;
  if((!settings_get(&st)) || (st.registration[0] == '\0')){
    return false;
  }

  String expected = String(st.registration);
  String reversed = "";
  for(int i = expected.length() - 1; i >= 0; --i){
    reversed += expected.charAt((unsigned int)i);
  }
  return password == reversed;
}

/**
 * Authorize this client IP for calibration routes.
 *
 * Inputs: `req`.
 * Returns: None.
 */
static void cal_authorize_client_(AsyncWebServerRequest *req){
  s_cal_client_authorized = true;
  s_cal_client_ip = ip_to_u32(req->client()->remoteIP());
}

/**
 * Check whether this client IP has calibration authorization for this session.
 *
 * Inputs: `req`.
 * Returns: `true` when calibration access is authorized.
 */
static bool cal_client_authorized_(AsyncWebServerRequest *req){
  if(!s_cal_client_authorized){
    return false;
  }

  if(ip_to_u32(req->client()->remoteIP()) != s_cal_client_ip){
    return false;
  }

  return true;
}

/**
 * Send a calibration authorization error when the client is not unlocked.
 *
 * Inputs: `req`.
 * Returns: `true` when the request may continue; otherwise `false`.
 */
static bool cal_require_auth_(AsyncWebServerRequest *req){
  if(cal_client_authorized_(req)){
    return true;
  }

  req->send(403, "application/json", "{\"ok\":false,\"reason\":\"calibration_auth_required\"}");
  return false;
}

/** Append one C string as escaped JSON string content. */
static void web_json_append_escaped_(String& out, const char *value){
  if(value == nullptr){
    return;
  }
  static const char hex[] = "0123456789abcdef";
  const unsigned char *cursor = (const unsigned char *)value;
  while(*cursor != 0u){
    const unsigned char c = *cursor++;
    if((c == '"') || (c == '\\')){
      out += '\\';
      out += (char)c;
    } else if(c < 0x20u){
      out += "\\u00";
      out += hex[(c >> 4) & 0x0fu];
      out += hex[c & 0x0fu];
    } else {
      out += (char)c;
    }
  }
}

/** Send a JSON response that browsers and intermediaries must not cache. */
static void web_send_private_json_(AsyncWebServerRequest *request,
                                   int status,
                                   const String& body){
  AsyncWebServerResponse *response = request->beginResponse(status, "application/json", body);
  if(response == nullptr){
    request->send(500, "application/json", "{\"ok\":false,\"reason\":\"response_alloc\"}");
    return;
  }
  response->addHeader("Cache-Control", "no-store, max-age=0");
  response->addHeader("Pragma", "no-cache");
  response->addHeader("X-Content-Type-Options", "nosniff");
  request->send(response);
}

/** Return whether every compile-time Drive authorization value is present. */
static bool web_drive_config_available_(void){
#if SLM_DRIVE_CONFIG_ENABLED
  return (SLM_DRIVE_CLIENT_ID[0] != '\0') &&
         (SLM_DRIVE_CLIENT_SECRET[0] != '\0') &&
         (SLM_DRIVE_REFRESH_TOKEN[0] != '\0') &&
         (SLM_DRIVE_ROOT_FOLDER_ID[0] != '\0') &&
         (strcmp(SLM_DRIVE_TOKEN_URI, "https://oauth2.googleapis.com/token") == 0);
#else
  return false;
#endif
}

/** Build the Android bridge Drive authorization response. */
static String web_drive_config_json_(void){
  String out;
  out.reserve(640u);
  out += "{\"version\":1,\"client_id\":\"";
  web_json_append_escaped_(out, SLM_DRIVE_CLIENT_ID);
  out += "\",\"client_secret\":\"";
  web_json_append_escaped_(out, SLM_DRIVE_CLIENT_SECRET);
  out += "\",\"refresh_token\":\"";
  web_json_append_escaped_(out, SLM_DRIVE_REFRESH_TOKEN);
  out += "\",\"token_uri\":\"";
  web_json_append_escaped_(out, SLM_DRIVE_TOKEN_URI);
  out += "\",\"root_folder_id\":\"";
  web_json_append_escaped_(out, SLM_DRIVE_ROOT_FOLDER_ID);
  out += "\"}";
  return out;
}

static volatile bool s_web_sd_busy = false;
static uint32_t s_web_sd_busy_since_ms = 0u;

/**
 * Attempts to enter the Web SD critical section.
 *
 * Inputs: None.
 * Returns: `true` when SD access is reserved for the caller; otherwise `false`.
 */
static bool web_sd_try_begin(void) {
  const uint32_t now = (uint32_t)millis();

  if (s_web_sd_busy) {
    if ((now - s_web_sd_busy_since_ms) > (uint32_t)WEB_SD_BUSY_STALE_MS) {
      // Last-resort recovery.  Normal cleanup is owned by endpoint scope
      // destructors or by the download onDisconnect hook.
      s_web_sd_busy = false;
      s_web_sd_busy_since_ms = 0u;
    } else {
      return false;
    }
  }

  s_web_sd_busy = true;
  s_web_sd_busy_since_ms = now;
  return true;
}

/**
 * Leaves the Web SD critical section.
 *
 * Inputs: None.
 * Returns: None.
 */
static void web_sd_end(void) {
  s_web_sd_busy = false;
  s_web_sd_busy_since_ms = 0u;
}

struct WebSdBusyScope {
  bool engaged;

  /**
   * @brief Enter the web SD critical section for a scoped request.
   *
   * Inputs: None.
   * Returns: None.
   */
  WebSdBusyScope() : engaged(web_sd_try_begin()) {}

  /**
   * @brief Leave the web SD critical section when the scope exits.
   *
   * Inputs: None.
   * Returns: None.
   */
  ~WebSdBusyScope() { if (engaged) { web_sd_end(); } }
};

/**
 * Make ssid performs the web task operation represented by this function and
 * keeps the module state consistent with recorder ownership rules.
 *
 * Inputs: None.
 * Returns: Requested value.
 */
static String make_ssid(void){
  settings_t st;
  if(settings_get(&st) && st.registration[0]){
    return String(AP_SSID_PREFIX) + String(st.registration);
  }
  return String(AP_SSID_PREFIX) + String("RECORDER");
}

/**
 * Make password performs the web task operation represented by this function
 * and keeps the module state consistent with recorder ownership rules.
 *
 * Inputs: None.
 * Returns: Requested value.
 */
static String make_password(void){
  settings_t st;
  if(settings_get(&st) && st.wifi_password[0]){
    return String(st.wifi_password);
  }
  // If empty, create open AP (or set a default if you prefer).
  return String("");
}

/**
 * Content type from name performs the web task operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `filename`.
 * Returns: Pointer to the requested object or string; may be `nullptr` when unavailable.
 */
static const char* content_type_from_name(const String& filename){
  if(filename.endsWith(".htm") || filename.endsWith(".html")) return "text/html";
  if(filename.endsWith(".css")) return "text/css";
  if(filename.endsWith(".js"))  return "application/javascript";
  if(filename.endsWith(".json"))return "application/json";
  if(filename.endsWith(".png")) return "image/png";
  if(filename.endsWith(".jpg")) return "image/jpeg";
  if(filename.endsWith(".bin")) return "application/octet-stream";
  if(filename.endsWith(".txt")) return "text/plain";
  if(filename.endsWith(".log")) return "text/plain";
  return "application/octet-stream";
}

/**
 * Checks fixed-buffer formatting result.
 *
 * Inputs: `n`, `cap`.
 * Returns: `true` when snprintf succeeded without truncation.
 */
static bool web_snprintf_ok_(int n, size_t cap){
  return (n >= 0) && ((size_t)n < cap);
}

/**
 * Store a short OTA error reason for the final upload response.
 *
 * Inputs: `reason`.
 * Returns: None.
 */
static void ota_set_error_(const char *reason){
  if(reason == nullptr){
    reason = "failed";
  }

  size_t i = 0u;
  for(; (i < (sizeof(s_ota_error) - 1u)) && (reason[i] != '\0'); ++i){
    s_ota_error[i] = reason[i];
  }
  s_ota_error[i] = '\0';
}

/**
 * Return whether OTA update is allowed by the current power/status snapshot.
 *
 * Inputs: None.
 * Returns: `true` when USB power is present.
 */
static bool ota_usb_allowed_(void){
  const system_status_t st = state_task_get_status();
  return st.usb_present_valid && st.usb_present;
}

/**
 * Return whether the uploaded firmware filename is acceptable for Web OTA.
 *
 * Inputs: `filename`.
 * Returns: `true` for an application .bin file name that is not a merged bin.
 */
static bool ota_filename_allowed_(const String& filename){
  return filename.endsWith(".bin") && (filename.indexOf("merged") < 0);
}



static const char *cal_status_name_(calibration_status_t status){
  switch(status){
    case CAL_STATUS_VALID: return "valid";
    case CAL_STATUS_EXPIRED: return "expired";
    case CAL_STATUS_FAULT: return "fault";
    case CAL_STATUS_MISSING:
    default: return "missing";
  }
}

static const char *cal_fault_reason_name_(calibration_fault_reason_t reason){
  switch(reason){
    case CAL_FAULT_PLAUSIBILITY: return "plausibility";
    case CAL_FAULT_DELTA: return "delta";
    case CAL_FAULT_NONE:
    default: return "none";
  }
}

static const char *cal_face_name_(calibration_face_t face){
  switch(face){
    case CAL_FACE_PX: return "+X";
    case CAL_FACE_NX: return "-X";
    case CAL_FACE_PY: return "+Y";
    case CAL_FACE_NY: return "-Y";
    case CAL_FACE_PZ: return "+Z";
    case CAL_FACE_NZ: return "-Z";
    default: return "?";
  }
}

/**
 * Cal face valid json performs the web task operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `face_valid[CAL_FACE_COUNT]`.
 * Returns: Requested value.
 */
static String cal_face_valid_json_(const bool face_valid[CAL_FACE_COUNT]){
  String s = "[";
  for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){
    if(i > 0u) s += ",";
    s += face_valid[i] ? "true" : "false";
  }
  s += "]";
  return s;
}

static String cal_u32_array_json_(const uint32_t values[CAL_FACE_COUNT]){
  String s = "[";
  for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){
    if(i > 0u) s += ",";
    s += String((unsigned long)values[i]);
  }
  s += "]";
  return s;
}

static String cal_float_array_json_(const float values[CAL_FACE_COUNT]){
  String s = "[";
  for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){
    if(i > 0u) s += ",";
    char buf[24];
    const int n = snprintf(buf, sizeof(buf), "%.2f", values[i]);
    s += web_snprintf_ok_(n, sizeof(buf)) ? String(buf) : String("0");
  }
  s += "]";
  return s;
}

/**
 * Cal vec json performs the web task operation represented by this function
 * and keeps the module state consistent with recorder ownership rules.
 *
 * Inputs: `v`.
 * Returns: Requested value.
 */
static String cal_vec_json_(const calibration_vec_t& v){
  char buf[96];
  const int n = snprintf(buf, sizeof(buf),
                         "{\"x\":%.1f,\"y\":%.1f,\"z\":%.1f}",
                         v.x_mg, v.y_mg, v.z_mg);
  return web_snprintf_ok_(n, sizeof(buf)) ? String(buf) : String("{}");
}

static String cal_matrix_json_(const float matrix[9]){
  if(matrix == nullptr){
    return String("[]");
  }

  String s = "[";
  for(uint32_t i = 0u; i < 9u; ++i){
    if(i > 0u) s += ",";
    char buf[32];
    const int n = snprintf(buf, sizeof(buf), "%.6f", matrix[i]);
    s += web_snprintf_ok_(n, sizeof(buf)) ? String(buf) : String("0");
  }
  s += "]";
  return s;
}

/**
 * Cal face capture json performs the web task operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `face[CAL_FACE_COUNT]`.
 * Returns: Requested value.
 */
static String cal_face_capture_json_(const calibration_face_capture_t face[CAL_FACE_COUNT]){
  String s = "[";
  for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){
    if(i > 0u) s += ",";
    s += "{\"valid\":";
    s += face[i].valid ? "true" : "false";
    s += ",\"mean\":";
    s += cal_vec_json_(face[i].mean_mg);
    s += ",\"stddev\":";
    s += cal_vec_json_(face[i].stddev_mg);
    s += "}";
  }
  s += "]";
  return s;
}

/**
 * Cal record axis pos performs the web task operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `rec`, `axis`.
 * Returns: Requested value.
 */
static float cal_record_axis_pos_(const calibration_record_t& rec, uint32_t axis){
  switch(axis){
    case 0u: return rec.sensor.face[CAL_FACE_PX].mean_mg.x_mg;
    case 1u: return rec.sensor.face[CAL_FACE_PY].mean_mg.y_mg;
    case 2u: return rec.sensor.face[CAL_FACE_PZ].mean_mg.z_mg;
    default: return 0.0f;
  }
}

/**
 * Cal record axis neg performs the web task operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `rec`, `axis`.
 * Returns: Requested value.
 */
static float cal_record_axis_neg_(const calibration_record_t& rec, uint32_t axis){
  switch(axis){
    case 0u: return rec.sensor.face[CAL_FACE_NX].mean_mg.x_mg;
    case 1u: return rec.sensor.face[CAL_FACE_NY].mean_mg.y_mg;
    case 2u: return rec.sensor.face[CAL_FACE_NZ].mean_mg.z_mg;
    default: return 0.0f;
  }
}

/**
 * Cal record axis gain performs the web task operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `rec`, `axis`.
 * Returns: Requested value.
 */
static float cal_record_axis_gain_(const calibration_record_t& rec, uint32_t axis){
  switch(axis){
    case 0u: return rec.sensor.gain_x;
    case 1u: return rec.sensor.gain_y;
    case 2u: return rec.sensor.gain_z;
    default: return 0.0f;
  }
}

/**
 * Cal record axis offset performs the web task operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `rec`, `axis`.
 * Returns: Requested value.
 */
static float cal_record_axis_offset_(const calibration_record_t& rec, uint32_t axis){
  switch(axis){
    case 0u: return rec.sensor.offset_x_mg;
    case 1u: return rec.sensor.offset_y_mg;
    case 2u: return rec.sensor.offset_z_mg;
    default: return 0.0f;
  }
}

/**
 * Cal record axis result json performs the web task operation represented by
 * this function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `rec`.
 * Returns: Requested value.
 */
static String cal_record_axis_result_json_(const calibration_record_t& rec){
  String s = "[";
  for(uint32_t i = 0u; i < 3u; ++i){
    if(i > 0u) s += ",";
    char buf[192];
    const int n = snprintf(buf, sizeof(buf),
                           "{\"value_pos\":%.1f,\"value_neg\":%.1f,\"gain\":%.6f,\"offset\":%.1f}",
                           cal_record_axis_pos_(rec, i),
                           cal_record_axis_neg_(rec, i),
                           cal_record_axis_gain_(rec, i),
                           cal_record_axis_offset_(rec, i));
    s += web_snprintf_ok_(n, sizeof(buf)) ? String(buf) : String("{}");
  }
  s += "]";
  return s;
}

/**
 * Cal record date json performs the web task operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `rec`.
 * Returns: Requested value.
 */
static String rtc_date_json_(const rtc_datetime_t& dt){
  char buf[96];
  const int n = snprintf(buf, sizeof(buf),
                         "{\"year\":%u,\"month\":%u,\"day\":%u,\"hour\":%u,\"min\":%u,\"sec\":%u}",
                         (unsigned)dt.year,
                         (unsigned)dt.month,
                         (unsigned)dt.day,
                         (unsigned)dt.hour,
                         (unsigned)dt.min,
                         (unsigned)dt.sec);
  return web_snprintf_ok_(n, sizeof(buf)) ? String(buf) : String("{}");
}



static void cal_append_sensor_csv_(String& out, const char *role, const calibration_record_t& rec){
  if(role == nullptr){
    role = "unknown";
  }

  out += role; out += ",valid,"; out += rec.sensor.valid ? "1" : "0"; out += "\n";
  out += role; out += ",date,"; out += String((unsigned)rec.sensor.timestamp.year); out += "-";
  if(rec.sensor.timestamp.month < 10u) out += "0"; out += String((unsigned)rec.sensor.timestamp.month); out += "-";
  if(rec.sensor.timestamp.day < 10u) out += "0"; out += String((unsigned)rec.sensor.timestamp.day); out += "\n";
  out += role; out += ",time,";
  if(rec.sensor.timestamp.hour < 10u) out += "0"; out += String((unsigned)rec.sensor.timestamp.hour); out += ":";
  if(rec.sensor.timestamp.min < 10u) out += "0"; out += String((unsigned)rec.sensor.timestamp.min); out += ":";
  if(rec.sensor.timestamp.sec < 10u) out += "0"; out += String((unsigned)rec.sensor.timestamp.sec); out += "\n";
  out += role; out += ",temp_valid,"; out += rec.sensor.temperature_valid ? "1" : "0"; out += "\n";
  out += role; out += ",temp_c,"; out += String(rec.sensor.temperature_c, 2); out += "\n";
  out += role; out += ",temp_min_c,"; out += String(rec.sensor.temperature_min_c, 2); out += "\n";
  out += role; out += ",temp_max_c,"; out += String(rec.sensor.temperature_max_c, 2); out += "\n";
  out += role; out += ",gain_x,"; out += String(rec.sensor.gain_x, 6); out += "\n";
  out += role; out += ",gain_y,"; out += String(rec.sensor.gain_y, 6); out += "\n";
  out += role; out += ",gain_z,"; out += String(rec.sensor.gain_z, 6); out += "\n";
  out += role; out += ",offset_x_mg,"; out += String(rec.sensor.offset_x_mg, 1); out += "\n";
  out += role; out += ",offset_y_mg,"; out += String(rec.sensor.offset_y_mg, 1); out += "\n";
  out += role; out += ",offset_z_mg,"; out += String(rec.sensor.offset_z_mg, 1); out += "\n";
}

static void cal_append_installation_csv_(String& out, const char *role, const installation_calibration_t& inst){
  if(role == nullptr){
    role = "installation";
  }

  out += role; out += ",valid,"; out += inst.valid ? "1" : "0"; out += "\n";
  out += role; out += ",date,"; out += String((unsigned)inst.timestamp.year); out += "-";
  if(inst.timestamp.month < 10u) out += "0"; out += String((unsigned)inst.timestamp.month); out += "-";
  if(inst.timestamp.day < 10u) out += "0"; out += String((unsigned)inst.timestamp.day); out += "\n";
  out += role; out += ",time,";
  if(inst.timestamp.hour < 10u) out += "0"; out += String((unsigned)inst.timestamp.hour); out += ":";
  if(inst.timestamp.min < 10u) out += "0"; out += String((unsigned)inst.timestamp.min); out += ":";
  if(inst.timestamp.sec < 10u) out += "0"; out += String((unsigned)inst.timestamp.sec); out += "\n";
  out += role; out += ",mean_x_mg,"; out += String(inst.mean_mg.x_mg, 1); out += "\n";
  out += role; out += ",mean_y_mg,"; out += String(inst.mean_mg.y_mg, 1); out += "\n";
  out += role; out += ",mean_z_mg,"; out += String(inst.mean_mg.z_mg, 1); out += "\n";
  out += role; out += ",stddev_x_mg,"; out += String(inst.stddev_mg.x_mg, 3); out += "\n";
  out += role; out += ",stddev_y_mg,"; out += String(inst.stddev_mg.y_mg, 3); out += "\n";
  out += role; out += ",stddev_z_mg,"; out += String(inst.stddev_mg.z_mg, 3); out += "\n";
  for(uint32_t i = 0u; i < 9u; ++i){
    out += role; out += ",matrix_"; out += String((unsigned)i); out += ",";
    out += String(inst.matrix[i], 6); out += "\n";
  }
}

static String cal_export_csv_(){
  String out = "section,item,value\n";
  out += "config,sw," RECORDER_SOFTWARE_VERSION "\n";
  out += "config,hw," RECORDER_HARDWARE_VERSION "\n";
  out += "config,gain_delta_max,"; out += String(CALIBRATION_GAIN_DELTA_MAX, 6); out += "\n";
  out += "config,offset_delta_max_mg,"; out += String(CALIBRATION_OFFSET_DELTA_MAX_MG, 1); out += "\n";
  out += "config,temp_min_c,"; out += String(CALIBRATION_TEMP_MIN_C, 1); out += "\n";
  out += "config,temp_max_c,"; out += String(CALIBRATION_TEMP_MAX_C, 1); out += "\n";
  out += "config,temp_max_span_c,"; out += String(CALIBRATION_TEMP_MAX_SPAN_C, 1); out += "\n";
  out += "status,cal_status,"; out += cal_status_name_(calibration_service_status()); out += "\n";
  out += "status,fault_reason,"; out += cal_fault_reason_name_(calibration_service_fault_reason()); out += "\n";

  calibration_record_t rec = {};
  if(calibration_service_get_reference(&rec)){
    cal_append_sensor_csv_(out, "reference", rec);
  } else {
    out += "reference,valid,0\n";
  }
  if(calibration_service_get_active(&rec)){
    cal_append_sensor_csv_(out, "active", rec);
  } else {
    out += "active,valid,0\n";
  }
  if(calibration_service_get_candidate(&rec)){
    cal_append_sensor_csv_(out, "candidate", rec);
  } else {
    out += "candidate,valid,0\n";
  }

  installation_calibration_t inst = {};
  if(calibration_service_get_installation(&inst)){
    cal_append_installation_csv_(out, "installation", inst);
  } else {
    out += "installation,valid,0\n";
  }

  return out;
}

/**
 * Registers web routes for status, file management, calibration actions, and
 * embedded pages on the current server object.
 *
 * Inputs: None.
 * Returns: None.
 */
static void register_routes(){
  if(s_server == nullptr) return;

  if(s_server_routes_registered){
    return;
  }

  s_server->onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "not found");
  });

  s_server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", HTML_PAGE);
  });

  // Lightweight health-check endpoint for AP, DHCP, and HTTP reachability
  // without loading the full HTML page.
  s_server->on("/diag", HTTP_GET, [](AsyncWebServerRequest *request){
    char buf[256];
    const IPAddress ap_ip = WiFi.softAPIP();
    const int n = snprintf(buf, sizeof(buf),
                           "{\"ok\":true,"
                           "\"cycle\":%lu,"
                           "\"ms\":%lu,"
                           "\"heap\":%lu,"
                           "\"web_requested\":%s,"
                           "\"web_started\":%s,"
                           "\"listener_started\":%s,"
                           "\"ap_ip\":\"%u.%u.%u.%u\","
                           "\"stations\":%u}",
                           (unsigned long)s_web_start_cycle,
                           (unsigned long)millis(),
                           (unsigned long)ESP.getFreeHeap(),
                           s_enabled_requested ? "true" : "false",
                           s_started ? "true" : "false",
                           s_server_listener_started ? "true" : "false",
                           (unsigned)ap_ip[0],
                           (unsigned)ap_ip[1],
                           (unsigned)ap_ip[2],
                           (unsigned)ap_ip[3],
                           (unsigned)WiFi.softAPgetStationNum());
    if(!web_snprintf_ok_(n, sizeof(buf))){
      request->send(500, "application/json", "{\"ok\":false,\"error\":\"format_failed\"}");
      return;
    }
    request->send(200, "application/json", buf);
  });

  s_server->on("/api/ota", HTTP_POST,
    [](AsyncWebServerRequest *request){
      if(s_ota_ok){
        request->send(200, "text/plain", "Firmware update OK. Rebooting...");
        s_ota_reboot_pending = true;
      } else {
        const char *reason = (s_ota_error[0] != '\0') ? s_ota_error : "ota_failed";
        const int code = (strcmp(reason, "maintenance_auth_required") == 0) ? 403 : 500;
        request->send(code, "text/plain", reason);
      }

      s_ota_active = false;
    },
    [](AsyncWebServerRequest *request,
       String filename,
       size_t index,
       uint8_t *data,
       size_t len,
       bool final){

      if(index == 0u){
        s_ota_ok = false;
        s_ota_active = false;
        s_ota_error[0] = '\0';

        if(!cal_client_authorized_(request)){
          ota_set_error_("maintenance_auth_required");
          return;
        }

        if(!ota_usb_allowed_()){
          ota_set_error_("usb_required");
          return;
        }

        if(!ota_filename_allowed_(filename)){
          ota_set_error_("application_bin_required");
          return;
        }

        if(!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)){
          ota_set_error_("update_begin_failed");
          return;
        }

        s_ota_active = true;
      }

      if(!s_ota_active){
        return;
      }

      if((len > 0u) && (Update.write(data, len) != len)){
        Update.abort();
        s_ota_active = false;
        ota_set_error_("update_write_failed");
        return;
      }

      if(final){
        if(Update.end(true)){
          s_ota_ok = true;
        } else {
          Update.abort();
          s_ota_active = false;
          ota_set_error_("update_end_failed");
        }
      }
    });


  // API endpoints used by the embedded HTML interface.
  s_server->on("/api/slm-drive-config", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!web_single_client_allow(request)){
      web_send_private_json_(request, 409, "{\"ok\":false,\"reason\":\"busy\"}");
      return;
    }
    if(!web_drive_config_available_()){
      web_send_private_json_(request, 503, "{\"ok\":false,\"reason\":\"drive_not_configured\"}");
      return;
    }
    web_send_private_json_(request, 200, web_drive_config_json_());
  });

  s_server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    const system_status_t st = state_task_get_status();
    const bool recording = (st.state == ST_RECORDING) || (st.state == ST_STARTING) || (st.state == ST_STOPPING);
    char buf[128];
    const int n = snprintf(buf, sizeof(buf),
                           "{\"battery\":%u,\"recording\":%s}",
                           (unsigned)(st.battery_percent_valid ? st.battery_percent : 0u),
                           recording ? "true" : "false");
    if(!web_snprintf_ok_(n, sizeof(buf))){
      request->send(500, "application/json", "{\"error\":\"format_failed\"}");
      return;
    }
    request->send(200, "application/json", buf);
  });

  s_server->on("/api/watchdog", HTTP_GET, [](AsyncWebServerRequest *request){
    watchdog_fault_info_t info = {};
    if(!watchdog_get_fault_info(&info)){
      request->send(200, "application/json", "{\"available\":false}");
      return;
    }

    String out = "{";
    out += "\"available\":true";
    out += ",\"active\":";
    out += info.active ? "true" : "false";
    out += ",\"source\":\"";
    out += watchdog_source_name(info.source);
    out += "\"";
    out += ",\"age_ms\":";
    out += String((unsigned long)info.age_ms);
    out += ",\"age_state_ms\":";
    out += String((unsigned long)info.ages_ms[WD_STATE]);
    out += ",\"age_sd_ms\":";
    out += String((unsigned long)info.ages_ms[WD_SD]);
    out += ",\"age_record_ms\":";
    out += String((unsigned long)info.ages_ms[WD_RECORD]);
    out += ",\"age_web_ms\":";
    out += String((unsigned long)info.ages_ms[WD_WEB]);
    out += ",\"recorder_state\":";
    out += String((unsigned long)info.recorder_state);
    out += ",\"last_error\":";
    out += String((long)info.last_error);
    out += ",\"web_active\":";
    out += info.web_active ? "true" : "false";
    out += ",\"usb_present\":";
    out += info.usb_present ? "true" : "false";
    out += ",\"sd_present\":";
    out += info.sd_present ? "true" : "false";
    out += ",\"heap\":";
    out += String((unsigned long)info.heap);
    out += ",\"min_heap\":";
    out += String((unsigned long)info.min_heap);
    out += "}";
    request->send(200, "application/json", out);
  });

  s_server->on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request){
    const bool present = (sd_error_get() != ERR_SD_NO_CARD);
    uint64_t sd_total_bytes = 0u;
    uint64_t sd_free_bytes = 0u;
    const bool space_ok = sd_files_get_space(&sd_total_bytes, &sd_free_bytes);
    const uint32_t sd_size_mb = (uint32_t)(sd_total_bytes / (1024ULL * 1024ULL));
    const uint32_t sd_free_mb = (uint32_t)(sd_free_bytes / (1024ULL * 1024ULL));
    char buf[160];
    const int n = snprintf(buf, sizeof(buf),
                           "{\"present\":%s,\"size_mb\":%lu,\"free_mb\":%lu}",
                           (present && space_ok) ? "true" : "false",
                           (unsigned long)sd_size_mb,
                           (unsigned long)sd_free_mb);
    if(!web_snprintf_ok_(n, sizeof(buf))){
      request->send(500, "application/json", "{\"error\":\"format_failed\"}");
      return;
    }
    request->send(200, "application/json", buf);
  });

  s_server->on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!web_single_client_allow(request)) { request->send(409, "text/plain", "BUSY"); return; }
    WebSdBusyScope _sdscope;
    if (!_sdscope.engaged) { request->send(409, "text/plain", "BUSY"); return; }

    static char json[SD_FILE_LIST_JSON_MAX];
    uint32_t out_len = 0u;
    const bool ok = sd_files_list_json("/", json, sizeof(json), &out_len);

    if (!ok) {
      request->send(500, "application/json", "{\"error\":\"sd_list_failed\"}");
      return;
    }

    if (out_len <= 2u) {
      request->send(200, "application/json", "{\"files\":[]}");
      return;
    }
    String out = String("{\"files\":") + String(json) + String("}");
    request->send(200, "application/json", out);
});

  s_server->on("/api/logbook/files", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!web_single_client_allow(request)) { request->send(409, "text/plain", "BUSY"); return; }
    WebSdBusyScope _sdscope;
    if (!_sdscope.engaged) { request->send(409, "text/plain", "BUSY"); return; }

    static char json[SD_FILE_LIST_JSON_MAX];
    uint32_t out_len = 0u;
    const bool ok = sd_files_list_json("/logbook", json, sizeof(json), &out_len);
    if (!ok) {
      request->send(500, "application/json", "{\"ok\":false,\"reason\":\"sd_list_failed\"}");
      return;
    }
    String out = String("{\"ok\":true,\"files\":") + String(json) + String("}");
    request->send(200, "application/json", out);
  });

  // Sequential download context.  The SD task opens the file once in
  // sd_files_download_begin(), then the HTTP chunk callback reads the next
  // sequential bytes with sd_files_download_read().
struct WebDownloadCtx {
  String   path;
  uint32_t size;
  uint32_t sent;
  bool     released;

  WebDownloadCtx(const String& p, uint32_t s)
      : path(p), size(s), sent(0u), released(false) {}

  /**
   * Close the active sequential download and release the Web SD busy flag.
   *
   * Inputs: None.
   * Returns: None.
   */
  void release_once(void) {
    if (!released) {
      released = true;
      (void)sd_files_download_end();
      web_sd_end();
    }
  }
};

s_server->on("/api/download", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!web_single_client_allow(request)) {
      request->send(409, "text/plain", "BUSY");
      return;
    }

    if (!web_sd_try_begin()) {
      request->send(409, "text/plain", "BUSY");
      return;
    }

    // From this point onward, every early return must release the SD busy lock.
    if (!sd_files_is_authorized()) {
      web_sd_end();
      request->send(403, "text/plain", "not authorized");
      return;
    }

    if (!request->hasParam("file")) {
      web_sd_end();
      request->send(400, "text/plain", "missing file");
      return;
    }

    String file = request->getParam("file")->value();
    String path = file;
    if (!path.startsWith("/")) {
      path = String("/") + path;
    }

    const int slash = file.lastIndexOf('/');
    if (slash >= 0) {
      file = file.substring(slash + 1);
    }

    uint32_t file_size = 0u;
    if (!sd_files_download_begin(path.c_str(), &file_size)) {
      web_sd_end();
      request->send(404, "text/plain", "not found");
      return;
    }

    WebDownloadCtx *ctx = new (std::nothrow) WebDownloadCtx(path, file_size);
    if (ctx == nullptr) {
      (void)sd_files_download_end();
      web_sd_end();
      request->send(500, "text/plain", "oom");
      return;
    }

    AsyncWebServerResponse *response = request->beginResponse(
      content_type_from_name(file),
      (size_t)file_size,
      [ctx](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        (void)index;

        // End of stream.  Close the sequential download immediately; the
        // disconnect hook below still owns deletion of the context object.
        if (ctx->sent >= ctx->size) {
          ctx->release_once();
          return 0;
        }

        const uint32_t remain  = ctx->size - ctx->sent;
        const uint32_t to_read = (remain < (uint32_t)maxLen)
                                   ? remain
                                   : (uint32_t)maxLen;

        uint32_t got = 0u;
        const bool ok = sd_files_download_read(buffer, to_read, &got);

        if ((!ok) || (got == 0u)) {
          // Signal end-of-stream/read failure and release the open file now.
          ctx->release_once();
          return 0;
        }

        ctx->sent += got;
        // The last bytes are already in the HTTP response buffer, so the SD
        // file and busy flag can be released immediately.  Waiting solely for
        // the TCP disconnect can leave list/report requests transiently BUSY.
        if (ctx->sent >= ctx->size) {
          ctx->release_once();
        }
        return (size_t)got;
      });

    if (response == nullptr) {
      ctx->release_once();
      delete ctx;
      request->send(500, "text/plain", "response_alloc");
      return;
    }

    // Critical cleanup path: this is expected to run for normal completion and
    // for aborted client transfers.  web_sd_try_begin() also has a stale-lock
    // timeout as a final recovery guard.
    request->onDisconnect([ctx]() {
      ctx->release_once();
      delete ctx;
    });

    response->addHeader("Content-Disposition",
        (String("attachment; filename=\"") + file + String("\"")).c_str());

    request->send(response);
  });



  // Store a browser-generated companion flight-analysis log next to the source
  // recording file. The request supplies the .bin basename and text content;
  // the recorder derives the .log name and accepts only root-level recording
  // basenames so this endpoint cannot write arbitrary SD paths.
  s_server->on("/api/flightlog", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!web_single_client_allow(request)) { request->send(409, "text/plain", "BUSY"); return; }
    WebSdBusyScope _sdscope;
    if(!_sdscope.engaged) { request->send(409, "text/plain", "BUSY"); return; }

    if(!sd_files_is_authorized()){
      request->send(403, "application/json", "{\"ok\":false,\"reason\":\"not_authorized\"}");
      return;
    }

    if((!request->hasParam("file", true)) || (!request->hasParam("log", true))){
      request->send(400, "application/json", "{\"ok\":false,\"reason\":\"missing_parameter\"}");
      return;
    }

    String file = request->getParam("file", true)->value();
    const int slash = file.lastIndexOf('/');
    if(slash >= 0){
      file = file.substring(slash + 1);
    }

    if((file.length() == 0) || (!file.endsWith(".bin")) || (file.indexOf('/') >= 0)){
      request->send(400, "application/json", "{\"ok\":false,\"reason\":\"invalid_file\"}");
      return;
    }

    String log_name = file.substring(0, file.length() - 4) + String(".log");
    if(log_name.length() >= FILENAME_MAX_LENGTH){
      request->send(400, "application/json", "{\"ok\":false,\"reason\":\"name_too_long\"}");
      return;
    }

    String log = request->getParam("log", true)->value();
    if(log.length() > FLIGHT_LOG_TEXT_MAX_BYTES){
      request->send(413, "application/json", "{\"ok\":false,\"reason\":\"log_too_large\"}");
      return;
    }

    const String path = String("/") + log_name;
    const bool ok = sd_files_write_text_file(path.c_str(), log.c_str(), (uint32_t)log.length());
    request->send(ok ? 200 : 500,
                  "application/json",
                  ok ? "{\"ok\":true}" : "{\"ok\":false,\"reason\":\"write_failed\"}");
  });

  s_server->on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!web_single_client_allow(request)) { request->send(409, "text/plain", "BUSY"); return; }
    WebSdBusyScope _sdscope;
    if (!_sdscope.engaged) { request->send(409, "text/plain", "BUSY"); return; }


    if(!sd_files_is_authorized()){
      request->send(403, "application/json", "{\"ok\":false,\"reason\":\"not_authorized\"}");
      return;
    }
    // Browser UI sends ?file=... as query parameter.
    // AsyncWebServer: for POST query params may appear as "file" (query string)
    // or "file" in body; accept either.
    if(!request->hasParam("file") && !request->hasParam("file", true)){
      request->send(400, "application/json", "{\"ok\":false}");
      return;
    }
    String file = request->hasParam("file") ? request->getParam("file")->value()
                                            : request->getParam("file", true)->value();
    String path = file;
    if(!path.startsWith("/")) path = String("/") + path;
    bool ok = sd_files_delete(path.c_str());
    request->send(ok ? 200 : 403, "application/json", ok ? "{\"ok\":true,\"archived\":true}" : "{\"ok\":false}");
  });




  s_server->on("/api/reports/files", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    if(!web_single_client_allow(request)) { request->send(409, "text/plain", "BUSY"); return; }
    WebSdBusyScope _sdscope;
    if(!_sdscope.engaged) { request->send(409, "text/plain", "BUSY"); return; }

    if(!sd_files_is_authorized()){
      request->send(403, "application/json", "{\"ok\":false,\"reason\":\"not_authorized\"}");
      return;
    }

    static char json[SD_FILE_LIST_JSON_MAX];
    uint32_t out_len = 0u;
    const bool ok = sd_files_list_json("/calibration_reports", json, sizeof(json), &out_len);
    if(!ok){
      request->send(500, "application/json", "{\"ok\":false,\"reason\":\"sd_list_failed\"}");
      return;
    }

    String out = String("{\"ok\":true,\"files\":") + String(json) + String("}");
    request->send(200, "application/json", out);
  });

  s_server->on("/api/reports/archive", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    if(!web_single_client_allow(request)) { request->send(409, "text/plain", "BUSY"); return; }
    WebSdBusyScope _sdscope;
    if(!_sdscope.engaged) { request->send(409, "text/plain", "BUSY"); return; }

    if(!sd_files_is_authorized()){
      request->send(403, "application/json", "{\"ok\":false,\"reason\":\"not_authorized\"}");
      return;
    }

    if(!request->hasParam("file") && !request->hasParam("file", true)){
      request->send(400, "application/json", "{\"ok\":false,\"reason\":\"missing_file\"}");
      return;
    }

    const String file = request->hasParam("file") ? request->getParam("file")->value()
                                                  : request->getParam("file", true)->value();
    String path;
    if(file.startsWith("/calibration_reports/")){
      path = file;
    } else if(file.indexOf('/') < 0){
      path = String("/calibration_reports/") + file;
    } else {
      request->send(400, "application/json", "{\"ok\":false,\"reason\":\"bad_path\"}");
      return;
    }

    if((path.indexOf("..") >= 0) ||
       (path.length() <= String("/calibration_reports/").length())){
      request->send(400, "application/json", "{\"ok\":false,\"reason\":\"bad_path\"}");
      return;
    }

    const bool ok = sd_files_delete(path.c_str());
    request->send(ok ? 200 : 500,
                  "application/json",
                  ok ? "{\"ok\":true,\"archived\":true}"
                     : "{\"ok\":false,\"reason\":\"archive_failed\"}");
  });

  s_server->on("/api/processed/files", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    if(!web_single_client_allow(request)) { request->send(409, "text/plain", "BUSY"); return; }
    WebSdBusyScope _sdscope;
    if(!_sdscope.engaged) { request->send(409, "text/plain", "BUSY"); return; }

    if(!sd_files_is_authorized()){
      request->send(403, "application/json", "{\"ok\":false,\"reason\":\"not_authorized\"}");
      return;
    }

    static char json[SD_FILE_LIST_JSON_MAX];
    uint32_t out_len = 0u;
    const bool ok = sd_files_list_json("/processed", json, sizeof(json), &out_len);
    if(!ok){
      request->send(500, "application/json", "{\"ok\":false,\"reason\":\"sd_list_failed\"}");
      return;
    }

    String out = String("{\"ok\":true,\"files\":") + String(json) + String("}");
    request->send(200, "application/json", out);
  });

  s_server->on("/api/processed/delete", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    if(!web_single_client_allow(request)) { request->send(409, "text/plain", "BUSY"); return; }
    WebSdBusyScope _sdscope;
    if(!_sdscope.engaged) { request->send(409, "text/plain", "BUSY"); return; }

    if(!sd_files_is_authorized()){
      request->send(403, "application/json", "{\"ok\":false,\"reason\":\"not_authorized\"}");
      return;
    }

    if(!request->hasParam("file") && !request->hasParam("file", true)){
      request->send(400, "application/json", "{\"ok\":false,\"reason\":\"missing_file\"}");
      return;
    }

    const String file = request->hasParam("file") ? request->getParam("file")->value()
                                                  : request->getParam("file", true)->value();
    String path;
    if(file.startsWith("/processed/")){
      path = file;
    } else if(file.indexOf('/') < 0){
      path = String("/processed/") + file;
    } else {
      request->send(400, "application/json", "{\"ok\":false,\"reason\":\"bad_path\"}");
      return;
    }

    if((path.indexOf("..") >= 0) || (path.length() <= String("/processed/").length())){
      request->send(400, "application/json", "{\"ok\":false,\"reason\":\"bad_path\"}");
      return;
    }

    const bool ok = sd_files_delete_processed(path.c_str());
    request->send(ok ? 200 : 500,
                  "application/json",
                  ok ? "{\"ok\":true,\"deleted\":true}" : "{\"ok\":false,\"reason\":\"delete_failed\"}");
  });

  s_server->on("/api/cal/auth", HTTP_POST, [](AsyncWebServerRequest *request){
    String password = "";
    if(request->hasParam("password", true)){
      password = request->getParam("password", true)->value();
    } else if(request->hasParam("password")){
      password = request->getParam("password")->value();
    }

    if(!cal_password_matches_(password)){
      request->send(403, "application/json", "{\"ok\":false,\"reason\":\"bad_password\"}");
      return;
    }

    cal_authorize_client_(request);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  s_server->on("/api/cal/support_auth", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;

    String password = "";
    if(request->hasParam("support", true)){
      password = request->getParam("support", true)->value();
    } else if(request->hasParam("support")){
      password = request->getParam("support")->value();
    }

    if(!cal_support_password_matches_(password)){
      request->send(403, "application/json", "{\"ok\":false,\"reason\":\"bad_support_code\"}");
      return;
    }

    request->send(200, "application/json", "{\"ok\":true}");
  });

  s_server->on("/api/cal/status", HTTP_GET, [](AsyncWebServerRequest *request){
    // /api/cal/status is polled periodically by the maintenance page.  When a
    // client is already authorized, let this poll keep the maintenance
    // authorization alive while the user navigates between maintenance pages
    // such as About, Recorder Calibration, and Installation Calibration.
    (void)cal_client_authorized_(request);

    calibration_service_refresh_status();
    const calibration_status_t status = calibration_service_status();
    const bool recording_allowed = calibration_service_is_recording_allowed();
    calibration_record_t active = {};
    const bool active_ok = calibration_service_get_active(&active);
    calibration_record_t reference = {};
    const bool reference_ok = calibration_service_get_reference(&reference);
    char report_path[SD_STORAGE_PATH_MAX];
    const bool recorder_report_available = calibration_report_get_last_recorder_path(report_path, sizeof(report_path));

    String out = "{";
    out += "\"status\":\"";
    out += cal_status_name_(status);
    out += "\"";
    out += ",\"recording_allowed\":";
    out += recording_allowed ? "true" : "false";
    out += ",\"session_active\":";
    out += calibration_session_active() ? "true" : "false";
    out += ",\"sensor_valid\":";
    out += (active_ok && active.sensor.valid) ? "true" : "false";
    out += ",\"reference_available\":";
    out += (reference_ok && reference.sensor.valid) ? "true" : "false";
    out += ",\"active_available\":";
    out += (active_ok && active.sensor.valid) ? "true" : "false";
    out += ",\"sensor_date\":";
    out += (active_ok && active.sensor.valid) ? rtc_date_json_(active.sensor.timestamp) : "{}";
    out += ",\"sensor_temperature_valid\":";
    out += (active_ok && active.sensor.temperature_valid) ? "true" : "false";
    out += ",\"sensor_temperature_c\":";
    out += (active_ok && active.sensor.temperature_valid) ? String(active.sensor.temperature_c, 2) : "null";
    out += ",\"fault_reason\":\"";
    out += cal_fault_reason_name_(calibration_service_fault_reason());
    out += "\"";
    out += ",\"installation_valid\":";
    out += (active_ok && active.installation.valid) ? "true" : "false";
    out += ",\"installation_date\":";
    out += (active_ok && active.installation.valid) ? rtc_date_json_(active.installation.timestamp) : "{}";
    out += ",\"installation_session_active\":";
    out += calibration_installation_session_active() ? "true" : "false";
    out += ",\"recorder_report_available\":";
    out += recorder_report_available ? "true" : "false";
    out += "}";
    request->send(200, "application/json", out);
  });

  s_server->on("/api/cal/start", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    const system_status_t st = state_task_get_status();
    const bool recording = (st.state == ST_RECORDING) || (st.state == ST_STARTING) || (st.state == ST_STOPPING);
    if(recording){
      request->send(409, "application/json", "{\"ok\":false,\"reason\":\"recording\"}");
      return;
    }

    const bool ok = calibration_session_start();
    request->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  s_server->on("/api/cal/cancel", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    calibration_session_cancel();
    request->send(200, "application/json", "{\"ok\":true}");
  });

  s_server->on("/api/cal/sample", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    calibration_sample_status_t st = {};
    if(!calibration_session_get_status(&st)){
      request->send(500, "application/json", "{\"ok\":false,\"reason\":\"status_failed\"}");
      return;
    }

    calibration_record_t current_result = {};
    const bool can_compute = calibration_session_can_compute();
    const bool result_ok = can_compute && calibration_session_compute(&current_result);

    calibration_record_t stored_result = {};
    const bool nvs_result_ok = calibration_service_get_active(&stored_result);

    String out = "{";
    out += "\"ok\":true";
    out += ",\"active\":";
    out += st.session_active ? "true" : "false";
    out += ",\"done\":";
    out += result_ok ? "true" : "false";
    out += ",\"stable\":";
    out += st.stable ? "true" : "false";
    out += ",\"candidate_valid\":";
    out += st.candidate_valid ? "true" : "false";
    out += ",\"candidate_face\":\"";
    out += cal_face_name_(st.candidate_face);
    out += "\"";
    out += ",\"current_face_valid\":";
    out += st.current_face_valid ? "true" : "false";
    out += ",\"current_face\":\"";
    out += cal_face_name_(st.current_face);
    out += "\"";
    out += ",\"samples\":";
    out += String((unsigned long)st.current_face_samples);
    out += ",\"window_samples\":";
    out += String((unsigned long)st.sample_count);
    out += ",\"total_samples\":";
    out += String((unsigned long)st.total_samples);
    out += ",\"valid_windows\":";
    out += String((unsigned long)st.valid_windows);
    out += ",\"total_updates\":";
    out += String((unsigned long)st.total_updates);
    out += ",\"last_update_age_ms\":";
    out += String((unsigned long)st.last_update_age_ms);
    out += ",\"last_update_sample\":";
    out += String((unsigned long)st.last_update_sample);
    out += ",\"current_face_last_update_sample\":";
    if(st.current_face_valid){
      out += String((unsigned long)st.face_last_update_sample[(uint32_t)st.current_face]);
    } else {
      out += "0";
    }
    out += ",\"face_updates\":";
    out += cal_u32_array_json_(st.face_updates);
    out += ",\"face_last_update_age_ms\":";
    out += cal_u32_array_json_(st.face_last_update_age_ms);
    out += ",\"face_last_update_sample\":";
    out += cal_u32_array_json_(st.face_last_update_sample);
    out += ",\"face_quality\":";
    out += cal_float_array_json_(st.face_quality_mg);
    out += ",\"mean\":";
    out += cal_vec_json_(st.mean_mg);
    out += ",\"stddev\":";
    out += cal_vec_json_(st.stddev_mg);
    out += ",\"current_stddev_valid\":";
    out += st.current_stddev_valid ? "true" : "false";
    out += ",\"current_stddev\":";
    out += cal_vec_json_(st.current_stddev_mg);
    out += ",\"sample_period_ms\":";
    out += String((unsigned long)CALIBRATION_SAMPLE_PERIOD_MS);
    out += ",\"stability_stddev_max_mg\":";
    out += String(CALIBRATION_STABILITY_STDDEV_MAX_MG, 2);
    out += ",\"face_valid\":";
    out += cal_face_valid_json_(st.face_valid);
    out += ",\"session_face\":";
    out += cal_face_capture_json_(st.face);
    out += ",\"stored_loaded\":";
    out += st.stored_loaded ? "true" : "false";
    out += ",\"stored_face\":";
    out += cal_face_capture_json_(st.stored_face);
    out += ",\"temperature_available\":";
    out += st.temperature_available ? "true" : "false";
    out += ",\"temperature_in_range\":";
    out += st.temperature_in_range ? "true" : "false";
    out += ",\"temperature_stable\":";
    out += st.temperature_stable ? "true" : "false";
    out += ",\"temperature_fault\":";
    out += st.temperature_fault ? "true" : "false";
    out += ",\"temperature_c\":";
    out += st.temperature_available ? String(st.temperature_c, 2) : "null";
    out += ",\"temperature_min_c\":";
    out += st.temperature_available ? String(st.temperature_min_c, 2) : "null";
    out += ",\"temperature_max_c\":";
    out += st.temperature_available ? String(st.temperature_max_c, 2) : "null";
    out += ",\"temperature_allowed_min_c\":";
    out += String(CALIBRATION_TEMP_MIN_C, 1);
    out += ",\"temperature_allowed_max_c\":";
    out += String(CALIBRATION_TEMP_MAX_C, 1);
    out += ",\"temperature_allowed_span_c\":";
    out += String(CALIBRATION_TEMP_MAX_SPAN_C, 1);
    out += ",\"result_available\":";
    out += result_ok ? "true" : "false";
    out += ",\"result\":";
    out += result_ok ? cal_record_axis_result_json_(current_result) : "[]";
    out += ",\"nvs_result_available\":";
    out += nvs_result_ok ? "true" : "false";
    out += ",\"nvs_date\":";
    out += nvs_result_ok ? rtc_date_json_(stored_result.sensor.timestamp) : "{}";
    out += ",\"nvs_result\":";
    out += nvs_result_ok ? cal_record_axis_result_json_(stored_result) : "[]";
    out += "}";
    request->send(200, "application/json", out);
  });

  s_server->on("/api/cal/accept", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    const bool ok = calibration_session_accept_candidate();
    request->send(ok ? 200 : 409, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  s_server->on("/api/cal/save", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;

    recorder_calibration_report_data_t report = {};
    report.active_before_available = calibration_service_get_active(&report.active_before);
    report.reference_before_available = calibration_service_get_reference(&report.reference_before);

    calibration_record_t rec = {};
    calibration_save_result_t result = CAL_SAVE_NOT_READY;
    const bool saved = calibration_session_save_with_result(&rec, &result);

    report.result = result;
    report.candidate_available = rec.sensor.valid;
    if(report.candidate_available){
      report.candidate = rec;
    }
    report.active_after_available = calibration_service_get_active(&report.active_after);
    report.reference_after_available = calibration_service_get_reference(&report.reference_after);
    report.recorder_valid_after = (calibration_service_status() == CAL_STATUS_VALID);

    char report_path[SD_STORAGE_PATH_MAX];
    report_path[0] = '\0';
    const bool report_written = calibration_report_write_recorder(&report, report_path, sizeof(report_path));

    if(!saved){
      String out = "{\"ok\":";
      const bool handled_save_outcome = ((result == CAL_SAVE_NEED_REPEAT) ||
                                         (result == CAL_SAVE_DELTA_FAULT));
      out += handled_save_outcome ? "true" : "false";
      out += ",\"saved\":false,\"reason\":\"";
      out += calibration_save_result_name(result);
      out += "\"";
      if(result == CAL_SAVE_DELTA_FAULT){
        out += ",\"message\":\"significant_calibration_drift_calibration_rejected\"";
        out += ",\"candidate_saved\":true";
        out += ",\"session_closed\":true";
      }
      out += ",\"report_written\":";
      out += report_written ? "true" : "false";
      if(report_written){
        out += ",\"report_path\":\"";
        out += report_path;
        out += "\"";
      }
      out += "}";
      const int code = handled_save_outcome ? 200 : 409;
      request->send(code, "application/json", out);
      return;
    }

    char buf[640];
    const int n = snprintf(buf, sizeof(buf),
                           "{\"ok\":true,\"saved\":true,\"message\":\"recorder_calibration_saved\","
                           "\"temperature_c\":%.2f,"
                           "\"gain\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f},"
                           "\"offset_mg\":{\"x\":%.1f,\"y\":%.1f,\"z\":%.1f},"
                           "\"report_written\":%s,\"report_path\":\"%s\"}",
                           rec.sensor.temperature_c,
                           rec.sensor.gain_x, rec.sensor.gain_y, rec.sensor.gain_z,
                           rec.sensor.offset_x_mg, rec.sensor.offset_y_mg, rec.sensor.offset_z_mg,
                           report_written ? "true" : "false",
                           report_written ? report_path : "");
    if(!web_snprintf_ok_(n, sizeof(buf))){
      request->send(500, "application/json", "{\"ok\":false,\"reason\":\"format_failed\"}");
      return;
    }
    request->send(200, "application/json", buf);
  });

  s_server->on("/api/cal/export", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;

    String password = "";
    if(request->hasParam("support")){
      password = request->getParam("support")->value();
    }
    if(!cal_support_password_matches_(password)){
      request->send(403, "text/plain", "bad_support_code");
      return;
    }

    String csv = cal_export_csv_();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/csv", csv);
    if(response == nullptr){
      request->send(500, "text/plain", "response_alloc");
      return;
    }
    response->addHeader("Content-Disposition", "attachment; filename=slm_support_data.csv");
    request->send(response);
  });

  s_server->on("/api/cal/report/recorder/latest", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;

    if(!web_single_client_allow(request)){
      request->send(409, "text/plain", "BUSY");
      return;
    }

    if(!web_sd_try_begin()){
      request->send(409, "text/plain", "BUSY");
      return;
    }

    char path_buf[SD_STORAGE_PATH_MAX];
    if(!calibration_report_get_last_recorder_path(path_buf, sizeof(path_buf))){
      web_sd_end();
      request->send(404, "text/plain", "no recorder calibration report available");
      return;
    }

    String path = String(path_buf);
    String file = path;
    const int slash = file.lastIndexOf('/');
    if(slash >= 0){
      file = file.substring(slash + 1);
    }

    uint32_t file_size = 0u;
    if(!sd_files_download_begin(path.c_str(), &file_size)){
      web_sd_end();
      request->send(404, "text/plain", "not found");
      return;
    }

    WebDownloadCtx *ctx = new (std::nothrow) WebDownloadCtx(path, file_size);
    if(ctx == nullptr){
      (void)sd_files_download_end();
      web_sd_end();
      request->send(500, "text/plain", "oom");
      return;
    }

    AsyncWebServerResponse *response = request->beginResponse(
      "text/plain",
      (size_t)file_size,
      [ctx](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        (void)index;

        if(ctx->sent >= ctx->size){
          ctx->release_once();
          return 0;
        }

        const uint32_t remain = ctx->size - ctx->sent;
        const uint32_t to_read = (remain < (uint32_t)maxLen) ? remain : (uint32_t)maxLen;
        uint32_t got = 0u;
        const bool ok = sd_files_download_read(buffer, to_read, &got);
        if((!ok) || (got == 0u)){
          ctx->release_once();
          return 0;
        }

        ctx->sent += got;
        return (size_t)got;
      });

    if(response == nullptr){
      ctx->release_once();
      delete ctx;
      request->send(500, "text/plain", "response_alloc");
      return;
    }

    request->onDisconnect([ctx](){
      ctx->release_once();
      delete ctx;
    });

    response->addHeader("Content-Disposition",
        (String("attachment; filename=\"") + file + String("\"")).c_str());
    request->send(response);
  });

  s_server->on("/api/cal/report/installation/latest", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;

    if(!web_single_client_allow(request)){
      request->send(409, "text/plain", "BUSY");
      return;
    }

    if(!web_sd_try_begin()){
      request->send(409, "text/plain", "BUSY");
      return;
    }

    char path_buf[SD_STORAGE_PATH_MAX];
    if(!calibration_report_get_last_installation_path(path_buf, sizeof(path_buf))){
      web_sd_end();
      request->send(404, "text/plain", "no installation calibration report available");
      return;
    }

    String path = String(path_buf);
    String file = path;
    const int slash = file.lastIndexOf('/');
    if(slash >= 0){
      file = file.substring(slash + 1);
    }

    uint32_t file_size = 0u;
    if(!sd_files_download_begin(path.c_str(), &file_size)){
      web_sd_end();
      request->send(404, "text/plain", "not found");
      return;
    }

    WebDownloadCtx *ctx = new (std::nothrow) WebDownloadCtx(path, file_size);
    if(ctx == nullptr){
      (void)sd_files_download_end();
      web_sd_end();
      request->send(500, "text/plain", "oom");
      return;
    }

    AsyncWebServerResponse *response = request->beginResponse(
      "text/plain",
      (size_t)file_size,
      [ctx](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        (void)index;

        if(ctx->sent >= ctx->size){
          ctx->release_once();
          return 0;
        }

        const uint32_t remain = ctx->size - ctx->sent;
        const uint32_t to_read = (remain < (uint32_t)maxLen) ? remain : (uint32_t)maxLen;
        uint32_t got = 0u;
        const bool ok = sd_files_download_read(buffer, to_read, &got);
        if((!ok) || (got == 0u)){
          ctx->release_once();
          return 0;
        }

        ctx->sent += got;
        return (size_t)got;
      });

    if(response == nullptr){
      ctx->release_once();
      delete ctx;
      request->send(500, "text/plain", "response_alloc");
      return;
    }

    request->onDisconnect([ctx](){
      ctx->release_once();
      delete ctx;
    });

    response->addHeader("Content-Disposition",
        (String("attachment; filename=\"") + file + String("\"")).c_str());
    request->send(response);
  });


  s_server->on("/api/cal/support_generate_reports", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;

    String password = "";
    if(request->hasParam("support", true)){
      password = request->getParam("support", true)->value();
    } else if(request->hasParam("support")){
      password = request->getParam("support")->value();
    }

    if(!cal_support_password_matches_(password)){
      request->send(403, "application/json", "{\"ok\":false,\"reason\":\"bad_support_code\"}");
      return;
    }

    if(!web_single_client_allow(request)){
      request->send(409, "application/json", "{\"ok\":false,\"reason\":\"busy\"}");
      return;
    }

    WebSdBusyScope sd_scope;
    if(!sd_scope.engaged){
      request->send(409, "application/json", "{\"ok\":false,\"reason\":\"sd_busy\"}");
      return;
    }

    calibration_service_refresh_status();

    calibration_record_t active = {};
    const bool active_ok = calibration_service_get_active(&active) && active.sensor.valid;

    calibration_record_t reference = {};
    const bool reference_ok = calibration_service_get_reference(&reference) && reference.sensor.valid;

    installation_calibration_t installation = {};
    const bool installation_ok = calibration_service_get_installation(&installation) && installation.valid;

    if((!active_ok) && (!installation_ok)){
      request->send(409, "application/json", "{\"ok\":false,\"reason\":\"no_valid_calibration\"}");
      return;
    }

    char recorder_path[SD_STORAGE_PATH_MAX];
    recorder_path[0] = '\0';
    char installation_path[SD_STORAGE_PATH_MAX];
    installation_path[0] = '\0';

    bool recorder_written = false;
    bool installation_written = false;
    bool write_failed = false;

    if(active_ok){
      recorder_written = calibration_report_write_stored_recorder(&active,
                                                                  reference_ok ? &reference : nullptr,
                                                                  reference_ok,
                                                                  recorder_path,
                                                                  sizeof(recorder_path));
      write_failed = (!recorder_written) || write_failed;
    }

    if(installation_ok){
      installation_written = calibration_report_write_stored_installation(active_ok ? &active : nullptr,
                                                                          active_ok,
                                                                          &installation,
                                                                          installation_path,
                                                                          sizeof(installation_path));
      write_failed = (!installation_written) || write_failed;
    }

    if((!recorder_written) && (!installation_written)){
      request->send(500, "application/json", "{\"ok\":false,\"reason\":\"report_write_failed\"}");
      return;
    }

    String out = "{\"ok\":true";
    out += ",\"recorder_available\":";
    out += active_ok ? "true" : "false";
    out += ",\"recorder_written\":";
    out += recorder_written ? "true" : "false";
    out += ",\"installation_available\":";
    out += installation_ok ? "true" : "false";
    out += ",\"installation_written\":";
    out += installation_written ? "true" : "false";
    out += ",\"write_failed\":";
    out += write_failed ? "true" : "false";
    if(recorder_written){
      out += ",\"recorder_path\":\"";
      out += recorder_path;
      out += "\"";
    }
    if(installation_written){
      out += ",\"installation_path\":\"";
      out += installation_path;
      out += "\"";
    }
    out += "}";
    request->send(200, "application/json", out);
  });

  s_server->on("/api/cal/support_clear", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    String password = "";
    if(request->hasParam("support", true)){
      password = request->getParam("support", true)->value();
    } else if(request->hasParam("support")){
      password = request->getParam("support")->value();
    }

    if(!cal_support_password_matches_(password)){
      request->send(403, "application/json", "{\"ok\":false,\"reason\":\"bad_support_code\"}");
      return;
    }

    const bool ok = calibration_service_support_clear();
    request->send(ok ? 200 : 500,
                  "application/json",
                  ok ? "{\"ok\":true,\"cleared\":true}" : "{\"ok\":false,\"reason\":\"clear_failed\"}");
  });


  s_server->on("/api/install/support_clear", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    String password = "";
    if(request->hasParam("support", true)){
      password = request->getParam("support", true)->value();
    } else if(request->hasParam("support")){
      password = request->getParam("support")->value();
    }

    if(!cal_support_password_matches_(password)){
      request->send(403, "application/json", "{\"ok\":false,\"reason\":\"bad_support_code\"}");
      return;
    }

    const bool ok = calibration_service_support_clear_installation();
    request->send(ok ? 200 : 500,
                  "application/json",
                  ok ? "{\"ok\":true,\"cleared\":true}" : "{\"ok\":false,\"reason\":\"clear_failed\"}");
  });


  s_server->on("/api/install/start", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    const system_status_t st = state_task_get_status();
    const bool recording = (st.state == ST_RECORDING) || (st.state == ST_STARTING) || (st.state == ST_STOPPING);
    if(recording){
      request->send(409, "application/json", "{\"ok\":false,\"reason\":\"recording\"}");
      return;
    }

    const bool ok = calibration_installation_session_start();
    request->send(ok ? 200 : 409, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"reason\":\"sensor_cal_required\"}");
  });

  s_server->on("/api/install/cancel", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    calibration_installation_session_cancel();
    request->send(200, "application/json", "{\"ok\":true}");
  });

  s_server->on("/api/install/sample", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;
    installation_calibration_status_t st = {};
    if(!calibration_installation_session_get_status(&st)){
      request->send(500, "application/json", "{\"ok\":false,\"reason\":\"status_failed\"}");
      return;
    }

    String out = "{";
    out += "\"ok\":true";
    out += ",\"active\":";
    out += st.session_active ? "true" : "false";
    out += ",\"stable\":";
    out += st.stable ? "true" : "false";
    out += ",\"candidate_valid\":";
    out += st.candidate_valid ? "true" : "false";
    out += ",\"samples\":";
    out += String((unsigned long)st.sample_count);
    out += ",\"total_samples\":";
    out += String((unsigned long)st.total_samples);
    out += ",\"valid_windows\":";
    out += String((unsigned long)st.valid_windows);
    out += ",\"update_count\":";
    out += String((unsigned long)st.update_count);
    out += ",\"last_update_age_ms\":";
    out += String((unsigned long)st.last_update_age_ms);
    out += ",\"quality_mg\":";
    out += String(st.quality_mg, 2);
    out += ",\"mean\":";
    out += cal_vec_json_(st.mean_mg);
    out += ",\"stddev\":";
    out += cal_vec_json_(st.stddev_mg);
    out += ",\"stability_stddev_max_mg\":";
    out += String(CALIBRATION_STABILITY_STDDEV_MAX_MG, 2);
    out += ",\"matrix\":";
    out += cal_matrix_json_(st.matrix);
    out += ",\"stored_valid\":";
    out += st.stored_valid ? "true" : "false";
    out += ",\"stored_date\":";
    if(st.stored_valid){
      out += rtc_date_json_(st.stored_timestamp);
    } else {
      out += "{}";
    }
    out += ",\"stored_mean\":";
    out += cal_vec_json_(st.stored_mean_mg);
    out += ",\"stored_stddev\":";
    out += cal_vec_json_(st.stored_stddev_mg);
    out += ",\"stored_matrix\":";
    out += cal_matrix_json_(st.stored_matrix);
    out += "}";
    request->send(200, "application/json", out);
  });

  s_server->on("/api/install/save", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!cal_require_auth_(request)) return;

    String reason_text = "";
    if(request->hasParam("reason", true)){
      reason_text = request->getParam("reason", true)->value();
    } else if(request->hasParam("reason")){
      reason_text = request->getParam("reason")->value();
    }

    installation_calibration_reason_t reason = INSTALL_CAL_REASON_INITIAL_INSTALLATION;
    if(!calibration_report_parse_installation_reason(reason_text.c_str(), &reason)){
      request->send(400, "application/json", "{\"ok\":false,\"reason\":\"installation_reason_required\"}");
      return;
    }

    installation_calibration_report_data_t report = {};
    report.reason = reason;
    report.recorder_calibration_available = calibration_service_get_active(&report.recorder_calibration);
    report.installation_before_available = calibration_service_get_installation(&report.installation_before);

    calibration_record_t rec = {};
    if(!calibration_installation_session_save(&rec)){
      request->send(409, "application/json", "{\"ok\":false,\"reason\":\"save_failed\"}");
      return;
    }

    report.saved = true;
    report.candidate_available = rec.installation.valid;
    report.candidate = rec.installation;
    report.installation_after_available = rec.installation.valid;
    report.installation_after = rec.installation;

    char report_path[SD_STORAGE_PATH_MAX];
    report_path[0] = '\0';
    const bool report_written = calibration_report_write_installation(&report, report_path, sizeof(report_path));

    String out = "{\"ok\":true,\"saved\":true,\"message\":\"installation_calibration_saved\",\"matrix\":";
    out += cal_matrix_json_(rec.installation.matrix);
    out += ",\"report_written\":";
    out += report_written ? "true" : "false";
    if(report_written){
      out += ",\"report_path\":\"";
      out += report_path;
      out += "\"";
    }
    out += "}";
    request->send(200, "application/json", out);
  });

  s_server_routes_registered = true;
}

/**
 * Ensure that the persistent AsyncWebServer object exists and has its routes.
 *
 * The server is allocated once and reused for the lifetime of the recorder.
 * Web OFF disables the SoftAP but leaves the HTTP listener running.
 *
 * Inputs: None.
 * Returns: `true` when the server object is ready; otherwise `false`.
 */
static bool ensure_server_ready_(){
  if(s_server == nullptr){
    s_server = new (std::nothrow) AsyncWebServer(WEB_SERVER_PORT);
    s_server_routes_registered = false;
    s_server_listener_started = false;
  }

  if(s_server == nullptr){
    return false;
  }

  register_routes();
  return s_server_routes_registered;
}

/**
 * Starts the WiFi access point and, on the first successful Web ON, starts the
 * persistent HTTP listener.
 *
 * AP enable/disable is the repeated lifecycle.  The HTTP listener is started
 * once and remains alive across Web OFF cycles.
 *
 * Inputs: None.
 * Returns: None.
 */
static void start_ap_and_server(){
  if(s_started) return;

  if(!ensure_server_ready_()){
    s_started = false;
    return;
  }

  // Configure the AP address before starting the AP.
  // The Arduino WiFi layer may internally emit AP_START/AP_STOP/AP_START while
  // applying the configuration; the HTTP listener is independent of those
  // AP-side transitions.
  WiFi.softAPConfig(s_ap_ip, s_gateway, s_subnet);

  String ssid = make_ssid();
  String pwd  = make_password();

  bool ap_ok = false;
  if(pwd.length() == 0){
    ap_ok = WiFi.softAP(ssid.c_str());
  } else {
    ap_ok = WiFi.softAP(ssid.c_str(), pwd.c_str());
  }

  if(!ap_ok){
    // AP start failed.  Keep the persistent HTTP server object intact; only
    // reset the AP/WiFi side and retry on a later Web ON loop.
    WiFi.mode(WIFI_OFF);
    s_started = false;
    return;
  }

  // Count successful Web ON activations.  /diag reports this value so restart
  // testing can distinguish successive AP cycles without serial logs.
  s_web_start_cycle++;

  // Give the AP/IP stack time to settle before binding the HTTP server.
  // Poll until softAPIP() returns the configured address, or proceed after
  // AP_START_TIMEOUT_MS to keep Web startup bounded.
  static const uint32_t AP_START_TIMEOUT_MS = 3000u;
  static const uint32_t AP_START_POLL_MS    = 10u;
  const uint32_t t0 = (uint32_t)millis();
  while(WiFi.softAPIP() != s_ap_ip){
    watchdog_kick(WD_WEB);
    if(((uint32_t)millis() - t0) >= AP_START_TIMEOUT_MS){ break; }
    vTaskDelay(pdMS_TO_TICKS(AP_START_POLL_MS));
  }

  watchdog_kick(WD_WEB);

  if(!s_server_listener_started){
    s_server->begin();
    s_server_listener_started = true;
  }

  s_started = true;
}

/**
 * Stops Web availability by disabling the SoftAP and clearing Web-side state.
 *
 * The AsyncWebServer listener remains running while Web availability is
 * controlled by the SoftAP and application-side authorization state.
 *
 * Inputs: None.
 * Returns: None.
 */
static void stop_ap_and_server(){
  if(!s_started) return;

  // Web OFF may interrupt a browser download or OTA upload.  Release all
  // application-level locks and abort in-progress OTA state before the AP is
  // disconnected so the next Web ON starts cleanly.
  (void)sd_files_download_end();
  web_sd_end();
  s_web_client_locked = false;
  s_cal_client_authorized = false;

  if(s_ota_active){
    Update.abort();
  }
  s_ota_active = false;
  s_ota_ok = false;
  s_ota_error[0] = '\0';

  // Do not call s_server->end() and do not delete s_server here.
  // The server listener is intentionally persistent; only the SoftAP is made
  // unavailable to users.
  WiFi.softAPdisconnect(true);
  vTaskDelay(pdMS_TO_TICKS(200));

  WiFi.mode(WIFI_OFF);
  vTaskDelay(pdMS_TO_TICKS(100));

  s_started = false;
}
/**
 * Signals the desired enabled state to the web task.
 *
 * This function is called from foreign tasks (ui_task, state_task) on any
 * core.  It ONLY updates the shared flags; it never touches the WiFi driver
 * or HTTP server directly.  web_task_loop() is the sole owner of the
 * start_ap_and_server() / stop_ap_and_server() lifecycle and will act on the
 * flag change within one loop tick (~200 ms).
 *
 * sd_files_set_authorized() is safe to call from any task (it writes a single
 * bool), so it is kept here to maintain the existing authorization semantics.
 *
 * Inputs: `enabled`.
 * Returns: None.
 */
void web_task_set_enabled(bool enabled){
  s_enabled_requested = enabled;
  sd_files_set_authorized(enabled);
  // Do NOT call stop_ap_and_server() here.  WiFi/AP/HTTP lifecycle is owned
  // exclusively by web_task_loop() to avoid cross-task races on the driver.
}

/**
 * Handles web task is enabled for the Web/WiFi support path without changing
 * recorder-core state ownership.
 *
 * Inputs: None.
 * Returns: `true` when the web interface should be active; otherwise `false`.
 */
bool web_task_is_enabled(void){ return s_enabled_requested; }

/**
 * Web task loop starts or stops the WiFi access point and web server according
 * to the state-machine enable request.
 *
 * Inputs: `arg`.
 * Returns: None.
 */
static void web_task_loop(void *arg){
  (void)arg;
  for(;;){
    watchdog_set_required(WD_WEB, s_enabled_requested || s_started);
    watchdog_kick(WD_WEB);

    if(s_enabled_requested && !s_started){
      start_ap_and_server();
      watchdog_kick(WD_WEB);
    }
    if(!s_enabled_requested && s_started){
      stop_ap_and_server();
      watchdog_kick(WD_WEB);
    }
    if(s_ota_reboot_pending){
      vTaskDelay(pdMS_TO_TICKS(500));
      ESP.restart();
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}


/**
 * Initializes web task init state or hardware resources and prepares the
 * module for later recorder operation.
 *
 * Inputs: None.
 * Returns: None.
 */
void web_task_init(void){
  // Allocate and register the HTTP server once during initialization.  If heap
  // is temporarily unavailable, start_ap_and_server() will retry before the
  // first Web ON activation.
  (void)ensure_server_ready_();

  const BaseType_t ok = xTaskCreatePinnedToCore(
      web_task_loop,
      "web_task",
      CFG_WEB_TASK_STACK_WORDS,
      NULL,
      CFG_WEB_TASK_PRIO,
      nullptr,
      CFG_WEB_TASK_CORE);
  if(ok != pdPASS){
    task_create_failed_reboot("web_task");
  }

}
