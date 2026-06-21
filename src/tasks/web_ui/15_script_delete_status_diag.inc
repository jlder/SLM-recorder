/*******************************************************************************
 * SCRIPT: ARCHIVE, PROCESSED DELETE, STATUS, AND DIAGNOSTICS
 * JavaScript for root archive, /processed permanent delete, SD/status updates, and watchdog diagnostic display.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        function deleteFile(filename) {
            if (!confirm('Archive ' + filename + '?')) return;
            fetch('/api/delete?file=' + encodeURIComponent(filename), { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    console.log('Delete response:', data);
                    refreshFiles();
                })
                .catch(err => console.error('Delete error:', err));
        }

        function refreshProcessedFiles() {
            const status = document.getElementById('processedDeleteStatus');
            const container = document.getElementById('processedFileListContainer');
            status.textContent = 'Loading...';
            container.innerHTML = '<div class="loading">Loading files...</div>';

            fetch('/api/processed/files')
                .then(r => r.json().then(j => ({ok:r.ok, json:j})))
                .then(res => {
                    if (!res.ok || !res.json || !res.json.ok) {
                        processedFiles = [];
                        status.textContent = 'File list unavailable.';
                        container.innerHTML = '<div class="loading">File list unavailable</div>';
                        return;
                    }

                    processedFiles = res.json.files || [];
                    status.textContent = processedFiles.length + ' file(s) in /processed.';
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
            files.forEach((file, index) => {
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

        function selectedProcessedFiles() {
            return Array.from(document.querySelectorAll('.processed-delete-check:checked'))
                .map(cb => cb.value)
                .filter(name => name.length > 0);
        }

        function delayMs(ms) {
            return new Promise(resolve => setTimeout(resolve, ms));
        }

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
                    // time.  During bulk deletion, the next HTTP request can
                    // occasionally reach the ESP32 before the previous request has
                    // fully released its BUSY state.  Retry transient BUSY/failed
                    // responses instead of declaring the whole batch failed.
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
                status.textContent = 'No files selected.';
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
                        // Give AsyncWebServer time to close/release the previous
                        // response before the next delete request is sent.
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
                    document.getElementById('battery').textContent = data.battery + '%';
                    const badge = document.getElementById('statusBadge');
                    if (data.recording) {
                        badge.innerHTML = '<span class="status-recording">● Recording</span>';
                    } else {
                        badge.innerHTML = '';
                    }
                });
        }

        function setButtonWarning(id, warning) {
            const btn = document.getElementById(id);
            if (!btn) return;
            btn.classList.toggle('btn-warning', !!warning);
            btn.classList.toggle('btn-primary', !warning);
        }

        function updateMaintenanceButtonState(data) {
            // Match the device UI convention: a button leading to a required
            // configuration/calibration action is amber instead of blue.
            const calRequired = !(data && data.sensor_valid && data.installation_valid);
            setButtonWarning('btnHomeMaintenance', calRequired);
            setButtonWarning('btnMaintenanceUnlock', calRequired);
            setButtonWarning('btnMenuRecorderCal', !(data && data.sensor_valid));
            setButtonWarning('btnMenuInstallCal', !(data && data.installation_valid));
        }

        function fmtBool(v) {
            return v ? 'yes' : 'no';
        }

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
