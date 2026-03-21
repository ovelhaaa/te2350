#include "audio_driver.h"
#include "audio_i2s.pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/structs/dma.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>

// --- I2S Pins ---
#define PIN_BCLK 0   // Bit Clock (output, master)
#define PIN_LRCK 1   // Word Select / LR Clock (output, master)
#define PIN_DOUT 2   // Data Out (to USB interface)
#define PIN_DIN  3   // Data In (from USB interface)

#define AUDIO_PIO pio0
#define SM_TX     0   // State Machine for TX
#define SM_RX     1   // State Machine for RX

// 128 samples * 2 (L/R) = 256 words
#define WORDS_PER_BLOCK (AUDIO_BLOCK_SIZE * 2)

static int32_t tx_buf[2][WORDS_PER_BLOCK] __attribute__((aligned(8)));
static int32_t rx_buf[2][WORDS_PER_BLOCK] __attribute__((aligned(8)));

static int dma_tx;
static int dma_rx;
static volatile uint8_t next_buf_idx = 0;

static audio_callback_t user_callback = NULL;

// --- DMA IRQ Handler ---
void __isr __not_in_flash_func(audio_dma_handler)(void) {
    // Clear the interrupt
    dma_hw->ints0 = (1u << dma_tx);
    
    // The buffer that JUST finished is (next_buf_idx ^ 1)
    uint8_t finished_idx = next_buf_idx;
    next_buf_idx ^= 1;
    
    // Trigger NEXT transfer immediately to minimize gaps
    dma_channel_set_read_addr(dma_tx, tx_buf[next_buf_idx], true);
    dma_channel_set_write_addr(dma_rx, rx_buf[next_buf_idx], true);
    
    // Process the buffer that we just swapped out from
    if (user_callback) {
        user_callback(rx_buf[finished_idx], tx_buf[finished_idx], AUDIO_BLOCK_SIZE);
    }
}

bool audio_init(audio_callback_t callback) {
    user_callback = callback;
    memset(tx_buf, 0, sizeof(tx_buf));
    memset(rx_buf, 0, sizeof(rx_buf));

    // ============================================
    // 1. PIO TX Setup (Master with clock generation)
    // ============================================
    uint offset_tx = pio_add_program(AUDIO_PIO, &audio_i2s_tx_program);
    pio_sm_config c_tx = audio_i2s_tx_program_get_default_config(offset_tx);
    
    // Sideset: BCLK, LRCK
    sm_config_set_sideset_pins(&c_tx, PIN_BCLK);
    sm_config_set_out_pins(&c_tx, PIN_DOUT, 1);
    
    // Configure TX GPIO pins
    pio_gpio_init(AUDIO_PIO, PIN_BCLK);
    pio_gpio_init(AUDIO_PIO, PIN_LRCK);
    pio_gpio_init(AUDIO_PIO, PIN_DOUT);
    
    pio_sm_set_consecutive_pindirs(AUDIO_PIO, SM_TX, PIN_BCLK, 2, true);  // BCLK, LRCK outputs
    pio_sm_set_consecutive_pindirs(AUDIO_PIO, SM_TX, PIN_DOUT, 1, true);  // DOUT output

    // Clock divider: 2 cycles per bit in PIO
    // clk_sys / (Fs * 64 * 2) for 32-bit stereo
    float div = (float)clock_get_hz(clk_sys) / (AUDIO_SAMPLE_RATE * 64.0f * 2.0f);
    sm_config_set_clkdiv(&c_tx, div);
    sm_config_set_out_shift(&c_tx, false, true, 32);  // MSB first, autopull 32 bits
    
    pio_sm_init(AUDIO_PIO, SM_TX, offset_tx, &c_tx);

    // ============================================
    // 2. PIO RX Setup (Runs in lockstep with TX)
    // ============================================
    uint offset_rx = pio_add_program(AUDIO_PIO, &audio_i2s_rx_program);
    pio_sm_config c_rx_pio = audio_i2s_rx_program_get_default_config(offset_rx);
    
    // Input pin for data
    sm_config_set_in_pins(&c_rx_pio, PIN_DIN);
    
    // Configure RX GPIO with pull-down (silence when disconnected)
    pio_gpio_init(AUDIO_PIO, PIN_DIN);
    gpio_pull_down(PIN_DIN);
    pio_sm_set_consecutive_pindirs(AUDIO_PIO, SM_RX, PIN_DIN, 1, false);  // DIN input
    
    // SAME clock divider as TX - critical for lockstep operation
    sm_config_set_clkdiv(&c_rx_pio, div);
    sm_config_set_in_shift(&c_rx_pio, false, true, 32);  // MSB first, autopush 32 bits
    
    pio_sm_init(AUDIO_PIO, SM_RX, offset_rx, &c_rx_pio);

    // ============================================
    // 3. DMA Setup
    // ============================================
    dma_tx = dma_claim_unused_channel(true);
    dma_rx = dma_claim_unused_channel(true);

    // TX DMA: Memory -> PIO TX FIFO
    dma_channel_config cfg_tx = dma_channel_get_default_config(dma_tx);
    channel_config_set_transfer_data_size(&cfg_tx, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg_tx, true);
    channel_config_set_write_increment(&cfg_tx, false);
    channel_config_set_dreq(&cfg_tx, pio_get_dreq(AUDIO_PIO, SM_TX, true));
    dma_channel_configure(dma_tx, &cfg_tx, &AUDIO_PIO->txf[SM_TX], tx_buf[0], WORDS_PER_BLOCK, false);

    // RX DMA: PIO RX FIFO -> Memory
    dma_channel_config cfg_rx = dma_channel_get_default_config(dma_rx);
    channel_config_set_transfer_data_size(&cfg_rx, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg_rx, false);
    channel_config_set_write_increment(&cfg_rx, true);
    channel_config_set_dreq(&cfg_rx, pio_get_dreq(AUDIO_PIO, SM_RX, false));
    dma_channel_configure(dma_rx, &cfg_rx, rx_buf[0], &AUDIO_PIO->rxf[SM_RX], WORDS_PER_BLOCK, false);

    // IRQ on TX completion (triggers buffer swap)
    dma_channel_set_irq0_enabled(dma_tx, true);
    irq_set_exclusive_handler(DMA_IRQ_0, audio_dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // ============================================
    // 4. Start both state machines simultaneously
    // ============================================
    // Enable both SMs at the same time for synchronization
    pio_enable_sm_mask_in_sync(AUDIO_PIO, (1u << SM_TX) | (1u << SM_RX));
    
    // Start DMA transfers
    dma_channel_start(dma_rx);
    dma_channel_start(dma_tx);

    return true;
}

void audio_poll(void) {}
