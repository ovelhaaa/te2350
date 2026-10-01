# M13 - Release Hardening, Packaging & Distribution

Date: 2026-10-01 (America/Sao_Paulo). Product: TE-2350 Antigravity.

## Versioning

Canonical product version: project(TE2350AntigravityPlugin VERSION 0.3.0) in
te2350-vst/CMakeLists.txt. JUCE metadata, VST3, Standalone resource metadata,
preset creator version, generated release metadata, package and manifest derive
from it. No UI About/version label exists to change the approved screenshot.
Historical changelog versions and M12 fixture metadata are records, not inputs.
Preset format remains 1; stateVersion remains 3. New preset documents include
presetFormatVersion=1 and keep the M12 formatVersion=1 alias. pluginVersion is
provenance only. The owner approved MIT for own project code; JUCE terms remain
separate. No public release is published by this workflow.

## JUCE

Default is direct upstream JUCE tag 7.0.12, with HEAD verified against
4f43011b96eb0636104cb3e433894cda98243626. Optional TE2350_JUCE_PATH must be an
actual JUCE root, with modules/build tools and a project(JUCE ...) declaration;
wrappers fail before add_subdirectory. Actual override commit and dirty status are recorded. An unversioned folder
is marked unversioned rather than borrowing an enclosing repository SHA.

Audit discovered that M12's wrapper actually fetched JUCE 7.0.12. The declared
8.0.8 default was unused. An initial fresh build of 8.0.8 rejected MinGW and
failed while bootstrapping juceaide. Pinning the actually approved 7.0.12 avoids
a JUCE upgrade within this frozen release milestone. No JUCE source is patched.
All new builds use direct FetchContent; no other plugin enters the target graph.
Historical caches are not used for configuration, linking or packaging.

## Clean build

Windows GCC 14.2.0 MinGW-w64 x86_64 UCRT, Ninja, CMake 4.1.1 and Git 2.51.0
are available locally. Release flags include -O3 -DNDEBUG, with no sanitizers.
Compiler runtimes are linked statically; MSVC CI uses the static runtime too.
Official product targets: TE2350Antigravity_VST3 and TE2350Antigravity_Standalone.

Initial clean direct-dependency build commands from repository root:

```powershell
cmake -S te2350-vst -B build/release-7 -G Ninja -DCMAKE_BUILD_TYPE=Release -DTE2350_BUILD_GOLDEN_TESTS=ON
cmake --build build/release-7 --config Release --target package_te2350 --parallel 4
```

Repeat validation uses a clean Git clone and its own fetched JUCE/build tree.
The portable documented commands are in te2350-vst/README.md and CMakePresets.json;
Release/Debug/MSVC trees are isolated. This report will record final results
once qualification and final package verification finish.

## Qualification

The 15 M12 CTest cases are preserved. PackagingGateTest adds a 16th case on
Windows: it verifies forced test failure removes old outputs, reduced test
inventory cannot qualify and Debug is rejected. Its injected tests/libraries
live strictly under the build tree. Manual negative-path validation passed.

The full required inventory is MacroCalibration, PresetVoicing, PresetWorkflow,
MusicalBehaviour, FreezeConsistency, HostCompatibility, VST3Load, UIRender,
ReleaseReadiness, OfflineReferenceRender, GoldenReferenceRender/Compare,
CoreControl, PluginSmoke and Calibration. release_qualification checks required
names before running the entire inventory; package_te2350 invokes the same gate
on every attempt. No cached success marker or golden-update command is used.

First complete corrected Release gate: **16/16 PASS**, zero failures, 317.40 s
wall time. The extracted ZIP loaded through the scanner and created its editor.
Final rerun adds stripping of MinGW runtime debug sections on staged copies;
build-tree binaries are left untouched. A stripped-copy host scan passed.

Core goldens compare two independent current-core render paths byte for byte;
they are not a stored historical full-plugin audio baseline. Plugin tolerances
are exercised by calibration/musical/host/readiness tests. Factory snapshots
remain unchanged. UIRender checks layout/interaction and emits PNGs; it does
not implement a stored pixel-baseline gate. M13 adds no visual change. All 15 generated PNGs were byte-identical to the
M12 outputs in the first Release qualification run. No snapshot was refreshed.

Paired warmed HostCompatibility benchmarks (three alternating runs per build)
compared the approved M12 binary and M13: 48 kHz Hardware 2.54% -> 2.50%,
48 kHz Studio 2.87% -> 2.78%, 192 kHz Studio 10.49% -> 10.51% CPU load. The
maximum positive relative difference was 0.19%, consistent with timing noise;
no callback change or performance regression was found. Raw rows: performance.csv.

## Packaging

Windows x64 package: dist/TE-2350-Antigravity-0.3.0-windows-x64.zip, containing
VST3/TE-2350 Antigravity.vst3 with Contents preserved, preview Standalone EXE,
README.txt/README.md, LICENSE, CHANGELOG.md, JUCE/third-party notices,
DEPENDENCIES.txt, release-manifest.txt and internal SHA256SUMS.txt. External
SHA256SUMS.txt and .zip.sha256 identify the ZIP itself, avoiding a circular hash.

Staging excludes source, JUCE source, caches, test renders and debug symbols.
The ZIP is extracted, payload hashes compared and the extracted VST3 scanned,
instantiated and processed with PATH reduced to Windows system directories.
Distribution is copied to dist only after these checks. The build-tree artifact
alone cannot satisfy final validation. Source commit/dirty status, dependency
version/commit, compiler, schemas, identity and hashes are recorded.

## Dependencies

Old M12 binaries imported libgcc_s_seh-1.dll, libstdc++-6.dll and libwinpthread-1.dll;
the old developer PATH masked this deployment risk. New binaries use static
compiler runtimes and packaging accepts only an explicit Windows system DLL
allowlist/API-set DLLs. No arbitrary toolchain DLL is redistributed. The per-binary
import lists are shipped in DEPENDENCIES.txt. Dynamic driver/OS facilities and
an actual clean-machine audio-device test remain separate from import auditing.

## Compatibility

No factory, DSP, CoreWrapper, MacroEngine, ParameterLayout or editor source is
changed. PresetWorkflow tests a frozen M12-created v1 XML fixture
with all 30 parameter IDs, arbitrary product-version provenance, both schema
keys and future/conflicting schema rejection. The fixture was generated with the approved M12 static library
(saved creator version 0.2.0) into a temporary library; it was not regenerated
by the new writer. The clean build uses only this checked-in fixture. Existing tests cover
legacy partial host states, M12 PRESET_BASELINE recall after library deletion,
all factory indices 0-19 and self-contained current parameter recall.

M12 processor CID: ABCDEF019182FAEB5465416754323335.
M12 controller CID: ABCDEF011234ABCD5465416754323335.
Manufacturer TE-2350, manufacturer code TeAg, plugin code T235 and bundle ID
com.te2350.antigravity remain unchanged. Packaging checks the two CIDs and
version against the final bundle moduleinfo; VST3Load checks runtime metadata and creates the actual hosted editor. The
host exposes 31 VST3 parameters: the 30 unchanged APVTS parameters plus
JUCE's existing Program selector. An overly strict initial scanner assertion
expected 30; it was corrected after the first gated run rejected the package.

The user preset directory remains JUCE userApplicationDataDirectory/TE-2350/
Antigravity/Presets. PresetWorkflow always injects temporary directories; editor
render tests only scan the default library and never save there. No migration
or relocation is needed.

## CI

.github/workflows/build-plugin.yml now uses a clean windows-2022 checkout,
Visual Studio 2022 x64, direct pinned JUCE, explicit product targets and the
same gated package/final scan. No CMake/dependency cache, installation copy,
floating pluginval download or broad executable artifact glob remains. Artifacts
are limited to qualified ZIP/checksums; test logs/PNG evidence are separate.
Push/PR and workflow_dispatch are supported. No GitHub Release publication.
Remote run result is pending; local results are not represented as an Actions pass.

## Known limitations

- Windows 10/11 x64 is the M13 qualification scope. macOS/AU/Linux and Windows
  ARM builds are developer configurations, not qualified distributions.
- Standalone is packaged as a developer preview, not officially qualified for
  audio-device routing. Its launch can be smoke-tested without claiming device
  matrix coverage. VST3 is the supported M13 distribution format.
- Unsigned ZIP, no installer, no notarization. Bit-identical binaries are not
  required; toolchain timestamps/path data can differ between clean builds.
- MIT covers own project code. Applicable JUCE 7 licensing/GPL route and
  third-party notice obligations must be settled before public distribution;
  local qualification does not establish license entitlement.
