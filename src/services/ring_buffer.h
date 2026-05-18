// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/ring_buffer.h
 * @brief Public ring-buffer API for producer/consumer recording flow.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once
// Ring buffer for fixed-size record blocks.
//
// Determinism notes (DO-178C prep):
// - Capacity is fixed by CFG_RING_BUFFER_CAPACITY_ITEMS and storage is static.
// - No dynamic memory allocation is performed.
// - Push/pop operations are O(1) with bounded execution time.
// - Concurrency model: single-core critical section (portENTER_CRITICAL) protects
//   indices and element writes/reads. This is suitable for bounded latency and
//   prevents tearing without seqlocks or heap use.
// - Overflow behavior is deterministic: when full, push fails and an overflow
//   counter is incremented; existing elements are preserved.
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "src/global.h"

typedef struct __attribute__((packed)) {
  uint8_t  sync;
  uint8_t  id;
  int32_t  ts_ms;
  int16_t  ax;
  int16_t  ay;
  int16_t  az;
  uint8_t  checksum;
} record_block_t;

/**
 * @brief Ring buffer init.
 *
 * Inputs: None.
 * Returns: None.
 */
void ring_buffer_init(void);
/**
 * @brief Ring buffer push.
 *
 * Inputs: `blk`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool ring_buffer_push(const record_block_t *blk);
/**
 * @brief Ring buffer pop.
 *
 * Inputs: `out`.
 * Returns: `true` when the requested condition or operation succeeds; otherwise `false`.
 */
bool ring_buffer_pop(record_block_t *out);
/**
 * @brief Ring buffer reset.
 *
 * Inputs: None.
 * Returns: None.
 */
void ring_buffer_reset(void);
/**
 * @brief Ring buffer get overflow count.
 *
 * Inputs: None.
 * Returns: Requested numeric value.
 */
uint32_t ring_buffer_get_overflow_count(void);
