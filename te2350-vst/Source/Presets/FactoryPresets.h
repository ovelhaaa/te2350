#pragma once

#include <JuceHeader.h>

namespace te2350
{
juce::StringArray getFactoryPresetNames();
void applyFactoryPreset(juce::AudioProcessorValueTreeState& state, int index);
}
