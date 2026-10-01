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
                { "timeMs", 420.0f, 1080.0f, Curve::Log, 1.0f / 1580.0f },
                { "lowCutHz", 80.0f, 140.0f, Curve::Log, 1.0f / 920.0f },
                { "highCutHz", 9000.0f, 5200.0f, Curve::Log, 1.0f / 8000.0f },
                { "diffusion", 0.40f, 0.70f, Curve::Linear, 1.0f / 0.60f },
                { "shimmerAmount", 0.0f, 0.08f, Curve::Linear, 1.0f },
                { "wetWidth", 0.60f, 0.82f, Curve::Linear, 1.0f / 0.40f }
            }
        },
        {
            "wild",
            {
                { "feedback", 0.45f, 0.57f, Curve::Linear, 1.0f / 1.05f },
                { "chaos", 0.0f, 0.72f, Curve::Progressive, 1.0f },
                { "wobble", 0.10f, 0.60f, Curve::Progressive, 1.0f / 0.90f },
                { "modRateHz", 0.15f, 0.90f, Curve::Log, 1.0f / 1.85f },
                { "modDepth", 0.10f, 0.65f, Curve::Progressive, 1.0f / 0.90f }
            }
        },
        {
            "bloom",
            {
                { "feedback", 0.45f, 0.75f, Curve::Linear, 1.0f / 1.05f },
                { "highCutHz", 9000.0f, 14000.0f, Curve::Log, 1.0f / 9000.0f },
                { "duckAmount", 0.10f, 0.55f, Curve::Exponential, 1.0f },
                // Existing core Bloom voicing enriches a manually enabled
                // shimmer. Avoid switching its feedback lane on at Bloom > 0.
                { "mix", 0.35f, 0.52f, Curve::Linear, 1.0f / 0.65f }
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

        case Curve::Progressive:
            // Nonzero initial slope; upper half still accelerates distinctly.
            return target.valueAt0 + (target.valueAt100 - target.valueAt0)
                * (0.35f * amount + 0.65f * amount * amount);

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
                auto contribution = offset;
                if (target.headroomScale > 0.0f)
                    if (const auto* spec = findParameterSpec(target.paramID))
                    {
                        const auto headroom = offset >= 0.0f
                            ? spec->maximum - value->target : value->target - spec->minimum;
                        contribution *= target.headroomScale * headroom;
                    }
                value->target = clampForParameter(target.paramID, value->target + contribution);
            }
        }
    }

    const auto wild = getMacroValue("wild");
    const auto wildFeedbackCeiling = 0.95f + wild * 0.04f;
    if (auto* feedback = findValue("feedback"))
    {
        // Compress only the upper manual range; no flat hard-ceiling interval.
        // Stay below unity at the Q31 boundary, including WILD=100%.
        constexpr float knee = 0.85f;
        if (feedback->target > knee)
            feedback->target = knee + (feedback->target - knee)
                * (wildFeedbackCeiling - knee) / (1.05f - knee);
    }
}
}
