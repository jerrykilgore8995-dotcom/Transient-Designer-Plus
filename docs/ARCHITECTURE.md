# DSP architecture and engineering notes

## Signal path

```text
Stereo or mono input
   |
Input trim → Stereo Link / Dual Mono / Mid-Side routing
   |
Full / 2-band / 3-band complementary crossover
   |
Per active band:
   ├── peak/RMS detector + sidechain-only HPF
   ├── fast / slow differential-envelope onset estimate
   ├── adjustable ATTACK shape (gain / length / curvature)
   ├── adjustable BODY shape (gain / length)
   └── adjustable SUSTAIN contour (gain / start hold / decay)
   |
Band sum (exact complementary zero-gain reconstruction)
   |
Stereo or M/S reconstruction → WET or DELTA mix
   |
Optional RMS-assisted auto-level → output trim
   |
Optional linked SAMPLE-PEAK guard, 0/1/3ms lookahead
   |
Output
```

## Intent

- Use independent clean gain modulation, not analog saturation, to manipulate the transient.
- Defaults are all zero gain: the processor should null to dry without auto gain or peak guard.
- Band 0 = LOW, band 1 = MID and FULL, band 2 = HIGH.
- Stereo LINK shares a joint detector. DUAL MONO separates detectors. M/S applies the same processing in mid-side coordinates.
- No deliberate K2/K3 generation or frequency EQ in the full-range default path.
- Multiband split uses cascaded Butterworth low-pass IIRs, subtracts each low-pass result from its source and reconstructs via addition. **This provides unity reconstruction when the band gains are zero, not linear phase for altered bands.** Phase may change with unequal band gain processing, as with any IIR multiband processor.
- The optional peak guard enforces a *sample* peak ceiling. It is not a true-peak/inter-sample oversampled limiter.
- Lookahead affects the peak-guard path only; it is disabled in the clean default.
- The internal detector is inspired by the concept of differential envelope tracking, **not** a reverse-engineered SPICE clone of SPL's proprietary circuitry.
- Attack duration, onset and sustain are relative to detected hits. Increasing sustain does not synthesize new ambience after source audio has ended.

## First verification targets

| Behavior | Acceptance target |
|---|---|
| Full-band gain zero, all safety off | Output nulls to 1e-6 in float |
| Tri-band gain zero | Reconstructs to 2e-5 in float |
| +15 dB attack | Measurably raises early hit RMS |
| +17 dB sustain | Measurably raises post-hit tail RMS |
| +12 dB body | Measurably raises middle of hit RMS |
| -6 dBFS sample ceiling | No output sample exceeding ceiling plus 1e-5 |
| Denorm/NaN | Tests produce only finite samples |
| 44.1 kHz stereo | Source DSP regression executable passes |

CPU claims require *host measurement* in Reaper/Studio One at actual buffer sizes; the GitHub Actions bench is not a surrogate for SER8 real-time thread usage.

## References used to inform the concept

- SPL Transient Designer 9946 manual (1999), especially pages 6–7, fast/slow differential envelopes.
- SSL Drumstrip user guide, transient shaping and audition workflows.
- Independent C++17 + JUCE 8 implementation. No third-party hardware vendor endorsement or proprietary source code.
