// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/tasks/html_interface.h
 * @brief Embedded HTML, CSS, and JavaScript for the recorder web interface.
 *
 * @details Documentation is intentionally concise and interface-oriented so
 * the source can support future DO-178C planning artifacts.
 */

/*******************************************************************************
 * EMBEDDED HTML INTERFACE
 *
 * Web-based file manager and rough calibration interface served by the ESP32.
 ******************************************************************************/

#ifndef HTML_INTERFACE_H
#define HTML_INTERFACE_H

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SLM Recorder</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 10px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.1);
            overflow: hidden;
        }
        .header {
            background: #2c3e50;
            color: white;
            padding: 20px;
            text-align: center;
        }
        .tabs {
            background: #34495e;
            padding: 10px;
            text-align: center;
        }
        .tab-btn {
            padding: 10px 18px;
            margin: 3px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-weight: bold;
            background: #ecf0f1;
            color: #2c3e50;
        }
        .tab-btn.active { background: #3498db; color: white; }
        .info-bar {
            background: #34495e;
            color: white;
            padding: 15px 20px;
            display: flex;
            justify-content: space-around;
            flex-wrap: wrap;
        }
        .info-item { font-size: 14px; padding: 4px; }
        .info-value { font-weight: bold; margin-left: 5px; }
        .controls {
            padding: 20px;
            background: #ecf0f1;
            text-align: center;
        }
        .section { padding: 20px; }
        .hidden { display: none; }
        .btn {
            padding: 10px 20px;
            margin: 5px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 14px;
            font-weight: bold;
        }
        .btn-primary { background: #3498db; color: white; }
        .btn-danger { background: #e74c3c; color: white; }
        .btn-warning { background: #f39c12; color: white; }
        .btn-success { background: #27ae60; color: white; }
        .btn:disabled { background: #bdc3c7; cursor: not-allowed; }
        .file-table, .cal-table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 10px;
        }
        .file-table th, .cal-table th {
            background: #2c3e50;
            color: white;
            padding: 12px;
            text-align: left;
        }
        .file-table td, .cal-table td {
            padding: 12px;
            border-bottom: 1px solid #ecf0f1;
        }
        .file-table tr:hover, .cal-table tr:hover { background: #f8f9fa; }
        .loading { text-align: center; padding: 40px; color: #7f8c8d; }
        .status-recording { color: #2ecc71; font-weight: bold; }
        .card {
            background: #f8f9fa;
            border: 1px solid #dfe6e9;
            border-radius: 8px;
            padding: 15px;
            margin: 10px 0;
        }
        .ok { color: #27ae60; font-weight: bold; }
        .warn { color: #f39c12; font-weight: bold; }
        .bad { color: #e74c3c; font-weight: bold; }
        .mono { font-family: Consolas, monospace; }
        .small { font-size: 13px; color: #555; }
        .face-active { background: #d6eaff !important; }
        .candidate {
            font-size: 18px;
            margin: 10px 0;
        }
        pre {
            white-space: pre-wrap;
            background: #2c3e50;
            color: #ecf0f1;
            padding: 10px;
            border-radius: 5px;
            max-height: 180px;
            overflow: auto;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>SLM Recorder</h1>
            <span id="statusBadge"></span>
        </div>

        <div class="tabs">
            <button id="tabFilesBtn" class="tab-btn active" onclick="showTab('files')">Files</button>
            <button id="tabCalBtn" class="tab-btn" onclick="showTab('cal')">Calibration</button>
            <button id="tabOtaBtn" class="tab-btn" onclick="showTab('ota')">Firmware Update</button>
        </div>

        <div class="info-bar">
            <span class="info-item">SD Card: <span class="info-value" id="sdSize">-</span></span>
            <span class="info-item">Free: <span class="info-value" id="sdFree">-</span></span>
            <span class="info-item">Files: <span class="info-value" id="fileCount">-</span></span>
            <span class="info-item">Battery: <span class="info-value" id="battery">-</span></span>
            <span class="info-item">Calibration: <span class="info-value" id="calTopStatus">-</span></span>
        </div>

        <div id="filesSection">
            <div class="controls">
                <button class="btn btn-primary" onclick="refreshFiles()">Refresh</button>
            </div>
            <div id="fileListContainer">
                <div class="loading">Loading files...</div>
            </div>
        </div>

        <div id="calSection" class="hidden section">
            <div id="calAuthPanel" class="card">
                <h2>Calibration Access</h2>
                <p><b>Calibration is a maintenance activity.</b> Enter the recorder registration to unlock accelerometer and installation calibration.</p>
                <input type="password" id="calPassword" placeholder="Registration" class="mono" style="padding:10px; margin:5px;">
                <button class="btn btn-warning" onclick="calUnlock()">Unlock Calibration</button>
                <div id="calAuthStatus" class="small">Locked.</div>
            </div>

            <div id="calMenuPanel" class="hidden">
                <h2>Calibration Menu</h2>
                <div class="card">
                    <button class="btn btn-primary" onclick="openAccelCal()">Accelerometer Calibration</button>
                    <span class="small">Last calibration: <span id="sensorCalDate" class="mono">-</span></span>
                </div>
                <div class="card">
                    <button class="btn btn-primary" onclick="openInstallCal()">Installation Calibration</button>
                    <span class="small">Last calibration: <span id="installationCalDate" class="mono">-</span></span>
                </div>
            </div>

            <div id="accelCalPage" class="hidden">
                <button class="btn btn-primary" onclick="showCalMenu()">Back to Calibration Menu</button>
                <h2>Accelerometer Calibration</h2>
            <div class="card">
                <p><b>Workflow:</b> Start calibration, place the recorder still on each of its six faces, and wait for each face to show OK. The recorder automatically keeps the best capture for each face. It is good practice to leave the recorder on a given face until the last best update is more than 10 seconds old. Save calibration when all six faces values are satisfactory.</p>
            </div>

            <div class="controls">
                <button class="btn btn-success" id="btnStart" onclick="calStart()">Start</button>
                <button class="btn btn-success" id="btnSave" onclick="calSave()">Save</button>
                <button class="btn btn-danger" id="btnCancel" onclick="calCancel()">Cancel</button>
            </div>

            <div class="card">
                <div>Status: <span id="calStatus" class="mono">-</span></div>
                <div>Session: <span id="calSession" class="mono">-</span></div>
                <div>Current face: <span id="calCurrentFace" class="mono">-</span></div>
                <div>Samples processed: <span id="calSamplesProcessed" class="mono">0</span></div>
                <div>Lowest stddev: <span id="calLowestNoise" class="mono">-</span></div>
                <div>Updates: <span id="calTotalUpdates" class="mono">0</span></div>
                <div>Last best update: <span id="calLastBestUpdate" class="mono">-</span></div>
                <div>Faces: <span id="calFaceSummary" class="mono">+X — | -X — | +Y — | -Y — | +Z — | -Z —</span></div>
                <div class="candidate"><span id="calCandidate" class="warn">Start calibration and place the recorder on a face.</span></div>
            </div>

            <div class="card">
                <b>Calibration:</b> <span id="calWorkflowStatus" class="mono">Ready</span>
                <div>NVS date: <span id="nvsDate" class="mono">-</span></div>
                <table class="cal-table">
                    <thead>
                        <tr>
                            <th>Axis</th>
                            <th>Gain</th>
                            <th>NVS Gain</th>
                            <th>Offset</th>
                            <th>NVS Offset</th>
                        </tr>
                    </thead>
                    <tbody>
                        <tr><td>X</td><td id="resGain0">-</td><td id="nvsGain0">-</td><td id="resOffset0">-</td><td id="nvsOffset0">-</td></tr>
                        <tr><td>Y</td><td id="resGain1">-</td><td id="nvsGain1">-</td><td id="resOffset1">-</td><td id="nvsOffset1">-</td></tr>
                        <tr><td>Z</td><td id="resGain2">-</td><td id="nvsGain2">-</td><td id="resOffset2">-</td><td id="nvsOffset2">-</td></tr>
                    </tbody>
                </table>
                <pre id="calLog" class="hidden">Ready.</pre>
            </div>

            </div>

            <div id="installCalPage" class="hidden">
                <button class="btn btn-primary" onclick="showCalMenu()">Back to Calibration Menu</button>
                <h2>Installation Calibration</h2>
            <div class="card">
                <p><b>Workflow:</b> Put the glider in its flight-level attitude with wings leveled, following the AMM procedure. Sensor calibration must already be valid before attempting installation calibration. Click Start, leave the glider still. It is good practice to wait until the last best update is more than 10 seconds old. Save calibration when noise is satisfactory.</p>
            </div>
            <div class="controls">
                <button class="btn btn-success" id="btnInstallStart" onclick="installStart()">Start Installation</button>
                <button class="btn btn-success" id="btnInstallSave" onclick="installSave()">Save Installation</button>
                <button class="btn btn-danger" id="btnInstallCancel" onclick="installCancel()">Cancel Installation</button>
            </div>
            <div class="card">
                <div>Status: <span id="installStatus" class="mono">-</span></div>
                <div>Session: <span id="installSession" class="mono">-</span></div>
                <div>Samples processed: <span id="installTotalSamples" class="mono">0</span></div>
                <div>Lowest noise: <span id="installBestNoise" class="mono">-</span></div>
                <div>Updates: <span id="installUpdates" class="mono">0</span></div>
                <div>Last best update: <span id="installLastUpdate" class="mono">-</span></div>
                <pre id="installMatrix">Matrix will appear after a stable candidate is found.</pre>
            </div>
            </div>
        </div>

        <div id="otaSection" class="hidden section">
            <h2>Firmware Update</h2>
            <div class="card">
                <p><b>USB power is required for firmware update.</b></p>
                <p class="small">Upload only the Arduino application <span class="mono">.bin</span> file, typically <span class="mono">SLM_recorder.ino.bin</span>. Do not upload the merged binary.</p>
                <p class="small">The recorder will restart automatically after a successful update.</p>
            </div>

            <div class="card">
                <input type="file" id="otaFile" accept=".bin">
                <button class="btn btn-warning" id="btnOtaUpload" onclick="otaUpload()">Upload Firmware</button>
                <div id="otaStatus" class="small">Ready.</div>
            </div>
        </div>
    </div>

    <script>
        let allFiles = [];
        let calPoll = null;
        let calAuth = false;
        let calView = 'menu';
        let calLastBestLocalMs = 0;
        let calLastBestLocalUpdates = 0;
        let calLastBestLocalFace = -1;

        function showTab(name) {
            const files = document.getElementById('filesSection');
            const cal = document.getElementById('calSection');
            const ota = document.getElementById('otaSection');
            document.getElementById('tabFilesBtn').classList.remove('active');
            document.getElementById('tabCalBtn').classList.remove('active');
            document.getElementById('tabOtaBtn').classList.remove('active');

            if (name === 'cal') {
                files.classList.add('hidden');
                ota.classList.add('hidden');
                cal.classList.remove('hidden');
                document.getElementById('tabCalBtn').classList.add('active');
                calStatus();
                if (calAuth) showCalMenu();
                else showCalLocked();
            } else if (name === 'ota') {
                files.classList.add('hidden');
                cal.classList.add('hidden');
                ota.classList.remove('hidden');
                document.getElementById('tabOtaBtn').classList.add('active');
                stopCalPolling();
            } else {
                cal.classList.add('hidden');
                ota.classList.add('hidden');
                files.classList.remove('hidden');
                document.getElementById('tabFilesBtn').classList.add('active');
                stopCalPolling();
                refreshFiles();
            }
        }

        function showCalLocked() {
            calView = 'locked';
            stopCalPolling();
            document.getElementById('calAuthPanel').classList.remove('hidden');
            document.getElementById('calMenuPanel').classList.add('hidden');
            document.getElementById('accelCalPage').classList.add('hidden');
            document.getElementById('installCalPage').classList.add('hidden');
            resetAccelCalUi();
            resetInstallCalUi();
        }

        function showCalMenu() {
            calView = 'menu';
            stopCalPolling();
            document.getElementById('calAuthPanel').classList.add('hidden');
            document.getElementById('calMenuPanel').classList.remove('hidden');
            document.getElementById('accelCalPage').classList.add('hidden');
            document.getElementById('installCalPage').classList.add('hidden');
            resetAccelCalUi();
            resetInstallCalUi();
            calStatus();
        }

        function openAccelCal() {
            calView = 'accel';
            stopCalPolling();
            resetAccelCalUi();
            resetInstallCalUi();
            document.getElementById('calAuthPanel').classList.add('hidden');
            document.getElementById('calMenuPanel').classList.add('hidden');
            document.getElementById('accelCalPage').classList.remove('hidden');
            document.getElementById('installCalPage').classList.add('hidden');
            calStatus();
            startCalPolling();
            calSample();
        }

        function openInstallCal() {
            calView = 'install';
            stopCalPolling();
            resetAccelCalUi();
            resetInstallCalUi();
            document.getElementById('calAuthPanel').classList.add('hidden');
            document.getElementById('calMenuPanel').classList.add('hidden');
            document.getElementById('accelCalPage').classList.add('hidden');
            document.getElementById('installCalPage').classList.remove('hidden');
            calStatus();
            startCalPolling();
            installSample();
        }

        function calUnlock() {
            const password = document.getElementById('calPassword').value || '';
            const status = document.getElementById('calAuthStatus');
            status.textContent = 'Checking...';
            fetch('/api/cal/auth?password=' + encodeURIComponent(password), { method: 'POST' })
                .then(r => r.json().then(j => ({ok:r.ok, json:j})))
                .then(res => {
                    if (res.ok && res.json && res.json.ok) {
                        calAuth = true;
                        status.textContent = 'Unlocked.';
                        showCalMenu();
                    } else {
                        calAuth = false;
                        status.textContent = 'Wrong registration or calibration access denied.';
                    }
                })
                .catch(err => {
                    calAuth = false;
                    status.textContent = 'Unlock error: ' + err;
                });
        }

        function logCal(text) {
            document.getElementById('calLog').textContent = text;
        }

        function prettyJson(obj) {
            return JSON.stringify(obj, null, 2);
        }

        function setCalSavedUi(saved) {
            const save = document.getElementById('btnSave');
            const cand = document.getElementById('calCandidate');

            if (saved) {
                save.disabled = true;
                cand.innerHTML = '<span class="ok">calibration saved</span>';
                document.getElementById('calWorkflowStatus').textContent = 'Done';
                stopCalPolling();
            } else {
                save.disabled = false;
            }
        }

        function handleCalibrationAuthError(res) {
            if (res && res.status === 403 && res.json && res.json.reason === 'calibration_auth_required') {
                calAuth = false;
                showCalLocked();
            }
        }

        function postJson(url) {
            return fetch(url, { method: 'POST' }).then(r => r.json().then(j => {
                const res = {ok:r.ok, status:r.status, json:j};
                handleCalibrationAuthError(res);
                return res;
            }));
        }

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

        function displayFiles(files) {
            const container = document.getElementById('fileListContainer');
            document.getElementById('fileCount').textContent = files.length;

            if (files.length === 0) {
                container.innerHTML = '<div class="loading">No files found</div>';
                return;
            }

            let html = '<table class="file-table"><thead><tr>';
            html += '<th>Filename</th><th>Size kB</th><th>Actions</th>';
            html += '</tr></thead><tbody>';

            files.forEach(file => {
                const sizeKb = Math.round((file.size || 0) / 1000);
                html += `<tr>
                    <td>${file.name}</td>
                    <td>${sizeKb}</td>
                    <td>
                        <button class="btn btn-primary" onclick="downloadFile('${file.name}')">Download</button>
                        <button class="btn btn-danger" onclick="deleteFile('${file.name}')">Delete</button>
                    </td>
                </tr>`;
            });

            html += '</tbody></table>';
            container.innerHTML = html;
        }

        function downloadFile(filename) {
            window.location.href = '/api/download?file=' + encodeURIComponent(filename);
        }

        function deleteFile(filename) {
            if (!confirm('Delete ' + filename + '?')) return;
            fetch('/api/delete?file=' + encodeURIComponent(filename), { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    console.log('Delete response:', data);
                    refreshFiles();
                })
                .catch(err => console.error('Delete error:', err));
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

        function calStatus() {
            fetch('/api/cal/status')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('calTopStatus').textContent = data.recording_allowed ? 'ready' : (data.sensor_valid ? 'install required' : data.status);
                    const sensorDateText = data.sensor_valid ? fmtDate(data.sensor_date) : '-';
                    const installDateText = data.installation_valid ? fmtDate(data.installation_date) : '-';
                    document.getElementById('sensorCalDate').textContent = sensorDateText;
                    document.getElementById('installationCalDate').textContent = installDateText;
                    if (calView === 'accel') document.getElementById('nvsDate').textContent = sensorDateText;
                    const accelStatus = data.sensor_valid ? ('valid since ' + sensorDateText) : data.status;
                    const installStatus = data.installation_valid ? ('valid since ' + installDateText) : 'missing';
                    document.getElementById('calStatus').textContent = accelStatus;
                    document.getElementById('calSession').textContent = data.session_active ? 'sampling' : 'not started';
                    document.getElementById('installStatus').textContent = installStatus;
                    document.getElementById('installSession').textContent = data.installation_session_active ? 'sampling' : 'not started';
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
            document.getElementById('calSession').textContent = '-';
            document.getElementById('calCurrentFace').textContent = '-';
            document.getElementById('calSamplesProcessed').textContent = '0';
            document.getElementById('calLowestNoise').textContent = '-';
            document.getElementById('calTotalUpdates').textContent = '0';
            document.getElementById('calLastBestUpdate').textContent = '-';
            document.getElementById('calFaceSummary').innerHTML = '+X — | -X — | +Y — | -Y — | +Z — | -Z —';
            calLastBestLocalMs = 0;
            calLastBestLocalUpdates = 0;
            calLastBestLocalFace = -1;
            document.getElementById('calCandidate').textContent = 'Start calibration and place the recorder on a face.';
            document.getElementById('calWorkflowStatus').textContent = 'Ready';
            document.getElementById('nvsDate').textContent = '-';
            for (let i = 0; i < 3; i++) {
                document.getElementById('resGain' + i).textContent = '-';
                document.getElementById('resOffset' + i).textContent = '-';
                document.getElementById('nvsGain' + i).textContent = '-';
                document.getElementById('nvsOffset' + i).textContent = '-';
            }
            document.getElementById('btnSave').disabled = true;
            logCal('Ready.');
        }

        function resetInstallCalUi() {
            document.getElementById('installStatus').textContent = '-';
            document.getElementById('installSession').textContent = '-';
            document.getElementById('installTotalSamples').textContent = '0';
            document.getElementById('installBestNoise').textContent = '-';
            document.getElementById('installUpdates').textContent = '0';
            document.getElementById('installLastUpdate').textContent = '-';
            document.getElementById('installMatrix').textContent = 'Matrix will appear after a stable candidate is found.';
            document.getElementById('btnInstallSave').disabled = true;
        }

        function faceIndexFromName(name) {
            const names = ['+X','-X','+Y','-Y','+Z','-Z'];
            return names.indexOf(name);
        }

        function fmtDate(dt) {
            if (!dt || !dt.year || !dt.month || !dt.day) return '-';
            const pad2 = function(v) { return String(v).padStart(2, '0'); };
            let out = String(dt.year) + '-' + pad2(dt.month) + '-' + pad2(dt.day);
            if (dt.hour !== undefined && dt.min !== undefined && dt.sec !== undefined) {
                out += ' ' + pad2(dt.hour) + ':' + pad2(dt.min) + ':' + pad2(dt.sec);
            }
            return out;
        }

        function fmtNoise(v) {
            if (v === null || v === undefined || !isFinite(Number(v)) || Number(v) > 900000) return '-';
            return Number(v).toFixed(2) + ' mg';
        }

        function updateFaceSummary(faceValid, activeIdx) {
            const names = ['+X', '-X', '+Y', '-Y', '+Z', '-Z'];
            const parts = [];
            for (let i = 0; i < names.length; i++) {
                const ok = !!(faceValid && faceValid[i]);
                const text = names[i] + ' ' + (ok ? 'OK' : '—');
                if (ok) {
                    parts.push('<span class="ok">' + text + '</span>');
                } else if (i === activeIdx) {
                    parts.push('<span class="warn">' + text + '</span>');
                } else {
                    parts.push('<span>' + text + '</span>');
                }
            }
            document.getElementById('calFaceSummary').innerHTML = parts.join(' | ');
        }

        function updateResultTable(data) {
            let workflow = 'Ready';
            if (data.active && data.done) workflow = 'Done';
            else if (data.active) workflow = 'Active';
            document.getElementById('calWorkflowStatus').textContent = workflow;
            document.getElementById('nvsDate').textContent = data.nvs_result_available ? fmtDate(data.nvs_date) : '-';

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

                    const cand = document.getElementById('calCandidate');
                    if (data.active && data.candidate_valid) {
                        cand.innerHTML = '<span class="ok">Sampling ' + data.candidate_face + '. Leave still to improve, or rotate to another face.</span>';
                    } else if (data.active) {
                        cand.innerHTML = '<span class="warn">Waiting for a stable face.</span>';
                    } else {
                        cand.textContent = 'Start calibration and place the recorder on a face.';
                    }

                    document.getElementById('calSession').textContent = data.active ? 'sampling' : 'not started';
                    const currentFaceName = data.current_face_valid ? data.current_face : (data.candidate_valid ? data.candidate_face : '-');
                    document.getElementById('calCurrentFace').textContent = currentFaceName;
                    document.getElementById('calSamplesProcessed').textContent = data.samples || 0;
                    const activeIdx = currentFaceName !== '-' ? faceIndexFromName(currentFaceName) : -1;
                    const activeUpdates = (activeIdx >= 0 && data.face_updates) ? (data.face_updates[activeIdx] || 0) : 0;
                    const activeAge = (activeIdx >= 0 && data.face_last_update_age_ms) ? data.face_last_update_age_ms[activeIdx] : null;
                    document.getElementById('calTotalUpdates').textContent = activeUpdates;
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

                    updateFaceSummary(data.face_valid, activeIdx);
                    updateResultTable(data);
                })
                .catch(err => logCal('sample error: ' + err));
        }

        function calStart() {
            setCalSavedUi(false);
            postJson('/api/cal/start')
                .then(res => {
                    logCal(prettyJson(res.json));
                    calStatus();
                    startCalPolling();
                });
        }

        function calAccept() {
            postJson('/api/cal/accept')
                .then(res => {
                    logCal(prettyJson(res.json));
                    calSample();
                });
        }

        function calCancel() {
            postJson('/api/cal/cancel')
                .then(res => {
                    setCalSavedUi(false);
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

        function fmtVec(v) {
            if (!v) return '-';
            return 'x=' + Number(v.x || 0).toFixed(1) + ', y=' + Number(v.y || 0).toFixed(1) + ', z=' + Number(v.z || 0).toFixed(1);
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
                    document.getElementById('installSession').textContent = data.active ? 'sampling' : 'not started';
                    document.getElementById('installTotalSamples').textContent = data.total_samples || 0;
                    document.getElementById('installBestNoise').textContent = data.candidate_valid ? fmtNoise(data.quality_mg) : '-';
                    document.getElementById('installUpdates').textContent = data.update_count || 0;
                    document.getElementById('installLastUpdate').textContent = (data.update_count || 0) > 0 ? fmtAge(data.last_update_age_ms) : '-';
                    document.getElementById('installStatus').textContent = data.stored_valid ? ('valid since ' + fmtDate(data.stored_date)) : 'missing';
                    if (data.candidate_valid) {
                        document.getElementById('installMatrix').textContent = fmtMatrix(data.matrix);
                    } else if (data.stored_valid) {
                        document.getElementById('installMatrix').textContent = fmtMatrix(data.stored_matrix);
                    } else {
                        document.getElementById('installMatrix').textContent = 'Waiting for stable level-flight attitude.';
                    }
                    document.getElementById('btnInstallSave').disabled = !data.candidate_valid;
                })
                .catch(err => console.error(err));
        }

        function installStart() {
            document.getElementById('btnInstallSave').disabled = true;
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

        function otaUpload() {
            const fileInput = document.getElementById('otaFile');
            const status = document.getElementById('otaStatus');
            const button = document.getElementById('btnOtaUpload');

            if (!fileInput.files || fileInput.files.length === 0) {
                status.textContent = 'Select a firmware .bin file first.';
                return;
            }

            const file = fileInput.files[0];
            if (!file.name.endsWith('.bin') || file.name.indexOf('merged') >= 0) {
                status.textContent = 'Use the application .bin file, not the merged binary.';
                return;
            }

            const form = new FormData();
            form.append('firmware', file, file.name);

            button.disabled = true;
            status.textContent = 'Uploading firmware: 0%. Do not disconnect USB power.';

            const xhr = new XMLHttpRequest();

            xhr.upload.onprogress = function(evt) {
                if (evt.lengthComputable) {
                    const pct = Math.round((evt.loaded * 100) / evt.total);
                    status.textContent = 'Uploading firmware: ' + pct + '%. Do not disconnect USB power.';
                } else {
                    status.textContent = 'Uploading firmware. Do not disconnect USB power.';
                }
            };

            xhr.onload = function() {
                status.textContent = xhr.responseText || 'Upload complete.';
                if (xhr.status < 200 || xhr.status >= 300) {
                    button.disabled = false;
                }
            };

            xhr.onerror = function() {
                status.textContent = 'Firmware update failed.';
                button.disabled = false;
            };

            xhr.open('POST', '/api/ota');
            xhr.send(form);
        }

        setInterval(updateStatus, 5000);
        setInterval(calStatus, 5000);

        refreshFiles();
    </script>
</body>
</html>
)rawliteral";

#endif // HTML_INTERFACE_H
