/*******************************************************************************
 * CALIBRATION PAGES
 * Static HTML for recorder six-face calibration, installation calibration, and processed-file delete page.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        <div id="accelCalPage" class="hidden section">
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
                <button class="btn btn-primary" id="btnProcessedDelete" onclick="deleteProcessedSelected()">Delete</button>
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

)rawliteral"
