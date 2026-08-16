/*******************************************************************************
 * HOME AND FILE-MANAGEMENT PAGES
 * Static HTML for the main menu, recorder file list, download controls, and flight-analysis result panel.
 ******************************************************************************/

R"rawliteral(<body>
    <div class="container">
        <div class="header">
            <h1 id="pageTitle" data-i18n="PAGE_RECORDER"></h1>
            <span id="statusBadge"></span>
        </div>

        <div id="homeSection" class="section">
            <h2 data-i18n="MAIN_MENU"></h2>
            <div class="button-grid main-menu-grid">
                <button class="btn btn-primary" id="btnHomeFiles" onclick="openFilesPage()" data-i18n="FILE_MANAGEMENT"></button>
                <button class="btn btn-primary" id="btnHomeLogbook" onclick="openLogbookPage()" data-i18n="LOGBOOK"></button>
                <button class="btn btn-primary" id="btnHomeMaintenance" onclick="openMaintenancePage()" data-i18n="MAINTENANCE"></button>
            </div>
            <div id="usbPowerRequiredMessage" class="analysis-window hidden">
                <b data-i18n="USB_POWER_REQUIRED"></b><br>
                <span data-i18n="USB_POWER_FUNCTIONS"></span>
            </div>
        </div>

        <div id="logbookSection" class="hidden">
            <p class="small"><b data-i18n="LOGBOOK_PROCESSED_ONLY"></b></p>
            <p class="small" data-i18n="LOGBOOK_LAST_FIVE_DAYS"></p>
            <div class="controls compact-controls cal-actions">
                <button class="btn btn-return" onclick="showHome()" data-i18n="RETURN"></button>
            </div>
            <div id="logbookContainer">
                <div class="loading" data-i18n="LOADING_LOGBOOK"></div>
            </div>
        </div>

        <div id="filesSection" class="hidden">
            <div class="info-bar">
                <span class="info-item"><span data-i18n="SD_CARD_LABEL"></span> <span class="info-value" id="sdSize">-</span></span>
                <span class="info-item"><span data-i18n="FREE_LABEL"></span> <span class="info-value" id="sdFree">-</span></span>
                <span class="info-item"><span data-i18n="FILES_LABEL"></span> <span class="info-value" id="fileCount">-</span></span>
            </div>

            <div class="controls compact-controls cal-actions">
                <button class="btn btn-return" onclick="showHome()" data-i18n="RETURN"></button>
            </div>
            <div id="fileListContainer">
                <div class="loading" data-i18n="LOADING_FILES"></div>
            </div>
            <div id="flightAnalysisWindow" class="analysis-window hidden">
                <h2 data-i18n="FLIGHT_ANALYSIS"></h2>
                <div id="flightAnalysisStatus" class="small" data-i18n="NO_FLIGHT_ANALYSIS"></div>
                <div id="flightAnalysisTable"></div>
                <div class="hidden">
                    <div id="downloadProgressText"></div>
                    <div id="downloadProgressBar"></div>
                </div>
            </div>
        </div>

)rawliteral"
