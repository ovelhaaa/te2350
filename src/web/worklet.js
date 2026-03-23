// web/worklet.js
// Ensure we can fall back to standard globals if ES6 module loading fails
// In Makefile, we'll change it to not be an ES6 module because AudioWorklets
// can be very picky about them on some platforms (e.g. Chrome Android).
// So instead of `import Module from ...`, we use `importScripts`.

try {
    importScripts('te2350.js');
} catch (e) {
    console.error("Failed to importScripts('te2350.js'):", e);
}

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

        this.port.onmessage = this.handleMessage.bind(this);

        // Initialize Wasm Module
        this.initWasm();
    }

    async initWasm() {
        try {
            this.port.postMessage({ type: 'debug', message: 'Starting Wasm initialization...' });

            // Module is a factory function created by Emscripten
            // If we use importScripts, Module is available globally.
            if (typeof Module === 'undefined') {
                throw new Error("Module factory is not defined. Ensure importScripts loaded te2350.js successfully.");
            }

            this.wasmModule = await Module({
                // Ensure Wasm can be loaded relative to the worklet.js script
                locateFile: (path, prefix) => {
                    if (path.endsWith('.wasm')) {
                        return 'te2350.wasm';
                    }
                    return prefix + path;
                }
            });

            this.port.postMessage({ type: 'debug', message: 'Wasm module factory instantiated.' });

            // Call C init function
            const initSuccess = this.wasmModule._wasm_te2350_init();
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

            this.wasmLoaded = true;
            this.port.postMessage({ type: 'ready' });
            this.port.postMessage({ type: 'debug', message: 'Wasm loaded, buffers allocated, _wasm_te2350_init() succeeded.' });
            console.log("TE-2350 WASM module loaded and initialized.");
        } catch (e) {
            console.error("Error initializing WASM module:", e);
            this.port.postMessage({ type: 'wasm_error', message: e.toString() });
        }
    }

    handleMessage(event) {
        if (!this.wasmLoaded) return;

        const { param, value } = event.data;

        switch (param) {
            case 'bypass':
                this.bypassMode = value;
                this.port.postMessage({ type: 'debug', message: `Bypass mode set to ${this.bypassMode}` });
                break;
            case 'time': this.wasmModule._wasm_te2350_set_time(value); break;
            case 'feedback': this.wasmModule._wasm_te2350_set_feedback(value); break;
            case 'mix': this.wasmModule._wasm_te2350_set_mix(value); break;
            case 'shimmer': this.wasmModule._wasm_te2350_set_shimmer(value); break;
            case 'diffusion': this.wasmModule._wasm_te2350_set_diffusion(value); break;
            case 'chaos': this.wasmModule._wasm_te2350_set_chaos(value); break;
            case 'tone': this.wasmModule._wasm_te2350_set_tone(value); break;
            case 'ducking': this.wasmModule._wasm_te2350_set_ducking(value); break;
            case 'wobble': this.wasmModule._wasm_te2350_set_wobble(value); break;

            case 'mod_rate': this.modRate = value; this.wasmModule._wasm_te2350_set_mod(this.modRate, this.modDepth); break;
            case 'mod_depth': this.modDepth = value; this.wasmModule._wasm_te2350_set_mod(this.modRate, this.modDepth); break;
            case 'freeze': this.wasmModule._wasm_te2350_set_freeze(value ? 1 : 0); break;
        }
    }

    process(inputs, outputs, parameters) {
        if (!this.wasmLoaded) return true;

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
        const inView = new Float32Array(this.wasmModule.HEAPF32.buffer, this.inPtr, numSamples);
        const outLView = new Float32Array(this.wasmModule.HEAPF32.buffer, this.outLPtr, numSamples);
        const outRView = new Float32Array(this.wasmModule.HEAPF32.buffer, this.outRPtr, numSamples);

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
                for (let i = 0; i < numSamples; i++) {
                    if (Math.abs(inView[i]) > 0.0001) {
                        hasInputSignal = true;
                        break;
                    }
                }
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
                for (let i = 0; i < numSamples; i++) {
                    if (Math.abs(outLView[i]) > 0.0001) {
                        hasOutputSignal = true;
                        break;
                    }
                }
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

registerProcessor('te2350-worklet', TE2350WorkletProcessor);
