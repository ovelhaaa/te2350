#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../include/te2350.h"

// 64KB memory for offline test
#define MEM_POOL_SIZE (128 * 1024)
static q31_t memory_pool[MEM_POOL_SIZE / 4];

static te2350_t pedal;

// Helper to write raw 16-bit PCM
void write_wav_header(FILE *f, int num_samples, int sample_rate) {
    int byte_rate = sample_rate * 2 * 2; // 16-bit stereo

    fwrite("RIFF", 1, 4, f);
    int chunk_size = 36 + num_samples * 4;
    fwrite(&chunk_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    int subchunk1_size = 16;
    fwrite(&subchunk1_size, 4, 1, f);
    short audio_format = 1;
    fwrite(&audio_format, 2, 1, f);
    short num_channels = 2;
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    short block_align = 4;
    fwrite(&block_align, 2, 1, f);
    short bits_per_sample = 16;
    fwrite(&bits_per_sample, 2, 1, f);
    fwrite("data", 1, 4, f);
    int subchunk2_size = num_samples * 4;
    fwrite(&subchunk2_size, 4, 1, f);
}

void render_test_signal(const char* filename, float sample_rate, int type, int duration_sec) {
    int num_samples = duration_sec * (int)sample_rate;

    // Reset effect
    if (!te2350_init(&pedal, memory_pool, MEM_POOL_SIZE, sample_rate)) {
        printf("Effect Init FAILED\n");
        return;
    }

    // Configure to showcase character
    te2350_set_time(&pedal, FLOAT_TO_Q31(0.7f));
    te2350_set_feedback(&pedal, FLOAT_TO_Q31(0.9f));
    te2350_set_mix(&pedal, FLOAT_TO_Q31(0.6f));
    te2350_set_shimmer(&pedal, FLOAT_TO_Q31(0.3f));

    FILE *f = fopen(filename, "wb");
    if (!f) return;

    write_wav_header(f, num_samples, (int)sample_rate);

    double peak_l = 0;
    double sum_sq_l = 0;

    // Process loop
    for (int i = 0; i < num_samples; i++) {
        // Run smoothing
        q31_t in_q31 = 0;

        // Input Generation
        if (type == 0) {
            // Impulse
            if (i == 0) in_q31 = FLOAT_TO_Q31(0.9f);
        } else if (type == 1) {
            // Short sine burst
            if (i < sample_rate * 0.1) {
                float val = sin(2.0 * 3.1415926535 * 440.0 * i / sample_rate);
                in_q31 = float_to_q31_safe(val * 0.5f);
            }
        }

        q31_t out_l, out_r;
        te2350_process(&pedal, in_q31, &out_l, &out_r);

        // Stats
        float fl = Q31_TO_FLOAT(out_l);
        if (fabs(fl) > peak_l) peak_l = fabs(fl);
        sum_sq_l += fl * fl;

        // Write 16-bit
        short pcm_l = (short)(fl * 32767.0f);
        float fr = Q31_TO_FLOAT(out_r);
        short pcm_r = (short)(fr * 32767.0f);

        fwrite(&pcm_l, 2, 1, f);
        fwrite(&pcm_r, 2, 1, f);
    }

    fclose(f);

    double rms = sqrt(sum_sq_l / num_samples);
    printf("Rendered %s: Peak = %.4f, RMS = %.4f\n", filename, peak_l, rms);
}

int main() {
    printf("TE-2350 Offline Validation Tool\n");
    printf("-------------------------------\n");

    render_test_signal("test_impulse.wav", 48000.0f, 0, 3);
    render_test_signal("test_sine_burst.wav", 48000.0f, 1, 3);

    return 0;
}
