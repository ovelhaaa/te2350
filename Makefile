# Simple top-level Makefile for building the offline test tool

offline:
	gcc -O2 -Iinclude src/test_offline.c src/te2350.c src/dsp_delay.c src/dsp_filters.c src/dsp_modulation.c src/dsp_pitch.c src/dsp_melody.c -o test_offline -lm

test: offline
	./test_offline

clean:
	rm -f test_offline test_impulse.wav test_sine_burst.wav
