# Transient Designer Plus V2 — DSP specification

## Signal flow

Input trim → stereo L/R or M/S → full/dual/tri frequency decomposition → adaptive transient/decay detection (with optional detector-only band-pass focus) → sample-aligned attack anticipation (0 / 0.5 / 1.5 / 3 ms) → independent per-band attack / body / live sustain → filtered room gain (positive enhancement or negative attenuation) → band recombination → optional time-frequency transient/tonal spectral shaper (512-point STFT, 75% overlap) → global Dry/Wet and Delta → slow level match → output trim → optional linked sample-peak guard and 4x intersample estimate.

## Main rack page

- Per active band: Attack dB, attack time and curvature; Body dB and length; Sustain dB, hold and recovery; independent IN/SOLO and envelope-action meter.
- Full (MID/FULL), dual (LOW/HIGH), or tri-band operation. 3-band and 2-band variants bypass inactive middle or side bands.
- Detector sensitivity, peak/RMS blend, detector-only HPF, edge shape, crossover frequencies, stereo link percentage.
- Input/Output trim, parallel mix, Delta Listen, automatic level match, optional sample-peak guard and 0/1/3 ms guard lookahead.

## Advanced rack page

- Retrigger interval, 3-way hit grouping (separate/adaptive/group), grouping window, minimum detected signal floor, gain smoothing. Transient lookahead is distinct from output limiter lookahead.
- Room Gain -200 to +200%; early onset, bloom, decay/HF damping, width.
- Adaptive sustain follow (0% timed / 100% live decay tracking), detector frequency and Q, and detection focus blend, each per band. Detector focus does **not** require splitting full-band audio.
- Mid/side attack and sustain trims.
- Spectral transient and steady/tonal gain, spectral flux sensitivity, 4x intersample peak estimation.
- Real-time waveform/envelope visualization (input, output, attack, body, sustain, room and gain action) fed by lock-free atomic sample capture.

## Transparency and accuracy

- No virtual transformers, saturation or synthetic harmonics. Time-varying gain can still create modulation sidebands.
- The zero-band-gain 2/3-band IIR crossover reconstructs the input by subtractive summation, including filter phase; it is **not** zero phase while individual band gains differ.
- The optional 257-tap FIR crossover offers matched group delay of 128 samples at all frequencies. Low-frequency band separation remains subject to the finite FIR transition width.
- The spectral stage is a real FFT/overlap-add flux mask, not a complete psychoacoustic source separator. It has 512-sample latency at any sample rate. Its linked spectral masks retain the original complex-bin phase.
- The optional 4x peak estimator is a windowed-sinc 4-phase estimate used to assist the sample-peak limiter. It is **not certified ITU-R BS.1770 true peak**, and the limiter's immediate safety clamp may itself introduce distortion if abused.
- Host PDC notification is dispatched via JUCE AsyncUpdater on the message thread. Change timing/quality modes while playback is stopped for stable host latency, and do not automate latency-changing mode controls mid-phrase.
- The first-release power budget is a design target, not a measured number. Check real-time CPU in Reaper at 44.1 kHz, 128 and 256 samples on the SER8.

## Regression suite

`tests/EngineTests.cpp`: original V1 baseline.

`tests/V2EngineTests.cpp`: full/dual/tri zero-gain reconstruction, FIR and STFT latency/null, adaptive attack, sustain, room boost/cut, detector focus, sample peak guard, combined-mode output and runtime, spectral Delta and 0%-Mix null.

`tests/EditorPreview.cpp`: captures authentic JUCE main and advanced page images at 1620 x 930 on the GitHub Windows runner. These are not AI-generated GUI mockups.

## Research basis

The attack/sustain concept is informed by differential-envelope dynamics; the optional spectral section uses phase-preserving STFT bins and spectral-flux novelty. This is an original implementation rather than copied proprietary SPL/SSL source code.
