#include "OversamplingChain.h"

namespace te2350
{
void OversamplingChain::prepare(double, int, int)
{
}

void OversamplingChain::setStudioMode(bool shouldUseStudioMode, juce::AudioProcessor& processor)
{
    studioMode = shouldUseStudioMode;
    latencySamples = 0;
    processor.setLatencySamples(latencySamples);
}
}
