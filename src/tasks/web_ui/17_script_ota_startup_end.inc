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
                    if (status) status.textContent = tr('SELECT_FW_SOURCE');
                }
                serverStatus.textContent = tr('SERVER_FIRMWARE_REQUIRES_BRIDGE');
            } else if (otaServerFirmwareFiles.length === 0) {
                serverStatus.textContent = tr('READY_SELECT_SERVER_FW');
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
            const source = file.sourcePath ? String(file.sourcePath) : tr('SERVER');
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
                if (status) status.textContent = tr('SELECT_FW_SOURCE');
                otaRefreshControls();
                return;
            }

            if (fileInput) fileInput.value = '';
            otaSelectedFirmwareSource = 'server';
            if (status) {
                status.textContent = tr('SELECTED_FROM_SERVER') + ': ' + String(otaServerFirmwareFiles[index].name || tr('FIRMWARE_GENERIC'));
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
                if (status) status.textContent = tr('SELECT_FW_SOURCE');
                otaRefreshControls();
                return;
            }

            if (select) select.selectedIndex = -1;
            otaSelectedFirmwareSource = 'phone';
            if (status) status.textContent = tr('SELECTED_FROM_PHONE') + ': ' + file.name;
            otaRefreshControls();
        }

        function otaListServerFirmware() {
            const serverStatus = document.getElementById('otaServerStatus');
            const select = document.getElementById('otaServerFirmwareSelect');
            if (!otaBridgeFirmwareAvailable()) {
                serverStatus.textContent = tr('SERVER_FIRMWARE_REQUIRES_BRIDGE');
                return;
            }

            otaServerFirmwareFiles = [];
            if (otaSelectedFirmwareSource === 'server') otaSelectedFirmwareSource = '';
            if (select) {
                select.innerHTML = '';
                select.classList.add('hidden');
            }
            serverStatus.textContent = tr('SEARCHING_SERVER_FW');
            otaSetBusy(true);
            try {
                window.SLMAndroid.listServerFirmware();
            } catch (err) {
                serverStatus.textContent = localizedError('BRIDGE_REQUEST_FAILED', err);
                otaSetBusy(false);
            }
        }

        function otaInstallServerFirmware() {
            const status = document.getElementById('otaStatus');
            const select = document.getElementById('otaServerFirmwareSelect');
            const index = select ? Number(select.value) : -1;
            if (!otaBridgeFirmwareAvailable()) {
                status.textContent = tr('SERVER_FIRMWARE_REQUIRES_BRIDGE');
                return;
            }
            if (!Number.isInteger(index) || index < 0 || index >= otaServerFirmwareFiles.length) {
                status.textContent = tr('SELECT_SERVER_FW_FIRST');
                return;
            }

            status.textContent = tr('FETCHING_SERVER_FW');
            otaSetBusy(true);
            try {
                window.SLMAndroid.installServerFirmware(JSON.stringify(otaServerFirmwareFiles[index]));
            } catch (err) {
                status.textContent = localizedError('BRIDGE_REQUEST_FAILED', err);
                otaSetBusy(false);
            }
        }

        function otaFailureText(code) {
            const keyByCode = {
                maintenance_auth_required:'MAINT_AUTH_REQUIRED',
                usb_required:'OTA_USB_REQUIRED',
                application_bin_required:'FW_BIN_REQUIRED',
                update_begin_failed:'FW_BEGIN_FAILED',
                update_write_failed:'FW_WRITE_FAILED',
                update_end_failed:'FW_END_FAILED',
                ota_failed:'FW_UPDATE_FAILED'
            };
            const key = keyByCode[String(code || '').trim()];
            return key ? tr(key) : tr('FW_UPDATE_FAILED');
        }

        // Upload one phone-selected firmware image to the recorder and keep the user informed during transfer.
        function otaUploadLocalFirmware() {
            const fileInput = document.getElementById('otaFile');
            const status = document.getElementById('otaStatus');

            if (!fileInput.files || fileInput.files.length === 0) {
                status.textContent = tr('SELECT_PHONE_BIN_FIRST');
                return;
            }

            const file = fileInput.files[0];
            if (!/\.bin$/i.test(file.name)) {
                status.textContent = tr('PHONE_FILE_MUST_BIN');
                return;
            }

            const form = new FormData();
            form.append('firmware', file, file.name);

            otaSetBusy(true);
            status.textContent = tr('UPDATING_FW_PREFIX') + ': 0%. ' + tr('DO_NOT_DISCONNECT_USB');

            const xhr = new XMLHttpRequest();
            xhr.upload.onprogress = function(evt) {
                if (evt.lengthComputable) {
                    const pct = Math.round((evt.loaded * 100) / evt.total);
                    status.textContent = tr('UPDATING_FW_PREFIX') + ': ' + pct + '%. ' + tr('DO_NOT_DISCONNECT_USB');
                } else {
                    status.textContent = tr('UPDATING_FW_PREFIX') + '. ' + tr('DO_NOT_DISCONNECT_USB');
                }
            };

            xhr.onload = function() {
                if (xhr.status === 403) {
                    calAuth = false;
                    status.textContent = tr('MAINT_AUTH_REQUIRED');
                    otaSetBusy(false);
                    showMaintenanceLocked();
                    return;
                }
                if (xhr.status >= 200 && xhr.status < 300) {
                    status.textContent = tr('FW_UPDATE_OK_RESTART');
                    return;
                }
                status.textContent = otaFailureText(xhr.responseText) + ' HTTP ' + xhr.status;
                otaSetBusy(false);
            };

            xhr.onerror = function() {
                status.textContent = tr('FW_UPDATE_FAILED');
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
                status.textContent = tr('SELECT_FW_SOURCE_FIRST');
            }
        }

        window.addEventListener('slm-firmware-event', function(event) {
            const detail = event && event.detail ? event.detail : {};
            const state = String(detail.state || '');
            const percent = Math.max(0, Math.min(100, Number(detail.percent) || 0));
            const status = document.getElementById('otaStatus');
            const serverStatus = document.getElementById('otaServerStatus');
            const select = document.getElementById('otaServerFirmwareSelect');

            if (state === 'listing') {
                serverStatus.textContent = tr('SEARCHING_SERVER_FW');
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
                    (otaServerFirmwareFiles.length + ' ' + tr('FW_FILES_FOUND_SUFFIX')) :
                    tr('NO_FW_SERVER');
                otaSetBusy(false);
                if (otaServerFirmwareFiles.length && select) {
                    select.selectedIndex = 0;
                    otaServerSelectionChanged();
                }
            } else if (state === 'downloading') {
                status.textContent = tr('DOWNLOADING_FW_SERVER') +
                    (percent > 0 ? ': ' + percent + '%' : '...');
                otaSetBusy(true);
            } else if (state === 'downloaded') {
                status.textContent = tr('FW_DOWNLOADED_SERVER');
                otaSetBusy(true);
            } else if (state === 'uploading') {
                status.textContent = tr('UPDATING_RECORDER_FW') +
                    (percent > 0 ? ': ' + percent + '%' : '...') +
                    '. ' + tr('DO_NOT_DISCONNECT_USB');
                otaSetBusy(true);
            } else if (state === 'complete') {
                status.textContent = tr('FW_UPDATE_OK_RESTART') + ' ' + tr('RESTART_WIFI_BEFORE_RECONNECT');
                otaSetBusy(true);
            } else if (state === 'busy') {
                status.textContent = tr('FW_OPERATION_BUSY');
                otaSetBusy(true);
            } else if (state === 'failed') {
                status.textContent = otaFailureText(detail.reason || detail.code || 'ota_failed');
                otaSetBusy(false);
            }
        });

        // Low-rate background refresh keeps badges and required-action colors current
        // even when the user is not on a live calibration page.
        setInterval(updateStatus, 5000);
        setInterval(calStatus, 5000);

        applyStaticTranslations();
        showHome();
    </script>
</body>
</html>
)rawliteral"
