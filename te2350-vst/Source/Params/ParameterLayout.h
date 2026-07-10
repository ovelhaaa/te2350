#pragma once

#include <JuceHeader.h>
#include <vector>

namespace te2350
{
struct ParameterSpec
{
    juce::String id;
    float minimum = 0.0f;
    float maximum = 1.0f;
    float defaultValue = 0.0f;
};

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

const std::vector<ParameterSpec>& getParameterSpecs();
const ParameterSpec* findParameterSpec(juce::StringRef parameterID);
float getParameterDefault(juce::StringRef parameterID);
}
