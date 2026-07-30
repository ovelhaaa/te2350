#include "OversamplingChain.h"

#include <cmath>

namespace te2350
{
void OversamplingChain::prepare(double sampleRate, int maximumBlockSize, int numChannels)
{
    preparedChannels = juce::jmax(1, numChannels);
    preparedBlockSize = juce::jmax(1, maximumBlockSize);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(preparedChannels),
        1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,
        true);
    oversampler->initProcessing(static_cast<size_t>(preparedBlockSize));
    latencySamples = juce::roundToInt(oversampler->getLatencyInSamples());

    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(preparedBlockSize),
        static_cast<juce::uint32>(preparedChannels)
    };
    hardwareDelay.setMaximumDelayInSamples(latencySamples + 4);
    bypassDelay.setMaximumDelayInSamples(latencySamples + 4);
    hardwareDelay.prepare(spec);
    bypassDelay.prepare(spec);
    hardwareDelay.setDelay(static_cast<float>(latencySamples));
    bypassDelay.setDelay(static_cast<float>(latencySamples));

    hardwareBuffer.setSize(preparedChannels, preparedBlockSize, false, false, true);
    studioBuffer.setSize(preparedChannels, preparedBlockSize, false, false, true);
    studioMix.reset(sampleRate, 0.02);
    studioMix.setCurrentAndTargetValue(studioMode ? 1.0f : 0.0f);
    reset();
}

void OversamplingChain::reset()
{
    if (oversampler != nullptr)
        oversampler->reset();

    hardwareDelay.reset();
    bypassDelay.reset();
    hardwareBuffer.clear();
    studioBuffer.clear();
    studioMix.setCurrentAndTargetValue(studioMode ? 1.0f : 0.0f);
}

void OversamplingChain::setStudioMode(bool shouldUseStudioMode)
{
    if (studioMode == shouldUseStudioMode)
        return;

    studioMode = shouldUseStudioMode;
    if (studioMode && oversampler != nullptr)
        oversampler->reset();

    studioMix.setTargetValue(studioMode ? 1.0f : 0.0f);
}

void OversamplingChain::processEffectBlock(juce::AudioBuffer<float>& buffer)
{
    if (oversampler == nullptr || buffer.getNumSamples() <= 0)
        return;

    jassert(buffer.getNumChannels() <= preparedChannels);
    jassert(buffer.getNumSamples() <= preparedBlockSize);

    const auto channels = juce::jmin(buffer.getNumChannels(), preparedChannels);
    const auto samples = juce::jmin(buffer.getNumSamples(), preparedBlockSize);
    for (auto channel = 0; channel < channels; ++channel)
    {
        hardwareBuffer.copyFrom(channel, 0, buffer, channel, 0, samples);
        studioBuffer.copyFrom(channel, 0, buffer, channel, 0, samples);
    }

    delayBlock(hardwareBuffer, hardwareDelay, samples);

    const auto needsStudioPath = studioMode || studioMix.isSmoothing()
                              || studioMix.getCurrentValue() > 0.0001f;
    if (needsStudioPath)
        processStudioColour(studioBuffer);

    for (auto sample = 0; sample < samples; ++sample)
    {
        const auto mix = studioMix.getNextValue();
        for (auto channel = 0; channel < channels; ++channel)
        {
            const auto fixed = hardwareBuffer.getSample(channel, sample);
            const auto studio = needsStudioPath ? studioBuffer.getSample(channel, sample) : fixed;
            buffer.setSample(channel, sample, fixed + (studio - fixed) * mix);
        }
    }
}

void OversamplingChain::processDryBlock(juce::AudioBuffer<float>& buffer)
{
    if (latencySamples > 0)
        delayBlock(buffer, bypassDelay, buffer.getNumSamples());
}

void OversamplingChain::delayBlock(juce::AudioBuffer<float>& buffer,
                                   juce::dsp::DelayLine<float>& delay,
                                   int numSamples)
{
    numSamples = juce::jmin(numSamples, buffer.getNumSamples());
    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* samples = buffer.getWritePointer(channel);
        for (auto sample = 0; sample < numSamples; ++sample)
        {
            delay.pushSample(channel, samples[sample]);
            samples[sample] = delay.popSample(channel);
        }
    }
}

void OversamplingChain::processStudioColour(juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = static_cast<size_t>(
        juce::jmin(buffer.getNumSamples(), preparedBlockSize));
    juce::dsp::AudioBlock<const float> inputBlock(buffer);
    auto oversampled = oversampler->processSamplesUp(inputBlock.getSubBlock(0, numSamples));

    constexpr auto drive = 0.60f;
    constexpr auto makeup = 1.01f / drive;
    for (size_t channel = 0; channel < oversampled.getNumChannels(); ++channel)
    {
        auto* samples = oversampled.getChannelPointer(channel);
        for (size_t sample = 0; sample < oversampled.getNumSamples(); ++sample)
            samples[sample] = std::tanh(samples[sample] * drive) * makeup;
    }

    juce::dsp::AudioBlock<float> outputBlock(buffer);
    auto destination = outputBlock.getSubBlock(0, numSamples);
    oversampler->processSamplesDown(destination);
}
}
