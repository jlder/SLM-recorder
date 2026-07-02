// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/calibration_report_service.cpp
 * @brief Human-readable calibration report generation.
 */

#include "src/services/calibration_report_service.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "config.h"
#include "src/services/settings_store.h"
#include "src/services/datetime_service.h"
#include "src/services/sd_files.h"

static char s_last_recorder_report_path[SD_STORAGE_PATH_MAX] = "";
static char s_last_installation_report_path[SD_STORAGE_PATH_MAX] = "";


const char *calibration_report_installation_reason_name(installation_calibration_reason_t reason){
  switch(reason){
    case INSTALL_CAL_REASON_INITIAL_INSTALLATION: return "Initial installation";
    case INSTALL_CAL_REASON_RECORDER_REINSTALLED: return "Recorder reinstalled";
    case INSTALL_CAL_REASON_FAILED_RETRY: return "Failed calibration retry";
    default: return "Unknown";
  }
}

bool calibration_report_parse_installation_reason(const char *text,
                                                  installation_calibration_reason_t *out_reason){
  if((text == nullptr) || (out_reason == nullptr)){
    return false;
  }

  if(strcmp(text, "initial_installation") == 0){
    *out_reason = INSTALL_CAL_REASON_INITIAL_INSTALLATION;
    return true;
  }
  if(strcmp(text, "recorder_reinstalled") == 0){
    *out_reason = INSTALL_CAL_REASON_RECORDER_REINSTALLED;
    return true;
  }
  if(strcmp(text, "failed_calibration_retry") == 0){
    *out_reason = INSTALL_CAL_REASON_FAILED_RETRY;
    return true;
  }

  return false;
}

static const char *report_result_name_(calibration_save_result_t result){
  switch(result){
    case CAL_SAVE_OK: return "ACCEPTED";
    case CAL_SAVE_NOT_READY: return "NOT READY";
    case CAL_SAVE_TEMP_UNAVAILABLE: return "REJECTED - TEMPERATURE UNAVAILABLE";
    case CAL_SAVE_TEMP_RANGE: return "REJECTED - TEMPERATURE OUT OF RANGE";
    case CAL_SAVE_TEMP_UNSTABLE: return "REJECTED - TEMPERATURE UNSTABLE";
    case CAL_SAVE_PLAUSIBILITY_FAULT: return "REJECTED - PLAUSIBILITY FAULT";
    case CAL_SAVE_DELTA_FAULT: return "REJECTED - SIGNIFICANT CALIBRATION DRIFT";
    case CAL_SAVE_NEED_REPEAT: return "REFERENCE STORED - REPEAT REQUIRED";
    case CAL_SAVE_STORAGE_FAILED: return "REJECTED - STORAGE FAILED";
    default: return "UNKNOWN";
  }
}

static void report_append_datetime_(String& out, const rtc_datetime_t *dt){
  if((dt == nullptr) || (dt->year == 0u)){
    out += "-";
    return;
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
           (unsigned int)dt->year,
           (unsigned int)dt->month,
           (unsigned int)dt->day,
           (unsigned int)dt->hour,
           (unsigned int)dt->min,
           (unsigned int)dt->sec);
  out += buf;
}

static void report_append_vec_(String& out, const char *label, const calibration_vec_t *v){
  if((label == nullptr) || (v == nullptr)){
    return;
  }

  out += "  ";
  out += label;
  out += ": X=";
  out += String(v->x_mg, 2);
  out += " mg, Y=";
  out += String(v->y_mg, 2);
  out += " mg, Z=";
  out += String(v->z_mg, 2);
  out += " mg\n";
}

static void report_append_sensor_record_(String& out,
                                         const char *title,
                                         bool available,
                                         const calibration_record_t *rec,
                                         bool include_face_quality){
  out += title;
  out += ":\n";
  if((!available) || (rec == nullptr) || (!rec->sensor.valid)){
    out += "  Not available\n\n";
    return;
  }

  out += "  Date/time: ";
  report_append_datetime_(out, &rec->sensor.timestamp);
  out += "\n";

  out += "  Temperature: ";
  if(rec->sensor.temperature_valid){
    out += String(rec->sensor.temperature_c, 1);
    out += " °C (min ";
    out += String(rec->sensor.temperature_min_c, 1);
    out += ", max ";
    out += String(rec->sensor.temperature_max_c, 1);
    out += ")\n";
  } else {
    out += "not available\n";
  }

  out += "  Gains:   X="; out += String(rec->sensor.gain_x, 6);
  out += ", Y="; out += String(rec->sensor.gain_y, 6);
  out += ", Z="; out += String(rec->sensor.gain_z, 6);
  out += "\n";
  out += "  Offsets: X="; out += String(rec->sensor.offset_x_mg, 1); out += " mg";
  out += ", Y="; out += String(rec->sensor.offset_y_mg, 1); out += " mg";
  out += ", Z="; out += String(rec->sensor.offset_z_mg, 1); out += " mg\n";

  if(include_face_quality){
    static const char *face_names[CAL_FACE_COUNT] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
    float max_stddev_mg = 0.0f;
    uint32_t valid_faces = 0u;
    String missing_faces;

    for(uint32_t i = 0u; i < (uint32_t)CAL_FACE_COUNT; ++i){
      if(rec->sensor.face[i].valid){
        float face_max = rec->sensor.face[i].stddev_mg.x_mg;
        face_max = fmaxf(face_max, rec->sensor.face[i].stddev_mg.y_mg);
        face_max = fmaxf(face_max, rec->sensor.face[i].stddev_mg.z_mg);
        max_stddev_mg = fmaxf(max_stddev_mg, face_max);
        ++valid_faces;
      } else {
        if(missing_faces.length() > 0u){
          missing_faces += ", ";
        }
        missing_faces += face_names[i];
      }
    }

    out += "  Candidate face stability: ";
    if(valid_faces == 0u){
      out += "not available\n";
    } else if(valid_faces == (uint32_t)CAL_FACE_COUNT){
      out += "OK, max stddev ";
      out += String(max_stddev_mg, 1);
      out += " mg\n";
    } else {
      out += "incomplete, max stddev ";
      out += String(max_stddev_mg, 1);
      out += " mg, missing ";
      out += missing_faces;
      out += "\n";
    }
  }
  out += "\n";
}

static void report_sensor_delta_(const sensor_calibration_t *candidate,
                                 const sensor_calibration_t *ref,
                                 float *out_gain_max,
                                 float *out_offset_max){
  if(out_gain_max != nullptr){ *out_gain_max = 0.0f; }
  if(out_offset_max != nullptr){ *out_offset_max = 0.0f; }
  if((candidate == nullptr) || (ref == nullptr) || !candidate->valid || !ref->valid){
    return;
  }

  float gain_max = fabsf(candidate->gain_x - ref->gain_x);
  gain_max = fmaxf(gain_max, fabsf(candidate->gain_y - ref->gain_y));
  gain_max = fmaxf(gain_max, fabsf(candidate->gain_z - ref->gain_z));

  float offset_max = fabsf(candidate->offset_x_mg - ref->offset_x_mg);
  offset_max = fmaxf(offset_max, fabsf(candidate->offset_y_mg - ref->offset_y_mg));
  offset_max = fmaxf(offset_max, fabsf(candidate->offset_z_mg - ref->offset_z_mg));

  if(out_gain_max != nullptr){ *out_gain_max = gain_max; }
  if(out_offset_max != nullptr){ *out_offset_max = offset_max; }
}

static void report_append_comparison_(String& out,
                                      const char *label,
                                      bool candidate_available,
                                      const calibration_record_t *candidate,
                                      bool ref_available,
                                      const calibration_record_t *ref){
  out += "  ";
  out += label;
  out += ": ";
  if((!candidate_available) || (!ref_available) ||
     (candidate == nullptr) || (ref == nullptr) ||
     (!candidate->sensor.valid) || (!ref->sensor.valid)){
    out += "not applicable\n";
    return;
  }

  float gain_delta = 0.0f;
  float offset_delta = 0.0f;
  report_sensor_delta_(&candidate->sensor, &ref->sensor, &gain_delta, &offset_delta);
  const bool within = (gain_delta <= CALIBRATION_GAIN_DELTA_MAX) &&
                      (offset_delta <= CALIBRATION_OFFSET_DELTA_MAX_MG);
  out += within ? "within limits" : "out of limits";
  out += " (max gain delta ";
  out += String(gain_delta, 6);
  out += ", max offset delta ";
  out += String(offset_delta, 2);
  out += " mg)\n";
}

static bool report_copy_path_(char *out_path, size_t out_path_len, const char *path){
  if((out_path == nullptr) || (out_path_len == 0u) || (path == nullptr)){
    return false;
  }

  const size_t n = strlen(path);
  if(n >= out_path_len){
    out_path[0] = '\0';
    return false;
  }
  memcpy(out_path, path, n + 1u);
  return true;
}

static void report_sanitized_registration_(char *out, size_t out_len){
  if((out == nullptr) || (out_len == 0u)){
    return;
  }

  settings_t st;
  const char *src = "RECORDER";
  if(settings_get(&st) && (st.registration[0] != '\0')){
    src = st.registration;
  }

  size_t j = 0u;
  for(size_t i = 0u; (src[i] != '\0') && (j + 1u < out_len); ++i){
    const char c = src[i];
    const bool ok = ((c >= 'A') && (c <= 'Z')) ||
                    ((c >= 'a') && (c <= 'z')) ||
                    ((c >= '0') && (c <= '9')) ||
                    (c == '-') || (c == '_');
    out[j++] = ok ? c : '_';
  }
  out[j] = '\0';
  if(j == 0u){
    strncpy(out, "RECORDER", out_len);
    out[out_len - 1u] = '\0';
  }
}

bool calibration_report_write_recorder(const recorder_calibration_report_data_t *data,
                                       char *out_path,
                                       size_t out_path_len){
  if(data == nullptr){
    return false;
  }

  rtc_datetime_t now = {};
  (void)datetime_service_get(&now);

  char reg[32];
  report_sanitized_registration_(reg, sizeof(reg));

  char path[SD_STORAGE_PATH_MAX];
  const int n = snprintf(path, sizeof(path),
                         "/calibration_reports/%s_%04u%02u%02u_%02u%02u%02u_REC_CAL.txt",
                         reg,
                         (unsigned int)now.year,
                         (unsigned int)now.month,
                         (unsigned int)now.day,
                         (unsigned int)now.hour,
                         (unsigned int)now.min,
                         (unsigned int)now.sec);
  if((n < 0) || ((size_t)n >= sizeof(path))){
    return false;
  }

  settings_t st;
  const bool settings_ok = settings_get(&st);
  const char *registration = (settings_ok && (st.registration[0] != '\0')) ? st.registration : "-";

  String report;
  report.reserve(4200);
  report += "Structural Life Monitoring - Recorder Calibration Report\n";
  report += "=====================================================\n\n";
  report += "Glider registration: "; report += registration; report += "\n";
  report += "Report date/time: "; report_append_datetime_(report, &now); report += "\n";
  report += "Firmware version: "; report += RECORDER_SOFTWARE_VERSION; report += "\n";
  report += "Recorder calibration before this attempt: ";
  if(data->active_before_available && data->active_before.sensor.valid){
    report_append_datetime_(report, &data->active_before.sensor.timestamp);
  } else {
    report += "Not available";
  }
  report += "\n\n";

  report += "Result\n";
  report += "------\n";
  report += "Status: "; report += report_result_name_(data->result); report += "\n";
  if(data->result == CAL_SAVE_OK){
    report += "Update: candidate calibration accepted and becomes current recorder calibration.\n";
  } else if(data->result == CAL_SAVE_NEED_REPEAT){
    report += "Update: first calibration stored as reference; repeat recorder calibration required before recorder use for SLM recording credit.\n";
  } else if(data->result == CAL_SAVE_DELTA_FAULT){
    report += "Update: significant calibration drift; candidate saved for support diagnostics, rejected as current calibration, and recorder calibration suspended.\n";
  } else {
    report += "Update: candidate calibration not accepted as current calibration.\n";
  }
  report += "\n";

  report += "Acceptance thresholds\n";
  report += "---------------------\n";
  report += "Allowed temperature range: ";
  report += String(CALIBRATION_TEMP_MIN_C, 1); report += " °C to ";
  report += String(CALIBRATION_TEMP_MAX_C, 1); report += " °C\n";
  report += "Maximum allowed temperature span: ";
  report += String(CALIBRATION_TEMP_MAX_SPAN_C, 1); report += " °C\n";
  report += "Gain delta limit: "; report += String(CALIBRATION_GAIN_DELTA_MAX, 6); report += "\n";
  report += "Offset delta limit: "; report += String(CALIBRATION_OFFSET_DELTA_MAX_MG, 1); report += " mg\n\n";

  report_append_sensor_record_(report, "Candidate calibration from this attempt",
                               data->candidate_available, &data->candidate, true);
  report_append_sensor_record_(report, "Current calibration before this attempt",
                               data->active_before_available, &data->active_before, false);
  report_append_sensor_record_(report, "Reference calibration before this attempt",
                               data->reference_before_available, &data->reference_before, false);

  report += "Comparison\n";
  report += "----------\n";
  report_append_comparison_(report, "Candidate vs active before",
                            data->candidate_available, &data->candidate,
                            data->active_before_available, &data->active_before);
  report_append_comparison_(report, "Candidate vs reference before",
                            data->candidate_available, &data->candidate,
                            data->reference_before_available, &data->reference_before);
  report += "\n";

  report += "Operational conclusion\n";
  report += "----------------------\n";
  if(data->result == CAL_SAVE_OK){
    report += "Recorder calibration is valid, new candidate replaces current calibration and may be used for SLM recording credit, provided installation calibration is valid and all other recorder conditions are satisfied.\n";
  } else if(data->result == CAL_SAVE_NEED_REPEAT){
    report += "First recorder calibration was stored as reference. Perform a second successful recorder calibration to confirm repeatability. The recorder is inop for SLM recording credit until this repeat calibration is successful.\n";
  } else if(data->result == CAL_SAVE_DELTA_FAULT){
    report += "SIGNIFICANT CALIBRATION DRIFT - CALIBRATION REJECTED. The candidate calibration was saved for support diagnostics but did not replace current calibration.\n";
    report += "The recorder is inop for SLM recording credit until a subsequent recorder calibration is successful or support clears the fault after investigation. Check calibration support, recorder seating, stable horizontal surface, and temperature range before retrying. Contact support if the fault repeats.\n";
    report += "Flight records after the last valid recorder calibration";
    if(data->active_before_available && data->active_before.sensor.valid){
      report += " (";
      report_append_datetime_(report, &data->active_before.sensor.timestamp);
      report += ")";
    }
    report += " should be considered suspect and potentially discarded unless a subsequent recorder calibration is successful.\n";
  } else if(data->result == CAL_SAVE_TEMP_UNAVAILABLE){
    report += "Recorder calibration was rejected because sensor temperature was unavailable. Temperature is mandatory for recorder calibration. The recorder is inop for SLM recording credit until an appropriate recorder calibration can be performed. Check sensor status and retry.\n";
  } else if(data->result == CAL_SAVE_TEMP_RANGE){
    report += "Recorder calibration was rejected because sensor temperature was outside the allowed range. The recorder is inop for SLM recording credit until an appropriate recorder calibration can be performed. Place the recorder in the required temperature range, wait for stabilization, and retry.\n";
  } else if(data->result == CAL_SAVE_TEMP_UNSTABLE){
    report += "Recorder calibration was rejected because sensor temperature was not stable enough. The recorder is inop for SLM recording credit until an appropriate recorder calibration can be performed. Wait for temperature stabilization and retry.\n";
  } else if(data->result == CAL_SAVE_PLAUSIBILITY_FAULT){
    report += "Recorder calibration was rejected because the computed gain/offset values were not plausible. The recorder is inop for SLM recording credit until an appropriate recorder calibration can be performed. Check recorder seating in the calibration support, support condition, horizontal stable surface, and repeat calibration. Contact support if the fault repeats.\n";
  } else if(data->result == CAL_SAVE_STORAGE_FAILED){
    report += "Recorder calibration could not be stored. The recorder is inop for SLM recording credit until an appropriate recorder calibration can be saved successfully. Check SD/NVS/storage status and retry.\n";
  } else {
    report += "Recorder calibration was not accepted. The recorder is inop for SLM recording credit until an appropriate recorder calibration can be performed. See status and thresholds above before retrying.\n";
  }

  const bool ok = sd_files_write_text_file(path, report.c_str(), (uint32_t)report.length());
  if(ok){
    report_copy_path_(s_last_recorder_report_path, sizeof(s_last_recorder_report_path), path);
    (void)report_copy_path_(out_path, out_path_len, path);
  }
  return ok;
}


static void report_append_matrix_(String& out, const float *m){
  if(m == nullptr){
    out += "  Not available\n";
    return;
  }

  for(uint32_t r = 0u; r < 3u; ++r){
    out += "  ";
    out += String(m[(r * 3u) + 0u], 6);
    out += "  ";
    out += String(m[(r * 3u) + 1u], 6);
    out += "  ";
    out += String(m[(r * 3u) + 2u], 6);
    out += "\n";
  }
}

static void report_append_installation_angles_(String& out, const float *m){
  if(m == nullptr){
    out += "Pitch: -   Roll: -\n";
    return;
  }

  const float rad_to_deg = 57.295779513f;
  const float pitch = atan2f(-m[6], m[0]) * rad_to_deg;
  const float roll = atan2f(-m[5], m[4]) * rad_to_deg;

  out += "Pitch: ";
  out += String(pitch, 2);
  out += "°   Roll: ";
  out += String(roll, 2);
  out += "°\n";
}

static void report_append_installation_record_(String& out,
                                               const char *title,
                                               bool available,
                                               const installation_calibration_t *inst){
  out += title;
  out += ":\n";
  if((!available) || (inst == nullptr) || (!inst->valid)){
    out += "  Not available\n\n";
    return;
  }

  out += "  Date/time: ";
  report_append_datetime_(out, &inst->timestamp);
  out += "\n";
  report_append_vec_(out, "Mean", &inst->mean_mg);
  report_append_vec_(out, "Stddev", &inst->stddev_mg);
  out += "  Matrix:\n";
  report_append_matrix_(out, inst->matrix);
  report_append_installation_angles_(out, inst->matrix);
  out += "\n";
}

bool calibration_report_write_installation(const installation_calibration_report_data_t *data,
                                           char *out_path,
                                           size_t out_path_len){
  if(data == nullptr){
    return false;
  }

  rtc_datetime_t now = {};
  (void)datetime_service_get(&now);

  char reg[32];
  report_sanitized_registration_(reg, sizeof(reg));

  char path[SD_STORAGE_PATH_MAX];
  const int n = snprintf(path, sizeof(path),
                         "/calibration_reports/%s_%04u%02u%02u_%02u%02u%02u_INST_CAL.txt",
                         reg,
                         (unsigned int)now.year,
                         (unsigned int)now.month,
                         (unsigned int)now.day,
                         (unsigned int)now.hour,
                         (unsigned int)now.min,
                         (unsigned int)now.sec);
  if((n < 0) || ((size_t)n >= sizeof(path))){
    return false;
  }

  settings_t st;
  const bool settings_ok = settings_get(&st);
  const char *registration = (settings_ok && (st.registration[0] != '\0')) ? st.registration : "-";

  String report;
  report.reserve(3600);
  report += "Structural Life Monitoring - Installation Calibration Report\n";
  report += "=========================================================\n\n";
  report += "Glider registration: "; report += registration; report += "\n";
  report += "Report date/time: "; report_append_datetime_(report, &now); report += "\n";
  report += "Firmware version: "; report += RECORDER_SOFTWARE_VERSION; report += "\n";
  report += "Recorder calibration Date/time: ";
  if(data->recorder_calibration_available && data->recorder_calibration.sensor.valid){
    report_append_datetime_(report, &data->recorder_calibration.sensor.timestamp);
  } else {
    report += "-";
  }
  report += "\n";
  report += "Maintenance reason: ";
  report += calibration_report_installation_reason_name(data->reason);
  report += "\n\n";

  report += "Results\n";
  report += "------\n";
  report += "Status: "; report += data->saved ? "ACCEPTED" : "REJECTED"; report += "\n\n";

  report_append_installation_record_(report, "Candidate installation calibration from this attempt",
                                     data->candidate_available,
                                     &data->candidate);
  report_append_installation_record_(report, "Installation calibration before this attempt",
                                     data->installation_before_available,
                                     &data->installation_before);

  report += "Operational conclusion\n";
  report += "----------------------\n";
  if(data->saved){
    report += "Installation calibration is valid, new candidate replaces previous calibration and may be used for SLM recording credit, provided recorder calibration is valid and all other recorder conditions are satisfied.\n";
  } else {
    report += "Installation calibration was not accepted. Check aircraft attitude, recorder installation, support condition, and stability before retrying.\n";
  }

  const bool ok = sd_files_write_text_file(path, report.c_str(), (uint32_t)report.length());
  if(ok){
    report_copy_path_(s_last_installation_report_path, sizeof(s_last_installation_report_path), path);
    (void)report_copy_path_(out_path, out_path_len, path);
  }
  return ok;
}

bool calibration_report_get_last_recorder_path(char *out_path, size_t out_path_len){
  if(s_last_recorder_report_path[0] == '\0'){
    return false;
  }
  return report_copy_path_(out_path, out_path_len, s_last_recorder_report_path);
}


bool calibration_report_get_last_installation_path(char *out_path, size_t out_path_len){
  if(s_last_installation_report_path[0] == '\0'){
    return false;
  }
  return report_copy_path_(out_path, out_path_len, s_last_installation_report_path);
}
