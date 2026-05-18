// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/ring_buffer.cpp
 * @brief Fixed-size ring buffer for accelerometer record blocks.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#include "src/services/ring_buffer.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static record_block_t s_buf[CFG_RING_BUFFER_CAPACITY_ITEMS];
static volatile size_t s_head = 0u;
static volatile size_t s_tail = 0u;
static volatile uint32_t s_overflow = 0u;

static portMUX_TYPE s_rb_mux = portMUX_INITIALIZER_UNLOCKED;

/**
 * Maintains the fixed-size ring buffer used to decouple acceleration
 * acquisition from SD writing.
 *
 * Inputs: `v`.
 * Returns: Next index in the ring buffer.
 */
static inline size_t next_idx(size_t v){ return (v + 1u) % CFG_RING_BUFFER_CAPACITY_ITEMS; }

/**
 * Initializes ring buffer init state or hardware resources and prepares the
 * module for later recorder operation.
 *
 * Inputs: None.
 * Returns: None.
 */
void ring_buffer_init(void){
  s_head = 0u;
  s_tail = 0u;
  s_overflow = 0u;
  memset(s_buf, 0, sizeof(s_buf));
}

/**
 * Maintains the fixed-size ring buffer used to decouple acceleration
 * acquisition from SD writing.
 *
 * Inputs: `blk`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool ring_buffer_push(const record_block_t *blk){
  if(!blk) return false;

  bool ok = false;
  portENTER_CRITICAL(&s_rb_mux);
  size_t h = (size_t)s_head;
  size_t n = next_idx(h);
  if(n == (size_t)s_tail){
    s_overflow++;
    ok = false;
  }else{
    s_buf[h] = *blk;
    s_head = n;
    ok = true;
  }
  portEXIT_CRITICAL(&s_rb_mux);
  return ok;
}

/**
 * Maintains the fixed-size ring buffer used to decouple acceleration
 * acquisition from SD writing.
 *
 * Inputs: `out`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool ring_buffer_pop(record_block_t *out){
  if(!out) return false;

  bool ok = false;
  portENTER_CRITICAL(&s_rb_mux);
  size_t t = (size_t)s_tail;
  if(t == (size_t)s_head){
    ok = false;
  }else{
    *out = s_buf[t];
    s_tail = next_idx(t);
    ok = true;
  }
  portEXIT_CRITICAL(&s_rb_mux);
  return ok;
}

/**
 * Maintains the fixed-size ring buffer used to decouple acceleration
 * acquisition from SD writing.
 *
 * Inputs: None.
 * Returns: None.
 */
void ring_buffer_reset(void){
  portENTER_CRITICAL(&s_rb_mux);
  s_head = 0u;
  s_tail = 0u;
  s_overflow = 0u;
  portEXIT_CRITICAL(&s_rb_mux);
}

/**
 * Returns the requested ring buffer get overflow count information from the
 * module state or underlying driver interface.
 *
 * Inputs: None.
 * Returns: Requested numeric value.
 */
uint32_t ring_buffer_get_overflow_count(void){
  uint32_t v;
  portENTER_CRITICAL(&s_rb_mux);
  v = s_overflow;
  portEXIT_CRITICAL(&s_rb_mux);
  return v;
}
