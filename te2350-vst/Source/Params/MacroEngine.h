#pragma once

#include <JuceHeader.h>
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
    struct ParameterValue
    {
        juce::String id;
        float defaultValue = 0.0f;
        float current = 0.0f;
        float target = 0.0f;
    };

    static float mapTarget(const MacroTarget& target, float macroValue);
    static float clampForParameter(juce::StringRef parameterID, float value);
    static float readStateValue(const juce::AudioProcessorValueTreeState& state, juce::StringRef parameterID, float fallback);

    ParameterValue* findValue(juce::StringRef parameterID);
    const ParameterValue* findValue(juce::StringRef parameterID) const;
    void calculateTargets(const juce::AudioProcessorValueTreeState& state);

    std::vector<MacroDefinition> definitions;
    std::vector<ParameterValue> values;
    float spaceMacro = 0.0f;
    float wildMacro = 0.0f;
    float bloomMacro = 0.0f;
    double sampleRate = 48000.0;
    int controlBlockSize = 64;
    int samplesUntilNextControlUpdate = 0;
};
}
