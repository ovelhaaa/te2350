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
const stopBtn = document.getElementById('stopBtn');
const fileInput = document.getElementById('fileInput');
const warningDiv = document.getElementById('warning');

// Debug UI Elements
const debugCtxState = document.getElementById('debugCtxState');
const debugSampleRate = document.getElementById('debugSampleRate');
const debugWorkletLoad = document.getElementById('debugWorkletLoad');
const debugWasmInit = document.getElementById('debugWasmInit');
const debugReadyMsg = document.getElementById('debugReadyMsg');
const debugActiveSource = document.getElementById('debugActiveSource');
const bypassMode = document.getElementById('bypassMode');

function updateDebugUI() {
    if (audioCtx) {
        debugCtxState.textContent = audioCtx.state;
        debugSampleRate.textContent = audioCtx.sampleRate + ' Hz';
    }
    debugActiveSource.textContent = activeSource ? activeSource : 'none';
}
setInterval(updateDebugUI, 500); // Poll context state occasionally

let decodedAudioBuffer = null;

fileInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) {
        playFileBtn.disabled = true;
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
        console.log(`Successfully decoded ${file.name}`);
    } catch (err) {
        console.error('Error decoding audio file:', err);
        alert('Failed to decode the selected audio file.');
        playFileBtn.disabled = true;
        decodedAudioBuffer = null;
    }
});

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
        console.log("Adding AudioWorklet module...");
        debugWorkletLoad.textContent = 'loading...';
        await audioCtx.audioWorklet.addModule('worklet.js');
        debugWorkletLoad.textContent = 'success';
        console.log("AudioWorklet module added successfully.");

        console.log("Creating AudioWorkletNode...");
        effectNode = new AudioWorkletNode(audioCtx, 'te2350-worklet', {
            numberOfInputs: 1,
            numberOfOutputs: 1,
            outputChannelCount: [2] // Stereo output
        });
        console.log("AudioWorkletNode created successfully.");

        // Listen for when Wasm is ready
        effectNode.port.onmessage = (event) => {
            if (event.data.type === 'ready') {
                console.log("Effect node ready (Wasm initialized).");
                debugReadyMsg.textContent = 'received';
                debugWasmInit.textContent = 'success';
                // Send initial parameter values
                syncAllParams();
                // Sync bypass state
                effectNode.port.postMessage({ param: 'bypass', value: bypassMode.checked });
            } else if (event.data.type === 'wasm_error') {
                console.error("Worklet reported Wasm error:", event.data.message);
                debugWasmInit.textContent = 'failed';
            } else if (event.data.type === 'debug') {
                console.log("[Worklet Debug]", event.data.message);
            }
        };

        effectNode.connect(audioCtx.destination);
        console.log("AudioWorkletNode connected to destination.");
    } catch (err) {
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
}

async function startMic() {
    try {
        stopCurrentSource();

        if (!audioCtx) {
            await initAudioContext();
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
        stopBtn.disabled = false;

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
        stopBtn.disabled = false;

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
const params = ['time', 'feedback', 'mix', 'shimmer', 'diffusion', 'chaos', 'tone', 'ducking', 'wobble', 'mod_rate', 'mod_depth'];

params.forEach(param => {
    const slider = document.getElementById(param);
    const valDisplay = document.getElementById(param + 'Val');

    slider.addEventListener('input', (e) => {
        const val = parseFloat(e.target.value);
        valDisplay.textContent = val.toFixed(2);
        if (effectNode && effectNode.port) {
            effectNode.port.postMessage({ param: param, value: val });
        }
    });
});

document.getElementById('freeze').addEventListener('change', (e) => {
    if (effectNode && effectNode.port) {
        effectNode.port.postMessage({ param: 'freeze', value: e.target.checked });
    }
});

bypassMode.addEventListener('change', (e) => {
    console.log("Bypass mode:", e.target.checked);
    if (effectNode && effectNode.port) {
        effectNode.port.postMessage({ param: 'bypass', value: e.target.checked });
    }
});

function syncAllParams() {
    if (!effectNode || !effectNode.port) return;
    params.forEach(param => {
        const slider = document.getElementById(param);
        effectNode.port.postMessage({ param: param, value: parseFloat(slider.value) });
    });
    const freeze = document.getElementById('freeze').checked;
    effectNode.port.postMessage({ param: 'freeze', value: freeze });
}
