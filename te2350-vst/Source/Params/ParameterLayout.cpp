#include "ParameterLayout.h"

namespace te2350
{
namespace
{
juce::NormalisableRange<float> makeRange(float minimum, float maximum, float centre = 0.0f)
{
    juce::NormalisableRange<float> range(minimum, maximum);
    if (centre > minimum && centre < maximum)
        range.setSkewForCentre(centre);
    return range;
}

std::unique_ptr<juce::RangedAudioParameter> makeFloat(const juce::String& id,
                                                      const juce::String& name,
                                                      float minimum,
                                                      float maximum,
                                                      float defaultValue,
                                                      float centre = 0.0f,
                                                      const juce::String& suffix = {})
{
    auto range = makeRange(minimum, maximum, centre);
    juce::ignoreUnused(suffix);
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(id, 1), name, range, defaultValue);
}

std::unique_ptr<juce::RangedAudioParameter> makeChoice(const juce::String& id,
                                                       const juce::String& name,
                                                       juce::StringArray choices,
                                                       int defaultIndex)
{
    return std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(id, 1), name, choices, defaultIndex);
}

std::unique_ptr<juce::RangedAudioParameter> makeBool(const juce::String& id,
                                                     const juce::String& name,
                                                     bool defaultValue)
{
    return std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(id, 1), name, defaultValue);
}
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(makeFloat("space", "Space", 0.0f, 1.0f, 0.30f));
    params.push_back(makeFloat("wild", "Wild", 0.0f, 1.0f, 0.00f));
    params.push_back(makeFloat("bloom", "Bloom", 0.0f, 1.0f, 0.20f));

    params.push_back(makeFloat("timeMs", "Time", 10.0f, 2000.0f, 420.0f, 420.0f, " ms"));
    params.push_back(makeChoice("syncMode", "Sync Mode", { "Free", "1/4", "1/8", "1/8.", "1/8T", "1/16" }, 0));
    params.push_back(makeFloat("feedback", "Feedback", 0.0f, 1.05f, 0.45f, 0.70f));
    params.push_back(makeFloat("mix", "Mix", 0.0f, 1.0f, 0.35f));
    params.push_back(makeBool("killDry", "Kill Dry", false));
    params.push_back(makeFloat("lowCutHz", "Low Cut", 20.0f, 1000.0f, 80.0f, 120.0f, " Hz"));
    params.push_back(makeFloat("highCutHz", "High Cut", 1000.0f, 18000.0f, 9000.0f, 9000.0f, " Hz"));

    params.push_back(makeFloat("diffusion", "Diffusion", 0.0f, 1.0f, 0.40f));
    params.push_back(makeFloat("chaos", "Chaos", 0.0f, 1.0f, 0.00f, 0.25f));
    params.push_back(makeFloat("wobble", "Wobble", 0.0f, 1.0f, 0.10f));
    params.push_back(makeFloat("presence", "Presence", 0.0f, 1.0f, 0.50f));
    params.push_back(makeFloat("modRateHz", "Mod Rate", 0.02f, 2.0f, 0.15f, 0.25f, " Hz"));
    params.push_back(makeFloat("modDepth", "Mod Depth", 0.0f, 1.0f, 0.10f));
    params.push_back(makeChoice("modShape", "Mod Shape", { "Triangle", "Random Walk", "S&H" }, 1));
    params.push_back(makeChoice("shimmerInterval", "Shimmer Interval", { "-1 oct", "5th", "+1 oct" }, 2));
    params.push_back(makeFloat("shimmerAmount", "Shimmer Amount", 0.0f, 1.0f, 0.00f));
    params.push_back(makeFloat("shimmerFeedback", "Shimmer Feedback", 0.0f, 0.95f, 0.30f, 0.55f));
    params.push_back(makeFloat("duckThreshold", "Duck Threshold", -60.0f, 0.0f, -24.0f, 0.0f, " dB"));
    params.push_back(makeFloat("duckAmount", "Duck Amount", 0.0f, 1.0f, 0.10f, 0.30f));

    params.push_back(makeFloat("inputTrim", "Input Trim", -24.0f, 24.0f, 0.0f, 0.0f, " dB"));
    params.push_back(makeFloat("outputTrim", "Output Trim", -24.0f, 24.0f, 0.0f, 0.0f, " dB"));
    params.push_back(makeChoice("qualityMode", "Hardware / Studio Mode", { "Hardware", "Studio" }, 0));
    params.push_back(makeBool("freezeEngage", "Freeze", false));
    params.push_back(makeChoice("freezeMode", "Freeze Mode", { "Momentary", "Latch" }, 1));
    params.push_back(makeBool("atmosFdnOn", "Atmos FDN", false));
    params.push_back(makeFloat("wetWidth", "Stereo Width", 0.0f, 1.0f, 0.60f));

    return { params.begin(), params.end() };
}

const std::vector<ParameterSpec>& getParameterSpecs()
{
    static const std::vector<ParameterSpec> specs {
        { "space", 0.0f, 1.0f, 0.30f },
        { "wild", 0.0f, 1.0f, 0.00f },
        { "bloom", 0.0f, 1.0f, 0.20f },
        { "timeMs", 10.0f, 2000.0f, 420.0f },
        { "feedback", 0.0f, 1.05f, 0.45f },
        { "mix", 0.0f, 1.0f, 0.35f },
        { "lowCutHz", 20.0f, 1000.0f, 80.0f },
        { "highCutHz", 1000.0f, 18000.0f, 9000.0f },
        { "diffusion", 0.0f, 1.0f, 0.40f },
        { "chaos", 0.0f, 1.0f, 0.00f },
        { "wobble", 0.0f, 1.0f, 0.10f },
        { "presence", 0.0f, 1.0f, 0.50f },
        { "modRateHz", 0.02f, 2.0f, 0.15f },
        { "modDepth", 0.0f, 1.0f, 0.10f },
        { "shimmerAmount", 0.0f, 1.0f, 0.00f },
        { "shimmerFeedback", 0.0f, 0.95f, 0.30f },
        { "duckThreshold", -60.0f, 0.0f, -24.0f },
        { "duckAmount", 0.0f, 1.0f, 0.10f },
        { "inputTrim", -24.0f, 24.0f, 0.0f },
        { "outputTrim", -24.0f, 24.0f, 0.0f },
        { "wetWidth", 0.0f, 1.0f, 0.60f }
    };

    return specs;
}

const ParameterSpec* findParameterSpec(juce::StringRef parameterID)
{
    for (const auto& spec : getParameterSpecs())
        if (spec.id == parameterID)
            return &spec;

    return nullptr;
}

float getParameterDefault(juce::StringRef parameterID)
{
    if (const auto* spec = findParameterSpec(parameterID))
        return spec->defaultValue;

    return 0.0f;
}
}
