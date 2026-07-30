#include <JuceHeader.h>

#include "Core/TE2350CoreWrapper.h"
#include "DSP/OversamplingChain.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 128;

double targetRt60(float bloom)
{
    return 0.5 * std::pow(120.0, juce::jlimit(0.0f, 1.0f, bloom));
}

double estimateRt60(std::vector<double>& energy)
{
    const auto peak = *std::max_element(energy.begin(), energy.end());
    const auto noiseGate = peak * 1.0e-10;
    for (auto& sampleEnergy : energy)
        if (sampleEnergy < noiseGate)
            sampleEnergy = 0.0;

    for (auto sample = energy.size() - 1; sample > 0; --sample)
        energy[sample - 1] += energy[sample];

    if (energy.empty() || energy.front() <= 0.0)
        return 0.0;

    const auto findDecayCrossing = [&energy] (double thresholdDb)
    {
        const auto threshold = energy.front() * std::pow(10.0, thresholdDb / 10.0);
        for (size_t sample = 0; sample < energy.size(); ++sample)
            if (energy[sample] <= threshold)
                return sample;
        return energy.size();
    };

    const auto minus60 = findDecayCrossing(-60.0);
    if (minus60 >= energy.size())
        return 0.0;

    return static_cast<double>(minus60) / sampleRate;
}

double measureRt60(float bloom)
{
    te2350::TE2350CoreWrapper core;
    if (! core.prepare(sampleRate, blockSize))
        return 0.0;

    te2350::CoreParameters parameters;
    parameters.space = 0.0f;
    parameters.wild = 0.0f;
    parameters.bloom = bloom;
    parameters.timeMs = 500.0f;
    parameters.feedback = 0.0f;
    parameters.mix = 1.0f;
    parameters.killDry = true;
    parameters.lowCutHz = 20.0f;
    parameters.highCutHz = 18000.0f;
    parameters.diffusion = 0.35f;
    parameters.chaos = 0.0f;
    parameters.wobble = 0.0f;
    parameters.presence = 0.25f;
    parameters.modRateHz = 0.10f;
    parameters.modDepth = 0.0f;
    parameters.shimmerAmount = 0.0f;
    parameters.shimmerFeedback = 0.0f;
    parameters.duckAmount = 0.0f;
    parameters.inputTrimDb = 0.0f;
    parameters.outputTrimDb = 0.0f;
    parameters.atmosFdnOn = false;
    parameters.wetWidth = 1.0f;
    core.setParameters(parameters);

    juce::AudioBuffer<float> block(2, blockSize);
    for (int warmup = 0; warmup < static_cast<int>(sampleRate / blockSize); ++warmup)
    {
        block.clear();
        core.processBlock(block);
    }

    const auto target = targetRt60(bloom);
    const auto renderSeconds = juce::jlimit(4.0, 80.0, target * 4.0 + 2.0);
    const auto totalSamples = static_cast<int>(std::ceil(renderSeconds * sampleRate));
    std::vector<double> energy(static_cast<size_t>(totalSamples), 0.0);

    int rendered = 0;
    while (rendered < totalSamples)
    {
        block.clear();
        if (rendered == 0)
        {
            block.setSample(0, 0, 0.5f);
            block.setSample(1, 0, 0.5f);
        }

        core.processBlock(block);
        const auto samplesThisBlock = juce::jmin(blockSize, totalSamples - rendered);
        for (int sample = 0; sample < samplesThisBlock; ++sample)
        {
            const auto left = static_cast<double>(block.getSample(0, sample));
            const auto right = static_cast<double>(block.getSample(1, sample));
            energy[static_cast<size_t>(rendered + sample)] = 0.5 * (left * left + right * right);
        }

        rendered += samplesThisBlock;
    }

    return estimateRt60(energy);
}

double measureStudioDeltaDb(float peakAmplitude)
{
    te2350::OversamplingChain hardware;
    te2350::OversamplingChain studio;
    hardware.prepare(sampleRate, blockSize, 2);
    studio.prepare(sampleRate, blockSize, 2);
    hardware.setStudioMode(false);
    studio.setStudioMode(true);

    juce::AudioBuffer<float> hardwareBlock(2, blockSize);
    juce::AudioBuffer<float> studioBlock(2, blockSize);
    double phase = 0.0;
    double hardwareEnergy = 0.0;
    double studioEnergy = 0.0;
    size_t measuredSamples = 0;
    const auto phaseIncrement = juce::MathConstants<double>::twoPi * 997.0 / sampleRate;

    for (int blockIndex = 0; blockIndex < 400; ++blockIndex)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = peakAmplitude * static_cast<float>(std::sin(phase));
            phase += phaseIncrement;
            hardwareBlock.setSample(0, sample, value);
            hardwareBlock.setSample(1, sample, value);
            studioBlock.setSample(0, sample, value);
            studioBlock.setSample(1, sample, value);
        }

        hardware.processEffectBlock(hardwareBlock);
        studio.processEffectBlock(studioBlock);
        if (blockIndex < 100)
            continue;

        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto fixed = static_cast<double>(hardwareBlock.getSample(0, sample));
            const auto polished = static_cast<double>(studioBlock.getSample(0, sample));
            hardwareEnergy += fixed * fixed;
            studioEnergy += polished * polished;
            ++measuredSamples;
        }
    }

    if (hardwareEnergy <= 0.0 || studioEnergy <= 0.0 || measuredSamples == 0)
        return -100.0;

    return 10.0 * std::log10(studioEnergy / hardwareEnergy);
}
}

int main()
{
    bool passed = true;
    const std::array<float, 6> bloomValues {
        0.0f, 0.25f, 0.50f, 0.75f, 0.90f, 1.0f
    };
    for (const auto bloom : bloomValues)
    {
        const auto target = targetRt60(bloom);
        const auto measured = measureRt60(bloom);
        const auto ratio = target > 0.0 ? measured / target : 0.0;
        std::printf("RT60 bloom=%.2f target=%.3fs measured=%.3fs ratio=%.3f\n",
                    bloom,
                    target,
                    measured,
                    ratio);
        passed = passed && ratio >= 0.55 && ratio <= 1.60;
    }

    for (const auto amplitude : { 0.125f, 0.25f, 0.50f, 0.85f })
    {
        const auto deltaDb = measureStudioDeltaDb(amplitude);
        std::printf("Studio sine peak=%.3f delta=%+.3f dB\n", amplitude, deltaDb);
        passed = passed && std::abs(deltaDb) <= 0.60;
    }

    if (! passed)
        std::fprintf(stderr, "Calibration limits exceeded\n");
    return passed ? 0 : 1;
}
