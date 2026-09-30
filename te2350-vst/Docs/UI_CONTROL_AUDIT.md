# TE-2350 — UI/UX Simplification & Control Semantics Audit

Audit date: 2026-09-30. Public contract: 30 parameters.

## UI control audit

All parameter controls use APVTS SliderAttachment, ComboBoxAttachment or ButtonAttachment, selected by type below. Every attached tile exposes its ParameterID as component ID; helper assertions reject missing IDs in debug builds. Bypass uses its dedicated ButtonAttachment. Processor collects continuous effective values after MacroEngine smoothing; choices/bools and trims/threshold retain existing raw reads. No public IDs were removed.

| UI label | ParameterID | Attachment / raw range (default) | Macro participation | DSP destination / consumer | Status |
|---|---|---|---|---|---|
| Space | `space` | Slider; `0.0, 1.0, 0.30` | Macro source | Macro targets: time, cuts, diffusion, shimmer, width; Delay read heads, tone filters, diffuser, pitch lane, stereo side | OK |
| Wild | `wild` | Slider; `0.0, 1.0, 0.00` | Macro source | Macro targets: feedback, chaos, wobble, mod rate/depth; Loop gain, random motion, delay modulation | OK |
| Bloom | `bloom` | Slider; `0.0, 1.0, 0.20` | Macro source | Macro targets: tone, duck, shimmer, mix; set_tail / set_tail_feedback; Calibrated feedback floor, tail wash, shimmer/duck/mix | OK |
| Time | `timeMs` | Slider; `10.0, 2000.0, 420.0, 420.0, " ms"` | Space | set_time + set_time_samples; Main delay read length; Sync overrides manual value | OK |
| Sync Mode | `syncMode` | ComboBox; `{ "Free", "1/4", "1/8", "1/8.", "1/8T", "1/16" }, 0` | None | Processor getSyncedTimeMs → set_time_samples; Host BPM division, exact delay length | OK |
| Feedback | `feedback` | Slider; `0.0, 1.05, 0.45, 0.70` | Wild | set_feedback → p_feedback_smoothed; feedback_condition, loop/FDN gain and wet drive | suspicious |
| Mix | `mix` | Slider; `0.0, 1.0, 0.35` | Bloom | set_mix → p_mix; Core equal-power dry/wet mix | OK |
| Kill Dry | `killDry` | Button; `false` | None | Wrapper mix override → set_mix(1); Removes direct signal in core mix | OK |
| Low Cut | `lowCutHz` | Slider; `20.0, 1000.0, 80.0, 120.0, " Hz"` | Space | Hz → one-pole coefficient → set_low_cut_coeff; wet_low_cut_l/r high-pass | semantic mismatch (corrected) |
| Tone | `highCutHz` | Slider; `1000.0, 18000.0, 9000.0, 9000.0, " Hz"` | Space + Bloom | normaliseLog → set_tone → p_tone_smoothed; update_tone_filter, feedback LP/HP and FDN tone | semantic mismatch (documented) |
| Diffusion | `diffusion` | Slider; `0.0, 1.0, 0.40` | Space | set_diffusion → p_diffusion_smoothed; Diffuser/cloud, late texture and FDN | OK |
| Chaos | `chaos` | Slider; `0.0, 1.0, 0.00, 0.25` | Wild | set_chaos → chaos zones; Random movement, tone/feedback protection | OK |
| Drift | `wobble` | Slider; `0.0, 1.0, 0.10` | Wild | set_wobble → p_wobble; Delay wow / drift | OK |
| Presence | `presence` | Slider; `0.0, 1.0, 0.50` | None | set_presence → p_presence_smoothed; Short clear presence rail and stereo detail | OK |
| Mod Rate | `modRateHz` | Slider; `0.02, 2.0, 0.15, 0.25, " Hz"` | Wild | set_mod + set_mod_rate_hz; Random walk rate and triangle/S&H phase increment | OK |
| Mod Depth | `modDepth` | Slider; `0.0, 1.0, 0.10` | Wild | set_mod → p_depth; Delay read-head modulation depth | OK |
| Mod Shape | `modShape` | ComboBox; `{ "Triangle", "Random Walk", "S&H" }, 1` | None | set_mod_shape; Triangle / random walk / sample-and-hold branch | OK |
| Interval | `shimmerInterval` | ComboBox; `{ "-1 oct", "5th", "+1 oct" }, 2` | None | set_shimmer_interval → p_shimmer_pitch; Shimmer and regeneration pitch shifters | semantic mismatch (corrected) |
| Shimmer Amount | `shimmerAmount` | Slider; `0.0, 1.0, 0.00` | Space + Bloom | set_shimmer → p_shimmer; Parallel pitched wet lane and loop coloration | OK |
| Regen | `shimmerFeedback` | Slider; `0.0, 0.95, 0.30, 0.55` | None | set_octave_feedback_enabled / amount; Pitched loop regeneration AND immediate wet return | semantic mismatch (corrected) |
| Duck Threshold | `duckThreshold` | Slider; `-60.0, 0.0, -24.0, 0.0, " dB"` | None | dB → gain → set_duck_threshold; Envelope threshold normalization | OK |
| Duck Amount | `duckAmount` | Slider; `0.0, 1.0, 0.10, 0.30` | Bloom | set_ducking → p_ducking; Wet duck reduction and transient loop ducking | OK |
| Input Trim | `inputTrim` | Slider; `-24.0, 24.0, 0.0, 0.0, " dB"` | None | Wrapper inputGain; Smoothed mono input gain | OK |
| Output Trim | `outputTrim` | Slider; `-24.0, 24.0, 0.0, 0.0, " dB"` | None | Wrapper outputGain; Smoothed stereo output gain | OK |
| Bypass | `bypass` | Button; `false` | None | Processor bypassMix + latency-aligned dry buffer; Smoothed host bypass crossfade | OK |
| Engine | `qualityMode` | ComboBox; `{ "Hardware", "Studio" }, 0` | None | OversamplingChain::setStudioMode; 2x oversampled output colour path | OK |
| Freeze | `freezeEngage` | Button; `false` | None | set_freeze → freeze_crossfade; Input suppression and near-unity loop retention | OK |
| Freeze Mode | `freezeMode` | ComboBox; `{ "Momentary", "Latch" }, 1` | None | Editor ModeAwareToggleButton + mode-change callback; Momentary/Latch interaction; no direct DSP consumer | UI-only |
| Atmos FDN | `atmosFdnOn` | Button; `false` | None | set_fdn_enabled; Optional atmospheric FDN processing | OK |
| Width | `wetWidth` | Slider; `0.0, 1.0, 0.60` | Space | Wrapper wetWidth; Stereo side scaling; dry side retained | OK |

## Non-parameter UI actions

Preset selector loads factory/user APVTS state; categories, embedded preset identity and dirty state remain intact. Actions menu retains save/delete, undo/redo and mutation depths, and adds Reset. A/B snapshots remain parameter ValueTrees with compact A/B state. Mutate retains its protected parameter list and 20% default depth. Advanced is local UI visibility, not an automatable parameter. Input/output meters remain level telemetry. Gravity meter and animated star field were removed.

## Changes

Perform has horizontal Space/Wild/Bloom macros, Time/Feedback/Mix/Tone/Width, Sync directly below Time, and highlighted Freeze. Advanced opens below Perform without replacing it; vertical scrolling keeps comfortable controls at the minimum size. Motion, Shimmer, Color, Dynamics, Freeze and Engine/System have dedicated sections. Engine, Kill Dry, Atmos FDN and trims are secondary. Preset actions and Mutate remain compact header actions. A/B shows A or B. Reset moved to the actions menu.

Knobs retain default markers, double-click reset, popup values, keyboard focus and accessibility metadata. Labels and values replace bordered cards/subtitles/value boxes. Background is solid dark; cyan dominates, violet identifies Wild, amber indicates strong regeneration. Combo explanations are tooltips. Input/output/feedback are narrow footer bars.

Processor publishes atomic snapshots for effective Time, Feedback, Tone, Diffusion, Width, Shimmer and Ducking. A translucent outer arc shows these values; the solid arc and APVTS attachment remain manual. UI never reads MacroEngine's mutable audio-thread state. Time snapshot includes host sync; ghost ring is telemetry, not automation. Snapshots update while audio callbacks run and retain the last value when the host stops processing.

Time is disabled when Sync is active. Interval/Regen and Duck Threshold availability use smoothed effective processor telemetry (0.001 threshold), including macro contributions. Freeze behavior stays accessible. Both Freeze buttons share freezeEngage, intentionally providing performance and configuration access.

## Problematic controls / findings

- `freezeMode`: intentionally UI-only. Editor interaction chooses Momentary/Latch; PluginProcessor consumes only freezeEngage. Moved to Freeze configuration, retaining automation/state.
- `feedback`: wrapper applies Wild ceiling then toQ31 clamps at 1.0, and core further changes loop gain through calibrated Bloom floor, freeze, modulation and protection (`src/te2350.c`, feedback calculation/feedback_condition). Above-unity public values therefore do not reach the core literally. This predates the milestone. Preserve range/default/curves; review any future DSP change separately with audio baselines.
- `highCutHz`: wrapper log-normalizes Hz into set_tone; update_tone_filter derives voiced LP/HP coefficients, also affected by Chaos, and FDN consumes tone. It is not a literal Hz cutoff of a final wet low-pass. Tone is a suitable musical label; tooltip discloses core mapping. Keep DSP intact; a precise calibrated Hz mapping would be a separate sonic change.
- `shimmerFeedback`: core regeneration also contributes an immediate wet return, in addition to pitched loop feedback. Regen describes the principal intent; no DSP rewrite.
- No missing parameter attachment, unused public ID or setter without a processing consumer was found. MacroEngine specs intentionally include only continuous parameters; choices/bools are handled by the processor/editor.

Feedback meter is the MacroEngine effective feedback, constrained by the same Wild ceiling as the wrapper. It is labeled Feedback, not Safety, and does not claim to quantify the final sample-by-sample loop gain or combined Bloom/Shimmer risk. No arbitrary risk formula was added. Above-unity telemetry describes the requested wrapper gain before Q31 saturation.

## Compatibility and verification

ParameterIDs unchanged. ParameterLayout.cpp, MacroEngine.cpp, factory preset values, state migration and DSP sources are unchanged. Presets preserved; automation preserved; state recall preserved by retaining attachments, ranges/defaults/choices/versioning and serialization. Existing host compatibility and preset workflow tests cover current/legacy recall and embedded user presets. Native AU runtime validation cannot run on Windows.

UIRender now checks all 30 expected IDs, corresponding parameter control IDs, continuous ranges/defaults, Perform/Advanced visibility, macro-effective feedback without raw movement, and dependent control states. It renders actual JUCE component trees at 960×680, 1040×680 and 1280×820 for both views. PNGs are produced in `build/te2350-vst-ninja-net/UIRenders`. Visual QA checks the smallest Perform and Advanced, standard and largest sizes, plus scrolled Advanced sections. JUCE's evaluation splash/watermark may overlap the footer in this local build; licensing/build configuration is unchanged.

Validation result: **11/11 TE2350 CTest checks passed**, including golden audio comparison, calibration, real VST3 load, legacy host state, user/factory presets and UI contract/dependencies/A/B. `git diff --check` passed. All six main-size/view renders and all five scrolled Advanced sections were visually inspected. No sonic change was introduced.

Screenshot index (relative to repository root):

- `build/te2350-vst-ninja-net/UIRenders/main_1040x680.png`
- `build/te2350-vst-ninja-net/UIRenders/main_compact_960x680.png`
- `build/te2350-vst-ninja-net/UIRenders/main_large_1280x820.png`
- `build/te2350-vst-ninja-net/UIRenders/advanced_1040x680.png`
- `build/te2350-vst-ninja-net/UIRenders/advanced_960x680.png`
- `build/te2350-vst-ninja-net/UIRenders/advanced_1280x820.png`
- `build/te2350-vst-ninja-net/UIRenders/advanced_shimmer.png`, `advanced_color.png`, `advanced_dynamics.png`, `advanced_freeze.png`, `advanced_system.png`
- `build/te2350-vst-ninja-net/UIRenders/effective_macros_1040x680.png`
