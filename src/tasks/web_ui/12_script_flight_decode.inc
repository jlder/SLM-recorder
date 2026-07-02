/*******************************************************************************
 * SCRIPT: FLIGHT-ANALYSIS CONSTANTS AND DECODERS
 * Constants and decoders for SLM 0x70 records and supported WIT packets.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        // Browser analysis constants. Keep these aligned with the recorder sampling rate
        // and the validated flight-time extraction thresholds.
        const SLM_ANALYSIS_FS = 20.0;
        const SLM_ANALYSIS_HIRMS_WINDOW_S = 4.0;
        const SLM_ANALYSIS_LOWRMS_WINDOW_S = 10.0;
        const SLM_ANALYSIS_FLIGHTGROUND_LPF_PERIOD_S = 20.0;
        const SLM_ANALYSIS_FLIGHTGROUND_THRESHOLD = 0.05;
        const SLM_ANALYSIS_FLIGHTGROUND_HYSTERESIS = 0.10;
        const SLM_ANALYSIS_SEARCH_WINDOW_S = 80.0;
        const SLM_ANALYSIS_TO_ROLL_START_THR = 0.10;
        const SLM_ANALYSIS_TO_ROLL_END_THR = 0.25;
        const SLM_ANALYSIS_LDG_ROLL_START_THR = 0.25;
        const SLM_ANALYSIS_LDG_ROLL_END_THR = 0.10;
        const SLM_ANALYSIS_MIN_FILE_S = 30.0;
        const SLM_ANALYSIS_BUTTER_Q = [0.541196100146, 1.306562964876];

        const SLM_RECORD_START = 0x55;
        const SLM_RECORD_TYPE_ACCEL = 0x70;
        const SLM_RECORD_LENGTH = 13;

        const WIT_PACKET_START = 0x55;
        const WIT_TYPE_TIME = 0x50;
        const WIT_TYPE_ACCEL = 0x51;
        const WIT_PACKET_LENGTH = 11;
        const WIT_SCALE_G = 16.0 / 32768.0;

        // Top-level decoder/analysis entry point used by downloadFile().
        async function analyzeRecorderFileAsync(buffer, progressCb) {
            const progress = progressCb || function(){};

            progress(5, 'Processing: decoding file format...');
            await analysisYield();

            const decoded = decodeRecorderTimeline(buffer);
            const samplePeriod = decoded ? computeSamplePeriodStats(decoded.records) : null;

            if (!decoded || decoded.records.length < SLM_ANALYSIS_FS * SLM_ANALYSIS_MIN_FILE_S) {
                progress(100, 'Processing: not enough valid flight-analysis data.');
                return {
                    ok: false,
                    reason: 'Not enough valid flight-analysis data.',
                    formatName: decoded ? decoded.formatName : 'unknown',
                    samplePeriod: samplePeriod
                };
            }

            progress(15, 'Processing: analyzing flight times...');
            await analysisYield();

            const flights = await analyzeAccelerationTimelineAsync(decoded.records, progress);
            flights.forEach((f, index) => { f.number = index + 1; });

            return {
                ok: true,
                formatName: decoded.formatName,
                samplePeriod: samplePeriod,
                flights: flights
            };
        }

        // Report sample-period quality using only plausible adjacent sample deltas.
        function computeSamplePeriodStats(records) {
            if (!records || records.length < 2) {
                return { ok: false, count: 0, excluded: 0 };
            }

            const nominalMs = 1000.0 / SLM_ANALYSIS_FS;
            const maxValidMs = nominalMs * 4.0;
            const minValidMs = nominalMs * 0.2;
            const periods = [];
            let excluded = 0;

            for (let i = 1; i < records.length; i++) {
                const dt = records[i].absMs - records[i - 1].absMs;
                if (Number.isFinite(dt) && dt >= minValidMs && dt <= maxValidMs) {
                    periods.push(dt);
                } else {
                    excluded++;
                }
            }

            if (periods.length === 0) {
                return { ok: false, count: 0, excluded: excluded };
            }

            let sum = 0.0;
            let min = periods[0];
            let max = periods[0];
            let outside49_51 = 0;
            let outside48_52 = 0;

            periods.forEach(dt => {
                sum += dt;
                if (dt < min) min = dt;
                if (dt > max) max = dt;
                if (dt < 49.0 || dt > 51.0) outside49_51++;
                if (dt < 48.0 || dt > 52.0) outside48_52++;
            });

            const avg = sum / periods.length;
            let varSum = 0.0;
            periods.forEach(dt => {
                const d = dt - avg;
                varSum += d * d;
            });

            return {
                ok: true,
                count: periods.length,
                excluded: excluded,
                avgMs: avg,
                stdMs: Math.sqrt(varSum / periods.length),
                minMs: min,
                maxMs: max,
                outside49_51: outside49_51,
                outside48_52: outside48_52
            };
        }

        function samplePeriodStatsText(stats) {
            if (!stats || !stats.ok) {
                return 'Sample period: unavailable';
            }

            return 'Sample period: avg ' + stats.avgMs.toFixed(2) +
                ' ms, std ' + stats.stdMs.toFixed(2) + ' ms';
        }

        // Try all supported binary decoders and select the longest valid acceleration timeline.
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

        // Decode native SLM 0x70 acceleration records with checksum and midnight rollover handling.
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
                const axMg = view.getInt16(i + 6, true);
                const ayMg = view.getInt16(i + 8, true);
                const azMg = view.getInt16(i + 10, true);

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

            progress(20, 'Processing: computing acceleration magnitude...');
            await analysisYield();
            const accelMod = records.map(r => Math.sqrt(r.axG * r.axG + r.ayG * r.ayG + r.azG * r.azG));

            progress(32, 'Processing: computing HIRMS (>3 Hz)...');
            await analysisYield();
            const accelHigh = butterHighpass4ZeroPhase(accelMod, 3.0, SLM_ANALYSIS_FS);
            const hirms = movingRms(accelHigh, SLM_ANALYSIS_HIRMS_WINDOW_S, SLM_ANALYSIS_FS);

            progress(48, 'Processing: computing LOWRMS (0.5..3 Hz)...');
            await analysisYield();
            const accelLow = butterBandpassApproxZeroPhase(accelMod, 0.5, 3.0, SLM_ANALYSIS_FS);
            const lowrms = movingRms(accelLow, SLM_ANALYSIS_LOWRMS_WINDOW_S, SLM_ANALYSIS_FS);

            progress(62, 'Processing: normalizing RMS signals...');
            await analysisYield();
            const hMin = arrayMin(hirms);
            const hMax = arrayMax(hirms);
            const hirmsNorm = normalizeWithRange(hirms, hMin, hMax);
            const lowrmsNorm = normalizeWithRange(lowrms, hMin, hMax);

            progress(72, 'Processing: computing FlightGround...');
            await analysisYield();
            const flightgroundRaw = lowrmsNorm.map((v, i) => Math.max(0.0, v - hirmsNorm[i]));
            const fgCutoff = 1.0 / SLM_ANALYSIS_FLIGHTGROUND_LPF_PERIOD_S;
            const flightground = butterLowpass4ZeroPhase(flightgroundRaw, fgCutoff, SLM_ANALYSIS_FS);

            progress(84, 'Processing: finding flight/ground transitions...');
            await analysisYield();
            const flightState = calculateBooleanState(
                flightground,
                SLM_ANALYSIS_FLIGHTGROUND_THRESHOLD,
                SLM_ANALYSIS_FLIGHTGROUND_HYSTERESIS
            );

            const segments = findTransitionPeaks(
                flightState,
                hirms,
                SLM_ANALYSIS_FS,
                SLM_ANALYSIS_SEARCH_WINDOW_S
            );

            progress(94, 'Processing: finding roll phase times...');
            await analysisYield();
            return findRollPhases(segments, hirmsNorm, records);
        }

)rawliteral"
