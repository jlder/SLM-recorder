/*******************************************************************************
 * SCRIPT: FILE LIST, DOWNLOAD, SAVE, AND RESULT DISPLAY
 * JavaScript for /api/list, binary download, local save, archive action, progress display, and flight-analysis table rendering.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        // Root recorder-file list. Refresh also updates SD, recording, and calibration badges.
        function refreshFiles() {
            fetch('/api/files')
                .then(r => r.json())
                .then(data => {
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
                        <button class="btn btn-primary file-btn" onclick='downloadFile(${jsName})'>Download</button>
                    </div>
                    <div class="file-row">
                        <div class="file-size">Size: ${sizeKb} kB</div>
                        <button class="btn btn-primary file-btn" onclick='deleteFile(${jsName})'>Archive</button>
                    </div>
                </div>`;
            });
            html += '</div>';
            container.innerHTML = html;
        }

        // Download one binary recording, save it locally, then analyze the same buffer in the browser.
        function downloadFile(filename) {
            const status = document.getElementById('flightAnalysisStatus');
            const panel = document.getElementById('flightAnalysisWindow');
            const table = document.getElementById('flightAnalysisTable');

            panel.classList.remove('hidden');
            table.innerHTML = '';
            const sampleStats = document.getElementById('samplePeriodStats');
            if (sampleStats) sampleStats.textContent = 'Sample period: -';
            status.textContent = 'Preparing download and analysis for ' + filename + '...';
            setProgress('downloadProgressBar', 'downloadProgressText', 0, 'Download: starting...');

            const downloadStartMs = performance.now();

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

                    status.textContent = 'Processing ' + filename + '...';

                    return analyzeRecorderFileAsync(buffer)
                        .then(analysis => {
                            showFlightAnalysis(filename, analysis);
                        });
                })
                .catch(err => {
                    status.textContent = 'Download or analysis failed. Falling back to direct download.';
                    console.error(err);
                    window.location.href = '/api/download?file=' + encodeURIComponent(filename);
                });
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
            const sampleStats = document.getElementById('samplePeriodStats');

            if (sampleStats) {
                sampleStats.textContent = samplePeriodStatsText(analysis && analysis.samplePeriod);
            }
            if (!analysis || !analysis.ok) {
                const reason = analysis && analysis.reason ? analysis.reason : 'Analysis failed.';
                status.textContent = reason;
                table.innerHTML = '';
                return;
            }

            if (analysis.flights.length === 0) {
                status.textContent = filename + ': no complete flight detected.';
                table.innerHTML = '';
                return;
            }

            status.textContent = filename + ': ' + analysis.flights.length + ' flight(s) detected.';

            let html = '<table class="analysis-table"><thead><tr>' +
                '<th>FLT<br>#</th><th>T/O<br>START</th><th>T/O<br>ROLL</th>' +
                '<th>LDG<br>ROLL</th><th>LDG<br>STOP</th><th>FLT<br>TIME</th>' +
                '</tr></thead><tbody>';

            analysis.flights.forEach(f => {
                html += '<tr>' +
                    '<td>' + f.number + '</td>' +
                    '<td class="mono">' + escapeHtml(f.takeoffRollStartShort || f.takeoffRollStart) + '</td>' +
                    '<td class="mono">' + escapeHtml(f.takeoffRollDuration || '-') + '</td>' +
                    '<td class="mono">' + escapeHtml(f.landingRollDuration || '-') + '</td>' +
                    '<td class="mono">' + escapeHtml(f.landingRollEndShort || f.landingRollEnd) + '</td>' +
                    '<td class="mono">' + escapeHtml(f.flightDurationShort || f.flightDuration) + '</td>' +
                    '</tr>';
            });

            html += '</tbody></table>';
            table.innerHTML = html;
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
