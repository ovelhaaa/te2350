#include "../include/dsp_fdn.h"

static inline q31_t clamp_unit(q31_t v) {
  if (v < 0) return 0;
  if (v > Q31_MAX) return Q31_MAX;
  return v;
}

static inline q31_t hadamard_sum_sat(q31_t a, q31_t b, q31_t c, q31_t d) {
  int64_t sum = (int64_t)a + b + c + d;
  if (sum > Q31_MAX) return Q31_MAX;
  if (sum < Q31_MIN) return Q31_MIN;
  return (q31_t)sum;
}

static inline q31_t neg_sat(q31_t x) {
  return (x == Q31_MIN) ? Q31_MAX : -x;
}

static inline uint32_t scale_mod_inc(uint32_t inc, uint32_t scale_q16) {
  uint64_t scaled = (uint64_t)inc * scale_q16;
  scaled >>= 16;
  if (scaled == 0) return 1;
  if (scaled > UINT32_MAX) return UINT32_MAX;
  return (uint32_t)scaled;
}

static inline q31_t tri_q31(uint32_t phase) {
  uint32_t ramp = phase & 0x7FFFFFFFu;
  if (phase & 0x80000000u) {
    ramp = 0x7FFFFFFFu - ramp;
  }
  return (q31_t)ramp - FLOAT_TO_Q31(0.5f);
}

void dsp_fdn4_init(dsp_fdn4_t *fdn,
                   q31_t *buf0,
                   q31_t *buf1,
                   q31_t *buf2,
                   q31_t *buf3,
                   size_t line_size,
                   float sample_rate) {
  q31_t *bufs[DSP_FDN4_LINES] = {buf0, buf1, buf2, buf3};
  if (!fdn || line_size < 128u || (line_size & (line_size - 1u)) != 0u) {
    return;
  }
  for (int i = 0; i < DSP_FDN4_LINES; ++i) {
    if (!bufs[i]) {
      return;
    }
  }

  if (sample_rate <= 0.0f) sample_rate = 48000.0f;

  for (int i = 0; i < DSP_FDN4_LINES; ++i) {
    dsp_delay_init(&fdn->delay[i], bufs[i], line_size);
    dsp_onepole_init(&fdn->air_lp[i], FLOAT_TO_Q31(0.10f));
    dsp_onepole_init(&fdn->floor_lp[i], FLOAT_TO_Q31(0.012f));
    fdn->last_read[i] = 0;
  }

  fdn->line_size = line_size;
  fdn->feedback = FLOAT_TO_Q31(0.72f);
  fdn->input_gain = FLOAT_TO_Q31(0.30f);
  fdn->mod_depth_samples_q16 = 2 << 16;
  fdn->wet_gain = FLOAT_TO_Q31(0.45f);

  // Prime-spaced base taps at 48 kHz. They are intentionally incommensurate
  // and fit in an 8192-sample line while leaving guard samples for modulation.
  static const uint16_t primes_48k[DSP_FDN4_LINES] = {1499, 2111, 2887, 4051};
  const float sr_ratio = sample_rate / 48000.0f;
  float inv_sr_ratio = 48000.0f / sample_rate;
  if (inv_sr_ratio <= 0.0f) inv_sr_ratio = 1.0f;
  fdn->mod_rate_scale_q16 = (uint32_t)(inv_sr_ratio * 65536.0f + 0.5f);
  if (fdn->mod_rate_scale_q16 == 0) fdn->mod_rate_scale_q16 = 1;
  for (int i = 0; i < DSP_FDN4_LINES; ++i) {
    uint32_t d = (uint32_t)(primes_48k[i] * sr_ratio);
    if (d < 64u) d = 64u;
    if (d > line_size - 16u) d = (uint32_t)line_size - 16u;
    fdn->base_delay_q16[i] = (q16_16_t)(d << 16);
  }

  fdn->mod_phase[0] = 0x13579BDFu;
  fdn->mod_phase[1] = 0x5A82799Au;
  fdn->mod_phase[2] = 0x9E3779B9u;
  fdn->mod_phase[3] = 0xD1B54A32u;
  fdn->mod_inc[0] = scale_mod_inc(18000u, fdn->mod_rate_scale_q16);
  fdn->mod_inc[1] = scale_mod_inc(23600u, fdn->mod_rate_scale_q16);
  fdn->mod_inc[2] = scale_mod_inc(31700u, fdn->mod_rate_scale_q16);
  fdn->mod_inc[3] = scale_mod_inc(42100u, fdn->mod_rate_scale_q16);
}

void dsp_fdn4_set_params(dsp_fdn4_t *fdn,
                         q31_t feedback,
                         q31_t tone,
                         q31_t diffusion,
                         q31_t mod_rate,
                         q31_t mod_depth) {
  if (!fdn) return;

  feedback = clamp_unit(feedback);
  tone = clamp_unit(tone);
  diffusion = clamp_unit(diffusion);
  mod_rate = clamp_unit(mod_rate);
  mod_depth = clamp_unit(mod_depth);

  // Keep the loop below unity; this mapping intentionally supports a longer
  // ambience tail while preserving margin for modulation transients.
  q31_t fb = q31_add_sat(FLOAT_TO_Q31(0.58f), q31_mul(feedback, FLOAT_TO_Q31(0.40f)));
  if (fb > FLOAT_TO_Q31(0.985f)) fb = FLOAT_TO_Q31(0.985f);
  fdn->feedback = fb;

  fdn->input_gain = q31_add_sat(FLOAT_TO_Q31(0.16f), q31_mul(diffusion, FLOAT_TO_Q31(0.24f)));
  fdn->wet_gain = q31_add_sat(FLOAT_TO_Q31(0.24f), q31_mul(diffusion, FLOAT_TO_Q31(0.34f)));
  fdn->mod_depth_samples_q16 = q31_add_sat(1 << 16, q31_mul(mod_depth, 9 << 16));

  q31_t air_coeff = q31_add_sat(FLOAT_TO_Q31(0.035f), q31_mul(tone, FLOAT_TO_Q31(0.22f)));
  q31_t floor_coeff = q31_add_sat(FLOAT_TO_Q31(0.004f), q31_mul(tone, FLOAT_TO_Q31(0.018f)));
  for (int i = 0; i < DSP_FDN4_LINES; ++i) {
    fdn->air_lp[i].coeff = air_coeff;
    fdn->floor_lp[i].coeff = floor_coeff;
    uint32_t base = 9000u + (uint32_t)(((uint64_t)mod_rate * (9000u + (uint32_t)i * 3700u)) >> 31);
    fdn->mod_inc[i] = scale_mod_inc(base + (uint32_t)i * 1103u, fdn->mod_rate_scale_q16);
  }
}

void dsp_fdn4_process(dsp_fdn4_t *fdn, q31_t input, q31_t *out_l, q31_t *out_r) {
  if (!fdn || !out_l || !out_r) return;

  q31_t y[DSP_FDN4_LINES];

  for (int i = 0; i < DSP_FDN4_LINES; ++i) {
    fdn->mod_phase[i] += fdn->mod_inc[i];
    q31_t tri = tri_q31(fdn->mod_phase[i]);
    int32_t mod_q16 = (int32_t)q31_mul(tri, fdn->mod_depth_samples_q16);
    q16_16_t delay_q16 = (q16_16_t)((int32_t)fdn->base_delay_q16[i] + mod_q16);
    y[i] = dsp_delay_read_frac(&fdn->delay[i], delay_q16);
    fdn->last_read[i] = y[i];
  }

  // Normalized Hadamard: H4 / 2. The pre-halved inputs provide the unitary
  // scale; the sum saturates only as a final safety net against wrap-around.
  q31_t h0 = y[0] >> 1;
  q31_t h1 = y[1] >> 1;
  q31_t h2 = y[2] >> 1;
  q31_t h3 = y[3] >> 1;
  q31_t m0 = hadamard_sum_sat(h0, h1, h2, h3);
  q31_t m1 = hadamard_sum_sat(h0, neg_sat(h1), h2, neg_sat(h3));
  q31_t m2 = hadamard_sum_sat(h0, h1, neg_sat(h2), neg_sat(h3));
  q31_t m3 = hadamard_sum_sat(h0, neg_sat(h1), neg_sat(h2), h3);
  q31_t mixed[DSP_FDN4_LINES] = {m0, m1, m2, m3};
  static const q31_t inject[DSP_FDN4_LINES] = {
      FLOAT_TO_Q31(0.50f), FLOAT_TO_Q31(0.37f), -FLOAT_TO_Q31(0.42f), FLOAT_TO_Q31(0.29f)};

  q31_t input_drive = q31_mul(input, fdn->input_gain);

  for (int i = 0; i < DSP_FDN4_LINES; ++i) {
    q31_t low_ref = dsp_onepole_lp(&fdn->floor_lp[i], mixed[i]);
    q31_t hp = q31_sub_sat(mixed[i], low_ref);
    q31_t absorbed = dsp_onepole_lp(&fdn->air_lp[i], hp);
    q31_t warm = dsp_soft_saturate_gentle(absorbed);
    q31_t fb = q31_mul(warm, fdn->feedback);
    q31_t feed = q31_mul(input_drive, inject[i]);
    dsp_delay_write(&fdn->delay[i], q31_add_sat(feed, fb));
  }

  q31_t l = q31_add_sat(q31_mul(y[0], FLOAT_TO_Q31(0.55f)), q31_mul(y[2], FLOAT_TO_Q31(0.45f)));
  q31_t r = q31_add_sat(q31_mul(y[1], FLOAT_TO_Q31(0.55f)), q31_mul(y[3], FLOAT_TO_Q31(0.45f)));
  *out_l = q31_mul(l, fdn->wet_gain);
  *out_r = q31_mul(r, fdn->wet_gain);
}
