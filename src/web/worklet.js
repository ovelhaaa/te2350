// web/worklet.js
// This file is concatenated directly after `te2350.js` in `te2350-worklet.bundle.js`.
// Because of this, `createTe2350Module` is already declared in this file's scope.

class TE2350WorkletProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this.wasmLoaded = false;
        this.bypassMode = false;

        // Debugging state
        this.logCounter = 0;
        this.hasLoggedWasmInit = false;

        // Block size is 128
        this.blockSize = 128;
        this.modRate = 0.5;
        this.modDepth = 0.4;
        this.reportedUnavailableParams = new Set();

        this.port.onmessage = this.handleMessage.bind(this);

        // Do not call initWasm immediately. Wait for the main thread to pass the wasm bytes.
        this.port.postMessage({ type: 'status', stage: 'init', message: 'waiting for wasm payload...' });
    }

    async initWasm(wasmBytes) {
        try {
            this.port.postMessage({ type: 'worklet_debug', stage: 'init_wasm_start', details: `bytes: ${wasmBytes?.byteLength || 0}` });
            this.port.postMessage({ type: 'status', stage: 'init', message: 'pending...' });

            if (typeof createTe2350Module !== 'function') {
                const keys = typeof globalThis !== 'undefined' ? Object.keys(globalThis).filter(k => k.toLowerCase().includes('create') || k.toLowerCase().includes('module') || k.toLowerCase().includes('te2350')) : [];
                throw new Error(`Wrapper bundled but no factory symbol found (expected createTe2350Module). Found globals: ${keys.join(', ')}`);
            }

            this.port.postMessage({ type: 'worklet_debug', stage: 'before_create_module' });
            this.port.postMessage({ type: 'status', stage: 'init', message: 'instantiating custom wasm...' });

            this.wasmModule = await createTe2350Module({
                instantiateWasm: (imports, successCallback) => {
                    this.port.postMessage({ type: 'worklet_debug', stage: 'instantiateWasm_enter' });
                    WebAssembly.instantiate(wasmBytes, imports).then(result => {
                        this.port.postMessage({ type: 'worklet_debug', stage: 'instantiateWasm_done' });
                        this.wasmInstance = result.instance;
                        successCallback(result.instance, result.module);
                    }).catch(e => {
                        this.port.postMessage({ type: 'worklet_debug', stage: 'instantiateWasm_error', details: String(e) });
                        this.port.postMessage({ type: 'wasm_error', message: "WebAssembly instantiation failed: " + e.message });
                    });
                    // We must return an empty object to indicate we are handling instantiation asynchronously
                    return {};
                }
            });

            this.port.postMessage({ type: 'worklet_debug', stage: 'module_created' });
            this.port.postMessage({ type: 'status', stage: 'init', message: 'initializing DSP...' });

            // Call C init function
            // Get sampleRate from the AudioWorkletGlobalScope
            const initSuccess = this.wasmModule._wasm_te2350_init(sampleRate);
            if (!initSuccess) {
                this.port.postMessage({ type: 'wasm_error', message: "WASM TE-2350 init returned false." });
                console.error("WASM TE-2350 init failed.");
                return;
            }

            // Allocate memory in Wasm for I/O buffers (float arrays)
            const bytesPerFloat = 4;
            this.inPtr = this.wasmModule._malloc(this.blockSize * bytesPerFloat);
            this.outLPtr = this.wasmModule._malloc(this.blockSize * bytesPerFloat);
            this.outRPtr = this.wasmModule._malloc(this.blockSize * bytesPerFloat);

            if (!this.inPtr || !this.outLPtr || !this.outRPtr) {
                throw new Error("Failed to allocate I/O buffers in Wasm memory.");
            }

            // Cache the WebAssembly.Memory instance to avoid GC allocations in the process loop
            this.wasmMemory = this.wasmInstance.exports.memory || Object.values(this.wasmInstance.exports).find(x => x instanceof WebAssembly.Memory);

            this.wasmLoaded = true;
            this._reportCapabilities();
            this.port.postMessage({ type: 'ready' });
            this.port.postMessage({ type: 'worklet_debug', stage: 'ready_sent' });
            this.port.postMessage({ type: 'status', stage: 'init', message: 'success' });
            console.log("TE-2350 WASM module loaded and initialized.");
        } catch (e) {
            console.error("Error initializing WASM module:", e);
            this.port.postMessage({
                type: 'worklet_debug',
                stage: 'init_wasm_error',
                details: `err: ${String(e)}, stack: ${e?.stack || ''}`
            });
            this.port.postMessage({ type: 'wasm_error', message: e.toString() });
        }
    }

    handleMessage(event) {
        const data = event.data;
        this.port.postMessage({ type: 'worklet_debug', stage: 'message_received', details: `msgType: ${data?.type || data?.param || 'unknown'}` });

        if (data.type === 'init_wasm') {
            this.initWasm(data.wasmBytes);
            return;
        }

        if (!this.wasmLoaded) return;

        const { param, value } = data;

        switch (param) {
            case 'bypass':
                this.bypassMode = value;
                this.port.postMessage({ type: 'debug', message: `Bypass mode set to ${this.bypassMode}` });
                break;
            case 'octave_feedback_enabled':
                this.octaveFeedbackEnabled = value;
                this._invokeWasm('_wasm_te2350_set_octave_feedback_enabled', [this.octaveFeedbackEnabled ? 1 : 0], param);
                break;
            case 'octave_feedback_amount':
                this.octaveFeedbackAmount = value;
                this._invokeWasm('_wasm_te2350_set_octave_feedback_amount', [this.octaveFeedbackAmount || 0], param);
                break;
            case 'octave_feedback':
                // Backward compatibility for older UIs.
                this.octaveFeedbackAmount = value;
                this._invokeWasm('_wasm_te2350_set_octave_feedback_amount', [this.octaveFeedbackAmount || 0], 'octave_feedback_amount');
                break;
            case 'melody_enabled':
                this._invokeWasm('_wasm_te2350_set_melody', [value ? 1 : 0], param);
                break;
            case 'melody_only':
                this._invokeWasm('_wasm_te2350_set_melody_only', [value ? 1 : 0], param);
                break;
            case 'melody_volume':
                this._invokeWasm('_wasm_te2350_set_melody_volume', [value], param);
                break;
            case 'melody_density':
                this._invokeWasm('_wasm_te2350_set_melody_density', [value], param);
                break;
            case 'melody_decay':
                this._invokeWasm('_wasm_te2350_set_melody_decay', [value], param);
                break;
            case 'time':
                this._invokeWasm('_wasm_te2350_set_time', [value], param);
                break;
            case 'feedback':
                this._invokeWasm('_wasm_te2350_set_feedback', [value], param);
                break;
            case 'mix':
                this._invokeWasm('_wasm_te2350_set_mix', [value], param);
                break;
            case 'shimmer':
                this._invokeWasm('_wasm_te2350_set_shimmer', [value], param);
                break;
            case 'diffusion':
                this._invokeWasm('_wasm_te2350_set_diffusion', [value], param);
                break;
            case 'chaos':
                this._invokeWasm('_wasm_te2350_set_chaos', [value], param);
                break;
            case 'tone':
                this._invokeWasm('_wasm_te2350_set_tone', [value], param);
                break;
            case 'ducking':
                this._invokeWasm('_wasm_te2350_set_ducking', [value], param);
                break;
            case 'wobble':
                this._invokeWasm('_wasm_te2350_set_wobble', [value], param);
                break;
            case 'presence':
                this._invokeWasm('_wasm_te2350_set_presence', [value], param);
                break;
            case 'mod_rate':
                this.modRate = value;
                this._invokeWasm('_wasm_te2350_set_mod', [this.modRate, this.modDepth], param);
                break;
            case 'mod_depth':
                this.modDepth = value;
                this._invokeWasm('_wasm_te2350_set_mod', [this.modRate, this.modDepth], param);
                break;
            case 'freeze':
                this._invokeWasm('_wasm_te2350_set_freeze', [value ? 1 : 0], param);
                break;
        }
    }

    _hasWasmFn(fnName) {
        return !!(this.wasmModule && typeof this.wasmModule[fnName] === 'function');
    }

    _invokeWasm(fnName, args, paramName) {
        if (!this._hasWasmFn(fnName)) {
            if (!this.reportedUnavailableParams.has(paramName)) {
                this.reportedUnavailableParams.add(paramName);
                this.port.postMessage({
                    type: 'parameter_unavailable',
                    param: paramName,
                    functionName: fnName
                });
            }
            return;
        }
        this.wasmModule[fnName](...args);
    }

    _reportCapabilities() {
        const OPTIONAL_CAPABILITIES = {
            presence: '_wasm_te2350_set_presence',
            octave_feedback_enabled: '_wasm_te2350_set_octave_feedback_enabled',
            octave_feedback_amount: '_wasm_te2350_set_octave_feedback_amount',
            melody_enabled: '_wasm_te2350_set_melody',
            melody_only: '_wasm_te2350_set_melody_only',
            melody_volume: '_wasm_te2350_set_melody_volume',
            melody_density: '_wasm_te2350_set_melody_density',
            melody_decay: '_wasm_te2350_set_melody_decay'
        };

        const capabilityMap = {};
        for (const param in OPTIONAL_CAPABILITIES) {
            capabilityMap[param] = this._hasWasmFn(OPTIONAL_CAPABILITIES[param]);
        }

        this.port.postMessage({
            type: 'capabilities',
            capabilities: capabilityMap
        });
    }

    _hasSignal(channelData, numSamples) {
        for (let i = 0; i < numSamples; i++) {
            if (Math.abs(channelData[i]) > 0.0001) {
                return true;
            }
        }
        return false;
    }

    process(inputs, outputs, parameters) {
        if (!this.wasmLoaded || !this.wasmInstance || !this.wasmMemory || !this.inPtr || !this.outLPtr || !this.outRPtr) {
            return true;
        }

        const input = inputs[0];
        const output = outputs[0];

        // Web Audio process block size is 128 frames
        const inChannel = input.length > 0 ? input[0] : null;
        const outLChannel = output[0];
        const outRChannel = output[1];

        // Ensure we have a valid block size (should be 128)
        let numSamples = 128;
        if (inChannel && inChannel.length < numSamples) {
             numSamples = inChannel.length;
        }

        // --- Diagnostic Bypass Mode ---
        if (this.bypassMode) {
            if (inChannel && outLChannel) {
                outLChannel.set(inChannel);
            }
            if (inChannel && outRChannel) {
                // If stereo input, use right channel, otherwise copy left to right
                if (input.length > 1) {
                    outRChannel.set(input[1]);
                } else {
                    outRChannel.set(inChannel);
                }
            }
            return true;
        }

        // Recreate views to easily write/read and avoid detached buffer errors if Wasm memory grew
        const inView = new Float32Array(this.wasmMemory.buffer, this.inPtr, numSamples);
        const outLView = new Float32Array(this.wasmMemory.buffer, this.outLPtr, numSamples);
        const outRView = new Float32Array(this.wasmMemory.buffer, this.outRPtr, numSamples);

        // Copy input to Wasm memory (convert stereo to mono sum if needed, but we'll just take L for now)
        let hasInputSignal = false;
        if (inChannel) {
            // We use simple set for the mono input
            inView.set(inChannel.subarray(0, numSamples));
            // If there's a right channel, we could mix it, but let's just use left
            if (input.length > 1) {
                for(let i=0; i<numSamples; i++){
                    inView[i] = (inChannel[i] + input[1][i]) * 0.5;
                }
            }

            // Check for signal occasionally
            if (this.logCounter % 48000 === 0) { // Approx once per second
                hasInputSignal = this._hasSignal(inView, numSamples);
            }
        } else {
            inView.fill(0);
        }

        // Call WASM process function
        this.wasmModule._wasm_te2350_process_block(this.inPtr, this.outLPtr, this.outRPtr, numSamples);

        // Copy output from Wasm memory to output buffers
        let hasOutputSignal = false;
        if (outLChannel) {
            outLChannel.set(outLView);

            if (this.logCounter % 48000 === 0) {
                hasOutputSignal = this._hasSignal(outLView, numSamples);
            }
        }
        if (outRChannel) {
            outRChannel.set(outRView);
        }

        // Debug logging (throttled)
        if (this.logCounter % 48000 === 0) {
            if (inChannel) {
                this.port.postMessage({
                    type: 'debug',
                    message: `[Process] input_active=${hasInputSignal}, wasm_output_active=${hasOutputSignal}`
                });
            } else {
                this.port.postMessage({
                    type: 'debug',
                    message: `[Process] NO INPUT CHANNEL CONNECTED`
                });
            }
        }
        this.logCounter += numSamples;

        return true;
    }
}

try {
    registerProcessor('te2350-worklet', TE2350WorkletProcessor);
    console.log("registerProcessor succeeded for te2350-worklet");
} catch (e) {
    console.error("registerProcessor failed:", e);
}
