// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

#include "src/services/language.h"

typedef struct {
  const char *key;
  const char *english;
  const char *french;
} language_row_t;

static const language_row_t kLanguageRows[] = {
#define LANGUAGE_TABLE_ROW(id, en, fr) { #id, en, fr },
  LANGUAGE_TEXT_LIST(LANGUAGE_TABLE_ROW)
#undef LANGUAGE_TABLE_ROW
};

static_assert((sizeof(kLanguageRows) / sizeof(kLanguageRows[0])) == (size_t)TXT_COUNT,
              "language table and enum must stay aligned");

bool language_valid(language_t language){
  return (language == LANGUAGE_FRENCH) || (language == LANGUAGE_ENGLISH);
}

const char *language_text(language_text_id_t id, language_t language){
  size_t index = (size_t)id;
  if(index >= (size_t)TXT_COUNT){
    index = (size_t)TXT_EMPTY;
  }
  if(language == LANGUAGE_ENGLISH){
    return kLanguageRows[index].english;
  }
  return kLanguageRows[index].french;
}

const char *language_key(language_text_id_t id){
  const size_t index = (size_t)id;
  if(index >= (size_t)TXT_COUNT){
    return kLanguageRows[(size_t)TXT_EMPTY].key;
  }
  return kLanguageRows[index].key;
}

size_t language_text_count(void){
  return (size_t)TXT_COUNT;
}
