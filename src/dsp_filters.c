#include "../include/dsp_filters.h"
#include "../include/dsp_math.h"

// --- DC Blocker ---
// y[n] = x[n] - x[n-1] + R * y[n-1]
// R = 0.9995 -> ~ 2146400000
#define DC_R ((q31_t)2146435072) // ~0.9995 in Q31

void dsp_dc_blocker_init(dsp_dc_blocker_t *bp) {
  bp->x1 = 0;
  bp->y1 = 0;
}

q31_t dsp_dc_blockProcess(dsp_dc_blocker_t *bp, q31_t x) {
  // y = x - x1 + R*y1
  q31_t part1 = q31_sub_sat(x, bp->x1);
  q31_t part2 = q31_mul(DC_R, bp->y1);
  q31_t y = q31_add_sat(part1, part2);

  bp->x1 = x;
  bp->y1 = y;
  return y;
}

// --- Soft Saturation ---
// Cubic saturation: y = x - (x³/3)
// More musical than linear reduction, adds pleasant harmonic distortion
// At x=0: y = 0 (correct)
// At x=0.5: y ≈ 0.458
// At x=1.0: y ≈ 0.667 (soft limiting)

q31_t dsp_soft_saturate(q31_t x) {
  // Calculate x² in Q31
  int64_t x2 = ((int64_t)x * x) >> 31;
  
  // Calculate x³ in Q31
  int64_t x3 = (x2 * x) >> 31;
  
  // x³/3
  int64_t x3_div3 = x3 / 3;
  
  // y = x - x³/3
  int64_t result = x - x3_div3;
  
  // Clamp to Q31 range
  if (result > Q31_MAX) result = Q31_MAX;
  if (result < Q31_MIN) result = Q31_MIN;
  
  return (q31_t)result;
}

// Boss-style Dual Saturation (Progressive Compression)

// Aggressive saturation (PRE-delay) - mais compressão
// y = x - x³/4 (≈ x - 0.25*x³)
q31_t dsp_soft_saturate_aggressive(q31_t x) {
  int64_t x2 = ((int64_t)x * x) >> 31;
  int64_t x3 = (x2 * x) >> 31;
  int64_t x3_div4 = x3 >> 2;  // x³/4
  
  int64_t result = x - x3_div4;
  
  if (result > Q31_MAX) result = Q31_MAX;
  if (result < Q31_MIN) result = Q31_MIN;
  
  return (q31_t)result;
}

// Gentle saturation (POST-delay) - quase nada
// y = x - x³/16 (≈ x - 0.0625*x³)
q31_t dsp_soft_saturate_gentle(q31_t x) {
  int64_t x2 = ((int64_t)x * x) >> 31;
  int64_t x3 = (x2 * x) >> 31;
  int64_t x3_div16 = x3 >> 4;  // x³/16
  
  int64_t result = x - x3_div16;
  
  if (result > Q31_MAX) result = Q31_MAX;
  if (result < Q31_MIN) result = Q31_MIN;
  
  return (q31_t)result;
}

// --- SVF ---
// Chamberlin State Variable Filter
// low = low + f * band
// high = input - low - q*band
// band = band + f * high

void dsp_svf_init(dsp_svf_t *svf, q31_t f_coeff, q31_t q_coeff) {
  svf->low = 0;
  svf->band = 0;
  svf->f = f_coeff;
  svf->q = q_coeff;
}

void dsp_svf_update_coeffs(dsp_svf_t *svf, q31_t f_coeff, q31_t q_coeff) {
  // Clamp f for stability (maximum ~0.25 to avoid instability near Nyquist)
  q31_t f_max = FLOAT_TO_Q31(0.25f);
  if (f_coeff > f_max) f_coeff = f_max;
  
  svf->f = f_coeff;
  svf->q = q_coeff;
}

q31_t dsp_svf_process(dsp_svf_t *svf, q31_t x, q31_t *out_bp, q31_t *out_hp) {
  // Chamberlin SVF with saturation protection
  
  // low += f * band
  q31_t f_band = q31_mul(svf->f, svf->band);
  svf->low = q31_add_sat(svf->low, f_band);

  // high = x - low - q * band
  q31_t q_band = q31_mul(svf->q, svf->band);
  q31_t high = q31_sub_sat(x, svf->low);
  high = q31_sub_sat(high, q_band);

  // band += f * high
  q31_t f_high = q31_mul(svf->f, high);
  svf->band = q31_add_sat(svf->band, f_high);

  if (out_bp)
    *out_bp = svf->band;
  if (out_hp)
    *out_hp = high;

  return svf->low;
}

// --- Allpass ---
// Schroeder-style allpass for diffusion
// Uses fractional delay read for smooth modulation
//
// Structure:
// val_from_delay = Read(Delay, d)
// feed_val = x_in + g * val_from_delay
// Write(feed_val)
// out = val_from_delay - g * feed_val

void dsp_allpass_init(dsp_allpass_t *ap, q31_t *buffer, size_t size,
                      q31_t gain) {
  dsp_delay_init(&ap->delay, buffer, size);
  ap->gain = gain;
}

q31_t dsp_allpass_process(dsp_allpass_t *ap, q31_t x, q16_16_t delay_samples) {
  // 1. Read from delay at modulated position (fractional for smooth mod)
  q31_t delayed = dsp_delay_read_frac(&ap->delay, delay_samples);
  
  // BOSS-STYLE: HF loss dentro do allpass (~3% loss)
  // Isso cria eco mais longe, menos brilho, mais profundidade
  delayed -= (delayed >> 5);  // ~3.125% HF damping

  // 2. Calculate feedback
  q31_t g_delayed = q31_mul(ap->gain, delayed);
  q31_t feed = q31_add_sat(x, g_delayed);

  // 3. Write to delay
  dsp_delay_write(&ap->delay, feed);

  // 4. Calculate output
  // out = delayed - g * feed
  q31_t g_feed = q31_mul(ap->gain, feed);
  q31_t out = q31_sub_sat(delayed, g_feed);

  return out;
}

// --- SVF Mode Selection ---
q31_t dsp_svf_process_mode(dsp_svf_t *svf, q31_t x, svf_mode_t mode) {
  // Process SVF (updates internal state)
  q31_t f_band = q31_mul(svf->f, svf->band);
  svf->low = q31_add_sat(svf->low, f_band);

  q31_t q_band = q31_mul(svf->q, svf->band);
  q31_t high = q31_sub_sat(x, svf->low);
  high = q31_sub_sat(high, q_band);

  q31_t f_high = q31_mul(svf->f, high);
  svf->band = q31_add_sat(svf->band, f_high);

  q31_t lp = svf->low;
  q31_t bp = svf->band;
  q31_t hp = high;

  // Select output based on mode
  switch(mode) {
    case SVF_LOWPASS:  return lp;
    case SVF_BANDPASS: return bp;
    case SVF_HIGHPASS: return hp;
    case SVF_NOTCH:    return q31_add_sat(lp, hp);  // LP + HP
    case SVF_PEAK:     return q31_sub_sat(q31_add_sat(lp, hp), bp);  // LP + HP - BP
    default:           return lp;
  }
}

// --- One-Pole Filters ---
void dsp_onepole_init(dsp_onepole_t *f, q31_t coeff) {
  f->z1 = 0;
  f->coeff = coeff;
}

q31_t dsp_onepole_lp(dsp_onepole_t *f, q31_t x) {
  // y[n] = y[n-1] + coeff * (x[n] - y[n-1])
  q31_t diff = q31_sub_sat(x, f->z1);
  q31_t delta = q31_mul(f->coeff, diff);
  f->z1 = q31_add_sat(f->z1, delta);
  return f->z1;
}

q31_t dsp_onepole_hp(dsp_onepole_t *f, q31_t x) {
  // HP = input - LP
  q31_t lp = dsp_onepole_lp(f, x);
  return q31_sub_sat(x, lp);
}

