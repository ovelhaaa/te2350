#include "FactoryPresets.h"

#include <array>
#include <map>
#include <string>

namespace te2350
{
namespace
{
struct Preset
{
    const char* name;
    std::map<std::string, float> values;
};

const std::array<Preset, 7> presets {{
    { "Low Orbit", { { "space", 0.35f }, { "wild", 0.05f }, { "bloom", 0.20f }, { "timeMs", 360.0f }, { "feedback", 0.42f }, { "mix", 0.32f }, { "diffusion", 0.48f }, { "highCutHz", 8200.0f } } },
    { "Tidal Lock", { { "space", 0.58f }, { "wild", 0.12f }, { "bloom", 0.44f }, { "timeMs", 720.0f }, { "feedback", 0.62f }, { "mix", 0.46f }, { "diffusion", 0.72f }, { "duckAmount", 0.24f }, { "outputTrim", 1.25f } } },
    { "Trade Winds", { { "space", 0.42f }, { "wild", 0.28f }, { "bloom", 0.30f }, { "timeMs", 540.0f }, { "wobble", 0.32f }, { "modRateHz", 0.32f }, { "modDepth", 0.28f }, { "modShape", 0.0f }, { "wetWidth", 0.74f } } },
    { "Solar Wind", { { "space", 0.66f }, { "wild", 0.34f }, { "bloom", 0.55f }, { "timeMs", 900.0f }, { "feedback", 0.72f }, { "shimmerInterval", 1.0f }, { "shimmerAmount", 0.18f }, { "shimmerFeedback", 0.42f }, { "presence", 0.62f }, { "outputTrim", 0.40f } } },
    { "Escape Velocity", { { "space", 0.76f }, { "wild", 0.62f }, { "bloom", 0.48f }, { "timeMs", 1120.0f }, { "feedback", 0.78f }, { "chaos", 0.36f }, { "wobble", 0.48f }, { "atmosFdnOn", 1.0f }, { "outputTrim", 0.50f } } },
    { "Zero-G", { { "space", 0.88f }, { "wild", 0.18f }, { "bloom", 0.74f }, { "timeMs", 1500.0f }, { "feedback", 0.86f }, { "mix", 0.58f }, { "diffusion", 0.86f }, { "highCutHz", 10800.0f }, { "atmosFdnOn", 1.0f }, { "outputTrim", 2.25f } } },
    { "Event Horizon", { { "space", 1.0f }, { "wild", 0.92f }, { "bloom", 0.82f }, { "timeMs", 1900.0f }, { "feedback", 0.95f }, { "mix", 0.70f }, { "chaos", 0.72f }, { "wobble", 0.82f }, { "modShape", 2.0f }, { "shimmerAmount", 0.34f }, { "atmosFdnOn", 1.0f }, { "outputTrim", 3.25f } } }
}};
}

juce::StringArray getFactoryPresetNames()
{
    juce::StringArray names;
    for (const auto& preset : presets)
        names.add(preset.name);

    return names;
}

void applyFactoryPreset(juce::AudioProcessorValueTreeState& state, int index)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(presets.size())))
        return;

    const auto& values = presets[static_cast<size_t>(index)].values;

    for (auto* parameter : state.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        if (ranged == nullptr || identified == nullptr)
            continue;

        auto plainValue = ranged->convertFrom0to1(ranged->getDefaultValue());
        if (const auto found = values.find(identified->paramID.toStdString()); found != values.end())
            plainValue = found->second;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(ranged->convertTo0to1(plainValue));
        parameter->endChangeGesture();
    }
}
}
