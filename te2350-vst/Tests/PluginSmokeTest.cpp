#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
bool setParameter(TE2350AudioProcessor& processor, const char* parameterID, float plainValue)
{
    auto* parameter = processor.apvts.getParameter(parameterID);
    if (parameter == nullptr)
    {
        std::fprintf(stderr, "missing parameter: %s\n", parameterID);
        return false;
    }

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
    parameter->endChangeGesture();
    return true;
}

bool requireParameters(TE2350AudioProcessor& processor, const std::vector<const char*>& ids)
{
    for (const auto* id : ids)
        if (processor.apvts.getParameter(id) == nullptr)
        {
            std::fprintf(stderr, "missing APVTS parameter: %s\n", id);
            return false;
        }

    return true;
}
}

int main()
{
    TE2350AudioProcessor processor;

    const std::vector<const char*> requiredParameters {
        "space", "wild", "bloom", "timeMs", "syncMode", "feedback", "mix",
        "killDry", "lowCutHz", "highCutHz", "diffusion", "chaos", "wobble",
        "presence", "modRateHz", "modDepth", "modShape", "shimmerInterval",
        "shimmerAmount", "shimmerFeedback", "duckThreshold", "duckAmount",
        "inputTrim", "outputTrim", "qualityMode", "freezeEngage", "freezeMode",
        "atmosFdnOn", "wetWidth"
    };

    if (!requireParameters(processor, requiredParameters))
        return 1;

    if (!setParameter(processor, "space", 1.0f)
        || !setParameter(processor, "wild", 1.0f)
        || !setParameter(processor, "bloom", 1.0f)
        || !setParameter(processor, "feedback", 1.05f)
        || !setParameter(processor, "mix", 1.0f)
        || !setParameter(processor, "diffusion", 1.0f)
        || !setParameter(processor, "chaos", 1.0f)
        || !setParameter(processor, "wobble", 1.0f)
        || !setParameter(processor, "modDepth", 1.0f)
        || !setParameter(processor, "shimmerAmount", 1.0f)
        || !setParameter(processor, "shimmerFeedback", 0.95f)
        || !setParameter(processor, "atmosFdnOn", 1.0f)
        || !setParameter(processor, "wetWidth", 1.0f))
    {
        return 1;
    }

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numBlocks = 400;
    processor.prepareToPlay(sampleRate, blockSize);

    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;

    float peak = 0.0f;
    double sumSquares = 0.0;
    int sampleCount = 0;

    for (int block = 0; block < numBlocks; ++block)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = ((block * blockSize + sample) % 97) == 0 ? 1.0f : 0.0f;
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, -value);
        }

        processor.processBlock(buffer, midi);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto value = buffer.getSample(channel, sample);
                if (!std::isfinite(value))
                {
                    std::fprintf(stderr, "non-finite output at block=%d channel=%d sample=%d\n", block, channel, sample);
                    return 1;
                }

                peak = juce::jmax(peak, std::fabs(value));
                sumSquares += static_cast<double>(value) * static_cast<double>(value);
                ++sampleCount;
            }
        }
    }

    const auto rms = std::sqrt(sumSquares / sampleCount);
    std::printf("Plugin smoke test peak=%.6f rms=%.6f instability=%.6f\n",
                peak,
                rms,
                processor.getInstabilityMeterValue());

    if (peak > 1.25f)
    {
        std::fprintf(stderr, "output exceeded smoke-test ceiling: peak=%.6f\n", peak);
        return 1;
    }

    if (processor.getInstabilityMeterValue() < 0.95f)
    {
        std::fprintf(stderr, "wild macro did not drive instability meter high enough\n");
        return 1;
    }

    processor.releaseResources();
    return 0;
}
