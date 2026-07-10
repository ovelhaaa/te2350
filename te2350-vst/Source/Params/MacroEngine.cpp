#include "MacroEngine.h"
#include "ParameterLayout.h"

#include <cmath>

namespace te2350
{
MacroEngine::MacroEngine()
    : definitions(createFactoryDefinitions())
{
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
    currentValues.clear();
    targetValues.clear();
    macroValues.clear();

    for (const auto& spec : getParameterSpecs())
    {
        currentValues[spec.id.toStdString()] = spec.defaultValue;
        targetValues[spec.id.toStdString()] = spec.defaultValue;
    }

    macroValues["space"] = getParameterDefault("space");
    macroValues["wild"] = getParameterDefault("wild");
    macroValues["bloom"] = getParameterDefault("bloom");
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

    for (auto& [id, current] : currentValues)
    {
        const auto targetIt = targetValues.find(id);
        if (targetIt == targetValues.end())
            continue;

        current += (targetIt->second - current) * smoothing;
    }
}

float MacroEngine::getEffectiveValue(juce::StringRef parameterID, float fallback) const
{
    const auto found = currentValues.find(juce::String(parameterID).toStdString());
    return found != currentValues.end() ? found->second : fallback;
}

float MacroEngine::getMacroValue(juce::StringRef macroID) const
{
    const auto found = macroValues.find(juce::String(macroID).toStdString());
    return found != macroValues.end() ? found->second : 0.0f;
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
    if (const auto* raw = state.getRawParameterValue(juce::String(parameterID)))
        return raw->load();

    return fallback;
}

void MacroEngine::calculateTargets(const juce::AudioProcessorValueTreeState& state)
{
    macroValues["space"] = readStateValue(state, "space", 0.0f);
    macroValues["wild"] = readStateValue(state, "wild", 0.0f);
    macroValues["bloom"] = readStateValue(state, "bloom", 0.0f);

    targetValues.clear();

    for (const auto& spec : getParameterSpecs())
        targetValues[spec.id.toStdString()] = readStateValue(state, spec.id, spec.defaultValue);

    for (const auto& definition : definitions)
    {
        const auto macroValue = getMacroValue(definition.macroID);

        for (const auto& target : definition.targets)
        {
            const auto id = target.paramID.toStdString();
            const auto mapped = mapTarget(target, macroValue);
            const auto offset = mapped - target.valueAt0;
            targetValues[id] = clampForParameter(target.paramID, targetValues[id] + offset);
        }
    }

    const auto wild = getMacroValue("wild");
    const auto wildFeedbackCeiling = 0.95f + wild * 0.10f;
    targetValues["feedback"] = juce::jmin(targetValues["feedback"], wildFeedbackCeiling);
}
}
