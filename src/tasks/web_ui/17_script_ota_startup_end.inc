/*******************************************************************************
 * SCRIPT: OTA UPLOAD AND PAGE STARTUP
 * Firmware upload handler, periodic refresh setup, initial page display, and closing HTML tags.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        // Upload one firmware image to the recorder and keep the user informed during transfer.
        function otaUpload() {
            const fileInput = document.getElementById('otaFile');
            const status = document.getElementById('otaStatus');
            const button = document.getElementById('btnOtaUpload');

            if (!fileInput.files || fileInput.files.length === 0) {
                status.textContent = 'Select a firmware .bin file first.';
                return;
            }

            const file = fileInput.files[0];
            if (!file.name.endsWith('.bin')) {
                status.textContent = 'Use a recorder firmware .bin file named like SLM_recorder_date_version.bin.';
                return;
            }

            const form = new FormData();
            form.append('firmware', file, file.name);

            button.disabled = true;
            status.textContent = 'Uploading firmware: 0%. Do not disconnect USB power.';

            const xhr = new XMLHttpRequest();

            xhr.upload.onprogress = function(evt) {
                if (evt.lengthComputable) {
                    const pct = Math.round((evt.loaded * 100) / evt.total);
                    status.textContent = 'Uploading firmware: ' + pct + '%. Do not disconnect USB power.';
                } else {
                    status.textContent = 'Uploading firmware. Do not disconnect USB power.';
                }
            };

            xhr.onload = function() {
                status.textContent = xhr.responseText || 'Upload complete.';
                if (xhr.status === 403) {
                    calAuth = false;
                    status.textContent = 'Maintenance authorization required.';
                    button.disabled = false;
                    showMaintenanceLocked();
                    return;
                }
                if (xhr.status < 200 || xhr.status >= 300) {
                    button.disabled = false;
                }
            };

            xhr.onerror = function() {
                status.textContent = 'Firmware update failed.';
                button.disabled = false;
            };

            xhr.open('POST', '/api/ota');
            xhr.send(form);
        }

        let otaServerFirmwareFiles = [];

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

        function otaUpdateBridgeControls() {
            const available = otaBridgeFirmwareAvailable();
            const listButton = document.getElementById('btnOtaServerList');
            const installButton = document.getElementById('btnOtaServerInstall');
            const installControls = document.getElementById('otaServerInstallControls');
            const select = document.getElementById('otaServerFirmwareSelect');
            const status = document.getElementById('otaServerStatus');
            if (!listButton || !status) return;

            listButton.disabled = !available;
            if (!available) {
                otaServerFirmwareFiles = [];
                if (select) {
                    select.innerHTML = '';
                    select.classList.add('hidden');
                }
                if (installControls) installControls.classList.add('hidden');
                if (installButton) installButton.disabled = true;
                status.textContent = 'Server firmware requires SLM Bridge.';
            } else if (otaServerFirmwareFiles.length === 0) {
                status.textContent = 'Ready to search server firmware.';
            }
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

        function otaSetServerBusy(busy) {
            const listButton = document.getElementById('btnOtaServerList');
            const installButton = document.getElementById('btnOtaServerInstall');
            if (listButton) listButton.disabled = busy || !otaBridgeFirmwareAvailable();
            if (installButton) installButton.disabled = busy || otaServerFirmwareFiles.length === 0;
        }

        function otaListServerFirmware() {
            const status = document.getElementById('otaServerStatus');
            if (!otaBridgeFirmwareAvailable()) {
                status.textContent = 'Server firmware requires SLM Bridge.';
                return;
            }
            otaServerFirmwareFiles = [];
            const select = document.getElementById('otaServerFirmwareSelect');
            const installControls = document.getElementById('otaServerInstallControls');
            if (select) {
                select.innerHTML = '';
                select.classList.add('hidden');
            }
            if (installControls) installControls.classList.add('hidden');
            status.textContent = 'Searching firmware from server...';
            otaSetServerBusy(true);
            try {
                window.SLMAndroid.listServerFirmware();
            } catch (err) {
                status.textContent = 'Bridge request failed: ' + err;
                otaSetServerBusy(false);
            }
        }

        function otaInstallServerFirmware() {
            const status = document.getElementById('otaServerStatus');
            const select = document.getElementById('otaServerFirmwareSelect');
            const index = select ? Number(select.value) : -1;
            if (!otaBridgeFirmwareAvailable()) {
                status.textContent = 'Server firmware requires SLM Bridge.';
                return;
            }
            if (!Number.isInteger(index) || index < 0 || index >= otaServerFirmwareFiles.length) {
                status.textContent = 'Select a firmware file first.';
                return;
            }
            status.textContent = 'Fetching selected firmware from server...';
            otaSetServerBusy(true);
            try {
                window.SLMAndroid.installServerFirmware(JSON.stringify(otaServerFirmwareFiles[index]));
            } catch (err) {
                status.textContent = 'Bridge request failed: ' + err;
                otaSetServerBusy(false);
            }
        }

        window.addEventListener('slm-firmware-event', function(event) {
            const detail = event && event.detail ? event.detail : {};
            const state = String(detail.state || '');
            const percent = Math.max(0, Math.min(100, Number(detail.percent) || 0));
            const message = String(detail.message || '');
            const status = document.getElementById('otaServerStatus');
            const select = document.getElementById('otaServerFirmwareSelect');
            const installControls = document.getElementById('otaServerInstallControls');

            if (state === 'listing') {
                status.textContent = message || 'Searching firmware from server...';
                otaSetServerBusy(true);
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
                if (installControls) {
                    installControls.classList.toggle('hidden', otaServerFirmwareFiles.length === 0);
                }
                status.textContent = otaServerFirmwareFiles.length +
                    ' firmware file(s) found. Select one and upload.';
                otaSetServerBusy(false);
            } else if (state === 'downloading') {
                status.textContent = (message || 'Downloading firmware from server') +
                    (percent > 0 ? ': ' + percent + '%' : '...');
                otaSetServerBusy(true);
            } else if (state === 'downloaded') {
                status.textContent = message || 'Firmware downloaded from server.';
                otaSetServerBusy(true);
            } else if (state === 'uploading') {
                status.textContent = (message || 'Uploading firmware to recorder') +
                    (percent > 0 ? ': ' + percent + '%' : '...') +
                    '. Do not disconnect USB power.';
                otaSetServerBusy(true);
            } else if (state === 'complete') {
                status.textContent = message || 'Firmware update OK. Rebooting...';
                otaSetServerBusy(false);
            } else if (state === 'busy') {
                status.textContent = message || 'Firmware operation already running.';
            } else if (state === 'failed') {
                status.textContent = 'Firmware from server failed: ' + (message || 'unknown error');
                otaSetServerBusy(false);
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
