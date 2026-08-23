/*******************************************************************************
 * SCRIPT: FLIGHT-ANALYSIS CONSTANTS AND DECODERS
 * Constants and decoders for SLM 0x70 records and supported WIT packets.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        // Browser analysis constants. Values are generated from config.h so the
        // validated flight-time extraction parameters are controlled in one place.
        const SLM_ANALYSIS_FS = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_FS_HZ) R"rawliteral(;
        const SLM_ANALYSIS_HIRMS_WINDOW_S = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_HIRMS_WINDOW_S) R"rawliteral(;
        const SLM_ANALYSIS_LOWRMS_WINDOW_S = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_LOWRMS_WINDOW_S) R"rawliteral(;
        const SLM_ANALYSIS_FLIGHTGROUND_LPF_PERIOD_S = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_FLIGHTGROUND_LPF_PERIOD_S) R"rawliteral(;
        const SLM_ANALYSIS_FLIGHTGROUND_THRESHOLD = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_FLIGHTGROUND_THRESHOLD) R"rawliteral(;
        const SLM_ANALYSIS_FLIGHTGROUND_HYSTERESIS = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_FLIGHTGROUND_HYSTERESIS) R"rawliteral(;
        const SLM_ANALYSIS_SEARCH_WINDOW_S = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_SEARCH_WINDOW_S) R"rawliteral(;
        const SLM_ANALYSIS_TO_ROLL_START_THR = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_TO_ROLL_START_THR) R"rawliteral(;
        const SLM_ANALYSIS_TO_ROLL_END_THR = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_TO_ROLL_END_THR) R"rawliteral(;
        const SLM_ANALYSIS_LDG_ROLL_START_THR = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_LDG_ROLL_START_THR) R"rawliteral(;
        const SLM_ANALYSIS_LDG_ROLL_END_THR = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_LDG_ROLL_END_THR) R"rawliteral(;
        const SLM_ANALYSIS_MIN_FILE_S = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_MIN_FILE_S) R"rawliteral(;
        const SLM_ANALYSIS_BUTTER_Q = [)rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_BUTTER_Q1) R"rawliteral(, )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_BUTTER_Q2) R"rawliteral(];
        const SLM_ANALYSIS_LDG_HIRMS_GATE_ENABLED = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_LDG_HIRMS_GATE_ENABLED) R"rawliteral(;
        const SLM_ANALYSIS_LDG_HIRMS_PEAK_MIN_NORM = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_LDG_HIRMS_PEAK_MIN_NORM) R"rawliteral(;
        const SLM_ANALYSIS_LDG_HIRMS_PEAK_MAX_AGE_S = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_LDG_HIRMS_PEAK_MAX_AGE_S) R"rawliteral(;
        const SLM_ANALYSIS_TO_HIRMS_GATE_ENABLED = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_TO_HIRMS_GATE_ENABLED) R"rawliteral(;
        const SLM_ANALYSIS_TO_HIRMS_PEAK_MIN_NORM = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_TO_HIRMS_PEAK_MIN_NORM) R"rawliteral(;
        const SLM_ANALYSIS_TO_HIRMS_PEAK_MAX_AGE_S = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_TO_HIRMS_PEAK_MAX_AGE_S) R"rawliteral(;
        const SLM_ANALYSIS_TO_STARTUP_IGNORE_S = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_TO_STARTUP_IGNORE_S) R"rawliteral(;
        const SLM_ANALYSIS_STRICT_TAKEOFF_TRANSITION_ENABLED = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_STRICT_TAKEOFF_TRANSITION_ENABLED) R"rawliteral(;
        const SLM_ANALYSIS_TRANSITION_CONFIRM_S = )rawliteral" SLM_HTML_STRINGIFY(FLIGHT_ANALYSIS_TRANSITION_CONFIRM_S) R"rawliteral(;

        const SLM_RECORD_START = 0x55;
        const SLM_RECORD_TYPE_ACCEL = 0x70;
        const SLM_RECORD_LENGTH = 13;
        const SLM_DIAGNOSTIC_CLEAN_AXIS_LIMIT_MG = )rawliteral" SLM_HTML_STRINGIFY(AUTOMATION_DIAGNOSTIC_CLEAN_AXIS_LIMIT_MG) R"rawliteral(;
        const SLM_DIAGNOSTIC_AXIS_OFFSET_MG = )rawliteral" SLM_HTML_STRINGIFY(AUTOMATION_DIAGNOSTIC_AXIS_OFFSET_MG) R"rawliteral(;

        const WIT_PACKET_START = 0x55;
        const WIT_TYPE_TIME = 0x50;
        const WIT_TYPE_ACCEL = 0x51;
        const WIT_PACKET_LENGTH = 11;
        const WIT_SCALE_G = 16.0 / 32768.0;

        // Top-level decoder/analysis entry point used by downloadFile().
        async function analyzeRecorderFileAsync(buffer, progressCb) {
            const progress = progressCb || function(){};

            progress(5);
            await analysisYield();

            const decoded = decodeRecorderTimeline(buffer);

            if (!decoded || decoded.records.length < SLM_ANALYSIS_FS * SLM_ANALYSIS_MIN_FILE_S) {
                progress(100);
                return {
                    ok: false,
                    reason: tr('ANALYSIS_NOT_ENOUGH_DATA'),
                    formatName: decoded ? decoded.formatName : 'unknown',
                };
            }

            progress(15);
            await analysisYield();

            const flights = await analyzeAccelerationTimelineAsync(decoded.records, progress);
            flights.forEach((f, index) => { f.number = index + 1; });

            return {
                ok: true,
                formatName: decoded.formatName,
                flights: flights
            };
        }

        // Select and decode the supported recorder binary format.
        function decodeRecorderTimeline(buffer) {
            const slm = decodeSlmTimeline(buffer);
            const wit = decodeWitTimeline(buffer);

            if (slm.records.length === 0 && wit.records.length === 0) {
                return { formatName: 'unknown', records: [] };
            }

            // The formats use distinct packet IDs.  Select the decoder that
            // produced the largest valid acceleration timeline so analysis is
            // independent of file extension.
            return (slm.records.length >= wit.records.length) ? slm : wit;
        }

        // Remove the reversible automation diagnostic offset before normal file analysis.
        // Checksum validation is deliberately performed on the stored bytes first.
        function restoreSlmDiagnosticAxisMg(storedMg) {
            if (storedMg < -SLM_DIAGNOSTIC_CLEAN_AXIS_LIMIT_MG) {
                return storedMg + SLM_DIAGNOSTIC_AXIS_OFFSET_MG;
            }
            if (storedMg > SLM_DIAGNOSTIC_CLEAN_AXIS_LIMIT_MG) {
                return storedMg - SLM_DIAGNOSTIC_AXIS_OFFSET_MG;
            }
            return storedMg;
        }

        // Decode native SLM 0x70 acceleration records with checksum, diagnostic-overlay
        // restoration, and midnight rollover handling.
        function decodeSlmTimeline(buffer) {
            const bytes = new Uint8Array(buffer);
            const view = new DataView(buffer);
            const records = [];
            let dayOffset = 0;
            let previousAbs = null;

            for (let i = 0; i + SLM_RECORD_LENGTH <= bytes.length; ) {
                if (bytes[i] !== SLM_RECORD_START || bytes[i + 1] !== SLM_RECORD_TYPE_ACCEL) {
                    i++;
                    continue;
                }

                let sum = 0;
                for (let k = 0; k < SLM_RECORD_LENGTH - 1; k++) {
                    sum = (sum + bytes[i + k]) & 0xFF;
                }

                if (sum !== bytes[i + SLM_RECORD_LENGTH - 1]) {
                    i++;
                    continue;
                }

                const timeMs = view.getUint32(i + 2, true);
                const storedAxMg = view.getInt16(i + 6, true);
                const storedAyMg = view.getInt16(i + 8, true);
                const storedAzMg = view.getInt16(i + 10, true);
                const axMg = restoreSlmDiagnosticAxisMg(storedAxMg);
                const ayMg = restoreSlmDiagnosticAxisMg(storedAyMg);
                const azMg = restoreSlmDiagnosticAxisMg(storedAzMg);


                if (previousAbs !== null && timeMs + dayOffset < previousAbs - 1000) {
                    dayOffset += 24 * 3600 * 1000;
                }

                const absMs = timeMs + dayOffset;
                records.push({
                    absMs: absMs,
                    axG: axMg / 1000.0,
                    ayG: ayMg / 1000.0,
                    azG: azMg / 1000.0
                });

                previousAbs = absMs;
                i += SLM_RECORD_LENGTH;
            }

            return { formatName: 'SLM 0x70', records: records };
        }

        // Decode supported WIT packets. Time anchors are used when present; otherwise 20 Hz spacing is assumed.
        function decodeWitTimeline(buffer) {
            const bytes = new Uint8Array(buffer);
            const view = new DataView(buffer);
            const records = [];
            let dayOffset = 0;
            let lastSodMs = null;
            let anchorAbsMs = null;
            let accelCountSinceAnchor = 0;

            for (let i = 0; i + 2 <= bytes.length; ) {
                if (bytes[i] !== WIT_PACKET_START) {
                    i++;
                    continue;
                }

                const packetType = bytes[i + 1];
                if ((packetType !== WIT_TYPE_TIME) && (packetType !== WIT_TYPE_ACCEL)) {
                    i++;
                    continue;
                }

                if (i + WIT_PACKET_LENGTH > bytes.length) {
                    break;
                }

                let sum = 0;
                for (let k = 0; k < WIT_PACKET_LENGTH - 1; k++) {
                    sum = (sum + bytes[i + k]) & 0xFF;
                }

                if (sum !== bytes[i + WIT_PACKET_LENGTH - 1]) {
                    i++;
                    continue;
                }

                if (packetType === WIT_TYPE_TIME) {
                    const sodMs = decodeWitTimeSodMs(bytes, view, i);
                    if (sodMs !== null) {
                        if (lastSodMs !== null && sodMs < lastSodMs) {
                            dayOffset += 24 * 3600 * 1000;
                        }
                        anchorAbsMs = dayOffset + sodMs;
                        accelCountSinceAnchor = 0;
                        lastSodMs = sodMs;
                    }
                } else {
                    const axRaw = view.getInt16(i + 2, true);
                    const ayRaw = view.getInt16(i + 4, true);
                    const azRaw = view.getInt16(i + 6, true);
                    const absMs = (anchorAbsMs === null)
                        ? Math.round(records.length * 1000.0 / SLM_ANALYSIS_FS)
                        : Math.round(anchorAbsMs + accelCountSinceAnchor * 1000.0 / SLM_ANALYSIS_FS);

                    records.push({
                        absMs: absMs,
                        axG: axRaw * WIT_SCALE_G,
                        ayG: ayRaw * WIT_SCALE_G,
                        azG: azRaw * WIT_SCALE_G
                    });

                    accelCountSinceAnchor++;
                }

                i += WIT_PACKET_LENGTH;
            }

            const formatName = (lastSodMs === null) ? 'WIT 0x51' : 'WIT 0x50/0x51';
            return { formatName: formatName, records: records };
        }

        function decodeWitTimeSodMs(bytes, view, offset) {
            const month = bytes[offset + 3];
            const day = bytes[offset + 4];
            const hour = bytes[offset + 5];
            const minute = bytes[offset + 6];
            const second = bytes[offset + 7];
            const millisecond = view.getUint16(offset + 8, true);

            if (month < 1 || month > 12 || day < 1 || day > 31 ||
                hour > 23 || minute > 59 || second > 59 || millisecond > 999) {
                return null;
            }

            return ((hour * 3600 + minute * 60 + second) * 1000 + millisecond);
        }

        // Common flight-detection pipeline after file-format-specific decoding.
        async function analyzeAccelerationTimelineAsync(records, progressCb) {
            const progress = progressCb || function(){};

            progress(20);
            await analysisYield();
            const accelMod = records.map(r => Math.sqrt(r.axG * r.axG + r.ayG * r.ayG + r.azG * r.azG));

            progress(32);
            await analysisYield();
            const accelHigh = butterHighpass4ZeroPhase(accelMod, 3.0, SLM_ANALYSIS_FS);
            const hirms = movingRms(accelHigh, SLM_ANALYSIS_HIRMS_WINDOW_S, SLM_ANALYSIS_FS);

            progress(48);
            await analysisYield();
            const accelLow = butterBandpassApproxZeroPhase(accelMod, 0.5, 3.0, SLM_ANALYSIS_FS);
            const lowrms = movingRms(accelLow, SLM_ANALYSIS_LOWRMS_WINDOW_S, SLM_ANALYSIS_FS);

            progress(62);
            await analysisYield();
            const hMin = arrayMin(hirms);
            const hMax = arrayMax(hirms);
            const hirmsNorm = normalizeWithRange(hirms, hMin, hMax);
            const lowrmsNorm = normalizeWithRange(lowrms, hMin, hMax);

            progress(72);
            await analysisYield();
            const flightgroundRaw = lowrmsNorm.map((v, i) => Math.max(0.0, v - hirmsNorm[i]));
            const fgCutoff = 1.0 / SLM_ANALYSIS_FLIGHTGROUND_LPF_PERIOD_S;
            const flightground = butterLowpass4ZeroPhase(flightgroundRaw, fgCutoff, SLM_ANALYSIS_FS);

            progress(84);
            await analysisYield();
            const flightState = calculateBooleanState(
                flightground,
                SLM_ANALYSIS_FLIGHTGROUND_THRESHOLD,
                SLM_ANALYSIS_FLIGHTGROUND_HYSTERESIS,
                hirmsNorm
            );

            const segments = findTransitionPeaks(
                flightState,
                hirms,
                SLM_ANALYSIS_FS,
                SLM_ANALYSIS_SEARCH_WINDOW_S
            );

            progress(94);
            await analysisYield();
            return findRollPhases(segments, hirmsNorm, records);
        }

)rawliteral"
