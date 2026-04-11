// web/main.js

let audioCtx;
let effectNode;
let micStream;
let micSource;
let fileSource;
let isPlaying = false;
let activeSource = null;

const startBtn = document.getElementById('startBtn');
const playFileBtn = document.getElementById('playFileBtn');
const btnFile = document.getElementById('btn-file'); // The visible Play File button
const exportMp3Btn = document.getElementById('exportMp3Btn');
const stopBtn = document.getElementById('stopBtn');
const fileInput = document.getElementById('fileInput');
const warningDiv = document.getElementById('warning');
const capabilityBindings = {
    presence: { id: 'presence', type: 'slider', label: 'Presence' },
    octave_feedback_amount: { id: 'octave_feedback_amount', type: 'slider', label: 'Octave Feedback Amount' },
    octave_feedback_enabled: { id: 'octave_feedback_enabled', type: 'toggle', label: 'Octave Feedback Toggle' },
    melody_enabled: { id: 'melody_enabled', type: 'toggle', label: 'Melody Generator Toggle' },
    melody_only: { id: 'melody_only', type: 'toggle', label: 'Melody Only Toggle' },
    melody_volume: { id: 'melody_volume', type: 'slider', label: 'Melody Volume' },
    melody_density: { id: 'melody_density', type: 'slider', label: 'Melody Density' },
    melody_decay: { id: 'melody_decay', type: 'slider', label: 'Melody Decay' }
};

// VU Meter mapping
const vuBars = document.querySelectorAll('.vu-bar');
let vuInterval;

function setVu(on) {
    if (on) {
        vuBars.forEach(b => b.classList.add('active'));
        if (!vuInterval) {
            vuInterval = setInterval(() => {
                vuBars.forEach(b => {
                    b.style.height = (Math.random() * 18 + 6) + 'px';
                });
            }, 300);
        }
    } else {
        vuBars.forEach(b => {
            b.classList.remove('active');
            b.style.height = '6px';
        });
        if (vuInterval) {
            clearInterval(vuInterval);
            vuInterval = null;
        }
    }
}

// Debug UI Elements
const debugCtxState = document.getElementById('debugCtxState');
const debugSampleRate = document.getElementById('debugSampleRate');
const debugWorkletLoad = document.getElementById('debugWorkletLoad');
const debugWasmInit = document.getElementById('debugWasmInit');
const debugReadyMsg = document.getElementById('debugReadyMsg');
const debugActiveSource = document.getElementById('debugActiveSource');
const debugLastStage = document.getElementById('debugLastStage');
const bootstrapLog = document.getElementById('bootstrapLog');
const bypassMode = document.getElementById('bypassMode');

function logBootstrapStage(stage, details = '') {
    const time = new Date().toISOString().split('T')[1].slice(0, 12);
    const msg = `[${time}] [main] ${stage} ${details}`;
    console.log(msg);
    if (bootstrapLog) {
        bootstrapLog.textContent += msg + '\n';
        bootstrapLog.scrollTop = bootstrapLog.scrollHeight;
    }
    if (debugLastStage) {
        debugLastStage.textContent = stage;
    }
}

function updateDebugUI() {
    if (audioCtx) {
        debugCtxState.textContent = audioCtx.state;
        debugSampleRate.textContent = audioCtx.sampleRate + ' Hz';
    }
    debugActiveSource.textContent = activeSource ? activeSource : 'none';
}
setInterval(updateDebugUI, 500); // Poll context state occasionally

function showCapabilityWarning(missingLabels) {
    if (!warningDiv) return;
    if (!missingLabels.length) return;
    warningDiv.style.display = 'block';
    warningDiv.textContent = `Some controls are unavailable in the loaded WASM build and were disabled: ${missingLabels.join(', ')}.`;
}

function applyCapabilityMap(capabilities = {}) {
    const missingLabels = [];

    Object.entries(capabilityBindings).forEach(([param, binding]) => {
        const el = document.getElementById(binding.id);
        if (!el) return;
        const available = capabilities[param] !== false;

        if (!available) {
            missingLabels.push(binding.label);
        }

        el.disabled = !available;
        el.dataset.available = available ? '1' : '0';

        if (binding.type === 'slider') {
            const arc = document.getElementById(`arc-${binding.id}`);
            if (arc) {
                arc.style.opacity = available ? '1' : '0.35';
            }
        }
    });

    showCapabilityWarning(missingLabels);
}

let decodedAudioBuffer = null;
let uploadedFileName = 'processed-audio';
let wasmBinaryCache = null;
let wasmFetchPromise = null;

fileInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) {
        playFileBtn.disabled = true;
        exportMp3Btn.disabled = true;
        decodedAudioBuffer = null;
        return;
    }

    try {
        if (!audioCtx) {
            await initAudioContext();
        }

        const arrayBuffer = await file.arrayBuffer();
        decodedAudioBuffer = await audioCtx.decodeAudioData(arrayBuffer);
        playFileBtn.disabled = false;
        exportMp3Btn.disabled = false;
        uploadedFileName = file.name.replace(/\.[^.]+$/, '') || 'processed-audio';
        console.log(`Successfully decoded ${file.name}`);

        // Auto-play when a file is selected
        startFile();
    } catch (err) {
        console.error('Error decoding audio file:', err);
        alert('Failed to decode the selected audio file.');
        playFileBtn.disabled = true;
        exportMp3Btn.disabled = true;
        decodedAudioBuffer = null;
    }
});

btnFile.addEventListener('click', () => {
    fileInput.click();
});

// Since btnFile is now just used to open fileInput, we should enable it from the start
btnFile.disabled = false;
playFileBtn.disabled = true;
exportMp3Btn.disabled = true;

async function fetchWasmBytes() {
    if (wasmBinaryCache) return wasmBinaryCache;
    if (!wasmFetchPromise) {
        wasmFetchPromise = (async () => {
            const wasmUrl = new URL('te2350.wasm', window.location.href).href;
            const response = await fetch(wasmUrl);
            if (!response.ok) {
                const msg = `Failed to fetch wasm: ${response.status}`;
                alert(msg);
                throw new Error(msg);
            }
            wasmBinaryCache = await response.arrayBuffer();
            return wasmBinaryCache;
        })().catch((err) => {
            wasmFetchPromise = null;
            throw err;
        });
    }
    return wasmFetchPromise;
}

async function initAudioContext() {
    if (audioCtx) return;

    console.log("Initializing AudioContext...");
    // Request 48kHz for highest fidelity to the original DSP code
    audioCtx = new (window.AudioContext || window.webkitAudioContext)({
        sampleRate: 48000
    });

    // Fidelity check
    if (audioCtx.sampleRate !== 48000) {
        warningDiv.style.display = 'block';
        warningDiv.textContent = `Warning: AudioContext running at ${audioCtx.sampleRate}Hz instead of 48000Hz. This may affect delay times, filter frequencies, and overall fidelity compared to the original hardware.`;
    }
    updateDebugUI();

    try {
        logBootstrapStage('before_fetch_wasm');
        if (typeof debugWasmFetch !== 'undefined') debugWasmFetch.textContent = 'fetching...';
        const wasmUrl = new URL('te2350.wasm', window.location.href).href;
        const response = await fetch(wasmUrl);
        logBootstrapStage('fetch_response', `status: ${response.status}, ok: ${response.ok}`);

        if (!response.ok) throw new Error(`Failed to fetch wasm: ${response.status}`);

        const wasmBytes = await response.arrayBuffer();
        wasmBinaryCache = wasmBytes;
        logBootstrapStage('after_arrayBuffer', `byteLength: ${wasmBytes.byteLength}`);

        if (typeof debugWasmFetch !== 'undefined') debugWasmFetch.textContent = 'success';

        logBootstrapStage('before_add_module');
        debugWorkletLoad.textContent = 'loading...';

        // Pass base URL so the worklet knows exactly where to fetch te2350.js
        const workletUrl = new URL('./te2350-worklet.bundle.js', window.location.href);

        await audioCtx.audioWorklet.addModule(workletUrl.href);
        logBootstrapStage('after_add_module');
        debugWorkletLoad.textContent = 'success';

        logBootstrapStage('before_create_node');
        effectNode = new AudioWorkletNode(audioCtx, 'te2350-worklet', {
            numberOfInputs: 1,
            numberOfOutputs: 1,
            outputChannelCount: [2] // Stereo output
        });
        logBootstrapStage('after_create_node');

        // Listen for messages from the worklet
        effectNode.port.onmessage = (event) => {
            const data = event.data;
            if (data.type === 'worklet_debug') {
                const time = new Date().toISOString().split('T')[1].slice(0, 12);
                const msg = `[${time}] [worklet] ${data.stage} ${data.details || ''}`;
                console.log(msg);
                if (bootstrapLog) {
                    bootstrapLog.textContent += msg + '\n';
                    bootstrapLog.scrollTop = bootstrapLog.scrollHeight;
                }
                if (debugLastStage) {
                    debugLastStage.textContent = data.stage;
                }
            } else if (data.type === 'ready') {
                console.log("Effect node ready (Wasm initialized).");
                debugReadyMsg.textContent = 'received';
                debugWasmInit.textContent = 'success';
                if (typeof debugWasmFetch !== 'undefined') debugWasmFetch.textContent = 'success';
                // Send initial parameter values
                syncAllParams();
                // Sync bypass state
                effectNode.port.postMessage({ param: 'bypass', value: bypassMode.checked });
            } else if (data.type === 'wasm_error') {
                console.error("Worklet reported Wasm error:", data.message);
                debugWasmInit.textContent = 'failed';
                if (typeof debugWasmFetch !== 'undefined') debugWasmFetch.textContent = 'failed';
            } else if (data.type === 'debug') {
                console.log("[Worklet Debug]", data.message);
            } else if (data.type === 'status') {
                console.log("[Worklet Status]", data.message);
                if (data.stage === 'fetch' && typeof debugWasmFetch !== 'undefined') debugWasmFetch.textContent = data.message;
                else if (data.stage === 'init') debugWasmInit.textContent = data.message;
            } else if (data.type === 'capabilities') {
                applyCapabilityMap(data.capabilities || {});
            } else if (data.type === 'parameter_unavailable') {
                console.warn(`Parameter unavailable in current WASM build: ${data.param} (${data.functionName})`);
            }
        };

        logBootstrapStage('before_postMessage', `init_wasm, bytes: ${wasmBytes.byteLength}`);
        effectNode.port.postMessage({ type: 'init_wasm', wasmBytes: wasmBytes });
        logBootstrapStage('after_postMessage', 'init_wasm sent');

        effectNode.connect(audioCtx.destination);
        console.log("AudioWorkletNode connected to destination.");
    } catch (err) {
        logBootstrapStage('init_error', `err: ${String(err)}`);
        console.error("Failed to initialize AudioWorklet:", err);
        debugWorkletLoad.textContent = 'failed';
    }
}

function stopCurrentSource() {
    if (activeSource === 'mic' && micStream) {
        micStream.getTracks().forEach(track => track.stop());
        micStream = null;
        if (micSource) {
            micSource.disconnect();
            micSource = null;
        }
    } else if (activeSource === 'file' && fileSource) {
        try { fileSource.stop(); } catch (e) { console.warn('Could not stop fileSource', e); }
        try { fileSource.disconnect(); } catch (e) { console.warn('Could not disconnect fileSource', e); }
        fileSource = null;
    }

    activeSource = null;
    isPlaying = false;
    stopBtn.disabled = true;
    startBtn.disabled = false;
    if (decodedAudioBuffer) {
        playFileBtn.disabled = false;
    }
    btnFile.disabled = false; // file input button is always enabled
    setVu(false);
}

async function startMic() {
    try {
        stopCurrentSource();

        if (!audioCtx) {
            await initAudioContext();
        }

        if (!effectNode) {
            console.error('Cannot start microphone: effectNode is not created.');
            alert('Cannot start microphone: AudioWorklet initialization failed. Please check the console.');
            return;
        }

        // Get Microphone
        micStream = await navigator.mediaDevices.getUserMedia({ audio: true, video: false });
        micSource = audioCtx.createMediaStreamSource(micStream);

        // Routing
        micSource.connect(effectNode);

        activeSource = 'mic';
        isPlaying = true;

        startBtn.disabled = true;
        playFileBtn.disabled = true;
        btnFile.disabled = true; // disable file selection while playing mic
        stopBtn.disabled = false;
        setVu(true);

        // Also resume context if it was suspended
        if (audioCtx.state === 'suspended') {
            await audioCtx.resume();
        }

    } catch (err) {
        console.error('Error starting microphone:', err);
        alert('Could not start microphone. Please ensure microphone permissions are granted.');
    }
}

async function startFile() {
    if (!decodedAudioBuffer) return;

    try {
        stopCurrentSource();

        if (!audioCtx) {
            await initAudioContext();
        }

        fileSource = audioCtx.createBufferSource();
        fileSource.buffer = decodedAudioBuffer;
        fileSource.loop = true; // Loop the file for continuous processing

        fileSource.connect(effectNode);
        fileSource.start();

        activeSource = 'file';
        isPlaying = true;

        startBtn.disabled = true;
        playFileBtn.disabled = true;
        btnFile.disabled = true; // disable file selection while playing
        stopBtn.disabled = false;
        setVu(true);

        if (audioCtx.state === 'suspended') {
            await audioCtx.resume();
        }

    } catch (err) {
        console.error('Error starting file playback:', err);
        alert('Could not start file playback. Check console for details.');
        stopCurrentSource(); // Reset state on error
    }
}

startBtn.addEventListener('click', startMic);
playFileBtn.addEventListener('click', startFile);
stopBtn.addEventListener('click', stopCurrentSource);

// Parameter Mapping
const sliderParams = [
    'time',
    'feedback',
    'mix',
    'shimmer',
    'diffusion',
    'chaos',
    'tone',
    'ducking',
    'wobble',
    'presence',
    'mod_rate',
    'mod_depth',
    'octave_feedback_amount',
    'melody_volume',
    'melody_density',
    'melody_decay'
];

const toggleParams = ['freeze', 'octave_feedback_enabled', 'melody_enabled', 'melody_only'];

const parameterTransforms = {
    time: (v) => Math.pow(v, 0.62),
    feedback: (v) => Math.min(0.985, 0.08 + Math.pow(v, 0.72) * 0.905),
    mix: (v) => Math.pow(v, 0.78),
    diffusion: (v) => Math.min(1, 0.06 + Math.pow(v, 0.60) * 0.94),
    chaos: (v) => Math.min(1, 0.02 + Math.pow(v, 0.82) * 0.98),
    wobble: (v) => Math.min(1, Math.pow(v, 0.68)),
    mod_rate: (v) => Math.min(1, 0.04 + Math.pow(v, 0.75) * 0.96),
    mod_depth: (v) => Math.min(1, Math.pow(v, 0.70)),
    presence: (v) => Math.min(1, Math.pow(v, 0.66)),
    ducking: (v) => Math.min(1, 0.02 + Math.pow(v, 0.62) * 0.98),
    shimmer: (v) => Math.min(1, 0.01 + Math.pow(v, 0.70) * 0.99),
    melody_volume: (v) => Math.min(1, Math.pow(v, 0.72)),
    melody_density: (v) => Math.min(1, 0.06 + Math.pow(v, 0.58) * 0.94),
    melody_decay: (v) => Math.min(1, 0.08 + Math.pow(v, 0.84) * 0.92),
    octave_feedback_amount: (v) => Math.min(1, 0.03 + Math.pow(v, 0.70) * 0.97)
};

function sendParam(param, value) {
    if (effectNode && effectNode.port) {
        const mapped = parameterTransforms[param] ? parameterTransforms[param](value) : value;
        effectNode.port.postMessage({ param, value: mapped });
    }
}

sliderParams.forEach((param) => {
    const slider = document.getElementById(param);
    const valDisplay = document.getElementById(`${param}Val`);
    if (!slider || !valDisplay) {
        console.warn(`[main] Missing slider binding for "${param}"`);
        return;
    }

    const updateDisplay = (value) => {
        valDisplay.textContent = value.toFixed(2);
    };

    updateDisplay(parseFloat(slider.value));
    slider.addEventListener('input', (e) => {
        const val = parseFloat(e.target.value);
        updateDisplay(val);
        sendParam(param, val);
    });
});

toggleParams.forEach((param) => {
    const toggle = document.getElementById(param);
    if (!toggle) {
        console.warn(`[main] Missing toggle binding for "${param}"`);
        return;
    }
    toggle.addEventListener('change', (e) => {
        sendParam(param, e.target.checked);
    });
});

bypassMode.addEventListener('change', (e) => {
    console.log("Bypass mode:", e.target.checked);
    sendParam('bypass', e.target.checked);
});

function syncAllParams() {
    if (!effectNode || !effectNode.port) return;
    sliderParams.forEach((param) => {
        const slider = document.getElementById(param);
        if (!slider) return;
        sendParam(param, parseFloat(slider.value));
    });
    toggleParams.forEach((param) => {
        const toggle = document.getElementById(param);
        if (!toggle) return;
        sendParam(param, toggle.checked);
    });
}

function getCurrentParamSnapshot() {
    const params = {};
    sliderParams.forEach((param) => {
        const slider = document.getElementById(param);
        if (!slider) return;
        params[param] = parseFloat(slider.value);
    });
    toggleParams.forEach((param) => {
        const toggle = document.getElementById(param);
        if (!toggle) return;
        params[param] = toggle.checked;
    });
    params.bypass = bypassMode.checked;
    return params;
}

function applyParamsToNode(node, params) {
    Object.entries(params).forEach(([param, rawValue]) => {
        const mapped = parameterTransforms[param] ? parameterTransforms[param](rawValue) : rawValue;
        node.port.postMessage({ param, value: mapped });
    });
}

function toInt16(float32Array) {
    const pcm = new Int16Array(float32Array.length);
    for (let i = 0; i < float32Array.length; i++) {
        const s = Math.max(-1, Math.min(1, float32Array[i]));
        pcm[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
    }
    return pcm;
}

function encodeMp3FromAudioBuffer(audioBuffer, kbps = 192) {
    if (typeof lamejs === 'undefined') {
        throw new Error('lamejs is unavailable.');
    }

    const numChannels = Math.min(2, audioBuffer.numberOfChannels);
    const left = toInt16(audioBuffer.getChannelData(0));
    const right = numChannels > 1 ? toInt16(audioBuffer.getChannelData(1)) : left;

    const encoder = new lamejs.Mp3Encoder(numChannels, audioBuffer.sampleRate, kbps);
    const chunkSize = 1152;
    const mp3Data = [];

    for (let i = 0; i < left.length; i += chunkSize) {
        const leftChunk = left.subarray(i, i + chunkSize);
        const mp3buf = numChannels > 1
            ? encoder.encodeBuffer(leftChunk, right.subarray(i, i + chunkSize))
            : encoder.encodeBuffer(leftChunk);
        if (mp3buf.length > 0) {
            mp3Data.push(mp3buf);
        }
    }

    const flushData = encoder.flush();
    if (flushData.length > 0) {
        mp3Data.push(flushData);
    }

    return new Blob(mp3Data, { type: 'audio/mpeg' });
}

async function exportProcessedMp3() {
    if (!decodedAudioBuffer) {
        alert('Please select an audio file before exporting.');
        return;
    }

    if (!window.OfflineAudioContext) {
        alert('Your browser does not support OfflineAudioContext for export.');
        return;
    }

    exportMp3Btn.disabled = true;
    exportMp3Btn.textContent = 'Exporting...';

    try {
        const wasmBytes = await fetchWasmBytes();
        const renderLength = decodedAudioBuffer.length;
        const sampleRate = decodedAudioBuffer.sampleRate;
        const offlineCtx = new OfflineAudioContext(2, renderLength, sampleRate);
        const workletUrl = new URL('./te2350-worklet.bundle.js', window.location.href);
        await offlineCtx.audioWorklet.addModule(workletUrl.href);

        const offlineNode = new AudioWorkletNode(offlineCtx, 'te2350-worklet', {
            numberOfInputs: 1,
            numberOfOutputs: 1,
            outputChannelCount: [2]
        });

        await new Promise((resolve, reject) => {
            let ready = false;
            const timeout = setTimeout(() => {
                if (!ready) reject(new Error('Timeout while initializing offline WASM worklet.'));
            }, 10000);

            offlineNode.port.onmessage = (event) => {
                const data = event.data;
                if (data.type === 'ready') {
                    ready = true;
                    clearTimeout(timeout);
                    resolve();
                } else if (data.type === 'wasm_error') {
                    clearTimeout(timeout);
                    reject(new Error(data.message || 'Unknown WASM error while exporting.'));
                }
            };

            offlineNode.port.postMessage({ type: 'init_wasm', wasmBytes });
        });

        applyParamsToNode(offlineNode, getCurrentParamSnapshot());

        const source = offlineCtx.createBufferSource();
        source.buffer = decodedAudioBuffer;
        source.connect(offlineNode);
        offlineNode.connect(offlineCtx.destination);
        source.start(0);

        const renderedBuffer = await offlineCtx.startRendering();
        const mp3Blob = encodeMp3FromAudioBuffer(renderedBuffer, 192);
        const downloadUrl = URL.createObjectURL(mp3Blob);

        const anchor = document.createElement('a');
        anchor.href = downloadUrl;
        anchor.download = `${uploadedFileName}-processed.mp3`;
        document.body.appendChild(anchor);
        anchor.click();
        anchor.remove();
        URL.revokeObjectURL(downloadUrl);
    } catch (err) {
        console.error('Failed to export MP3:', err);
        alert('Failed to export MP3. See console for details.');
    } finally {
        exportMp3Btn.textContent = '⇩ Export MP3';
        exportMp3Btn.disabled = !decodedAudioBuffer;
    }
}

exportMp3Btn.addEventListener('click', exportProcessedMp3);
