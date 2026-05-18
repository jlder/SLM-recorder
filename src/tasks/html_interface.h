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
            <h2>Accelerometer Calibration</h2>
            <div class="card">
                <p><b>Workflow:</b> Click Start, then slowly rotate the recorder through all six faces. When a stable face is detected, the current session value is stored automatically for that face. Leave the recorder still on a face to improve the stddev for that current-session value, then review and save.</p>
                <p class="small">The backend detects the face from the dominant stable acceleration axis. For each face, the current session keeps the stable value with the lowest stddev. Stored values are shown only for comparison. Recording remains disabled until calibration is valid.</p>
            </div>

            <div class="controls">
                <button class="btn btn-success" id="btnStart" onclick="calStart()">Start</button>
                <button class="btn btn-success" id="btnSave" onclick="calSave()">Save</button>
                <button class="btn btn-danger" id="btnCancel" onclick="calCancel()">Cancel</button>
            </div>

            <div class="card">
                <div>Status: <span id="calStatus" class="mono">-</span></div>
                <div>Session: <span id="calSession" class="mono">-</span></div>
                <div class="candidate">Current / last updated face: <span id="calCandidate" class="warn">none</span></div>
            </div>

            <table class="cal-table">
                <thead>
                    <tr>
                        <th>Face</th>
                        <th>Status</th>
                        <th>Value</th>
                        <th>NVS value</th>
                        <th>Stddev</th>
                        <th>NVS stddev</th>
                    </tr>
                </thead>
                <tbody>
                    <tr id="faceRow0"><td>+X</td><td id="faceStatus0">not captured</td><td id="faceMean0">-</td><td id="storedMean0">-</td><td id="faceStd0">-</td><td id="storedStd0">-</td></tr>
                    <tr id="faceRow1"><td>-X</td><td id="faceStatus1">not captured</td><td id="faceMean1">-</td><td id="storedMean1">-</td><td id="faceStd1">-</td><td id="storedStd1">-</td></tr>
                    <tr id="faceRow2"><td>+Y</td><td id="faceStatus2">not captured</td><td id="faceMean2">-</td><td id="storedMean2">-</td><td id="faceStd2">-</td><td id="storedStd2">-</td></tr>
                    <tr id="faceRow3"><td>-Y</td><td id="faceStatus3">not captured</td><td id="faceMean3">-</td><td id="storedMean3">-</td><td id="faceStd3">-</td><td id="storedStd3">-</td></tr>
                    <tr id="faceRow4"><td>+Z</td><td id="faceStatus4">not captured</td><td id="faceMean4">-</td><td id="storedMean4">-</td><td id="faceStd4">-</td><td id="storedStd4">-</td></tr>
                    <tr id="faceRow5"><td>-Z</td><td id="faceStatus5">not captured</td><td id="faceMean5">-</td><td id="storedMean5">-</td><td id="faceStd5">-</td><td id="storedStd5">-</td></tr>
                </tbody>
            </table>

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
    </div>

    <script>
        let allFiles = [];
        let calPoll = null;

        function showTab(name) {
            const files = document.getElementById('filesSection');
            const cal = document.getElementById('calSection');
            document.getElementById('tabFilesBtn').classList.remove('active');
            document.getElementById('tabCalBtn').classList.remove('active');

            if (name === 'cal') {
                files.classList.add('hidden');
                cal.classList.remove('hidden');
                document.getElementById('tabCalBtn').classList.add('active');
                calStatus();
                startCalPolling();
            } else {
                cal.classList.add('hidden');
                files.classList.remove('hidden');
                document.getElementById('tabFilesBtn').classList.add('active');
                stopCalPolling();
                refreshFiles();
            }
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

        function postJson(url) {
            return fetch(url, { method: 'POST' }).then(r => r.json().then(j => ({ok:r.ok, status:r.status, json:j})));
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
                    document.getElementById('calTopStatus').textContent = data.status;
                    document.getElementById('calStatus').textContent = data.status + ', recording_allowed=' + data.recording_allowed;
                    document.getElementById('calSession').textContent = data.session_active ? 'active' : 'inactive';
                })
                .catch(err => console.error(err));
        }

        function startCalPolling() {
            if (calPoll) return;
            calPoll = setInterval(calSample, 500);
            calSample();
        }

        function stopCalPolling() {
            if (calPoll) {
                clearInterval(calPoll);
                calPoll = null;
            }
        }

        function faceAxisValue(faceIndex, vec) {
            if (!vec) return null;
            if (faceIndex === 0 || faceIndex === 1) return vec.x;
            if (faceIndex === 2 || faceIndex === 3) return vec.y;
            return vec.z;
        }

        function fmtFaceValue(faceIndex, vec) {
            const v = faceAxisValue(faceIndex, vec);
            return (v === null || v === undefined) ? '-' : v.toFixed(1);
        }

        function faceIndexFromName(name) {
            const names = ['+X','-X','+Y','-Y','+Z','-Z'];
            return names.indexOf(name);
        }

        function updateFaceTable(sessionFace, storedFace, activeFaceName, activeValid) {
            const activeIndex = activeValid ? faceIndexFromName(activeFaceName) : -1;

            for (let i = 0; i < 6; i++) {
                const sf = sessionFace && sessionFace[i] ? sessionFace[i] : null;
                const st = storedFace && storedFace[i] ? storedFace[i] : null;
                const row = document.getElementById('faceRow' + i);

                if (row) {
                    if (i === activeIndex) row.classList.add('face-active');
                    else row.classList.remove('face-active');
                }

                document.getElementById('faceStatus' + i).innerHTML =
                    (sf && sf.valid) ? '<span class="ok">captured</span>' : '<span class="warn">not captured</span>';

                document.getElementById('faceMean' + i).innerHTML =
                    (sf && sf.valid) ? '<span class="ok">' + fmtFaceValue(i, sf.mean) + '</span>' : '-';
                document.getElementById('faceStd' + i).innerHTML =
                    (sf && sf.valid) ? '<span class="ok">' + fmtFaceValue(i, sf.stddev) + '</span>' : '-';

                document.getElementById('storedMean' + i).textContent =
                    (st && st.valid) ? fmtFaceValue(i, st.mean) : '-';
                document.getElementById('storedStd' + i).textContent =
                    (st && st.valid) ? fmtFaceValue(i, st.stddev) : '-';
            }
        }

        function fmtDate(d) {
            if (!d || !d.year) return '-';
            const mm = String(d.month).padStart(2, '0');
            const dd = String(d.day).padStart(2, '0');
            const hh = String(d.hour).padStart(2, '0');
            const mi = String(d.min).padStart(2, '0');
            const ss = String(d.sec).padStart(2, '0');
            return d.year + '-' + mm + '-' + dd + ' ' + hh + ':' + mi + ':' + ss;
        }

        function updateResultTable(data) {
            let workflow = 'Ready';
            if (data.active && data.done) workflow = 'Done';
            else if (data.active) workflow = 'Active';
            document.getElementById('calWorkflowStatus').textContent = workflow;
            document.getElementById('nvsDate').textContent = data.nvs_result_available ? fmtDate(data.nvs_date) : '-';

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
                .then(r => r.json())
                .then(data => {
                    if (!data.ok) {
                        logCal(JSON.stringify(data, null, 2));
                        return;
                    }

                    const cand = document.getElementById('calCandidate');
                    if (data.candidate_valid) {
                        cand.innerHTML = '<span class="ok">' + data.candidate_face + ' updated</span>';
                    } else if (data.active) {
                        cand.innerHTML = '<span class="warn">waiting for stable face</span>';
                    } else {
                        cand.textContent = 'none';
                    }

                    document.getElementById('calSession').textContent = data.active ? 'active' : 'inactive';

                    updateFaceTable(data.session_face, data.stored_face, data.candidate_face, data.candidate_valid);
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

        setInterval(updateStatus, 5000);
        setInterval(calStatus, 5000);

        refreshFiles();
    </script>
</body>
</html>
)rawliteral";

#endif // HTML_INTERFACE_H
