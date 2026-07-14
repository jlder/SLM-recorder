/*******************************************************************************
 * SCRIPT: FORMAT HELPERS
 * Formatting helpers for numbers, clocks, durations, and simple utility functions.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(
        // Small numeric helpers shared by the browser-side analysis pipeline.
        function maxIndexInRange(data, start, end) {
            let bestIdx = -1;
            let bestVal = -Infinity;

            for (let i = start; i < end; i++) {
                if (data[i] > bestVal) {
                    bestVal = data[i];
                    bestIdx = i;
                }
            }

            return bestIdx;
        }

        function arrayMin(data) {
            return data.reduce((a, b) => Math.min(a, b), Infinity);
        }

        function arrayMax(data) {
            return data.reduce((a, b) => Math.max(a, b), -Infinity);
        }

        // Clock formatting wraps recorder milliseconds at 24 h because records store time-of-day.
        function formatClockMs(absMs) {
            let totalSeconds = Math.round(absMs / 1000.0);
            totalSeconds = ((totalSeconds % 86400) + 86400) % 86400;
            const h = Math.floor(totalSeconds / 3600);
            const m = Math.floor((totalSeconds % 3600) / 60);
            const s = totalSeconds % 60;
            return pad2(h) + ':' + pad2(m) + ':' + pad2(s);
        }

        function formatClockHm(absMs) {
            let totalSeconds = Math.round(absMs / 1000.0);
            totalSeconds = ((totalSeconds % 86400) + 86400) % 86400;
            const h = Math.floor(totalSeconds / 3600);
            const m = Math.floor((totalSeconds % 3600) / 60);
            return pad2(h) + ':' + pad2(m);
        }

        // Round detected flight boundary times to the closest displayed minute.
        // Flight duration shown as HH:MM is then computed from these same
        // rounded boundaries so the table remains arithmetically consistent.
        function roundMsToNearestMinute(absMs) {
            return Math.round((Number(absMs) || 0) / 60000.0) * 60000.0;
        }

        // Duration formatting does not wrap at 24 h.
        function formatDurationMs(ms) {
            let totalSeconds = Math.round(ms / 1000.0);
            const h = Math.floor(totalSeconds / 3600);
            const m = Math.floor((totalSeconds % 3600) / 60);
            const s = totalSeconds % 60;
            return pad2(h) + ':' + pad2(m) + ':' + pad2(s);
        }

        function formatDurationHm(ms) {
            let totalSeconds = Math.round(ms / 1000.0);
            const h = Math.floor(totalSeconds / 3600);
            const m = Math.floor((totalSeconds % 3600) / 60);
            return pad2(h) + ':' + pad2(m);
        }

        function formatDurationSeconds(ms) {
            return String(Math.round((Number(ms) || 0) / 1000.0));
        }

        function pad2(v) {
            return String(v).padStart(2, '0');
        }

)rawliteral"
