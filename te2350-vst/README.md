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
  raw parameter values; the raw APVTS parameters remain automatable. `BLOOM`
  uses a measured six-point feedback curve that approximates a 0.5-60 second
  RT60 range at 500 ms and scales with the selected loop duration, while
  retaining the core's diffusion/tone shaping.
- The plugin wrapper calls the existing fixed-point core setters and sample
  processor directly.
- The internal melody generator is linked only because the current core owns
  that state; it is disabled by the wrapper and not exposed as plugin UI.
- The editor exposes the performance, modulation, texture, engine, freeze, and
  utility controls in main and advanced views.
- The custom TE-2350 SVG logo lives in `Source/Assets` and is embedded through
  JUCE BinaryData for future UI work.

## Notes

- Hardware Mode uses the same Q31 fixed-point processing path as the firmware
  and web ports. The firmware default remains `32768`; the VST build overrides
  `TE2350_MAIN_DELAY_SIZE` to `524288`, enough for 2 seconds through 192 kHz.
- Studio Mode runs a floating-point harmonic finishing stage at 2x
  oversampling. Hardware and bypass paths receive the same fixed latency, so
  mode changes use a click-free 20 ms crossfade without changing DAW latency.
  Its calibrated gain stays within approximately +0.1/-0.5 dB from Hardware
  Mode over the tested -18 to -1.4 dBFS sine range.
- `highCutHz` maps to the core tone setter. `lowCutHz` filters the wet return,
  and `wetWidth` scales only the wet side signal while preserving the stereo
  dry path.
- Sync mode reads host BPM and sends an exact sample target to the core. The
  wide Hermite reader preserves exact targets beyond 65535 samples.
- Mod Shape selects Triangle, Random Walk, or Sample & Hold. Freeze can be
  momentary (active only while pressed) or latched.
- Shimmer feedback is dormant while Shimmer Amount is zero, so neutral presets
  no longer acquire hidden octave coloration.
- Factory-preset output trims were calibrated against the same musical render;
  active RMS is within 0.5 dB across all seven presets without peak clipping.
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

The calibration regression measures direct -60 dB decay time and Hardware /
Studio gain at several levels:

```bash
cmake --build build/te2350-vst --target TE2350CalibrationTest
ctest --test-dir build/te2350-vst -R TE2350CalibrationTest --output-on-failure
```

To generate the deterministic musical reference and all factory-preset WAVs:

```bash
cmake --build build/te2350-vst --target TE2350CalibrationRender
build/te2350-vst/TE2350CalibrationRender build/te2350-vst/CalibrationRenders
```
