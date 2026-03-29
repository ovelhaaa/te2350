CC = emcc
CFLAGS = -O3 -s WASM=1 -s MODULARIZE=1 -s EXPORT_ES6=0 -s ENVIRONMENT=worklet -s EXPORT_NAME="createTe2350Module" -s EXPORTED_RUNTIME_METHODS='["cwrap"]' -s EXPORTED_FUNCTIONS='["_wasm_te2350_init", "_wasm_te2350_process_block", "_wasm_te2350_set_time", "_wasm_te2350_set_feedback", "_wasm_te2350_set_mix", "_wasm_te2350_set_octave_feedback_enabled", "_wasm_te2350_set_octave_feedback_amount", "_wasm_te2350_set_shimmer", "_wasm_te2350_set_diffusion", "_wasm_te2350_set_chaos", "_wasm_te2350_set_tone", "_wasm_te2350_set_ducking", "_wasm_te2350_set_wobble", "_wasm_te2350_set_freeze", "_wasm_te2350_set_mod", "_wasm_te2350_set_melody", "_malloc", "_free"]' --no-entry

SRC = wasm_wrapper.c ../te2350.c ../dsp_delay.c ../dsp_filters.c ../dsp_modulation.c ../dsp_pitch.c ../dsp_melody.c
INC = -I../../include

WRAPPER = te2350.js
WORKLET = worklet.js
TARGET = te2350-worklet.bundle.js

all: $(TARGET)

$(WRAPPER): $(SRC)
	$(CC) $(CFLAGS) $(INC) $(SRC) -o $(WRAPPER)

$(TARGET): $(WRAPPER) $(WORKLET)
	cat $(WRAPPER) $(WORKLET) > $(TARGET)

clean:
	rm -f $(WRAPPER) te2350.wasm $(TARGET)
