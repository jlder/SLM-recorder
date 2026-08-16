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
                    restoreRecorderProcessStates();
                    displayFiles(allFiles);
                    updateSDInfo();
                    updateStatus();
                    calStatus();
                })
                .catch(err => {
                    document.getElementById('fileListContainer').innerHTML = '<div class="loading">' + escapeHtml(tr('FILE_LIST_UNAVAILABLE')) + '</div>';
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

        const recorderProcessStates = {};
        const recorderDailySequences = {};
        const recorderDailyFlightResults = {};

        function recorderFileBasename(filename) {
            const normalized = String(filename || '').replace(/\\/g, '/');
            const slash = normalized.lastIndexOf('/');
            return slash >= 0 ? normalized.substring(slash + 1) : normalized;
        }

        function recorderDailyKey(filename) {
            const name = recorderFileBasename(filename);
            const match = name.match(/^(.*_\d{8})_\d+\.bin$/i);
            return match ? match[1] + '.bin' : name;
        }

        function recorderDailyDisplayName(dailyKey) {
            const name = String(dailyKey || '').replace(/\.bin$/i, '');
            const match = name.match(/^(.*)_(\d{4})(\d{2})(\d{2})$/);
            if (!match) return name;
            return match[1] + '   ' + match[4] + '/' + match[3] + '/' + match[2];
        }

        function recorderFileSuffix(filename) {
            const match = String(filename || '').match(/_(\d+)\.bin$/i);
            return match ? Number(match[1]) : 0;
        }

        function recorderFilesForDay(dailyKey, readyOnly) {
            return (allFiles || [])
                .filter(file => recorderDailyKey(file.name) === dailyKey)
                .filter(file => !readyOnly || recorderProcessState(file.name) === 'ready')
                .sort((a, b) => recorderFileSuffix(a.name) - recorderFileSuffix(b.name));
        }

        function recorderProcessState(filename) {
            return recorderProcessStates[recorderFileBasename(filename)] || 'ready';
        }

        function recorderDailyProcessState(dailyKey) {
            const files = recorderFilesForDay(dailyKey, false);
            const states = files.map(file => recorderProcessState(file.name));
            if (states.indexOf('downloading') >= 0) return 'downloading';
            if (states.indexOf('analyzing') >= 0) return 'analyzing';
            if (states.indexOf('ready') >= 0) return 'ready';
            if (states.indexOf('finalizing') >= 0) return 'finalizing';
            if (states.indexOf('uploading') >= 0) return 'uploading';
            return states.length ? 'queued' : 'ready';
        }

        function recorderProcessLabel(state) {
            if (state === 'downloading' || state === 'analyzing') return tr('PROCESSING');
            if (state === 'queued') return tr('QUEUED');
            if (state === 'uploading') return tr('UPLOADING');
            if (state === 'finalizing') return tr('FINALIZING');
            return tr('PROCESS');
        }

        function recorderLocalProcessingActive() {
            return Object.keys(recorderProcessStates).some(filename => {
                const state = recorderProcessState(filename);
                return state === 'downloading' || state === 'analyzing';
            });
        }

        function refreshRecorderProcessButtons() {
            const localProcessingActive = recorderLocalProcessingActive();
            document.querySelectorAll('.process-file-btn').forEach(button => {
                const current = recorderDailyProcessState(button.dataset.dailykey);
                button.disabled = current !== 'ready' || localProcessingActive;
                button.textContent = recorderProcessLabel(current);
            });
        }

        function setRecorderProcessState(filename, state) {
            const name = recorderFileBasename(filename);
            if (!name) return;
            if (!state || state === 'ready') delete recorderProcessStates[name];
            else recorderProcessStates[name] = state;
            refreshRecorderProcessButtons();
        }

        function restoreRecorderProcessStates() {
            if (!window.SLMAndroid ||
                typeof window.SLMAndroid.getRecorderTransferStates !== 'function') return;
            try {
                const states = JSON.parse(window.SLMAndroid.getRecorderTransferStates());
                if (!states || !Array.isArray(states.files)) return;
                states.files.forEach(item => {
                    if (item && item.filename) {
                        const name = recorderFileBasename(item.filename);
                        if (name) recorderProcessStates[name] = String(item.state || 'queued');
                    }
                });
            } catch (err) {
                console.warn('Recorder process state could not be restored', err);
            }
        }

        function recorderDailyGroups(files) {
            const groups = {};
            (files || []).forEach(file => {
                const key = recorderDailyKey(file.name);
                if (!groups[key]) groups[key] = { key: key, files: [], size: 0 };
                groups[key].files.push(file);
                groups[key].size += Number(file.size || 0);
            });
            return Object.keys(groups)
                .map(key => groups[key])
                .sort((a, b) => b.key.localeCompare(a.key));
        }

        // Build one visible entry per recording day. Physical immutable files
        // remain independent and are processed sequentially behind this view.
        function displayFiles(files) {
            const container = document.getElementById('fileListContainer');
            const groups = recorderDailyGroups(files);
            document.getElementById('fileCount').textContent = groups.length;

            if (groups.length === 0) {
                container.innerHTML = '<div class="loading">' + escapeHtml(tr('NO_FILES_FOUND')) + '</div>';
                return;
            }

            let html = '<div class="file-list">';
            groups.forEach(group => {
                const displayName = recorderDailyDisplayName(group.key);
                const safeName = escapeHtml(displayName);
                const jsKey = JSON.stringify(group.key);
                const sizeKb = Math.round(group.size / 1000);
                const processState = recorderDailyProcessState(group.key);
                const processDisabled = processState !== 'ready' || recorderLocalProcessingActive()
                    ? ' disabled' : '';
                const processLabel = recorderProcessLabel(processState);

                html += `<div class="file-item">
                    <div class="file-row">
                        <div class="file-name">${safeName}</div>
                        <button class="btn btn-primary file-btn process-file-btn" data-dailykey="${escapeHtml(group.key)}" onclick='processDailyFiles(${jsKey})'${processDisabled}>${processLabel}</button>
                    </div>
                    <div class="file-row">
                        <div class="file-size">${escapeHtml(tr('SIZE_LABEL'))} ${sizeKb} kB</div>
                    </div>
                </div>`;
            });
            html += '</div>';
            container.innerHTML = html;
            refreshRecorderProcessButtons();
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

        function processDailyFiles(dailyKey) {
            if (recorderDailyProcessState(dailyKey) !== 'ready') return;
            const files = recorderFilesForDay(dailyKey, true);
            if (!files.length) return;

            recorderDailyFlightResults[dailyKey] = [];
            recorderDailySequences[dailyKey] = {
                files: files,
                nextIndex: 0,
                totalBytes: files.reduce((sum, file) => sum + Number(file.size || 0), 0),
                completedBytes: 0,
                currentBytes: 0
            };
            startNextDailyFile(dailyKey);
        }

        function startNextDailyFile(dailyKey) {
            const sequence = recorderDailySequences[dailyKey];
            if (!sequence) return;
            if (sequence.nextIndex >= sequence.files.length) {
                renderDailyFlightAnalysis(dailyKey, true);
                setProgress('downloadProgressBar', 'downloadProgressText', 100, tr('PROCESSING_PREFIX') + ': ' + tr('COMPLETE'));
                delete recorderDailySequences[dailyKey];
                refreshRecorderProcessButtons();
                return;
            }

            const file = sequence.files[sequence.nextIndex++];
            sequence.currentBytes = Number(file.size || 0);
            startPhysicalRecorderFile(dailyKey, file.name);
        }

        // One file contributes 0..100% to the complete-day progress. File transfer
        // occupies 0..95% and browser flight analysis occupies 95..100%.
        // The day percentage is weighted by physical file size so multi-file days
        // progress according to the amount of recorded data rather than file count.
        function updateDailyProgress(dailyKey, currentPercent) {
            const sequence = recorderDailySequences[dailyKey];
            if (!sequence || sequence.totalBytes <= 0) return;
            const currentDone = sequence.currentBytes * Math.max(0, Math.min(100, currentPercent)) / 100;
            const percent = Math.round(100 * (sequence.completedBytes + currentDone) / sequence.totalBytes);
            const status = document.getElementById('flightAnalysisStatus');
            if (status) status.textContent = tr('PROCESSING_PREFIX') + ': ' + percent + '%';
            setProgress('downloadProgressBar', 'downloadProgressText', percent,
                tr('PROCESSING_PREFIX') + ': ' + percent + '%');
        }

        function updateDailyTransferProgress(dailyKey, transferPercent) {
            const p = Math.max(0, Math.min(100, Number(transferPercent) || 0));
            updateDailyProgress(dailyKey, p * 0.95);
        }

        function updateDailyAnalysisProgress(dailyKey, analysisPercent) {
            const p = Math.max(0, Math.min(100, Number(analysisPercent) || 0));
            updateDailyProgress(dailyKey, 95 + p * 0.05);
        }

        function markDailyFileQueued(filename) {
            const dailyKey = recorderDailyKey(filename);
            const sequence = recorderDailySequences[dailyKey];
            if (!sequence) return;
            sequence.completedBytes += sequence.currentBytes;
            sequence.currentBytes = 0;
            updateDailyProgress(dailyKey, 0);
            startNextDailyFile(dailyKey);
        }

        function abortDailySequence(filename) {
            const dailyKey = recorderDailyKey(filename);
            delete recorderDailySequences[dailyKey];
            refreshRecorderProcessButtons();
        }

        function addDailyFlightAnalysis(dailyKey, analysis) {
            if (!analysis || !analysis.ok || !Array.isArray(analysis.flights)) return;
            if (!Array.isArray(recorderDailyFlightResults[dailyKey])) {
                recorderDailyFlightResults[dailyKey] = [];
            }
            analysis.flights.forEach(function(flight) {
                recorderDailyFlightResults[dailyKey].push(flight);
            });
            renderDailyFlightAnalysis(dailyKey, false);
        }

        function renderDailyFlightAnalysis(dailyKey, complete) {
            const status = document.getElementById('flightAnalysisStatus');
            const table = document.getElementById('flightAnalysisTable');
            const flights = Array.isArray(recorderDailyFlightResults[dailyKey]) ?
                recorderDailyFlightResults[dailyKey] : [];
            const displayName = recorderDailyDisplayName(dailyKey);

            if (!flights.length) {
                if (complete) status.textContent = displayName + ' \u2014 ' + tr('NO_FLIGHT_DETECTED');
                table.innerHTML = '';
                return;
            }

            if (complete) {
                status.textContent = displayName + ' \u2014 ' + flights.length + ' ' + tr('FLIGHT_PLURAL');
            }
            let html = '<table class="analysis-table"><thead><tr>' +
                '<th>' + escapeHtml(tr('TABLE_FLT')).replace(' ', '<br>') + '</th><th>' + escapeHtml(tr('TABLE_TAKEOFF')) + '</th><th>' + escapeHtml(tr('TABLE_LANDING')) + '</th><th>' + escapeHtml(tr('TABLE_FLIGHT_TIME')).replace(' ', '<br>') + '</th>' +
                '</tr></thead><tbody>';
            flights.forEach(function(f, index) {
                html += '<tr>' +
                    '<td>' + (index + 1) + '</td>' +
                    '<td class="mono">' + escapeHtml(f.takeoffRollStartShort || f.takeoffRollStart) + '</td>' +
                    '<td class="mono">' + escapeHtml(f.landingRollEndShort || f.landingRollEnd) + '</td>' +
                    '<td class="mono">' + escapeHtml(f.flightDurationShort || f.flightDuration) + '</td>' +
                    '</tr>';
            });
            html += '</tbody></table>';
            table.innerHTML = html;
        }

        function showRecorderProcessingFailure(message) {
            const status = document.getElementById('flightAnalysisStatus');
            if (!status) return;

            const detail = String(message || '').trim();
            let text = tr('PROCESSING_FAILED') + (detail ? (': ' + detail) : '.');
            if (detail && !/[.!?]$/.test(text)) text += '.';

            if (window.SLMAndroid &&
                typeof window.SLMAndroid.downloadRecorderFile === 'function') {
                text += ' ' + tr('CHECK_BRIDGE_CONNECTION');
            } else {
                text += ' ' + tr('CHECK_RECORDER_CONNECTION');
            }
            status.textContent = text;
        }

        // Start one physical immutable file. The daily UI calls this repeatedly
        // and the existing Bridge continues to transfer each file independently.
        function startPhysicalRecorderFile(dailyKey, filename) {
            const status = document.getElementById('flightAnalysisStatus');
            const panel = document.getElementById('flightAnalysisWindow');
            const table = document.getElementById('flightAnalysisTable');

            panel.classList.remove('hidden');
            const sequence = recorderDailySequences[dailyKey];
            if (sequence && sequence.nextIndex === 1) {
                table.innerHTML = '';
                status.textContent = tr('PROCESSING_DOT');
            }
            if (recorderProcessState(filename) !== 'ready') {
                showRecorderProcessingFailure(tr('TRANSFER_ALREADY_ACTIVE'));
                abortDailySequence(filename);
                return;
            }
            setRecorderProcessState(filename, 'downloading');
            updateDailyTransferProgress(dailyKey, 0);

            if (startAndroidRecorderTransfer(filename)) return;

            downloadFileBuffer(filename, function(percent) {
                updateDailyTransferProgress(dailyKey, percent);
            })
                .then(buffer => {
                    updateDailyTransferProgress(dailyKey, 100);
                    saveDownloadedBuffer(filename, buffer);
                    setRecorderProcessState(filename, 'analyzing');
                    return analyzeRecorderFileAsync(buffer, function(percent) {
                        updateDailyAnalysisProgress(dailyKey, percent);
                    })
                        .then(analysis => {
                            updateDailyAnalysisProgress(dailyKey, 100);
                            addDailyFlightAnalysis(dailyKey, analysis);
                            return writeFlightAnalysisLog(filename, analysis);
                        })
                        .then(() => {
                            setRecorderProcessState(filename, 'queued');
                            markDailyFileQueued(filename);
                        });
                })
                .catch(err => {
                    setRecorderProcessState(filename, 'ready');
                    abortDailySequence(filename);
                    showRecorderProcessingFailure(err && err.message ? err.message : tr('DOWNLOAD_ANALYSIS_INCOMPLETE'));
                    console.error(err);
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
                if (!capabilities || Number(capabilities.bridgeVersion) < 2) return false;
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
            const dailyKey = recorderDailyKey(filename);
            const percent = Math.max(0, Math.min(100, Number(detail.percent) || 0));
            const status = document.getElementById('flightAnalysisStatus');

            if (state === 'download-started') {
                setRecorderProcessState(filename, 'downloading');
                updateDailyTransferProgress(dailyKey, 0);
            } else if (state === 'downloading') {
                setRecorderProcessState(filename, 'downloading');
                updateDailyTransferProgress(dailyKey, percent);
            } else if (state === 'download-complete') {
                updateDailyTransferProgress(dailyKey, 100);
                setRecorderProcessState(filename, 'analyzing');
                fetch(detail.localUrl, { cache: 'no-store' })
                    .then(function(response) {
                        if (!response.ok) throw new Error(tr('PRIVATE_FILE_UNAVAILABLE'));
                        return response.arrayBuffer();
                    })
                    .then(function(buffer) {
                        return analyzeRecorderFileAsync(buffer, function(percent) {
                            updateDailyAnalysisProgress(dailyKey, percent);
                        });
                    })
                    .then(function(analysis) {
                        updateDailyAnalysisProgress(dailyKey, 100);
                        addDailyFlightAnalysis(dailyKey, analysis);
                        return writeFlightAnalysisLog(filename, analysis);
                    })
                    .then(function() {
                        setRecorderProcessState(filename, 'queued');
                        notifyAndroidAnalysisComplete(detail.transferId);
                        markDailyFileQueued(filename);
                    })
                    .catch(function(err) {
                        console.error(err);
                        notifyAndroidAnalysisFailed(detail.transferId);
                        setRecorderProcessState(filename, 'ready');
                        abortDailySequence(filename);
                        showRecorderProcessingFailure(err && err.message ? err.message : tr('DOWNLOADED_FILE_NOT_ANALYZED'));
                    });
            } else if (state === 'upload-pending') {
                setRecorderProcessState(filename, 'queued');
            } else if (state === 'upload-started' || state === 'uploading') {
                setRecorderProcessState(filename, 'uploading');
            } else if (state === 'upload-complete' || state === 'archive-pending') {
                setRecorderProcessState(filename, 'finalizing');
            } else if (state === 'archive-complete') {
                setRecorderProcessState(filename, 'ready');
                refreshFiles();
            } else if (state === 'failed') {
                setRecorderProcessState(filename, 'ready');
                abortDailySequence(filename);
                showRecorderProcessingFailure(detail.reason ? apiReasonText(detail.reason, 'DOWNLOAD_ANALYSIS_INCOMPLETE') : tr('DOWNLOAD_ANALYSIS_INCOMPLETE'));
            }
        });

        function notifyAndroidAnalysisFailed(transferId) {
            if (!transferId || !window.SLMAndroid ||
                typeof window.SLMAndroid.analysisFailed !== 'function') return;
            try {
                window.SLMAndroid.analysisFailed(String(transferId));
            } catch (err) {
                console.warn('failed recorder transfer could not be cleared', err);
            }
        }

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
                            tr('DOWNLOAD_PREFIX') + ': ' + percent + '% (' +
                                formatBytes(evt.loaded) + ' / ' + formatBytes(evt.total) + ')'
                        );
                    } else {
                        progressCb(0, tr('DOWNLOAD_PREFIX') + ': ' + formatBytes(evt.loaded) + ' ' + tr('RECEIVED_SUFFIX'));
                    }
                };

                xhr.onload = function() {
                    if (xhr.status >= 200 && xhr.status < 300) {
                        resolve(xhr.response);
                    } else {
                        reject(new Error(tr('DOWNLOAD_FAILED') + ': HTTP ' + xhr.status));
                    }
                };

                xhr.onerror = function() {
                    reject(new Error(tr('DOWNLOAD_FAILED')));
                };

                xhr.send();
            });
        }

        function clearFlightAnalysisPanel() {
            const panel = document.getElementById('flightAnalysisWindow');
            const status = document.getElementById('flightAnalysisStatus');
            const table = document.getElementById('flightAnalysisTable');
            if (table) table.innerHTML = '';
            if (status) status.textContent = tr('NO_ANALYSIS_AVAILABLE');
            setProgress('downloadProgressBar', 'downloadProgressText', 0, tr('DOWNLOAD_IDLE'));
            if (panel) panel.classList.add('hidden');
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
            const match = String(filename || '').match(/^(.+?)_(\d{4})(\d{2})(\d{2})_(\d+)\.log$/i);
            if (!match) return null;
            return {
                registration: match[1],
                dateKey: match[2] + match[3] + match[4],
                date: match[2] + '-' + match[3] + '-' + match[4],
                session: Number(match[5]) || 0
            };
        }

        function latestLogbookDays(files, dayCount) {
            const groups = new Map();
            (files || []).forEach(file => {
                const parts = logbookFileParts(file.name);
                if (!parts) return;
                const key = parts.registration + '|' + parts.dateKey;
                if (!groups.has(key)) {
                    groups.set(key, {
                        registration: parts.registration,
                        dateKey: parts.dateKey,
                        date: parts.date,
                        files: []
                    });
                }
                groups.get(key).files.push({ name: file.name, session: parts.session });
            });

            return Array.from(groups.values())
                .sort((a, b) => {
                    if (a.dateKey !== b.dateKey) return b.dateKey.localeCompare(a.dateKey);
                    return a.registration.localeCompare(b.registration);
                })
                .slice(0, dayCount)
                .map(day => {
                    day.files.sort((a, b) => a.session - b.session);
                    return day;
                });
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

        function flightDurationFromTimes(takeoff, landing) {
            const parseMinutes = value => {
                const match = String(value || '').match(/^(\d{1,2}):(\d{2})$/);
                if (!match) return null;
                return Number(match[1]) * 60 + Number(match[2]);
            };
            const start = parseMinutes(takeoff);
            const end = parseMinutes(landing);
            if (start === null || end === null) return '-';
            let duration = end - start;
            if (duration < 0) duration += 24 * 60;
            return String(Math.floor(duration / 60)).padStart(2, '0') + ':' +
                String(duration % 60).padStart(2, '0');
        }

        function parseLogbookRows(filename, text) {
            const parts = logbookFileParts(filename);
            const date = parts ? parts.date : '-';
            const rows = [];
            String(text || '').split(/\r?\n/).forEach(line => {
                const fields = line.trim().split(',');
                if (fields.length !== 2) return;
                const takeoff = fields[0].trim();
                const landing = fields[1].trim();
                if (!/^\d{1,2}:\d{2}$/.test(takeoff) || !/^\d{1,2}:\d{2}$/.test(landing)) return;
                rows.push({
                    date: date,
                    takeoff: takeoff,
                    landing: landing,
                    flightTime: flightDurationFromTimes(takeoff, landing)
                });
            });
            return rows;
        }

        function refreshLogbook() {
            const container = document.getElementById('logbookContainer');
            container.innerHTML = '<div class="loading">' + escapeHtml(tr('LOADING_LOGBOOK')) + '</div>';
            recorderJsonWithRetry('/api/logbook/files')
                .then(result => {
                    if (!result.ok || !result.json || !result.json.ok) {
                        throw new Error('Recorder logbook list unavailable');
                    }
                    const days = latestLogbookDays(result.json.files || [], 5);
                    if (days.length === 0) {
                        displayLogbookRows([]);
                        return;
                    }

                    const rows = [];
                    let chain = Promise.resolve();
                    days.forEach(day => {
                        day.files.forEach(file => {
                            chain = chain.then(() =>
                                recorderTextWithRetry('/api/download?file=' +
                                    encodeURIComponent('/' + String(file.name || '')), 1)
                                    .catch(() => recorderTextWithRetry('/api/download?file=' +
                                        encodeURIComponent('/processed/' + String(file.name || '')), 1))
                                    .then(text => {
                                        rows.push.apply(rows, parseLogbookRows(file.name, text));
                                    })
                                    .catch(err => console.warn(err))
                            );
                        });
                    });
                    return chain.then(() => displayLogbookRows(rows));
                })
                .catch(err => {
                    console.error(err);
                    container.innerHTML = '<div class="loading">' + escapeHtml(tr('LOGBOOK_UNAVAILABLE')) + '</div>';
                });
        }

        function displayLogbookRows(rows) {
            const container = document.getElementById('logbookContainer');
            if (!rows || rows.length === 0) {
                container.innerHTML = '<div class="loading">' + escapeHtml(tr('NO_DOWNLOADED_LOGS')) + '</div>';
                return;
            }
            let html = '<table class="analysis-table"><thead><tr>' +
                '<th>' + escapeHtml(tr('TABLE_DATE')) + '</th><th>' + escapeHtml(tr('TABLE_TAKEOFF')) + '</th><th>' + escapeHtml(tr('TABLE_LANDING')) + '</th><th>' + escapeHtml(tr('TABLE_FLIGHT_TIME_LONG')).replace(' ', '<br>') + '</th>' +
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
                const reason = analysis && analysis.reason ? analysis.reason : tr('ANALYSIS_FAILED');
                status.textContent = filename + ' \u2014 ' + tr('COMPLETE') + ': ' + reason;
                table.innerHTML = '';
                return;
            }

            if (analysis.flights.length === 0) {
                status.textContent = filename + ' \u2014 ' + tr('COMPLETE') + ': ' + tr('NO_FLIGHT_DETECTED').toLowerCase();
                table.innerHTML = '';
                return;
            }

            status.textContent = filename + ' \u2014 ' + tr('COMPLETE') + ': ' + analysis.flights.length + ' ' + tr('FLIGHT_PLURAL');

            let html = '<table class="analysis-table"><thead><tr>' +
                '<th>' + escapeHtml(tr('TABLE_FLT')).replace(' ', '<br>') + '</th><th>' + escapeHtml(tr('TABLE_TAKEOFF')) + '</th><th>' + escapeHtml(tr('TABLE_LANDING')) + '</th><th>' + escapeHtml(tr('TABLE_FLIGHT_TIME')).replace(' ', '<br>') + '</th>' +
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

        // Build one compact per-recording log. Each valid line contains only
        // the minute-rounded takeoff and landing times separated by a comma.
        function buildFlightAnalysisLogText(filename, analysis) {
            const lines = [];
            if (analysis && analysis.ok && Array.isArray(analysis.flights)) {
                analysis.flights.forEach(f => {
                    const takeoff = f.takeoffRollStartShort || f.takeoffRollStart || '';
                    const landing = f.landingRollEndShort || f.landingRollEnd || '';
                    if (takeoff && landing) lines.push(takeoff + ',' + landing);
                });
            }
            return lines.length > 0 ? (lines.join('\n') + '\n') : '';
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
