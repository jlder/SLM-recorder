/*******************************************************************************
 * SCRIPT: REPORTS, STATUS, AND DIAGNOSTICS
 * JavaScript for root archive, calibration reports, SD/status updates, and watchdog diagnostic display.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        function delayMs(ms) {
            return new Promise(resolve => setTimeout(resolve, ms));
        }

        // Calibration reports are listed separately from root recordings.
        function refreshReports() {
            const container = document.getElementById('reportListContainer');
            if (!container) return;
            container.innerHTML = '<div class="loading">' + escapeHtml(tr('LOADING_REPORTS')) + '</div>';

            recorderJsonWithRetry('/api/reports/files')
                .then(res => {
                    if (!res.ok || !res.json || !res.json.ok) {
                        reportFiles = [];
                        container.innerHTML = '<div class="loading">' + escapeHtml(tr('REPORT_LIST_UNAVAILABLE')) + '</div>';
                        return;
                    }

                    reportFiles = res.json.files || [];
                    displayReports(reportFiles);
                })
                .catch(err => {
                    reportFiles = [];
                    container.innerHTML = '<div class="loading">' + escapeHtml(tr('REPORT_LIST_UNAVAILABLE')) + '</div>';
                    console.error(err);
                });
        }

        function displayReports(files) {
            const container = document.getElementById('reportListContainer');
            if (!container) return;
            if (!files || files.length === 0) {
                container.innerHTML = '<div class="loading">' + escapeHtml(tr('NO_CAL_REPORTS')) + '</div>';
                return;
            }

            let html = '<div class="file-list">';
            files.forEach(file => {
                const name = String(file.name || '');
                const safeName = escapeHtml(name);
                const jsName = JSON.stringify(name);
                const sizeKb = Math.round((file.size || 0) / 1000);

                html += `<div class="file-item">
                    <div class="file-row">
                        <div class="file-name">${safeName}</div>
                        <button class="btn btn-primary file-btn" onclick='downloadReport(${jsName})'>${escapeHtml(tr('DOWNLOAD'))}</button>
                    </div>
                    <div class="file-size">${escapeHtml(tr('SIZE_LABEL'))} ${sizeKb} kB</div>
                </div>`;
            });
            html += '</div>';
            container.innerHTML = html;
        }

        function downloadReport(filename) {
            window.location = '/api/download?file=' + encodeURIComponent('/calibration_reports/' + filename);
        }

        // Lightweight SD-capacity refresh used by file page updates.
        function updateSDInfo() {
            fetch('/api/info')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('sdSize').textContent = data.present ? (data.size_mb + ' MB') : tr('NOT_PRESENT');
                    document.getElementById('sdFree').textContent = data.present ? (data.free_mb + ' MB') : '-';
                });
        }

        function updateStatus() {
            fetch('/api/status')
                .then(r => r.json())
                .then(data => {
                    if (data && (data.language === 'en' || data.language === 'fr')) {
                        setSlmLanguage(data.language);
                    }
                    const badge = document.getElementById('statusBadge');
                    if (data.recording) {
                        badge.innerHTML = '<span class="status-recording">● ' + escapeHtml(tr('RECORDING_BADGE')) + '</span>';
                    } else {
                        badge.innerHTML = '';
                    }
                    updateUsbRequiredButtons(data);
                });
        }

        // Recorder calibration is the only Web workflow intentionally allowed
        // while the recorder is battery powered. Keep the navigation visible
        // but grey the USB-powered functions so the requirement is obvious.
        function updateUsbRequiredButtons(data) {
            const usbAvailable = !!(data && data.usb_present_valid && data.usb_present);
            ['btnHomeFiles', 'btnHomeLogbook', 'btnMenuInstallCal',
             'btnMenuReport', 'btnMenuFirmware', 'btnMenuAbout'].forEach(id => {
                const button = document.getElementById(id);
                if (button) button.disabled = !usbAvailable;
            });

            const message = document.getElementById('usbPowerRequiredMessage');
            if (message) message.classList.toggle('hidden', usbAvailable);
        }

        // Apply the amber required-action cue without overriding disabled or Cancel behavior.
        function setButtonWarning(id, warning) {
            const btn = document.getElementById(id);
            if (!btn) return;

            // Start buttons become Cancel while a calibration is running.
            // Cancel is not the corrective action for a missing calibration,
            // so the active/Cancel state must override the amber warning cue.
            const isCancel = (btn.dataset && btn.dataset.active === '1');
            const showWarning = !!warning && !isCancel;

            btn.classList.toggle('btn-warning', showWarning);
            btn.classList.toggle('btn-primary', !showWarning);
        }

        function updateMaintenanceButtonState(data) {
            // Match the device UI convention: a button leading to a required
            // configuration/calibration action is amber instead of blue.
            const sensorRequired = !(data && data.sensor_valid);
            const installRequired = !(data && data.installation_valid);
            const calRequired = sensorRequired || installRequired;
            setButtonWarning('btnHomeMaintenance', calRequired);
            setButtonWarning('btnMaintenanceUnlock', calRequired);
            setButtonWarning('btnMenuRecorderCal', sensorRequired);
            setButtonWarning('btnMenuInstallCal', installRequired);

            // Apply the same rule inside the calibration pages.  Disabled
            // buttons still appear grey because .btn:disabled takes precedence,
            // but when Start/Save becomes enabled it keeps the amber action cue.
            setButtonWarning('btnStart', sensorRequired);
            setButtonWarning('btnSave', sensorRequired);
            setButtonWarning('btnInstallStart', installRequired);
            setButtonWarning('btnInstallSave', installRequired);
        }

        function fmtBool(v) {
            return v ? tr('YES') : tr('NO');
        }

        // Render the last watchdog supervision fault, if one has been captured.
        function updateWatchdogDiag() {
            fetch('/api/watchdog')
                .then(r => r.json())
                .then(data => {
                    const panel = document.getElementById('watchdogDiagPanel');
                    const text = document.getElementById('watchdogDiagText');
                    if (!panel || !text) return;
                    panel.classList.remove('hidden');
                    if (!data || !data.available) {
                        text.innerHTML = escapeHtml(tr('LAST_WDG_NONE'));
                        return;
                    }

                    const status = data.active ? tr('WDG_ACTIVE') : tr('WDG_ACK');
                    text.innerHTML =
                        tr('STATUS_LABEL') + ' ' + escapeHtml(status) + '<br>' +
                        tr('SOURCE_PREFIX') + ' ' + escapeHtml(data.source || tr('UNKNOWN')) + '<br>' +
                        tr('FAILED_AGE') + ' ' + escapeHtml(fmtAge(data.age_ms)) + '<br>' +
                        tr('AGES_WDG') + ' ' +
                        escapeHtml(fmtAge(data.age_state_ms)) + ', ' +
                        escapeHtml(fmtAge(data.age_sd_ms)) + ', ' +
                        escapeHtml(fmtAge(data.age_record_ms)) + ', ' +
                        escapeHtml(fmtAge(data.age_web_ms)) + '<br>' +
                        tr('RECORDER_STATE_LABEL') + ' ' + escapeHtml(data.recorder_state) +
                        ', ' + tr('LAST_ERROR_LABEL') + ' ' + escapeHtml(data.last_error) + '<br>' +
                        tr('WEB_LABEL') + ' ' + escapeHtml(fmtBool(data.web_active)) +
                        ', ' + tr('USB_LABEL') + ' ' + escapeHtml(fmtBool(data.usb_present)) +
                        ', ' + tr('SD_LABEL') + ' ' + escapeHtml(fmtBool(data.sd_present)) + '<br>' +
                        tr('HEAP_LABEL') + ' ' + escapeHtml(data.heap) +
                        ', ' + tr('MIN_HEAP_LABEL') + ' ' + escapeHtml(data.min_heap);
                })
                .catch(err => console.error(err));
        }

)rawliteral"
