// Frozen M10 catalog: captured before M11; do not regenerate.
#pragma once
#include "Presets/FactoryPresets.h"

#include <array>
#include <map>
#include <string>

namespace m10Snapshot
{ using PresetDescriptor = te2350::PresetDescriptor; }
namespace m10Snapshot
{
namespace
{
struct Preset
{
    const char* name;
    const char* category;
    const char* description;
    std::map<std::string, float> values;
};

// M10: voicing only. Keep the catalog/order and global parameter defaults intact.
const std::array<Preset, 13> presets {{
    { "Low Orbit", "FOUNDATIONS", "Dark, focused short ambience with subtle drift.",
      { { "space", 0.35f }, { "wild", 0.02f }, { "bloom", 0.18f }, { "timeMs", 230.0f }, { "feedback", 0.34f },
        { "mix", 0.3f }, { "highCutHz", 4800.0f }, { "lowCutHz", 100.0f }, { "diffusion", 0.35f }, { "wetWidth", 0.28f },
        { "shimmerAmount", 0.0f }, { "shimmerFeedback", 0.0f }, { "shimmerInterval", 2.0f }, { "duckAmount", 0.08f }, { "modRateHz", 0.12f },
        { "modDepth", 0.03f }, { "chaos", 0.0f }, { "wobble", 0.04f }, { "presence", 0.42f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 0.0f }, { "outputTrim", 0.0f } } },
    { "Tidal Lock", "DEEP SPACE", "Deep, wide ducked echoes with space behind the attack.",
      { { "space", 0.56f }, { "wild", 0.1f }, { "bloom", 0.48f }, { "timeMs", 650.0f }, { "feedback", 0.67f },
        { "mix", 0.36f }, { "highCutHz", 6800.0f }, { "lowCutHz", 135.0f }, { "diffusion", 0.6f }, { "wetWidth", 0.78f },
        { "shimmerAmount", 0.0f }, { "shimmerFeedback", 0.0f }, { "shimmerInterval", 2.0f }, { "duckAmount", 0.26f }, { "modRateHz", 0.12f },
        { "modDepth", 0.1f }, { "chaos", 0.01f }, { "wobble", 0.12f }, { "presence", 0.4f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 0.0f }, { "outputTrim", 0.0f } } },
    { "Trade Winds", "MOTION", "Airy chorus-like stereo movement with medium repeats.",
      { { "space", 0.36f }, { "wild", 0.3f }, { "bloom", 0.26f }, { "timeMs", 420.0f }, { "feedback", 0.46f },
        { "mix", 0.32f }, { "highCutHz", 12000.0f }, { "lowCutHz", 95.0f }, { "diffusion", 0.42f }, { "wetWidth", 0.64f },
        { "shimmerAmount", 0.0f }, { "shimmerFeedback", 0.0f }, { "shimmerInterval", 2.0f }, { "duckAmount", 0.1f }, { "modRateHz", 0.64f },
        { "modDepth", 0.4f }, { "chaos", 0.02f }, { "wobble", 0.28f }, { "presence", 0.62f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 0.0f }, { "outputTrim", 0.0f } } },
    { "Solar Wind", "SHIMMER", "Bright, chaotic fifth shimmer with low regeneration.",
      { { "space", 0.58f }, { "wild", 0.58f }, { "bloom", 0.46f }, { "timeMs", 680.0f }, { "feedback", 0.64f },
        { "mix", 0.33f }, { "highCutHz", 14500.0f }, { "lowCutHz", 180.0f }, { "diffusion", 0.48f }, { "wetWidth", 0.78f },
        { "shimmerAmount", 0.21f }, { "shimmerFeedback", 0.23f }, { "shimmerInterval", 1.0f }, { "duckAmount", 0.18f }, { "modRateHz", 0.45f },
        { "modDepth", 0.5f }, { "chaos", 0.32f }, { "wobble", 0.46f }, { "presence", 0.68f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 0.0f }, { "outputTrim", 0.0f } } },
    { "Escape Velocity", "MOTION", "Fast expanding animated space that preserves the attack.",
      { { "space", 0.44f }, { "wild", 0.65f }, { "bloom", 0.38f }, { "timeMs", 330.0f }, { "feedback", 0.6f },
        { "mix", 0.31f }, { "highCutHz", 11500.0f }, { "lowCutHz", 155.0f }, { "diffusion", 0.57f }, { "wetWidth", 0.72f },
        { "shimmerAmount", 0.0f }, { "shimmerFeedback", 0.0f }, { "shimmerInterval", 2.0f }, { "duckAmount", 0.32f }, { "modRateHz", 0.75f },
        { "modDepth", 0.56f }, { "chaos", 0.28f }, { "wobble", 0.35f }, { "presence", 0.58f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 1.0f }, { "outputTrim", 0.0f } } },
    { "Zero-G", "DEEP SPACE", "Soft, floating Atmos field with slow drift and no shimmer regeneration.",
      { { "space", 0.72f }, { "wild", 0.06f }, { "bloom", 0.68f }, { "timeMs", 1000.0f }, { "feedback", 0.7f },
        { "mix", 0.34f }, { "highCutHz", 10800.0f }, { "lowCutHz", 95.0f }, { "diffusion", 0.82f }, { "wetWidth", 0.86f },
        { "shimmerAmount", 0.0f }, { "shimmerFeedback", 0.0f }, { "shimmerInterval", 2.0f }, { "duckAmount", 0.1f }, { "modRateHz", 0.07f },
        { "modDepth", 0.08f }, { "chaos", 0.0f }, { "wobble", 0.06f }, { "presence", 0.46f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 1.0f }, { "outputTrim", 0.0f } } },
    { "Event Horizon", "EXPERIMENTAL", "Huge dark near-infinite Atmos orbit with restrained movement.",
      { { "space", 0.78f }, { "wild", 0.32f }, { "bloom", 0.42f }, { "timeMs", 1450.0f }, { "feedback", 0.89f },
        { "mix", 0.34f }, { "highCutHz", 1000.0f }, { "lowCutHz", 60.0f }, { "diffusion", 0.76f }, { "wetWidth", 0.83f },
        { "shimmerAmount", 0.0f }, { "shimmerFeedback", 0.0f }, { "shimmerInterval", 0.0f }, { "duckAmount", 0.16f }, { "modRateHz", 0.07f },
        { "modDepth", 0.2f }, { "chaos", 0.1f }, { "wobble", 0.26f }, { "presence", 0.18f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 1.0f }, { "outputTrim", 0.0f } } },
    { "Glass Transit", "SHIMMER", "Transparent octave-up shimmer with clean bright reflections.",
      { { "space", 0.46f }, { "wild", 0.04f }, { "bloom", 0.36f }, { "timeMs", 510.0f }, { "feedback", 0.52f },
        { "mix", 0.34f }, { "highCutHz", 15500.0f }, { "lowCutHz", 180.0f }, { "diffusion", 0.52f }, { "wetWidth", 0.87f },
        { "shimmerAmount", 0.3f }, { "shimmerFeedback", 0.34f }, { "shimmerInterval", 2.0f }, { "duckAmount", 0.14f }, { "modRateHz", 0.18f },
        { "modDepth", 0.05f }, { "chaos", 0.0f }, { "wobble", 0.04f }, { "presence", 0.74f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 0.0f }, { "outputTrim", 0.0f } } },
    { "Cassette Moon", "MOTION", "Warm narrow tape-like echoes with unstable slow drift.",
      { { "space", 0.32f }, { "wild", 0.46f }, { "bloom", 0.2f }, { "timeMs", 375.0f }, { "feedback", 0.44f },
        { "mix", 0.31f }, { "highCutHz", 3400.0f }, { "lowCutHz", 100.0f }, { "diffusion", 0.23f }, { "wetWidth", 0.48f },
        { "shimmerAmount", 0.0f }, { "shimmerFeedback", 0.0f }, { "shimmerInterval", 0.0f }, { "duckAmount", 0.12f }, { "modRateHz", 0.1f },
        { "modDepth", 0.4f }, { "chaos", 0.08f }, { "wobble", 0.52f }, { "presence", 0.3f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 0.0f }, { "outputTrim", 0.0f } } },
    { "Pulsar Eighths", "RHYTHMIC", "Clear tempo-locked eighth-note echoes with focused repeat identity.",
      { { "space", 0.18f }, { "wild", 0.08f }, { "bloom", 0.14f }, { "timeMs", 250.0f }, { "feedback", 0.58f },
        { "mix", 0.33f }, { "highCutHz", 10000.0f }, { "lowCutHz", 150.0f }, { "diffusion", 0.06f }, { "wetWidth", 0.52f },
        { "shimmerAmount", 0.0f }, { "shimmerFeedback", 0.0f }, { "shimmerInterval", 2.0f }, { "duckAmount", 0.27f }, { "modRateHz", 0.3f },
        { "modDepth", 0.05f }, { "chaos", 0.0f }, { "wobble", 0.06f }, { "presence", 0.68f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 0.0f }, { "outputTrim", 0.0f }, { "syncMode", 2.0f } } },
    { "Frozen Choir", "SHIMMER", "Sustained fifth-shimmer choir ready for manual Freeze performance.",
      { { "space", 0.68f }, { "wild", 0.1f }, { "bloom", 0.78f }, { "timeMs", 1050.0f }, { "feedback", 0.77f },
        { "mix", 0.35f }, { "highCutHz", 11500.0f }, { "lowCutHz", 220.0f }, { "diffusion", 0.86f }, { "wetWidth", 0.76f },
        { "shimmerAmount", 0.44f }, { "shimmerFeedback", 0.62f }, { "shimmerInterval", 1.0f }, { "duckAmount", 0.18f }, { "modRateHz", 0.09f },
        { "modDepth", 0.14f }, { "chaos", 0.0f }, { "wobble", 0.08f }, { "presence", 0.6f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 1.0f }, { "outputTrim", 0.0f } } },
    { "Dark Matter", "DEEP SPACE", "Dense, shadowed downward-octave Atmos with restrained movement.",
      { { "space", 0.7f }, { "wild", 0.04f }, { "bloom", 0.3f }, { "timeMs", 1150.0f }, { "feedback", 0.77f },
        { "mix", 0.34f }, { "highCutHz", 1500.0f }, { "lowCutHz", 60.0f }, { "diffusion", 0.84f }, { "wetWidth", 0.4f },
        { "shimmerAmount", 0.13f }, { "shimmerFeedback", 0.22f }, { "shimmerInterval", 0.0f }, { "duckAmount", 0.1f }, { "modRateHz", 0.045f },
        { "modDepth", 0.04f }, { "chaos", 0.0f }, { "wobble", 0.03f }, { "presence", 0.12f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 1.0f }, { "outputTrim", 0.0f } } },
    { "Microgravity Slap", "RHYTHMIC", "Short focused slap for percussive definition.",
      { { "space", 0.07f }, { "wild", 0.03f }, { "bloom", 0.04f }, { "timeMs", 75.0f }, { "feedback", 0.24f },
        { "mix", 0.25f }, { "highCutHz", 12500.0f }, { "lowCutHz", 160.0f }, { "diffusion", 0.1f }, { "wetWidth", 0.2f },
        { "shimmerAmount", 0.0f }, { "shimmerFeedback", 0.0f }, { "shimmerInterval", 2.0f }, { "duckAmount", 0.04f }, { "modRateHz", 0.4f },
        { "modDepth", 0.0f }, { "chaos", 0.0f }, { "wobble", 0.02f }, { "presence", 0.72f }, { "freezeEngage", 0.0f },
        { "atmosFdnOn", 0.0f }, { "outputTrim", 0.0f } } }
}};
}

const std::vector<PresetDescriptor>& getFactoryPresetDescriptors()
{
    static const auto descriptors = []
    {
        std::vector<PresetDescriptor> result;
        result.reserve(presets.size());
        for (const auto& preset : presets)
            result.push_back({ preset.name, preset.category, preset.description });
        return result;
    }();

    return descriptors;
}

juce::StringArray getFactoryPresetNames()
{
    juce::StringArray names;
    for (const auto& descriptor : getFactoryPresetDescriptors())
        names.add(descriptor.name);

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

        // Capture M10 values for every parameter omitted from the sparse catalog.
        static const std::map<std::string, float> omittedDefaults {
            { "syncMode", 0.0f }, { "killDry", 0.0f }, { "modShape", 1.0f },
            { "duckThreshold", -24.0f }, { "inputTrim", 0.0f }, { "bypass", 0.0f },
            { "qualityMode", 0.0f }, { "freezeMode", 1.0f }
        };
        const auto key = identified->paramID.toStdString();
        auto plainValue = values.count(key) ? values.at(key) : omittedDefaults.at(key);
        if (const auto found = values.find(identified->paramID.toStdString()); found != values.end())
            plainValue = found->second;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(ranged->convertTo0to1(plainValue));
        parameter->endChangeGesture();
    }
}
}
