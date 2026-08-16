/*******************************************************************************
 * MAINTENANCE MENU PAGE
 * Static HTML for maintenance login and the protected maintenance menu.
 ******************************************************************************/

R"rawliteral(        <div id="maintenanceSection" class="hidden section">
            <div id="calAuthPanel" class="card">
                <h2 data-i18n="MAINTENANCE_ACCESS"></h2>
                <p data-i18n="MAINTENANCE_RESTRICTED"></p>
                <input type="password" id="calPassword" data-i18n-placeholder="PASSWORD" class="mono" style="padding:10px; margin:5px;">
                <div class="button-grid two-button-grid">
                    <button class="btn btn-primary" id="btnMaintenanceUnlock" onclick="calUnlock()" data-i18n="UNLOCK_MAINTENANCE"></button>
                    <button class="btn btn-return" onclick="showHome()" data-i18n="RETURN"></button>
                </div>
                <div id="calAuthStatus" class="small" data-i18n="LOCKED"></div>
            </div>

            <div id="calMenuPanel" class="hidden">
                <div class="button-grid maintenance-grid">
                    <div class="cal-menu-item">
                        <button class="btn btn-primary" id="btnMenuRecorderCal" onclick="openAccelCal()" data-i18n="RECORDER_CALIBRATION"></button>
                        <div class="last-cal-heading"><div data-i18n="LAST_RECORDER"></div><div data-i18n="CALIBRATION_WORD"></div></div>
                        <div class="small mono last-cal-date" id="sensorCalDate">-</div>
                        <div class="small mono last-cal-date" id="sensorCalTime">-</div>
                    </div>
                    <div class="cal-menu-item">
                        <button class="btn btn-primary" id="btnMenuInstallCal" onclick="openInstallCal()" data-i18n="INSTALLATION_CALIBRATION"></button>
                        <div class="last-cal-heading"><div data-i18n="LAST_INSTALLATION"></div><div data-i18n="INSTALLATION_WORD"></div></div>
                        <div class="small mono last-cal-date" id="installationCalDate">-</div>
                        <div class="small mono last-cal-date" id="installationCalTime">-</div>
                    </div>
                    <button class="btn btn-primary" id="btnMenuReport" onclick="openReportPage()" data-i18n="REPORT_MANAGEMENT"></button>
                    <button class="btn btn-primary" id="btnMenuFirmware" onclick="openOtaPage()" data-i18n="FIRMWARE_UPDATE"></button>
                    <button class="btn btn-primary" id="btnMenuAbout" onclick="openHealthPage()" data-i18n="ABOUT"></button>
                    <button class="btn btn-return" onclick="showHome()" data-i18n="RETURN"></button>
                </div>
            </div>
        </div>

)rawliteral"
