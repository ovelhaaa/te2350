#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "Presets/FactoryPresets.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <memory>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 128;
constexpr double inputSeconds = 8.0;
constexpr double tailSeconds = 12.0;

bool setParameter(TE2350AudioProcessor& processor,
                  const char* parameterID,
                  float plainValue)
{
    auto* parameter = processor.apvts.getParameter(parameterID);
    if (parameter == nullptr)
        return false;

    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
    return true;
}

float midiToHz(int midiNote)
{
    return 440.0f * std::pow(2.0f, (static_cast<float>(midiNote) - 69.0f) / 12.0f);
}

float deterministicNoise(int sample)
{
    auto value = static_cast<juce::uint32>(sample) * 747796405u + 2891336453u;
    value = ((value >> ((value >> 28u) + 4u)) ^ value) * 277803737u;
    value = (value >> 22u) ^ value;
    return static_cast<float>(static_cast<juce::int32>(value)) / 2147483648.0f;
}

void generateMusicalInput(juce::AudioBuffer<float>& destination)
{
    constexpr std::array<std::array<int, 4>, 4> chords {{
        {{ 48, 55, 59, 64 }},
        {{ 45, 52, 55, 60 }},
        {{ 41, 48, 52, 57 }},
        {{ 43, 50, 55, 60 }}
    }};
    constexpr std::array<int, 8> melody {{ 72, 76, 79, 76, 69, 72, 74, 67 }};

    for (int sample = 0; sample < destination.getNumSamples(); ++sample)
    {
        const auto time = static_cast<double>(sample) / sampleRate;
        if (time >= inputSeconds)
            continue;

        const auto chordIndex = juce::jlimit(
            0, 3, static_cast<int>(time / 2.0));
        const auto beatPhase = std::fmod(time, 0.5);
        const auto beatEnvelope = std::exp(-beatPhase * 7.5);
        const auto barFade = juce::jmin(1.0, time * 5.0)
                           * juce::jmin(1.0, (inputSeconds - time) * 4.0);

        double left = 0.0;
        double right = 0.0;
        for (size_t voice = 0; voice < chords[static_cast<size_t>(chordIndex)].size(); ++voice)
        {
            const auto frequency = midiToHz(
                chords[static_cast<size_t>(chordIndex)][voice]);
            const auto voiceGain = 0.035 / (1.0 + 0.18 * static_cast<double>(voice));
            left += std::sin(juce::MathConstants<double>::twoPi * frequency * time
                             + 0.12 * static_cast<double>(voice)) * voiceGain;
            right += std::sin(juce::MathConstants<double>::twoPi * frequency * time
                              - 0.16 * static_cast<double>(voice)) * voiceGain;
        }

        const auto step = static_cast<int>(time / 0.5);
        const auto melodyFrequency = midiToHz(
            melody[static_cast<size_t>(step) % melody.size()]);
        const auto pluck = std::sin(
            juce::MathConstants<double>::twoPi * melodyFrequency * time)
                         * 0.20 * beatEnvelope;
        const auto secondHarmonic = std::sin(
            juce::MathConstants<double>::twoPi * melodyFrequency * 2.0 * time)
                                  * 0.055 * beatEnvelope;

        const auto quarterPhase = std::fmod(time, 0.5);
        const auto kickEnvelope = std::exp(-quarterPhase * 22.0);
        const auto kick = std::sin(
            juce::MathConstants<double>::twoPi
            * (52.0 + 38.0 * kickEnvelope) * quarterPhase)
                        * 0.22 * kickEnvelope;

        const auto eighthPhase = std::fmod(time, 0.25);
        const auto hatEnvelope = std::exp(-eighthPhase * 45.0);
        const auto hat = deterministicNoise(sample) * 0.035 * hatEnvelope;

        left = (left + pluck + secondHarmonic + kick + hat) * barFade;
        right = (right + pluck * 0.96 - secondHarmonic * 0.85 + kick - hat) * barFade;
        destination.setSample(0, sample, juce::jlimit(-0.9f, 0.9f, static_cast<float>(left)));
        destination.setSample(1, sample, juce::jlimit(-0.9f, 0.9f, static_cast<float>(right)));
    }
}

bool writeWav(const juce::File& file, const juce::AudioBuffer<float>& audio)
{
    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
    if (stream == nullptr || ! stream->openedOk())
        return false;

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(stream.release(),
                               sampleRate,
                               static_cast<unsigned int>(audio.getNumChannels()),
                               24,
                               {},
                               0));
    return writer != nullptr
        && writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
}

juce::String safeFileStem(juce::String name)
{
    return name.toLowerCase()
        .replaceCharacter(' ', '_')
        .retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789_-");
}

struct Metrics
{
    float peak = 0.0f;
    float activeRms = 0.0f;
    float finalTailRms = 0.0f;
};

Metrics measure(const juce::AudioBuffer<float>& audio)
{
    Metrics result;
    const auto activeSamples = static_cast<int>(inputSeconds * sampleRate);
    const auto finalSecondStart = juce::jmax(
        0, audio.getNumSamples() - static_cast<int>(sampleRate));

    double activeEnergy = 0.0;
    double tailEnergy = 0.0;
    size_t activeCount = 0;
    size_t tailCount = 0;
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
    {
        const auto* samples = audio.getReadPointer(channel);
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            const auto value = samples[sample];
            result.peak = juce::jmax(result.peak, std::abs(value));
            if (sample < activeSamples)
            {
                activeEnergy += static_cast<double>(value) * value;
                ++activeCount;
            }
            if (sample >= finalSecondStart)
            {
                tailEnergy += static_cast<double>(value) * value;
                ++tailCount;
            }
        }
    }

    result.activeRms = activeCount > 0
        ? static_cast<float>(std::sqrt(activeEnergy / activeCount)) : 0.0f;
    result.finalTailRms = tailCount > 0
        ? static_cast<float>(std::sqrt(tailEnergy / tailCount)) : 0.0f;
    return result;
}

juce::AudioBuffer<float> renderPreset(int presetIndex,
                                      const juce::AudioBuffer<float>& dryInput)
{
    TE2350AudioProcessor processor;
    processor.setCurrentProgram(presetIndex);
    setParameter(processor, "qualityMode", 0.0f);
    processor.prepareToPlay(sampleRate, blockSize);

    juce::AudioBuffer<float> rendered(
        2, static_cast<int>((inputSeconds + tailSeconds) * sampleRate));
    juce::AudioBuffer<float> block(2, blockSize);
    juce::MidiBuffer midi;

    int position = 0;
    while (position < rendered.getNumSamples())
    {
        block.clear();
        const auto samplesThisBlock = juce::jmin(
            blockSize, rendered.getNumSamples() - position);
        if (position < dryInput.getNumSamples())
        {
            const auto inputSamples = juce::jmin(
                samplesThisBlock, dryInput.getNumSamples() - position);
            for (int channel = 0; channel < 2; ++channel)
                block.copyFrom(channel, 0, dryInput, channel, position, inputSamples);
        }

        processor.processBlock(block, midi);
        for (int channel = 0; channel < 2; ++channel)
            rendered.copyFrom(channel, position, block, channel, 0, samplesThisBlock);
        position += samplesThisBlock;
    }

    return rendered;
}
}

int main(int argc, char** argv)
{
    const auto outputDirectory = argc > 1
        ? juce::File(juce::String::fromUTF8(argv[1]))
        : juce::File::getCurrentWorkingDirectory().getChildFile("CalibrationRenders");
    if (! outputDirectory.createDirectory())
    {
        std::fprintf(stderr, "Could not create output directory\n");
        return 1;
    }

    juce::AudioBuffer<float> dryInput(
        2, static_cast<int>((inputSeconds + tailSeconds) * sampleRate));
    dryInput.clear();
    generateMusicalInput(dryInput);
    if (! writeWav(outputDirectory.getChildFile("00_dry_reference.wav"), dryInput))
        return 1;

    const auto metricsFile = outputDirectory.getChildFile("preset_metrics.csv");
    metricsFile.deleteFile();
    std::unique_ptr<juce::FileOutputStream> metrics(metricsFile.createOutputStream());
    if (metrics == nullptr || ! metrics->openedOk())
        return 1;
    metrics->writeText("preset,peak,active_rms_db,tail_rms_db\n", false, false, nullptr);

    const auto names = te2350::getFactoryPresetNames();
    for (int preset = 0; preset < names.size(); ++preset)
    {
        auto rendered = renderPreset(preset, dryInput);
        const auto result = measure(rendered);
        const auto filename = juce::String(preset + 1).paddedLeft('0', 2)
                            + "_" + safeFileStem(names[preset]) + ".wav";
        if (! writeWav(outputDirectory.getChildFile(filename), rendered))
            return 1;

        const auto activeDb = juce::Decibels::gainToDecibels(
            result.activeRms, -120.0f);
        const auto tailDb = juce::Decibels::gainToDecibels(
            result.finalTailRms, -120.0f);
        metrics->writeText(names[preset] + ","
                           + juce::String(result.peak, 6) + ","
                           + juce::String(activeDb, 3) + ","
                           + juce::String(tailDb, 3) + "\n",
                           false,
                           false,
                           nullptr);
        std::printf("%s peak=%.4f active=%.2f dB tail@12s=%.2f dB\n",
                    names[preset].toRawUTF8(),
                    result.peak,
                    activeDb,
                    tailDb);
    }

    metrics->flush();
    std::printf("Calibration renders written to %s\n",
                outputDirectory.getFullPathName().toRawUTF8());
    return 0;
}
