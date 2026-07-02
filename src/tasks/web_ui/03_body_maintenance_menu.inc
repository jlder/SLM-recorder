/*******************************************************************************
 * MAINTENANCE MENU PAGE
 * Static HTML for maintenance login and the protected maintenance menu.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        <div id="maintenanceSection" class="hidden section">
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
                    <button class="btn btn-primary" onclick="openReportPage()">Report Management</button>
                    <button class="btn btn-primary" onclick="openDeletePage()">Delete Files/Reports</button>
                    <button class="btn btn-primary" onclick="openOtaPage()">Firmware Update</button>
                    <button class="btn btn-primary" onclick="openHealthPage()">About</button>
                    <button class="btn btn-return" onclick="showHome()">Return</button>
                </div>
            </div>
        </div>

)rawliteral"
