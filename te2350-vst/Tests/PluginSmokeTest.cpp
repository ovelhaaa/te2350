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

float getPlainParameterValue(TE2350AudioProcessor& processor, const char* parameterID)
{
    if (auto* parameter = processor.apvts.getParameter(parameterID))
        return parameter->convertFrom0to1(parameter->getValue());

    return 0.0f;
}

bool verifyStereoDryPath(TE2350AudioProcessor& processor,
                         juce::AudioBuffer<float>& buffer,
                         juce::MidiBuffer& midi)
{
    if (! setParameter(processor, "bloom", 0.0f)
        || ! setParameter(processor, "mix", 0.0f)
        || ! setParameter(processor, "inputTrim", 0.0f)
        || ! setParameter(processor, "outputTrim", 0.0f)
        || ! setParameter(processor, "bypass", 0.0f))
    {
        return false;
    }

    for (int block = 0; block < 250; ++block)
    {
        buffer.clear();
        processor.processBlock(buffer, midi);
    }

    buffer.clear();
    buffer.setSample(0, 0, 0.5f);
    buffer.setSample(1, 0, -0.5f);
    processor.processBlock(buffer, midi);

    const auto delayedSample = juce::jlimit(0, buffer.getNumSamples() - 1,
                                            processor.getLatencySamples());
    const auto left = buffer.getSample(0, delayedSample);
    const auto right = buffer.getSample(1, delayedSample);
    if (left < 0.35f || right > -0.35f)
    {
        std::fprintf(stderr,
                     "stereo dry path collapsed or attenuated: left=%.6f right=%.6f\n",
                     left,
                     right);
        return false;
    }

    return true;
}

std::vector<float> renderQualityMode(float qualityMode)
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    TE2350AudioProcessor processor;

    setParameter(processor, "space", 0.0f);
    setParameter(processor, "wild", 0.0f);
    setParameter(processor, "bloom", 0.0f);
    setParameter(processor, "mix", 0.0f);
    setParameter(processor, "inputTrim", 0.0f);
    setParameter(processor, "outputTrim", 0.0f);
    setParameter(processor, "qualityMode", qualityMode);
    processor.prepareToPlay(sampleRate, blockSize);

    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;
    std::vector<float> rendered;
    double phase = 0.0;
    const auto phaseIncrement = juce::MathConstants<double>::twoPi * 997.0 / sampleRate;

    for (int block = 0; block < 40; ++block)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = static_cast<float>(0.85 * std::sin(phase));
            phase += phaseIncrement;
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, value);
        }

        processor.processBlock(buffer, midi);
        if (block >= 32)
            rendered.insert(rendered.end(),
                            buffer.getReadPointer(0),
                            buffer.getReadPointer(0) + blockSize);
    }

    return rendered;
}

bool verifyStudioModeIsAudible()
{
    const auto hardware = renderQualityMode(0.0f);
    const auto studio = renderQualityMode(1.0f);
    if (hardware.size() != studio.size() || hardware.empty())
        return false;

    double differenceEnergy = 0.0;
    double hardwareEnergy = 0.0;
    for (size_t sample = 0; sample < hardware.size(); ++sample)
    {
        const auto difference = static_cast<double>(hardware[sample] - studio[sample]);
        differenceEnergy += difference * difference;
        hardwareEnergy += static_cast<double>(hardware[sample]) * hardware[sample];
    }

    const auto differenceRms = std::sqrt(differenceEnergy / hardware.size());
    const auto hardwareRms = std::sqrt(hardwareEnergy / hardware.size());
    if (differenceRms < 0.01 || hardwareRms < 0.1)
    {
        std::fprintf(stderr,
                     "Studio Mode was not audibly distinct: diffRms=%.6f hardwareRms=%.6f\n",
                     differenceRms,
                     hardwareRms);
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
        "inputTrim", "outputTrim", "bypass", "qualityMode", "freezeEngage", "freezeMode",
        "atmosFdnOn", "wetWidth"
    };

    if (!requireParameters(processor, requiredParameters))
        return 1;

    if (processor.getTailLengthSeconds() < 60.0)
    {
        std::fprintf(stderr, "reported tail length does not cover maximum Bloom RT60\n");
        return 1;
    }

    if (! verifyStudioModeIsAudible())
        return 1;

    if (! setParameter(processor, "chaos", 1.0f))
        return 1;

    processor.setCurrentProgram(0);
    if (std::fabs(getPlainParameterValue(processor, "chaos")) > 0.0001f)
    {
        std::fprintf(stderr, "factory preset did not reset unspecified parameters\n");
        return 1;
    }

    processor.setCurrentProgram(3);
    juce::MemoryBlock savedState;
    processor.getStateInformation(savedState);
    TE2350AudioProcessor restoredProcessor;
    restoredProcessor.setStateInformation(savedState.getData(),
                                          static_cast<int>(savedState.getSize()));
    if (restoredProcessor.getCurrentProgram() != 3)
    {
        std::fprintf(stderr, "program index did not survive state round-trip\n");
        return 1;
    }

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numBlocks = 400;
    processor.prepareToPlay(sampleRate, blockSize);
    if (processor.getLatencySamples() <= 0)
    {
        std::fprintf(stderr, "fixed oversampling latency was not reported\n");
        return 1;
    }

    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;

    if (! verifyStereoDryPath(processor, buffer, midi))
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
        || !setParameter(processor, "modShape", 2.0f)
        || !setParameter(processor, "shimmerAmount", 1.0f)
        || !setParameter(processor, "shimmerFeedback", 0.95f)
        || !setParameter(processor, "atmosFdnOn", 1.0f)
        || !setParameter(processor, "wetWidth", 1.0f)
        || !setParameter(processor, "qualityMode", 1.0f))
    {
        return 1;
    }

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
    std::printf("Plugin smoke test peak=%.6f rms=%.6f instability=%.6f latency=%d\n",
                peak,
                rms,
                processor.getInstabilityMeterValue(),
                processor.getLatencySamples());

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
