#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "ReleaseMetadata.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <set>
#include <vector>

namespace
{
constexpr std::array<const char*, 30> expectedParameterIDs {{
    "space", "wild", "bloom", "timeMs", "syncMode", "feedback", "mix",
    "killDry", "lowCutHz", "highCutHz", "diffusion", "chaos", "wobble",
    "presence", "modRateHz", "modDepth", "modShape", "shimmerInterval",
    "shimmerAmount", "shimmerFeedback", "duckThreshold", "duckAmount",
    "inputTrim", "outputTrim", "bypass", "qualityMode", "freezeEngage",
    "freezeMode", "atmosFdnOn", "wetWidth"
}};

struct SweepConfiguration
{
    double sampleRate;
    int blockSize;
    int channels;
};

bool setPlainValue(TE2350AudioProcessor& processor,
                   juce::StringRef parameterID,
                   float plainValue)
{
    auto* parameter = processor.apvts.getParameter(parameterID);
    if (parameter == nullptr)
        return false;

    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
    return true;
}

bool bufferIsFiniteAndBounded(const juce::AudioBuffer<float>& buffer,
                              float& maximumPeak)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = buffer.getSample(channel, sample);
            if (!std::isfinite(value) || std::abs(value) > 32.0f)
                return false;

            maximumPeak = juce::jmax(maximumPeak, std::abs(value));
        }
    }

    return true;
}

void fillInput(juce::AudioBuffer<float>& buffer,
               double sampleRate,
               double& phase,
               int blockIndex)
{
    const auto increment = juce::MathConstants<double>::twoPi * 223.0 / sampleRate;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto value = 0.07f * static_cast<float>(std::sin(phase));
        if (((blockIndex * buffer.getNumSamples() + sample) % 997) == 0)
            value += 0.18f;
        phase += increment;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, channel == 0 ? value : value * 0.83f);
    }
}

bool verifyParameterAndProgramContract()
{
    TE2350AudioProcessor processor;
    if (processor.getName() != "TE-2350 Antigravity"
        || juce::String(TE2350_PRODUCT_VERSION).isEmpty()
        || TE2350_RELEASE_PRESET_FORMAT_VERSION != 1
        || TE2350_RELEASE_STATE_VERSION != 3)
    {
        std::fprintf(stderr, "release metadata/schema contract changed\n");
        return false;
    }
    const auto& parameters = processor.getParameters();
    if (parameters.size() != expectedParameterIDs.size())
    {
        std::fprintf(stderr, "parameter contract changed: expected=%zu actual=%d\n",
                     expectedParameterIDs.size(), parameters.size());
        return false;
    }

    std::set<std::string> actualIDs;
    for (auto* parameter : parameters)
    {
        const auto* identified =
            dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        const auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (identified == nullptr || ranged == nullptr || !parameter->isAutomatable())
        {
            std::fprintf(stderr, "non-automatable or unidentified release parameter\n");
            return false;
        }

        actualIDs.insert(identified->paramID.toStdString());
        const auto& range = ranged->getNormalisableRange();
        if (!std::isfinite(range.start)
            || !std::isfinite(range.end)
            || range.start >= range.end
            || !std::isfinite(ranged->getDefaultValue()))
        {
            std::fprintf(stderr, "invalid parameter range: %s\n",
                         identified->paramID.toRawUTF8());
            return false;
        }
    }

    for (const auto* expectedID : expectedParameterIDs)
    {
        if (actualIDs.count(expectedID) == 0)
        {
            std::fprintf(stderr, "release parameter missing: %s\n", expectedID);
            return false;
        }
    }

    if (processor.getNumPrograms() != 20)
    {
        std::fprintf(stderr, "factory program contract changed: %d programs\n",
                     processor.getNumPrograms());
        return false;
    }

    std::set<std::string> programNames;
    for (int program = 0; program < processor.getNumPrograms(); ++program)
    {
        const auto name = processor.getProgramName(program);
        if (name.isEmpty() || !programNames.insert(name.toStdString()).second)
        {
            std::fprintf(stderr, "empty or duplicated factory program name\n");
            return false;
        }
    }

    return true;
}

bool configureLayout(TE2350AudioProcessor& processor, int channels)
{
    juce::AudioProcessor::BusesLayout layout;
    const auto channelSet = channels == 1 ? juce::AudioChannelSet::mono()
                                          : juce::AudioChannelSet::stereo();
    layout.inputBuses.add(channelSet);
    layout.outputBuses.add(channelSet);
    return processor.setBusesLayout(layout);
}

bool verifyAutomationSweep(const SweepConfiguration& configuration,
                           float& maximumPeak)
{
    TE2350AudioProcessor processor;
    if (!configureLayout(processor, configuration.channels))
    {
        std::fprintf(stderr, "layout rejected during automation sweep\n");
        return false;
    }

    processor.prepareToPlay(configuration.sampleRate, configuration.blockSize);
    const auto reportedLatency = processor.getLatencySamples();
    juce::AudioBuffer<float> buffer(configuration.channels, configuration.blockSize);
    juce::MidiBuffer midi;
    double phase = 0.0;

    const auto& parameters = processor.getParameters();
    auto blockIndex = 0;
    const auto processCurrentBlock = [&]
    {
        fillInput(buffer, configuration.sampleRate, phase, blockIndex++);
        midi.clear();
        processor.processBlock(buffer, midi);
        return processor.getLatencySamples() == reportedLatency
            && bufferIsFiniteAndBounded(buffer, maximumPeak);
    };

    for (auto* parameter : parameters)
    {
        const auto defaultValue = parameter->getDefaultValue();
        parameter->setValueNotifyingHost(0.0f);
        if (!processCurrentBlock())
            return false;

        parameter->setValueNotifyingHost(1.0f);
        if (!processCurrentBlock())
            return false;

        parameter->setValueNotifyingHost(defaultValue);
    }

    for (int block = 0; block < 240; ++block)
    {
        for (int index = 0; index < parameters.size(); ++index)
        {
            auto normalised = 0.45f + 0.40f * std::sin(
                static_cast<float>(block) * (0.017f + 0.0003f * index)
                + static_cast<float>(index) * 0.37f);

            if (const auto* identified =
                    dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters[index]))
            {
                if (identified->paramID == "inputTrim"
                    || identified->paramID == "outputTrim")
                    normalised = 0.50f + (normalised - 0.45f) * 0.25f;
            }

            parameters[index]->setValueNotifyingHost(
                juce::jlimit(0.0f, 1.0f, normalised));
        }

        if (!processCurrentBlock())
        {
            std::fprintf(stderr,
                         "automation sweep failed: rate=%.0f block=%d channels=%d at=%d\n",
                         configuration.sampleRate,
                         configuration.blockSize,
                         configuration.channels,
                         block);
            return false;
        }
    }

    return true;
}

bool verifyPresetSwitchingUnderLoad(float& maximumPeak)
{
    TE2350AudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer midi;
    double phase = 0.0;
    auto blockIndex = 0;

    for (int program = 0; program < processor.getNumPrograms(); ++program)
    {
        processor.setCurrentProgram(program);
        if (processor.getCurrentProgram() != program
            || processor.getProgramName(program) != processor.getActivePresetName()
            || processor.isActivePresetUser())
        {
            std::fprintf(stderr, "factory program identity failed under load\n");
            return false;
        }

        for (int block = 0; block < 24; ++block)
        {
            fillInput(buffer, 48000.0, phase, blockIndex++);
            midi.clear();
            processor.processBlock(buffer, midi);
            if (!bufferIsFiniteAndBounded(buffer, maximumPeak))
            {
                std::fprintf(stderr, "preset switch produced invalid audio: %s\n",
                             processor.getProgramName(program).toRawUTF8());
                return false;
            }
        }
    }

    return true;
}

bool verifyStateRecallAndDeterministicAudio(float& maximumDifference)
{
    TE2350AudioProcessor source;
    source.setCurrentProgram(11);

    for (int index = 0; index < source.getParameters().size(); ++index)
    {
        auto value = 0.12f + 0.76f * static_cast<float>((index * 7) % 29) / 28.0f;
        if (source.getParameters()[index]->getNumSteps() == 2)
            value = (index % 2) == 0 ? 0.0f : 1.0f;
        source.getParameters()[index]->setValueNotifyingHost(value);
    }
    setPlainValue(source, "bypass", 0.0f);
    setPlainValue(source, "freezeEngage", 0.0f);
    setPlainValue(source, "killDry", 0.0f);
    setPlainValue(source, "qualityMode", 1.0f);
    source.setActiveUserPreset("RC Recall Probe");

    std::vector<float> expectedValues;
    expectedValues.reserve(static_cast<size_t>(source.getParameters().size()));
    for (auto* parameter : source.getParameters())
        expectedValues.push_back(parameter->getValue());

    juce::MemoryBlock savedState;
    source.getStateInformation(savedState);
    if (savedState.isEmpty())
    {
        std::fprintf(stderr, "host state serialization returned an empty block\n");
        return false;
    }

    auto stateXml = juce::AudioProcessor::getXmlFromBinary(
        savedState.getData(), static_cast<int>(savedState.getSize()));
    if (stateXml == nullptr
        || static_cast<int>(stateXml->getIntAttribute("stateVersion")) != 3)
    {
        std::fprintf(stderr, "host state did not advertise stateVersion=3\n");
        return false;
    }

    TE2350AudioProcessor restored;
    restored.setStateInformation(savedState.getData(),
                                 static_cast<int>(savedState.getSize()));
    if (!restored.isActivePresetUser()
        || restored.getActivePresetName() != "RC Recall Probe"
        || restored.getCurrentProgram() != 11
        || restored.getParameters().size() != expectedValues.size())
    {
        std::fprintf(stderr, "host state identity did not round-trip\n");
        return false;
    }

    for (int index = 0; index < restored.getParameters().size(); ++index)
    {
        if (std::abs(restored.getParameters()[index]->getValue()
                     - expectedValues[static_cast<size_t>(index)]) > 0.000001f)
        {
            std::fprintf(stderr, "parameter recall mismatch at index=%d\n", index);
            return false;
        }
    }

    source.prepareToPlay(48000.0, 128);
    restored.prepareToPlay(48000.0, 128);
    source.reset();
    restored.reset();

    juce::AudioBuffer<float> input(2, 128);
    juce::AudioBuffer<float> sourceOutput(2, 128);
    juce::AudioBuffer<float> restoredOutput(2, 128);
    juce::MidiBuffer sourceMidi;
    juce::MidiBuffer restoredMidi;
    double phase = 0.0;

    for (int block = 0; block < 180; ++block)
    {
        fillInput(input, 48000.0, phase, block);
        sourceOutput.makeCopyOf(input, true);
        restoredOutput.makeCopyOf(input, true);
        sourceMidi.clear();
        restoredMidi.clear();
        source.processBlock(sourceOutput, sourceMidi);
        restored.processBlock(restoredOutput, restoredMidi);

        for (int channel = 0; channel < 2; ++channel)
        {
            for (int sample = 0; sample < 128; ++sample)
            {
                const auto difference = std::abs(
                    sourceOutput.getSample(channel, sample)
                    - restoredOutput.getSample(channel, sample));
                maximumDifference = juce::jmax(maximumDifference, difference);
                if (!std::isfinite(difference) || difference > 0.000001f)
                {
                    std::fprintf(stderr,
                                 "recalled processor diverged: block=%d channel=%d sample=%d diff=%.9f\n",
                                 block, channel, sample, difference);
                    return false;
                }
            }
        }
    }

    return true;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    float maximumPeak = 0.0f;
    float maximumRecallDifference = 0.0f;

    constexpr std::array<SweepConfiguration, 4> configurations {{
        { 44100.0, 64, 1 },
        { 48000.0, 127, 2 },
        { 96000.0, 256, 2 },
        { 192000.0, 512, 2 }
    }};

    if (!verifyParameterAndProgramContract())
        return 1;

    for (const auto& configuration : configurations)
        if (!verifyAutomationSweep(configuration, maximumPeak))
            return 1;

    if (!verifyPresetSwitchingUnderLoad(maximumPeak)
        || !verifyStateRecallAndDeterministicAudio(maximumRecallDifference))
    {
        return 1;
    }

    std::printf("Release readiness passed: 30 automatable parameters, 20 programs, "
                "44.1-192 kHz, mono/stereo, maxPeak=%.4f recallDiff=%.9f\n",
                maximumPeak,
                maximumRecallDifference);
    return 0;
}
