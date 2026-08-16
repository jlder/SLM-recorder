// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/tasks/html_interface.h
 * @brief Assembles the embedded HTML, CSS, and JavaScript for the recorder web interface.
 *
 * @details The recorder web page is served as one PROGMEM document.  The
 * implementation is split into source fragments under src/tasks/web_ui/ only to
 * improve reviewability.  The fragments are adjacent C++ raw string literals;
 * the compiler concatenates them into the same HTML_PAGE[] document.
 *
 * Byte-stability rule for this release cleanup:
 * - comments are placed outside the raw string fragments;
 * - the generated HTML/CSS/JavaScript payload is intentionally unchanged;
 * - edit inside web_ui/*.inc raw strings only when changing the web page itself.
 */

#ifndef HTML_INTERFACE_H
#define HTML_INTERFACE_H

#include "config.h"
#include "src/services/language.h"

#define SLM_HTML_STRINGIFY_IMPL(x) #x
#define SLM_HTML_STRINGIFY(x) SLM_HTML_STRINGIFY_IMPL(x)

// Build the browser translation catalog directly into the single embedded
// HTML document. language.h remains the only translation source, while the
// Web UI remains self-contained (no second HTTP resource is required).
// Each entry is [English, French]; the recorder-selected language chooses the
// element at display time.
#define SLM_WEB_ROW(id, en, fr) ",\"" #id "\":[\"" en "\",\"" fr "\"]"

const char HTML_PAGE[] PROGMEM =
#include "web_ui/00_page_begin.inc"
#include "web_ui/01_styles.inc"
R"rawliteral(
    <script>
        window.SLM_TEXT={"_":["",""]
)rawliteral"
LANGUAGE_TEXT_LIST(SLM_WEB_ROW)
R"rawliteral(
        };
        window.SLM_LANGUAGE='fr';
        window.tr=function(k){
            const pair=window.SLM_TEXT[k];
            if(!pair) return k;
            return pair[(window.SLM_LANGUAGE==='en') ? 0 : 1];
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
        window.setSlmLanguage=function(code){
            const next=(code==='en') ? 'en' : 'fr';
            if(window.SLM_LANGUAGE===next) return false;
            window.SLM_LANGUAGE=next;
            window.applyStaticTranslations();
            return true;
        };
    </script>
</head>
)rawliteral"
#include "web_ui/02_body_home_files.inc"
#include "web_ui/03_body_maintenance_menu.inc"
#include "web_ui/04_body_calibration_pages.inc"
#include "web_ui/05_body_health_ota.inc"
#include "web_ui/10_script_navigation_auth.inc"
#include "web_ui/11_script_files_download.inc"
#include "web_ui/12_script_flight_decode.inc"
#include "web_ui/13_script_signal_processing.inc"
#include "web_ui/14_script_format_helpers.inc"
#include "web_ui/15_script_archive_status_diag.inc"
#include "web_ui/16_script_calibration.inc"
#include "web_ui/17_script_ota_startup_end.inc"
;

#undef SLM_WEB_ROW

#endif
