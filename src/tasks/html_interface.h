// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/tasks/html_interface.h
 * @brief Assembles the embedded HTML, CSS, and JavaScript for the recorder web interface.
 *
 * @details The recorder web page is served as one document.  The implementation
 * is split into source fragments under src/tasks/web_ui/ only to improve
 * reviewability.  The fragments are adjacent C++ raw string literals; the
 * compiler concatenates them into the PROGMEM segments declared below.
 *
 * Segmentation rationale (v1.50):
 * - the browser translation catalog is emitted for the recorder-selected
 *   language only, instead of shipping both languages on every request;
 * - the language code is injected server-side, so the page never renders in
 *   the wrong language and never re-translates itself after /api/status;
 * - the static text is applied immediately after the body markup instead of
 *   after the last script fragment, so the page paints with its labels.
 *
 * The segments are concatenated in this order by web_task.cpp:
 *   HTML_PAGE_A  |  HTML_TEXT_xx  |  HTML_PAGE_B  |  HTML_LANG_xx  |
 *   HTML_PAGE_C  |  HTML_PAGE_D
 *
 * Byte-stability rule:
 * - comments are placed outside the raw string fragments;
 * - edit inside web_ui/*.inc raw strings only when changing the web page itself.
 */

#ifndef HTML_INTERFACE_H
#define HTML_INTERFACE_H

#include <stddef.h>

#include "config.h"
#include "src/services/language.h"

#define SLM_HTML_STRINGIFY_IMPL(x) #x
#define SLM_HTML_STRINGIFY(x) SLM_HTML_STRINGIFY_IMPL(x)

// Build the browser translation catalog directly into the embedded HTML
// document.  language.h remains the only translation source, while the Web UI
// remains self-contained (no second HTTP resource is required).
// One row per language; a request carries a single language, so each entry is
// a plain string rather than an [English, French] pair.
#define SLM_WEB_ROW_EN(id, en, fr) ",\"" #id "\":\"" en "\""
#define SLM_WEB_ROW_FR(id, en, fr) ",\"" #id "\":\"" fr "\""

// ---------------------------------------------------------------------------
// Segment A: document head, styles, and the opening of the catalog object.
// ---------------------------------------------------------------------------
const char HTML_PAGE_A[] PROGMEM =
#include "web_ui/00_page_begin.inc"
#include "web_ui/01_styles.inc"
R"rawliteral(
    <script>
        window.SLM_TEXT={"_":""
)rawliteral";

// ---------------------------------------------------------------------------
// Catalog bodies and language codes.  Exactly one of each is sent per request.
// ---------------------------------------------------------------------------
const char HTML_TEXT_EN[] PROGMEM = LANGUAGE_TEXT_LIST(SLM_WEB_ROW_EN);
const char HTML_TEXT_FR[] PROGMEM = LANGUAGE_TEXT_LIST(SLM_WEB_ROW_FR);

const char HTML_LANG_EN[] PROGMEM = "en";
const char HTML_LANG_FR[] PROGMEM = "fr";

// ---------------------------------------------------------------------------
// Segment B: closes the catalog object and opens the language assignment.
// ---------------------------------------------------------------------------
const char HTML_PAGE_B[] PROGMEM = R"rawliteral(
        };
        window.SLM_LANGUAGE=')rawliteral";

// ---------------------------------------------------------------------------
// Segment C: translation helpers, the static body sections, and the early
// translation pass.  The pass runs while the parser is still ahead of the
// large script fragments, so the labels are present at first paint.
// ---------------------------------------------------------------------------
const char HTML_PAGE_C[] PROGMEM = R"rawliteral(';
        window.tr=function(k){
            const t=window.SLM_TEXT[k];
            return (t===undefined) ? k : t;
        };
        window.applyStaticTranslations=function(){
            document.documentElement.lang=window.SLM_LANGUAGE;
            document.querySelectorAll('[data-i18n]').forEach(function(e){
                e.textContent=window.tr(e.dataset.i18n);
            });
            document.querySelectorAll('[data-i18n-placeholder]').forEach(function(e){
                e.placeholder=window.tr(e.dataset.i18nPlaceholder);
            });
        };
        window.slmLanguageReloadPending=false;
        window.setSlmLanguage=function(code){
            const next=(code==='en') ? 'en' : 'fr';
            if(next===window.SLM_LANGUAGE) return false;
            if(window.slmLanguageReloadPending) return false;
            window.slmLanguageReloadPending=true;
            window.location.reload();
            return true;
        };
    </script>
</head>
)rawliteral"
#include "web_ui/02_body_home_files.inc"
#include "web_ui/03_body_maintenance_menu.inc"
#include "web_ui/04_body_calibration_pages.inc"
#include "web_ui/05_body_health_ota.inc"
R"rawliteral(    <script>window.applyStaticTranslations();</script>
)rawliteral";

// ---------------------------------------------------------------------------
// Segment D: the application scripts.
// ---------------------------------------------------------------------------
const char HTML_PAGE_D[] PROGMEM =
#include "web_ui/10_script_navigation_auth.inc"
#include "web_ui/11_script_files_download.inc"
#include "web_ui/12_script_flight_decode.inc"
#include "web_ui/13_script_signal_processing.inc"
#include "web_ui/14_script_format_helpers.inc"
#include "web_ui/15_script_archive_status_diag.inc"
#include "web_ui/16_script_calibration.inc"
#include "web_ui/17_script_ota_startup_end.inc"
;

#undef SLM_WEB_ROW_EN
#undef SLM_WEB_ROW_FR

// Compile-time segment lengths.  Used by the response filler so no repeated
// strlen_P() scan of a ~180 kB flash region is needed per transmitted chunk.
static const size_t HTML_PAGE_A_LEN  = sizeof(HTML_PAGE_A)  - 1u;
static const size_t HTML_PAGE_B_LEN  = sizeof(HTML_PAGE_B)  - 1u;
static const size_t HTML_PAGE_C_LEN  = sizeof(HTML_PAGE_C)  - 1u;
static const size_t HTML_PAGE_D_LEN  = sizeof(HTML_PAGE_D)  - 1u;
static const size_t HTML_TEXT_EN_LEN = sizeof(HTML_TEXT_EN) - 1u;
static const size_t HTML_TEXT_FR_LEN = sizeof(HTML_TEXT_FR) - 1u;
static const size_t HTML_LANG_EN_LEN = sizeof(HTML_LANG_EN) - 1u;
static const size_t HTML_LANG_FR_LEN = sizeof(HTML_LANG_FR) - 1u;

#endif
