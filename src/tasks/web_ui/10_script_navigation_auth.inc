/*******************************************************************************
 * SCRIPT: PAGE NAVIGATION AND MAINTENANCE AUTHENTICATION
 * JavaScript state variables, page switching, maintenance unlock, and common API helper functions.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(    <script>
        let allFiles = [];
        let processedFiles = [];
        let calPoll = null;
        let calAuth = false;
        let calView = 'menu';
        let calLastBestLocalMs = 0;
        let calLastBestLocalUpdates = 0;
        let calLastBestLocalFace = -1;

        function setPageTitle(title) {
            document.getElementById('pageTitle').textContent = title;
        }

        function hideAllPages() {
            document.getElementById('homeSection').classList.add('hidden');
            document.getElementById('filesSection').classList.add('hidden');
            document.getElementById('maintenanceSection').classList.add('hidden');
            document.getElementById('accelCalPage').classList.add('hidden');
            document.getElementById('installCalPage').classList.add('hidden');
            document.getElementById('healthSection').classList.add('hidden');
            document.getElementById('otaSection').classList.add('hidden');
            document.getElementById('deleteSection').classList.add('hidden');
        }

        function showHome() {
            setPageTitle('SLM Recorder');
            stopCalPolling();
            hideAllPages();
            document.getElementById('homeSection').classList.remove('hidden');
            updateStatus();
            calStatus();
        }

        function openFilesPage() {
            setPageTitle('SLM File Management');
            stopCalPolling();
            hideAllPages();
            document.getElementById('filesSection').classList.remove('hidden');
            refreshFiles();
        }

        function openMaintenancePage() {
            setPageTitle('SLM Maintenance');
            stopCalPolling();
            hideAllPages();
            document.getElementById('maintenanceSection').classList.remove('hidden');
            if (calAuth) showMaintenanceMenu();
            else showMaintenanceLocked();
        }

        function showMaintenanceLocked() {
            setPageTitle('SLM Maintenance');
            calView = 'locked';
            stopCalPolling();
            hideAllPages();
            document.getElementById('maintenanceSection').classList.remove('hidden');
            document.getElementById('calAuthPanel').classList.remove('hidden');
            document.getElementById('calMenuPanel').classList.add('hidden');
            resetAccelCalUi();
            resetInstallCalUi();
        }

        function showMaintenanceMenu() {
            setPageTitle('SLM Maintenance');
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
            setPageTitle('SLM Recorder Calibration');
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
            setPageTitle('SLM Installation Calibration');
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
            setPageTitle('SLM Health');
            if (!calAuth) { showMaintenanceLocked(); return; }
            stopCalPolling();
            hideAllPages();
            document.getElementById('healthSection').classList.remove('hidden');
            updateWatchdogDiag();
        }

        function openOtaPage() {
            setPageTitle('SLM Firmware Update');
            if (!calAuth) { showMaintenanceLocked(); return; }
            stopCalPolling();
            hideAllPages();
            document.getElementById('otaSection').classList.remove('hidden');
        }

        function openDeletePage() {
            setPageTitle('SLM Delete');
            if (!calAuth) { showMaintenanceLocked(); return; }
            stopCalPolling();
            hideAllPages();
            document.getElementById('deleteSection').classList.remove('hidden');
            refreshProcessedFiles();
        }

        function calUnlock() {
            const password = document.getElementById('calPassword').value || '';
            const status = document.getElementById('calAuthStatus');
            status.textContent = 'Checking...';
            fetch('/api/cal/auth?password=' + encodeURIComponent(password), { method: 'POST' })
                .then(r => r.json().then(j => ({ok:r.ok, json:j})))
                .then(res => {
                    if (res.ok && res.json && res.json.ok) {
                        calAuth = true;
                        status.textContent = 'Unlocked.';
                        showMaintenanceMenu();
                    } else {
                        calAuth = false;
                        status.textContent = 'Wrong registration or calibration access denied.';
                    }
                })
                .catch(err => {
                    calAuth = false;
                    status.textContent = 'Unlock error: ' + err;
                });
        }

        function logCal(text) {
            document.getElementById('calLog').textContent = text;
        }

        function prettyJson(obj) {
            return JSON.stringify(obj, null, 2);
        }

        function setCalSavedUi(saved) {
            const save = document.getElementById('btnSave');

            if (saved) {
                save.disabled = true;
                stopCalPolling();
            } else {
                save.disabled = false;
            }
        }

        function handleCalibrationAuthError(res) {
            if (res && res.status === 403 && res.json && res.json.reason === 'calibration_auth_required') {
                calAuth = false;
                showMaintenanceLocked();
            }
        }

        function postJson(url) {
            return fetch(url, { method: 'POST' }).then(r => r.json().then(j => {
                const res = {ok:r.ok, status:r.status, json:j};
                handleCalibrationAuthError(res);
                return res;
            }));
        }

)rawliteral"
