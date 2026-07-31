# Changelog

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
