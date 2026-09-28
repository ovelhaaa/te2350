#include "../include/dsp_pitch.h"
#include "dsp_pitch_lut.h"

#define DSP_PITCH_MAX_WINDOW_SIZE 65535u

static size_t dsp_pitch_clamp_window_size(const dsp_pitch_shifter_t *ps, size_t window_size) {
  size_t max_window = ps->delay.size;
  if (max_window > DSP_PITCH_MAX_WINDOW_SIZE) {
    max_window = DSP_PITCH_MAX_WINDOW_SIZE;
  }
  if (max_window < 2u) {
    max_window = 2u;
  }

  if (window_size < 2u) {
    window_size = 2u;
  }
  if (window_size > max_window) {
    window_size = max_window;
  }
  return window_size;
}

static uint32_t dsp_pitch_phase_inc_for_window(size_t window_size) {
  if (window_size < 2u) {
    window_size = 2u;
  }
  if (window_size > DSP_PITCH_MAX_WINDOW_SIZE) {
    window_size = DSP_PITCH_MAX_WINDOW_SIZE;
  }

  return (uint32_t)((1ULL << 31) / window_size);
}

void dsp_pitch_set_window_size(dsp_pitch_shifter_t *ps, size_t window_size) {
  size_t clamped = dsp_pitch_clamp_window_size(ps, window_size);
  if (clamped == ps->window_size) return;
  ps->window_size = clamped;
  ps->window_phase_inc = dsp_pitch_phase_inc_for_window(ps->window_size);
}

void dsp_pitch_init(dsp_pitch_shifter_t *ps, q31_t *buffer, size_t size) {
  dsp_delay_init(&ps->delay, buffer, size);

  ps->pitch_inc = 1 << 16;  // 1.0 in Q16.16 (no pitch shift initially)
  ps->window_size = 0;
  dsp_pitch_set_window_size(ps, size);
  ps->read_pos_a = 0;
  ps->read_pos_b = (q16_16_t)((uint32_t)(ps->window_size / 2u) << 16);
  ps->crossfade = 0;
}

q31_t dsp_pitch_ratio_from_octave_amount(q31_t octave_amount) {
  if (octave_amount <= 0) {
    return (q31_t)(Q31_MAX / 3);
  }
  if (octave_amount >= Q31_MAX) {
    return Q31_MAX;
  }

  // dsp_pitch_process uses 0..Q31_MAX to represent 0.5x..2.0x. A musical
  // octave amount uses 0..Q31_MAX to represent 1.0x..2.0x, so:
  // internal = (ratio - 0.5) / 1.5 = 1/3 + (2/3 * octave_amount).
  const int64_t numerator = (int64_t)Q31_MAX + 2LL * (int64_t)octave_amount;
  return (q31_t)(numerator / 3LL);
}

q31_t dsp_pitch_process(dsp_pitch_shifter_t *ps, q31_t in, q31_t pitch_ratio_q31) {
  // Write input to delay.
  dsp_delay_write(&ps->delay, in);

  // Convert pitch control from Q31 to Q16.16.
  // pitch_ratio_q31: 0 = 0.5x, Q31_MAX = 2.0x (range 0.5 to 2.0)
  // Map to Q16.16: 0x8000 (0.5) to 0x20000 (2.0)
  int64_t ratio_temp = ((int64_t)pitch_ratio_q31 * 98304) >> 31;  // Scale to 0.5..2.0 range
  ratio_temp += 32768;  // Add 0.5 offset
  ps->pitch_inc = (q16_16_t)ratio_temp;

  // Clamp to safe range.
  if (ps->pitch_inc < 0x8000) ps->pitch_inc = 0x8000;    // Min 0.5x
  if (ps->pitch_inc > 0x20000) ps->pitch_inc = 0x20000;  // Max 2.0x

  // Use a local clamp only. Do not call dsp_pitch_set_window_size() here: that
  // would reset read heads in the audio path and create a click if state was
  // ever out of range.
  size_t window_size = dsp_pitch_clamp_window_size(ps, ps->window_size);
  uint32_t window_phase_inc = ps->window_phase_inc;
  if (window_size != ps->window_size || window_phase_inc == 0u) {
    window_phase_inc = dsp_pitch_phase_inc_for_window(window_size);
  }

  // Delay increment is 1.0 - pitch_ratio.
  // When pitch_ratio > 1 (pitch up), delay decreases so we read faster.
  // When pitch_ratio < 1 (pitch down), delay increases so we read slower.
  int64_t delay_inc = (1LL << 16) - (int64_t)ps->pitch_inc;

  // Advance and wrap read heads inside the configured crossfade window. Keeping
  // this window independent from the underlying buffer allows long, smooth
  // shimmer windows and short, transient-friendly feedback-octave windows.
  int64_t window_q16 = (int64_t)window_size << 16;
  int64_t read_pos_a = (int64_t)ps->read_pos_a + delay_inc;
  int64_t read_pos_b = (int64_t)ps->read_pos_b + delay_inc;

  while (read_pos_a < 0) read_pos_a += window_q16;
  while (read_pos_a >= window_q16) read_pos_a -= window_q16;

  while (read_pos_b < 0) read_pos_b += window_q16;
  while (read_pos_b >= window_q16) read_pos_b -= window_q16;

  ps->read_pos_a = (q16_16_t)read_pos_a;
  ps->read_pos_b = (q16_16_t)read_pos_b;

  // Read from both heads with HYBRID interpolation (reduces warbling).
  q31_t sample_a = dsp_delay_read_hybrid(&ps->delay, ps->read_pos_a);
  q31_t sample_b = dsp_delay_read_hybrid(&ps->delay, ps->read_pos_b);

  // Calculate a phase (0..Q31_MAX) representing how far head A is along the
  // configured window. The setter precalculates the per-sample phase increment
  // so the audio loop can use a multiply instead of a division.
  size_t pos_int_a = ps->read_pos_a >> 16;
  uint64_t phase_temp = (uint64_t)pos_int_a * window_phase_inc;
  q31_t phase_a = (phase_temp > (uint64_t)Q31_MAX) ? Q31_MAX : (q31_t)phase_temp;
  ps->crossfade = phase_a;

  // Use equal-power crossfade instead of linear/triangular.
  // We can treat phase_a as a triangle to index a quarter-sine LUT.
  q31_t tri_a;
  if (phase_a < (Q31_MAX >> 1)) {
    tri_a = phase_a << 1;
  } else {
    tri_a = q31_sub_sat(Q31_MAX, phase_a) << 1;
  }

  q31_t fade_a = lookup_equal_power(tri_a);
  q31_t fade_b = lookup_equal_power(q31_sub_sat(Q31_MAX, tri_a));

  // A small calibration trim to prevent correlated signals from gaining +3dB at the center.
  // 0.94 is a good compromise for mixed correlation.
  fade_a = q31_mul(fade_a, FLOAT_TO_Q31(0.94f));
  fade_b = q31_mul(fade_b, FLOAT_TO_Q31(0.94f));

  // Mix: out = a * fade_a + b * fade_b. Equal power weights keep energy steady.
  return q31_add_sat(q31_mul(sample_a, fade_a), q31_mul(sample_b, fade_b));
}
