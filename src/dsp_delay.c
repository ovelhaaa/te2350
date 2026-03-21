#include "../include/dsp_delay.h"
#include "../include/dsp_math.h"

// --- Implementation ---

void dsp_delay_init(dsp_delay_t *delay, q31_t *buffer, size_t size) {
  // Validate that size is power of 2
  if (size == 0 || (size & (size - 1)) != 0) {
    // Error: size must be power of 2
    // For safety, we'll still initialize but this is invalid
    delay->buffer = buffer;
    delay->size = size;
    delay->mask = 0;  // Will cause issues, but prevents crash
    delay->write_idx = 0;
    return;
  }
  
  delay->buffer = buffer;
  delay->size = size;
  delay->mask = size - 1;
  delay->write_idx = 0;

  // Clear buffer with loop unrolling (4x) for performance
  size_t i = 0;
  size_t unroll_limit = size & ~3;  // Round down to multiple of 4
  
  for (; i < unroll_limit; i += 4) {
    delay->buffer[i] = 0;
    delay->buffer[i+1] = 0;
    delay->buffer[i+2] = 0;
    delay->buffer[i+3] = 0;
  }
  
  // Handle remaining samples
  for (; i < size; i++) {
    delay->buffer[i] = 0;
  }
}

void dsp_delay_write(dsp_delay_t *delay, q31_t sample) {
  // Basic write, no processing
  delay->buffer[delay->write_idx] = sample;

  // Increment and wrap
  delay->write_idx = (delay->write_idx + 1) & delay->mask;
}

q31_t dsp_delay_read(const dsp_delay_t *delay, size_t delay_samples) {
  // Calculate read index: (write_idx - delay_samples) & mask
  // We add size to ensure positivity before masking if delay > write_idx
  // (though masking handles it mostly) Because size is power of 2, standard
  // unsigned math & mask works correctly for "negative" results in 2s
  // complement wrapping context provided delay_samples isn't HUGE.

  size_t read_idx = (delay->write_idx - delay_samples) & delay->mask;
  return delay->buffer[read_idx];
}

q31_t dsp_delay_read_frac(const dsp_delay_t *delay, q16_16_t delay_q16) {
  // Split into integer and fractional parts
  // delay_q16 represents the delay time in samples.
  // Integer part
  size_t d_int = delay_q16 >> 16;
  // Fractional part (Q16), range [0, 65535] mapped to [0, 0.999...]
  // We need to convert this to Q31 for multiplication if we use q31_mul,
  // or just use it as is if we do custom interpolation math.
  int32_t frac = delay_q16 & 0xFFFF; // Q16

  // Read index corresponding to the integer delay
  // Current time is write_idx.
  // We want sample at t - d_int.
  // Let's say d_int = 1. We want t-1.
  // Linear interp usually looks at indices: i and i+1.
  // But since this is a delay *line*, "i" is "t - d_int" and "i+1" is "t -
  // (d_int + 1)"? Wait. If delay is 1.5 samples. We should read values at 1
  // sample back and 2 samples back. And interpolate. Interp = val(1.0) *
  // (1-frac) + val(2.0) * frac
  //   OR   val(1.0) + frac * (val(2.0) - val(1.0))
  // Let's check the direction.
  // If frac is 0.1, we are close to 1.0.
  // If frac is 0.9, we are close to 2.0.

  // Index A = (write_idx - d_int) & mask     => sample at d_int
  // Index B = (write_idx - d_int - 1) & mask => sample at d_int + 1

  size_t idx_a = (delay->write_idx - d_int) & delay->mask;
  size_t idx_b = (delay->write_idx - d_int - 1) & delay->mask;

  q31_t sample_a = delay->buffer[idx_a];
  q31_t sample_b = delay->buffer[idx_b];

  // Need careful math here.
  // b - a can range nearly full int32 range.
  // frac is Q16 (0..65535).
  // Result of frac * (b-a) needs to be shifted down by 16.

  // Interpolation: y = a + frac * (b - a)
  // Use 64-bit difference to avoid saturation (e.g. 0.9 - (-0.9) = 1.8 which
  // overflows Q31)
  int64_t diff = (int64_t)sample_b - (int64_t)sample_a;

  // 64-bit multiply: Q16 * Q31 -> Q47 (conceptually, though diff is effectively
  // Q32 range)
  int64_t delta = (diff * frac) >> 16;

  // Result is sample_a + delta.
  // Delta fits in Q31 range roughly, but adding to sample_a might saturate if
  // we are super unlucky (though logically it stays between a and b). We cast
  // delta to q31_t. Since we shifted by 16, it should fit unless the slope was
  // massive (which it is). Actually, delta is the portion to add.

  return q31_add_sat(sample_a, (q31_t)delta);
}

q31_t dsp_delay_read_hermite(const dsp_delay_t *delay, q16_16_t delay_q16) {
  // Hermite interpolation (4-point, 3rd order) - Tera Echo style
  // Band-limited with implicit saturation for organic sound
  
  size_t d_int = delay_q16 >> 16;
  int32_t frac = delay_q16 & 0xFFFF; // Q16 fractional part
  
  // AJUSTE 1: Corrected center point (removes pitch bias)
  // Centro correto: x0 = exatamente t - d_int - frac
  size_t idx_0  = (delay->write_idx - d_int - 1) & delay->mask;  // x[0] - center
  size_t idx_m1 = (idx_0 + 1) & delay->mask;                     // x[-1]
  size_t idx_1  = (idx_0 - 1) & delay->mask;                     // x[1]
  size_t idx_2  = (idx_0 - 2) & delay->mask;                     // x[2]
  
  // Fetch samples
  int64_t xm1 = delay->buffer[idx_m1];
  int64_t x0  = delay->buffer[idx_0];
  int64_t x1  = delay->buffer[idx_1];
  int64_t x2  = delay->buffer[idx_2];
  
  // Convert frac from Q16 to normalized [0, 1] for calculation
  int64_t t = frac;  // 0..65535
  
  // AJUSTE 2: Hermite "Tera-style" (band-limited)
  // Suaviza o slope, não o valor - menos brilho, mais "neblina"
  // c0 = x0
  // c1 = (x1 - xm1) / 4  <- AJUSTADO: era /2, agora /4 (suaviza HF)
  // c2 = xm1 - 2.5*x0 + 2*x1 - 0.5*x2
  // c3 = (x2 - xm1)/4 + (x0 - x1)  <- AJUSTADO: compensação
  
  int64_t c0 = x0;
  int64_t c1 = (x1 - xm1) >> 2;  // AJUSTE 2: era >>1, agora >>2
  int64_t c2 = xm1 - ((5 * x0) >> 1) + (x1 << 1) - (x2 >> 1);
  int64_t c3 = ((x2 - xm1) >> 2) + (x0 - x1);  // AJUSTE 2: compensação em c3
  
  // Evaluate polynomial: y = c0 + c1*t + c2*t^2 + c3*t^3
  // t is in range [0, 65535], normalize to [0, 1] by dividing by 65536
  
  // t^2 in Q16: (t * t) >> 16
  int64_t t2 = (t * t) >> 16;
  // t^3 in Q16: (t2 * t) >> 16
  int64_t t3 = (t2 * t) >> 16;
  
  // Calculate result: c0 + c1*(t/65536) + c2*(t2/65536) + c3*(t3/65536)
  int64_t result = c0 + ((c1 * t) >> 16) + ((c2 * t2) >> 16) + ((c3 * t3) >> 16);
  
  // AJUSTE 3: Saturação implícita no interpolador
  // Boss satura dentro do delay, não só fora
  // Soft HF damping + saturation feel (praticamente grátis em CPU)
  result = result - (result >> 4);
  
  // Clamp to Q31 range
  if (result > Q31_MAX) result = Q31_MAX;
  if (result < Q31_MIN) result = Q31_MIN;
  
  return (q31_t)result;
}

// AJUSTE 4: Linear + Hermite híbrido (barato e lindo)
// Para pitch shifter: 70% Hermite + 30% Linear
// Reduz warbling e "digital shimmer"
q31_t dsp_delay_read_hybrid(const dsp_delay_t *delay, q16_16_t delay_q16) {
  // Get both interpolations
  q31_t hermite_result = dsp_delay_read_hermite(delay, delay_q16);
  q31_t linear_result = dsp_delay_read_frac(delay, delay_q16);
  
  // Blend: 70% Hermite + 30% Linear
  // 0.7 in Q31 = 0x59999999, 0.3 in Q31 = 0x26666666
  q31_t hermite_scaled = q31_mul(hermite_result, (q31_t)0x59999999);
  q31_t linear_scaled = q31_mul(linear_result, (q31_t)0x26666666);
  
  return q31_add_sat(hermite_scaled, linear_scaled);
}

