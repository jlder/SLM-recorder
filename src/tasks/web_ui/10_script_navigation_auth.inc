/*******************************************************************************
 * SCRIPT: PAGE NAVIGATION AND MAINTENANCE AUTHENTICATION
 * JavaScript state variables, page switching, maintenance unlock, and common API helper functions.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(    <script>
        // Shared client-side state for the single-page interface.
        // Page sections are static HTML; navigation only hides/shows sections and starts/stops polling.
        let allFiles = [];
        let reportFiles = [];
        let calPoll = null;
        let calAuth = false;
        let aboutAuth = false;
        let aboutSupportCode = '';
        let calView = 'menu';
        let calRecorderSaveMessageSticky = false;
        let installSampleRequestActive = false;
        let calFaceBestUpdateSeen = [0, 0, 0, 0, 0, 0];

        // -----------------------------------------------------------------
        // Page navigation
        // -----------------------------------------------------------------
        function setPageTitle(key) {
            const title = document.getElementById('pageTitle');
            if (!title) return;
            title.dataset.i18n = key;
            title.textContent = tr(key);
        }

        function hideAllPages() {
            document.getElementById('homeSection').classList.add('hidden');
            document.getElementById('logbookSection').classList.add('hidden');
            document.getElementById('filesSection').classList.add('hidden');
            document.getElementById('maintenanceSection').classList.add('hidden');
            document.getElementById('accelCalPage').classList.add('hidden');
            document.getElementById('installCalPage').classList.add('hidden');
            document.getElementById('healthSection').classList.add('hidden');
            document.getElementById('otaSection').classList.add('hidden');
            document.getElementById('reportSection').classList.add('hidden');
        }

        function showHome() {
            setPageTitle('PAGE_RECORDER');
            stopCalPolling();
            hideAllPages();
            document.getElementById('homeSection').classList.remove('hidden');
            updateStatus();
            calStatus();
        }

        function openFilesPage() {
            setPageTitle('PAGE_FILE_MANAGEMENT');
            stopCalPolling();
            hideAllPages();
            clearFlightAnalysisPanel();
            document.getElementById('filesSection').classList.remove('hidden');
            refreshFiles();
        }

        function openLogbookPage() {
            setPageTitle('PAGE_LOGBOOK');
            stopCalPolling();
            hideAllPages();
            document.getElementById('logbookSection').classList.remove('hidden');
            refreshLogbook();
        }

        function openMaintenancePage() {
            setPageTitle('PAGE_MAINTENANCE');
            stopCalPolling();
            hideAllPages();
            document.getElementById('maintenanceSection').classList.remove('hidden');
            if (calAuth) showMaintenanceMenu();
            else showMaintenanceLocked();
        }

        function showMaintenanceLocked() {
            setPageTitle('PAGE_MAINTENANCE');
            calView = 'locked';
            stopCalPolling();
            hideAllPages();
            document.getElementById('maintenanceSection').classList.remove('hidden');
            document.getElementById('calAuthPanel').classList.remove('hidden');
            document.getElementById('calMenuPanel').classList.add('hidden');
            const password = document.getElementById('calPassword');
            if (password) password.value = '';
            resetAccelCalUi();
            resetInstallCalUi();
        }

        function showMaintenanceMenu() {
            setPageTitle('PAGE_MAINTENANCE');
            calView = 'menu';
            stopCalPolling();
            hideAllPages();
            document.getElementById('maintenanceSection').classList.remove('hidden');
            document.getElementById('calAuthPanel').classList.add('hidden');
            document.getElementById('calMenuPanel').classList.remove('hidden');
            resetAccelCalUi();
            resetInstallCalUi();
            calStatus();
        }

        function openAccelCal() {
            setPageTitle('PAGE_RECORDER_CAL');
            if (!calAuth) { showMaintenanceLocked(); return; }
            calView = 'accel';
            stopCalPolling();
            resetAccelCalUi();
            resetInstallCalUi();
            hideAllPages();
            document.getElementById('accelCalPage').classList.remove('hidden');
            calStatus();
            startCalPolling();
            calSample();
        }

        function openInstallCal() {
            setPageTitle('PAGE_INSTALL_CAL');
            if (!calAuth) { showMaintenanceLocked(); return; }
            calView = 'install';
            stopCalPolling();
            resetAccelCalUi();
            resetInstallCalUi();
            hideAllPages();
            document.getElementById('installCalPage').classList.remove('hidden');
            calStatus();
            startCalPolling();
            installSample();
        }

        function openHealthPage() {
            setPageTitle('PAGE_ABOUT');
            if (!calAuth) { showMaintenanceLocked(); return; }
            stopCalPolling();
            hideAllPages();
            document.getElementById('healthSection').classList.remove('hidden');
            if (aboutAuth) showAboutMenu();
            else showAboutLocked();
        }

        function showAboutLocked() {
            document.getElementById('aboutAuthPanel').classList.remove('hidden');
            document.getElementById('aboutMenuPanel').classList.add('hidden');
            const password = document.getElementById('aboutPassword');
            if (password) password.value = '';
            const status = document.getElementById('aboutAuthStatus');
            if (status) status.textContent = tr('LOCKED');
        }

        function showAboutMenu() {
            document.getElementById('aboutAuthPanel').classList.add('hidden');
            document.getElementById('aboutMenuPanel').classList.remove('hidden');
            aboutStatus(tr('ABOUT_UNLOCKED'));
            updateWatchdogDiag();
        }

        function aboutUnlock() {
            const code = document.getElementById('aboutPassword').value || '';
            const status = document.getElementById('aboutAuthStatus');
            status.textContent = tr('CHECKING');
            fetch('/api/cal/support_auth?support=' + encodeURIComponent(code), { method: 'POST' })
                .then(r => r.json().then(j => ({ok:r.ok, status:r.status, json:j})))
                .then(res => {
                    handleCalibrationAuthError(res);
                    if (res.ok && res.json && res.json.ok) {
                        aboutAuth = true;
                        aboutSupportCode = code;
                        status.textContent = tr('UNLOCKED');
                        showAboutMenu();
                    } else {
                        aboutAuth = false;
                        aboutSupportCode = '';
                        status.textContent = tr('WRONG_SUPPORT_CODE');
                    }
                })
                .catch(err => {
                    aboutAuth = false;
                    aboutSupportCode = '';
                    status.textContent = localizedError('UNLOCK_ERROR', err);
                });
        }

        function openReportPage() {
            setPageTitle('PAGE_REPORTS');
            if (!calAuth) { showMaintenanceLocked(); return; }
            stopCalPolling();
            hideAllPages();
            document.getElementById('reportSection').classList.remove('hidden');
            refreshReports();
        }

        function openOtaPage() {
            setPageTitle('PAGE_FIRMWARE');
            if (!calAuth) { showMaintenanceLocked(); return; }
            stopCalPolling();
            hideAllPages();
            document.getElementById('otaSection').classList.remove('hidden');
            otaUpdateBridgeControls();
        }


        // -----------------------------------------------------------------
        // Maintenance authentication and common API helpers
        // -----------------------------------------------------------------
        function calUnlock() {
            const password = document.getElementById('calPassword').value || '';
            const status = document.getElementById('calAuthStatus');
            status.textContent = tr('CHECKING');
            fetch('/api/cal/auth?password=' + encodeURIComponent(password), { method: 'POST' })
                .then(r => r.json().then(j => ({ok:r.ok, json:j})))
                .then(res => {
                    if (res.ok && res.json && res.json.ok) {
                        calAuth = true;
                        status.textContent = tr('UNLOCKED');
                        showMaintenanceMenu();
                    } else {
                        calAuth = false;
                        status.textContent = tr('WRONG_MAINTENANCE_PASSWORD');
                    }
                })
                .catch(err => {
                    calAuth = false;
                    status.textContent = localizedError('UNLOCK_ERROR', err);
                });
        }

        function logCal(text) {
            const el = document.getElementById('calLog');
            if (!el) return;
            el.classList.remove('hidden');
            el.textContent = text;
        }

        function localizedError(key, err) {
            if (err) console.error(err);
            return tr(key);
        }

        // API/Bridge reason codes remain language-neutral. Only this browser
        // presentation layer converts them into recorder-selected language.
        function apiReasonText(reason, fallbackKey) {
            const code = String(reason || '');
            const map = {
                usb_required: 'USB_POWER_REQUIRED',
                bad_password: 'WRONG_MAINTENANCE_PASSWORD',
                bad_support_code: 'WRONG_SUPPORT_CODE',
                calibration_auth_required: 'MAINT_AUTH_REQUIRED',
                maintenance_auth_required: 'MAINT_AUTH_REQUIRED',
                sensor_cal_required: 'INSTALL_SENSOR_CAL_REQUIRED',
                recording: 'INSTALL_RECORDING_BLOCK',
                installation_reason_required: 'INSTALL_REASON_REQUIRED',
                no_installation_report: 'NO_INSTALL_REPORT_REG',
                no_valid_installation_report: 'NO_VALID_INSTALL_REPORT_REG',
                save_failed: 'GENERIC_OPERATION_ERROR',
                report_write_failed: 'GENERIC_OPERATION_ERROR',
                nvs_write_failed: 'GENERIC_OPERATION_ERROR',
                clear_failed: 'GENERIC_OPERATION_ERROR',
                status_failed: 'GENERIC_OPERATION_ERROR',
                sd_busy: 'GENERIC_OPERATION_ERROR',
                busy: 'GENERIC_OPERATION_ERROR',
                format_failed: 'GENERIC_OPERATION_ERROR',
                response_alloc: 'GENERIC_OPERATION_ERROR',
                invalid_file: 'GENERIC_OPERATION_ERROR',
                missing_parameter: 'GENERIC_OPERATION_ERROR',
                not_authorized: 'GENERIC_OPERATION_ERROR'
            };
            if (map[code]) return tr(map[code]);
            return fallbackKey ? tr(fallbackKey) : tr('GENERIC_OPERATION_ERROR');
        }

        // -----------------------------------------------------------------
        // Shared calibration-button state
        // Start buttons become Cancel while the matching calibration is active.
        // Warning color is handled separately by updateMaintenanceButtonState().
        // -----------------------------------------------------------------
        function setRecorderStartButton(active, busy) {
            const start = document.getElementById('btnStart');
            if (!start) return;
            start.disabled = !!busy;
            start.textContent = active ? tr('CANCEL') : tr('START');
            start.dataset.active = active ? '1' : '0';
            if (active) setButtonWarning('btnStart', false);
        }

        function setInstallStartButton(active, busy) {
            const start = document.getElementById('btnInstallStart');
            if (!start) return;
            start.disabled = !!busy;
            start.textContent = active ? tr('CANCEL') : tr('START');
            start.dataset.active = active ? '1' : '0';
            if (active) setButtonWarning('btnInstallStart', false);
        }

        function setCalSavedUi(saved) {
            const save = document.getElementById('btnSave');

            if (saved) {
                setRecorderStartButton(false, false);
                if (save) save.disabled = true;
                stopCalPolling();
            } else {
                if (save) save.disabled = false;
            }
        }

        // Clear only live measurement fields. Stored-calibration dates are refreshed by calStatus().
        function clearAccelCalMeasurementUi() {
            document.getElementById('calStatus').textContent = '-';
            document.getElementById('calSamplesProcessed').textContent = '0';
            document.getElementById('calSensorTemp').textContent = '-';
            document.getElementById('calCurrentNoise').textContent = '-';
            document.getElementById('calLowestNoise').textContent = '-';
            document.getElementById('calFaceTime').textContent = '-';
            calFaceBestUpdateSeen = [0, 0, 0, 0, 0, 0];
            updateFaceSummary(null, -1, false);
            for (let i = 0; i < 3; i++) {
                document.getElementById('resGain' + i).textContent = '-';
                document.getElementById('resOffset' + i).textContent = '-';
                document.getElementById('nvsGain' + i).textContent = '-';
                document.getElementById('nvsOffset' + i).textContent = '-';
            }
        }

        function handleCalibrationAuthError(res) {
            if (res && res.status === 403 && res.json && res.json.reason === 'calibration_auth_required') {
                calAuth = false;
                aboutAuth = false;
                aboutSupportCode = '';
                showMaintenanceLocked();
            }
        }

        // POST helper used by maintenance actions that return JSON and may fail if auth expired.
        function postJson(url) {
            return fetch(url, { method: 'POST' }).then(r => r.json().then(j => {
                const res = {ok:r.ok, status:r.status, json:j};
                handleCalibrationAuthError(res);
                return res;
            }));
        }

)rawliteral"
