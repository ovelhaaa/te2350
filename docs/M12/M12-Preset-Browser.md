# M12 — Preset Browser, Categories & User Presets

## Existing architecture

Before M12, FactoryPresets held the approved 20-program catalog, category and description metadata. The editor grouped categories using a second hardcoded list. UserPresetManager already scanned XML `.te2350preset` files, saved APVTS snapshots through JUCE TemporaryFile, loaded parameters with host notifications and defaults for missing IDs, and deleted library files. Saving silently replaced an existing name. The editor maintained a transient dirty snapshot, so reopening an editor accepted the current edited sound as clean. Processor host state already embedded all parameter values plus preset identity.

## New architecture

UserPresetManager remains the single filesystem and document service: scan, validation, sanitisation, duplicate checks, explicit overwrite, rename, delete, import and export. The editor provides asynchronous menus, text dialogs and file choosers, with SafePointer callbacks. Processor owns the loaded preset reference and normalised dirty comparison. No DSP, MacroEngine, parameter layout or factory data was edited.

## User preset format

Readable XML, independent `formatVersion=1` (not plugin `stateVersion`):

```xml
<TE2350_PRESET formatVersion="1" name="My Vocal Cloud"
               plugin="TE-2350 Antigravity" pluginVersion="0.2.0" category="USER">
  <PARAMETERS>
    <PARAM id="space" value="0.73"/>
    <!-- Every APVTS parameter is included in actual saved files. -->
  </PARAMETERS>
</TE2350_PRESET>
```

Only parameter children are saved; no DSP buffers, tails or Freeze capture. Existing v1 documents without the new optional metadata remain supported. Invalid roots, empty states, duplicate IDs, missing values, nonnumeric/nonfinite values and future format versions are rejected before application. Unknown IDs are ignored; missing known IDs reset to declared parameter defaults, preventing a hybrid of the old and new preset. Format migration can be added at the document reader boundary.

## Filesystem

JUCE `userApplicationDataDirectory/TE-2350/Antigravity/Presets`, preserving the existing location. On Windows this follows JUCE's application-data location; macOS and Linux follow their JUCE mappings. The directory is created on save/import. An unavailable directory yields an empty user list and a clear write error; factory selection remains independent. Tests inject isolated temporary folders and never use the real user library.

Names are trimmed, restricted to 48 characters, cleaned with JUCE's legal filename conversion and stripped of trailing spaces/dots. Empty names, traversal-only names and Windows reserved device names are rejected. User names are unique case-insensitively; factory names may coexist because they belong to a separate read-only catalog. Users sort alphabetically ignoring case. Save/import/export write a temporary sibling and use JUCE replacement after the write closes. Rename writes updated metadata to the new file before removing the original; failed original removal attempts to roll back the new file. Rename is not a transactional multi-file operation across a process crash.

## UX

- Browser: Factory heading with categories derived in first-occurrence order from real descriptors; User heading for saved sounds. Factory descriptions appear in the existing tooltip. The compact caption shows category or USER, and MODIFIED when needed.
- Save Preset As: Enter confirms, Escape cancels. Existing user names require a separate Replace/Cancel confirmation.
- Save: enabled only for an existing selected user preset; explicitly updates that preset and marks the sound clean.
- Rename: user only; changes filename/metadata and identity without saving edited parameter values or clearing dirty state. Duplicate destinations are rejected.
- Delete: user only, with confirmation. The current sound remains embedded and may be saved again.
- Import: validates and copies an external preset into the library. Duplicate names are rejected with an error; no silent replacement. Select the imported item to load it.
- Export: selected saved user preset via a native save chooser with overwrite warning; exports the saved file, including its stored parameter state.
- Reload: reloads the selected factory/user preset, discarding edits. Disabled for a missing local user file.
- Previous/Next: menu actions wrap through the factory program order, followed by users alphabetically. Categories do not change host program indices.

Search was intentionally omitted. Existing ComboBox keyboard navigation remains available.

## Dirty state

Processor records the full APVTS reference after factory load or successful user load/save. Every parameter is compared in normalised space with absolute tolerance `1e-5`; returning a control to its saved value clears MODIFIED. Includes macros, advanced controls, Freeze parameter switches, Atmos, Quality and Bypass. Bypass remains part of preset state, consistent with existing save/load behavior. Rename preserves the reference. No autosave.

The host document contains an additional PRESET_BASELINE child containing reference parameters, while current APVTS parameters remain at the existing root. State migration still restores the current values through the original path; the reference is extracted separately. Legacy host states without a reference adopt their restored current parameters as the initial reference, preserving existing restore behavior. Existing files remain loadable.

## DAW compatibility and audio safety

Host state is self-contained: both current parameters and preset identity/reference survive removal of a library file. Missing user presets display `(Embedded)` and disable file-specific actions. No file access, parsing or new mutex is added to processBlock. User operations originate from message-thread UI callbacks; values use the existing APVTS host-notification path. MacroEngine receives the changed raw parameter values through its existing path. Factory loads and user loads keep the existing tail behavior: parameters change, existing DSP buffers continue under the new parameters. No forced tail clear or Freeze capture persistence was added.

## Factory preservation

FactoryPresets.cpp/.h, ParameterLayout, MacroEngine and DSP sources are unchanged. Host programs remain the same 20 names and indices. PresetVoicingTest retains the M10 bitwise snapshot comparison for the original 13 presets and M11 checks for the seven additions. The workflow test verifies 20 descriptors and metadata.

## Tests

M12 manager tests are integrated into PresetWorkflowTest: complete parameter roundtrip, explicit overwrite, case-insensitive duplicate rejection, sanitised invalid/device/traversal names, creation of a missing directory, inaccessible directory, rename, duplicate rename, delete, invalid user indices (factory protection boundary), import/export, enumeration by a new manager instance, malformed/empty/future/nonnumeric files, dirty/change/return, and host restoration after deleting the local preset. Existing workflow coverage includes Save As from factory, default fallback and host identity restore. UIRender's caption assertion was updated for category/USER plus MODIFIED.

Final Release validation: **15/15 CTest tests passed, zero failures**, 280.04 seconds wall time, on Windows (1 October 2026). Command: `ctest --test-dir build/te2350-vst-ninja-net --output-on-failure -j 3`.

| Test | Result |
| --- | --- |
| PresetWorkflow (including M12 UserPresetManager cases and dirty detection for all 30 parameters) | PASS |
| PresetVoicing (M10 bitwise snapshot, M11 catalog/voicing audit) | PASS |
| MacroCalibration | PASS |
| MusicalBehaviour | PASS |
| HostCompatibility | PASS |
| VST3Load | PASS |
| ReleaseReadiness | PASS |
| UIRender | PASS |
| OfflineReferenceRender, GoldenReferenceRender, GoldenReferenceCompare | PASS |
| CoreControl, PluginSmoke, Calibration, FreezeConsistency | PASS |

Visual review of `UIRenders/modified_1040x680.png` confirmed the category/MODIFIED caption fits the existing header. `git diff --check` passed. `git diff --exit-code` for factory sources, Source/Params, src and include confirmed those approved files are unchanged.

During implementation, an old workflow assertion expected silent overwrite and the UIRender assertion expected exactly MODIFIED. Both were updated to verify the new explicit overwrite/category-caption requirements. The new host roundtrip test caught that migrateState intentionally omits nonparameter children; reference restoration was corrected to extract PRESET_BASELINE directly from the incoming document before migration. The final full run above includes these fixes.

## Build environment observation

The existing build cache points TE2350_JUCE_PATH at the BubbleCloud project wrapper. The initial unrestricted build therefore also built and copied Bubbles to C:/VST/Bubbles.vst3. Subsequent builds explicitly target TE-2350 executables/plugin only. This inherited cache should be replaced with a direct JUCE checkout when preparing a clean distribution build.
