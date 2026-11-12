# Transient Designer Plus

A clean, detailed attack / body / sustain workstation for stereo mixing. A purpose-built JUCE VST3 for Windows x64.

Designed to offer three independently shaped sections of each transient, optional complementary 3-band operation, a threshold-free differential-envelope detector, stereo link / dual mono / mid-side, mix and delta audition, a configurable sample-peak guard, and a hardware-inspired rack interface.

**Status:** source project in active development. Release artifacts are produced by GitHub Actions once the build passes. Not an electrical clone or an official SPL/SSL product.

Default sound is clean: no saturation, no intentionally generated harmonics. Crossover modes use additive-complementary signal decomposition, intended to reconstruct cleanly at zero band gain; they are not advertised as linear-phase.

## Windows build

GitHub Actions > Windows VST3 Build > latest run > artifact. On Windows with CMake + Visual Studio 2022, run `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` and `cmake --build build --config Release --target TransientDesignerPlus_VST3`.

## Technical source references
- SPL 9946 Transient Designer manual and its differential-envelope discussion.
- SSL Drumstrip documentation on attack shaping and detector audition.

Independent implementation for this project. No SPL/SSL proprietary code is used.
