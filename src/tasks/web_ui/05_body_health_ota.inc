/*******************************************************************************
 * HEALTH AND FIRMWARE-UPDATE PAGES
 * Static HTML for watchdog diagnostic and OTA firmware upload pages.
 ******************************************************************************/

R"rawliteral(        <div id="healthSection" class="hidden section">
            <div class="card workflow-card">
                <h2 data-i18n="ABOUT"></h2>
                <div><span data-i18n="SOFTWARE_VERSION_LABEL"></span> <span class="mono">)rawliteral" RECORDER_SOFTWARE_VERSION R"rawliteral(</span></div>
                <div><span data-i18n="HARDWARE_VERSION_LABEL"></span> <span class="mono">)rawliteral" RECORDER_HARDWARE_VERSION R"rawliteral(</span></div>
            </div>

            <div id="aboutAuthPanel" class="card">
                <h2 data-i18n="ABOUT_ACCESS"></h2>
                <p data-i18n="SUPPORT_RESTRICTED"></p>
                <input type="password" id="aboutPassword" data-i18n-placeholder="SUPPORT_CODE" class="mono" style="padding:10px; margin:5px;">
                <div class="button-grid two-button-grid">
                    <button class="btn btn-primary" id="btnAboutUnlock" onclick="aboutUnlock()" data-i18n="UNLOCK_ABOUT"></button>
                    <button class="btn btn-return" onclick="showMaintenanceMenu()" data-i18n="RETURN"></button>
                </div>
                <div id="aboutAuthStatus" class="small" data-i18n="LOCKED"></div>
            </div>

            <div id="aboutMenuPanel" class="hidden">
                <div class="controls cal-actions">
                    <button class="btn btn-primary" onclick="supportDownloadData()" data-i18n="DOWNLOAD_SUPPORT_DATA"></button>
                    <button class="btn btn-primary" id="btnVerifyRecordings" onclick="supportVerifyRecordings()" data-i18n="VERIFY_RECORDINGS"></button>
                    <button class="btn btn-primary" onclick="supportGenerateCalibrationReports()" data-i18n="GENERATE_CAL_REPORTS"></button>
                    <button class="btn btn-primary" id="btnRestoreInstallationCalibration" onclick="supportRestoreInstallationCalibration()" data-i18n="RESTORE_INSTALL_CAL"></button>
                    <button class="btn btn-primary" onclick="supportClearRecorderCalibration()" data-i18n="CLEAR_REC_CAL"></button>
                    <button class="btn btn-primary" onclick="supportClearInstallationCalibration()" data-i18n="CLEAR_INSTALL_CAL"></button>
                </div>
                <div id="aboutSupportStatus" class="small" data-i18n="ABOUT_UNLOCKED"></div>
                <div id="watchdogDiagPanel" class="card">
                    <h2 data-i18n="WATCHDOG_DIAGNOSTIC"></h2>
                    <div id="watchdogDiagText" class="small mono" data-i18n="LOADING"></div>
                </div>
                <div class="controls cal-actions">
                    <button class="btn btn-primary" onclick="updateWatchdogDiag()" data-i18n="REFRESH"></button>
                    <button class="btn btn-return" onclick="showMaintenanceMenu()" data-i18n="RETURN"></button>
                </div>
            </div>
        </div>

        <div id="otaSection" class="hidden section">
            <div class="card workflow-card">
                <p><b data-i18n="OTA_USB_REQUIRED"></b></p>
                <p class="small" data-i18n="OTA_RESTART_NOTE"></p>
            </div>
            <div class="card">
                <h2 data-i18n="SELECT_FIRMWARE"></h2>
                <p class="small"><b data-i18n="FROM_SERVER_PREFERRED"></b></p>
                <div class="controls cal-actions">
                    <button class="btn btn-primary" id="btnOtaServerList" onclick="otaListServerFirmware()" data-i18n="SELECT_FROM_SERVER"></button>
                </div>
                <select id="otaServerFirmwareSelect" class="hidden" onchange="otaServerSelectionChanged()"></select>
                <div id="otaServerStatus" class="small" data-i18n="SERVER_FIRMWARE_REQUIRES_BRIDGE"></div>

                <p class="small"><b data-i18n="FROM_PHONE_SUPPORT"></b></p>
                <input type="file" id="otaFile" accept=".bin" onchange="otaLocalSelectionChanged()">
            </div>
            <div class="card">
                <div class="button-grid two-button-grid">
                    <button class="btn btn-primary" id="btnOtaUpdate" onclick="otaUpdateSelected()" data-i18n="UPDATE_FIRMWARE"></button>
                    <button class="btn btn-return" onclick="showMaintenanceMenu()" data-i18n="RETURN"></button>
                </div>
                <div id="otaStatus" class="small" data-i18n="SELECT_FW_SOURCE"></div>
            </div>
        </div>
    </div>

)rawliteral"
