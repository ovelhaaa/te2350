#include <cmath>
#include <cstdio>
#include <vector>

extern "C"
{
#include "../../include/te2350.h"
#include "../../include/dsp_math.h"
}

namespace
{
constexpr size_t memoryPoolBytes = TE2350_REQUIRED_MEMORY_BYTES;

bool expect(bool condition, const char* message)
{
    if (! condition)
        std::fprintf(stderr, "%s\n", message);

    return condition;
}
}

int main()
{
    std::vector<q31_t> memory(memoryPoolBytes / sizeof(q31_t), 0);
    te2350_t core {};

    if (! te2350_init(&core, memory.data(), memoryPoolBytes, 48000.0f))
    {
        std::fprintf(stderr, "te2350_init failed\n");
        return 1;
    }

    te2350_set_time_samples(&core, 24000);
    if (! expect(core.p_time_samples_target == 24000
                 && core.p_time_samples_smoothed == 24000,
                 "exact 500 ms target was not preserved"))
    {
        return 1;
    }

    te2350_set_time_samples(&core, 96000);
    if (! expect(core.p_time_samples_target == 96000,
                 "desktop delay target above the Q16 integer range was truncated"))
    {
        return 1;
    }

    if (! te2350_init(&core, memory.data(), memoryPoolBytes, 192000.0f))
    {
        std::fprintf(stderr, "te2350_init failed at 192 kHz\n");
        return 1;
    }

    te2350_set_time_samples(&core, 384000);
    if (! expect(core.p_time_samples_target == 384000,
                 "2 second delay target was not preserved at 192 kHz"))
    {
        return 1;
    }

    te2350_set_time_samples(&core, TE_MAIN_DELAY_SIZE * 2);
    if (! expect(core.p_time_samples_target == core.max_delay_samples,
                 "exact delay target was not clamped to the allocated line"))
    {
        return 1;
    }

    te2350_set_shimmer_interval(&core, 0);
    if (! expect(core.p_shimmer_pitch == 0, "sub-octave ratio was not selected"))
        return 1;

    te2350_set_shimmer_interval(&core, 1);
    if (! expect(std::abs(Q31_TO_FLOAT(core.p_shimmer_pitch) - 0.6666667f) < 0.0001f,
                 "perfect-fifth ratio was not selected"))
    {
        return 1;
    }

    te2350_set_shimmer_interval(&core, 2);
    if (! expect(core.p_shimmer_pitch == Q31_MAX, "octave-up ratio was not selected"))
        return 1;

    te2350_set_low_cut_coeff(&core, FLOAT_TO_Q31(0.1f));
    te2350_set_duck_threshold(&core, FLOAT_TO_Q31(0.25f));
    te2350_set_tail_feedback(&core, FLOAT_TO_Q31(0.91f));
    te2350_set_mod_shape(&core, 2);
    te2350_set_mod_rate_hz(&core, 1.0f);
    if (! expect(std::abs(Q31_TO_FLOAT(core.p_low_cut_coeff) - 0.1f) < 0.0001f,
                 "low-cut coefficient did not reach the core")
        || ! expect(std::abs(Q31_TO_FLOAT(core.p_duck_threshold) - 0.25f) < 0.0001f,
                    "duck threshold did not reach the core")
        || ! expect(std::abs(Q31_TO_FLOAT(core.p_tail_feedback) - 0.91f) < 0.0001f,
                    "RT60 feedback floor did not reach the core")
        || ! expect(core.p_mod_shape == 2, "modulation shape did not reach the core")
        || ! expect(core.mod_shape_phase_inc > 0, "modulation rate did not produce a phase increment"))
    {
        return 1;
    }

    std::vector<q31_t> wideDelayMemory(131072, 0);
    dsp_delay_t wideDelay {};
    dsp_delay_init(&wideDelay, wideDelayMemory.data(), wideDelayMemory.size());
    dsp_delay_write(&wideDelay, FLOAT_TO_Q31(0.5f));
    for (int sample = 0; sample < 70000; ++sample)
        dsp_delay_write(&wideDelay, 0);

    const auto wideRead = dsp_delay_read_hermite_wide(&wideDelay, 70000ull << 16);
    if (! expect(std::abs(Q31_TO_FLOAT(wideRead) - 0.46875f) < 0.0001f,
                 "wide Hermite read did not address a delay above 65535 samples"))
    {
        return 1;
    }

    std::printf("Core control mapping test passed\n");
    return 0;
}
