/*******************************************************************************
 * SCRIPT: OTA UPLOAD AND PAGE STARTUP
 * Firmware upload handler, periodic refresh setup, initial page display, and closing HTML tags.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        function otaUpload() {
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

        setInterval(updateStatus, 5000);
        setInterval(calStatus, 5000);

        showHome();
    </script>
</body>
</html>
)rawliteral"
