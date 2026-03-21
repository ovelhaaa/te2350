#include "../include/dsp_pitch.h"

void dsp_pitch_init(dsp_pitch_shifter_t *ps, q31_t *buffer, size_t size) {
  dsp_delay_init(&ps->delay, buffer, size);
  
  // Initialize dual read heads at different positions
  ps->read_pos_a = 0;
  ps->read_pos_b = (size / 2) << 16;  // Offset by half buffer
  
  ps->crossfade = 0;
  ps->pitch_inc = 1 << 16;  // 1.0 in Q16.16 (no pitch shift initially)
  ps->window_size = size / 4;  // Crossfade window is 1/4 of buffer
}

q31_t dsp_pitch_process(dsp_pitch_shifter_t *ps, q31_t in, q31_t pitch_ratio_q31) {
  // Write input to delay
  dsp_delay_write(&ps->delay, in);
  
  // Convert pitch ratio from Q31 to Q16.16
  // pitch_ratio_q31: 0 = 0.5x, Q31_MAX = 2.0x (range 0.5 to 2.0)
  // Map to Q16.16: 0x8000 (0.5) to 0x20000 (2.0)
  int64_t ratio_temp = ((int64_t)pitch_ratio_q31 * 98304) >> 31;  // Scale to 0.5..2.0 range
  ratio_temp += 32768;  // Add 0.5 offset
  ps->pitch_inc = (q16_16_t)ratio_temp;
  
  // Clamp to safe range
  if (ps->pitch_inc < 0x8000) ps->pitch_inc = 0x8000;    // Min 0.5x
  if (ps->pitch_inc > 0x20000) ps->pitch_inc = 0x20000;  // Max 2.0x
  
  // Advance read positions
  ps->read_pos_a += ps->pitch_inc;
  ps->read_pos_b += ps->pitch_inc;
  
  // Wrap positions
  size_t max_pos = (ps->delay.size - 1) << 16;
  if (ps->read_pos_a >= max_pos) ps->read_pos_a -= max_pos;
  if (ps->read_pos_b >= max_pos) ps->read_pos_b -= max_pos;
  
  // Read from both heads with HYBRID interpolation (reduces warbling)
  q31_t sample_a = dsp_delay_read_hybrid(&ps->delay, ps->read_pos_a);
  q31_t sample_b = dsp_delay_read_hybrid(&ps->delay, ps->read_pos_b);
  
  // Crossfade between heads
  // Simple linear crossfade based on position
  size_t pos_int_a = ps->read_pos_a >> 16;
  size_t window_pos = pos_int_a % ps->window_size;
  
  // Calculate crossfade coefficient (0..Q31_MAX over window)
  q31_t fade = (q31_t)(((int64_t)window_pos << 31) / ps->window_size);
  
  // Mix: out = a * (1-fade) + b * fade
  q31_t inv_fade = q31_sub_sat(Q31_MAX, fade);
  q31_t out = q31_add_sat(q31_mul(sample_a, inv_fade), q31_mul(sample_b, fade));
  
  return out;
}
