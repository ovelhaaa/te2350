async function createTe2350Module(moduleArg={}){var moduleRtn;var Module=moduleArg;var ENVIRONMENT_IS_AUDIO_WORKLET=!!globalThis.AudioWorkletGlobalScope;var arguments_=[];var thisProgram="./this.program";var scriptDirectory="";function locateFile(path){if(Module["locateFile"]){return Module["locateFile"](path,scriptDirectory)}return scriptDirectory+path}var readAsync,readBinary;{}var out=console.log.bind(console);var err=console.error.bind(console);var wasmBinary;var ABORT=false;var readyPromiseResolve,readyPromiseReject;var runtimeInitialized=false;function updateMemoryViews(){var b=wasmMemory.buffer;HEAP8=new Int8Array(b);HEAP16=new Int16Array(b);HEAPU8=new Uint8Array(b);HEAPU16=new Uint16Array(b);HEAP32=new Int32Array(b);HEAPU32=new Uint32Array(b);HEAPF32=new Float32Array(b);HEAPF64=new Float64Array(b);HEAP64=new BigInt64Array(b);HEAPU64=new BigUint64Array(b)}function preRun(){if(Module["preRun"]){if(typeof Module["preRun"]=="function")Module["preRun"]=[Module["preRun"]];while(Module["preRun"].length){addOnPreRun(Module["preRun"].shift())}}callRuntimeCallbacks(onPreRuns)}function initRuntime(){runtimeInitialized=true;wasmExports["c"]()}function postRun(){if(Module["postRun"]){if(typeof Module["postRun"]=="function")Module["postRun"]=[Module["postRun"]];while(Module["postRun"].length){addOnPostRun(Module["postRun"].shift())}}callRuntimeCallbacks(onPostRuns)}function abort(what){Module["onAbort"]?.(what);what="Aborted("+what+")";err(what);ABORT=true;what+=". Build with -sASSERTIONS for more info.";var e=new WebAssembly.RuntimeError(what);readyPromiseReject?.(e);throw e}var wasmBinaryFile;function findWasmBinary(){return locateFile("te2350.wasm")}function getBinarySync(file){if(file==wasmBinaryFile&&wasmBinary){return new Uint8Array(wasmBinary)}if(readBinary){return readBinary(file)}throw"both async and sync fetching of the wasm failed"}async function getWasmBinary(binaryFile){if(!wasmBinary){try{var response=await readAsync(binaryFile);return new Uint8Array(response)}catch{}}return getBinarySync(binaryFile)}async function instantiateArrayBuffer(binaryFile,imports){try{var binary=await getWasmBinary(binaryFile);var instance=await WebAssembly.instantiate(binary,imports);return instance}catch(reason){err(`failed to asynchronously prepare wasm: ${reason}`);abort(reason)}}async function instantiateAsync(binary,binaryFile,imports){if(!binary){try{var response=fetch(binaryFile,{credentials:"same-origin"});var instantiationResult=await WebAssembly.instantiateStreaming(response,imports);return instantiationResult}catch(reason){err(`wasm streaming compile failed: ${reason}`);err("falling back to ArrayBuffer instantiation")}}return instantiateArrayBuffer(binaryFile,imports)}function getWasmImports(){var imports={a:wasmImports};return imports}async function createWasm(){function receiveInstance(instance,module){wasmExports=instance.exports;assignWasmExports(wasmExports);updateMemoryViews();return wasmExports}function receiveInstantiationResult(result){return receiveInstance(result["instance"])}var info=getWasmImports();if(Module["instantiateWasm"]){return new Promise((resolve,reject)=>{Module["instantiateWasm"](info,(inst,mod)=>{resolve(receiveInstance(inst,mod))})})}wasmBinaryFile??=findWasmBinary();var result=await instantiateAsync(wasmBinary,wasmBinaryFile,info);var exports=receiveInstantiationResult(result);return exports}class ExitStatus{name="ExitStatus";constructor(status){this.message=`Program terminated with exit(${status})`;this.status=status}}var HEAP16;var HEAP32;var HEAP64;var HEAP8;var HEAPF32;var HEAPF64;var HEAPU16;var HEAPU32;var HEAPU64;var HEAPU8;var callRuntimeCallbacks=callbacks=>{while(callbacks.length>0){callbacks.shift()(Module)}};var onPostRuns=[];var addOnPostRun=cb=>onPostRuns.push(cb);var onPreRuns=[];var addOnPreRun=cb=>onPreRuns.push(cb);var noExitRuntime=true;var stackRestore=val=>__emscripten_stack_restore(val);var stackSave=()=>_emscripten_stack_get_current();var abortOnCannotGrowMemory=requestedSize=>{abort("OOM")};var _emscripten_resize_heap=requestedSize=>{var oldSize=HEAPU8.length;requestedSize>>>=0;abortOnCannotGrowMemory(requestedSize)};var getCFunc=ident=>{var func=Module["_"+ident];return func};var writeArrayToMemory=(array,buffer)=>{HEAP8.set(array,buffer)};var lengthBytesUTF8=str=>{var len=0;for(var i=0;i<str.length;++i){var c=str.charCodeAt(i);if(c<=127){len++}else if(c<=2047){len+=2}else if(c>=55296&&c<=57343){len+=4;++i}else{len+=3}}return len};var stringToUTF8Array=(str,heap,outIdx,maxBytesToWrite)=>{if(!(maxBytesToWrite>0))return 0;var startIdx=outIdx;var endIdx=outIdx+maxBytesToWrite-1;for(var i=0;i<str.length;++i){var u=str.codePointAt(i);if(u<=127){if(outIdx>=endIdx)break;heap[outIdx++]=u}else if(u<=2047){if(outIdx+1>=endIdx)break;heap[outIdx++]=192|u>>6;heap[outIdx++]=128|u&63}else if(u<=65535){if(outIdx+2>=endIdx)break;heap[outIdx++]=224|u>>12;heap[outIdx++]=128|u>>6&63;heap[outIdx++]=128|u&63}else{if(outIdx+3>=endIdx)break;heap[outIdx++]=240|u>>18;heap[outIdx++]=128|u>>12&63;heap[outIdx++]=128|u>>6&63;heap[outIdx++]=128|u&63;i++}}heap[outIdx]=0;return outIdx-startIdx};var stringToUTF8=(str,outPtr,maxBytesToWrite)=>stringToUTF8Array(str,HEAPU8,outPtr,maxBytesToWrite);var stackAlloc=sz=>__emscripten_stack_alloc(sz);var stringToUTF8OnStack=str=>{var size=lengthBytesUTF8(str)+1;var ret=stackAlloc(size);stringToUTF8(str,ret,size);return ret};var UTF8Decoder=globalThis.TextDecoder&&new TextDecoder;var findStringEnd=(heapOrArray,idx,maxBytesToRead,ignoreNul)=>{var maxIdx=idx+maxBytesToRead;if(ignoreNul)return maxIdx;while(heapOrArray[idx]&&!(idx>=maxIdx))++idx;return idx};var UTF8ArrayToString=(heapOrArray,idx=0,maxBytesToRead,ignoreNul)=>{var endPtr=findStringEnd(heapOrArray,idx,maxBytesToRead,ignoreNul);if(endPtr-idx>16&&heapOrArray.buffer&&UTF8Decoder){return UTF8Decoder.decode(heapOrArray.subarray(idx,endPtr))}var str="";while(idx<endPtr){var u0=heapOrArray[idx++];if(!(u0&128)){str+=String.fromCharCode(u0);continue}var u1=heapOrArray[idx++]&63;if((u0&224)==192){str+=String.fromCharCode((u0&31)<<6|u1);continue}var u2=heapOrArray[idx++]&63;if((u0&240)==224){u0=(u0&15)<<12|u1<<6|u2}else{u0=(u0&7)<<18|u1<<12|u2<<6|heapOrArray[idx++]&63}if(u0<65536){str+=String.fromCharCode(u0)}else{var ch=u0-65536;str+=String.fromCharCode(55296|ch>>10,56320|ch&1023)}}return str};var UTF8ToString=(ptr,maxBytesToRead,ignoreNul)=>ptr?UTF8ArrayToString(HEAPU8,ptr,maxBytesToRead,ignoreNul):"";var ccall=(ident,returnType,argTypes,args,opts)=>{var toC={string:str=>{var ret=0;if(str!==null&&str!==undefined&&str!==0){ret=stringToUTF8OnStack(str)}return ret},array:arr=>{var ret=stackAlloc(arr.length);writeArrayToMemory(arr,ret);return ret}};function convertReturnValue(ret){if(returnType==="string"){return UTF8ToString(ret)}if(returnType==="boolean")return Boolean(ret);return ret}var func=getCFunc(ident);var cArgs=[];var stack=0;if(args){for(var i=0;i<args.length;i++){var converter=toC[argTypes[i]];if(converter){if(stack===0)stack=stackSave();cArgs[i]=converter(args[i])}else{cArgs[i]=args[i]}}}var ret=func(...cArgs);function onDone(ret){if(stack!==0)stackRestore(stack);return convertReturnValue(ret)}ret=onDone(ret);return ret};var cwrap=(ident,returnType,argTypes,opts)=>{var numericArgs=!argTypes||argTypes.every(type=>type==="number"||type==="boolean");var numericRet=returnType!=="string";if(numericRet&&numericArgs&&!opts){return getCFunc(ident)}return(...args)=>ccall(ident,returnType,argTypes,args,opts)};{if(Module["noExitRuntime"])noExitRuntime=Module["noExitRuntime"];if(Module["print"])out=Module["print"];if(Module["printErr"])err=Module["printErr"];if(Module["wasmBinary"])wasmBinary=Module["wasmBinary"];if(Module["arguments"])arguments_=Module["arguments"];if(Module["thisProgram"])thisProgram=Module["thisProgram"];if(Module["preInit"]){if(typeof Module["preInit"]=="function")Module["preInit"]=[Module["preInit"]];while(Module["preInit"].length>0){Module["preInit"].shift()()}}}Module["cwrap"]=cwrap;var _wasm_te2350_init,_wasm_te2350_process_block,_wasm_te2350_set_time,_wasm_te2350_set_feedback,_wasm_te2350_set_mix,_wasm_te2350_set_octave_feedback,_wasm_te2350_set_shimmer,_wasm_te2350_set_diffusion,_wasm_te2350_set_chaos,_wasm_te2350_set_tone,_wasm_te2350_set_ducking,_wasm_te2350_set_wobble,_wasm_te2350_set_freeze,_wasm_te2350_set_mod,_wasm_te2350_set_melody,_malloc,_free,__emscripten_stack_restore,__emscripten_stack_alloc,_emscripten_stack_get_current,memory,__indirect_function_table,wasmMemory;function assignWasmExports(wasmExports){_wasm_te2350_init=Module["_wasm_te2350_init"]=wasmExports["d"];_wasm_te2350_process_block=Module["_wasm_te2350_process_block"]=wasmExports["e"];_wasm_te2350_set_time=Module["_wasm_te2350_set_time"]=wasmExports["f"];_wasm_te2350_set_feedback=Module["_wasm_te2350_set_feedback"]=wasmExports["g"];_wasm_te2350_set_mix=Module["_wasm_te2350_set_mix"]=wasmExports["h"];_wasm_te2350_set_octave_feedback=Module["_wasm_te2350_set_octave_feedback"]=wasmExports["i"];_wasm_te2350_set_shimmer=Module["_wasm_te2350_set_shimmer"]=wasmExports["j"];_wasm_te2350_set_diffusion=Module["_wasm_te2350_set_diffusion"]=wasmExports["k"];_wasm_te2350_set_chaos=Module["_wasm_te2350_set_chaos"]=wasmExports["l"];_wasm_te2350_set_tone=Module["_wasm_te2350_set_tone"]=wasmExports["m"];_wasm_te2350_set_ducking=Module["_wasm_te2350_set_ducking"]=wasmExports["n"];_wasm_te2350_set_wobble=Module["_wasm_te2350_set_wobble"]=wasmExports["o"];_wasm_te2350_set_freeze=Module["_wasm_te2350_set_freeze"]=wasmExports["p"];_wasm_te2350_set_mod=Module["_wasm_te2350_set_mod"]=wasmExports["q"];_wasm_te2350_set_melody=Module["_wasm_te2350_set_melody"]=wasmExports["r"];_malloc=Module["_malloc"]=wasmExports["s"];_free=Module["_free"]=wasmExports["t"];__emscripten_stack_restore=wasmExports["u"];__emscripten_stack_alloc=wasmExports["v"];_emscripten_stack_get_current=wasmExports["w"];memory=wasmMemory=wasmExports["b"];__indirect_function_table=wasmExports["__indirect_function_table"]}var wasmImports={a:_emscripten_resize_heap};function run(){preRun();function doRun(){Module["calledRun"]=true;if(ABORT)return;initRuntime();readyPromiseResolve?.(Module);Module["onRuntimeInitialized"]?.();postRun()}if(Module["setStatus"]){Module["setStatus"]("Running...");setTimeout(()=>{setTimeout(()=>Module["setStatus"](""),1);doRun()},1)}else{doRun()}}var wasmExports;wasmExports=await (createWasm());run();if(runtimeInitialized){moduleRtn=Module}else{moduleRtn=new Promise((resolve,reject)=>{readyPromiseResolve=resolve;readyPromiseReject=reject})}
;return moduleRtn}if(typeof exports==="object"&&typeof module==="object"){module.exports=createTe2350Module;module.exports.default=createTe2350Module}else if(typeof define==="function"&&define["amd"])define([],()=>createTe2350Module);
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
            case 'octave_feedback_enabled': this.octaveFeedbackEnabled = value; this.wasmModule._wasm_te2350_set_octave_feedback(this.octaveFeedbackEnabled ? 1 : 0, this.octaveFeedbackAmount || 0); break;
            case 'octave_feedback': this.octaveFeedbackAmount = value; this.wasmModule._wasm_te2350_set_octave_feedback(this.octaveFeedbackEnabled ? 1 : 0, this.octaveFeedbackAmount || 0); break;
            case 'melody_enabled': this.wasmModule._wasm_te2350_set_melody(value ? 1 : 0); break;
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
