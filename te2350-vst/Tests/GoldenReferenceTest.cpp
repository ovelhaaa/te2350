#include <cstdio>
#include <vector>

extern "C"
{
#include "../../include/te2350.h"
#include "../../include/dsp_math.h"
}

namespace
{
constexpr int sampleRate = 48000;
constexpr int seconds = 2;
constexpr int samples = sampleRate * seconds;
constexpr size_t memoryPoolBytes = 320 * 1024;
}

int main()
{
    std::vector<q31_t> memory(memoryPoolBytes / sizeof(q31_t), 0);
    te2350_t core {};

    if (!te2350_init(&core, memory.data(), memoryPoolBytes, static_cast<float>(sampleRate)))
    {
        std::fprintf(stderr, "te2350_init failed\n");
        return 1;
    }

    te2350_set_time(&core, FLOAT_TO_Q31(0.8f));
    te2350_set_feedback(&core, FLOAT_TO_Q31(0.5f));
    te2350_set_mix(&core, FLOAT_TO_Q31(0.5f));

    std::FILE* file = std::fopen("plugin_reference.raw", "wb");
    if (file == nullptr)
    {
        std::fprintf(stderr, "failed to open plugin_reference.raw\n");
        return 1;
    }

    for (int index = 0; index < samples; ++index)
    {
        const q31_t input = index == 0 ? FLOAT_TO_Q31(1.0f) : 0;
        q31_t left = 0;
        q31_t right = 0;
        te2350_process(&core, input, &left, &right);

        const int16_t pcmLeft = static_cast<int16_t>(left >> 16);
        const int16_t pcmRight = static_cast<int16_t>(right >> 16);
        std::fwrite(&pcmLeft, sizeof(int16_t), 1, file);
        std::fwrite(&pcmRight, sizeof(int16_t), 1, file);
    }

    std::fclose(file);
    std::puts("Wrote plugin_reference.raw. Compare against src/web/offline_host.c output.raw with the same core parameters.");
    return 0;
}
