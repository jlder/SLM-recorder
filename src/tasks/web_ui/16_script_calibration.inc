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
                    const faultText = (data.fault_reason && data.fault_reason !== 'none') ? (' fault: ' + data.fault_reason) : '';
                    let accelStatus = data.sensor_valid ? ('valid since ' + sensorDateText) : (data.status + faultText);
                    if (!data.sensor_valid && data.reference_available && !data.active_available) {
                        accelStatus = 'repeat recorder calibration required';
                    } else if (!data.sensor_valid && !data.reference_available) {
                        accelStatus = 'first recorder calibration required';
                    }
                    const installStatus = data.installation_valid ? ('valid since ' + installDateText) : 'missing';
                    if (!(calView === 'accel' && calRecorderSaveMessageSticky)) {
                        document.getElementById('calStatus').textContent = accelStatus;
                    }
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
            clearAccelCalMeasurementUi();
            calRecorderSaveMessageSticky = false;
            setRecorderStartButton(false, false);
            document.getElementById('btnSave').disabled = true;
            logCal('Ready.');
        }

        function resetInstallCalUi() {
            document.getElementById('installStatus').textContent = '-';
            document.getElementById('installTotalSamples').textContent = '0';
            document.getElementById('installBestNoise').textContent = '-';
            document.getElementById('installStability').textContent = '-';
            document.getElementById('installPitchCandidate').textContent = '-';
            document.getElementById('installRollCandidate').textContent = '-';
            document.getElementById('installPitchStored').textContent = '-';
            document.getElementById('installRollStored').textContent = '-';
            logInstall('Ready.');
            setInstallStartButton(false, false);
            document.getElementById('btnInstallSave').disabled = true;
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

        function fmtTemperature(v) {
            if (v === null || v === undefined || !isFinite(Number(v))) return '-';
            return Number(v).toFixed(1) + ' °C';
        }

        function updateCalibrationTemperature(data) {
            const available = !!data.temperature_available;
            const stable = !!data.temperature_stable;
            const minAllowed = data.temperature_allowed_min_c;
            const maxAllowed = data.temperature_allowed_max_c;
            const spanAllowed = data.temperature_allowed_span_c;
            let status = 'OK';
            if (!available) {
                status = 'UNAVAILABLE';
            } else if (minAllowed !== undefined && Number(data.temperature_c) < Number(minAllowed)) {
                status = 'TOO LOW';
            } else if (maxAllowed !== undefined && Number(data.temperature_c) > Number(maxAllowed)) {
                status = 'TOO HIGH';
            } else if (!stable) {
                status = 'WAIT STABLE';
            }

            let text = available ? (fmtTemperature(data.temperature_c) + ' ' + status) : status;
            if (available && !stable && spanAllowed !== undefined) {
                text += ' (span max ' + Number(spanAllowed).toFixed(1) + ' °C)';
            }
            document.getElementById('calSensorTemp').textContent = text;
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
            setRecorderStartButton(!!data.active, false);
            const temperatureOk = !!data.temperature_available && !!data.temperature_in_range && !!data.temperature_stable;
            document.getElementById('btnSave').disabled = !data.result_available || !temperatureOk;

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
                        logCal('Recorder calibration status unavailable.');
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
                    updateCalibrationTemperature(data);

                    updateFaceSummary(data.face_valid, activeIdx, !!data.active);
                    updateResultTable(data);

                    const temperatureOk = !!data.temperature_available && !!data.temperature_in_range && !!data.temperature_stable;
                    if (data.active && data.result_available && temperatureOk && !calRecorderSaveMessageSticky) {
                        logCal('Valid calibration solution, continue to optimize or Save.');
                    }
                })
                .catch(err => logCal('Recorder calibration communication error: ' + err));
        }

        function calStart() {
            const start = document.getElementById('btnStart');
            if (start && start.dataset.active === '1') {
                calCancel();
                return;
            }
            calRecorderSaveMessageSticky = false;
            setCalSavedUi(false);
            setRecorderStartButton(false, true);
            postJson('/api/cal/start')
                .then(res => {
                    if (!res.ok || !res.json || !res.json.ok) {
                        setRecorderStartButton(false, false);
                        logCal('Recorder calibration could not start.');
                        calStatus();
                        calSample();
                        return;
                    }
                    setRecorderStartButton(true, false);
                    logCal('Recorder calibration started. Place the recorder on each face until all six faces show OK.');
                    calStatus();
                    startCalPolling();
                })
                .catch(err => {
                    setRecorderStartButton(false, false);
                    logCal('Recorder calibration start error: ' + err);
                });
        }


        function calCancel() {
            calRecorderSaveMessageSticky = false;
            postJson('/api/cal/cancel')
                .then(res => {
                    setCalSavedUi(false);
                    setRecorderStartButton(false, false);
                    logCal('Recorder calibration cancelled.');
                    calStatus();
                    calSample();
                });
        }

        function calSave() {
            const save = document.getElementById('btnSave');
            save.disabled = true;
            calRecorderSaveMessageSticky = true;

            postJson('/api/cal/save')
                .then(res => {
                    if (res.ok && res.json && res.json.saved) {
                        logCal('Recorder calibration saved. Calibration report downloading.');
                        setCalSavedUi(true);
                        clearAccelCalMeasurementUi();
                        calStatus();
                        if (res.json && res.json.report_written) setTimeout(calDownloadLastReport, 1000);
                        return;
                    }

                    if (res.ok && res.json && res.json.reason === 'repeat_required') {
                        save.disabled = true;
                        logCal('First recorder calibration stored. Repeat recorder calibration once to confirm repeatability. Calibration report downloading.');
                        setCalSavedUi(true);
                        clearAccelCalMeasurementUi();
                        calStatus();
                        if (res.json && res.json.report_written) setTimeout(calDownloadLastReport, 1000);
                        return;
                    }

                    if (res.json && res.json.reason === 'delta_fault') {
                        logCal('SIGNIFICANT CALIBRATION DRIFT - CALIBRATION REJECTED. Candidate saved for support diagnostics but not used as current calibration. Flight records since the last valid recorder calibration should be considered suspect and potentially discarded unless another recorder calibration is successful. Check recorder seating in the calibration support, support condition, horizontal stable test surface, and temperature range. Retry calibration. If it repeats, contact support.');
                        const status = document.getElementById('calStatus');
                        if (status) status.textContent = 'SIGNIFICANT CALIBRATION DRIFT - CALIBRATION REJECTED';
                        setCalSavedUi(true);
                        clearAccelCalMeasurementUi();
                        calStatus();
                        if (res.json && res.json.report_written) setTimeout(calDownloadLastReport, 1000);
                        return;
                    }

                    save.disabled = false;
                    calRecorderSaveMessageSticky = false;
                    if (res.json && (res.json.reason === 'temperature_out_of_range' || res.json.reason === 'temperature_unavailable' || res.json.reason === 'temperature_unstable')) {
                        logCal('Temperature not acceptable for recorder calibration. Place recorder at room temperature and wait before retrying.');
                    } else {
                        logCal('Recorder calibration save failed. Check that all six faces are OK and retry.');
                    }
                    calStatus();
                    if (res.json && res.json.report_written) setTimeout(calDownloadLastReport, 1000);
                    calSample();
                })
                .catch(err => {
                    save.disabled = false;
                    calRecorderSaveMessageSticky = false;
                    logCal('Recorder calibration save error: ' + err);
                });
        }

        function aboutStatus(text) {
            const el = document.getElementById('aboutSupportStatus');
            if (el) el.textContent = text;
        }

        function supportDownloadData() {
            const code = prompt('Support code required to download support data');
            if (!code) return;
            window.location = '/api/cal/export?support=' + encodeURIComponent(code);
        }

        function calDownloadLastReport() {
            window.location = '/api/cal/report/recorder/latest';
        }


        function supportClearRecorderCalibration() {
            const code = prompt('Support code required to clear recorder calibration history');
            if (!code) return;
            aboutStatus('Clearing recorder calibration...');
            postJson('/api/cal/support_clear?support=' + encodeURIComponent(code))
                .then(res => {
                    if (res.ok && res.json && res.json.cleared) {
                        aboutStatus('Recorder calibration history cleared. Recorder calibration is required.');
                        calStatus();
                    } else {
                        aboutStatus('Recorder calibration clear failed: ' + prettyJson(res.json));
                    }
                })
                .catch(err => aboutStatus('Recorder calibration clear error: ' + err));
        }

        function supportClearInstallationCalibration() {
            const code = prompt('Support code required to clear installation calibration');
            if (!code) return;
            aboutStatus('Clearing installation calibration...');
            postJson('/api/install/support_clear?support=' + encodeURIComponent(code))
                .then(res => {
                    if (res.ok && res.json && res.json.cleared) {
                        aboutStatus('Installation calibration cleared. Installation calibration is required.');
                        calStatus();
                    } else {
                        aboutStatus('Installation calibration clear failed: ' + prettyJson(res.json));
                    }
                })
                .catch(err => aboutStatus('Installation calibration clear error: ' + err));
        }


        function logInstall(text) {
            const el = document.getElementById('installLog');
            if (!el) return;
            el.classList.remove('hidden');
            el.textContent = text;
        }

        function fmtAngle(v) {
            if (v === null || v === undefined || !isFinite(Number(v))) return '-';
            return Number(v).toFixed(2) + '°';
        }

        function installAnglesFromMatrix(matrix) {
            if (!Array.isArray(matrix) || matrix.length < 9) return null;
            const m00 = Number(matrix[0]);
            const m11 = Number(matrix[4]);
            const m12 = Number(matrix[5]);
            const m20 = Number(matrix[6]);
            if (!isFinite(m00) || !isFinite(m11) || !isFinite(m12) || !isFinite(m20)) return null;

            // Display the pitch/roll correction represented by the actual
            // installation matrix, not the raw accelerometer attitude formula.
            // The installation matrix is the rotation applied to recorder data
            // to align measured gravity with aircraft +Z.  With the convention
            // R = Ry(pitch) * Rx(roll):
            //   pitch = atan2(-R20, R00)
            //   roll  = atan2(-R12, R11)
            // This gives pitch = -90 deg and roll = 0 deg when gravity is
            // aligned with recorder +X, matching the applied minimal rotation.
            const rad2deg = 180.0 / Math.PI;
            const pitch = Math.atan2(-m20, m00) * rad2deg;
            const roll = Math.atan2(-m12, m11) * rad2deg;
            return { pitch: pitch, roll: roll };
        }

        function updateInstallAngles(data) {
            const candidate = data.candidate_valid ? installAnglesFromMatrix(data.matrix) : null;
            const stored = data.stored_valid ? installAnglesFromMatrix(data.stored_matrix) : null;
            document.getElementById('installPitchCandidate').textContent = candidate ? fmtAngle(candidate.pitch) : '-';
            document.getElementById('installRollCandidate').textContent = candidate ? fmtAngle(candidate.roll) : '-';
            document.getElementById('installPitchStored').textContent = stored ? fmtAngle(stored.pitch) : '-';
            document.getElementById('installRollStored').textContent = stored ? fmtAngle(stored.roll) : '-';
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
            if (installSampleRequestActive) return;
            installSampleRequestActive = true;
            fetch('/api/install/sample')
                .then(r => r.json().then(j => ({ok:r.ok, status:r.status, json:j})))
                .then(res => {
                    handleCalibrationAuthError(res);
                    const data = res.json;
                    if (!data.ok) return;
                    document.getElementById('installTotalSamples').textContent = data.total_samples || 0;
                    document.getElementById('installBestNoise').textContent = data.candidate_valid ? fmtNoise(data.quality_mg) : '-';
                    document.getElementById('installStability').textContent = data.candidate_valid ? 'stable' : 'not stable';
                    document.getElementById('installStatus').textContent = data.stored_valid ? ('valid since ' + fmtDate(data.stored_date)) : 'missing';
                    updateInstallAngles(data);
                    setInstallStartButton(!!data.active, false);
                    const reasonEl = document.getElementById('installReason');
                    const reasonOk = !!(reasonEl && reasonEl.value);
                    document.getElementById('btnInstallSave').disabled = (!data.candidate_valid) || (!reasonOk);
                })
                .catch(err => logInstall('Installation calibration communication error: ' + err))
                .then(() => { installSampleRequestActive = false; });
        }

        function installStart() {
            const start = document.getElementById('btnInstallStart');
            if (start && start.dataset.active === '1') {
                installCancel();
                return;
            }
            setInstallStartButton(false, true);
            document.getElementById('btnInstallSave').disabled = true;
            postJson('/api/install/start')
                .then(res => {
                    if (!res.ok || !res.json || !res.json.ok) {
                        setInstallStartButton(false, false);
                        const reason = res.json ? res.json.reason : '';
                        if (reason === 'sensor_cal_required') {
                            logInstall('Recorder calibration is required before installation calibration.');
                        } else if (reason === 'recording') {
                            logInstall('Installation calibration cannot start while recording is active.');
                        } else if (reason) {
                            logInstall('Installation calibration could not start: ' + reason + '.');
                        } else {
                            logInstall('Installation calibration could not start.');
                        }
                        calStatus();
                        installSample();
                        return;
                    }
                    setInstallStartButton(true, false);
                    logInstall('Installation calibration started. Put the glider in flight-level attitude with wings level and keep it still.');
                    calStatus();
                    installSample();
                    startCalPolling();
                })
                .catch(err => {
                    setInstallStartButton(false, false);
                    logInstall('Installation calibration start error: ' + err);
                });
        }

        function installCancel() {
            postJson('/api/install/cancel')
                .then(res => {
                    setInstallStartButton(false, false);
                    logInstall('Installation calibration cancelled.');
                    calStatus();
                    installSample();
                });
        }

        function installDownloadLastReport() {
            window.location = '/api/cal/report/installation/latest';
        }

        function installSave() {
            const save = document.getElementById('btnInstallSave');
            const reasonEl = document.getElementById('installReason');
            const reason = reasonEl ? reasonEl.value : '';
            if (!reason) {
                logInstall('Select an installation calibration reason before saving.');
                return;
            }
            save.disabled = true;
            postJson('/api/install/save?reason=' + encodeURIComponent(reason))
                .then(res => {
                    if (res.ok && res.json && res.json.matrix) {
                        logInstall('Installation calibration saved. Calibration report downloading.');
                        if (res.json && res.json.report_written) setTimeout(installDownloadLastReport, 250);
                    } else {
                        logInstall('Installation calibration save failed. Check that the glider is stable, wings are level, the selected reason is correct, and retry.');
                    }
                    calStatus();
                    installSample();
                    if (!res.ok) save.disabled = false;
                })
                .catch(err => {
                    logInstall('Installation calibration save error: ' + err);
                    save.disabled = false;
                });
        }

)rawliteral"
