#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Audio Config
#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_BLOCK_SIZE 128 // Increased for stability on RP2350

// Callback type
// Input:  Pointer to incoming samples (Interleaved L/R Q31)
// Output: Pointer to outgoing samples (Interleaved L/R Q31)
// Count: Number of stereo frames (samples pairs)
typedef void (*audio_callback_t)(int32_t *rx_buffer, int32_t *tx_buffer,
                                 size_t sample_count);

/**
 * @brief Initialize the I2S Audio Driver.
 *
 * Configures PIO I2S (Pins 9-12) and MCLK (Pin 13).
 * Sets up DMA for full duplex transfer.
 *
 * @param callback The function to call when a block of audio is ready/needed.
 * @return true on success.
 */
bool audio_init(audio_callback_t callback);

// Internal tick function if using polling (optional, we'll try to use IRQ)
void audio_poll();

#endif // AUDIO_DRIVER_H
