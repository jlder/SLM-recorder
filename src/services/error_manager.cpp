// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/error_manager.cpp
 * @brief Error manager that maps active error codes to recovery and display behavior.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/services/error_manager.h"

// Ownership note:
// This module is a latch and mapping table only. State task owns the policy
// decision to raise, clear, and update active user-visible errors.
// Table-driven, single-active-error manager.
// Policy:
//  - Last error wins (only one active error is tracked).
//  - Error remains latched until user acknowledgement.
//  - Even if the condition disappears, the error does not clear automatically;
//    instead UI shows a per-error "OK/CLR" message (msg_clear).

typedef struct {
  error_code_t err;
  msg_id_t     msg_set;     // shown while condition is present
  msg_id_t     msg_clear;   // shown when condition gone (recoverable only)
  bool         recoverable; // if false, msg_clear must be MSG_NONE
} error_def_t;

// All error-to-UI mappings shall be defined in a single table.
static const error_def_t s_err_table[] = {
  // SD / storage
  { ERR_SD_NO_CARD,      MSG_NO_SD,         MSG_SD_OK_CLR,   true  },
  { ERR_SD_SPACE_LOW,    MSG_SD_LOW_SPACE, MSG_SD_OK_CLR,   true  },
  { ERR_SD_FILES_FULL,   MSG_SD_FULL_FILES,MSG_SD_OK_CLR,   true  },
  { ERR_SD_FAULT,        MSG_SD_ERROR,     MSG_NONE,        false },

  // Hardware / runtime
  { ERR_ACCEL_NO_RESPONSE,   MSG_ACCEL_ERROR, MSG_NONE,   false },
  { ERR_RINGBUFFER_OVERFLOW, MSG_RECORD_FAIL, MSG_NONE,   false },
  { ERR_RTC_INVALID,         MSG_RTC_ERROR,   MSG_NONE,   false },
  { ERR_PMU_FAULT,           MSG_PMU_ERROR,   MSG_NONE,   false },
  { ERR_TOUCH_FAULT,         MSG_TOUCH_ERROR, MSG_NONE,   false },
  { ERR_CALIBRATION_FAULT,  MSG_CALIBRATION_FAULT, MSG_NONE, false },

  // Generic fatal
  { ERR_FATAL_GENERIC,       MSG_FATAL,       MSG_NONE,   false },
};

/**
 * Find def performs the error manager operation represented by this function
 * and keeps the module state consistent with recorder ownership rules.
 *
 * Inputs: `err`.
 * Returns: Pointer to the requested object or string; may be `nullptr` when unavailable.
 */
static const error_def_t* find_def(error_code_t err){
  for(unsigned i=0; i<(unsigned)(sizeof(s_err_table)/sizeof(s_err_table[0])); i++){
    if(s_err_table[i].err == err) return &s_err_table[i];
  }
  return (const error_def_t*)0;
}

static error_code_t s_active_err = ERR_NONE;
static bool s_clearable = false;

/**
 * Classifies, stores, reports, or clears error manager is sd error information
 * used by the recorder error-handling path.
 *
 * Inputs: `code`.
 * Returns: `true` for SD/storage error codes; otherwise `false`.
 */
bool error_manager_is_sd_error(error_code_t code){
  switch(code){
    case ERR_SD_NO_CARD:
    case ERR_SD_FILES_FULL:
    case ERR_SD_SPACE_LOW:
    case ERR_SD_FAULT:
      return true;
    default:
      return false;
  }
}




/**
 * Returns the requested error manager get active information from the module
 * state or underlying driver interface.
 *
 * Inputs: None.
 * Returns: Active error code, or `ERR_NONE` when no error is active.
 */
error_code_t error_manager_get_active(void){ return s_active_err; }

/**
 * Classifies, stores, reports, or clears error manager raise information used
 * by the recorder error-handling path.
 *
 * Inputs: `code`.
 * Returns: None.
 */
void error_manager_raise(error_code_t code){
  if(code == ERR_NONE) return;
  // Last error wins.
  s_active_err = code;
  // Reset clearable: caller must re-evaluate the condition.
  s_clearable = false;
}

/**
 * Updates error manager set clearable state and applies the change to the
 * owning module or hardware interface.
 *
 * Inputs: `clearable`.
 * Returns: None.
 */
void error_manager_set_clearable(bool clearable){
  s_clearable = clearable;
}

/**
 * Is recoverable performs the error manager operation represented by this
 * function and keeps the module state consistent with recorder ownership
 * rules.
 *
 * Inputs: `err`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool is_recoverable(error_code_t err){
  const error_def_t* d = find_def(err);
  return (d && d->recoverable);
}

/**
 * Clears error manager can clear state, latches, or stored data so the owning
 * module returns to its idle/default condition.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool error_manager_can_clear(void){
  if(s_active_err == ERR_NONE) return false;
  if(!is_recoverable(s_active_err)) return false;
  if(!s_clearable) return false;
  return true;
}

/**
 * Clears error manager clear active state, latches, or stored data so the
 * owning module returns to its idle/default condition.
 *
 * Inputs: None.
 * Returns: None.
 */
void error_manager_clear_active(void){
  s_active_err = ERR_NONE;
  s_clearable  = false;
}


/**
 * Returns the requested error manager get display message information from the
 * module state or underlying driver interface.
 *
 * Inputs: None.
 * Returns: Message identifier selected for display.
 */
msg_id_t error_manager_get_display_message(void){
  if(s_active_err == ERR_NONE) return MSG_NONE;
  const error_def_t* d = find_def(s_active_err);
  if(!d) return MSG_ERROR;

  if(s_clearable && d->recoverable && d->msg_clear != MSG_NONE) return d->msg_clear;
  return d->msg_set;
}
