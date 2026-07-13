# TE-2350 Antigravity JUCE Plugin

This directory contains the JUCE front-end scaffold for the shared TE-2350 DSP
core. The plugin links the existing portable C files from the repository root;
it does not copy or fork `te2350.c`, `dsp_delay.c`, `dsp_filters.c`,
`dsp_modulation.c`, `dsp_pitch.c`, or `dsp_fdn.c`.

## Build

```bash
cmake -S te2350-vst -B build/te2350-vst -DTE2350_JUCE_PATH=/path/to/JUCE
cmake --build build/te2350-vst --config Release
```

If `TE2350_JUCE_PATH` is not set, CMake can fetch JUCE with FetchContent:

```bash
cmake -S te2350-vst -B build/te2350-vst -DTE2350_FETCH_JUCE=ON
```

The build enables VST3 on every platform and AU on Apple platforms.

## Current Scope

- `AudioProcessorValueTreeState` includes the complete Layer 1-3 parameter set.
- `SPACE`, `WILD`, and `BLOOM` are implemented as smoothed macro offsets over
  raw parameter values; the raw APVTS parameters remain automatable.
- The plugin wrapper calls the existing fixed-point core setters and sample
  processor directly.
- The internal melody generator is linked only because the current core owns
  that state; it is disabled by the wrapper and not exposed as plugin UI.
- The editor is a minimal validation UI organized as Layer 1, Layer 2, and
  utility controls.
- The custom TE-2350 SVG logo lives in `Source/Assets` and is embedded through
  JUCE BinaryData for future UI work.

## Notes

- Hardware Mode uses the same Q31 fixed-point processing path as the firmware
  and web ports. `TE2350_MAIN_DELAY_SIZE` defaults to `32768` for parity with
  the existing project build.
- Studio Mode is present as an architectural hook and reports zero latency for
  now. The oversampling chain should only become active after the core exposes
  stage boundaries for the nonlinear FDN and pitch/shimmer sections.
- `highCutHz` maps to the existing core tone setter. `lowCutHz` and `wetWidth`
  are APVTS-ready and macro-ready, but need explicit shared-core hooks before
  they should affect sound.
- Sync mode reads host BPM and maps note values onto `timeMs` before the core's
  normalized time setter.
- `Source/Assets/te2350_logo_custom.svg` is available to C++ as BinaryData
  (`te2350_logo_custom_svg`) once the plugin target is built.

## Validation

The optional golden-reference scaffold renders an impulse through the same core:

```bash
cmake -S te2350-vst -B build/te2350-vst -DTE2350_JUCE_PATH=/path/to/JUCE -DTE2350_BUILD_GOLDEN_TESTS=ON
cmake --build build/te2350-vst --target TE2350GoldenReference
```

Compare `plugin_reference.raw` with `output.raw` from `src/web/offline_host.c`
after aligning the same parameter values and `TE_MAIN_DELAY_SIZE`.
