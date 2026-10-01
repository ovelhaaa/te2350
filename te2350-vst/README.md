# TE-2350 Antigravity

TE-2350 Antigravity is a stereo ambient delay/reverb texture instrument built
around the shared fixed-point TE-2350 core. The approved sound, 30 parameter
IDs and 20 factory programs are frozen for M13.

## Product and controls

Windows 10/11 x64 VST3 is the current release qualification scope. Standalone
is included as a **developer preview**, with no official audio-device support
claim. macOS VST3/AU/Standalone and other platforms remain developer builds;
M13 does not qualify or package them.

SPACE, WILD and BLOOM shape space, motion and feedback bloom. Time/Sync,
Feedback, Mix, wet filtering, diffusion, shimmer, modulation, ducking, wet
width, I/O trim, Atmos, Freeze, Bypass and Hardware/Studio remain automatable.
PERFORM and SCULPT provide the existing performance and detailed views.
Freeze and bypass preserve the approved capture/spillover behavior. Hardware
uses the Q31 core; Studio adds the existing 2x oversampled finishing stage.

The categorized browser contains **20 embedded factory presets**. The User
section supports Save Preset As, explicit Save/overwrite, Rename, Delete,
Import, Export, Reload and Previous/Next. Edits display MODIFIED. User files
are readable XML `.te2350preset` v1; user files store parameters, not live tails
or Freeze audio. DAW sessions embed current parameters and PRESET_BASELINE,
so recall remains self-contained if a user file is moved or deleted.

User storage remains JUCE `userApplicationDataDirectory/TE-2350/Antigravity/Presets`
(Windows `%APPDATA%\TE-2350\Antigravity\Presets`). Upgrading the binary does
not replace this library. Tests inject temporary libraries and never save in it.
Factory presets, logo, UI and DSP are compiled into the product; the installed
binary requires no source tree or JUCE checkout.

## Clean build, qualification and package

CMake >= 3.22, Git and a Windows x64 C++17 toolchain are required. CI uses
Visual Studio 2022 Build Tools with Desktop development with C++ and Windows
SDK on windows-2022. GCC 14.2 MinGW-w64 UCRT + Ninja is the locally qualified
alternative. Run Ninja presets in the selected toolchain's shell.

```powershell
git clone https://github.com/ovelhaaa/te2350.git
cd te2350/te2350-vst
cmake --preset windows-release
cmake --build --preset windows-release --parallel 4
cmake --build ../build/release-msvc --config Release --target release_qualification --parallel 4
ctest --test-dir ../build/release-msvc -C Release --output-on-failure
cmake --build --preset windows-package --parallel 4
```

For Ninja, use `release`, `qualify` and `package` presets. They explicitly set
`CMAKE_BUILD_TYPE=Release` and isolate outputs in `build/release`; `debug` uses
`build/debug`. Multi-config Visual Studio uses `build/release-msvc` and Release.
Product targets are **TE2350Antigravity_VST3** and **TE2350Antigravity_Standalone**.
No unrestricted build of every target is required. The firmware root CMake
project is separate; configure the desktop plugin from `te2350-vst`.

Default JUCE is fetched directly from upstream tag **7.0.12**, verified against
commit `4f43011b96eb0636104cb3e433894cda98243626`. This matches the actually
qualified M12 dependency; the former unused 8.0.8 default was misleading.
There is no dependency on another plugin or local wrapper. Offline developer
override: `-DTE2350_JUCE_PATH=/path/to/JUCE` must point at a real JUCE project
root with modules and build tools. Its actual commit is recorded in the manifest.
An override with another version is a developer experiment requiring qualification.
Never reuse a historical cache; start a new build directory when changing JUCE.

`TE2350_COPY_PLUGIN_AFTER_BUILD=OFF` is the default. Developers can enable it
with an explicit `TE2350_PLUGIN_COPY_DIR`; CI does not install anything.
MSVC and MinGW compiler runtimes are statically linked. Packaging audits both
binaries and rejects non-system imported DLLs. Release uses optimization and
NDEBUG, has no sanitizer configuration, and excludes PDB/debug artifacts.
MinGW debug/symbol sections from static runtimes are stripped from staged
binaries before final verification; build-tree binaries are retained for diagnosis.

`release_qualification` builds the required tools, checks all 15 required CTest
names are registered and runs the entire inventory (16 tests on Windows, including PackagingGateTest).
`package_te2350` reruns qualification on every invocation, then stages the
complete VST3 bundle, preview Standalone, instructions, licenses, changelog,
manifest, dependency report and per-file SHA256 checksums. It extracts the
ZIP, verifies payload hashes and loads the extracted VST3 with only Windows
system paths on PATH before publishing to `dist`. Any qualification or final
verification failure prevents a new distribution; previous outputs with the
same name are removed at the beginning of the packaging script.
`TE2350PackageBeta` remains a compatibility alias to the same gated target.

The package name/version is derived from `project(... VERSION ...)`, the sole
product version authority. `TE2350_PACKAGE_OUTPUT_DIR` can override `dist`.
Verify the archive with `Get-FileHash -Algorithm SHA256` against the external
`SHA256SUMS.txt`. Do not distribute build trees or test artifacts.

## Install on Windows

Close the DAW and copy the **whole** `VST3/TE-2350 Antigravity.vst3` directory to
`C:\Program Files\Common Files\VST3`, preserving `Contents/x86_64-win` and
`Contents/Resources`. Reopen the host and rescan VST3. Installation there may
require administrator rights; configure/build/test/package do not.
The package is unsigned and has no installer. Preview Standalone can run from
its extracted directory; audio device routing requires local setup.

## Version and compatibility policy

- Product release: edit the semantic version in `te2350-vst/CMakeLists.txt`.
  JUCE VST3/Standalone metadata, preset `pluginVersion`, generated release
  metadata, archive filename and manifest derive from it. Changelog headings
  are historical release records, not build inputs; README uses the build authority.
- `.te2350preset` schema changes: increment `UserPresetManager::presetFormatVersion`
  only when needed. Current v1 also writes `presetFormatVersion`; legacy
  `formatVersion=1` is preserved/read as an alias for M12 compatibility.
  `pluginVersion` is provenance and never gates loading. Future schemas and
  conflicting version aliases are rejected before application.
- Host state schema changes: increment `currentStateVersion` only when needed.
  It remains 3; legacy states and M12 PRESET_BASELINE are supported.
- Never change ParameterIDs, factory indices 0-19 or VST3 identity casually:
  manufacturer TE-2350 / TeAg, plugin code T235, bundle com.te2350.antigravity.

## Regression references and CI

The gate retains MacroCalibration, PresetVoicing, PresetWorkflow, MusicalBehaviour,
FreezeConsistency, HostCompatibility, VST3Load, UIRender, ReleaseReadiness,
OfflineReferenceRender, GoldenReferenceRender/Compare, CoreControl, PluginSmoke
and Calibration. HostCompatibility also benchmarks warmed extreme DSP settings
and checks zero tracked audio-callback allocations.

Core goldens compare independent offline/core renders byte for byte;
GoldenReference is a core-wrapper render, not a full DAW audio golden.
Plugin DSP render/recall tolerances live in calibration/musical/host/readiness
tests. Factory snapshots in M10FactorySnapshot and PresetVoicing stay frozen.
UIRender creates deterministic PNGs and checks layout/interaction; it does not
claim a stored pixel-baseline gate. Preserve PNG evidence and compare it when
needed. Builds never refresh approved golden data automatically.

Windows GitHub Actions performs a clean configure with no dependency cache,
explicit product build, gated package/final scan and source-cleanliness check.
It uploads ZIP/checksums and separate qualification evidence; workflow_dispatch
is available. It does not create a GitHub Release. CI status is reported in
[the M13 report](../docs/M13/M13-Release-Hardening.md), separately from local results.

## Licensing

Own project code: [MIT](../LICENSE). JUCE 7 and bundled third-party terms remain
separate; see Packaging/THIRD-PARTY-NOTICES.txt and the included upstream license.
Public distribution must select and comply with a JUCE licensing route. Local
qualification does not assert that a commercial license has been acquired.
