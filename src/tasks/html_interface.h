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
            padding: 10px;
            font-size: 16px;
        }
        .container {
            max-width: 1000px;
            margin: 0 auto;
            background: white;
            border-radius: 8px;
            box-shadow: 0 8px 28px rgba(0,0,0,0.12);
            overflow: hidden;
        }
        .header {
            background: #2c3e50;
            color: white;
            padding: 12px;
            text-align: center;
        }
        .header h1 { font-size: 20px; line-height: 1.2; }
        .tabs {
            background: #34495e;
            padding: 6px;
            display: flex;
            gap: 6px;
            justify-content: center;
            flex-wrap: wrap;
        }
        .tab-btn {
            padding: 8px 10px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-weight: bold;
            background: #ecf0f1;
            color: #2c3e50;
            min-height: 38px;
        }
        .tab-btn.active { background: #3498db; color: white; }
        .info-bar {
            background: #34495e;
            color: white;
            padding: 8px 10px;
            display: flex;
            justify-content: space-around;
            flex-wrap: wrap;
            gap: 4px 10px;
        }
        .info-item { font-size: 13px; padding: 2px; }
        .info-value { font-weight: bold; margin-left: 4px; }
        .controls {
            padding: 10px;
            background: #ecf0f1;
            text-align: center;
        }
        .section { padding: 12px; }
        h2 { font-size: 18px; margin: 2px 0 8px; }
        p { margin: 6px 0; line-height: 1.35; }
        .hidden { display: none; }
        .btn {
            padding: 7px 10px;
            margin: 2px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 13px;
            font-weight: bold;
            min-height: 34px;
        }
        .btn-primary { background: #3498db; color: white; }
        .btn-danger { background: #e74c3c; color: white; }
        .btn-warning { background: #f39c12; color: white; }
        .btn-success { background: #27ae60; color: white; }
        .btn:disabled { background: #bdc3c7; color: #666; cursor: not-allowed; }
        .delete-warning-window {
            border: 1px solid #f1c40f;
            background: #fff8e1;
            color: #5d4600;
            border-radius: 8px;
            padding: 10px 12px;
            margin: 8px 0;
        }
        .delete-warning-window {
            border: 1px solid #f1c40f;
            background: #fff8e1;
            color: #5d4600;
            border-radius: 8px;
            padding: 10px 12px;
            margin: 8px 0;
        }
        .btn-return { background: #27ae60; color: white; }
        input[type="password"], input[type="file"] {
            max-width: 100%;
            min-height: 40px;
            font-size: 16px;
        }
        .file-list { margin-top: 8px; }
        .file-item {
            border-bottom: 1px solid #ecf0f1;
            padding: 8px 10px;
        }
        .file-item:hover { background: #f8f9fa; }
        .file-row {
            display: grid;
            grid-template-columns: minmax(0, 1fr) auto;
            gap: 8px;
            align-items: center;
            min-height: 34px;
        }
        .file-name {
            font-family: Consolas, monospace;
            font-size: 14px;
            overflow-wrap: anywhere;
        }
        .file-size { font-size: 13px; color: #555; }
        .analysis-window {
            border: 1px solid #3498db;
            background: #eef7ff;
            border-radius: 8px;
            padding: 10px;
            margin: 10px 0;
        }
        .analysis-window h2 { margin-bottom: 6px; }
        .progress-block {
            margin: 8px 0;
        }
        .progress-label {
            font-size: 12px;
            margin-bottom: 3px;
        }
        .progress-track {
            width: 100%;
            height: 14px;
            border: 1px solid #95a5a6;
            border-radius: 7px;
            overflow: hidden;
            background: #ecf0f1;
        }
        .progress-fill {
            height: 100%;
            width: 0%;
            background: #3498db;
            transition: width 0.15s linear;
        }
        .analysis-table {
            width: 100%;
            border-collapse: collapse;
            table-layout: fixed;
            margin-top: 8px;
        }
        .analysis-table th {
            background: #2c3e50;
            color: white;
            padding: 5px 2px;
            text-align: center;
            font-size: 11px;
            line-height: 1.05;
        }
        .analysis-table td {
            padding: 6px 2px;
            border-bottom: 1px solid #dfe6e9;
            font-size: 11px;
            text-align: center;
            white-space: nowrap;
        }
        .file-btn {
            width: 96px;
            min-height: 32px;
            padding: 6px 8px;
            margin: 1px 0;
            font-size: 13px;
        }
        .delete-file-row {
            display: grid;
            grid-template-columns: auto minmax(0, 1fr) auto;
            gap: 8px;
            align-items: center;
            min-height: 36px;
        }
        .delete-file-row input[type="checkbox"] {
            width: 22px;
            height: 22px;
        }
        .cal-table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 8px;
        }
        .cal-table th {
            background: #2c3e50;
            color: white;
            padding: 8px;
            text-align: left;
            font-size: 13px;
        }
        .cal-table td {
            padding: 8px;
            border-bottom: 1px solid #ecf0f1;
            font-size: 13px;
        }
        .loading { text-align: center; padding: 24px; color: #7f8c8d; }
        .status-recording { color: #2ecc71; font-weight: bold; }
        .card {
            background: #f8f9fa;
            border: 1px solid #dfe6e9;
            border-radius: 8px;
            padding: 10px;
            margin: 8px 0;
        }
        .ok { color: #27ae60; font-weight: bold; }
        .warn { color: #f39c12; font-weight: bold; }
        .bad { color: #e74c3c; font-weight: bold; }
        .mono { font-family: Consolas, monospace; }
        .small { font-size: 13px; color: #555; }
        .face-summary-grid {
            display: grid;
            grid-template-columns: repeat(3, minmax(58px, 1fr));
            gap: 6px;
            margin-top: 4px;
        }
        .face-chip {
            display: flex;
            justify-content: space-between;
            align-items: center;
            gap: 5px;
            padding: 5px 7px;
            border: 1px solid #dfe6e9;
            border-radius: 5px;
            background: #fff;
            color: #111;
            font-weight: bold;
        }
        .face-chip-warn { color: #f39c12; border-color: #f39c12; }
        .face-chip-ok { color: #27ae60; border-color: #27ae60; }
        .face-chip-active { border-width: 4px; padding: 2px 4px; }

        .section-title-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            gap: 8px;
            padding-bottom: 0;
        }
        .return-btn { min-width: 82px; }
        .button-grid {
            display: grid;
            grid-template-columns: repeat(2, minmax(120px, 180px));
            gap: 8px;
            margin: 8px 0;
            justify-content: center;
        }
        .main-menu-grid .btn, .maintenance-grid .btn {
            min-height: 42px;
        }
        .cal-menu-item { margin-bottom: 10px; text-align: center; }
        .cal-menu-item .small { margin-top: 3px; }
        .last-cal-heading {
            text-align: center;
            font-weight: bold;
            color: #2c3e50;
            margin: 5px 0 2px;
            line-height: 1.15;
        }
        .last-cal-date {
            text-align: center;
            line-height: 1.2;
        }
        .cal-actions {
            display: grid;
            grid-template-columns: repeat(2, minmax(100px, 140px));
            gap: 6px;
            justify-content: center;
            text-align: initial;
            margin: 8px auto;
        }
        .cal-actions .btn, .button-grid .btn {
            width: 100%;
            margin: 0;
        }
        .workflow-card { font-size: 13px; }
        .workflow-card p { line-height: 1.3; }
        .candidate {
            font-size: 16px;
            margin: 8px 0;
        }
        pre {
            white-space: pre-wrap;
            background: #2c3e50;
            color: #ecf0f1;
            padding: 8px;
            border-radius: 5px;
            max-height: 160px;
            overflow: auto;
            font-size: 13px;
        }
        @media (max-width: 600px) {
            body { padding: 6px; font-size: 15px; }
            .container { border-radius: 6px; }
            .header { padding: 10px 8px; }
            .header h1 { font-size: 18px; }
            .tab-btn { flex: 1 1 30%; padding: 8px 6px; font-size: 13px; }
            .section { padding: 10px; }
            .controls { padding: 8px; }
            .controls .btn, .card > .btn { width: 100%; }
            .cal-actions .btn, .button-grid .btn { width: 100%; }
            .return-btn { width: auto !important; }
            .card { padding: 9px; }
            .file-item { padding: 7px 8px; }
            .file-name { font-size: 13px; }
            .file-btn { width: 88px; font-size: 12px; }
            .cal-table { display: block; overflow-x: auto; white-space: nowrap; }
            .face-summary-grid { grid-template-columns: repeat(2, minmax(58px, 1fr)); }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1 id="pageTitle">SLM Recorder</h1>
            <span id="statusBadge"></span>
        </div>

        <div id="homeSection" class="section">
            <h2>Main Menu</h2>
            <div class="button-grid main-menu-grid">
                <button class="btn btn-primary" onclick="openFilesPage()">File Management</button>
                <button class="btn btn-primary" id="btnHomeMaintenance" onclick="openMaintenancePage()">Maintenance</button>
            </div>
        </div>

        <div id="filesSection" class="hidden">
            <div class="info-bar">
                <span class="info-item">SD Card: <span class="info-value" id="sdSize">-</span></span>
                <span class="info-item">Free: <span class="info-value" id="sdFree">-</span></span>
                <span class="info-item">Files: <span class="info-value" id="fileCount">-</span></span>
                <span class="info-item">Battery: <span class="info-value" id="battery">-</span></span>
                <span class="info-item">Calibration: <span class="info-value" id="calTopStatus">-</span></span>
            </div>

            <div class="controls compact-controls cal-actions">
                <button class="btn btn-primary" onclick="refreshFiles()">Refresh</button>
                <button class="btn btn-return" onclick="showHome()">Return</button>
            </div>
            <div id="fileListContainer">
                <div class="loading">Loading files...</div>
            </div>
            <div id="flightAnalysisWindow" class="analysis-window hidden">
                <h2>SLM Flight Analysis</h2>
                <div id="flightAnalysisStatus" class="small">No analysis available.</div>
                <div id="samplePeriodStats" class="small mono">Sample period: -</div>
                <div class="progress-block">
                    <div id="downloadProgressText" class="progress-label small">Download: idle</div>
                    <div class="progress-track"><div id="downloadProgressBar" class="progress-fill"></div></div>
                </div>
                <div id="flightAnalysisTable"></div>
            </div>
        </div>

        <div id="maintenanceSection" class="hidden section">
            <div id="calAuthPanel" class="card">
                <h2>Maintenance Access</h2>
                <p><b>Maintenance is restricted.</b> Enter the recorder registration to unlock calibration and firmware update functions.</p>
                <input type="password" id="calPassword" placeholder="Registration" class="mono" style="padding:10px; margin:5px;">
                <div class="button-grid two-button-grid">
                    <button class="btn btn-primary" id="btnMaintenanceUnlock" onclick="calUnlock()">Unlock Maintenance</button>
                    <button class="btn btn-return" onclick="showHome()">Return</button>
                </div>
                <div id="calAuthStatus" class="small">Locked.</div>
            </div>

            <div id="calMenuPanel" class="hidden">
                <div class="button-grid maintenance-grid">
                    <div class="cal-menu-item">
                        <button class="btn btn-primary" id="btnMenuRecorderCal" onclick="openAccelCal()">Recorder Calibration</button>
                        <div class="last-cal-heading"><div>Last Recorder</div><div>Calibration</div></div>
                        <div class="small mono last-cal-date" id="sensorCalDate">-</div>
                        <div class="small mono last-cal-date" id="sensorCalTime">-</div>
                    </div>
                    <div class="cal-menu-item">
                        <button class="btn btn-primary" id="btnMenuInstallCal" onclick="openInstallCal()">Installation Calibration</button>
                        <div class="last-cal-heading"><div>Last Installation</div><div>Calibration</div></div>
                        <div class="small mono last-cal-date" id="installationCalDate">-</div>
                        <div class="small mono last-cal-date" id="installationCalTime">-</div>
                    </div>
                    <button class="btn btn-primary" onclick="openHealthPage()">Health</button>
                    <button class="btn btn-primary" onclick="openOtaPage()">Firmware Update</button>
                    <button class="btn btn-primary" onclick="openDeletePage()">Delete</button>
                    <button class="btn btn-return" onclick="showHome()">Return</button>
                </div>
            </div>
        </div>

        <div id="accelCalPage" class="hidden section">
            <div class="card workflow-card">
                <p><b>Workflow:</b> Start calibration, place the recorder still on each of its six faces, and wait for each face to show OK. The recorder automatically keeps the best capture for each face. It is good practice to leave the recorder on a given face until the last best update is more than 10 seconds old. Save calibration when all six faces values are satisfactory.</p>
            </div>

            <div class="controls cal-actions">
                <button class="btn btn-primary" id="btnStart" onclick="calStart()">Start</button>
                <button class="btn btn-primary" id="btnSave" onclick="calSave()">Save</button>
                <button class="btn btn-primary" id="btnCancel" onclick="calCancel()">Cancel</button>
                <button class="btn btn-return" onclick="showMaintenanceMenu()">Return</button>
            </div>

            <div class="card">
                <div>Status: <span id="calStatus" class="mono">-</span></div>
                <div>Samples processed: <span id="calSamplesProcessed" class="mono">0</span></div>
                <div>Lowest stddev: <span id="calLowestNoise" class="mono">-</span></div>
                <div>Last best update: <span id="calLastBestUpdate" class="mono">-</span></div>
                <div>Faces:</div>
                <div id="calFaceSummary" class="mono face-summary-grid"></div>
            </div>

            <div class="card">
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

        <div id="installCalPage" class="hidden section">
            <div class="card workflow-card">
                <p><b>Workflow:</b> Put the glider in its flight-level attitude with wings leveled, following the AMM procedure. Sensor calibration must already be valid before attempting installation calibration. Click Start, leave the glider still. It is good practice to wait until the last best update is more than 10 seconds old. Save calibration when noise is satisfactory.</p>
            </div>
            <div class="controls cal-actions">
                <button class="btn btn-primary" id="btnInstallStart" onclick="installStart()">Start</button>
                <button class="btn btn-primary" id="btnInstallSave" onclick="installSave()">Save</button>
                <button class="btn btn-primary" id="btnInstallCancel" onclick="installCancel()">Cancel</button>
                <button class="btn btn-return" onclick="showMaintenanceMenu()">Return</button>
            </div>
            <div class="card">
                <div>Status: <span id="installStatus" class="mono">-</span></div>
                <div>Samples processed: <span id="installTotalSamples" class="mono">0</span></div>
                <div>Lowest noise: <span id="installBestNoise" class="mono">-</span></div>
                <div>Last best update: <span id="installLastUpdate" class="mono">-</span></div>
                <pre id="installMatrix">Matrix will appear after a stable candidate is found.</pre>
            </div>
        </div>

        <div id="deleteSection" class="hidden section">
            <h2>SLM Delete</h2>
            <div class="controls cal-actions">
                <button class="btn btn-primary" onclick="deleteProcessedSelected()">Delete</button>
                <button class="btn btn-return" onclick="showMaintenanceMenu()">Return</button>
            </div>
            <div class="card workflow-card">
                <p>Select archived files from <span class="mono">/processed</span>, then press Delete.</p>
            </div>
            <div class="delete-warning-window">
                <b>Important:</b> deleted files are permanently lost and cannot be recovered by the recorder.
            </div>
            <div id="processedDeleteStatus" class="small">Loading...</div>
            <div id="processedFileListContainer">
                <div class="loading">Loading files...</div>
            </div>
        </div>

        <div id="healthSection" class="hidden section">
            <div id="watchdogDiagPanel" class="card">
                <h2>Watchdog Diagnostic</h2>
                <div id="watchdogDiagText" class="small mono">Loading...</div>
            </div>
            <div class="controls cal-actions">
                <button class="btn btn-primary" onclick="updateWatchdogDiag()">Refresh</button>
                <button class="btn btn-return" onclick="showMaintenanceMenu()">Return</button>
            </div>
        </div>

        <div id="otaSection" class="hidden section">
            <div class="card workflow-card">
                <p><b>USB power is required for firmware update.</b></p>
                <p class="small">Upload only the Arduino application <span class="mono">.bin</span> file, typically <span class="mono">SLM_recorder.ino.bin</span>. Do not upload the merged binary.</p>
                <p class="small">The recorder will restart automatically after a successful update.</p>
            </div>

            <div class="card">
                <input type="file" id="otaFile" accept=".bin">
                <div class="controls cal-actions">
                    <button class="btn btn-primary" id="btnOtaUpload" onclick="otaUpload()">Upload Firmware</button>
                    <button class="btn btn-return" onclick="showMaintenanceMenu()">Return</button>
                </div>
                <div id="otaStatus" class="small">Ready.</div>
            </div>
        </div>
    </div>

    <script>
        let allFiles = [];
        let processedFiles = [];
        let calPoll = null;
        let calAuth = false;
        let calView = 'menu';
        let calLastBestLocalMs = 0;
        let calLastBestLocalUpdates = 0;
        let calLastBestLocalFace = -1;

        function setPageTitle(title) {
            document.getElementById('pageTitle').textContent = title;
        }

        function hideAllPages() {
            document.getElementById('homeSection').classList.add('hidden');
            document.getElementById('filesSection').classList.add('hidden');
            document.getElementById('maintenanceSection').classList.add('hidden');
            document.getElementById('accelCalPage').classList.add('hidden');
            document.getElementById('installCalPage').classList.add('hidden');
            document.getElementById('healthSection').classList.add('hidden');
            document.getElementById('otaSection').classList.add('hidden');
            document.getElementById('deleteSection').classList.add('hidden');
        }

        function showHome() {
            setPageTitle('SLM Recorder');
            stopCalPolling();
            hideAllPages();
            document.getElementById('homeSection').classList.remove('hidden');
            updateStatus();
            calStatus();
        }

        function openFilesPage() {
            setPageTitle('SLM File Management');
            stopCalPolling();
            hideAllPages();
            document.getElementById('filesSection').classList.remove('hidden');
            refreshFiles();
        }

        function openMaintenancePage() {
            setPageTitle('SLM Maintenance');
            stopCalPolling();
            hideAllPages();
            document.getElementById('maintenanceSection').classList.remove('hidden');
            if (calAuth) showMaintenanceMenu();
            else showMaintenanceLocked();
        }

        function showMaintenanceLocked() {
            setPageTitle('SLM Maintenance');
            calView = 'locked';
            stopCalPolling();
            hideAllPages();
            document.getElementById('maintenanceSection').classList.remove('hidden');
            document.getElementById('calAuthPanel').classList.remove('hidden');
            document.getElementById('calMenuPanel').classList.add('hidden');
            resetAccelCalUi();
            resetInstallCalUi();
        }

        function showMaintenanceMenu() {
            setPageTitle('SLM Maintenance');
            calView = 'menu';
            stopCalPolling();
            hideAllPages();
            document.getElementById('maintenanceSection').classList.remove('hidden');
            document.getElementById('calAuthPanel').classList.add('hidden');
            document.getElementById('calMenuPanel').classList.remove('hidden');
            resetAccelCalUi();
            resetInstallCalUi();
            calStatus();
        }

        function openAccelCal() {
            setPageTitle('SLM Recorder Calibration');
            if (!calAuth) { showMaintenanceLocked(); return; }
            calView = 'accel';
            stopCalPolling();
            resetAccelCalUi();
            resetInstallCalUi();
            hideAllPages();
            document.getElementById('accelCalPage').classList.remove('hidden');
            calStatus();
            startCalPolling();
            calSample();
        }

        function openInstallCal() {
            setPageTitle('SLM Installation Calibration');
            if (!calAuth) { showMaintenanceLocked(); return; }
            calView = 'install';
            stopCalPolling();
            resetAccelCalUi();
            resetInstallCalUi();
            hideAllPages();
            document.getElementById('installCalPage').classList.remove('hidden');
            calStatus();
            startCalPolling();
            installSample();
        }

        function openHealthPage() {
            setPageTitle('SLM Health');
            if (!calAuth) { showMaintenanceLocked(); return; }
            stopCalPolling();
            hideAllPages();
            document.getElementById('healthSection').classList.remove('hidden');
            updateWatchdogDiag();
        }

        function openOtaPage() {
            setPageTitle('SLM Firmware Update');
            if (!calAuth) { showMaintenanceLocked(); return; }
            stopCalPolling();
            hideAllPages();
            document.getElementById('otaSection').classList.remove('hidden');
        }

        function openDeletePage() {
            setPageTitle('SLM Delete');
            if (!calAuth) { showMaintenanceLocked(); return; }
            stopCalPolling();
            hideAllPages();
            document.getElementById('deleteSection').classList.remove('hidden');
            refreshProcessedFiles();
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
                        showMaintenanceMenu();
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
                stopCalPolling();
            } else {
                save.disabled = false;
            }
        }

        function handleCalibrationAuthError(res) {
            if (res && res.status === 403 && res.json && res.json.reason === 'calibration_auth_required') {
                calAuth = false;
                showMaintenanceLocked();
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

        function escapeHtml(text) {
            return String(text)
                .replace(/&/g, '&amp;')
                .replace(/</g, '&lt;')
                .replace(/>/g, '&gt;')
                .replace(/"/g, '&quot;')
                .replace(/'/g, '&#39;');
        }

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
                    const downloadMs = performance.now() - downloadStartMs;
                    setProgress(
                        'downloadProgressBar',
                        'downloadProgressText',
                        100,
                        'Download: complete in ' + formatElapsedMs(downloadMs) +
                            ' (' + formatBytes(buffer.byteLength) + ')'
                    );

                    // Save the file as soon as the binary download is complete.
                    // Analysis is then performed from the same buffer.
                    saveDownloadedBuffer(filename, buffer);

                    const processingStartMs = performance.now();
                    status.textContent = 'Processing ' + filename + '...';

                    return analyzeRecorderFileAsync(buffer)
                        .then(analysis => {
                            analysis.downloadMs = downloadMs;
                            analysis.processingMs = performance.now() - processingStartMs;
                            showFlightAnalysis(filename, analysis);
                        });
                })
                .catch(err => {
                    status.textContent = 'Download or analysis failed. Falling back to direct download.';
                    console.error(err);
                    window.location.href = '/api/download?file=' + encodeURIComponent(filename);
                });
        }

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

        function formatElapsedMs(ms) {
            const seconds = (Number(ms) || 0) / 1000.0;
            if (seconds < 10.0) return seconds.toFixed(1) + ' s';
            return Math.round(seconds) + ' s';
        }

        function analysisYield() {
            return new Promise(resolve => setTimeout(resolve, 0));
        }

        function saveDownloadedBuffer(filename, buffer) {
            const blob = new Blob([buffer], { type: 'application/octet-stream' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            a.remove();
            setTimeout(() => URL.revokeObjectURL(url), 30000);
        }

        function showFlightAnalysis(filename, analysis) {
            const status = document.getElementById('flightAnalysisStatus');
            const table = document.getElementById('flightAnalysisTable');
            const sampleStats = document.getElementById('samplePeriodStats');

            const timingText = analysisTimingText(analysis);
            if (sampleStats) {
                sampleStats.textContent = samplePeriodStatsText(analysis && analysis.samplePeriod);
            }

            if (!analysis || !analysis.ok) {
                const reason = analysis && analysis.reason ? analysis.reason : 'Analysis failed.';
                status.textContent = reason + timingText;
                table.innerHTML = '';
                return;
            }


            if (analysis.flights.length === 0) {
                status.textContent = filename + ' (' + (analysis.formatName || 'unknown format') +
                    '): no complete flight detected from ' + analysis.recordCount +
                    ' acceleration records.' + timingText;
                table.innerHTML = '';
                return;
            }

            status.textContent = filename + ' (' + (analysis.formatName || 'unknown format') +
                '): ' + analysis.flights.length + ' flight(s) detected from ' +
                analysis.recordCount + ' acceleration records.' + timingText;

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


        function analysisTimingText(analysis) {
            if (!analysis) return '';

            const parts = [];
            if (Number.isFinite(analysis.downloadMs)) {
                parts.push('download ' + formatElapsedMs(analysis.downloadMs));
            }
            if (Number.isFinite(analysis.processingMs)) {
                parts.push('processing ' + formatElapsedMs(analysis.processingMs));
            }

            return parts.length > 0 ? (' [' + parts.join(', ') + ']') : '';
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
        // The flight analysis is common to both formats and implements the
        // Python method used during development: HIRMS (>3 Hz), LOWRMS
        // (0.5..3 Hz), FlightGround = normalized LOWRMS - normalized HIRMS,
        // hysteresis flight/ground state, then roll-phase timing from
        // normalized HIRMS thresholds.
        // ---------------------------------------------------------------------

        const SLM_ANALYSIS_FS = 20.0;
        const SLM_ANALYSIS_HIRMS_WINDOW_S = 4.0;
        const SLM_ANALYSIS_LOWRMS_WINDOW_S = 10.0;
        const SLM_ANALYSIS_FLIGHTGROUND_LPF_PERIOD_S = 20.0;
        const SLM_ANALYSIS_FLIGHTGROUND_THRESHOLD = 0.05;
        const SLM_ANALYSIS_FLIGHTGROUND_HYSTERESIS = 0.10;
        const SLM_ANALYSIS_SEARCH_WINDOW_S = 80.0;
        const SLM_ANALYSIS_TO_ROLL_START_THR = 0.10;
        const SLM_ANALYSIS_TO_ROLL_END_THR = 0.25;
        const SLM_ANALYSIS_LDG_ROLL_START_THR = 0.25;
        const SLM_ANALYSIS_LDG_ROLL_END_THR = 0.10;
        const SLM_ANALYSIS_MIN_FILE_S = 30.0;
        const SLM_ANALYSIS_BUTTER_Q = [0.541196100146, 1.306562964876];

        const SLM_RECORD_START = 0x55;
        const SLM_RECORD_TYPE_ACCEL = 0x70;
        const SLM_RECORD_LENGTH = 13;

        const WIT_PACKET_START = 0x55;
        const WIT_TYPE_TIME = 0x50;
        const WIT_TYPE_ACCEL = 0x51;
        const WIT_PACKET_LENGTH = 11;
        const WIT_SCALE_G = 16.0 / 32768.0;

        async function analyzeRecorderFileAsync(buffer, progressCb) {
            const progress = progressCb || function(){};

            progress(5, 'Processing: decoding file format...');
            await analysisYield();

            const decoded = decodeRecorderTimeline(buffer);
            const samplePeriod = decoded ? computeSamplePeriodStats(decoded.records) : null;

            if (!decoded || decoded.records.length < SLM_ANALYSIS_FS * SLM_ANALYSIS_MIN_FILE_S) {
                progress(100, 'Processing: not enough valid acceleration records.');
                return {
                    ok: false,
                    reason: 'Not enough valid acceleration records for flight analysis.',
                    formatName: decoded ? decoded.formatName : 'unknown',
                    recordCount: decoded ? decoded.records.length : 0,
                    samplePeriod: samplePeriod
                };
            }

            progress(15, 'Processing: decoded ' + decoded.formatName + ', ' +
                decoded.records.length + ' acceleration records.');
            await analysisYield();

            const flights = await analyzeAccelerationTimelineAsync(decoded.records, progress);
            flights.forEach((f, index) => { f.number = index + 1; });

            return {
                ok: true,
                formatName: decoded.formatName,
                recordCount: decoded.records.length,
                samplePeriod: samplePeriod,
                flights: flights
            };
        }

        function computeSamplePeriodStats(records) {
            if (!records || records.length < 2) {
                return { ok: false, count: 0, excluded: 0 };
            }

            const nominalMs = 1000.0 / SLM_ANALYSIS_FS;
            const maxValidMs = nominalMs * 4.0;
            const minValidMs = nominalMs * 0.2;
            const periods = [];
            let excluded = 0;

            for (let i = 1; i < records.length; i++) {
                const dt = records[i].absMs - records[i - 1].absMs;
                if (Number.isFinite(dt) && dt >= minValidMs && dt <= maxValidMs) {
                    periods.push(dt);
                } else {
                    excluded++;
                }
            }

            if (periods.length === 0) {
                return { ok: false, count: 0, excluded: excluded };
            }

            let sum = 0.0;
            let min = periods[0];
            let max = periods[0];
            let outside49_51 = 0;
            let outside48_52 = 0;

            periods.forEach(dt => {
                sum += dt;
                if (dt < min) min = dt;
                if (dt > max) max = dt;
                if (dt < 49.0 || dt > 51.0) outside49_51++;
                if (dt < 48.0 || dt > 52.0) outside48_52++;
            });

            const avg = sum / periods.length;
            let varSum = 0.0;
            periods.forEach(dt => {
                const d = dt - avg;
                varSum += d * d;
            });

            return {
                ok: true,
                count: periods.length,
                excluded: excluded,
                avgMs: avg,
                stdMs: Math.sqrt(varSum / periods.length),
                minMs: min,
                maxMs: max,
                outside49_51: outside49_51,
                outside48_52: outside48_52
            };
        }

        function samplePeriodStatsText(stats) {
            if (!stats || !stats.ok) {
                return 'Sample period: unavailable';
            }

            let text = 'Sample period: avg ' + stats.avgMs.toFixed(2) +
                ' ms, std ' + stats.stdMs.toFixed(2) +
                ' ms, min/max ' + stats.minMs.toFixed(1) +
                '/' + stats.maxMs.toFixed(1) + ' ms' +
                ', outside 49-51 ms: ' + stats.outside49_51 +
                ', outside 48-52 ms: ' + stats.outside48_52;

            if (stats.excluded > 0) {
                text += ', gaps excluded ' + stats.excluded;
            }

            return text;
        }

        function decodeRecorderTimeline(buffer) {
            const slm = decodeSlmTimeline(buffer);
            const wit = decodeWitTimeline(buffer);

            if (slm.records.length === 0 && wit.records.length === 0) {
                return { formatName: 'unknown', records: [] };
            }

            // The formats use distinct packet IDs.  Select the decoder that
            // produced the largest valid acceleration timeline.  This keeps the
            // analysis independent of file extension and supports old WIT files
            // directly from the phone download page.
            return (slm.records.length >= wit.records.length) ? slm : wit;
        }

        function decodeSlmTimeline(buffer) {
            const bytes = new Uint8Array(buffer);
            const view = new DataView(buffer);
            const records = [];
            let dayOffset = 0;
            let previousAbs = null;

            for (let i = 0; i + SLM_RECORD_LENGTH <= bytes.length; ) {
                if (bytes[i] !== SLM_RECORD_START || bytes[i + 1] !== SLM_RECORD_TYPE_ACCEL) {
                    i++;
                    continue;
                }

                let sum = 0;
                for (let k = 0; k < SLM_RECORD_LENGTH - 1; k++) {
                    sum = (sum + bytes[i + k]) & 0xFF;
                }

                if (sum !== bytes[i + SLM_RECORD_LENGTH - 1]) {
                    i++;
                    continue;
                }

                const timeMs = view.getUint32(i + 2, true);
                const axMg = view.getInt16(i + 6, true);
                const ayMg = view.getInt16(i + 8, true);
                const azMg = view.getInt16(i + 10, true);

                if (previousAbs !== null && timeMs + dayOffset < previousAbs - 1000) {
                    dayOffset += 24 * 3600 * 1000;
                }

                const absMs = timeMs + dayOffset;
                records.push({
                    absMs: absMs,
                    axG: axMg / 1000.0,
                    ayG: ayMg / 1000.0,
                    azG: azMg / 1000.0
                });

                previousAbs = absMs;
                i += SLM_RECORD_LENGTH;
            }

            return { formatName: 'SLM 0x70', records: records };
        }

        function decodeWitTimeline(buffer) {
            const bytes = new Uint8Array(buffer);
            const view = new DataView(buffer);
            const records = [];
            let dayOffset = 0;
            let lastSodMs = null;
            let anchorAbsMs = null;
            let accelCountSinceAnchor = 0;

            for (let i = 0; i + 2 <= bytes.length; ) {
                if (bytes[i] !== WIT_PACKET_START) {
                    i++;
                    continue;
                }

                const packetType = bytes[i + 1];
                if ((packetType !== WIT_TYPE_TIME) && (packetType !== WIT_TYPE_ACCEL)) {
                    i++;
                    continue;
                }

                if (i + WIT_PACKET_LENGTH > bytes.length) {
                    break;
                }

                let sum = 0;
                for (let k = 0; k < WIT_PACKET_LENGTH - 1; k++) {
                    sum = (sum + bytes[i + k]) & 0xFF;
                }

                if (sum !== bytes[i + WIT_PACKET_LENGTH - 1]) {
                    i++;
                    continue;
                }

                if (packetType === WIT_TYPE_TIME) {
                    const sodMs = decodeWitTimeSodMs(bytes, view, i);
                    if (sodMs !== null) {
                        if (lastSodMs !== null && sodMs < lastSodMs) {
                            dayOffset += 24 * 3600 * 1000;
                        }
                        anchorAbsMs = dayOffset + sodMs;
                        accelCountSinceAnchor = 0;
                        lastSodMs = sodMs;
                    }
                } else {
                    const axRaw = view.getInt16(i + 2, true);
                    const ayRaw = view.getInt16(i + 4, true);
                    const azRaw = view.getInt16(i + 6, true);
                    const absMs = (anchorAbsMs === null)
                        ? Math.round(records.length * 1000.0 / SLM_ANALYSIS_FS)
                        : Math.round(anchorAbsMs + accelCountSinceAnchor * 1000.0 / SLM_ANALYSIS_FS);

                    records.push({
                        absMs: absMs,
                        axG: axRaw * WIT_SCALE_G,
                        ayG: ayRaw * WIT_SCALE_G,
                        azG: azRaw * WIT_SCALE_G
                    });

                    accelCountSinceAnchor++;
                }

                i += WIT_PACKET_LENGTH;
            }

            const formatName = (lastSodMs === null) ? 'WIT 0x51' : 'WIT 0x50/0x51';
            return { formatName: formatName, records: records };
        }

        function decodeWitTimeSodMs(bytes, view, offset) {
            const month = bytes[offset + 3];
            const day = bytes[offset + 4];
            const hour = bytes[offset + 5];
            const minute = bytes[offset + 6];
            const second = bytes[offset + 7];
            const millisecond = view.getUint16(offset + 8, true);

            if (month < 1 || month > 12 || day < 1 || day > 31 ||
                hour > 23 || minute > 59 || second > 59 || millisecond > 999) {
                return null;
            }

            return ((hour * 3600 + minute * 60 + second) * 1000 + millisecond);
        }

        async function analyzeAccelerationTimelineAsync(records, progressCb) {
            const progress = progressCb || function(){};

            progress(20, 'Processing: computing acceleration magnitude...');
            await analysisYield();
            const accelMod = records.map(r => Math.sqrt(r.axG * r.axG + r.ayG * r.ayG + r.azG * r.azG));

            progress(32, 'Processing: computing HIRMS (>3 Hz)...');
            await analysisYield();
            const accelHigh = butterHighpass4ZeroPhase(accelMod, 3.0, SLM_ANALYSIS_FS);
            const hirms = movingRms(accelHigh, SLM_ANALYSIS_HIRMS_WINDOW_S, SLM_ANALYSIS_FS);

            progress(48, 'Processing: computing LOWRMS (0.5..3 Hz)...');
            await analysisYield();
            const accelLow = butterBandpassApproxZeroPhase(accelMod, 0.5, 3.0, SLM_ANALYSIS_FS);
            const lowrms = movingRms(accelLow, SLM_ANALYSIS_LOWRMS_WINDOW_S, SLM_ANALYSIS_FS);

            progress(62, 'Processing: normalizing RMS signals...');
            await analysisYield();
            const hMin = arrayMin(hirms);
            const hMax = arrayMax(hirms);
            const hirmsNorm = normalizeWithRange(hirms, hMin, hMax);
            const lowrmsNorm = normalizeWithRange(lowrms, hMin, hMax);

            progress(72, 'Processing: computing FlightGround...');
            await analysisYield();
            const flightgroundRaw = lowrmsNorm.map((v, i) => Math.max(0.0, v - hirmsNorm[i]));
            const fgCutoff = 1.0 / SLM_ANALYSIS_FLIGHTGROUND_LPF_PERIOD_S;
            const flightground = butterLowpass4ZeroPhase(flightgroundRaw, fgCutoff, SLM_ANALYSIS_FS);

            progress(84, 'Processing: finding flight/ground transitions...');
            await analysisYield();
            const flightState = calculateBooleanState(
                flightground,
                SLM_ANALYSIS_FLIGHTGROUND_THRESHOLD,
                SLM_ANALYSIS_FLIGHTGROUND_HYSTERESIS
            );

            const segments = findTransitionPeaks(
                flightState,
                hirms,
                SLM_ANALYSIS_FS,
                SLM_ANALYSIS_SEARCH_WINDOW_S
            );

            progress(94, 'Processing: finding roll phase times...');
            await analysisYield();
            return findRollPhases(segments, hirmsNorm, records);
        }

        function makeBiquad(type, cutoffHz, fs, q) {
            const w0 = 2.0 * Math.PI * cutoffHz / fs;
            const cosw0 = Math.cos(w0);
            const sinw0 = Math.sin(w0);
            const alpha = sinw0 / (2.0 * q);

            let b0, b1, b2;
            if (type === 'low') {
                b0 = (1.0 - cosw0) / 2.0;
                b1 = 1.0 - cosw0;
                b2 = (1.0 - cosw0) / 2.0;
            } else {
                b0 = (1.0 + cosw0) / 2.0;
                b1 = -(1.0 + cosw0);
                b2 = (1.0 + cosw0) / 2.0;
            }

            const a0 = 1.0 + alpha;
            const a1 = -2.0 * cosw0;
            const a2 = 1.0 - alpha;

            return {
                b0: b0 / a0,
                b1: b1 / a0,
                b2: b2 / a0,
                a1: a1 / a0,
                a2: a2 / a0
            };
        }

        function applyBiquad(data, coeff) {
            const out = new Array(data.length);
            let x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

            for (let i = 0; i < data.length; i++) {
                const x0 = data[i];
                const y0 = coeff.b0 * x0 + coeff.b1 * x1 + coeff.b2 * x2 - coeff.a1 * y1 - coeff.a2 * y2;
                out[i] = Number.isFinite(y0) ? y0 : 0.0;
                x2 = x1; x1 = x0;
                y2 = y1; y1 = out[i];
            }

            return out;
        }

        function applyCascade(data, sections) {
            let out = data.slice();
            sections.forEach(section => { out = applyBiquad(out, section); });
            return out;
        }

        function applyZeroPhase(data, sections) {
            const fwd = applyCascade(data, sections);
            const rev = applyCascade(fwd.slice().reverse(), sections);
            return rev.reverse();
        }

        function butterLowpass4ZeroPhase(data, cutoffHz, fs) {
            const sections = SLM_ANALYSIS_BUTTER_Q.map(q => makeBiquad('low', cutoffHz, fs, q));
            return applyZeroPhase(data, sections);
        }

        function butterHighpass4ZeroPhase(data, cutoffHz, fs) {
            const sections = SLM_ANALYSIS_BUTTER_Q.map(q => makeBiquad('high', cutoffHz, fs, q));
            return applyZeroPhase(data, sections);
        }

        function butterBandpassApproxZeroPhase(data, lowHz, highHz, fs) {
            return butterLowpass4ZeroPhase(butterHighpass4ZeroPhase(data, lowHz, fs), highHz, fs);
        }

        function movingRms(data, windowTimeS, fs) {
            const n = Math.max(1, Math.round(windowTimeS * fs));
            const half = Math.floor(n / 2);
            const prefix = new Array(data.length + 1);
            prefix[0] = 0.0;

            for (let i = 0; i < data.length; i++) {
                prefix[i + 1] = prefix[i] + data[i] * data[i];
            }

            const out = new Array(data.length);
            for (let i = 0; i < data.length; i++) {
                const startRaw = i - half;
                const endRaw = startRaw + n;
                const start = Math.max(0, startRaw);
                const end = Math.min(data.length, endRaw);
                const sum = prefix[end] - prefix[start];
                out[i] = Math.sqrt(Math.max(0.0, sum / n));
            }

            return out;
        }

        function normalizeWithRange(data, minVal, maxVal) {
            if (!Number.isFinite(minVal) || !Number.isFinite(maxVal) || maxVal === minVal) {
                return data.map(() => 0.0);
            }
            return data.map(v => (v - minVal) / (maxVal - minVal));
        }

        function calculateBooleanState(data, threshold, hysteresis) {
            const tUp = threshold + hysteresis / 2.0;
            const tDown = threshold - hysteresis / 2.0;
            let state = 0;

            return data.map(value => {
                if (state === 0 && value >= tUp) state = 1;
                else if (state === 1 && value <= tDown) state = 0;
                return state;
            });
        }

        function findTransitionPeaks(booleanArray, signalToPeak, fs, windowTimeS) {
            const transitions = [];
            for (let i = 1; i < booleanArray.length; i++) {
                if (booleanArray[i] !== booleanArray[i - 1]) transitions.push(i);
            }

            const startIndices = transitions.filter(idx => booleanArray[idx] === 1);
            const endIndices = transitions.filter(idx => booleanArray[idx] === 0);
            if (booleanArray[0] === 1) startIndices.unshift(0);

            const halfWindow = Math.round((windowTimeS / 2.0) * fs);
            const segments = [];
            let endCursor = 0;

            startIndices.forEach(startIdx => {
                while (endCursor < endIndices.length && endIndices[endCursor] <= startIdx) endCursor++;
                if (endCursor >= endIndices.length) return;

                const endIdx = endIndices[endCursor++];

                const startWin0 = Math.max(0, startIdx - halfWindow);
                const startWin1 = Math.min(signalToPeak.length, startIdx + halfWindow);
                const endWin0 = Math.max(0, endIdx - halfWindow);
                const endWin1 = Math.min(signalToPeak.length, endIdx + halfWindow);

                const peakStartIdx = maxIndexInRange(signalToPeak, startWin0, startWin1);
                const peakEndIdx = maxIndexInRange(signalToPeak, endWin0, endWin1);

                if (peakStartIdx >= 0 && peakEndIdx > peakStartIdx) {
                    segments.push({
                        startIdx: startIdx,
                        endIdx: endIdx,
                        peakStartIdx: peakStartIdx,
                        peakEndIdx: peakEndIdx
                    });
                }
            });

            return segments;
        }

        function findRollPhases(segments, hirmsNorm, records) {
            const results = [];

            segments.forEach(segment => {
                const peakStartIdx = segment.peakStartIdx;
                const peakEndIdx = segment.peakEndIdx;

                let toRollStartIdx = 0;
                let liftOffIdx = peakEndIdx;
                let touchdownIdx = peakStartIdx;
                let ldgRollEndIdx = records.length - 1;

                for (let i = peakStartIdx - 1; i >= 0; i--) {
                    if (hirmsNorm[i] <= SLM_ANALYSIS_TO_ROLL_START_THR) {
                        toRollStartIdx = i + 1;
                        break;
                    }
                }

                for (let i = peakStartIdx; i < peakEndIdx; i++) {
                    if (hirmsNorm[i] <= SLM_ANALYSIS_TO_ROLL_END_THR) {
                        liftOffIdx = i;
                        break;
                    }
                }

                for (let i = peakEndIdx - 1; i >= peakStartIdx; i--) {
                    if (hirmsNorm[i] <= SLM_ANALYSIS_LDG_ROLL_START_THR) {
                        touchdownIdx = i + 1;
                        break;
                    }
                }

                for (let i = peakEndIdx; i < records.length; i++) {
                    if (hirmsNorm[i] <= SLM_ANALYSIS_LDG_ROLL_END_THR) {
                        ldgRollEndIdx = i;
                        break;
                    }
                }

                const takeoffRollMs = Math.max(0, records[liftOffIdx].absMs - records[toRollStartIdx].absMs);
                const landingRollMs = Math.max(0, records[ldgRollEndIdx].absMs - records[touchdownIdx].absMs);
                const durationMs = Math.max(0, records[ldgRollEndIdx].absMs - records[toRollStartIdx].absMs);

                results.push({
                    takeoffRollStart: formatClockMs(records[toRollStartIdx].absMs),
                    takeoffRollStartShort: formatClockHm(records[toRollStartIdx].absMs),
                    liftOff: formatClockMs(records[liftOffIdx].absMs),
                    touchdown: formatClockMs(records[touchdownIdx].absMs),
                    landingRollEnd: formatClockMs(records[ldgRollEndIdx].absMs),
                    landingRollEndShort: formatClockHm(records[ldgRollEndIdx].absMs),
                    takeoffRollDuration: formatDurationSeconds(takeoffRollMs),
                    landingRollDuration: formatDurationSeconds(landingRollMs),
                    flightDuration: formatDurationMs(durationMs),
                    flightDurationShort: formatDurationHm(durationMs)
                });
            });

            return results;
        }

        function maxIndexInRange(data, start, end) {
            let bestIdx = -1;
            let bestVal = -Infinity;

            for (let i = start; i < end; i++) {
                if (data[i] > bestVal) {
                    bestVal = data[i];
                    bestIdx = i;
                }
            }

            return bestIdx;
        }

        function arrayMin(data) {
            return data.reduce((a, b) => Math.min(a, b), Infinity);
        }

        function arrayMax(data) {
            return data.reduce((a, b) => Math.max(a, b), -Infinity);
        }

        function formatClockMs(absMs) {
            let totalSeconds = Math.round(absMs / 1000.0);
            totalSeconds = ((totalSeconds % 86400) + 86400) % 86400;
            const h = Math.floor(totalSeconds / 3600);
            const m = Math.floor((totalSeconds % 3600) / 60);
            const s = totalSeconds % 60;
            return pad2(h) + ':' + pad2(m) + ':' + pad2(s);
        }

        function formatClockHm(absMs) {
            let totalSeconds = Math.round(absMs / 1000.0);
            totalSeconds = ((totalSeconds % 86400) + 86400) % 86400;
            const h = Math.floor(totalSeconds / 3600);
            const m = Math.floor((totalSeconds % 3600) / 60);
            return pad2(h) + ':' + pad2(m);
        }

        function formatDurationMs(ms) {
            let totalSeconds = Math.round(ms / 1000.0);
            const h = Math.floor(totalSeconds / 3600);
            const m = Math.floor((totalSeconds % 3600) / 60);
            const s = totalSeconds % 60;
            return pad2(h) + ':' + pad2(m) + ':' + pad2(s);
        }

        function formatDurationHm(ms) {
            let totalSeconds = Math.round(ms / 1000.0);
            const h = Math.floor(totalSeconds / 3600);
            const m = Math.floor((totalSeconds % 3600) / 60);
            return pad2(h) + ':' + pad2(m);
        }

        function formatDurationSeconds(ms) {
            return String(Math.round((Number(ms) || 0) / 1000.0));
        }

        function pad2(v) {
            return String(v).padStart(2, '0');
        }

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

        function deleteProcessedOne(name) {
            return fetch('/api/processed/delete?file=' + encodeURIComponent(name), { method: 'POST' })
                .then(r => r.json().then(j => ({ok:r.ok, json:j, name:name})));
        }

        function deleteProcessedSelected() {
            const selected = selectedProcessedFiles();
            const status = document.getElementById('processedDeleteStatus');
            if (selected.length === 0) {
                status.textContent = 'No files selected.';
                return;
            }

            if (!confirm('All selected files will be lost. Delete ' + selected.length + ' file(s)?')) {
                return;
            }

            status.textContent = 'Deleting...';
            let chain = Promise.resolve();
            let deleted = 0;
            let failed = 0;

            selected.forEach(name => {
                chain = chain.then(() => deleteProcessedOne(name))
                    .then(res => {
                        if (res.ok && res.json && res.json.ok) deleted++;
                        else failed++;
                    })
                    .catch(err => {
                        failed++;
                        console.error(err);
                    });
            });

            chain.then(() => {
                status.textContent = 'Deleted: ' + deleted + ', failed: ' + failed + '.';
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

        function calStatus() {
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
                if (xhr.status === 403) {
                    calAuth = false;
                    status.textContent = 'Maintenance authorization required.';
                    button.disabled = false;
                    showMaintenanceLocked();
                    return;
                }
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

        showHome();
    </script>
</body>
</html>
)rawliteral";

#endif // HTML_INTERFACE_H
