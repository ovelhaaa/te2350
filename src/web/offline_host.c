#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../include/te2350.h"
#include "../../include/dsp_math.h"

#define MEM_POOL_SIZE (128 * 1024)
static q31_t memory_pool[MEM_POOL_SIZE / 4];
static te2350_t pedal;

#define SR 48000
#define SECONDS 2
#define NUM_SAMPLES (SR * SECONDS)

int main() {
    printf("Starting offline validation host...\n");

    if (!te2350_init(&pedal, memory_pool, MEM_POOL_SIZE)) {
        printf("Failed to init te2350\n");
        return 1;
    }

    // Set some basic parameters
    te2350_set_time(&pedal, FLOAT_TO_Q31(0.8f));
    te2350_set_feedback(&pedal, FLOAT_TO_Q31(0.5f));
    te2350_set_mix(&pedal, FLOAT_TO_Q31(0.5f));

    printf("Generating %d samples...\n", NUM_SAMPLES);

    FILE* out_file = fopen("output.raw", "wb");
    if (!out_file) {
        printf("Failed to open output.raw for writing\n");
        return 1;
    }

    // Process loop
    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Generate an impulse at the beginning
        q31_t in_mono = 0;
        if (i == 0) {
            in_mono = FLOAT_TO_Q31(1.0f);
        }

        q31_t out_l, out_r;
        te2350_process(&pedal, in_mono, &out_l, &out_r);

        // Write out as 16-bit PCM stereo
        int16_t l_16 = out_l >> 16;
        int16_t r_16 = out_r >> 16;
        fwrite(&l_16, sizeof(int16_t), 1, out_file);
        fwrite(&r_16, sizeof(int16_t), 1, out_file);
    }

    fclose(out_file);
    printf("Validation complete. Wrote output.raw (48kHz, 16-bit, stereo)\n");
    return 0;
}
