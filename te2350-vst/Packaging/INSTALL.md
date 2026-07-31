# TE-2350 Antigravity — Installation

This package contains the 64-bit VST3 build of TE-2350 Antigravity.

## Windows

1. Close the DAW.
2. Copy the complete `TE-2350 Antigravity.vst3` directory from `VST3` to:
   `C:\Program Files\Common Files\VST3`
3. Reopen the DAW and rescan VST3 plug-ins if Antigravity is not listed.

Administrator permission may be required to write to the system VST3 folder.
Keep the `.vst3` directory intact; it is a bundle, not a single loose DLL.

## macOS

Copy `TE-2350 Antigravity.vst3` to either:

- `~/Library/Audio/Plug-Ins/VST3` for the current user, or
- `/Library/Audio/Plug-Ins/VST3` for every user.

The local RC package is not code-signed or notarized.

## User presets

Factory presets are compiled into the plug-in. User presets are stored outside
the bundle so upgrading the VST3 does not replace them.

- Windows: `%APPDATA%\TE-2350\Antigravity\Presets`
- macOS: `~/Library/TE-2350/Antigravity/Presets`

## Removal

Close the DAW and remove the `TE-2350 Antigravity.vst3` bundle from the VST3
folder. User presets remain available unless their separate preset directory is
also removed.

## RC verification checklist

- Confirm that the DAW reports version `0.2.0`.
- Load each factory preset and confirm the output remains below clipping.
- Save, close, and reopen a session to verify parameter and preset recall.
- Automate Time, Feedback, Mix, Freeze, Bypass, and Engine transitions.
- Report the DAW, OS, sample rate, block size, and failing preset with any issue.
