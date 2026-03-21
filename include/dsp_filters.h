#ifndef DSP_FILTERS_H
#define DSP_FILTERS_H

#include "dsp_delay.h" // For q31_t

// --- 1. DC Blocker ---
typedef struct {
  q31_t x1;
  q31_t y1;
} dsp_dc_blocker_t;

void dsp_dc_blocker_init(dsp_dc_blocker_t *bp);
q31_t dsp_dc_blockProcess(dsp_dc_blocker_t *bp, q31_t x);

// --- 2. Soft Saturation ---
// Simple polynomial or clamped shaper
q31_t dsp_soft_saturate(q31_t x);

// Boss-style dual saturation (progressive compression)
q31_t dsp_soft_saturate_aggressive(q31_t x);  // Pre-delay (mais agressiva)
q31_t dsp_soft_saturate_gentle(q31_t x);      // Post-delay (quase nada)

// --- 3. State Variable Filter (SVF) ---
typedef enum {
  SVF_LOWPASS,
  SVF_BANDPASS,
  SVF_HIGHPASS,
  SVF_NOTCH,
  SVF_PEAK
} svf_mode_t;

typedef struct {
  q31_t low;
  q31_t band;
  q31_t f; // Frequency coeff (2*sin(pi*freq/sr))
  q31_t q; // Damping coeff (1/Q)
} dsp_svf_t;

void dsp_svf_init(dsp_svf_t *svf, q31_t f_coeff, q31_t q_coeff);
void dsp_svf_update_coeffs(dsp_svf_t *svf, q31_t f_coeff, q31_t q_coeff);
// Returns Lowpass, Bandpass, Highpass via pointers (optional) and returns
// Lowpass by default
q31_t dsp_svf_process(dsp_svf_t *svf, q31_t x, q31_t *out_bp, q31_t *out_hp);
// Process with mode selection
q31_t dsp_svf_process_mode(dsp_svf_t *svf, q31_t x, svf_mode_t mode);

// --- 4. Modulated Allpass Filter ---
// Used for diffusion. Needs a delay line.
typedef struct {
  dsp_delay_t delay;
  q31_t gain;  // Feedback gain
  q31_t old_y; // Not strictly needed if loop is inside, but for canonical
               // structure
} dsp_allpass_t;

void dsp_allpass_init(dsp_allpass_t *ap, q31_t *buffer, size_t size,
                      q31_t gain);
// Process with modulation (using fractional read from delay)
// delay_samples is the CURRENT delay length (modulated)
q31_t dsp_allpass_process(dsp_allpass_t *ap, q31_t x, q16_16_t delay_samples);

// --- 5. One-Pole Filters (Efficient Tone Controls) ---
typedef struct {
  q31_t z1;     // State variable
  q31_t coeff;  // Filter coefficient
} dsp_onepole_t;

void dsp_onepole_init(dsp_onepole_t *f, q31_t coeff);
q31_t dsp_onepole_lp(dsp_onepole_t *f, q31_t x);  // Lowpass
q31_t dsp_onepole_hp(dsp_onepole_t *f, q31_t x);  // Highpass

#endif // DSP_FILTERS_H
