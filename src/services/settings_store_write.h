// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/settings_store_write.h
 * @brief State-task-only persistent settings write API.
 *
 * Runtime callers other than state_task.cpp shall use settings_store.h reads
 * and post requests to the State task instead of writing NVS directly.
 */

#pragma once
#include "src/services/settings_store.h"

bool settings_set_registration(const char *reg);
bool settings_set_wifi_password(const char *pwd);
bool settings_set_date_set(bool done);
bool settings_set_time_set(bool done);
bool settings_set_language(language_t language);
bool settings_set_auto_recording(bool enabled);
bool settings_set_auto_wifi(bool enabled);
bool settings_set_auto_delete(bool enabled);
bool settings_clear(void);
