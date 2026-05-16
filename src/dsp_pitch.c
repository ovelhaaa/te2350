#include "../include/dsp_pitch.h"

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
  ps->window_size = dsp_pitch_clamp_window_size(ps, window_size);
  ps->window_phase_inc = dsp_pitch_phase_inc_for_window(ps->window_size);
  ps->read_pos_a = 0;
  ps->read_pos_b = (q16_16_t)((uint32_t)(ps->window_size / 2u) << 16);
  ps->crossfade = 0;
}

void dsp_pitch_init(dsp_pitch_shifter_t *ps, q31_t *buffer, size_t size) {
  dsp_delay_init(&ps->delay, buffer, size);

  ps->pitch_inc = 1 << 16;  // 1.0 in Q16.16 (no pitch shift initially)
  dsp_pitch_set_window_size(ps, size);
}

q31_t dsp_pitch_ratio_from_octave_amount(q31_t octave_amount) {
  if (octave_amount < 0) {
    octave_amount = 0;
  }
  if (octave_amount > Q31_MAX) {
    octave_amount = Q31_MAX;
  }

  // dsp_pitch_process uses 0..Q31_MAX to represent 0.5x..2.0x. A musical
  // octave amount uses 0..Q31_MAX to represent 1.0x..2.0x, so:
  // internal = (ratio - 0.5) / 1.5 = 1/3 + (2/3 * octave_amount).
  return q31_add_sat(FLOAT_TO_Q31(0.33333334f),
                     q31_mul(octave_amount, FLOAT_TO_Q31(0.6666667f)));
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

  // Advance and wrap read heads inside the configured crossfade window. Keeping
  // this window independent from the underlying buffer allows long, smooth
  // shimmer windows and short, transient-friendly feedback-octave windows.
  uint32_t window_q16 = (uint32_t)window_size << 16;
  uint64_t read_pos_a = (uint64_t)ps->read_pos_a + ps->pitch_inc;
  uint64_t read_pos_b = (uint64_t)ps->read_pos_b + ps->pitch_inc;
  while (read_pos_a >= window_q16) read_pos_a -= window_q16;
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

  q31_t tri_a;
  if (phase_a < (Q31_MAX >> 1)) {
    tri_a = phase_a << 1;
  } else {
    tri_a = q31_sub_sat(Q31_MAX, phase_a) << 1;
  }

  // B is offset by half the window, so its triangle is inverted.
  q31_t tri_b = q31_sub_sat(Q31_MAX, tri_a);

  // Mix: out = a * tri_a + b * tri_b. Complementary weights keep nominal gain
  // steady while avoiding hard read-head switches.
  return q31_add_sat(q31_mul(sample_a, tri_a), q31_mul(sample_b, tri_b));
}
