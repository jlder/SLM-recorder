// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/task_helpers.h
 * @brief Public task helper API.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

#pragma once

/*
 * Fatal task-creation failure handling. At this point the scheduler/product
 * cannot run correctly; Serial is the only feedback channel likely to work.
 */
void task_create_failed_reboot(const char *task_name);
