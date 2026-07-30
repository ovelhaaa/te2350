#pragma once

#include <JuceHeader.h>

namespace te2350
{
class OversamplingChain
{
public:
    void prepare(double sampleRate, int maximumBlockSize, int numChannels);
    void reset();
    void setStudioMode(bool shouldUseStudioMode);
    void processEffectBlock(juce::AudioBuffer<float>& buffer);
    void processDryBlock(juce::AudioBuffer<float>& buffer);
    bool isStudioMode() const noexcept { return studioMode; }
    int getLatencySamples() const noexcept { return latencySamples; }

private:
    static void delayBlock(juce::AudioBuffer<float>& buffer,
                           juce::dsp::DelayLine<float>& delay,
                           int numSamples);
    void processStudioColour(juce::AudioBuffer<float>& buffer);

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::DelayLine<float> hardwareDelay;
    juce::dsp::DelayLine<float> bypassDelay;
    juce::AudioBuffer<float> hardwareBuffer;
    juce::AudioBuffer<float> studioBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> studioMix;
    bool studioMode = false;
    int latencySamples = 0;
    int preparedChannels = 0;
    int preparedBlockSize = 0;
};
}
