/*******************************************************************************
 * HEALTH AND FIRMWARE-UPDATE PAGES
 * Static HTML for watchdog diagnostic and OTA firmware upload pages.
 *
 * Byte-stability rule: this file contains adjacent string literal fragments.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        <!-- About/support page. One support unlock grants access to all actions for this page session. -->
        <div id="healthSection" class="hidden section">
            <div class="card workflow-card">
                <h2>About</h2>
                <div>Software version: <span class="mono">)rawliteral" RECORDER_SOFTWARE_VERSION R"rawliteral(</span></div>
                <div>Hardware version: <span class="mono">)rawliteral" RECORDER_HARDWARE_VERSION R"rawliteral(</span></div>
            </div>

            <div id="aboutAuthPanel" class="card">
                <h2>About Access</h2>
                <p><b>Support functions are restricted.</b> Enter the support code once to unlock all About functions.</p>
                <input type="password" id="aboutPassword" placeholder="Support code" class="mono" style="padding:10px; margin:5px;">
                <div class="button-grid two-button-grid">
                    <button class="btn btn-primary" id="btnAboutUnlock" onclick="aboutUnlock()">Unlock About</button>
                    <button class="btn btn-return" onclick="showMaintenanceMenu()">Return</button>
                </div>
                <div id="aboutAuthStatus" class="small">Locked.</div>
            </div>

            <div id="aboutMenuPanel" class="hidden">
                <div class="controls cal-actions">
                    <button class="btn btn-primary" onclick="supportDownloadData()">Download Support Data</button>
                    <button class="btn btn-primary" onclick="supportGenerateCalibrationReports()">Generate Calibration Reports</button>
                    <button class="btn btn-primary" onclick="supportClearRecorderCalibration()">Clear Recorder Calibration</button>
                    <button class="btn btn-primary" onclick="supportClearInstallationCalibration()">Clear Installation Calibration</button>
                    <button class="btn btn-primary" onclick="openDeletePage()">Delete Files/Reports</button>
                </div>
                <div id="aboutSupportStatus" class="small">About functions unlocked.</div>

                <!-- Watchdog diagnostic exposes the last supervision fault captured by firmware. -->
                <div id="watchdogDiagPanel" class="card">
                    <h2>Watchdog Diagnostic</h2>
                    <div id="watchdogDiagText" class="small mono">Loading...</div>
                </div>
                <div class="controls cal-actions">
                    <button class="btn btn-primary" onclick="updateWatchdogDiag()">Refresh</button>
                    <button class="btn btn-return" onclick="showMaintenanceMenu()">Return</button>
                </div>
            </div>
        </div>

        <!-- OTA page. Upload is deliberately simple: one firmware .bin file posted to /api/ota. -->
        <div id="otaSection" class="hidden section">
            <div class="card workflow-card">
                <p><b>USB power is required for firmware update.</b></p>
                <p class="small">Upload only the recorder firmware <span class="mono">.bin</span> file, typically <span class="mono">SLM_recorder_date_version.bin</span>.</p>
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

)rawliteral"
