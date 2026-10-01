# Changelog

## 0.3.0 — 2026-10-01 (M13)

### DSP, Freeze and Macros

- Consolidates the approved fixed-point DSP, Hardware/Studio, musical macro
  calibration, Freeze consistency and bypass/spillover. M13 changes no DSP or
  parameter behavior.

### UI and presets

- Preserves PERFORM/SCULPT and the approved visual/interaction design.
- Expands the earlier bank to 20 categorized factory presets, preserving program
  order and the approved snapshots (M11).
- Includes the persistent M12 user preset browser: Save As, explicit Save,
  Rename, Delete, Import, Export and Reload; MODIFIED tracking and embedded
  preset baseline survive DAW recall without the local library.
- Preset creator version now derives from build metadata. User format v1 and
  host state v3 remain compatible; M12 formatVersion is retained as an alias.

### Stability and distribution

- Direct pinned JUCE 7.0.12, matching the actual M12 build dependency; rejects
  cross-project wrappers. Product metadata retains its existing identity.
- Isolated Debug/Release presets, explicit product targets and a full 15-test
  qualification gate. Packaging fails closed, scans the extracted final VST3
  and audits runtime DLL dependencies.
- Windows x64 ZIP includes the complete VST3 bundle, developer preview
  Standalone, instructions, licenses, manifest and SHA256 checksums.
- Windows clean-build CI uploads qualified artifacts without publishing releases.
- Own project code adopts MIT; JUCE/third-party terms apply separately.

### Limits

- Unsigned Windows package; no installer. Standalone audio-device support and
  macOS/Linux distributions are not qualified by M13.

## 0.2.0-rc1 — 2026-07-31

### Added

- Categorized 13-preset factory library with six new musical starting points.
- Versioned user-preset files with atomic saving and malformed-file rejection.
- Embedded user-preset identity for reliable project transfer and recall.
- Protected musical mutation at Gentle, Musical, and Deep intensities.
- Undo/Redo for preset loads, mutations, and parameter resets.
- Dedicated Perform and Sculpt workflows, live I/O meters, and dependent-control states.
- Release-readiness regression for all 30 parameters, every factory program,
  mono/stereo processing, 44.1-192 kHz operation, and deterministic state recall.
- Reproducible VST3 beta package with build manifest and SHA-256 checksum.

### Improved

- Fixed-latency bypass and Hardware/Studio transitions.
- Oversized and variable host-block handling without audio-thread allocations.
- Macro smoothing, feedback safety, shimmer neutrality, wet filtering, and stereo width.
- Factory-preset level calibration and UI accessibility at supported editor sizes.

### Compatibility

- Host state format is now `stateVersion=3`; older and partial states are migrated.
- Existing factory programs retain their original indices, with new programs appended.

### RC limitations

- Locally generated packages are unsigned and not notarized.
- The distribution target packages VST3 only; Standalone and AU remain build artifacts.
