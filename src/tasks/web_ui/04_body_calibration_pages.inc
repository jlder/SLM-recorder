/*******************************************************************************
 * CALIBRATION PAGES
 * Static HTML for recorder six-face calibration, installation calibration, and calibration report management.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        <!-- Recorder six-face calibration page. The JavaScript calibration poll owns all live fields and button states. -->
        <div id="accelCalPage" class="hidden section">
            <div class="card workflow-card">
                <p><b>Workflow:</b> Start recorder calibration, place the recorder still on each of its six faces, and wait for each face to show OK. The recorder automatically keeps the best capture for each face. Save when all six face values are satisfactory. If no trusted recorder calibration exists, repeat once to confirm.</p>
            </div>

            <!-- Start becomes Cancel while active; Save is enabled only when the service reports a complete result. -->
            <div class="controls cal-actions">
                <button class="btn btn-primary" id="btnStart" onclick="calStart()">Start</button>
                <button class="btn btn-primary" id="btnSave" onclick="calSave()">Save</button>
                <button class="btn btn-return" onclick="showMaintenanceMenu()">Return</button>
            </div>

            <!-- Live recorder-calibration summary: current face, active-axis noise, time since best capture, temperature gate, and face completion. -->
            <div class="card cal-status-card">
                <div>Status: <span id="calStatus" class="mono">-</span></div>
                <div>Samples: <span id="calSamplesProcessed" class="mono">0</span></div>
                <div>Sensor temperature: <span id="calSensorTemp" class="mono">-</span></div>
                <div>Stddev current/min: <span id="calCurrentNoise" class="mono">-</span> / <span id="calLowestNoise" class="mono">-</span></div>
                <div>Time since min: <span id="calFaceTime" class="mono">-</span></div>
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

        <!-- Installation calibration page. The candidate is the current stable rolling window only. -->
        <div id="installCalPage" class="hidden section">
            <div class="card workflow-card">
                <p><b>Workflow:</b> Installation calibration is an aircraft maintenance action. Perform it only for initial installation, recorder reinstallation, or failed calibration retry. Put the glider in flight-level attitude with wings leveled, click Start, leave it still, then Save when noise is satisfactory.</p>
            </div>
            <!-- Save is enabled only when a reason is selected and the current window is stable. -->
            <div class="controls cal-actions">
                <button class="btn btn-primary" id="btnInstallStart" onclick="installStart()">Start</button>
                <button class="btn btn-primary" id="btnInstallSave" onclick="installSave()">Save</button>
                <button class="btn btn-return" onclick="showMaintenanceMenu()">Return</button>
            </div>
            <div class="card">
                <div>Reason for calibration: <select id="installReason" onchange="installSample()">
                    <option value="">Select reason</option>
                    <option value="initial_installation">Initial installation</option>
                    <option value="recorder_reinstalled">Recorder reinstalled</option>
                    <option value="failed_calibration_retry">Failed calibration retry</option>
                </select></div>
                <div class="small">This action shall be recorded in the aircraft maintenance documentation.</div>
                <div>Status: <span id="installStatus" class="mono">-</span></div>
                <div>Samples processed: <span id="installTotalSamples" class="mono">0</span></div>
                <div>Current noise: <span id="installBestNoise" class="mono">-</span></div>
                <div>Stability: <span id="installStability" class="mono">-</span></div>
                <!-- Candidate angles are from the current stable matrix; Current angles are from the stored matrix in NVS. -->
                <table class="cal-table">
                    <thead>
                        <tr>
                            <th>Angle</th>
                            <th>Candidate</th>
                            <th>Current</th>
                        </tr>
                    </thead>
                    <tbody>
                        <tr><td>Pitch</td><td id="installPitchCandidate">-</td><td id="installPitchStored">-</td></tr>
                        <tr><td>Roll</td><td id="installRollCandidate">-</td><td id="installRollStored">-</td></tr>
                    </tbody>
                </table>
                <pre id="installLog" class="hidden">Ready.</pre>
            </div>
        </div>


        <!-- Calibration report page. Reports are generated by save actions and remain available for download/upload. -->
        <div id="reportSection" class="hidden section">
            <h2>SLM Report Management</h2>
            <div class="controls compact-controls cal-actions">
                <button class="btn btn-primary" onclick="refreshReports()">Refresh</button>
                <button class="btn btn-return" onclick="showMaintenanceMenu()">Return</button>
            </div>
            <div class="card workflow-card">
                <p>Download calibration reports for maintenance records. Reports remain on the SD card and can also be uploaded by SLM Bridge.</p>
            </div>
            <div id="reportListContainer">
                <div class="loading">Loading reports...</div>
            </div>
        </div>



)rawliteral"
