#pragma once

#include <JuceHeader.h>
#include <vector>

namespace te2350
{
struct PresetDescriptor
{
    juce::String name;
    juce::String category;
    juce::String description;
};

const std::vector<PresetDescriptor>& getFactoryPresetDescriptors();
juce::StringArray getFactoryPresetNames();
void applyFactoryPreset(juce::AudioProcessorValueTreeState& state, int index);
}
