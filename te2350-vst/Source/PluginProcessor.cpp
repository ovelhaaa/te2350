#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"

namespace
{
float measureBufferLevel(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return 0.0f;

    auto rms = 0.0f;
    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        rms = juce::jmax(rms, buffer.getRMSLevel(channel, 0, buffer.getNumSamples()));

    return juce::jlimit(0.0f, 1.0f, rms * 1.8f);
}

float smoothMeter(float previous, float current)
{
    return current > previous ? current : previous * 0.88f + current * 0.12f;
}
}

TE2350AudioProcessor::TE2350AudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", te2350::createParameterLayout())
{
}

void TE2350AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    core.prepare(sampleRate, samplesPerBlock);
    macroEngine.prepare(sampleRate, 64);
    oversampling.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

void TE2350AudioProcessor::releaseResources()
{
}

bool TE2350AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

void TE2350AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto inputLevel = measureBufferLevel(buffer);
    inputMeter.store(smoothMeter(inputMeter.load(), inputLevel));

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    if (getBool("bypass"))
    {
        outputMeter.store(smoothMeter(outputMeter.load(), inputLevel));
        return;
    }

    const auto bpm = getHostBpm();
    macroEngine.update(apvts, buffer.getNumSamples());
    instabilityMeter.store(macroEngine.getInstability());
    oversampling.setStudioMode(getChoiceIndex("qualityMode") == 1, *this);

    core.setParameters(collectCoreParameters(bpm));
    core.processBlock(buffer);

    outputMeter.store(smoothMeter(outputMeter.load(), measureBufferLevel(buffer)));
}

juce::AudioProcessorEditor* TE2350AudioProcessor::createEditor()
{
    return new TE2350AudioProcessorEditor(*this);
}

int TE2350AudioProcessor::getNumPrograms()
{
    return te2350::getFactoryPresetNames().size();
}

void TE2350AudioProcessor::setCurrentProgram(int index)
{
    if (!juce::isPositiveAndBelow(index, getNumPrograms()))
        return;

    currentProgram = index;
    te2350::applyFactoryPreset(apvts, index);
}

const juce::String TE2350AudioProcessor::getProgramName(int index)
{
    const auto names = te2350::getFactoryPresetNames();
    return juce::isPositiveAndBelow(index, names.size()) ? names[index] : juce::String();
}

void TE2350AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void TE2350AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

float TE2350AudioProcessor::getRawFloat(juce::StringRef parameterID, float fallback) const
{
    if (const auto* raw = apvts.getRawParameterValue(juce::String(parameterID)))
        return raw->load();

    return fallback;
}

int TE2350AudioProcessor::getChoiceIndex(juce::StringRef parameterID) const
{
    return juce::roundToInt(getRawFloat(parameterID, 0.0f));
}

bool TE2350AudioProcessor::getBool(juce::StringRef parameterID) const
{
    return getRawFloat(parameterID, 0.0f) >= 0.5f;
}

double TE2350AudioProcessor::getHostBpm() const
{
    if (auto* currentPlayHead = getPlayHead())
    {
       #if JUCE_MAJOR_VERSION >= 7
        if (auto position = currentPlayHead->getPosition())
            if (auto bpm = position->getBpm())
                return *bpm;
       #else
        juce::AudioPlayHead::CurrentPositionInfo position;
        if (currentPlayHead->getCurrentPosition(position) && position.bpm > 0.0)
            return position.bpm;
       #endif
    }

    return 120.0;
}

float TE2350AudioProcessor::getSyncedTimeMs(int syncMode, double bpm, float freeTimeMs) const
{
    if (syncMode == 0 || bpm <= 0.0)
        return freeTimeMs;

    const auto quarterMs = static_cast<float>(60000.0 / bpm);

    switch (syncMode)
    {
        case 1: return quarterMs;
        case 2: return quarterMs * 0.5f;
        case 3: return quarterMs * 0.75f;
        case 4: return quarterMs / 3.0f;
        case 5: return quarterMs * 0.25f;
        default: break;
    }

    return freeTimeMs;
}

te2350::CoreParameters TE2350AudioProcessor::collectCoreParameters(double bpm) const
{
    te2350::CoreParameters parameters;

    const auto fallback = [] (juce::StringRef id) { return te2350::getParameterDefault(id); };

    parameters.space = getRawFloat("space", fallback("space"));
    parameters.wild = getRawFloat("wild", fallback("wild"));
    parameters.timeMs = macroEngine.getEffectiveValue("timeMs", fallback("timeMs"));
    parameters.timeMs = getSyncedTimeMs(getChoiceIndex("syncMode"), bpm, parameters.timeMs);
    parameters.feedback = macroEngine.getEffectiveValue("feedback", fallback("feedback"));
    parameters.mix = macroEngine.getEffectiveValue("mix", fallback("mix"));
    parameters.killDry = getBool("killDry");
    parameters.lowCutHz = macroEngine.getEffectiveValue("lowCutHz", fallback("lowCutHz"));
    parameters.highCutHz = macroEngine.getEffectiveValue("highCutHz", fallback("highCutHz"));
    parameters.diffusion = macroEngine.getEffectiveValue("diffusion", fallback("diffusion"));
    parameters.chaos = macroEngine.getEffectiveValue("chaos", fallback("chaos"));
    parameters.wobble = macroEngine.getEffectiveValue("wobble", fallback("wobble"));
    parameters.presence = macroEngine.getEffectiveValue("presence", fallback("presence"));
    parameters.modRateHz = macroEngine.getEffectiveValue("modRateHz", fallback("modRateHz"));
    parameters.modDepth = macroEngine.getEffectiveValue("modDepth", fallback("modDepth"));
    parameters.shimmerInterval = getChoiceIndex("shimmerInterval");
    parameters.shimmerAmount = macroEngine.getEffectiveValue("shimmerAmount", fallback("shimmerAmount"));
    parameters.shimmerFeedback = macroEngine.getEffectiveValue("shimmerFeedback", fallback("shimmerFeedback"));
    parameters.duckAmount = macroEngine.getEffectiveValue("duckAmount", fallback("duckAmount"));
    parameters.inputTrimDb = getRawFloat("inputTrim", fallback("inputTrim"));
    parameters.outputTrimDb = getRawFloat("outputTrim", fallback("outputTrim"));
    parameters.freeze = getBool("freezeEngage");
    parameters.atmosFdnOn = getBool("atmosFdnOn") || parameters.space >= 0.40f;
    parameters.wetWidth = macroEngine.getEffectiveValue("wetWidth", fallback("wetWidth"));

    return parameters;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TE2350AudioProcessor();
}
