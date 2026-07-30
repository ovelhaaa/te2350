#pragma once

#include <JuceHeader.h>

#include <vector>

namespace te2350
{
struct UserPresetInfo
{
    juce::String name;
    juce::File file;
};

class UserPresetManager
{
public:
    explicit UserPresetManager(juce::AudioProcessorValueTreeState& stateToUse,
                               juce::File presetDirectory = {});

    const std::vector<UserPresetInfo>& refresh();
    const std::vector<UserPresetInfo>& getPresets() const noexcept { return presets; }
    const juce::File& getDirectory() const noexcept { return directory; }

    juce::Result save(const juce::String& name);
    juce::Result load(int index);
    juce::Result remove(int index);

    static juce::Result applyParameterState(
        juce::AudioProcessorValueTreeState& state,
        const juce::ValueTree& parameterState);

private:
    static constexpr int presetFormatVersion = 1;
    static juce::File defaultPresetDirectory();
    static juce::ValueTree readPresetDocument(const juce::File& file);

    juce::AudioProcessorValueTreeState& state;
    juce::File directory;
    std::vector<UserPresetInfo> presets;
};
}
