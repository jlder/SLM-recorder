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
            container.innerHTML = '<div class="loading">Loading reports...</div>';

            recorderJsonWithRetry('/api/reports/files')
                .then(res => {
                    if (!res.ok || !res.json || !res.json.ok) {
                        reportFiles = [];
                        container.innerHTML = '<div class="loading">Report list unavailable</div>';
                        return;
                    }

                    reportFiles = res.json.files || [];
                    displayReports(reportFiles);
                })
                .catch(err => {
                    reportFiles = [];
                    container.innerHTML = '<div class="loading">Report list unavailable</div>';
                    console.error(err);
                });
        }

        function displayReports(files) {
            const container = document.getElementById('reportListContainer');
            if (!container) return;
            if (!files || files.length === 0) {
                container.innerHTML = '<div class="loading">No calibration reports found</div>';
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
                        <button class="btn btn-primary file-btn" onclick='downloadReport(${jsName})'>Download</button>
                    </div>
                    <div class="file-size">Size: ${sizeKb} kB</div>
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
                    document.getElementById('sdSize').textContent = data.present ? (data.size_mb + ' MB') : 'Not present';
                    document.getElementById('sdFree').textContent = data.present ? (data.free_mb + ' MB') : '-';
                });
        }

        function updateStatus() {
            fetch('/api/status')
                .then(r => r.json())
                .then(data => {
                    const badge = document.getElementById('statusBadge');
                    if (data.recording) {
                        badge.innerHTML = '<span class="status-recording">● Recording</span>';
                    } else {
                        badge.innerHTML = '';
                    }
                });
        }

        // Apply the amber required-action cue without overriding disabled or Cancel behavior.
        function setButtonWarning(id, warning) {
            const btn = document.getElementById(id);
            if (!btn) return;

            // Start buttons become Cancel while a calibration is running.
            // Cancel is not the corrective action for a missing calibration,
            // so the active/Cancel state must override the amber warning cue.
            const isCancel = (btn.dataset && btn.dataset.active === '1') ||
                             (btn.textContent && btn.textContent.trim() === 'Cancel');
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
            return v ? 'yes' : 'no';
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
                        text.innerHTML = 'Last watchdog fault: none';
                        return;
                    }

                    const status = data.active ? 'active / not acknowledged' : 'acknowledged';
                    text.innerHTML =
                        'Status: ' + escapeHtml(status) + '<br>' +
                        'Source: ' + escapeHtml(data.source || 'unknown') + '<br>' +
                        'Failed age: ' + escapeHtml(fmtAge(data.age_ms)) + '<br>' +
                        'Ages [state, sd, record, web]: ' +
                        escapeHtml(fmtAge(data.age_state_ms)) + ', ' +
                        escapeHtml(fmtAge(data.age_sd_ms)) + ', ' +
                        escapeHtml(fmtAge(data.age_record_ms)) + ', ' +
                        escapeHtml(fmtAge(data.age_web_ms)) + '<br>' +
                        'Recorder state: ' + escapeHtml(data.recorder_state) +
                        ', last error: ' + escapeHtml(data.last_error) + '<br>' +
                        'Web: ' + escapeHtml(fmtBool(data.web_active)) +
                        ', USB: ' + escapeHtml(fmtBool(data.usb_present)) +
                        ', SD: ' + escapeHtml(fmtBool(data.sd_present)) + '<br>' +
                        'Heap: ' + escapeHtml(data.heap) +
                        ', min heap: ' + escapeHtml(data.min_heap);
                })
                .catch(err => console.error(err));
        }

)rawliteral"
