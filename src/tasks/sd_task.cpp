// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/tasks/sd_task.cpp
 * @brief SD task state machine for recorder file lifecycle, writes, status checks, and file requests.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/tasks/sd_task.h"
#include "src/services/sd_files.h"
#include "src/global.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "src/drivers/sd_storage.h"
#include "src/services/ring_buffer.h"
#include "src/services/settings_store.h"
#include "src/services/timebase.h"
#include "src/services/record_format.h"
#include "src/services/calibration_service.h"
#include "src/services/task_helpers.h"
#include "src/services/watchdog_service.h"
#include "config.h"

// SD error state exported to state_task through semantic getters.
static error_code_t s_sd_error = ERR_NONE;
static bool s_sd_error_clearable = false;

static bool sd_error_is_maintenance_(error_code_t err){
  return (err == ERR_SD_FILES_FULL);
}

// Request latches written by state_task and consumed by sd_task.
static volatile bool s_open_requested = false;
static volatile bool s_close_requested = false;
static volatile bool s_error_ack_requested = false;

// SD task state-machine runtime.
static sd_state_t s_sd_state = SD_BOOT;
static uint32_t s_state_tick = 0u;
static uint32_t s_sd_tick = 0u;

// Recording/write status owned by sd_task.
static uint8_t s_write_fail_consec = 0u;
static uint64_t s_remaining_bytes = 0u;

/**
 * Performs sd time due for SD storage, recording files, or SD-backed web file
 * management while preserving SD ownership rules.
 *
 * Inputs: `period_ms`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static inline bool sd_time_due(uint32_t period_ms){
  if(period_ms == 0u){
    return false;
  }

  const uint32_t period_ticks =
      (period_ms <= SD_TASK_PERIOD_MS)
          ? 1u
          : ((period_ms + SD_TASK_PERIOD_MS - 1u) / SD_TASK_PERIOD_MS);

  return (s_sd_tick % period_ticks) == 0u;
}

/**
 * Performs sd remaining bytes update for SD storage, recording files, or SD-
 * backed web file management while preserving SD ownership rules.
 *
 * Inputs: `written_bytes`.
 * Returns: None.
 */
static void sd_remaining_bytes_update(uint32_t written_bytes){
  if(written_bytes >= s_remaining_bytes){
    s_remaining_bytes = 0u;
  } else {
    s_remaining_bytes -= (uint64_t)written_bytes;
  }
}

/**
 * Updates sd remaining bytes set state and applies the change to the owning
 * module or hardware interface.
 *
 * Inputs: `value`.
 * Returns: None.
 */
static void sd_remaining_bytes_set(uint64_t value){
  s_remaining_bytes = value;
}

/**
 * Updates sd state set state and applies the change to the owning module or
 * hardware interface.
 *
 * Inputs: `next_state`.
 * Returns: None.
 */
static inline void sd_state_set(sd_state_t next_state){
  if(s_sd_state != next_state){
    s_sd_state = next_state;
    s_state_tick = 0u;
  }
}

/**
 * Performs sd state elapsed ms for SD storage, recording files, or SD-backed
 * web file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: Requested numeric value.
 */
static inline uint32_t sd_state_elapsed_ms(void){
  return s_state_tick * SD_TASK_PERIOD_MS;
}





/**
 * Updates sd error set state and applies the change to the owning module or
 * hardware interface.
 *
 * Inputs: `err`.
 * Returns: None.
 */
static void sd_error_set(error_code_t err){
  if(err == ERR_NONE){
    err = ERR_SD_FAULT;
  }

  s_sd_error = err;
  s_sd_error_clearable = false;

  // Entering SD_ERROR aborts the active SD operation. Pending open/close
  // requests belong to the aborted operation and must not be replayed after
  // recovery.
  s_open_requested = false;
  s_close_requested = false;
  s_error_ack_requested = false;

  s_remaining_bytes = 0u;

  if(s_sd_state != SD_ERROR){
    sd_state_set(SD_ERROR);
  }
}

/**
 * Performs sd write flush for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool sd_write_flush(void){
  if(!sd_time_due(SD_RECORD_FLUSH_PERIOD_MS)){
    return true;
  }

  const error_code_t rc = sd_flush_record();
  if(rc == ERR_NONE){
    s_write_fail_consec = 0;
    return true;
  }

  s_write_fail_consec++;
  s_sd_error = (rc == ERR_SD_NO_CARD) ? ERR_SD_NO_CARD : ERR_SD_FAULT;
  return false;
}

/**
 * Writes a byte range to the active recording file, tracks remaining-space
 * estimates, and classifies repeated write failures.
 *
 * Inputs: `data`, `len`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool write_sd_bytes(const uint8_t *data, uint32_t len){
  uint32_t write_fail_count = 0u;

  for(;;){
    size_t written_bytes = 0u;
    const error_code_t rc = sd_write_record_block(data, (size_t)len, &written_bytes);

    // Preserve progress information even if the write was partial.
    if(written_bytes > 0u){
      sd_remaining_bytes_update((uint32_t)written_bytes);
    }

    if(rc == ERR_NONE){
      s_sd_error = ERR_NONE;
      s_write_fail_consec = 0u;
      return true;
    }

    ++write_fail_count;

    // No-card and low-space are classified conditions. Only generic SD
    // faults are retried locally.
    s_sd_error = ((rc == ERR_SD_NO_CARD) || (rc == ERR_SD_SPACE_LOW)) ? rc : ERR_SD_FAULT;

    if((s_sd_error != ERR_SD_FAULT) ||
       (write_fail_count >= (uint32_t)SD_WRITE_RETRY_MAX)){
      break;
    }
  }

  s_write_fail_consec = (uint8_t)write_fail_count;
  return false;
}

/**
 * Writes the recording status block to the file during the close path so post-
 * processing can inspect close/overflow information.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool write_status_block(void){
  record_status_block_t blk;
  record_format_build_status_block(&blk, ring_buffer_get_overflow_count());
  return write_sd_bytes((const uint8_t*)&blk, (uint32_t)sizeof(blk));
}

// Append the calibration block at the start of the recording file, before any
// acceleration blocks. Recording authorization should guarantee that an active
// calibration exists; this guard protects the file format if that assumption is
// ever violated.
/**
 * Writes the active calibration block to the newly opened recording file
 * before any acceleration data blocks are written.
 *
 * Inputs: None.
 * Returns: `true` when calibration block was written; otherwise `false`.
 */
static bool write_calibration_block(void){
  calibration_record_t cal = {};
  if(!calibration_service_get_active(&cal)){
    s_sd_error = ERR_SD_FAULT;
    return false;
  }

  record_calibration_block_t blk = {};
  record_format_build_calibration_block(&blk, &cal);
  return write_sd_bytes((const uint8_t*)&blk, (uint32_t)sizeof(blk));
}

/**
 * Closes the active recording file after flushing and writing final status
 * information, then resets SD task file state.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
static bool sd_close_file(void){
  const error_code_t rc = sd_close_record();

  if(rc == ERR_NONE){
    s_sd_error = ERR_NONE;
    s_write_fail_consec = 0u;
    return true;
  }

  s_sd_error = ERR_SD_FAULT;
  s_write_fail_consec = 1u;
  return false;
}

/**
 * SD task loop owns SD-card probing, recording file open/write/close requests,
 * recoverable SD error handling, and recording data draining from the ring
 * buffer.
 *
 * Inputs: `arg`.
 * Returns: None.
 */
static void sd_task_loop(void *arg){
  (void)arg;

  s_sd_error = ERR_NONE;
  s_error_ack_requested = false;
  s_remaining_bytes = 0u;
  s_sd_tick = 0u;
  s_state_tick = 0u;
  s_sd_error_clearable = false;

  for(;;){
    watchdog_kick(WD_SD);

    s_sd_tick++;
    s_state_tick++;

    // Service exactly one SD mode per loop. Recording states own the SD
    // driver and block filesystem-management work until recording is closed.
    switch(s_sd_state){
      case SD_BOOT:
        // Purpose: initialize or reinitialize SD storage before idle/record use.

        // Reset recovery/write state before probing the card again.
        s_error_ack_requested = false;
        s_write_fail_consec = 0u;
        s_sd_error = ERR_NONE;

        (void)sd_reinit();

        // Full status check is used here because BOOT/IDLE are allowed to
        // validate both recording space and file-count policy.
        {
          const error_code_t boot_status_rc = sd_status_check(SD_STATUS_ALL);
          if(boot_status_rc != ERR_NONE){
            sd_error_set(boot_status_rc);
            break;
          }
        }

        sd_state_set(SD_IDLE);
        break;

      case SD_IDLE:
        // Purpose: keep SD ready while idle and service non-recording file
        // requests. Support file operations are intentionally not serviced
        // while recording is active.

        // Periodically re-probe while idle so card insertion/removal or a full
        // root directory is detected before a recording start request.
        if(sd_time_due(SD_IDLE_REPROBE_PERIOD_MS)){
          (void)sd_reinit();

          const error_code_t idle_status_rc = sd_status_check(SD_STATUS_ALL);
          if(idle_status_rc != ERR_NONE){
            sd_error_set(idle_status_rc);
            break;
          }
        }

        // Recording open request has priority over support file operations.
        if(s_open_requested){
          s_open_requested = false;
          sd_state_set(SD_OPEN);
          break;
        }

        // Support Web/UI file requests are serviced only from SD_IDLE.
        sd_file_ops_service();
        break;

      case SD_OPEN: {
        // Purpose: create/open a new recording file, then enter write mode.

        // SD task owns the start/open timeout so state_task cannot wait forever
        // in ST_STARTING.
        if(sd_state_elapsed_ms() > CFG_STARTING_TIMEOUT_MS){
          sd_error_set(ERR_SD_FAULT);
          break;
        }

        // Opening is allowed only when settings are available and a recording
        // filename can be generated.
        settings_t sett = {};
        if(!settings_get(&sett)){
          sd_error_set(ERR_SD_FAULT);
          break;
        }

        char fn[FILENAME_MAX_LENGTH];
        if(!record_filename(fn,
                            sizeof(fn),
                            sett.registration,
                            timebase_get_datetime_compact())){
          sd_error_set(ERR_SD_FAULT);
          break;
        }

        // Final guard before file creation. This scope checks card presence
        // and free space, but intentionally does not re-count root files.
        const error_code_t status_rc = sd_status_check(SD_STATUS_PRESENCE_SPACE);
        if(status_rc != ERR_NONE){
          sd_error_set(status_rc);
          break;
        }

        const error_code_t open_rc = sd_open_record(fn);
        if(open_rc != ERR_NONE){
          sd_error_set(open_rc);
          break;
        }

        s_write_fail_consec = 0u;
        s_sd_error = ERR_NONE;

        // The calibration block is the first block in every recording file.
        // It captures the active calibration that will be used for corrected
        // acceleration samples in this recording.
        if(!write_calibration_block()){
          (void)sd_close_record();
          sd_error_set(s_sd_error != ERR_NONE ? s_sd_error : ERR_SD_FAULT);
          break;
        }

        sd_state_set(SD_WRITING);
        break;
      }

      case SD_WRITING:
        // Purpose: drain the ring buffer and append record blocks to the open
        // recording file.

        record_block_t blk;

        // Drain all currently available record blocks. Acquisition and SD write
        // are decoupled by the ring buffer.
        while(ring_buffer_pop(&blk)){
          if(!write_sd_bytes((const uint8_t*)&blk, (uint32_t)sizeof(blk))){
            // Known no-card faults and repeated generic I/O faults go directly
            // to SD_ERROR. Low-space keeps the file open and exits through
            // SD_CLOSING so the final status block can still be attempted.
            if(s_sd_error == ERR_SD_NO_CARD ||
               s_write_fail_consec >= SD_IO_FAIL_LIMIT){
              sd_error_set(s_sd_error);
            }
            break;
          }
        }

        // Flush on cadence after draining available blocks. Flush failures use
        // the same terminal classification as write failures.
        if((s_sd_state != SD_ERROR) && !sd_write_flush()){
          if(s_sd_error == ERR_SD_NO_CARD ||
             s_write_fail_consec >= SD_IO_FAIL_LIMIT){
            sd_error_set(s_sd_error);
          }
        }

        // Normal close requests and low-space stop requests both go through
        // SD_CLOSING. Low-space is preserved as the close reason and reported
        // after the file is closed.
        if((s_sd_state != SD_ERROR) &&
           (s_sd_error == ERR_SD_SPACE_LOW || s_close_requested)){
          s_close_requested = false;
          sd_state_set(SD_CLOSING);
        }
        break;

      case SD_CLOSING: {
        // Purpose: normal recording stop path. Append the final status block,
        // close the file, then either return to IDLE or report the close reason.

        const error_code_t close_reason = s_sd_error;
        s_close_requested = false;

        // Timeout prevents state_task from waiting indefinitely for close.
        if(sd_state_elapsed_ms() > CFG_CLOSING_TIMEOUT_MS){
          sd_error_set(ERR_SD_FAULT);
          break;
        }

        // Final status block includes recording summary such as ring-buffer
        // overflow count. It is still attempted for low-space closure because
        // the low-space threshold should leave enough room.
        if(!write_status_block()){
          sd_error_set(s_sd_error != ERR_NONE ? s_sd_error : ERR_SD_FAULT);
          break;
        }

        if(!sd_close_file()){
          sd_error_set(s_sd_error != ERR_NONE ? s_sd_error : ERR_SD_FAULT);
          break;
        }

        s_write_fail_consec = 0u;
        s_remaining_bytes = 0u;

        // If closing was requested because of a recoverable SD condition, report
        // that condition after the file has been closed. Normal stop returns idle.
        if(close_reason != ERR_NONE){
          sd_error_set(close_reason);
        } else {
          s_sd_error = ERR_NONE;
          sd_state_set(SD_IDLE);
        }
        break;
      }

      case SD_ERROR:
        // Purpose: own SD recovery probing and wait for operator acknowledgement.
        // state_task displays/acknowledges SD errors through error_manager.
        // Low-space and max-file-count are maintenance conditions: support file
        // operations remain available so Web maintenance can archive files.

        if(sd_error_is_maintenance_(s_sd_error)){
          sd_file_ops_service();
        }

        // Re-probe periodically; do not spin on SD/MMC.
        if(!sd_time_due(SD_ERROR_REPROBE_PERIOD_MS)){
          break;
        }

        (void)sd_reinit();

        // If SD is still bad, update the visible SD error classification and
        // keep the error not clearable. Maintenance errors remain in SD_ERROR
        // but continue to service file operations.
        {
          const error_code_t error_status_rc = sd_status_check(SD_STATUS_ALL);
          if(error_status_rc != ERR_NONE){
            s_sd_error = error_status_rc;
            s_sd_error_clearable = false;
            break;
          }
        }

        // SD now looks healthy. Maintenance recovery is automatic because the
        // operator already fixed the condition through file maintenance.
        if(sd_error_is_maintenance_(s_sd_error)){
          s_sd_error = ERR_NONE;
          s_sd_error_clearable = false;
          sd_state_set(SD_BOOT);
          break;
        }

        // Other recoverable SD errors still expose OK/CLR and wait for the
        // operator acknowledgement.
        s_sd_error_clearable = true;

        if(!s_error_ack_requested){
          break;
        }
        s_error_ack_requested = false;

        // Close any leftover record object best-effort, then restart the SD
        // state machine through SD_BOOT.
        (void)sd_close_record();

        s_sd_error = ERR_NONE;
        s_sd_error_clearable = false;
        sd_state_set(SD_BOOT);
        break;

      default:
        break;
    }

    vTaskDelay(pdMS_TO_TICKS(SD_TASK_PERIOD_MS));
  }
}


// =============================================================================
// Public API
// =============================================================================

/**
 * Performs sd request open for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_request_open(void){
  s_open_requested = true;
}

/**
 * Performs sd request close for SD storage, recording files, or SD-backed web
 * file management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_request_close(void){
  s_close_requested = true;
}

/**
 * Returns the requested sd error get information from the module state or
 * underlying driver interface.
 *
 * Inputs: None.
 * Returns: `ERR_NONE` on success; otherwise an error code that explains the failure.
 */
error_code_t sd_error_get(void){
  if(s_sd_state != SD_ERROR){
    return ERR_NONE;
  }

  return s_sd_error;
}

/**
 * Classifies, stores, reports, or clears sd request ack error information used
 * by the recorder error-handling path.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_request_ack_error(void){
  s_error_ack_requested = true;
}

/**
 * Clears sd error show ok clear state, latches, or stored data so the owning
 * module returns to its idle/default condition.
 *
 * Inputs: None.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool sd_error_show_ok_clear(void){
  return s_sd_error_clearable;
}

/**
 * Performs sd is open for SD storage, recording files, or SD-backed web file
 * management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `true` when the requested operation succeeds or condition is met; otherwise `false`.
 */
bool sd_is_open(void){ return (s_sd_state == SD_WRITING); }
/**
 * Performs sd is closed for SD storage, recording files, or SD-backed web file
 * management while preserving SD ownership rules.
 *
 * Inputs: None.
 * Returns: `true` when no recording file is open; otherwise `false`.
 */
bool sd_is_closed(void){ return (s_sd_state == SD_IDLE); }

/**
 * Initializes sd task init state or hardware resources and prepares the module
 * for later recorder operation.
 *
 * Inputs: None.
 * Returns: None.
 */
void sd_task_init(void){
  const BaseType_t create_ok = xTaskCreatePinnedToCore(
      sd_task_loop,
      "sd_task",
      CFG_SD_TASK_STACK_WORDS,
      nullptr,
      CFG_SD_TASK_PRIO,
      nullptr,
      CFG_SD_TASK_CORE);
  if(create_ok != pdPASS){
    task_create_failed_reboot("sd_task");
  }

}
