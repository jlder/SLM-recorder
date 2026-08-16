/*******************************************************************************
 * SCRIPT: CALIBRATION WORKFLOWS
 * JavaScript for recorder sensor calibration and installation calibration polling, status display, save, and cancel operations.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        function calibrationStatusText(code) {
            const map = { valid:'VALID', expired:'EXPIRED', fault:'FAULT', missing:'MISSING' };
            return map[code] ? tr(map[code]) : tr('UNKNOWN');
        }

        function calibrationFaultText(code) {
            const map = { plausibility:'PLAUSIBILITY', delta:'DIFFERENCE', none:'NONE' };
            return map[code] ? tr(map[code]) : tr('UNKNOWN');
        }

        // Global calibration status refresh. This drives menu dates, amber action cues,
        // top status text, and stored-calibration status fields.
        function calStatus() {
            fetch('/api/cal/status')
                .then(r => r.json())
                .then(data => {
                    const sensorDateText = data.sensor_valid ? fmtDate(data.sensor_date) : '-';
                    const installDateText = data.installation_valid ? fmtDate(data.installation_date) : '-';
                    const sensorParts = data.sensor_valid ? fmtDateParts(data.sensor_date) : {date:'-', time:'-'};
                    const installParts = data.installation_valid ? fmtDateParts(data.installation_date) : {date:'-', time:'-'};
                    document.getElementById('sensorCalDate').textContent = sensorParts.date;
                    document.getElementById('sensorCalTime').textContent = sensorParts.time;
                    document.getElementById('installationCalDate').textContent = installParts.date;
                    document.getElementById('installationCalTime').textContent = installParts.time;
                    updateMaintenanceButtonState(data);
                    const faultText = (data.fault_reason && data.fault_reason !== 'none') ? (' ' + tr('FAULT_PREFIX') + ' ' + calibrationFaultText(data.fault_reason)) : '';
                    let accelStatus = data.sensor_valid ? (tr('VALID_SINCE') + ' ' + sensorDateText) : (calibrationStatusText(data.status) + faultText);
                    if (!data.sensor_valid && data.reference_available && !data.active_available) {
                        accelStatus = tr('REPEAT_REC_CAL_REQUIRED');
                    } else if (!data.sensor_valid && !data.reference_available) {
                        accelStatus = tr('FIRST_REC_CAL_REQUIRED');
                    }
                    const installStatus = data.installation_valid ? (tr('VALID_SINCE') + ' ' + installDateText) : tr('MISSING');
                    if (!(calView === 'accel' && calRecorderSaveMessageSticky)) {
                        document.getElementById('calStatus').textContent = accelStatus;
                    }
                    document.getElementById('installStatus').textContent = installStatus;
                })
                .catch(err => console.error(err));
        }

        // High-rate polling is enabled only while a live calibration page is visible.
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

        // Reset recorder-calibration controls when leaving/entering the page.
        function resetAccelCalUi() {
            clearAccelCalMeasurementUi();
            calRecorderSaveMessageSticky = false;
            setRecorderStartButton(false, false);
            document.getElementById('btnSave').disabled = true;
            logCal(tr('READY_DOT'));
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
            logInstall(tr('READY_DOT'));
            setInstallStartButton(false, false);
            document.getElementById('btnInstallSave').disabled = true;
        }

        // Recorder-calibration display helpers.
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

        function stddevForFace(stddev, faceName) {
            if (!stddev || !faceName || faceName === '-') return null;
            if (faceName.indexOf('X') >= 0) return stddev.x;
            if (faceName.indexOf('Y') >= 0) return stddev.y;
            if (faceName.indexOf('Z') >= 0) return stddev.z;
            return null;
        }

        function fmtNoiseLimit(v, limit) {
            const text = fmtNoise(v);
            if (text === '-') return '-';
            const ok = isFinite(Number(limit)) && (Number(v) <= Number(limit));
            return '<span class="' + (ok ? 'ok' : 'bad') + '">' + text + '</span>';
        }


        function fmtTemperature(v) {
            if (v === null || v === undefined || !isFinite(Number(v))) return '-';
            return Number(v).toFixed(1) + ' °C';
        }

        // Display the recorder-calibration temperature gate. A high absolute
        // temperature or excessive session temperature span cancels the session
        // in the service; in that case the normal temperature readout is
        // replaced by a direct restart instruction.
        function updateCalibrationTemperature(data) {
            const el = document.getElementById('calSensorTemp');
            if (!!data.temperature_fault) {
                el.innerHTML = '<span class="bad">' + tr('TEMP_TOO_HIGH_RESTART') + '</span>';
                return;
            }

            const available = !!data.temperature_available;
            const stable = !!data.temperature_stable;
            const minAllowed = data.temperature_allowed_min_c;
            const maxAllowed = data.temperature_allowed_max_c;
            const spanAllowed = data.temperature_allowed_span_c;
            let status = tr('OK');
            if (!available) {
                status = tr('UNAVAILABLE');
            } else if (minAllowed !== undefined && Number(data.temperature_c) < Number(minAllowed)) {
                status = tr('TOO_LOW');
            } else if (maxAllowed !== undefined && Number(data.temperature_c) > Number(maxAllowed)) {
                status = tr('TOO_HIGH');
            } else if (!stable) {
                status = tr('WAIT_STABLE');
            }

            let text = available ? (fmtTemperature(data.temperature_c) + ' ' + status) : status;
            if (available && !stable && spanAllowed !== undefined) {
                text += ' (' + tr('SPAN_MAX') + ' ' + Number(spanAllowed).toFixed(1) + ' °C)';
            }
            el.textContent = text;
        }

        // Render the six recorder faces with active and completed state.
        function updateFaceSummary(faceValid, activeIdx, active) {
            const names = ['+X', '-X', '+Y', '-Y', '+Z', '-Z'];
            const parts = [];
            for (let i = 0; i < names.length; i++) {
                const ok = !!(active && faceValid && faceValid[i]);
                const isActive = !!active && (i === activeIdx);
                const result = ok ? tr('OK') : '—';
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

        // Update candidate and stored recorder-calibration gain/offset values.
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

        // Poll the live recorder-calibration session. The service keeps best captures per face.
        // Current/min stddev are both active-axis values. When the per-face best
        // update counter changes, the current field is forced to the same
        // accepted-window value for that refresh so the operator does not see a
        // best update that appears unrelated to the displayed current value.
        function calSample() {
            fetch('/api/cal/sample')
                .then(r => r.json().then(j => ({ok:r.ok, status:r.status, json:j})))
                .then(res => {
                    handleCalibrationAuthError(res);
                    const data = res.json;
                    if (!data.ok) {
                        logCal(tr('REC_CAL_STATUS_UNAVAILABLE'));
                        return;
                    }

                    const currentFaceName = data.current_face_valid ? data.current_face : (data.candidate_valid ? data.candidate_face : '-');
                    document.getElementById('calSamplesProcessed').textContent = data.samples || 0;
                    const activeIdx = currentFaceName !== '-' ? faceIndexFromName(currentFaceName) : -1;
                    const activeUpdates = (activeIdx >= 0 && data.face_updates) ? Number(data.face_updates[activeIdx] || 0) : 0;
                    const bestNoise = (activeIdx >= 0 && activeUpdates > 0 && data.face_quality) ? Number(data.face_quality[activeIdx]) : null;
                    const bestUpdateCount = activeUpdates;
                    const bestUpdatedSinceLastPoll = (activeIdx >= 0 && bestUpdateCount > 0 && bestUpdateCount !== calFaceBestUpdateSeen[activeIdx]);
                    if (activeIdx >= 0) calFaceBestUpdateSeen[activeIdx] = bestUpdateCount;

                    let currentNoise = data.current_stddev_valid ? stddevForFace(data.current_stddev, currentFaceName) : null;
                    if (bestUpdatedSinceLastPoll && bestNoise !== null && isFinite(bestNoise)) {
                        currentNoise = bestNoise;
                    }
                    document.getElementById('calCurrentNoise').innerHTML = fmtNoiseLimit(currentNoise, data.stability_stddev_max_mg);
                    document.getElementById('calLowestNoise').textContent = (bestNoise !== null && isFinite(bestNoise)) ? fmtNoise(bestNoise) : '-';

                    // Show how long the retained minimum has been stable on
                    // the current face. This is more useful than the total time on
                    // the face because it tells the operator whether the best
                    // capture is still improving.
                    const bestAgeMs = (activeIdx >= 0 && bestUpdateCount > 0 && data.face_last_update_age_ms) ?
                        Number(data.face_last_update_age_ms[activeIdx] || 0) : null;
                    document.getElementById('calFaceTime').textContent =
                        (bestAgeMs !== null && isFinite(bestAgeMs)) ? fmtAge(bestAgeMs) : '-';
                    updateCalibrationTemperature(data);

                    updateFaceSummary(data.face_valid, activeIdx, !!data.active);
                    updateResultTable(data);

                    const temperatureOk = !!data.temperature_available && !!data.temperature_in_range && !!data.temperature_stable;
                    if (data.active && data.result_available && temperatureOk && !calRecorderSaveMessageSticky) {
                        logCal(tr('VALID_CAL_CONTINUE'));
                    }
                })
                .catch(err => logCal(localizedError('REC_CAL_COMM_ERROR', err)));
        }

        // Start or cancel recorder calibration depending on the current Start/Cancel button state.
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
                        logCal(tr('REC_CAL_COULD_NOT_START'));
                        calStatus();
                        calSample();
                        return;
                    }
                    setRecorderStartButton(true, false);
                    logCal(tr('REC_CAL_STARTED'));
                    calStatus();
                    startCalPolling();
                })
                .catch(err => {
                    setRecorderStartButton(false, false);
                    logCal(localizedError('REC_CAL_START_ERROR', err));
                });
        }


        function calCancel() {
            calRecorderSaveMessageSticky = false;
            postJson('/api/cal/cancel')
                .then(res => {
                    setCalSavedUi(false);
                    setRecorderStartButton(false, false);
                    logCal(tr('REC_CAL_CANCELLED'));
                    calStatus();
                    calSample();
                });
        }

        // Save the recorder calibration result. The server decides whether this is a first pass,
        // confirmed pass, drift rejection, or failure and returns the matching reason.
        function calSave() {
            const save = document.getElementById('btnSave');
            save.disabled = true;
            calRecorderSaveMessageSticky = true;

            postJson('/api/cal/save')
                .then(res => {
                    if (res.ok && res.json && res.json.saved) {
                        logCal(tr('REC_CAL_SAVED'));
                        setCalSavedUi(true);
                        clearAccelCalMeasurementUi();
                        calStatus();
                        if (res.json && res.json.report_written) setTimeout(calDownloadLastReport, 1000);
                        return;
                    }

                    if (res.ok && res.json && res.json.reason === 'repeat_required') {
                        save.disabled = true;
                        logCal(tr('FIRST_REC_CAL_SAVED'));
                        setCalSavedUi(true);
                        clearAccelCalMeasurementUi();
                        calStatus();
                        if (res.json && res.json.report_written) setTimeout(calDownloadLastReport, 1000);
                        return;
                    }

                    if (res.json && res.json.reason === 'delta_fault') {
                        logCal(tr('CAL_DRIFT_MESSAGE'));
                        const status = document.getElementById('calStatus');
                        if (status) status.textContent = tr('CAL_DRIFT_TITLE');
                        setCalSavedUi(true);
                        clearAccelCalMeasurementUi();
                        calStatus();
                        if (res.json && res.json.report_written) setTimeout(calDownloadLastReport, 1000);
                        return;
                    }

                    save.disabled = false;
                    calRecorderSaveMessageSticky = false;
                    if (res.json && (res.json.reason === 'temperature_out_of_range' || res.json.reason === 'temperature_unavailable' || res.json.reason === 'temperature_unstable')) {
                        logCal(tr('REC_CAL_TEMP_NOT_ACCEPTABLE'));
                    } else {
                        logCal(tr('REC_CAL_SAVE_FAILED'));
                    }
                    calStatus();
                    if (res.json && res.json.report_written) setTimeout(calDownloadLastReport, 1000);
                    calSample();
                })
                .catch(err => {
                    save.disabled = false;
                    calRecorderSaveMessageSticky = false;
                    logCal(localizedError('REC_CAL_SAVE_ERROR', err));
                });
        }

        // Support actions share the support code entered once on the About access panel.
        function aboutStatus(text) {
            const el = document.getElementById('aboutSupportStatus');
            if (el) el.textContent = text;
        }

        function requireAboutAuthorization() {
            if (aboutAuth && aboutSupportCode) return true;
            aboutAuth = false;
            aboutSupportCode = '';
            openHealthPage();
            return false;
        }

        function handleAboutAuthorizationError(res) {
            if (res && res.status === 403 && res.json &&
                res.json.reason === 'bad_support_code') {
                aboutAuth = false;
                aboutSupportCode = '';
                openHealthPage();
                const status = document.getElementById('aboutAuthStatus');
                if (status) status.textContent = tr('SUPPORT_AUTH_EXPIRED');
                return true;
            }
            return false;
        }

        function supportDownloadData() {
            if (!requireAboutAuthorization()) return;
            window.location = '/api/cal/export?support=' +
                encodeURIComponent(aboutSupportCode);
        }

        function calDownloadLastReport() {
            window.location = '/api/cal/report/recorder/latest';
        }



        function supportVerifyRecordings() {
            if (!requireAboutAuthorization()) return;
            const button = document.getElementById('btnVerifyRecordings');
            if (button) button.disabled = true;
            aboutStatus(tr('VERIFYING_RECORDINGS'));
            postJson('/api/cal/support_verify_recordings?support=' +
                encodeURIComponent(aboutSupportCode))
                .then(res => {
                    if (handleAboutAuthorizationError(res)) return;
                    if (!res.ok || !res.json || !res.json.ok) {
                        alert(tr('RECORDING_VERIFY_FAILED'));
                        aboutStatus(tr('RECORDING_VERIFY_FAILED_PREFIX') + ': ' + apiReasonText(res.json && res.json.reason));
                        return;
                    }
                    const j = res.json;
                    let text = tr('FILES_CHECKED') + ': ' + j.files_checked + '\n' +
                               tr('FILES_VALID') + ': ' + j.files_valid + '\n' +
                               tr('LEGACY_FILES') + ': ' + j.legacy_files + '\n' +
                               tr('ERRORS') + ': ' + (j.metadata_errors + j.sha_mismatches);
                    if (j.first_error_file) text += '\n\n' + tr('FIRST_ERROR') + ': ' + j.first_error_file;
                    alert(text);
                    aboutStatus(text.replace(/\n/g, ' '));
                })
                .catch(err => {
                    alert(tr('RECORDING_VERIFY_ERROR') + '.');
                    aboutStatus(localizedError('RECORDING_VERIFY_ERROR', err));
                })
                .finally(() => { if (button) button.disabled = false; });
        }

        function supportGenerateCalibrationReports() {
            if (!requireAboutAuthorization()) return;
            aboutStatus(tr('GENERATING_CAL_REPORTS'));
            postJson('/api/cal/support_generate_reports?support=' +
                encodeURIComponent(aboutSupportCode))
                .then(res => {
                    if (handleAboutAuthorizationError(res)) return;
                    if (res.ok && res.json) {
                        const parts = [];
                        const queued = [];
                        if (res.json.recorder_written) parts.push('recorder');
                        if (res.json.installation_written) parts.push('installation');
                        if (res.json.recorder_written &&
                            queueGeneratedCalibrationReport(res.json.recorder_path)) {
                            queued.push('recorder');
                        }
                        if (res.json.installation_written &&
                            queueGeneratedCalibrationReport(res.json.installation_path)) {
                            queued.push('installation');
                        }
                        if (parts.length > 0) {
                            let text = (parts.length === 2) ? tr('GENERATED_CAL_REPORTS') :
                                       (parts[0] === 'recorder' ? tr('GENERATED_REC_REPORT') : tr('GENERATED_INSTALL_REPORT'));
                            if (queued.length > 0) {
                                text += ' ' + ((queued.length === 2) ? tr('QUEUED_CAL_REPORTS') :
                                    (queued[0] === 'recorder' ? tr('QUEUED_REC_REPORT') : tr('QUEUED_INSTALL_REPORT')));
                            } else {
                                text += ' ' + tr('CONNECT_ANDROID_UPLOAD');
                            }
                            if (res.json.write_failed) text += ' ' + tr('REPORT_WRITE_ONE_FAILED');
                            aboutStatus(text);
                            calStatus();
                        } else {
                            aboutStatus(tr('NO_CAL_REPORT_GENERATED') + ': ' + apiReasonText(res.json && res.json.reason));
                        }
                    } else {
                        aboutStatus(tr('CAL_REPORT_GENERATION_FAILED') + ': ' + apiReasonText(res.json && res.json.reason));
                    }
                })
                .catch(err => aboutStatus(localizedError('CAL_REPORT_GENERATION_ERROR', err)));
        }

        function supportRestoreInstallationCalibration() {
            if (!requireAboutAuthorization()) return;
            if (!confirm(tr('RESTORE_INSTALL_CONFIRM'))) return;

            const button = document.getElementById('btnRestoreInstallationCalibration');
            if (button) button.disabled = true;
            aboutStatus(tr('SEARCHING_INSTALL_CAL'));

            postJson('/api/install/support_restore?support=' +
                encodeURIComponent(aboutSupportCode))
                .then(res => {
                    if (handleAboutAuthorizationError(res)) return;
                    if (res.ok && res.json && res.json.restored) {
                        const source = res.json.source || tr('INSTALL_REPORT_NAME');
                        const when = res.json.calibration_time || '-';
                        aboutStatus(tr('INSTALL_RESTORED_FROM') + ' ' + source + '. ' + tr('RECORDER_RESTARTING'));
                        alert(tr('INSTALL_RESTORED') + '\n\n' + tr('SOURCE_LABEL') + ': ' + source +
                              '\n' + tr('CAL_DATE_TIME_LABEL') + ': ' + when +
                              '\n\n' + tr('REPLACEMENT_REC_CAL_REQUIRED'));
                    } else {
                        const reason = (res.json && res.json.reason) ? res.json.reason : 'restore_failed';
                        const reasonText = apiReasonText(reason, 'INSTALL_RESTORE_FAILED');
                        aboutStatus(tr('INSTALL_RESTORE_FAILED') + ' ' + reasonText);
                        alert(tr('INSTALL_RESTORE_FAILED') + '\n\n' + reasonText);
                        if (button) button.disabled = false;
                    }
                })
                .catch(err => {
                    aboutStatus(localizedError('INSTALL_RESTORE_ERROR', err));
                    alert(tr('INSTALL_RESTORE_ERROR'));
                    if (button) button.disabled = false;
                });
        }

        function queueGeneratedCalibrationReport(path) {
            if (!path || !window.SLMAndroid ||
                typeof window.SLMAndroid.queueCalibrationReport !== 'function') return false;
            try {
                window.SLMAndroid.queueCalibrationReport(String(path));
                return true;
            } catch (err) {
                console.error('Calibration report queue error', err);
                return false;
            }
        }

        function supportClearRecorderCalibration() {
            if (!requireAboutAuthorization()) return;
            if (!confirm(tr('CLEAR_REC_CONFIRM'))) return;
            aboutStatus(tr('CLEARING_REC_CAL'));
            postJson('/api/cal/support_clear?support=' +
                encodeURIComponent(aboutSupportCode))
                .then(res => {
                    if (handleAboutAuthorizationError(res)) return;
                    if (res.ok && res.json && res.json.cleared) {
                        aboutStatus(tr('REC_CAL_CLEARED'));
                        calStatus();
                    } else {
                        aboutStatus(tr('REC_CAL_CLEAR_FAILED') + ': ' + apiReasonText(res.json && res.json.reason));
                    }
                })
                .catch(err => aboutStatus(localizedError('REC_CAL_CLEAR_ERROR', err)));
        }

        function supportClearInstallationCalibration() {
            if (!requireAboutAuthorization()) return;
            if (!confirm(tr('CLEAR_INSTALL_CONFIRM'))) return;
            aboutStatus(tr('CLEARING_INSTALL_CAL'));
            postJson('/api/install/support_clear?support=' +
                encodeURIComponent(aboutSupportCode))
                .then(res => {
                    if (handleAboutAuthorizationError(res)) return;
                    if (res.ok && res.json && res.json.cleared) {
                        aboutStatus(tr('INSTALL_CAL_CLEARED'));
                        calStatus();
                    } else {
                        aboutStatus(tr('INSTALL_CAL_CLEAR_FAILED') + ': ' + apiReasonText(res.json && res.json.reason));
                    }
                })
                .catch(err => aboutStatus(localizedError('INSTALL_CAL_CLEAR_ERROR', err)));
        }


        // Installation-calibration display and action helpers.
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

        // Convert an installation rotation matrix into pitch/roll correction angles for display.
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

        // Display both the current stable candidate and the stored NVS installation calibration.
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
            const sec = Math.max(0, Math.round(ms / 1000));
            if (sec < 60) return sec + ' s';
            const min = Math.floor(sec / 60);
            const rem = sec % 60;
            return min + ' min ' + rem + ' s';
        }



        // Poll the live installation session. Only the current stable rolling window is saveable.
        // Overlapping requests are skipped so an older response cannot overwrite newer state.
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
                    document.getElementById('installStability').textContent = data.candidate_valid ? tr('STABLE') : tr('NOT_STABLE');
                    document.getElementById('installStatus').textContent = data.stored_valid ? (tr('VALID_SINCE') + ' ' + fmtDate(data.stored_date)) : tr('MISSING');
                    updateInstallAngles(data);
                    setInstallStartButton(!!data.active, false);
                    const reasonEl = document.getElementById('installReason');
                    const reasonOk = !!(reasonEl && reasonEl.value);
                    document.getElementById('btnInstallSave').disabled = (!data.candidate_valid) || (!reasonOk);
                })
                .catch(err => logInstall(localizedError('INSTALL_COMM_ERROR', err)))
                .then(() => { installSampleRequestActive = false; });
        }

        // Start or cancel installation calibration depending on the current Start/Cancel button state.
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
                            logInstall(tr('INSTALL_SENSOR_CAL_REQUIRED'));
                        } else if (reason === 'recording') {
                            logInstall(tr('INSTALL_RECORDING_BLOCK'));
                        } else if (reason) {
                            logInstall(tr('INSTALL_COULD_NOT_START') + ' ' + apiReasonText(reason));
                        } else {
                            logInstall(tr('INSTALL_COULD_NOT_START'));
                        }
                        calStatus();
                        installSample();
                        return;
                    }
                    setInstallStartButton(true, false);
                    logInstall(tr('INSTALL_STARTED'));
                    calStatus();
                    installSample();
                    startCalPolling();
                })
                .catch(err => {
                    setInstallStartButton(false, false);
                    logInstall(localizedError('INSTALL_START_ERROR', err));
                });
        }

        function installCancel() {
            postJson('/api/install/cancel')
                .then(res => {
                    setInstallStartButton(false, false);
                    logInstall(tr('INSTALL_CANCELLED'));
                    calStatus();
                    installSample();
                });
        }

        function installDownloadLastReport() {
            window.location = '/api/cal/report/installation/latest';
        }

        // Save the current stable installation candidate with the selected maintenance reason.
        function installSave() {
            const save = document.getElementById('btnInstallSave');
            const reasonEl = document.getElementById('installReason');
            const reason = reasonEl ? reasonEl.value : '';
            if (!reason) {
                logInstall(tr('INSTALL_REASON_REQUIRED'));
                return;
            }
            save.disabled = true;
            postJson('/api/install/save?reason=' + encodeURIComponent(reason))
                .then(res => {
                    if (res.ok && res.json && res.json.matrix) {
                        logInstall(tr('INSTALL_SAVED'));
                        if (res.json && res.json.report_written) setTimeout(installDownloadLastReport, 250);
                    } else {
                        logInstall(tr('INSTALL_SAVE_FAILED'));
                    }
                    calStatus();
                    installSample();
                    if (!res.ok) save.disabled = false;
                })
                .catch(err => {
                    logInstall(localizedError('INSTALL_SAVE_ERROR', err));
                    save.disabled = false;
                });
        }

)rawliteral"
