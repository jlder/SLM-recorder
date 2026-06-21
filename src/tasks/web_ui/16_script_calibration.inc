/*******************************************************************************
 * SCRIPT: CALIBRATION WORKFLOWS
 * JavaScript for recorder sensor calibration and installation calibration polling, status display, save, and cancel operations.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        function calStatus() {
            fetch('/api/cal/status')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('calTopStatus').textContent = data.recording_allowed ? 'ready' : (data.sensor_valid ? 'install required' : data.status);
                    const sensorDateText = data.sensor_valid ? fmtDate(data.sensor_date) : '-';
                    const installDateText = data.installation_valid ? fmtDate(data.installation_date) : '-';
                    const sensorParts = data.sensor_valid ? fmtDateParts(data.sensor_date) : {date:'-', time:'-'};
                    const installParts = data.installation_valid ? fmtDateParts(data.installation_date) : {date:'-', time:'-'};
                    document.getElementById('sensorCalDate').textContent = sensorParts.date;
                    document.getElementById('sensorCalTime').textContent = sensorParts.time;
                    document.getElementById('installationCalDate').textContent = installParts.date;
                    document.getElementById('installationCalTime').textContent = installParts.time;
                    updateMaintenanceButtonState(data);
                    const accelStatus = data.sensor_valid ? ('valid since ' + sensorDateText) : data.status;
                    const installStatus = data.installation_valid ? ('valid since ' + installDateText) : 'missing';
                    document.getElementById('calStatus').textContent = accelStatus;
                    document.getElementById('installStatus').textContent = installStatus;
                })
                .catch(err => console.error(err));
        }

        function startCalPolling() {
            if (calPoll) return;
            calPoll = setInterval(function(){
                if (calView === 'accel') calSample();
                else if (calView === 'install') installSample();
            }, 500);
            if (calView === 'accel') calSample();
            else if (calView === 'install') installSample();
        }

        function stopCalPolling() {
            if (calPoll) {
                clearInterval(calPoll);
                calPoll = null;
            }
        }

        function resetAccelCalUi() {
            document.getElementById('calStatus').textContent = '-';
            document.getElementById('calSamplesProcessed').textContent = '0';
            document.getElementById('calLowestNoise').textContent = '-';
            document.getElementById('calLastBestUpdate').textContent = '-';
            updateFaceSummary(null, -1, false);
            calLastBestLocalMs = 0;
            calLastBestLocalUpdates = 0;
            calLastBestLocalFace = -1;
            for (let i = 0; i < 3; i++) {
                document.getElementById('resGain' + i).textContent = '-';
                document.getElementById('resOffset' + i).textContent = '-';
                document.getElementById('nvsGain' + i).textContent = '-';
                document.getElementById('nvsOffset' + i).textContent = '-';
            }
            document.getElementById('btnStart').disabled = false;
            document.getElementById('btnSave').disabled = true;
            document.getElementById('btnCancel').disabled = true;
            logCal('Ready.');
        }

        function resetInstallCalUi() {
            document.getElementById('installStatus').textContent = '-';
            document.getElementById('installTotalSamples').textContent = '0';
            document.getElementById('installBestNoise').textContent = '-';
            document.getElementById('installLastUpdate').textContent = '-';
            document.getElementById('installMatrix').textContent = 'Matrix will appear after a stable candidate is found.';
            document.getElementById('btnInstallStart').disabled = false;
            document.getElementById('btnInstallSave').disabled = true;
            document.getElementById('btnInstallCancel').disabled = true;
        }

        function faceIndexFromName(name) {
            const names = ['+X','-X','+Y','-Y','+Z','-Z'];
            return names.indexOf(name);
        }

        function fmtDateParts(dt) {
            if (!dt || !dt.year || !dt.month || !dt.day) return { date: '-', time: '-' };
            const pad2 = function(v) { return String(v).padStart(2, '0'); };
            const date = String(dt.year) + '-' + pad2(dt.month) + '-' + pad2(dt.day);
            let time = '-';
            if (dt.hour !== undefined && dt.min !== undefined && dt.sec !== undefined) {
                time = pad2(dt.hour) + ':' + pad2(dt.min) + ':' + pad2(dt.sec);
            }
            return { date: date, time: time };
        }

        function fmtDate(dt) {
            const parts = fmtDateParts(dt);
            if (parts.date === '-') return '-';
            return (parts.time === '-') ? parts.date : (parts.date + ' ' + parts.time);
        }

        function fmtNoise(v) {
            if (v === null || v === undefined || !isFinite(Number(v)) || Number(v) > 900000) return '-';
            return Number(v).toFixed(2) + ' mg';
        }

        function updateFaceSummary(faceValid, activeIdx, active) {
            const names = ['+X', '-X', '+Y', '-Y', '+Z', '-Z'];
            const parts = [];
            for (let i = 0; i < names.length; i++) {
                const ok = !!(active && faceValid && faceValid[i]);
                const isActive = !!active && (i === activeIdx);
                const result = ok ? 'OK' : '—';
                let cls = 'face-chip';
                if (ok) {
                    cls += ' face-chip-ok';
                }
                if (isActive && !ok) {
                    cls += ' face-chip-warn';
                }
                if (isActive) {
                    cls += ' face-chip-active';
                }
                parts.push('<div class="' + cls + '">' +
                           '<span class="face-name">' + names[i] + '</span>' +
                           '<span class="face-result">' + result + '</span>' +
                           '</div>');
            }
            document.getElementById('calFaceSummary').innerHTML = parts.join('');
        }

        function updateResultTable(data) {
            document.getElementById('btnStart').disabled = !!data.active;
            document.getElementById('btnCancel').disabled = !data.active;
            document.getElementById('btnSave').disabled = !data.result_available;

            for (let i = 0; i < 3; i++) {
                const r = data.result_available && data.result ? data.result[i] : null;
                const n = data.nvs_result_available && data.nvs_result ? data.nvs_result[i] : null;

                document.getElementById('resGain' + i).innerHTML = r ? '<span class="ok">' + r.gain.toFixed(6) + '</span>' : '-';
                document.getElementById('resOffset' + i).innerHTML = r ? '<span class="ok">' + r.offset.toFixed(1) + '</span>' : '-';

                document.getElementById('nvsGain' + i).textContent = n ? n.gain.toFixed(6) : '-';
                document.getElementById('nvsOffset' + i).textContent = n ? n.offset.toFixed(1) : '-';
            }
        }

        function calSample() {
            fetch('/api/cal/sample')
                .then(r => r.json().then(j => ({ok:r.ok, status:r.status, json:j})))
                .then(res => {
                    handleCalibrationAuthError(res);
                    const data = res.json;
                    if (!data.ok) {
                        logCal(JSON.stringify(data, null, 2));
                        return;
                    }

                    const currentFaceName = data.current_face_valid ? data.current_face : (data.candidate_valid ? data.candidate_face : '-');
                    document.getElementById('calSamplesProcessed').textContent = data.samples || 0;
                    const activeIdx = currentFaceName !== '-' ? faceIndexFromName(currentFaceName) : -1;
                    const activeUpdates = (activeIdx >= 0 && data.face_updates) ? (data.face_updates[activeIdx] || 0) : 0;
                    const activeAge = (activeIdx >= 0 && data.face_last_update_age_ms) ? data.face_last_update_age_ms[activeIdx] : null;
                    document.getElementById('calLowestNoise').textContent = activeIdx >= 0 ? fmtNoise(data.face_quality && data.face_quality[activeIdx]) : '-';
                    if (activeIdx < 0 || activeUpdates === 0) {
                        calLastBestLocalMs = 0;
                        calLastBestLocalUpdates = 0;
                        calLastBestLocalFace = activeIdx;
                    } else if (calLastBestLocalFace !== activeIdx || calLastBestLocalUpdates !== activeUpdates) {
                        const ageMs = (activeAge !== undefined && activeAge !== null) ? Number(activeAge) : 0;
                        calLastBestLocalMs = Date.now() - Math.max(0, ageMs);
                        calLastBestLocalUpdates = activeUpdates;
                        calLastBestLocalFace = activeIdx;
                    }
                    document.getElementById('calLastBestUpdate').textContent = activeUpdates > 0 ? fmtAgeFromLocal(calLastBestLocalMs) : '-';

                    updateFaceSummary(data.face_valid, activeIdx, !!data.active);
                    updateResultTable(data);
                })
                .catch(err => logCal('sample error: ' + err));
        }

        function calStart() {
            setCalSavedUi(false);
            document.getElementById('btnStart').disabled = true;
            document.getElementById('btnCancel').disabled = false;
            postJson('/api/cal/start')
                .then(res => {
                    logCal(prettyJson(res.json));
                    calStatus();
                    startCalPolling();
                });
        }


        function calCancel() {
            postJson('/api/cal/cancel')
                .then(res => {
                    setCalSavedUi(false);
                    document.getElementById('btnStart').disabled = false;
                    document.getElementById('btnCancel').disabled = true;
                    logCal(prettyJson(res.json));
                    calStatus();
                    calSample();
                });
        }

        function calSave() {
            const save = document.getElementById('btnSave');
            save.disabled = true;

            postJson('/api/cal/save')
                .then(res => {
                    if (res.ok && res.json && res.json.saved) {
                        logCal('SAVED SUCCESSFULLY\\n' + prettyJson(res.json));
                        setCalSavedUi(true);
                        calStatus();
                        return;
                    }

                    save.disabled = false;
                    logCal('SAVE FAILED\\n' + prettyJson(res.json));
                    calStatus();
                    calSample();
                })
                .catch(err => {
                    save.disabled = false;
                    logCal('SAVE ERROR: ' + err);
                });
        }


        function fmtMatrix(m) {
            if (!m || m.length < 9) return '-';
            return '[' + Number(m[0]).toFixed(6) + ' ' + Number(m[1]).toFixed(6) + ' ' + Number(m[2]).toFixed(6) + ']\n' +
                   '[' + Number(m[3]).toFixed(6) + ' ' + Number(m[4]).toFixed(6) + ' ' + Number(m[5]).toFixed(6) + ']\n' +
                   '[' + Number(m[6]).toFixed(6) + ' ' + Number(m[7]).toFixed(6) + ' ' + Number(m[8]).toFixed(6) + ']';
        }

        function fmtAge(ms) {
            if (ms === undefined || ms === null) return '-';
            if (ms <= 0) return 'now';
            const sec = Math.round(ms / 1000);
            if (sec < 60) return sec + ' s ago';
            const min = Math.floor(sec / 60);
            const rem = sec % 60;
            return min + 'm ' + rem + 's ago';
        }


        function fmtAgeFromLocal(startMs) {
            if (!startMs) return '-';
            return fmtAge(Date.now() - startMs);
        }

        function installSample() {
            fetch('/api/install/sample')
                .then(r => r.json().then(j => ({ok:r.ok, status:r.status, json:j})))
                .then(res => {
                    handleCalibrationAuthError(res);
                    const data = res.json;
                    if (!data.ok) return;
                    document.getElementById('installTotalSamples').textContent = data.total_samples || 0;
                    document.getElementById('installBestNoise').textContent = data.candidate_valid ? fmtNoise(data.quality_mg) : '-';
                    document.getElementById('installLastUpdate').textContent = (data.update_count || 0) > 0 ? fmtAge(data.last_update_age_ms) : '-';
                    document.getElementById('installStatus').textContent = data.stored_valid ? ('valid since ' + fmtDate(data.stored_date)) : 'missing';
                    if (data.candidate_valid) {
                        document.getElementById('installMatrix').textContent = fmtMatrix(data.matrix);
                    } else if (data.stored_valid) {
                        document.getElementById('installMatrix').textContent = fmtMatrix(data.stored_matrix);
                    } else {
                        document.getElementById('installMatrix').textContent = 'Waiting for stable level-flight attitude.';
                    }
                    document.getElementById('btnInstallStart').disabled = !!data.active;
                    document.getElementById('btnInstallCancel').disabled = !data.active;
                    document.getElementById('btnInstallSave').disabled = !data.candidate_valid;
                })
                .catch(err => console.error(err));
        }

        function installStart() {
            document.getElementById('btnInstallStart').disabled = false;
            document.getElementById('btnInstallSave').disabled = true;
            document.getElementById('btnInstallCancel').disabled = true;
            postJson('/api/install/start')
                .then(res => {
                    document.getElementById('installMatrix').textContent = 'Waiting for stable level-flight attitude.';
                    calStatus();
                    installSample();
                    startCalPolling();
                });
        }

        function installCancel() {
            postJson('/api/install/cancel')
                .then(res => {
                    document.getElementById('btnInstallStart').disabled = false;
                    document.getElementById('btnInstallCancel').disabled = true;
                    document.getElementById('installMatrix').textContent = 'Waiting for stable level-flight attitude.';
                    calStatus();
                    installSample();
                });
        }

        function installSave() {
            const save = document.getElementById('btnInstallSave');
            save.disabled = true;
            postJson('/api/install/save')
                .then(res => {
                    if (res.ok && res.json && res.json.matrix) {
                        document.getElementById('installMatrix').textContent = fmtMatrix(res.json.matrix);
                    } else {
                        document.getElementById('installMatrix').textContent = prettyJson(res.json);
                    }
                    calStatus();
                    installSample();
                    if (!res.ok) save.disabled = false;
                })
                .catch(err => {
                    document.getElementById('installMatrix').textContent = 'SAVE ERROR: ' + err;
                    save.disabled = false;
                });
        }

)rawliteral"
