// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/automation_service.cpp
 * @brief Continuous 20 Hz motion and flight-presence signal processing.
 *
 * This module intentionally contains only small fixed-size causal filters and
 * rolling windows. It performs no state transitions, SD access, or file work.
 */

#include "src/services/automation_service.h"

#include <math.h>
#include <string.h>

#include "config.h"

namespace {

static constexpr float kSampleRateHz = 20.0f;
static constexpr float kPi = 3.14159265358979323846f;
static constexpr uint32_t kMotionWindowSamples =
    (uint32_t)(AUTO_RECORD_MOTION_WINDOW_S * kSampleRateHz + 0.5f);
static constexpr uint32_t kStartConfirmSamples =
    (uint32_t)(AUTO_RECORD_START_CONFIRM_S * kSampleRateHz + 0.5f);
static constexpr uint32_t kStopConfirmSamples =
    (uint32_t)(AUTO_RECORD_STOP_QUIET_S * kSampleRateHz + 0.5f);
static constexpr uint32_t kHirmsWindowSamples =
    (uint32_t)(FLIGHT_ANALYSIS_HIRMS_WINDOW_S * kSampleRateHz + 0.5f);
static constexpr uint32_t kLowrmsWindowSamples =
    (uint32_t)(FLIGHT_ANALYSIS_LOWRMS_WINDOW_S * kSampleRateHz + 0.5f);
static constexpr uint32_t kPrimaryConfirmSamples =
    (uint32_t)(AUTO_FLIGHT_PRIMARY_CONFIRM_S * kSampleRateHz + 0.5f);
static constexpr uint32_t kLateConfirmSamples =
    (uint32_t)(AUTO_FLIGHT_LATE_CONFIRM_S * kSampleRateHz + 0.5f);
static constexpr uint32_t kFlightEndHirmsEventConfirmSamples =
    (uint32_t)(AUTO_FLIGHT_END_HIRMS_EVENT_CONFIRM_S * kSampleRateHz + 0.5f);
static constexpr uint32_t kFlightEndEventWindowSamples =
    (uint32_t)(AUTO_FLIGHT_END_EVENT_WINDOW_S * kSampleRateHz + 0.5f);
static constexpr uint32_t kFlightEndFgConfirmSamples =
    (uint32_t)(AUTO_FLIGHT_END_FG_CONFIRM_S * kSampleRateHz + 0.5f);
static constexpr uint32_t kFlightEndGroundConfirmSamples =
    (uint32_t)(AUTO_FLIGHT_END_GROUND_CONFIRM_S * kSampleRateHz + 0.5f);

static_assert(kMotionWindowSamples > 0u, "motion RMS window must not be empty");
static_assert(kStartConfirmSamples > 0u, "motion start confirmation must not be empty");
static_assert(kStopConfirmSamples > 0u, "motion stop confirmation must not be empty");
static_assert(kHirmsWindowSamples > 0u, "HIRMS window must not be empty");
static_assert(kLowrmsWindowSamples > 0u, "LOWRMS window must not be empty");
static_assert(kPrimaryConfirmSamples > 0u, "primary flight confirmation must not be empty");
static_assert(kLateConfirmSamples > 0u, "late-start confirmation must not be empty");
static_assert(kFlightEndHirmsEventConfirmSamples > 0u,
              "flight-end HIRMS event confirmation must not be empty");
static_assert(kFlightEndEventWindowSamples > 0u,
              "flight-end event window must not be empty");
static_assert(kFlightEndFgConfirmSamples > 0u,
              "flight-end FG confirmation must not be empty");
static_assert(kFlightEndGroundConfirmSamples > 0u,
              "flight-end ground confirmation must not be empty");

struct first_order_hp_t {
  float alpha;
  float x_previous;
  float y_previous;
  bool initialized;
};

struct biquad_t {
  float b0;
  float b1;
  float b2;
  float a1;
  float a2;
  float x1;
  float x2;
  float y1;
  float y2;
};

struct butterworth4_t {
  biquad_t q1;
  biquad_t q2;
};

struct trailing_rms_t {
  float *squares;
  uint32_t capacity;
  uint32_t position;
  uint32_t count;
  double sum_squares;
};

static accel_sample_t s_motion_ring[kMotionWindowSamples] = {};
static uint32_t s_motion_count = 0u;
static uint32_t s_motion_head = 0u;
static double s_motion_sum[3] = {0.0, 0.0, 0.0};
static double s_motion_sum2 = 0.0;
static uint32_t s_motion_above_count = 0u;
static uint32_t s_motion_below_count = 0u;
static first_order_hp_t s_attitude_hp_x = {};
static first_order_hp_t s_attitude_hp_y = {};
static uint32_t s_attitude_above_count = 0u;

static float s_hirms_squares[kHirmsWindowSamples] = {};
static float s_lowrms_squares[kLowrmsWindowSamples] = {};
static trailing_rms_t s_hirms_rms = {};
static trailing_rms_t s_lowrms_rms = {};
static butterworth4_t s_hirms_hp3 = {};
static butterworth4_t s_lowrms_hp025 = {};
static butterworth4_t s_lowrms_lp3 = {};

static automation_status_t s_status = {};

// Per-record logical evidence. These variables are reset by
// automation_service_begin_recording(); the filters above are deliberately not.
static bool s_had_hirms = false;
static bool s_possible_flight = false;
static bool s_previous_hirms_above = false;
static uint32_t s_primary_count = 0u;
static uint32_t s_late_count = 0u;

// Per-record flight-end detector. This state is initialized only after
// flight_seen latches; the causal HIRMS/LOWRMS filters remain continuous.
static bool s_flight_end_initialized = false;
static float s_flight_hirms_min_g = 0.0f;
static float s_flight_hirms_max_g = 0.0f;
static bool s_hirms_event_active = false;
static uint32_t s_hirms_event_count = 0u;
static bool s_hirms_event_saw_flight_fg = false;
static bool s_valid_landing_event = false;
static uint32_t s_landing_event_age = 0u;
static bool s_ground_candidate = false;
static uint32_t s_fg_low_count = 0u;
static uint32_t s_ground_count = 0u;

static void first_order_hp_init_(first_order_hp_t *state, float cutoff_hz){
  if(state == nullptr){
    return;
  }

  memset(state, 0, sizeof(*state));
  const float tau_s = 1.0f / (2.0f * kPi * cutoff_hz);
  const float sample_period_s = 1.0f / kSampleRateHz;
  state->alpha = tau_s / (tau_s + sample_period_s);
}

static float first_order_hp_process_(first_order_hp_t *state, float input){
  if(state == nullptr){
    return 0.0f;
  }

  if(!state->initialized){
    state->x_previous = input;
    state->y_previous = 0.0f;
    state->initialized = true;
    return 0.0f;
  }

  float output = state->alpha * (state->y_previous + input - state->x_previous);
  if(!isfinite(output)){
    output = 0.0f;
  }
  state->x_previous = input;
  state->y_previous = output;
  return output;
}

static void biquad_init_(biquad_t *state, bool low_pass, float cutoff_hz, float q){
  if(state == nullptr){
    return;
  }

  const float w0 = 2.0f * kPi * cutoff_hz / kSampleRateHz;
  const float cos_w0 = cosf(w0);
  const float sin_w0 = sinf(w0);
  const float alpha = sin_w0 / (2.0f * q);

  float b0 = 0.0f;
  float b1 = 0.0f;
  float b2 = 0.0f;
  if(low_pass){
    b0 = (1.0f - cos_w0) * 0.5f;
    b1 = 1.0f - cos_w0;
    b2 = b0;
  } else {
    b0 = (1.0f + cos_w0) * 0.5f;
    b1 = -(1.0f + cos_w0);
    b2 = b0;
  }

  const float a0 = 1.0f + alpha;
  memset(state, 0, sizeof(*state));
  state->b0 = b0 / a0;
  state->b1 = b1 / a0;
  state->b2 = b2 / a0;
  state->a1 = (-2.0f * cos_w0) / a0;
  state->a2 = (1.0f - alpha) / a0;
}

static void butterworth4_init_(butterworth4_t *state, bool low_pass, float cutoff_hz){
  if(state == nullptr){
    return;
  }
  biquad_init_(&state->q1, low_pass, cutoff_hz, (float)FLIGHT_ANALYSIS_BUTTER_Q1);
  biquad_init_(&state->q2, low_pass, cutoff_hz, (float)FLIGHT_ANALYSIS_BUTTER_Q2);
}

static float biquad_process_(biquad_t *state, float input){
  if(state == nullptr){
    return 0.0f;
  }

  float output = state->b0 * input + state->b1 * state->x1 + state->b2 * state->x2 -
                 state->a1 * state->y1 - state->a2 * state->y2;
  if(!isfinite(output)){
    output = 0.0f;
  }

  state->x2 = state->x1;
  state->x1 = input;
  state->y2 = state->y1;
  state->y1 = output;
  return output;
}

static float butterworth4_process_(butterworth4_t *state, float input){
  return biquad_process_(&state->q2, biquad_process_(&state->q1, input));
}

static void trailing_rms_init_(trailing_rms_t *state, float *storage, uint32_t capacity){
  if(state == nullptr){
    return;
  }

  memset(state, 0, sizeof(*state));
  state->squares = storage;
  state->capacity = capacity;
  if((storage != nullptr) && (capacity > 0u)){
    memset(storage, 0, (size_t)capacity * sizeof(float));
  }
}

static float trailing_rms_update_(trailing_rms_t *state, float value){
  if((state == nullptr) || (state->squares == nullptr) || (state->capacity == 0u)){
    return 0.0f;
  }

  const float square = value * value;
  if(state->count >= state->capacity){
    state->sum_squares -= (double)state->squares[state->position];
  } else {
    ++state->count;
  }

  state->squares[state->position] = square;
  state->sum_squares += (double)square;
  state->position = (state->position + 1u) % state->capacity;

  const double mean_square = state->sum_squares / (double)state->count;
  return sqrtf((float)fmax(0.0, mean_square));
}

static void motion_update_(const accel_sample_t *sample){
  const double x = (double)sample->ax * 0.001;
  const double y = (double)sample->ay * 0.001;
  const double z = (double)sample->az * 0.001;

  const float hp_x = first_order_hp_process_(&s_attitude_hp_x, (float)x);
  const float hp_y = first_order_hp_process_(&s_attitude_hp_y, (float)y);
  const bool attitude_above =
      (fabsf(hp_x) >= (float)AUTO_RECORD_ATTITUDE_THRESHOLD_G) ||
      (fabsf(hp_y) >= (float)AUTO_RECORD_ATTITUDE_THRESHOLD_G);
  if(attitude_above){
    if(s_attitude_above_count < kStartConfirmSamples){
      ++s_attitude_above_count;
    }
  } else {
    s_attitude_above_count = 0u;
  }
  s_status.attitude_start_confirmed =
      (s_attitude_above_count >= kStartConfirmSamples);

  if(s_motion_count == kMotionWindowSamples){
    const accel_sample_t &old = s_motion_ring[s_motion_head];
    const double ox = (double)old.ax * 0.001;
    const double oy = (double)old.ay * 0.001;
    const double oz = (double)old.az * 0.001;
    s_motion_sum[0] -= ox;
    s_motion_sum[1] -= oy;
    s_motion_sum[2] -= oz;
    s_motion_sum2 -= (ox * ox) + (oy * oy) + (oz * oz);
  } else {
    ++s_motion_count;
  }

  s_motion_ring[s_motion_head] = *sample;
  s_motion_head = (s_motion_head + 1u) % kMotionWindowSamples;
  s_motion_sum[0] += x;
  s_motion_sum[1] += y;
  s_motion_sum[2] += z;
  s_motion_sum2 += (x * x) + (y * y) + (z * z);

  const double count = (double)s_motion_count;
  const double mean_square =
      (s_motion_sum[0] / count) * (s_motion_sum[0] / count) +
      (s_motion_sum[1] / count) * (s_motion_sum[1] / count) +
      (s_motion_sum[2] / count) * (s_motion_sum[2] / count);
  const double variance_sum = fmax(0.0, (s_motion_sum2 / count) - mean_square);
  s_status.motion_rms_g = (float)sqrt(variance_sum);

  if(s_status.motion_rms_g >= (float)AUTO_RECORD_MOTION_THRESHOLD_G){
    if(s_motion_above_count < kStartConfirmSamples){
      ++s_motion_above_count;
    }
    s_motion_below_count = 0u;
  } else {
    if(s_motion_below_count < kStopConfirmSamples){
      ++s_motion_below_count;
    }
    s_motion_above_count = 0u;
  }

  s_status.motion_start_confirmed = (s_motion_above_count >= kStartConfirmSamples);
  s_status.motion_stop_confirmed = (s_motion_below_count >= kStopConfirmSamples);
}

static void flight_end_reset_(void){
  s_flight_end_initialized = false;
  s_flight_hirms_min_g = 0.0f;
  s_flight_hirms_max_g = 0.0f;
  s_hirms_event_active = false;
  s_hirms_event_count = 0u;
  s_hirms_event_saw_flight_fg = false;
  s_valid_landing_event = false;
  s_landing_event_age = 0u;
  s_ground_candidate = false;
  s_fg_low_count = 0u;
  s_ground_count = 0u;
  s_status.flight_end_confirmed = false;
}

static void flight_end_update_(bool hirms_above){
  if(!s_status.flight_seen || s_status.flight_end_confirmed){
    return;
  }

  if(!s_flight_end_initialized){
    s_flight_hirms_min_g = s_status.hirms_g;
    s_flight_hirms_max_g = s_status.hirms_g;
    s_flight_end_initialized = true;
  }

  if(s_status.hirms_g < s_flight_hirms_min_g){
    s_flight_hirms_min_g = s_status.hirms_g;
  }
  if(s_status.hirms_g > s_flight_hirms_max_g){
    s_flight_hirms_max_g = s_status.hirms_g;
  }

  const float hirms_range_g = s_flight_hirms_max_g - s_flight_hirms_min_g;
  float flight_ground = 1.0f;
  if(hirms_range_g >= (float)AUTO_FLIGHT_END_HIRMS_RANGE_MIN_G){
    flight_ground = (s_status.lowrms_g - s_status.hirms_g) / hirms_range_g;
    if(!isfinite(flight_ground) || (flight_ground < 0.0f)){
      flight_ground = 0.0f;
    }
  }

  // Keep the post-event validity clock running while a provisional GROUND
  // candidate is active. A landing bounce may cancel GROUND while the same
  // HIRMS event is still recent enough to qualify a later low-FG transition.
  if(s_valid_landing_event){
    if(s_landing_event_age < kFlightEndEventWindowSamples){
      ++s_landing_event_age;
    } else {
      s_valid_landing_event = false;
      s_landing_event_age = 0u;
      s_fg_low_count = 0u;
    }
  }

  // Once a GROUND candidate exists, only a clear return to the FLIGHT side of
  // the hysteresis cancels it. Otherwise continuous GROUND time accumulates.
  if(s_ground_candidate){
    if(flight_ground >= (float)AUTO_FLIGHT_END_FG_FLIGHT_NORM){
      s_ground_candidate = false;
      s_ground_count = 0u;
      s_fg_low_count = 0u;
    } else {
      if(s_ground_count < kFlightEndGroundConfirmSamples){
        ++s_ground_count;
      }
      if(s_ground_count >= kFlightEndGroundConfirmSamples){
        s_status.flight_end_confirmed = true;
      }
    }
    return;
  }

  // HIRMS is treated as an ordered event. A valid event must last long enough
  // and must itself have seen FlightGround clearly on the FLIGHT side. Short
  // later HIRMS blips do not erase a previously valid post-event window.
  if(hirms_above){
    if(!s_hirms_event_active){
      s_hirms_event_active = true;
      s_hirms_event_count = 0u;
      s_hirms_event_saw_flight_fg = false;
    }
    if(s_hirms_event_count < kFlightEndHirmsEventConfirmSamples){
      ++s_hirms_event_count;
    }
    if(flight_ground >= (float)AUTO_FLIGHT_END_FG_FLIGHT_NORM){
      s_hirms_event_saw_flight_fg = true;
    }
    s_fg_low_count = 0u;
  } else if(s_hirms_event_active){
    if((s_hirms_event_count >= kFlightEndHirmsEventConfirmSamples) &&
       s_hirms_event_saw_flight_fg){
      s_valid_landing_event = true;
      s_landing_event_age = 0u;
    }
    s_hirms_event_active = false;
    s_hirms_event_count = 0u;
    s_hirms_event_saw_flight_fg = false;
  }

  // The low-FG confirmation is explicitly post-HIRMS. This prevents a large
  // HIRMS transient (HIRMS > LOWRMS) from manufacturing GROUND merely because
  // negative LOWRMS-HIRMS is clipped to zero.
  if(s_valid_landing_event && !hirms_above &&
     (flight_ground <= (float)AUTO_FLIGHT_END_FG_GROUND_NORM)){
    if(s_fg_low_count < kFlightEndFgConfirmSamples){
      ++s_fg_low_count;
    }
    if(s_fg_low_count >= kFlightEndFgConfirmSamples){
      s_ground_candidate = true;
      // The 2 s low-FG confirmation is already observed GROUND time, so count
      // it toward the 50 s persistence requirement rather than starting over.
      s_ground_count = kFlightEndFgConfirmSamples;
    }
  } else {
    s_fg_low_count = 0u;
  }
}


static void flight_signal_update_(const accel_sample_t *sample, bool recording_active){
  const float ax = (float)sample->ax * 0.001f;
  const float ay = (float)sample->ay * 0.001f;
  const float az = (float)sample->az * 0.001f;
  const float modulus = sqrtf(ax * ax + ay * ay + az * az);

  const float high = butterworth4_process_(&s_hirms_hp3, modulus);
  const float low_high_passed = butterworth4_process_(&s_lowrms_hp025, modulus);
  const float low = butterworth4_process_(&s_lowrms_lp3, low_high_passed);
  s_status.hirms_g = trailing_rms_update_(&s_hirms_rms, high);
  s_status.lowrms_g = trailing_rms_update_(&s_lowrms_rms, low);

  const bool hirms_above = (s_status.hirms_g >= (float)AUTO_FLIGHT_HIRMS_G);
  if(!recording_active){
    return;
  }

  // Primary normal-takeoff path: a HIRMS event is remembered, then sustained
  // LOWRMS dominance confirms that the automatic recording contains a flight.
  if(hirms_above){
    s_had_hirms = true;
  }

  if(s_had_hirms &&
     ((s_status.lowrms_g - s_status.hirms_g) >= (float)AUTO_FLIGHT_PRIMARY_DELTA_G)){
    if(s_primary_count < kPrimaryConfirmSamples){
      ++s_primary_count;
    }
  } else {
    s_primary_count = 0u;
  }

  if(s_primary_count >= kPrimaryConfirmSamples){
    s_status.flight_seen = true;
  }

  // Secondary late-start path: sustained LOWRMS while HIRMS remains low marks
  // possible airborne motion. Flight is confirmed only by a later new upward
  // HIRMS crossing, corresponding to the landing/roll event in validation.
  if(!s_possible_flight){
    if((s_status.lowrms_g >= (float)AUTO_FLIGHT_LATE_LOWRMS_G) && !hirms_above){
      if(s_late_count < kLateConfirmSamples){
        ++s_late_count;
      }
      if(s_late_count >= kLateConfirmSamples){
        s_possible_flight = true;
      }
    } else {
      s_late_count = 0u;
    }
  }

  if(s_possible_flight && !s_previous_hirms_above && hirms_above){
    s_status.flight_seen = true;
  }

  s_previous_hirms_above = hirms_above;
  flight_end_update_(hirms_above);
}

static void reset_filters_(void){
  memset(s_motion_ring, 0, sizeof(s_motion_ring));
  s_motion_count = 0u;
  s_motion_head = 0u;
  s_motion_sum[0] = 0.0;
  s_motion_sum[1] = 0.0;
  s_motion_sum[2] = 0.0;
  s_motion_sum2 = 0.0;
  s_motion_above_count = 0u;
  s_motion_below_count = 0u;
  first_order_hp_init_(&s_attitude_hp_x, (float)AUTO_RECORD_ATTITUDE_HP_HZ);
  first_order_hp_init_(&s_attitude_hp_y, (float)AUTO_RECORD_ATTITUDE_HP_HZ);
  s_attitude_above_count = 0u;

  trailing_rms_init_(&s_hirms_rms, s_hirms_squares, kHirmsWindowSamples);
  trailing_rms_init_(&s_lowrms_rms, s_lowrms_squares, kLowrmsWindowSamples);
  butterworth4_init_(&s_hirms_hp3, false, 3.0f);
  butterworth4_init_(&s_lowrms_hp025, false, (float)AUTO_FLIGHT_LOWRMS_HP_HZ);
  butterworth4_init_(&s_lowrms_lp3, true, 3.0f);

  s_status.motion_rms_g = 0.0f;
  s_status.motion_start_confirmed = false;
  s_status.attitude_start_confirmed = false;
  s_status.motion_stop_confirmed = false;
  s_status.hirms_g = 0.0f;
  s_status.lowrms_g = 0.0f;
  s_status.flight_end_confirmed = false;
}

} // namespace

void automation_service_init(void){
  memset(&s_status, 0, sizeof(s_status));
  reset_filters_();
  automation_service_begin_recording();
  s_status.flight_seen = false;
}

void automation_service_update(const accel_sample_t *sample, bool recording_active){
  if(sample == nullptr){
    return;
  }

  motion_update_(sample);
  flight_signal_update_(sample, recording_active);
}

void automation_service_begin_recording(void){
  s_had_hirms = false;
  s_possible_flight = false;
  s_primary_count = 0u;
  s_late_count = 0u;
  s_motion_below_count = 0u;
  s_status.motion_stop_confirmed = false;
  s_status.flight_seen = false;
  s_previous_hirms_above = (s_status.hirms_g >= (float)AUTO_FLIGHT_HIRMS_G);
  flight_end_reset_();
}

void automation_service_reset_start_confirmation(void){
  s_motion_above_count = 0u;
  s_attitude_above_count = 0u;
  s_status.motion_start_confirmed = false;
  s_status.attitude_start_confirmed = false;
}

void automation_service_reset_signal_history(void){
  reset_filters_();
  automation_service_begin_recording();
}

automation_status_t automation_service_status(void){
  return s_status;
}

automation_debug_status_t automation_service_debug_status(void){
  // Diagnostic snapshot is derived on demand. The State task calls this after
  // the current recording sample has already been pushed to the ring, so no
  // diagnostic bookkeeping is added to the acquisition/filter critical path.
  automation_debug_status_t out = {};
  out.hirms_above = (s_status.hirms_g >= (float)AUTO_FLIGHT_HIRMS_G);
  out.primary_confirming = (s_primary_count > 0u);
  out.primary_confirmed = (s_primary_count >= kPrimaryConfirmSamples);
  out.possible_flight = s_possible_flight;
  out.hirms_event_active = s_hirms_event_active;
  out.hirms_event_duration_ok =
      s_hirms_event_active &&
      (s_hirms_event_count >= kFlightEndHirmsEventConfirmSamples);
  out.hirms_event_saw_flight_fg = s_hirms_event_saw_flight_fg;
  out.valid_landing_event = s_valid_landing_event;
  out.fg_low_confirming = (s_fg_low_count > 0u);
  out.ground_candidate = s_ground_candidate;
  out.hirms_event_count_samples = s_hirms_event_count;
  out.landing_event_age_samples = s_landing_event_age;
  out.fg_low_count_samples = s_fg_low_count;
  out.ground_count_samples = s_ground_count;

  out.flight_ground_norm = 1.0f;
  if(s_flight_end_initialized){
    const float range_g = s_flight_hirms_max_g - s_flight_hirms_min_g;
    if(range_g >= (float)AUTO_FLIGHT_END_HIRMS_RANGE_MIN_G){
      float fg = (s_status.lowrms_g - s_status.hirms_g) / range_g;
      if(!isfinite(fg) || (fg < 0.0f)){
        fg = 0.0f;
      }
      out.flight_ground_norm = fg;
    }
  }

  return out;
}
