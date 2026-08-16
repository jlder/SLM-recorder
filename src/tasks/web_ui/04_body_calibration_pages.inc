/*******************************************************************************
 * CALIBRATION PAGES
 * Static HTML for recorder six-face calibration, installation calibration, and calibration report management.
 ******************************************************************************/

R"rawliteral(        <div id="accelCalPage" class="hidden section">
            <div class="card workflow-card">
                <p><b data-i18n="WORKFLOW"></b> <span data-i18n="REC_CAL_WORKFLOW"></span></p>
            </div>
            <div class="controls cal-actions">
                <button class="btn btn-primary" id="btnStart" onclick="calStart()" data-i18n="START"></button>
                <button class="btn btn-primary" id="btnSave" onclick="calSave()" data-i18n="SAVE"></button>
                <button class="btn btn-return" onclick="showMaintenanceMenu()" data-i18n="RETURN"></button>
            </div>
            <div class="card cal-status-card">
                <div><span data-i18n="STATUS_LABEL"></span> <span id="calStatus" class="mono">-</span></div>
                <div><span data-i18n="SAMPLES_LABEL"></span> <span id="calSamplesProcessed" class="mono">0</span></div>
                <div><span data-i18n="SENSOR_TEMPERATURE_LABEL"></span> <span id="calSensorTemp" class="mono">-</span></div>
                <div><span data-i18n="STDDEV_CURRENT_MIN_LABEL"></span> <span id="calCurrentNoise" class="mono">-</span> / <span id="calLowestNoise" class="mono">-</span></div>
                <div><span data-i18n="TIME_SINCE_MIN_LABEL"></span> <span id="calFaceTime" class="mono">-</span></div>
                <div data-i18n="FACES_LABEL"></div>
                <div id="calFaceSummary" class="mono face-summary-grid"></div>
            </div>
            <div class="card">
                <table class="cal-table">
                    <thead><tr>
                        <th data-i18n="AXIS"></th><th data-i18n="GAIN"></th><th data-i18n="NVS_GAIN"></th><th data-i18n="OFFSET"></th><th data-i18n="NVS_OFFSET"></th>
                    </tr></thead>
                    <tbody>
                        <tr><td>X</td><td id="resGain0">-</td><td id="nvsGain0">-</td><td id="resOffset0">-</td><td id="nvsOffset0">-</td></tr>
                        <tr><td>Y</td><td id="resGain1">-</td><td id="nvsGain1">-</td><td id="resOffset1">-</td><td id="nvsOffset1">-</td></tr>
                        <tr><td>Z</td><td id="resGain2">-</td><td id="nvsGain2">-</td><td id="resOffset2">-</td><td id="nvsOffset2">-</td></tr>
                    </tbody>
                </table>
                <pre id="calLog" class="hidden" data-i18n="READY_DOT"></pre>
            </div>
        </div>

        <div id="installCalPage" class="hidden section">
            <div class="card workflow-card">
                <p><b data-i18n="WORKFLOW"></b> <span data-i18n="INSTALL_CAL_WORKFLOW"></span></p>
            </div>
            <div class="controls cal-actions">
                <button class="btn btn-primary" id="btnInstallStart" onclick="installStart()" data-i18n="START"></button>
                <button class="btn btn-primary" id="btnInstallSave" onclick="installSave()" data-i18n="SAVE"></button>
                <button class="btn btn-return" onclick="showMaintenanceMenu()" data-i18n="RETURN"></button>
            </div>
            <div class="card">
                <div><span data-i18n="REASON_FOR_CALIBRATION"></span> <select id="installReason" onchange="installSample()">
                    <option value="" data-i18n="SELECT_REASON"></option>
                    <option value="initial_installation" data-i18n="INITIAL_INSTALLATION"></option>
                    <option value="recorder_reinstalled" data-i18n="RECORDER_REINSTALLED"></option>
                    <option value="failed_calibration_retry" data-i18n="FAILED_CAL_RETRY"></option>
                </select></div>
                <div class="small" data-i18n="MAINTENANCE_RECORD_NOTE"></div>
                <div><span data-i18n="STATUS_LABEL"></span> <span id="installStatus" class="mono">-</span></div>
                <div><span data-i18n="SAMPLES_PROCESSED_LABEL"></span> <span id="installTotalSamples" class="mono">0</span></div>
                <div><span data-i18n="CURRENT_NOISE_LABEL"></span> <span id="installBestNoise" class="mono">-</span></div>
                <div><span data-i18n="STABILITY_LABEL"></span> <span id="installStability" class="mono">-</span></div>
                <table class="cal-table">
                    <thead><tr><th data-i18n="ANGLE"></th><th data-i18n="CANDIDATE"></th><th data-i18n="CURRENT"></th></tr></thead>
                    <tbody>
                        <tr><td data-i18n="PITCH"></td><td id="installPitchCandidate">-</td><td id="installPitchStored">-</td></tr>
                        <tr><td data-i18n="ROLL"></td><td id="installRollCandidate">-</td><td id="installRollStored">-</td></tr>
                    </tbody>
                </table>
                <pre id="installLog" class="hidden" data-i18n="READY_DOT"></pre>
            </div>
        </div>

        <div id="reportSection" class="hidden section">
            <h2 data-i18n="PAGE_REPORTS"></h2>
            <div class="controls compact-controls cal-actions">
                <button class="btn btn-primary" onclick="refreshReports()" data-i18n="REFRESH"></button>
                <button class="btn btn-return" onclick="showMaintenanceMenu()" data-i18n="RETURN"></button>
            </div>
            <div class="card workflow-card"><p data-i18n="REPORT_WORKFLOW"></p></div>
            <div id="reportListContainer"><div class="loading" data-i18n="LOADING_REPORTS"></div></div>
        </div>

)rawliteral"
