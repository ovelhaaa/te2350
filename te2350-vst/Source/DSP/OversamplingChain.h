#pragma once

#include <JuceHeader.h>

namespace te2350
{
class OversamplingChain
{
public:
    void prepare(double sampleRate, int maximumBlockSize, int numChannels);
    void setStudioMode(bool shouldUseStudioMode, juce::AudioProcessor& processor);
    bool isStudioMode() const noexcept { return studioMode; }
    int getLatencySamples() const noexcept { return latencySamples; }

private:
    bool studioMode = false;
    int latencySamples = 0;
};
}
