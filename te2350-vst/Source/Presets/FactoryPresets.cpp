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
    const char* category;
    const char* description;
    std::map<std::string, float> values;
};

const std::array<Preset, 13> presets {{
    { "Low Orbit", "FOUNDATIONS", "Balanced short ambience for an immediate starting point.",
      { { "space", 0.35f }, { "wild", 0.05f }, { "bloom", 0.20f }, { "timeMs", 360.0f }, { "feedback", 0.42f }, { "mix", 0.32f }, { "diffusion", 0.48f }, { "highCutHz", 8200.0f } } },
    { "Tidal Lock", "DEEP SPACE", "Wide, ducked repeats that open behind the source.",
      { { "space", 0.58f }, { "wild", 0.12f }, { "bloom", 0.44f }, { "timeMs", 720.0f }, { "feedback", 0.62f }, { "mix", 0.46f }, { "diffusion", 0.72f }, { "duckAmount", 0.24f }, { "outputTrim", 1.25f } } },
    { "Trade Winds", "MOTION", "Animated stereo drift with a stable, musical centre.",
      { { "space", 0.42f }, { "wild", 0.28f }, { "bloom", 0.30f }, { "timeMs", 540.0f }, { "wobble", 0.32f }, { "modRateHz", 0.32f }, { "modDepth", 0.28f }, { "modShape", 0.0f }, { "wetWidth", 0.74f } } },
    { "Solar Wind", "SHIMMER", "Bright fifth-up halo with controlled recirculation.",
      { { "space", 0.66f }, { "wild", 0.34f }, { "bloom", 0.55f }, { "timeMs", 900.0f }, { "feedback", 0.72f }, { "shimmerInterval", 1.0f }, { "shimmerAmount", 0.18f }, { "shimmerFeedback", 0.42f }, { "presence", 0.62f }, { "outputTrim", 0.40f } } },
    { "Escape Velocity", "MOTION", "Long unstable orbit with Atmos reinforcement.",
      { { "space", 0.76f }, { "wild", 0.62f }, { "bloom", 0.48f }, { "timeMs", 1120.0f }, { "feedback", 0.78f }, { "chaos", 0.36f }, { "wobble", 0.48f }, { "atmosFdnOn", 1.0f }, { "outputTrim", 0.50f } } },
    { "Zero-G", "DEEP SPACE", "Expansive diffuse field with a very long luminous decay.",
      { { "space", 0.88f }, { "wild", 0.18f }, { "bloom", 0.74f }, { "timeMs", 1500.0f }, { "feedback", 0.86f }, { "mix", 0.58f }, { "diffusion", 0.86f }, { "highCutHz", 10800.0f }, { "atmosFdnOn", 1.0f }, { "outputTrim", 2.25f } } },
    { "Event Horizon", "EXPERIMENTAL", "Near-limit feedback and sample-and-hold movement.",
      { { "space", 1.0f }, { "wild", 0.92f }, { "bloom", 0.82f }, { "timeMs", 1900.0f }, { "feedback", 0.95f }, { "mix", 0.70f }, { "chaos", 0.72f }, { "wobble", 0.82f }, { "modShape", 2.0f }, { "shimmerAmount", 0.34f }, { "atmosFdnOn", 1.0f }, { "outputTrim", 3.25f } } },
    { "Glass Transit", "SHIMMER", "Clear octave-up reflections with an airy stereo image.",
      { { "space", 0.54f }, { "wild", 0.08f }, { "bloom", 0.46f }, { "timeMs", 640.0f }, { "feedback", 0.57f }, { "mix", 0.42f }, { "diffusion", 0.68f }, { "highCutHz", 13000.0f }, { "presence", 0.68f }, { "shimmerInterval", 2.0f }, { "shimmerAmount", 0.26f }, { "shimmerFeedback", 0.38f }, { "wetWidth", 0.92f }, { "outputTrim", 0.50f } } },
    { "Cassette Moon", "MOTION", "Dark tape-like drift with slow random-walk modulation.",
      { { "space", 0.38f }, { "wild", 0.46f }, { "bloom", 0.24f }, { "timeMs", 460.0f }, { "feedback", 0.52f }, { "mix", 0.40f }, { "lowCutHz", 110.0f }, { "highCutHz", 5800.0f }, { "diffusion", 0.32f }, { "chaos", 0.18f }, { "wobble", 0.62f }, { "presence", 0.38f }, { "modRateHz", 0.20f }, { "modDepth", 0.46f }, { "modShape", 1.0f }, { "wetWidth", 0.70f }, { "outputTrim", 1.0f } } },
    { "Pulsar Eighths", "RHYTHMIC", "Tempo-locked eighth-note echoes with assertive ducking.",
      { { "space", 0.28f }, { "wild", 0.24f }, { "bloom", 0.18f }, { "syncMode", 2.0f }, { "feedback", 0.64f }, { "mix", 0.44f }, { "diffusion", 0.24f }, { "chaos", 0.12f }, { "wobble", 0.18f }, { "modRateHz", 0.50f }, { "modDepth", 0.34f }, { "modShape", 2.0f }, { "duckThreshold", -28.0f }, { "duckAmount", 0.42f }, { "wetWidth", 0.82f }, { "outputTrim", 1.0f } } },
    { "Frozen Choir", "SHIMMER", "Dense fifth-shimmer cloud designed for Freeze performance.",
      { { "space", 0.82f }, { "wild", 0.10f }, { "bloom", 0.86f }, { "timeMs", 1350.0f }, { "feedback", 0.82f }, { "mix", 0.64f }, { "diffusion", 0.92f }, { "highCutHz", 12000.0f }, { "presence", 0.70f }, { "shimmerInterval", 1.0f }, { "shimmerAmount", 0.38f }, { "shimmerFeedback", 0.55f }, { "atmosFdnOn", 1.0f }, { "wetWidth", 1.0f }, { "outputTrim", 1.50f } } },
    { "Dark Matter", "DEEP SPACE", "Low, shadowed Atmos tail with downward octave colour.",
      { { "space", 0.90f }, { "wild", 0.56f }, { "bloom", 0.70f }, { "timeMs", 1700.0f }, { "feedback", 0.88f }, { "mix", 0.62f }, { "lowCutHz", 160.0f }, { "highCutHz", 4200.0f }, { "diffusion", 0.80f }, { "chaos", 0.48f }, { "wobble", 0.60f }, { "presence", 0.22f }, { "modRateHz", 0.09f }, { "modDepth", 0.52f }, { "shimmerInterval", 0.0f }, { "shimmerAmount", 0.12f }, { "shimmerFeedback", 0.25f }, { "atmosFdnOn", 1.0f }, { "wetWidth", 0.88f }, { "outputTrim", 2.0f } } },
    { "Microgravity Slap", "RHYTHMIC", "Compact slap ambience for rhythmic definition and width.",
      { { "space", 0.12f }, { "wild", 0.08f }, { "bloom", 0.06f }, { "timeMs", 95.0f }, { "feedback", 0.32f }, { "mix", 0.28f }, { "lowCutHz", 140.0f }, { "highCutHz", 10500.0f }, { "diffusion", 0.18f }, { "presence", 0.70f }, { "modRateHz", 0.40f }, { "modDepth", 0.08f }, { "modShape", 0.0f }, { "wetWidth", 0.55f } } }
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

        auto plainValue = ranged->convertFrom0to1(ranged->getDefaultValue());
        if (const auto found = values.find(identified->paramID.toStdString()); found != values.end())
            plainValue = found->second;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(ranged->convertTo0to1(plainValue));
        parameter->endChangeGesture();
    }
}
}
