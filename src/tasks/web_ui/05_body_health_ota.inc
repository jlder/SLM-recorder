/*******************************************************************************
 * HEALTH AND FIRMWARE-UPDATE PAGES
 * Static HTML for watchdog diagnostic and OTA firmware upload pages.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        <div id="healthSection" class="hidden section">
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
