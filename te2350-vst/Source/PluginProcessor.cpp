#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"

#include <array>

namespace
{
constexpr int currentStateVersion = 3;

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
      apvts(*this, &undoManager, "PARAMETERS", te2350::createParameterLayout())
{
    defaultState = apvts.copyState();
    te2350::applyFactoryPreset(apvts, currentProgram);
    apvts.copyState();
    undoManager.clearUndoHistory();
    activePresetName = getProgramName(currentProgram);
}

void TE2350AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const auto preparedBlockSize = juce::jmax(1, samplesPerBlock);
    core.prepare(sampleRate, preparedBlockSize);
    macroEngine.prepare(sampleRate, 64);
    oversampling.prepare(sampleRate, preparedBlockSize, getTotalNumOutputChannels());
    oversampling.setStudioMode(getChoiceIndex("qualityMode") == 1);
    setLatencySamples(oversampling.getLatencySamples());
    bypassDryBuffer.setSize(getTotalNumOutputChannels(), preparedBlockSize, false, false, true);
    bypassRamp.resize(static_cast<size_t>(preparedBlockSize), 0.0f);
    bypassMix.reset(sampleRate, 0.02);
    bypassMix.setCurrentAndTargetValue(getBool("bypass") ? 1.0f : 0.0f);
}

void TE2350AudioProcessor::releaseResources()
{
    bypassDryBuffer.setSize(0, 0);
    bypassRamp.clear();
}

void TE2350AudioProcessor::reset()
{
    core.reset();
    macroEngine.reset();
    oversampling.reset();
    instabilityMeter.store(0.0f);
    inputMeter.store(0.0f);
    outputMeter.store(0.0f);
    bypassMix.setCurrentAndTargetValue(getBool("bypass") ? 1.0f : 0.0f);
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

    const auto chunkCapacity = juce::jmin(
        bypassDryBuffer.getNumSamples(),
        static_cast<int>(bypassRamp.size()));
    if (chunkCapacity <= 0 || bypassDryBuffer.getNumChannels() < buffer.getNumChannels())
    {
        jassertfalse;
        buffer.clear();
        outputMeter.store(0.0f);
        return;
    }

    const auto bpm = getHostBpm();
    for (auto offset = 0; offset < buffer.getNumSamples(); offset += chunkCapacity)
    {
        const auto numSamples = juce::jmin(chunkCapacity, buffer.getNumSamples() - offset);
        juce::AudioBuffer<float> effectBlock(
            buffer.getArrayOfWritePointers(),
            buffer.getNumChannels(),
            offset,
            numSamples);
        juce::AudioBuffer<float> dryBlock(
            bypassDryBuffer.getArrayOfWritePointers(),
            buffer.getNumChannels(),
            numSamples);

        for (auto channel = 0; channel < effectBlock.getNumChannels(); ++channel)
            dryBlock.copyFrom(channel, 0, effectBlock, channel, 0, numSamples);

        bypassMix.setTargetValue(getBool("bypass") ? 1.0f : 0.0f);
        for (auto sample = 0; sample < numSamples; ++sample)
        {
            const auto bypassAmount = bypassMix.getNextValue();
            bypassRamp[static_cast<size_t>(sample)] = bypassAmount;

            for (auto channel = 0; channel < effectBlock.getNumChannels(); ++channel)
                effectBlock.getWritePointer(channel)[sample] *= 1.0f - bypassAmount;
        }

        macroEngine.update(apvts, numSamples);
        instabilityMeter.store(macroEngine.getInstability());

        core.setParameters(collectCoreParameters(bpm));
        core.processBlock(effectBlock);
        oversampling.setStudioMode(getChoiceIndex("qualityMode") == 1);
        oversampling.processEffectBlock(effectBlock);
        oversampling.processDryBlock(dryBlock);

        for (auto channel = 0; channel < effectBlock.getNumChannels(); ++channel)
        {
            auto* output = effectBlock.getWritePointer(channel);
            const auto* dry = dryBlock.getReadPointer(channel);

            for (auto sample = 0; sample < numSamples; ++sample)
            {
                const auto bypassAmount = bypassRamp[static_cast<size_t>(sample)];
                output[sample] = output[sample] * (1.0f - bypassAmount)
                               + dry[sample] * bypassAmount;
            }
        }
    }

    outputMeter.store(smoothMeter(outputMeter.load(), measureBufferLevel(buffer)));
}

juce::AudioProcessorParameter* TE2350AudioProcessor::getBypassParameter() const
{
    return apvts.getParameter("bypass");
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

    beginUndoTransaction("Load " + getProgramName(index));
    currentProgram = index;
    te2350::applyFactoryPreset(apvts, index);
    apvts.copyState();
    activePresetName = getProgramName(index);
    activePresetIsUser = false;
}

const juce::String TE2350AudioProcessor::getProgramName(int index)
{
    const auto names = te2350::getFactoryPresetNames();
    return juce::isPositiveAndBelow(index, names.size()) ? names[index] : juce::String();
}

void TE2350AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("currentProgram", currentProgram, nullptr);
    state.setProperty("stateVersion", currentStateVersion, nullptr);
    state.setProperty("activePresetName", activePresetName, nullptr);
    state.setProperty("activePresetIsUser", activePresetIsUser, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void TE2350AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
        {
            const auto incomingState = juce::ValueTree::fromXml(*xml);
            currentProgram = juce::jlimit(0,
                                         juce::jmax(0, getNumPrograms() - 1),
                                         static_cast<int>(incomingState.getProperty("currentProgram", 0)));
            activePresetName = incomingState.getProperty(
                "activePresetName",
                getProgramName(currentProgram)).toString();
            activePresetIsUser = static_cast<bool>(
                incomingState.getProperty("activePresetIsUser", false));
            if (activePresetName.trim().isEmpty())
            {
                activePresetName = getProgramName(currentProgram);
                activePresetIsUser = false;
            }
            apvts.replaceState(migrateState(incomingState));
        }
}

void TE2350AudioProcessor::beginUndoTransaction(const juce::String& name)
{
    apvts.copyState();
    undoManager.beginNewTransaction(name);
}

bool TE2350AudioProcessor::undo()
{
    return undoManager.undo();
}

bool TE2350AudioProcessor::redo()
{
    return undoManager.redo();
}

void TE2350AudioProcessor::mutateParameters(float amount, juce::uint32 seed)
{
    struct MutationRule
    {
        const char* parameterID;
        float scale;
        float maximumPlainValue;
    };

    constexpr std::array<MutationRule, 17> rules {{
        { "space", 1.00f, 1.00f },
        { "wild", 0.65f, 0.86f },
        { "bloom", 0.82f, 0.92f },
        { "feedback", 0.58f, 0.92f },
        { "lowCutHz", 0.48f, 1000.0f },
        { "highCutHz", 0.48f, 18000.0f },
        { "diffusion", 0.78f, 1.00f },
        { "chaos", 0.72f, 0.90f },
        { "wobble", 0.72f, 0.90f },
        { "presence", 0.60f, 1.00f },
        { "modRateHz", 0.52f, 2.00f },
        { "modDepth", 0.68f, 0.90f },
        { "shimmerAmount", 0.54f, 0.72f },
        { "shimmerFeedback", 0.46f, 0.80f },
        { "duckThreshold", 0.42f, 0.0f },
        { "duckAmount", 0.58f, 0.82f },
        { "wetWidth", 0.52f, 1.00f }
    }};

    amount = juce::jlimit(0.0f, 1.0f, amount);
    if (amount <= 0.0f)
        return;

    juce::Random random(seed == 0 ? juce::Random::getSystemRandom().nextInt()
                                  : static_cast<juce::int64>(seed));
    beginUndoTransaction("Mutate");
    for (const auto& rule : rules)
    {
        auto* parameter = apvts.getParameter(rule.parameterID);
        if (parameter == nullptr)
            continue;

        const auto movement = (random.nextFloat() * 2.0f - 1.0f) * amount * rule.scale;
        auto maximum = 1.0f;
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            maximum = ranged->convertTo0to1(rule.maximumPlainValue);

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(
            juce::jlimit(0.0f, juce::jlimit(0.0f, 1.0f, maximum),
                         parameter->getValue() + movement));
        parameter->endChangeGesture();
    }
    apvts.copyState();
}

void TE2350AudioProcessor::setActiveUserPreset(const juce::String& name)
{
    activePresetName = name.trim();
    activePresetIsUser = true;
}

juce::ValueTree TE2350AudioProcessor::migrateState(const juce::ValueTree& incomingState) const
{
    auto migrated = defaultState.createCopy();

    for (int property = 0; property < incomingState.getNumProperties(); ++property)
    {
        const auto propertyName = incomingState.getPropertyName(property);
        migrated.setProperty(propertyName, incomingState.getProperty(propertyName), nullptr);
    }

    for (const auto& incomingParameter : incomingState)
    {
        const auto parameterID = incomingParameter.getProperty("id").toString();
        if (parameterID.isEmpty() || !incomingParameter.hasProperty("value"))
            continue;

        for (auto migratedParameter : migrated)
        {
            if (migratedParameter.getProperty("id").toString() == parameterID)
            {
                migratedParameter.setProperty(
                    "value",
                    incomingParameter.getProperty("value"),
                    nullptr);
                break;
            }
        }
    }

    migrated.setProperty("stateVersion", currentStateVersion, nullptr);
    return migrated;
}

float TE2350AudioProcessor::getRawFloat(juce::StringRef parameterID, float fallback) const
{
    if (const auto* raw = apvts.getRawParameterValue(parameterID))
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
    parameters.bloom = getRawFloat("bloom", fallback("bloom"));
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
    parameters.modShape = getChoiceIndex("modShape");
    parameters.shimmerInterval = getChoiceIndex("shimmerInterval");
    parameters.shimmerAmount = macroEngine.getEffectiveValue("shimmerAmount", fallback("shimmerAmount"));
    parameters.shimmerFeedback = macroEngine.getEffectiveValue("shimmerFeedback", fallback("shimmerFeedback"));
    parameters.duckThresholdDb = getRawFloat("duckThreshold", fallback("duckThreshold"));
    parameters.duckAmount = macroEngine.getEffectiveValue("duckAmount", fallback("duckAmount"));
    parameters.inputTrimDb = getRawFloat("inputTrim", fallback("inputTrim"));
    parameters.outputTrimDb = getRawFloat("outputTrim", fallback("outputTrim"));
    parameters.freeze = getBool("freezeEngage");
    parameters.atmosFdnOn = getBool("atmosFdnOn");
    parameters.wetWidth = macroEngine.getEffectiveValue("wetWidth", fallback("wetWidth"));

    return parameters;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TE2350AudioProcessor();
}
