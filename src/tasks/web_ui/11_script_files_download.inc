/*******************************************************************************
 * SCRIPT: FILE LIST, DOWNLOAD, SAVE, AND RESULT DISPLAY
 * JavaScript for /api/list, binary download, local save, archive action, progress display, and flight-analysis table rendering.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        // Retry short recorder/SD busy windows instead of showing a false empty/unavailable list.
        function recorderJsonWithRetry(url, attempt) {
            const tryNumber = attempt || 1;
            return fetch(url, { cache: 'no-store' })
                .then(r => r.json().catch(() => ({})).then(json => ({
                    ok: r.ok,
                    status: r.status,
                    json: json
                })))
                .then(result => {
                    const retryable = result.status === 409 || result.status === 500 || result.status === 503;
                    if (!result.ok && retryable && tryNumber < 6) {
                        return new Promise(resolve => setTimeout(resolve, 200 * tryNumber))
                            .then(() => recorderJsonWithRetry(url, tryNumber + 1));
                    }
                    return result;
                })
                .catch(err => {
                    if (tryNumber < 6) {
                        return new Promise(resolve => setTimeout(resolve, 200 * tryNumber))
                            .then(() => recorderJsonWithRetry(url, tryNumber + 1));
                    }
                    throw err;
                });
        }

        // Root recorder-file list. Refresh also updates SD, recording, and calibration badges.
        function refreshFiles() {
            recorderJsonWithRetry('/api/files')
                .then(result => {
                    if (!result.ok) throw new Error('Recorder file list returned HTTP ' + result.status);
                    const data = result.json || {};
                    allFiles = data.files || [];
                    displayFiles(allFiles);
                    updateSDInfo();
                    updateStatus();
                    calStatus();
                })
                .catch(err => {
                    document.getElementById('fileListContainer').innerHTML = '<div class="loading">File list unavailable</div>';
                    console.error(err);
                });
        }

        // Escape text before inserting file names or diagnostic strings into generated HTML.
        function escapeHtml(text) {
            return String(text)
                .replace(/&/g, '&amp;')
                .replace(/</g, '&lt;')
                .replace(/>/g, '&gt;')
                .replace(/"/g, '&quot;')
                .replace(/'/g, '&#39;');
        }

        // Build the visible root file list. File names are preserved for API calls and shortened only for display.
        function displayFiles(files) {
            const container = document.getElementById('fileListContainer');
            document.getElementById('fileCount').textContent = files.length;

            if (files.length === 0) {
                container.innerHTML = '<div class="loading">No files found</div>';
                return;
            }

            let html = '<div class="file-list">';
            files.forEach(file => {
                const name = String(file.name || '');
                const displayName = name.replace(/\.bin$/i, '');
                const safeName = escapeHtml(displayName);
                const jsName = JSON.stringify(name);
                const sizeKb = Math.round((file.size || 0) / 1000);

                html += `<div class="file-item">
                    <div class="file-row">
                        <div class="file-name">${safeName}</div>
                        <button class="btn btn-primary file-btn download-file-btn" onclick='downloadFile(${jsName})'>Download</button>
                    </div>
                    <div class="file-row">
                        <div class="file-size">Size: ${sizeKb} kB</div>
                    </div>
                    <div class="file-row manual-archive-row">
                        <div class="file-size">Stored analysis, if available</div>
                        <button class="btn btn-primary file-btn" onclick='deleteFile(${jsName})'>Archive</button>
                    </div>
                </div>`;
            });
            html += '</div>';
            container.innerHTML = html;
            if (androidAutomaticArchiveAvailable()) {
                document.querySelectorAll('.manual-archive-row').forEach(row => row.remove());
            }
        }

        function androidAutomaticArchiveAvailable() {
            if (!window.SLMAndroid || typeof window.SLMAndroid.getCapabilities !== 'function') {
                return false;
            }
            try {
                const capabilities = JSON.parse(window.SLMAndroid.getCapabilities());
                return capabilities && Array.isArray(capabilities.features) &&
                    capabilities.features.indexOf('automatic-recorder-archive') >= 0;
            } catch (err) {
                return false;
            }
        }

        // Download one binary recording, save it locally, then analyze the same buffer in the browser.
        function downloadFile(filename) {
            const status = document.getElementById('flightAnalysisStatus');
            const panel = document.getElementById('flightAnalysisWindow');
            const table = document.getElementById('flightAnalysisTable');

            panel.classList.remove('hidden');
            table.innerHTML = '';
            setRecorderDownloadAvailable(false);
            status.textContent = filename + ' \u2014 Downloading';
            setProgress('downloadProgressBar', 'downloadProgressText', 0, 'Download: starting...');

            // In the Android shell, download once into app-private storage. The
            // same private bytes are analyzed here and queued for Drive without
            // displaying Android's Save As dialog to the pilot.
            if (startAndroidRecorderTransfer(filename)) {
                return;
            }

            downloadFileBuffer(filename, function(percent, text) {
                setProgress('downloadProgressBar', 'downloadProgressText', percent, text);
            })
                .then(buffer => {
                    setProgress(
                        'downloadProgressBar',
                        'downloadProgressText',
                        100,
                        'Download: complete'
                    );

                    // Save the file as soon as the binary download is complete.
                    // Analysis is then performed from the same buffer.
                    saveDownloadedBuffer(filename, buffer);

                    status.textContent = filename + ' \u2014 Analyzing';

                    return analyzeRecorderFileAsync(buffer)
                        .then(analysis => {
                            showFlightAnalysis(filename, analysis);
                            writeFlightAnalysisLog(filename, analysis);
                        })
                        .finally(() => setRecorderDownloadAvailable(true));
                })
                .catch(err => {
                    status.textContent = filename + ' \u2014 Download/analysis failed';
                    setRecorderDownloadAvailable(true);
                    console.error(err);
                    window.location.href = '/api/download?file=' + encodeURIComponent(filename);
                });
        }

        function setRecorderDownloadAvailable(available) {
            document.querySelectorAll('.download-file-btn').forEach(button => {
                button.disabled = !available;
            });
        }

        function startAndroidRecorderTransfer(filename) {
            if (!window.SLMAndroid ||
                typeof window.SLMAndroid.downloadRecorderFile !== 'function' ||
                typeof window.SLMAndroid.getCapabilities !== 'function') {
                return false;
            }

            try {
                const capabilities = JSON.parse(window.SLMAndroid.getCapabilities());
                if (!capabilities || Number(capabilities.bridgeVersion) < 2) {
                    return false;
                }
                window.SLMAndroid.downloadRecorderFile(JSON.stringify({
                    filename: filename,
                    analyze: true,
                    upload: true
                }));
                return true;
            } catch (err) {
                console.warn('SLM Android bridge is unavailable', err);
                return false;
            }
        }

        window.addEventListener('slm-transfer-event', function(event) {
            const detail = event && event.detail ? event.detail : {};
            const state = String(detail.state || '');
            const filename = String(detail.filename || 'recording');
            const percent = Math.max(0, Math.min(100, Number(detail.percent) || 0));
            const status = document.getElementById('flightAnalysisStatus');

            if (state === 'download-started') {
                setRecorderDownloadAvailable(false);
                status.textContent = filename + ' \u2014 Downloading';
                setProgress('downloadProgressBar', 'downloadProgressText', 0, 'Download: starting...');
            } else if (state === 'downloading') {
                status.textContent = filename + ' \u2014 Downloading ' + Math.round(percent) + '%';
                setProgress('downloadProgressBar', 'downloadProgressText', percent,
                    'Download: ' + Math.round(percent) + '%');
            } else if (state === 'download-complete') {
                setProgress('downloadProgressBar', 'downloadProgressText', 100, 'Download: complete');
                status.textContent = filename + ' \u2014 Analyzing';
                fetch(detail.localUrl, { cache: 'no-store' })
                    .then(function(response) {
                        if (!response.ok) throw new Error('private file unavailable');
                        return response.arrayBuffer();
                    })
                    .then(function(buffer) { return analyzeRecorderFileAsync(buffer); })
                    .then(function(analysis) {
                        showFlightAnalysis(filename, analysis);
                        return writeFlightAnalysisLog(filename, analysis);
                    })
                    .catch(function(err) {
                        console.error(err);
                        status.textContent = filename + ' \u2014 Analysis failed';
                    })
                    .finally(function() {
                        notifyAndroidAnalysisComplete(detail.transferId);
                        setRecorderDownloadAvailable(true);
                    });
            } else if (state === 'archive-complete') {
                refreshFiles();
            } else if (state === 'failed') {
                status.textContent = filename + ' \u2014 Transfer failed: ' +
                    (detail.error || 'unknown error');
                setRecorderDownloadAvailable(true);
            }
        });

        function notifyAndroidAnalysisComplete(transferId) {
            if (!transferId || !window.SLMAndroid ||
                typeof window.SLMAndroid.analysisComplete !== 'function') return;
            try {
                window.SLMAndroid.analysisComplete(String(transferId));
            } catch (err) {
                console.warn('automatic recorder archive could not be scheduled', err);
            }
        }

        // XMLHttpRequest is used here because fetch does not provide simple progress callbacks.
        function downloadFileBuffer(filename, progressCb) {
            return new Promise(function(resolve, reject) {
                const xhr = new XMLHttpRequest();
                xhr.open('GET', '/api/download?file=' + encodeURIComponent(filename), true);
                xhr.responseType = 'arraybuffer';

                xhr.onprogress = function(evt) {
                    if (evt.lengthComputable && evt.total > 0) {
                        const percent = Math.round((evt.loaded * 100) / evt.total);
                        progressCb(
                            percent,
                            'Download: ' + percent + '% (' +
                                formatBytes(evt.loaded) + ' / ' + formatBytes(evt.total) + ')'
                        );
                    } else {
                        progressCb(0, 'Download: ' + formatBytes(evt.loaded) + ' received');
                    }
                };

                xhr.onload = function() {
                    if (xhr.status >= 200 && xhr.status < 300) {
                        resolve(xhr.response);
                    } else {
                        reject(new Error('download failed: HTTP ' + xhr.status));
                    }
                };

                xhr.onerror = function() {
                    reject(new Error('download failed'));
                };

                xhr.send();
            });
        }

        // Shared progress-bar helper for download and analysis progress.
        function setProgress(barId, textId, percent, text) {
            const bar = document.getElementById(barId);
            const label = document.getElementById(textId);
            const p = Math.max(0, Math.min(100, Math.round(Number(percent) || 0)));

            if (bar) bar.style.width = p + '%';
            if (label) label.textContent = text || (p + '%');
        }

        function formatBytes(bytes) {
            const b = Number(bytes) || 0;
            if (b >= 1000000) return (b / 1000000).toFixed(1) + ' MB';
            if (b >= 1000) return (b / 1000).toFixed(0) + ' kB';
            return b + ' B';
        }

        function logbookFileParts(filename) {
            const match = String(filename || '').match(/_(\d{4})(\d{2})(\d{2})_(\d+)\.log$/i);
            if (!match) return null;
            return {
                dateKey: match[1] + match[2] + match[3],
                date: match[1] + '-' + match[2] + '-' + match[3],
                session: Number(match[4]) || 0
            };
        }

        function compareLogbookFilesNewestFirst(left, right) {
            const a = logbookFileParts(left.name);
            const b = logbookFileParts(right.name);
            if (a && b) {
                if (a.dateKey !== b.dateKey) return b.dateKey.localeCompare(a.dateKey);
                return b.session - a.session;
            }
            return String(right.name || '').localeCompare(String(left.name || ''));
        }

        function recorderTextWithRetry(url, attempt) {
            const tryNumber = attempt || 1;
            return fetch(url, { cache: 'no-store' }).then(response => {
                if (response.ok) return response.text();
                if ((response.status === 409 || response.status === 500) && tryNumber < 6) {
                    return delayMs(200 * tryNumber)
                        .then(() => recorderTextWithRetry(url, tryNumber + 1));
                }
                throw new Error('Recorder log returned HTTP ' + response.status);
            });
        }

        function parseLogbookRows(filename, text) {
            const parts = logbookFileParts(filename);
            const date = parts ? parts.date : '-';
            const rows = [];
            String(text || '').split(/\r?\n/).forEach(line => {
                const fields = line.trim().split(/\s+/);
                if (fields.length < 4 || !/^\d+$/.test(fields[0])) return;
                rows.push({
                    date: date,
                    takeoff: fields[1],
                    landing: fields[2],
                    flightTime: fields.slice(3).join(' ')
                });
            });
            return rows;
        }

        function refreshLogbook() {
            const container = document.getElementById('logbookContainer');
            container.innerHTML = '<div class="loading">Loading logbook...</div>';
            recorderJsonWithRetry('/api/logbook/files')
                .then(result => {
                    if (!result.ok || !result.json || !result.json.ok) {
                        throw new Error('Recorder logbook list unavailable');
                    }
                    const latest = (result.json.files || [])
                        .slice()
                        .sort(compareLogbookFilesNewestFirst)
                        .slice(0, 5);
                    if (latest.length === 0) {
                        displayLogbookRows([]);
                        return;
                    }

                    const rows = [];
                    let chain = Promise.resolve();
                    latest.forEach(file => {
                        chain = chain.then(() =>
                            recorderTextWithRetry('/api/download?file=' +
                                encodeURIComponent('/processed/' + String(file.name || '')), 1)
                                .then(text => {
                                    rows.push.apply(rows, parseLogbookRows(file.name, text));
                                })
                                .catch(err => console.warn(err))
                        );
                    });
                    return chain.then(() => displayLogbookRows(rows));
                })
                .catch(err => {
                    console.error(err);
                    container.innerHTML = '<div class="loading">Logbook unavailable</div>';
                });
        }

        function displayLogbookRows(rows) {
            const container = document.getElementById('logbookContainer');
            if (!rows || rows.length === 0) {
                container.innerHTML = '<div class="loading">No downloaded flight logs found</div>';
                return;
            }
            let html = '<table class="analysis-table"><thead><tr>' +
                '<th>Date</th><th>T/O</th><th>LDG</th><th>Flight<br>Time</th>' +
                '</tr></thead><tbody>';
            rows.forEach(row => {
                html += '<tr>' +
                    '<td class="mono">' + escapeHtml(row.date) + '</td>' +
                    '<td class="mono">' + escapeHtml(row.takeoff) + '</td>' +
                    '<td class="mono">' + escapeHtml(row.landing) + '</td>' +
                    '<td class="mono">' + escapeHtml(row.flightTime) + '</td>' +
                    '</tr>';
            });
            container.innerHTML = html + '</tbody></table>';
        }


        // Yield to the browser so long analysis steps do not freeze the page completely.
        function analysisYield() {
            return new Promise(resolve => setTimeout(resolve, 0));
        }

        // Local browser save helpers. The recorder is not involved after the buffer has arrived.
        function saveDownloadedBuffer(filename, buffer) {
            const blob = new Blob([buffer], { type: 'application/octet-stream' });
            saveBlobFile(filename, blob);
        }

        function saveBlobFile(filename, blob) {
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            a.remove();
            setTimeout(() => URL.revokeObjectURL(url), 30000);
        }

        // Render the compact flight-time table from the normalized analysis result.
        function showFlightAnalysis(filename, analysis) {
            const status = document.getElementById('flightAnalysisStatus');
            const table = document.getElementById('flightAnalysisTable');
            if (!analysis || !analysis.ok) {
                const reason = analysis && analysis.reason ? analysis.reason : 'Analysis failed.';
                status.textContent = filename + ' \u2014 Complete: ' + reason;
                table.innerHTML = '';
                return;
            }

            if (analysis.flights.length === 0) {
                status.textContent = filename + ' \u2014 Complete: no flight detected';
                table.innerHTML = '';
                return;
            }

            status.textContent = filename + ' \u2014 Complete: ' + analysis.flights.length + ' flight(s)';

            let html = '<table class="analysis-table"><thead><tr>' +
                '<th>FLT<br>#</th><th>T/O</th><th>LDG</th><th>FLT<br>TIME</th>' +
                '</tr></thead><tbody>';

            analysis.flights.forEach(f => {
                html += '<tr>' +
                    '<td>' + f.number + '</td>' +
                    '<td class="mono">' + escapeHtml(f.takeoffRollStartShort || f.takeoffRollStart) + '</td>' +
                    '<td class="mono">' + escapeHtml(f.landingRollEndShort || f.landingRollEnd) + '</td>' +
                    '<td class="mono">' + escapeHtml(f.flightDurationShort || f.flightDuration) + '</td>' +
                    '</tr>';
            });

            html += '</tbody></table>';
            table.innerHTML = html;
        }

        // Build the recorder-side text log associated with the downloaded binary.
        function buildFlightAnalysisLogText(filename, analysis) {
            const widths = [5, 7, 7, 11];
            const row = function(a, b, c, d) {
                return String(a).padEnd(widths[0], ' ') + '  ' +
                       String(b).padEnd(widths[1], ' ') + '  ' +
                       String(c).padEnd(widths[2], ' ') + '  ' +
                       String(d).padEnd(widths[3], ' ');
            };

            const lines = [];
            lines.push(row('flt #', 'Takeoff', 'Landing', 'Flight Time'));

            if (analysis && analysis.ok && Array.isArray(analysis.flights)) {
                analysis.flights.forEach(f => {
                    lines.push(row(
                        f.number || '-',
                        f.takeoffRollStartShort || f.takeoffRollStart || '-',
                        f.landingRollEndShort || f.landingRollEnd || '-',
                        f.flightDurationShort || f.flightDuration || '-'
                    ));
                });
            }

            return lines.join('\n') + '\n';
        }

        // Store the same flight-analysis summary on the recorder SD card.
        // The recorder derives the .log file name from the original .bin name.
        function writeFlightAnalysisLog(filename, analysis) {
            const text = buildFlightAnalysisLogText(filename, analysis);
            const body = new URLSearchParams();
            body.set('file', filename);
            body.set('log', text);

            return fetch('/api/flightlog', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: body.toString()
            })
                .then(r => r.json().then(data => ({ ok: r.ok, data: data })))
                .then(result => {
                    if (!result.ok || !result.data || !result.data.ok) return;
                })
                .catch(err => {
                    console.warn('flight log not stored', err);
                });
        }


        // ---------------------------------------------------------------------
        // Browser-side flight analysis
        //
        // The browser accepts both recorder binary formats:
        //   - SLM 0x70 records:
        //       0x55, 0x70, uint32 time_ms, int16 ax_mg, int16 ay_mg,
        //       int16 az_mg, uint8 checksum
        //   - WIT 0x50/0x51 packets:
        //       0x50 time anchors and 0x51 acceleration samples.
        //
        // Both decoders produce the same internal acceleration timeline:
        //   { absMs, axG, ayG, azG }
        //
        // The flight analysis is common to both formats: HIRMS (>3 Hz),
        // LOWRMS (0.5..3 Hz), FlightGround = normalized LOWRMS - normalized HIRMS,
        // hysteresis flight/ground state, then roll-phase timing from
        // normalized HIRMS thresholds.
        // ---------------------------------------------------------------------

)rawliteral"
