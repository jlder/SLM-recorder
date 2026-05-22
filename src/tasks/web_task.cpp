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
#include <Arduino.h>
#include <stdio.h>

#include <WiFi.h>
#include <new>
#include <ESPAsyncWebServer.h>

#include "src/tasks/html_interface.h"
#include "src/tasks/state_task.h"
#include "src/global.h"
#include "src/services/settings_store.h"
#include "src/services/sd_files.h" // authorization gate for file ops
#include "src/services/task_helpers.h"
#include "src/services/calibration_service.h"

#include "src/tasks/sd_task.h"
// Webserver must ONLY be enabled in READY (State Task enforces this).
// This task owns WiFi/AP and HTTP server lifecycle.

static AsyncWebServer s_server(WEB_SERVER_PORT);
static bool s_enabled = false;
static bool s_started = false;

static IPAddress s_ap_ip = AP_IP_ADDRESS;
static IPAddress s_gateway = AP_GATEWAY;
static IPAddress s_subnet = AP_SUBNET;


// --- Web single-client policy ---
// Only one client (IP) may access SD endpoints at a time. Session expires after inactivity.
static bool s_web_client_locked = false;
static bool s_routes_registered = false;
static uint32_t s_web_client_ip = 0u; // IPv4 packed
static uint32_t s_web_client_last_ms = 0u;

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


static const char *cal_status_name_(calibration_status_t status){
  switch(status){
    case CAL_STATUS_VALID: return "valid";
    case CAL_STATUS_EXPIRED: return "expired";
    case CAL_STATUS_FAULT: return "fault";
    case CAL_STATUS_MISSING:
    default: return "missing";
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
    case 0u: return rec.face[CAL_FACE_PX].mean_mg.x_mg;
    case 1u: return rec.face[CAL_FACE_PY].mean_mg.y_mg;
    case 2u: return rec.face[CAL_FACE_PZ].mean_mg.z_mg;
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
    case 0u: return rec.face[CAL_FACE_NX].mean_mg.x_mg;
    case 1u: return rec.face[CAL_FACE_NY].mean_mg.y_mg;
    case 2u: return rec.face[CAL_FACE_NZ].mean_mg.z_mg;
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
    case 0u: return rec.gain_x;
    case 1u: return rec.gain_y;
    case 2u: return rec.gain_z;
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
    case 0u: return rec.offset_x_mg;
    case 1u: return rec.offset_y_mg;
    case 2u: return rec.offset_z_mg;
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
static String cal_record_date_json_(const calibration_record_t& rec){
  char buf[96];
  const int n = snprintf(buf, sizeof(buf),
                         "{\"year\":%u,\"month\":%u,\"day\":%u,\"hour\":%u,\"min\":%u,\"sec\":%u}",
                         (unsigned)rec.timestamp.year,
                         (unsigned)rec.timestamp.month,
                         (unsigned)rec.timestamp.day,
                         (unsigned)rec.timestamp.hour,
                         (unsigned)rec.timestamp.min,
                         (unsigned)rec.timestamp.sec);
  return web_snprintf_ok_(n, sizeof(buf)) ? String(buf) : String("{}");
}

/**
 * Registers web routes for status, file management, calibration actions, and
 * embedded pages once during server setup.
 *
 * Inputs: None.
 * Returns: None.
 */
static void register_routes_once(){
  if(s_routes_registered) return;

  // Routes
  s_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    // Serve embedded HTML interface.
    request->send_P(200, "text/html", HTML_PAGE);
  });

  // API endpoints expected by the embedded HTML interface (prototype-compatible)
  s_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
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

  s_server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request){
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

  s_server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request){
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

  // Raw view for debugging: returns the SD task JSON array directly
struct WebDownloadCtx {
  String   path;
  uint32_t size;
  bool     released;

  WebDownloadCtx(const String& p, uint32_t s)
      : path(p), size(s), released(false) {}

  /**
   * Releases the Web SD busy flag once for this download context.
   *
   * Inputs: None.
   * Returns: None.
   */
  void release_busy_once(void) {
    if (!released) {
      released = true;
      (void)sd_files_download_end();
      web_sd_end();
    }
  }
};

s_server.on("/api/download", HTTP_GET, [](AsyncWebServerRequest *request){
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

    uint32_t file_size = 0u;
    if (!sd_files_download_begin(path.c_str(), &file_size)) {
      web_sd_end();
      request->send(404, "text/plain", "not found");
      return;
    }

    const int slash = file.lastIndexOf('/');
    if (slash >= 0) {
      file = file.substring(slash + 1);
    }

    WebDownloadCtx *ctx = new (std::nothrow) WebDownloadCtx(path, file_size);
    if (ctx == nullptr) {
      (void)sd_files_download_end();
      web_sd_end();
      request->send(500, "text/plain", "oom");
      return;
    }

    AsyncWebServerResponse *response = request->beginChunkedResponse(
      content_type_from_name(file),
      [ctx](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        // End of stream.  Cleanup is centralized in request->onDisconnect()
        // so aborted transfers and completed transfers use the same path.
        if (index >= ctx->size) {
          return 0;
        }

        const uint32_t remain  = ctx->size - (uint32_t)index;
        const uint32_t to_read = (remain < (uint32_t)maxLen)
                                   ? remain
                                   : (uint32_t)maxLen;

        uint32_t got = 0u;
        const bool ok = sd_files_download_read(buffer, to_read, &got);

        if ((!ok) || (got == 0u)) {
          // Signal end-of-stream/read failure.  The disconnect hook owns
          // cleanup to avoid double delete paths.
          return 0;
        }

        return (size_t)got;
      });

    if (response == nullptr) {
      ctx->release_busy_once();
      delete ctx;
      request->send(500, "text/plain", "response_alloc");
      return;
    }

    // Critical cleanup path: this is expected to run for normal completion and
    // for aborted client transfers.  web_sd_try_begin() also has a stale-lock
    // timeout as a final recovery guard.
    request->onDisconnect([ctx]() {
      ctx->release_busy_once();
      delete ctx;
    });

    response->addHeader("Content-Disposition",
        (String("attachment; filename=\"") + file + String("\"")).c_str());

    request->send(response);
  });

  s_server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!web_single_client_allow(request)) { request->send(409, "text/plain", "BUSY"); return; }
    WebSdBusyScope _sdscope;
    if (!_sdscope.engaged) { request->send(409, "text/plain", "BUSY"); return; }


    if(!sd_files_is_authorized()){
      request->send(403, "application/json", "{\"ok\":false,\"reason\":\"not_authorized\"}");
      return;
    }
    // Prototype UI sends ?file=... as query parameter.
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

  


  s_server.on("/api/cal/status", HTTP_GET, [](AsyncWebServerRequest *request){
    calibration_service_refresh_status();
    const calibration_status_t status = calibration_service_status();
    const bool recording_allowed = calibration_service_is_recording_allowed();

    char buf[192];
    const int n = snprintf(buf, sizeof(buf),
                           "{\"status\":\"%s\",\"recording_allowed\":%s,\"session_active\":%s}",
                           cal_status_name_(status),
                           recording_allowed ? "true" : "false",
                           calibration_session_active() ? "true" : "false");
    if(!web_snprintf_ok_(n, sizeof(buf))){
      request->send(500, "application/json", "{\"error\":\"format_failed\"}");
      return;
    }
    request->send(200, "application/json", buf);
  });

  s_server.on("/api/cal/start", HTTP_POST, [](AsyncWebServerRequest *request){
    const system_status_t st = state_task_get_status();
    const bool recording = (st.state == ST_RECORDING) || (st.state == ST_STARTING) || (st.state == ST_STOPPING);
    if(recording){
      request->send(409, "application/json", "{\"ok\":false,\"reason\":\"recording\"}");
      return;
    }

    const bool ok = calibration_session_start();
    request->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  s_server.on("/api/cal/cancel", HTTP_POST, [](AsyncWebServerRequest *request){
    calibration_session_cancel();
    request->send(200, "application/json", "{\"ok\":true}");
  });

  s_server.on("/api/cal/sample", HTTP_GET, [](AsyncWebServerRequest *request){
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
    out += ",\"samples\":";
    out += String((unsigned long)st.sample_count);
    out += ",\"mean\":";
    out += cal_vec_json_(st.mean_mg);
    out += ",\"stddev\":";
    out += cal_vec_json_(st.stddev_mg);
    out += ",\"face_valid\":";
    out += cal_face_valid_json_(st.face_valid);
    out += ",\"session_face\":";
    out += cal_face_capture_json_(st.face);
    out += ",\"stored_loaded\":";
    out += st.stored_loaded ? "true" : "false";
    out += ",\"stored_face\":";
    out += cal_face_capture_json_(st.stored_face);
    out += ",\"result_available\":";
    out += result_ok ? "true" : "false";
    out += ",\"result\":";
    out += result_ok ? cal_record_axis_result_json_(current_result) : "[]";
    out += ",\"nvs_result_available\":";
    out += nvs_result_ok ? "true" : "false";
    out += ",\"nvs_date\":";
    out += nvs_result_ok ? cal_record_date_json_(stored_result) : "{}";
    out += ",\"nvs_result\":";
    out += nvs_result_ok ? cal_record_axis_result_json_(stored_result) : "[]";
    out += "}";
    request->send(200, "application/json", out);
  });

  s_server.on("/api/cal/accept", HTTP_POST, [](AsyncWebServerRequest *request){
    const bool ok = calibration_session_accept_candidate();
    request->send(ok ? 200 : 409, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  s_server.on("/api/cal/result", HTTP_GET, [](AsyncWebServerRequest *request){
    calibration_record_t rec = {};
    if(!calibration_session_compute(&rec)){
      request->send(409, "application/json", "{\"ok\":false}");
      return;
    }

    char buf[384];
    const int n = snprintf(buf, sizeof(buf),
                           "{\"ok\":true,\"saved\":true,\"message\":\"calibration_saved\","
                           "\"gain\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f},"
                           "\"offset_mg\":{\"x\":%.1f,\"y\":%.1f,\"z\":%.1f}}",
                           rec.gain_x, rec.gain_y, rec.gain_z,
                           rec.offset_x_mg, rec.offset_y_mg, rec.offset_z_mg);
    if(!web_snprintf_ok_(n, sizeof(buf))){
      request->send(500, "application/json", "{\"ok\":false,\"reason\":\"format_failed\"}");
      return;
    }
    request->send(200, "application/json", buf);
  });

  s_server.on("/api/cal/save", HTTP_POST, [](AsyncWebServerRequest *request){
    calibration_record_t rec = {};
    if(!calibration_session_save(&rec)){
      request->send(409, "application/json", "{\"ok\":false,\"reason\":\"save_failed\"}");
      return;
    }

    char buf[384];
    const int n = snprintf(buf, sizeof(buf),
                           "{\"ok\":true,\"saved\":true,\"message\":\"calibration_saved\","
                           "\"gain\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f},"
                           "\"offset_mg\":{\"x\":%.1f,\"y\":%.1f,\"z\":%.1f}}",
                           rec.gain_x, rec.gain_y, rec.gain_z,
                           rec.offset_x_mg, rec.offset_y_mg, rec.offset_z_mg);
    if(!web_snprintf_ok_(n, sizeof(buf))){
      request->send(500, "application/json", "{\"ok\":false,\"reason\":\"format_failed\"}");
      return;
    }
    request->send(200, "application/json", buf);
  });


  s_routes_registered = true;
}

/**
 * Starts the WiFi access point and web server using the configured AP address,
 * SSID, password, and registered routes.
 *
 * Inputs: None.
 * Returns: None.
 */
static void start_ap_and_server(){
  if(s_started) return;
  register_routes_once();

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(s_ap_ip, s_gateway, s_subnet);

  String ssid = make_ssid();
  String pwd  = make_password();

  if(pwd.length() == 0){
    WiFi.softAP(ssid.c_str());
  } else {
    WiFi.softAP(ssid.c_str(), pwd.c_str());
  }

  s_server.begin();
  s_started = true;
}

/**
 * Stops the web server, releases SD web access, disconnects the access point,
 * and turns the ESP WiFi mode off.
 *
 * Inputs: None.
 * Returns: None.
 */
static void stop_ap_and_server(){
  if(!s_started) return;
  s_server.end();
  web_sd_end();
  s_web_client_locked = false;
  WiFi.softAPdisconnect(true);
  delay(20);
  WiFi.mode(WIFI_OFF);
  s_started = false;
}
/**
 * Updates web task set enabled state and applies the change to the owning
 * module or hardware interface.
 *
 * Inputs: `enabled`.
 * Returns: None.
 */
void web_task_set_enabled(bool enabled){
  s_enabled = enabled;
  sd_files_set_authorized(s_enabled);
  if(!s_enabled){
    stop_ap_and_server();
  }
}

/**
 * Handles web task is enabled for the Web/WiFi support path without changing
 * recorder-core state ownership.
 *
 * Inputs: None.
 * Returns: `true` when the web interface should be active; otherwise `false`.
 */
bool web_task_is_enabled(void){ return s_enabled; }

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
    if(s_enabled && !s_started){
      start_ap_and_server();
    }
    if(!s_enabled && s_started){
      stop_ap_and_server();
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

