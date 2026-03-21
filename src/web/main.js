// web/main.js

let audioCtx;
let effectNode;
let micStream;
let isPlaying = false;

const startBtn = document.getElementById('startBtn');
const warningDiv = document.getElementById('warning');

async function initAudio() {
    try {
        // Request 48kHz for highest fidelity to the original DSP code
        audioCtx = new (window.AudioContext || window.webkitAudioContext)({
            sampleRate: 48000
        });

        // Fidelity check
        if (audioCtx.sampleRate !== 48000) {
            warningDiv.style.display = 'block';
            warningDiv.textContent = `Warning: AudioContext running at ${audioCtx.sampleRate}Hz instead of 48000Hz. This may affect delay times, filter frequencies, and overall fidelity compared to the original hardware.`;
        }

        // Load the AudioWorklet
        await audioCtx.audioWorklet.addModule('worklet.js');
        effectNode = new AudioWorkletNode(audioCtx, 'te2350-worklet', {
            numberOfInputs: 1,
            numberOfOutputs: 1,
            outputChannelCount: [2] // Stereo output
        });

        // Listen for when Wasm is ready
        effectNode.port.onmessage = (event) => {
            if (event.data.type === 'ready') {
                console.log("Effect node ready.");
                // Send initial parameter values
                syncAllParams();
            }
        };

        // Get Microphone
        micStream = await navigator.mediaDevices.getUserMedia({ audio: true, video: false });
        const micSource = audioCtx.createMediaStreamSource(micStream);

        // Routing
        micSource.connect(effectNode);
        effectNode.connect(audioCtx.destination);

        isPlaying = true;
        startBtn.textContent = 'Stop Audio';
        startBtn.style.background = '#f44336'; // Red

        // Also resume context if it was suspended
        if (audioCtx.state === 'suspended') {
            await audioCtx.resume();
        }

    } catch (err) {
        console.error('Error starting audio:', err);
        alert('Could not start audio. Please ensure microphone permissions are granted.');
    }
}

function stopAudio() {
    if (audioCtx) {
        audioCtx.close();
        audioCtx = null;
    }
    if (micStream) {
        micStream.getTracks().forEach(track => track.stop());
        micStream = null;
    }
    isPlaying = false;
    startBtn.textContent = 'Start Audio (Mic)';
    startBtn.style.background = '#4caf50'; // Green
}

startBtn.addEventListener('click', () => {
    if (isPlaying) {
        stopAudio();
    } else {
        initAudio();
    }
});

// Parameter Mapping
const params = ['time', 'feedback', 'mix', 'shimmer', 'diffusion', 'chaos', 'tone', 'ducking', 'wobble'];

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

function syncAllParams() {
    if (!effectNode || !effectNode.port) return;
    params.forEach(param => {
        const slider = document.getElementById(param);
        effectNode.port.postMessage({ param: param, value: parseFloat(slider.value) });
    });
    const freeze = document.getElementById('freeze').checked;
    effectNode.port.postMessage({ param: 'freeze', value: freeze });
}
