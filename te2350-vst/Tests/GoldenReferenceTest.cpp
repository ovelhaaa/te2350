#include <cstdio>
#include <cmath>
#include <cstdint>
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

void writeU16(std::FILE* file, uint16_t value)
{
    std::fwrite(&value, sizeof(value), 1, file);
}

void writeU32(std::FILE* file, uint32_t value)
{
    std::fwrite(&value, sizeof(value), 1, file);
}

bool writeWav(const char* filename, const std::vector<int16_t>& pcm)
{
    std::FILE* file = std::fopen(filename, "wb");
    if (file == nullptr)
        return false;

    const auto dataBytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
    std::fwrite("RIFF", 1, 4, file);
    writeU32(file, 36u + dataBytes);
    std::fwrite("WAVE", 1, 4, file);
    std::fwrite("fmt ", 1, 4, file);
    writeU32(file, 16);
    writeU16(file, 1);
    writeU16(file, 2);
    writeU32(file, sampleRate);
    writeU32(file, sampleRate * 2u * sizeof(int16_t));
    writeU16(file, 2u * sizeof(int16_t));
    writeU16(file, 16);
    std::fwrite("data", 1, 4, file);
    writeU32(file, dataBytes);
    std::fwrite(pcm.data(), sizeof(int16_t), pcm.size(), file);
    std::fclose(file);
    return true;
}
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

    std::vector<int16_t> pcm;
    pcm.reserve(static_cast<size_t>(samples) * 2u);

    double peakLeft = 0.0;
    double peakRight = 0.0;
    double sumSquaredLeft = 0.0;
    double sumSquaredRight = 0.0;
    int firstNonZeroFrame = -1;
    int lastNonZeroFrame = -1;

    for (int index = 0; index < samples; ++index)
    {
        const q31_t input = index == 0 ? FLOAT_TO_Q31(1.0f) : 0;
        q31_t left = 0;
        q31_t right = 0;
        te2350_process(&core, input, &left, &right);

        const int16_t pcmLeft = static_cast<int16_t>(left >> 16);
        const int16_t pcmRight = static_cast<int16_t>(right >> 16);
        pcm.push_back(pcmLeft);
        pcm.push_back(pcmRight);

        const auto leftFloat = static_cast<double>(pcmLeft) / 32768.0;
        const auto rightFloat = static_cast<double>(pcmRight) / 32768.0;
        peakLeft = std::fmax(peakLeft, std::fabs(leftFloat));
        peakRight = std::fmax(peakRight, std::fabs(rightFloat));
        sumSquaredLeft += leftFloat * leftFloat;
        sumSquaredRight += rightFloat * rightFloat;

        if (pcmLeft != 0 || pcmRight != 0)
        {
            if (firstNonZeroFrame < 0)
                firstNonZeroFrame = index;

            lastNonZeroFrame = index;
        }
    }

    std::FILE* rawFile = std::fopen("plugin_reference.raw", "wb");
    if (rawFile == nullptr)
    {
        std::fprintf(stderr, "failed to open plugin_reference.raw\n");
        return 1;
    }

    std::fwrite(pcm.data(), sizeof(int16_t), pcm.size(), rawFile);
    std::fclose(rawFile);

    if (!writeWav("plugin_reference.wav", pcm))
    {
        std::fprintf(stderr, "failed to open plugin_reference.wav\n");
        return 1;
    }

    std::FILE* metricsFile = std::fopen("plugin_reference_metrics.txt", "w");
    if (metricsFile == nullptr)
    {
        std::fprintf(stderr, "failed to open plugin_reference_metrics.txt\n");
        return 1;
    }

    const auto rmsLeft = std::sqrt(sumSquaredLeft / samples);
    const auto rmsRight = std::sqrt(sumSquaredRight / samples);
    std::fprintf(metricsFile, "frames=%d\n", samples);
    std::fprintf(metricsFile, "sample_rate=%d\n", sampleRate);
    std::fprintf(metricsFile, "seconds=%d\n", seconds);
    std::fprintf(metricsFile, "peak_l=%.9f\n", peakLeft);
    std::fprintf(metricsFile, "peak_r=%.9f\n", peakRight);
    std::fprintf(metricsFile, "rms_l=%.9f\n", rmsLeft);
    std::fprintf(metricsFile, "rms_r=%.9f\n", rmsRight);
    std::fprintf(metricsFile, "first_nonzero_frame=%d\n", firstNonZeroFrame);
    std::fprintf(metricsFile, "last_nonzero_frame=%d\n", lastNonZeroFrame);
    std::fclose(metricsFile);

    std::printf("Wrote plugin_reference.raw, plugin_reference.wav, and plugin_reference_metrics.txt\n");
    std::printf("Peak L/R %.6f %.6f, RMS L/R %.6f %.6f\n", peakLeft, peakRight, rmsLeft, rmsRight);
    return 0;
}
