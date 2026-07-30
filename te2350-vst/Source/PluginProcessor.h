#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <vector>

#include "Core/TE2350CoreWrapper.h"
#include "DSP/OversamplingChain.h"
#include "Params/MacroEngine.h"
#include "Params/ParameterLayout.h"

class TE2350AudioProcessor final : public juce::AudioProcessor
{
    juce::UndoManager undoManager;

public:
    TE2350AudioProcessor();
    ~TE2350AudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        processBlock(buffer, midi);
    }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 65.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void beginUndoTransaction(const juce::String& name);
    bool canUndo() const { return undoManager.canUndo(); }
    bool canRedo() const { return undoManager.canRedo(); }
    juce::String getUndoDescription() const { return undoManager.getUndoDescription(); }
    juce::String getRedoDescription() const { return undoManager.getRedoDescription(); }
    bool undo();
    bool redo();
    void mutateParameters(float amount, juce::uint32 seed = 0);
    void setActiveUserPreset(const juce::String& name);
    const juce::String& getActivePresetName() const noexcept { return activePresetName; }
    bool isActivePresetUser() const noexcept { return activePresetIsUser; }

    juce::AudioProcessorValueTreeState apvts;
    float getInstabilityMeterValue() const { return instabilityMeter.load(); }
    float getInputMeterValue() const { return inputMeter.load(); }
    float getOutputMeterValue() const { return outputMeter.load(); }

private:
    float getRawFloat(juce::StringRef parameterID, float fallback) const;
    int getChoiceIndex(juce::StringRef parameterID) const;
    bool getBool(juce::StringRef parameterID) const;
    double getHostBpm() const;
    float getSyncedTimeMs(int syncMode, double bpm, float freeTimeMs) const;
    te2350::CoreParameters collectCoreParameters(double bpm) const;
    juce::ValueTree migrateState(const juce::ValueTree& incomingState) const;

    te2350::TE2350CoreWrapper core;
    te2350::MacroEngine macroEngine;
    te2350::OversamplingChain oversampling;
    juce::AudioBuffer<float> bypassDryBuffer;
    std::vector<float> bypassRamp;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix;
    std::atomic<float> instabilityMeter { 0.0f };
    std::atomic<float> inputMeter { 0.0f };
    std::atomic<float> outputMeter { 0.0f };
    juce::ValueTree defaultState;
    juce::String activePresetName;
    bool activePresetIsUser = false;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TE2350AudioProcessor)
};
