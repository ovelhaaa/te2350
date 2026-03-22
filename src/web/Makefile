CC = emcc
CFLAGS = -O3 -s WASM=1 -s MODULARIZE=1 -s EXPORT_ES6=1 -s EXPORTED_RUNTIME_METHODS='["cwrap"]' -s EXPORTED_FUNCTIONS='["_wasm_te2350_init", "_wasm_te2350_process_block", "_wasm_te2350_set_time", "_wasm_te2350_set_feedback", "_wasm_te2350_set_mix", "_wasm_te2350_set_shimmer", "_wasm_te2350_set_diffusion", "_wasm_te2350_set_chaos", "_wasm_te2350_set_tone", "_wasm_te2350_set_ducking", "_wasm_te2350_set_wobble", "_wasm_te2350_set_freeze", "_wasm_te2350_set_mod", "_malloc", "_free"]' --no-entry

SRC = wasm_wrapper.c ../te2350.c ../dsp_delay.c ../dsp_filters.c ../dsp_modulation.c ../dsp_pitch.c
INC = -I../../include

TARGET = te2350.js

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(INC) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) te2350.wasm
