/*******************************************************************************
 * SCRIPT: FLIGHT-TIME SIGNAL PROCESSING
 * Filter, RMS, threshold, transition, and roll-phase helper functions used by browser-side flight-time analysis.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(        // Fourth-order filters are built from cascaded second-order sections.
        function makeBiquad(type, cutoffHz, fs, q) {
            const w0 = 2.0 * Math.PI * cutoffHz / fs;
            const cosw0 = Math.cos(w0);
            const sinw0 = Math.sin(w0);
            const alpha = sinw0 / (2.0 * q);

            let b0, b1, b2;
            if (type === 'low') {
                b0 = (1.0 - cosw0) / 2.0;
                b1 = 1.0 - cosw0;
                b2 = (1.0 - cosw0) / 2.0;
            } else {
                b0 = (1.0 + cosw0) / 2.0;
                b1 = -(1.0 + cosw0);
                b2 = (1.0 + cosw0) / 2.0;
            }

            const a0 = 1.0 + alpha;
            const a1 = -2.0 * cosw0;
            const a2 = 1.0 - alpha;

            return {
                b0: b0 / a0,
                b1: b1 / a0,
                b2: b2 / a0,
                a1: a1 / a0,
                a2: a2 / a0
            };
        }

        // Apply one direct-form IIR section to a scalar array.
        function applyBiquad(data, coeff) {
            const out = new Array(data.length);
            let x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

            for (let i = 0; i < data.length; i++) {
                const x0 = data[i];
                const y0 = coeff.b0 * x0 + coeff.b1 * x1 + coeff.b2 * x2 - coeff.a1 * y1 - coeff.a2 * y2;
                out[i] = Number.isFinite(y0) ? y0 : 0.0;
                x2 = x1; x1 = x0;
                y2 = y1; y1 = out[i];
            }

            return out;
        }

        function applyCascade(data, sections) {
            let out = data.slice();
            sections.forEach(section => { out = applyBiquad(out, section); });
            return out;
        }

        // Forward/reverse filtering removes phase delay for event timing.
        function applyZeroPhase(data, sections) {
            const fwd = applyCascade(data, sections);
            const rev = applyCascade(fwd.slice().reverse(), sections);
            return rev.reverse();
        }

        function butterLowpass4ZeroPhase(data, cutoffHz, fs) {
            const sections = SLM_ANALYSIS_BUTTER_Q.map(q => makeBiquad('low', cutoffHz, fs, q));
            return applyZeroPhase(data, sections);
        }

        function butterHighpass4ZeroPhase(data, cutoffHz, fs) {
            const sections = SLM_ANALYSIS_BUTTER_Q.map(q => makeBiquad('high', cutoffHz, fs, q));
            return applyZeroPhase(data, sections);
        }

        function butterBandpassApproxZeroPhase(data, lowHz, highHz, fs) {
            return butterLowpass4ZeroPhase(butterHighpass4ZeroPhase(data, lowHz, fs), highHz, fs);
        }

        // Centered RMS window used to estimate high-frequency and low-frequency activity.
        function movingRms(data, windowTimeS, fs) {
            const n = Math.max(1, Math.round(windowTimeS * fs));
            const half = Math.floor(n / 2);
            const prefix = new Array(data.length + 1);
            prefix[0] = 0.0;

            for (let i = 0; i < data.length; i++) {
                prefix[i + 1] = prefix[i] + data[i] * data[i];
            }

            const out = new Array(data.length);
            for (let i = 0; i < data.length; i++) {
                const startRaw = i - half;
                const endRaw = startRaw + n;
                const start = Math.max(0, startRaw);
                const end = Math.min(data.length, endRaw);
                const sum = prefix[end] - prefix[start];
                out[i] = Math.sqrt(Math.max(0.0, sum / n));
            }

            return out;
        }

        function normalizeWithRange(data, minVal, maxVal) {
            if (!Number.isFinite(minVal) || !Number.isFinite(maxVal) || maxVal === minVal) {
                return data.map(() => 0.0);
            }
            return data.map(v => (v - minVal) / (maxVal - minVal));
        }

        // Rolling maximum over a causal look-back window ending at the current sample.
        // For landing detection this answers: "was there a high-frequency RMS
        // peak not older than the configured age window before this transition?"
        function rollingPastMax(data, windowSamples) {
            const n = Math.max(1, Math.round(windowSamples));
            const out = new Array(data.length);
            const deque = [];

            for (let i = 0; i < data.length; i++) {
                const minIdx = i - n;
                while (deque.length > 0 && deque[0] < minIdx) deque.shift();
                while (deque.length > 0 && data[deque[deque.length - 1]] <= data[i]) deque.pop();
                deque.push(i);
                out[i] = data[deque[0]];
            }

            return out;
        }

        // Convert a continuous flight/ground score into a state with hysteresis.
        // FlightGround still drives both transitions. HIRMS gates add physical
        // confirmation that rolling/touchdown vibration was recently observed.
        function calculateBooleanState(data, threshold, hysteresis, hirmsNorm) {
            const tUp = threshold + hysteresis / 2.0;
            const tDown = threshold - hysteresis / 2.0;
            const useLandingGate = (SLM_ANALYSIS_LDG_HIRMS_GATE_ENABLED !== 0) && Array.isArray(hirmsNorm);
            const landingRecentMax = useLandingGate
                ? rollingPastMax(hirmsNorm, SLM_ANALYSIS_LDG_HIRMS_PEAK_MAX_AGE_S * SLM_ANALYSIS_FS)
                : null;
            const useTakeoffGate = (SLM_ANALYSIS_TO_HIRMS_GATE_ENABLED !== 0) && Array.isArray(hirmsNorm);
            const takeoffRecentMax = useTakeoffGate
                ? rollingPastMax(hirmsNorm, SLM_ANALYSIS_TO_HIRMS_PEAK_MAX_AGE_S * SLM_ANALYSIS_FS)
                : null;
            const startupIgnoreSamples = Math.max(0, Math.round(SLM_ANALYSIS_TO_STARTUP_IGNORE_S * SLM_ANALYSIS_FS));
            const strictTakeoff = (SLM_ANALYSIS_STRICT_TAKEOFF_TRANSITION_ENABLED !== 0);
            const confirmSamples = Math.max(1, Math.round(SLM_ANALYSIS_TRANSITION_CONFIRM_S * SLM_ANALYSIS_FS));
            const out = new Array(data.length).fill(0);
            let state = 0;
            let takeoffArmed = !strictTakeoff;
            let pendingKind = null;
            let pendingStart = -1;
            function resetPending() { pendingKind = null; pendingStart = -1; }
            function acceptTransition(kind, startIdx, endIdx) {
                state = (kind === 'takeoff') ? 1 : 0;
                if (kind === 'takeoff') takeoffArmed = false;
                for (let j = startIdx; j <= endIdx; j++) out[j] = state;
                resetPending();
            }
            for (let i = 0; i < data.length; i++) {
                const value = data[i];
                if (state === 0) {
                    if (i >= startupIgnoreSamples && value < tUp) takeoffArmed = true;
                    const requestTakeoff = (i >= startupIgnoreSamples) && (value >= tUp) && (!strictTakeoff || takeoffArmed);
                    const takeoffGatePass = !useTakeoffGate || (takeoffRecentMax[i] >= SLM_ANALYSIS_TO_HIRMS_PEAK_MIN_NORM);
                    if (requestTakeoff && takeoffGatePass) {
                        if (pendingKind !== 'takeoff') { pendingKind = 'takeoff'; pendingStart = i; }
                        if ((i - pendingStart + 1) >= confirmSamples) acceptTransition('takeoff', pendingStart, i);
                    } else if (pendingKind === 'takeoff') {
                        resetPending();
                    }
                } else {
                    const requestLanding = (value <= tDown);
                    const landingGatePass = !useLandingGate || (landingRecentMax[i] >= SLM_ANALYSIS_LDG_HIRMS_PEAK_MIN_NORM);
                    if (requestLanding && landingGatePass) {
                        if (pendingKind !== 'landing') { pendingKind = 'landing'; pendingStart = i; }
                        if ((i - pendingStart + 1) >= confirmSamples) acceptTransition('landing', pendingStart, i);
                    } else if (pendingKind === 'landing') {
                        resetPending();
                    }
                }
                out[i] = state;
            }
            return out;
        }

        // Pair flight/ground transitions and locate nearby activity peaks for roll timing.
        function findTransitionPeaks(booleanArray, signalToPeak, fs, windowTimeS) {
            const transitions = [];
            for (let i = 1; i < booleanArray.length; i++) {
                if (booleanArray[i] !== booleanArray[i - 1]) transitions.push(i);
            }

            const startIndices = transitions.filter(idx => booleanArray[idx] === 1);
            const endIndices = transitions.filter(idx => booleanArray[idx] === 0);
            if (booleanArray[0] === 1) startIndices.unshift(0);

            const halfWindow = Math.round((windowTimeS / 2.0) * fs);
            const segments = [];
            let endCursor = 0;

            startIndices.forEach(startIdx => {
                while (endCursor < endIndices.length && endIndices[endCursor] <= startIdx) endCursor++;
                if (endCursor >= endIndices.length) return;

                const endIdx = endIndices[endCursor++];

                const startWin0 = Math.max(0, startIdx - halfWindow);
                const startWin1 = Math.min(signalToPeak.length, startIdx + halfWindow);
                const endWin0 = Math.max(0, endIdx - halfWindow);
                const endWin1 = Math.min(signalToPeak.length, endIdx + halfWindow);

                const peakStartIdx = maxIndexInRange(signalToPeak, startWin0, startWin1);
                const peakEndIdx = maxIndexInRange(signalToPeak, endWin0, endWin1);

                if (peakStartIdx >= 0 && peakEndIdx > peakStartIdx) {
                    segments.push({
                        startIdx: startIdx,
                        endIdx: endIdx,
                        peakStartIdx: peakStartIdx,
                        peakEndIdx: peakEndIdx
                    });
                }
            });

            return segments;
        }

        // Convert transition/peak segments into takeoff roll, landing roll, and flight duration fields.
        function findRollPhases(segments, hirmsNorm, records) {
            const results = [];

            segments.forEach(segment => {
                const peakStartIdx = segment.peakStartIdx;
                const peakEndIdx = segment.peakEndIdx;

                let toRollStartIdx = 0;
                let liftOffIdx = peakEndIdx;
                let touchdownIdx = peakStartIdx;
                let ldgRollEndIdx = records.length - 1;

                for (let i = peakStartIdx - 1; i >= 0; i--) {
                    if (hirmsNorm[i] <= SLM_ANALYSIS_TO_ROLL_START_THR) {
                        toRollStartIdx = i + 1;
                        break;
                    }
                }

                for (let i = peakStartIdx; i < peakEndIdx; i++) {
                    if (hirmsNorm[i] <= SLM_ANALYSIS_TO_ROLL_END_THR) {
                        liftOffIdx = i;
                        break;
                    }
                }

                for (let i = peakEndIdx - 1; i >= peakStartIdx; i--) {
                    if (hirmsNorm[i] <= SLM_ANALYSIS_LDG_ROLL_START_THR) {
                        touchdownIdx = i + 1;
                        break;
                    }
                }

                for (let i = peakEndIdx; i < records.length; i++) {
                    if (hirmsNorm[i] <= SLM_ANALYSIS_LDG_ROLL_END_THR) {
                        ldgRollEndIdx = i;
                        break;
                    }
                }

                const takeoffRollStartMs = records[toRollStartIdx].absMs;
                const landingRollEndMs = records[ldgRollEndIdx].absMs;
                const takeoffRollStartRoundedMs = roundMsToNearestMinute(takeoffRollStartMs);
                const landingRollEndRoundedMs = roundMsToNearestMinute(landingRollEndMs);

                const takeoffRollMs = Math.max(0, records[liftOffIdx].absMs - takeoffRollStartMs);
                const landingRollMs = Math.max(0, landingRollEndMs - records[touchdownIdx].absMs);
                const durationMs = Math.max(0, landingRollEndRoundedMs - takeoffRollStartRoundedMs);

                results.push({
                    takeoffRollStart: formatClockMs(takeoffRollStartMs),
                    takeoffRollStartShort: formatClockHm(takeoffRollStartRoundedMs),
                    liftOff: formatClockMs(records[liftOffIdx].absMs),
                    touchdown: formatClockMs(records[touchdownIdx].absMs),
                    landingRollEnd: formatClockMs(landingRollEndMs),
                    landingRollEndShort: formatClockHm(landingRollEndRoundedMs),
                    takeoffRollDuration: formatDurationSeconds(takeoffRollMs),
                    landingRollDuration: formatDurationSeconds(landingRollMs),
                    flightDuration: formatDurationMs(durationMs),
                    flightDurationShort: formatDurationHm(durationMs),
                    toRollStartIdx: toRollStartIdx,
                    liftOffIdx: liftOffIdx,
                    touchdownIdx: touchdownIdx,
                    ldgRollEndIdx: ldgRollEndIdx
                });
            });

            return results;
        }

)rawliteral"
