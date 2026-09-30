#include "MacroEngine.h"
#include "ParameterLayout.h"

#include <cmath>

namespace te2350
{
MacroEngine::MacroEngine()
    : definitions(createFactoryDefinitions())
{
    const auto& specs = getParameterSpecs();
    values.reserve(specs.size());
    for (const auto& spec : specs)
        values.push_back({ spec.id, spec.defaultValue, spec.defaultValue, spec.defaultValue });
}

void MacroEngine::prepare(double newSampleRate, int newControlBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    controlBlockSize = juce::jmax(16, newControlBlockSize);
    samplesUntilNextControlUpdate = 0;
    reset();
}

void MacroEngine::reset()
{
    for (auto& value : values)
    {
        value.current = value.defaultValue;
        value.target = value.defaultValue;
    }

    spaceMacro = getParameterDefault("space");
    wildMacro = getParameterDefault("wild");
    bloomMacro = getParameterDefault("bloom");
}

void MacroEngine::update(const juce::AudioProcessorValueTreeState& state, int numSamples)
{
    samplesUntilNextControlUpdate -= numSamples;

    if (samplesUntilNextControlUpdate <= 0)
    {
        calculateTargets(state);
        samplesUntilNextControlUpdate = controlBlockSize;
    }

    const auto smoothing = static_cast<float>(1.0 - std::exp(-static_cast<double>(juce::jmax(1, numSamples)) / (0.008 * sampleRate)));

    for (auto& value : values)
        value.current += (value.target - value.current) * smoothing;
}

float MacroEngine::getEffectiveValue(juce::StringRef parameterID, float fallback) const
{
    if (const auto* value = findValue(parameterID))
        return value->current;

    return fallback;
}

float MacroEngine::getMacroValue(juce::StringRef macroID) const
{
    if (macroID == juce::StringRef("space"))
        return spaceMacro;
    if (macroID == juce::StringRef("wild"))
        return wildMacro;
    if (macroID == juce::StringRef("bloom"))
        return bloomMacro;

    return 0.0f;
}

float MacroEngine::getInstability() const
{
    const auto wild = getMacroValue("wild");
    const auto chaos = getEffectiveValue("chaos", 0.0f);
    const auto wobble = getEffectiveValue("wobble", 0.0f);
    return juce::jlimit(0.0f, 1.0f, wild * 0.45f + chaos * 0.35f + wobble * 0.20f);
}

std::vector<MacroEngine::MacroDefinition> MacroEngine::createFactoryDefinitions()
{
    return {
        {
            "space",
            {
                { "timeMs", 420.0f, 1080.0f, Curve::Log },
                { "lowCutHz", 80.0f, 140.0f, Curve::Log },
                { "highCutHz", 9000.0f, 5200.0f, Curve::Log },
                { "diffusion", 0.40f, 0.78f, Curve::Linear },
                { "shimmerAmount", 0.0f, 0.32f, Curve::Linear },
                { "wetWidth", 0.60f, 0.95f, Curve::Linear }
            }
        },
        {
            "wild",
            {
                { "feedback", 0.45f, 1.05f, Curve::Linear },
                { "chaos", 0.0f, 0.85f, Curve::Exponential },
                { "wobble", 0.10f, 0.85f, Curve::Exponential },
                { "modRateHz", 0.15f, 1.20f, Curve::Log },
                { "modDepth", 0.10f, 0.85f, Curve::Exponential }
            }
        },
        {
            "bloom",
            {
                { "highCutHz", 9000.0f, 14000.0f, Curve::Log },
                { "duckAmount", 0.10f, 0.55f, Curve::Exponential },
                { "shimmerAmount", 0.0f, 0.18f, Curve::Linear },
                { "mix", 0.35f, 0.52f, Curve::Linear }
            }
        }
    };
}

float MacroEngine::mapTarget(const MacroTarget& target, float macroValue)
{
    const auto amount = juce::jlimit(0.0f, 1.0f, macroValue);

    switch (target.curve)
    {
        case Curve::Log:
            if (target.valueAt0 > 0.0f && target.valueAt100 > 0.0f)
                return target.valueAt0 * std::pow(target.valueAt100 / target.valueAt0, amount);
            break;

        case Curve::Exponential:
            return target.valueAt0 + (target.valueAt100 - target.valueAt0) * amount * amount;

        case Curve::Linear:
            break;
    }

    return target.valueAt0 + (target.valueAt100 - target.valueAt0) * amount;
}

float MacroEngine::clampForParameter(juce::StringRef parameterID, float value)
{
    if (const auto* spec = findParameterSpec(parameterID))
        return juce::jlimit(spec->minimum, spec->maximum, value);

    return value;
}

float MacroEngine::readStateValue(const juce::AudioProcessorValueTreeState& state,
                                  juce::StringRef parameterID,
                                  float fallback)
{
    if (const auto* raw = state.getRawParameterValue(parameterID))
        return raw->load();

    return fallback;
}

MacroEngine::ParameterValue* MacroEngine::findValue(juce::StringRef parameterID)
{
    for (auto& value : values)
        if (value.id == parameterID)
            return &value;

    return nullptr;
}

const MacroEngine::ParameterValue* MacroEngine::findValue(juce::StringRef parameterID) const
{
    for (const auto& value : values)
        if (value.id == parameterID)
            return &value;

    return nullptr;
}

void MacroEngine::calculateTargets(const juce::AudioProcessorValueTreeState& state)
{
    spaceMacro = readStateValue(state, "space", 0.0f);
    wildMacro = readStateValue(state, "wild", 0.0f);
    bloomMacro = readStateValue(state, "bloom", 0.0f);

    for (auto& value : values)
        value.target = readStateValue(state, value.id, value.defaultValue);

    for (const auto& definition : definitions)
    {
        const auto macroValue = getMacroValue(definition.macroID);

        for (const auto& target : definition.targets)
        {
            const auto mapped = mapTarget(target, macroValue);
            const auto offset = mapped - target.valueAt0;
            if (auto* value = findValue(target.paramID))
            {
                // Bloom uses remaining duck headroom so 0..100% stays useful.
                const auto contribution = target.paramID == juce::StringRef("duckAmount")
                    ? offset * (1.0f - value->target) : offset;
                value->target = clampForParameter(target.paramID, value->target + contribution);
            }
        }
    }

    const auto wild = getMacroValue("wild");
    const auto wildFeedbackCeiling = 0.95f + wild * 0.10f;
    if (auto* feedback = findValue("feedback"))
        feedback->target = juce::jmin(feedback->target, wildFeedbackCeiling);
}
}
