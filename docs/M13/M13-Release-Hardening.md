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
Local OS: Windows 10 x64, build 19045. The exact independent-clone flow was:

```powershell
git clone --branch codex/m13-release-hardening --single-branch https://github.com/ovelhaaa/te2350.git C:/tmp/te2350-m13-clean-0cf22e7
cmake --preset release -S C:/tmp/te2350-m13-clean-0cf22e7/te2350-vst
# Follow the branch fixes during qualification; final source revision is f1ffad2672ca5e13b1aa044920be8f1e9d7b9824.
git -C C:/tmp/te2350-m13-clean-0cf22e7 pull --ff-only
cmake --build C:/tmp/te2350-m13-clean-0cf22e7/build/release --config Release --target package_te2350 --parallel 3
```

The clone configured from scratch, fetched its own JUCE, built the products/tools,
and invoked CTest through the gate. No original-machine source/dependency path
or other-plugin target appears in its CMakeCache/build graph. Source status
remained clean after configure/build/tests/package in both checkouts.
The portable documented commands are in te2350-vst/README.md and CMakePresets.json;
Release/Debug/MSVC trees are isolated. Final build/configuration/package inputs are pinned at commit
`f1ffad2672ca5e13b1aa044920be8f1e9d7b9824`. Both final packages record clean
project and JUCE trees. Later documentation-only commits do not change the
recorded binary build revision. The initial failed JUCE8 configure was moved
reversibly to the ignored build/m13-failed-juce8-configure directory; the
standard build/release preset path is free.

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
Final stripped-package gate: **16/16 PASS**, zero failures, **285.51 s** locally
and **315.88 s** in the independent clone. Each ZIP was extracted, checked and
loaded with its editor through the VST3 scanner. MinGW runtime debug sections
are removed only on staged copies; build-tree binaries remain available.
No debug sections remain in the two shipped binaries.

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


| Test | Final Release result | Seconds |
| --- | --- | ---: |
| TE2350CalibrationTest | PASS | 3.25 |
| TE2350CoreControlTest | PASS | 0.03 |
| TE2350FreezeConsistencyTest | PASS | 34.57 |
| TE2350GoldenReferenceCompare | PASS | 0.06 |
| TE2350GoldenReferenceRender | PASS | 0.13 |
| TE2350HostCompatibilityTest | PASS | 0.51 |
| TE2350MacroCalibrationTest | PASS | 162.64 |
| TE2350MusicalBehaviourTest | PASS | 12.88 |
| TE2350OfflineReferenceRender | PASS | 0.08 |
| TE2350PackagingGateTest | PASS | 0.76 |
| TE2350PluginSmokeTest | PASS | 0.10 |
| TE2350PresetVoicingTest | PASS | 285.47 |
| TE2350PresetWorkflowTest | PASS | 0.19 |
| TE2350ReleaseReadinessTest | PASS | 0.38 |
| TE2350UIRenderTest | PASS | 1.37 |
| TE2350VST3LoadTest | PASS | 0.22 |

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

Both final ZIPs passed CRC/payload hash verification and have the same 12-file
logical payload, document contents, metadata, versions and identity. Binary
hashes differ as allowed for timestamps/build-path data; LICENSE differs only
in LF/CRLF checkout line endings. Core golden outputs are byte-identical
across builds, SHA256 `b949b61097048401472cdde46366250e717fb5ea94c27a6d5b4bce655494c234`.
All 15 UI PNGs are also byte-identical between builds and against M12.

Local ZIP SHA256:
`dc66ef89fcd17b6289b2f6fa3d74dfbeb57fcd7bb2c87e13b8997b63c6c73f22`.

Packaged preview Standalone was started from an unrelated build working directory
with only Windows system paths on PATH and remained alive for the three-second
launch smoke check. It was stopped after the check. This verifies process
startup only; GUI/audio-device operation is not claimed. Native GUI inspection
was unavailable because the computer-use runtime could not write kernel assets.

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
Remote clean build **PASS**: [run 36921772224](https://github.com/ovelhaaa/te2350/actions/runs/36921772224),
source commit `f1ffad2672ca5e13b1aa044920be8f1e9d7b9824`, completed
2026-10-01 20:55 UTC. Runner: Windows Server 2022 x64 (10.0.20348),
Visual Studio 2022, MSVC 19.44.35229.0. All **16/16 tests passed**, zero
failures, 524.42 s test wall time. The extracted final VST3 loaded and created
its editor: version 0.3.0, 31 host parameters, latency 4, tail 65.0 s.
Source-cleanliness verification and both artifact uploads passed.
Full test-result excerpt: qualification-windows-msvc-ci.txt; local complete
gate logs: qualification-windows-gcc.txt and qualification-clean-clone.txt.

Qualified MSVC ZIP/checksums: [TE-2350-windows-x64 artifact 11193322056](https://github.com/ovelhaaa/te2350/actions/runs/36921772224/artifacts/11193322056).
Test log/PNG evidence: [artifact 11193506939](https://github.com/ovelhaaa/te2350/actions/runs/36921772224/artifacts/11193506939).
Actions artifacts have retention limits; the local test ZIP remains in dist.
Earlier runs were superseded/cancelled by subsequent code fixes. The final
code revision above is the successful qualification result.

The runner emitted a post-checkout cleanup warning for the pre-existing
src/web/emsdk gitlink without a .gitmodules URL. It does not enter the desktop
build, and all required steps completed successfully. It remains outside this
desktop release milestone, along with runner action-runtime deprecation notices.

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
