# TE-2350 Antigravity

**TE-2350 Antigravity** is an experimental stereo ambient DSP engine targeting the **Raspberry Pi Pico 2 / RP2350** and a matching **WebAssembly + AudioWorklet** port for fast iteration in the browser.

The project combines a compact fixed-point DSP core with:

- a **hardware firmware build** for RP2350 using the Pico SDK,
- a **browser build** that runs the same core through Wasm,
- an **offline validation host** for simple render testing.

Live demo: <https://ovelhaaa.github.io/te2350/>

---

## What it is

TE-2350 is designed as a **cloudy, modulated, freeze-capable delay/reverb texture machine** rather than a strict emulation of a single commercial unit. Its character comes from the interaction of:

- a main delay line,
- multi-stage diffusion via allpass sections,
- slow random modulation,
- feedback tone shaping,
- optional shimmer/pitch coloration,
- freeze behavior,
- stereo decorrelation from the mono input path.

The result sits somewhere between:

- ambient delay,
- smeared reverb,
- pitch-washed shimmer,
- unstable “antigravity” space textures.

---

## Current feature set

### DSP engine

- Mono input → stereo output processing
- Fixed-point internal DSP (Q31-style workflow)
- Main delay line with smoothed delay-time changes
- Diffusion chain with multiple allpass stages
- Fractional delay reads for modulation
- Random-walk modulation source for slow movement / instability
- Feedback low-pass damping
- Envelope follower support for dynamic behavior
- Freeze mode
- Shimmer / pitch component
- Mix, tone, diffusion, chaos, ducking, wobble, modulation rate and depth controls

### RP2350 firmware

- Pico SDK + CMake project
- Full-duplex audio path using **PIO + DMA**
- 48 kHz audio configuration
- 128-sample block processing
- USB stdio for runtime parameter tweaking / debug logging
- On-board LED + WS2812 visual feedback hooks
- Optional built-in generative melody source for self-test / demo use

### Web build

- Same DSP core compiled with **Emscripten**
- AudioWorklet-based real-time processing
- Browser UI for live parameter control
- Microphone input path
- File playback path
- Diagnostic bootstrap/status panel for debugging Wasm/worklet initialization
- Simple bypass path for troubleshooting audio routing

### Offline validation

- Small native host program that renders a short impulse-based test to `output.raw`
- Useful for sanity checks without deploying to hardware or the browser

---

## Project structure

```text
te2350/
├─ include/                # public headers for the DSP core and audio driver
├─ src/
│  ├─ main.c               # RP2350 firmware entry point
│  ├─ te2350.c             # core effect implementation
│  ├─ audio_driver.c       # PIO + DMA full-duplex audio backend
│  ├─ dsp_delay.c          # delay line primitives
│  ├─ dsp_filters.c        # filters / tone shaping helpers
│  ├─ dsp_modulation.c     # modulation utilities
│  ├─ dsp_pitch.c          # pitch / shimmer helper code
│  ├─ audio_i2s.pio        # PIO program for I2S-style audio transport
│  ├─ ws2812.pio           # PIO program for NeoPixel / WS2812 feedback
│  └─ web/
│     ├─ index.html        # browser UI
│     ├─ main.js           # main-thread bootstrapping + UI logic
│     ├─ worklet.js        # AudioWorklet processor logic
│     ├─ wasm_wrapper.c    # Wasm-facing C bridge
│     ├─ offline_host.c    # offline validation renderer
│     └─ Makefile          # Emscripten build for the web target
├─ CMakeLists.txt          # Pico SDK build definition
└─ .github/workflows/      # GitHub Actions workflows
```

---

## Architecture overview

The repository is built around **one shared DSP core** and multiple front ends.

### 1. Shared core
The core processing lives in `src/te2350.c` and related DSP modules. This is the part you want to preserve when adding new platforms.

### 2. Hardware target (RP2350)
`src/main.c` initializes the effect, clocking, debug I/O, visual feedback, and the audio backend. Audio is streamed through a custom PIO + DMA path and passed to the core in blocks.

### 3. Web target
`src/web/wasm_wrapper.c` exposes a small C ABI to JavaScript. The browser side loads the Wasm module, instantiates the AudioWorklet, and forwards parameter changes to the DSP core.

### 4. Offline target
`src/web/offline_host.c` is a tiny validation harness that initializes the effect and renders a short raw audio file for inspection.

This split is a strong foundation if you want the **embedded** and **web** versions to evolve together.

---

## Firmware build (RP2350 / Pico 2)

### Requirements

- Raspberry Pi Pico SDK
- CMake
- A working ARM embedded toolchain supported by the Pico SDK
- A Pico 2 / RP2350 setup compatible with the current board configuration

### Notes from the current build

The current CMake project is configured around:

- **Pico SDK 2.2.0**
- `PICO_BOARD = pico2`
- executable target `te2350`

### Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Generated outputs should include the usual Pico artifacts such as `.uf2`, `.bin`, and `.elf`.

### Flashing

Use your normal Pico SDK / Pico 2 flashing workflow for the generated UF2.

---

## Audio path on hardware

The RP2350 version currently uses a **custom PIO audio backend** rather than a higher-level audio framework. At a high level it:

- runs at **48 kHz**,
- processes audio in **128-sample blocks**,
- uses **DMA double buffering**,
- feeds the DSP callback with interleaved stereo buffers,
- currently processes the incoming signal as **mono in → stereo out** inside the core.

This makes the project lightweight and portable, but it also means hardware pin mapping and codec/interface expectations should be treated as part of the source-level configuration, not as a polished end-user hardware abstraction yet.

---

## Runtime controls on hardware

The firmware exposes a serial-control interface for interactive tweaking.

### Serial commands

- `q / w` → time down / up
- `a / s` → feedback down / up
- `z / x` → mix down / up
- `e / r` → shimmer down / up
- `t / y` → tone down / up
- `d / f` → diffusion down / up
- `g / h` → chaos down / up
- `j / l` → ducking down / up
- `v / V` → wobble down / up
- `[ / ]` → presence down / up
- `1 / 2` → mod rate down / up
- `3 / 4` → mod depth down / up
- `c` → toggle freeze
- `m` → toggle internal melody generator
- `k` → toggle melody-only mode
- `n / b` → melody volume down / up
- `5 / 6` → melody density down / up
- `7 / 8` → melody decay down / up
- `o` → toggle octave-feedback mode
- `i / u` → octave-feedback amount down / up
- `p` → print current values

This is extremely useful for quick bring-up before a physical control surface exists.

---

## Web build

### Requirements

- Emscripten (`emcc`)
- A local static web server
- A browser with AudioWorklet + WebAssembly support

### Build

From `src/web/`:

```bash
make
```

This generates:

- `te2350.js`
- `te2350.wasm`
- `te2350-worklet.bundle.js`

The current build concatenates the generated Emscripten wrapper and the custom worklet code into a single bundle.

### Serve locally

Any simple static server works. For example:

```bash
python -m http.server 8000
```

Then open `index.html` through that server.

> Do not open the page directly via `file://`; use a local server so the worklet and Wasm assets load correctly.

---

## Browser UI

The browser version is both a **playable port** and a **debug surface**.

It currently includes:

- Start Audio (microphone)
- File playback
- Stop source
- sliders for all main effect parameters
- freeze toggle
- bootstrap / diagnostic status readout
- worklet / Wasm init state visibility
- bypass mode for troubleshooting

This is especially useful when you are tuning the DSP and want faster iteration than repeatedly flashing hardware.

---

## Offline validation host

`src/web/offline_host.c` provides a very small host-side renderer that initializes the effect, sends an impulse, and writes a short raw stereo file.

Why it matters:

- fast regression checks,
- easy A/B listening,
- easier debugging than full hardware deployment,
- helpful for future automated validation.

A natural next step would be to expand this into a proper offline test harness with:

- WAV output,
- parameter sweep renders,
- golden-reference comparisons,
- automated CI artifacts.

---

## DSP character and intent

From the current codebase, the engine is clearly aiming for a sound with:

- long decays,
- heavy diffusion,
- softened top end in feedback,
- slow modulation drift,
- freezeable ambient beds,
- optional internal generative excitation.

This makes the project especially interesting for:

- ambient guitar / synth textures,
- sound design,
- experimental delay/reverb hybrids,
- embedded DSP prototyping,
- browser-based DSP validation.

---

## Known rough edges

This repository is already promising, but it still reads like an active prototype rather than a finished product. Some areas likely worth tightening next are:

- documenting the exact hardware hookup expected by the audio backend,
- clarifying current pinout assumptions in one canonical place,
- adding WAV-based host tests instead of raw PCM only,
- documenting parameter ranges and sonic behavior more formally,
- adding screenshots / signal-flow diagrams,
- cleaning up duplicate or compressed header definitions,
- adding a license file,
- adding a proper “quick start” path for first-time contributors.

---

## Suggested roadmap

### Near term

- Add a block diagram of the DSP signal flow
- Add hardware hookup notes and tested codec/interface combinations
- Add screenshots/GIFs of the web UI
- Add example sounds or demo recordings
- Turn the offline host into a repeatable validation tool

### Mid term

- Keep the web and RP2350 versions feature-matched
- Add parameter presets
- Add a cleaner controller abstraction for knobs / MIDI / CV-style mapping
- Add a WAV-based regression test suite
- Improve browser packaging and deployment ergonomics

### Long term

- Stereo input support
- richer modulation topologies
- more deliberate shimmer voicing modes
- preset morphing / macro controls
- a dedicated hardware pedal or desktop unit around the engine

---

## Why this repo is interesting

A lot of embedded DSP repos pick only one of these worlds:

- hardware prototype only, or
- browser demo only.

`te2350` is interesting because it is already trying to do **both with one engine**.

That is exactly the kind of structure that makes a DSP project easier to:

- hear,
- debug,
- validate,
- evolve,
- and eventually productize.

---

## Contributing ideas

Good contributions for this project would include:

- DSP tuning and voicing improvements
- better host-side test infrastructure
- web UI/UX cleanup
- hardware documentation
- performance profiling on RP2350
- preset management
- docs and diagrams

If you contribute, try to preserve the core design principle of the repo:

> **one DSP core, multiple front ends, shared sonic behavior**

---

## License

No license file is visible in the repository root at the time of writing. If you plan to accept outside contributions or want others to build on the code safely, adding an explicit license should be a priority.

---

## Acknowledgements

Built around:

- Raspberry Pi Pico SDK / RP2350 workflow
- Emscripten + WebAssembly
- Web Audio API / AudioWorklet
- fixed-point embedded DSP techniques

---

## Demo

- GitHub repository: <https://github.com/ovelhaaa/te2350>
- Browser demo: <https://ovelhaaa.github.io/te2350/>
