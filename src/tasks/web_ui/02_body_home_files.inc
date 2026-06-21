/*******************************************************************************
 * HOME AND FILE-MANAGEMENT PAGES
 * Static HTML for the main menu, recorder file list, download controls, and flight-analysis result panel.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(<body>
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

)rawliteral"
