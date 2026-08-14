/*******************************************************************************
 * SCRIPT: OTA UPLOAD AND PAGE STARTUP
 * Firmware upload handler, periodic refresh setup, initial page display, and closing HTML tags.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        let otaServerFirmwareFiles = [];
        let otaSelectedFirmwareSource = '';
        let otaFirmwareBusy = false;

        function otaBridgeFirmwareAvailable() {
            if (!window.SLMAndroid ||
                typeof window.SLMAndroid.getCapabilities !== 'function' ||
                typeof window.SLMAndroid.listServerFirmware !== 'function' ||
                typeof window.SLMAndroid.installServerFirmware !== 'function') {
                return false;
            }
            try {
                const capabilities = JSON.parse(window.SLMAndroid.getCapabilities());
                return capabilities && Array.isArray(capabilities.features) &&
                    capabilities.features.indexOf('server-firmware') >= 0;
            } catch (err) {
                return false;
            }
        }

        function otaRefreshControls() {
            const listButton = document.getElementById('btnOtaServerList');
            const updateButton = document.getElementById('btnOtaUpdate');
            const fileInput = document.getElementById('otaFile');
            const select = document.getElementById('otaServerFirmwareSelect');

            if (listButton) listButton.disabled = otaFirmwareBusy || !otaBridgeFirmwareAvailable();
            if (updateButton) updateButton.disabled = otaFirmwareBusy || !otaSelectedFirmwareSource;
            if (fileInput) fileInput.disabled = otaFirmwareBusy;
            if (select) select.disabled = otaFirmwareBusy;
        }

        function otaSetBusy(busy) {
            otaFirmwareBusy = !!busy;
            otaRefreshControls();
        }

        function otaUpdateBridgeControls() {
            const available = otaBridgeFirmwareAvailable();
            const select = document.getElementById('otaServerFirmwareSelect');
            const serverStatus = document.getElementById('otaServerStatus');
            if (!serverStatus) return;

            if (!available) {
                otaServerFirmwareFiles = [];
                if (select) {
                    select.innerHTML = '';
                    select.classList.add('hidden');
                }
                if (otaSelectedFirmwareSource === 'server') {
                    otaSelectedFirmwareSource = '';
                    const status = document.getElementById('otaStatus');
                    if (status) status.textContent = 'Select firmware from server or phone.';
                }
                serverStatus.textContent = 'Server firmware requires SLM Bridge.';
            } else if (otaServerFirmwareFiles.length === 0) {
                serverStatus.textContent = 'Ready to select firmware from server.';
            }
            otaRefreshControls();
        }

        function otaFormatBytes(bytes) {
            const b = Number(bytes) || 0;
            if (b >= 1000000) return (b / 1000000).toFixed(1) + ' MB';
            if (b >= 1000) return (b / 1000).toFixed(0) + ' kB';
            return b + ' B';
        }

        function otaFirmwareOptionText(file) {
            const source = file.sourcePath ? String(file.sourcePath) : 'server';
            const modified = file.modifiedTime ? String(file.modifiedTime).substring(0, 10) : '-';
            return String(file.name || '') + ' — ' + source + ' — ' +
                otaFormatBytes(file.size) + ' — ' + modified;
        }

        function otaServerSelectionChanged() {
            const select = document.getElementById('otaServerFirmwareSelect');
            const fileInput = document.getElementById('otaFile');
            const status = document.getElementById('otaStatus');
            const index = select ? Number(select.value) : -1;

            if (!Number.isInteger(index) || index < 0 || index >= otaServerFirmwareFiles.length) {
                if (otaSelectedFirmwareSource === 'server') otaSelectedFirmwareSource = '';
                if (status) status.textContent = 'Select firmware from server or phone.';
                otaRefreshControls();
                return;
            }

            if (fileInput) fileInput.value = '';
            otaSelectedFirmwareSource = 'server';
            if (status) {
                status.textContent = 'Selected from server: ' + String(otaServerFirmwareFiles[index].name || 'firmware');
            }
            otaRefreshControls();
        }

        function otaLocalSelectionChanged() {
            const fileInput = document.getElementById('otaFile');
            const select = document.getElementById('otaServerFirmwareSelect');
            const status = document.getElementById('otaStatus');
            const file = fileInput && fileInput.files && fileInput.files.length ? fileInput.files[0] : null;

            if (!file) {
                if (otaSelectedFirmwareSource === 'phone') otaSelectedFirmwareSource = '';
                if (status) status.textContent = 'Select firmware from server or phone.';
                otaRefreshControls();
                return;
            }

            if (select) select.selectedIndex = -1;
            otaSelectedFirmwareSource = 'phone';
            if (status) status.textContent = 'Selected from phone: ' + file.name;
            otaRefreshControls();
        }

        function otaListServerFirmware() {
            const serverStatus = document.getElementById('otaServerStatus');
            const select = document.getElementById('otaServerFirmwareSelect');
            if (!otaBridgeFirmwareAvailable()) {
                serverStatus.textContent = 'Server firmware requires SLM Bridge.';
                return;
            }

            otaServerFirmwareFiles = [];
            if (otaSelectedFirmwareSource === 'server') otaSelectedFirmwareSource = '';
            if (select) {
                select.innerHTML = '';
                select.classList.add('hidden');
            }
            serverStatus.textContent = 'Searching firmware from server...';
            otaSetBusy(true);
            try {
                window.SLMAndroid.listServerFirmware();
            } catch (err) {
                serverStatus.textContent = 'Bridge request failed: ' + err;
                otaSetBusy(false);
            }
        }

        function otaInstallServerFirmware() {
            const status = document.getElementById('otaStatus');
            const select = document.getElementById('otaServerFirmwareSelect');
            const index = select ? Number(select.value) : -1;
            if (!otaBridgeFirmwareAvailable()) {
                status.textContent = 'Server firmware requires SLM Bridge.';
                return;
            }
            if (!Number.isInteger(index) || index < 0 || index >= otaServerFirmwareFiles.length) {
                status.textContent = 'Select a firmware file from server first.';
                return;
            }

            status.textContent = 'Fetching selected firmware from server...';
            otaSetBusy(true);
            try {
                window.SLMAndroid.installServerFirmware(JSON.stringify(otaServerFirmwareFiles[index]));
            } catch (err) {
                status.textContent = 'Bridge request failed: ' + err;
                otaSetBusy(false);
            }
        }

        // Upload one phone-selected firmware image to the recorder and keep the user informed during transfer.
        function otaUploadLocalFirmware() {
            const fileInput = document.getElementById('otaFile');
            const status = document.getElementById('otaStatus');

            if (!fileInput.files || fileInput.files.length === 0) {
                status.textContent = 'Select a firmware .bin file from phone first.';
                return;
            }

            const file = fileInput.files[0];
            if (!/\.bin$/i.test(file.name)) {
                status.textContent = 'The selected phone file must be a recorder firmware .bin file.';
                return;
            }

            const form = new FormData();
            form.append('firmware', file, file.name);

            otaSetBusy(true);
            status.textContent = 'Updating firmware: 0%. Do not disconnect USB power.';

            const xhr = new XMLHttpRequest();
            xhr.upload.onprogress = function(evt) {
                if (evt.lengthComputable) {
                    const pct = Math.round((evt.loaded * 100) / evt.total);
                    status.textContent = 'Updating firmware: ' + pct + '%. Do not disconnect USB power.';
                } else {
                    status.textContent = 'Updating firmware. Do not disconnect USB power.';
                }
            };

            xhr.onload = function() {
                if (xhr.status === 403) {
                    calAuth = false;
                    status.textContent = 'Maintenance authorization required.';
                    otaSetBusy(false);
                    showMaintenanceLocked();
                    return;
                }
                if (xhr.status >= 200 && xhr.status < 300) {
                    status.textContent = xhr.responseText || 'Firmware update OK. Recorder restarting.';
                    return;
                }
                status.textContent = xhr.responseText || ('Firmware update failed: HTTP ' + xhr.status);
                otaSetBusy(false);
            };

            xhr.onerror = function() {
                status.textContent = 'Firmware update failed.';
                otaSetBusy(false);
            };

            xhr.open('POST', '/api/ota');
            xhr.send(form);
        }

        function otaUpdateSelected() {
            const status = document.getElementById('otaStatus');
            if (otaSelectedFirmwareSource === 'server') {
                otaInstallServerFirmware();
            } else if (otaSelectedFirmwareSource === 'phone') {
                otaUploadLocalFirmware();
            } else if (status) {
                status.textContent = 'Select firmware from server or phone first.';
            }
        }

        window.addEventListener('slm-firmware-event', function(event) {
            const detail = event && event.detail ? event.detail : {};
            const state = String(detail.state || '');
            const percent = Math.max(0, Math.min(100, Number(detail.percent) || 0));
            const message = String(detail.message || '');
            const status = document.getElementById('otaStatus');
            const serverStatus = document.getElementById('otaServerStatus');
            const select = document.getElementById('otaServerFirmwareSelect');

            if (state === 'listing') {
                serverStatus.textContent = message || 'Searching firmware from server...';
                otaSetBusy(true);
            } else if (state === 'list') {
                otaServerFirmwareFiles = Array.isArray(detail.files) ? detail.files : [];
                if (select) {
                    select.innerHTML = '';
                    otaServerFirmwareFiles.forEach(function(file, index) {
                        const option = document.createElement('option');
                        option.value = String(index);
                        option.textContent = otaFirmwareOptionText(file);
                        select.appendChild(option);
                    });
                    select.classList.toggle('hidden', otaServerFirmwareFiles.length === 0);
                }
                serverStatus.textContent = otaServerFirmwareFiles.length ?
                    (otaServerFirmwareFiles.length + ' firmware file(s) found.') :
                    'No firmware file found on server.';
                otaSetBusy(false);
                if (otaServerFirmwareFiles.length && select) {
                    select.selectedIndex = 0;
                    otaServerSelectionChanged();
                }
            } else if (state === 'downloading') {
                status.textContent = (message || 'Downloading firmware from server') +
                    (percent > 0 ? ': ' + percent + '%' : '...');
                otaSetBusy(true);
            } else if (state === 'downloaded') {
                status.textContent = message || 'Firmware downloaded from server.';
                otaSetBusy(true);
            } else if (state === 'uploading') {
                status.textContent = (message || 'Updating recorder firmware') +
                    (percent > 0 ? ': ' + percent + '%' : '...') +
                    '. Do not disconnect USB power.';
                otaSetBusy(true);
            } else if (state === 'complete') {
                status.textContent = message || 'Firmware update OK. Recorder restarting. Restart the Wi-Fi connection before reconnecting.';
                otaSetBusy(true);
            } else if (state === 'busy') {
                status.textContent = message || 'Firmware operation already running.';
                otaSetBusy(true);
            } else if (state === 'failed') {
                status.textContent = 'Firmware update failed: ' + (message || 'unknown error');
                otaSetBusy(false);
            }
        });

        // Low-rate background refresh keeps badges and required-action colors current
        // even when the user is not on a live calibration page.
        setInterval(updateStatus, 5000);
        setInterval(calStatus, 5000);

        showHome();
    </script>
</body>
</html>
)rawliteral"
