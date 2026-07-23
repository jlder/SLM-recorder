/*******************************************************************************
 * SCRIPT: ARCHIVE, PROCESSED DELETE, STATUS, AND DIAGNOSTICS
 * JavaScript for root archive, /processed permanent delete, SD/status updates, and watchdog diagnostic display.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        // Archive a root recording into /processed. Permanent deletion is handled on the processed page.
        function deleteFile(filename) {
            if (!confirm('Archive ' + filename + '?')) return;
            fetch('/api/delete?file=' + encodeURIComponent(filename), { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    console.log('Delete response:', data);
                    refreshFiles();
                })
                .catch(err => console.error('Delete error:', err));
        }

        // Load files/reports already in /processed for permanent deletion.
        function refreshProcessedFiles() {
            const status = document.getElementById('processedDeleteStatus');
            const container = document.getElementById('processedFileListContainer');
            status.textContent = 'Loading...';
            container.innerHTML = '<div class="loading">Loading files...</div>';

            recorderJsonWithRetry('/api/processed/files')
                .then(res => {
                    if (!res.ok || !res.json || !res.json.ok) {
                        processedFiles = [];
                        status.textContent = 'File list unavailable.';
                        container.innerHTML = '<div class="loading">File list unavailable</div>';
                        return;
                    }

                    processedFiles = res.json.files || [];
                    status.textContent = processedFiles.length + ' file(s)/report(s) in /processed.';
                    displayProcessedFiles(processedFiles);
                })
                .catch(err => {
                    processedFiles = [];
                    status.textContent = 'File list unavailable.';
                    container.innerHTML = '<div class="loading">File list unavailable</div>';
                    console.error(err);
                });
        }

        function displayProcessedFiles(files) {
            const container = document.getElementById('processedFileListContainer');
            if (!files || files.length === 0) {
                container.innerHTML = '<div class="loading">No files in /processed</div>';
                return;
            }

            let html = '<div class="file-list">';
            files.forEach(file => {
                const name = String(file.name || '');
                const safeName = escapeHtml(name);
                const sizeKb = Math.round((file.size || 0) / 1000);
                html += `<label class="file-item delete-file-row">
                    <input type="checkbox" class="processed-delete-check" value="${escapeHtml(name)}">
                    <span class="file-name">${safeName}</span>
                    <span class="file-size">${sizeKb} kB</span>
                </label>`;
            });
            html += '</div>';
            container.innerHTML = html;
        }

        // Read selected processed-file checkboxes built by displayProcessedFiles().
        function selectedProcessedFiles() {
            return Array.from(document.querySelectorAll('.processed-delete-check:checked'))
                .map(cb => cb.value)
                .filter(name => name.length > 0);
        }

        function delayMs(ms) {
            return new Promise(resolve => setTimeout(resolve, ms));
        }

        // Delete one /processed item with bounded retry for transient SD-operation conflicts.
        function deleteProcessedOne(name, attempt) {
            const tryNumber = attempt || 1;
            return fetch('/api/processed/delete?file=' + encodeURIComponent(name), {
                    method: 'POST',
                    cache: 'no-store'
                })
                .then(r => r.json().catch(() => ({})).then(j => ({
                    ok: r.ok,
                    status: r.status,
                    json: j,
                    name: name
                })))
                .then(res => {
                    // The recorder accepts only one SD-backed web operation at a
                    // time.  During bulk deletion, requests can briefly overlap
                    // the server-side BUSY release.  Retry transient BUSY/failed
                    // responses before declaring the batch failed.
                    const retryable = (res.status === 409) || (res.status === 500) ||
                                      (res.json && res.json.reason === 'delete_failed');
                    if ((!res.ok || !(res.json && res.json.ok)) && retryable && tryNumber < 5) {
                        return delayMs(150 * tryNumber).then(() => deleteProcessedOne(name, tryNumber + 1));
                    }
                    return res;
                })
                .catch(err => {
                    if (tryNumber < 5) {
                        return delayMs(150 * tryNumber).then(() => deleteProcessedOne(name, tryNumber + 1));
                    }
                    throw err;
                });
        }

        function deleteProcessedSelected() {
            const selected = selectedProcessedFiles();
            const status = document.getElementById('processedDeleteStatus');
            const button = document.getElementById('btnProcessedDelete');
            if (selected.length === 0) {
                status.textContent = 'No files/reports selected.';
                return;
            }

            if (!confirm('All selected files will be lost. Delete ' + selected.length + ' file(s)?')) {
                return;
            }

            if (button) button.disabled = true;
            let deleted = 0;
            let failed = 0;
            let index = 0;

            function updateDeleteStatus(currentName) {
                status.textContent = 'Deleting ' + index + '/' + selected.length +
                    '... Deleted: ' + deleted + ', failed: ' + failed +
                    (currentName ? ' (' + currentName + ')' : '');
            }

            let chain = Promise.resolve();
            selected.forEach(name => {
                chain = chain
                    .then(() => {
                        index++;
                        updateDeleteStatus(name);
                        return deleteProcessedOne(name, 1);
                    })
                    .then(res => {
                        if (res.ok && res.json && res.json.ok) deleted++;
                        else failed++;
                        updateDeleteStatus('');
                        // Pace consecutive delete requests so the server-side SD
                        // operation gate can return to idle.
                        return delayMs(75);
                    })
                    .catch(err => {
                        failed++;
                        console.error(err);
                        updateDeleteStatus('');
                        return delayMs(75);
                    });
            });

            chain.then(() => {
                status.textContent = 'Deleted: ' + deleted + ', failed: ' + failed + '.';
                if (button) button.disabled = false;
                refreshProcessedFiles();
            });
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
                    <div class="file-row">
                        <div class="file-size">Size: ${sizeKb} kB</div>
                        <button class="btn btn-primary file-btn" onclick='archiveReport(${jsName})'>Archive</button>
                    </div>
                </div>`;
            });
            html += '</div>';
            container.innerHTML = html;
        }

        function downloadReport(filename) {
            window.location = '/api/download?file=' + encodeURIComponent('/calibration_reports/' + filename);
        }

        function archiveReport(filename) {
            if (!confirm('Archive report ' + filename + '?')) return;
            fetch('/api/reports/archive?file=' + encodeURIComponent(filename), { method: 'POST' })
                .then(r => r.json().then(j => ({ok:r.ok, json:j})))
                .then(res => {
                    refreshReports();
                    if (!res.ok || !res.json || !res.json.ok) {
                        alert('Report archive failed.');
                    }
                })
                .catch(err => {
                    console.error('Report archive error:', err);
                    alert('Report archive error.');
                });
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
