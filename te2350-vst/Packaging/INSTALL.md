# TE-2350 Antigravity installation

Windows x64 is the M13 distribution scope. The package README.txt is generated
from Packaging/README.txt.in with the canonical build version.

1. Close the DAW.
2. Copy the whole `VST3/TE-2350 Antigravity.vst3` bundle to
   `C:\Program Files\Common Files\VST3` (administrator rights may be needed).
3. Reopen and rescan VST3. Preserve all bundle subdirectories.

The embedded presets/UI/DSP need no source files. User presets remain in
`%APPDATA%\TE-2350\Antigravity\Presets`; replacing the bundle preserves them.
Standalone is a developer preview requiring local audio-device setup.
There is no installer, code signing, notarization or macOS distribution in M13.
See the packaged LICENSE and third-party notices before public redistribution.
