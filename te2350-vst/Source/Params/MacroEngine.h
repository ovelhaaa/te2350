#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <vector>

namespace te2350
{
class MacroEngine
{
public:
    enum class Curve
    {
        Linear,
        Log,
        Exponential
    };

    struct MacroTarget
    {
        juce::String paramID;
        float valueAt0 = 0.0f;
        float valueAt100 = 1.0f;
        Curve curve = Curve::Linear;
    };

    struct MacroDefinition
    {
        juce::String macroID;
        std::vector<MacroTarget> targets;
    };

    MacroEngine();

    void prepare(double newSampleRate, int newControlBlockSize);
    void reset();
    void update(const juce::AudioProcessorValueTreeState& state, int numSamples);

    float getEffectiveValue(juce::StringRef parameterID, float fallback) const;
    float getMacroValue(juce::StringRef macroID) const;
    float getInstability() const;

    static std::vector<MacroDefinition> createFactoryDefinitions();

private:
    using ValueMap = std::unordered_map<std::string, float>;

    static float mapTarget(const MacroTarget& target, float macroValue);
    static float clampForParameter(juce::StringRef parameterID, float value);
    static float readStateValue(const juce::AudioProcessorValueTreeState& state, juce::StringRef parameterID, float fallback);

    void calculateTargets(const juce::AudioProcessorValueTreeState& state);

    std::vector<MacroDefinition> definitions;
    ValueMap currentValues;
    ValueMap targetValues;
    ValueMap macroValues;
    double sampleRate = 48000.0;
    int controlBlockSize = 64;
    int samplesUntilNextControlUpdate = 0;
};
}
