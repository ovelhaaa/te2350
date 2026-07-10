#pragma once

#include <JuceHeader.h>

#include "Core/TE2350CoreWrapper.h"
#include "DSP/OversamplingChain.h"
#include "Params/MacroEngine.h"
#include "Params/ParameterLayout.h"

class TE2350AudioProcessor final : public juce::AudioProcessor
{
public:
    TE2350AudioProcessor();
    ~TE2350AudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    float getInstabilityMeterValue() const { return macroEngine.getInstability(); }

private:
    float getRawFloat(juce::StringRef parameterID, float fallback) const;
    int getChoiceIndex(juce::StringRef parameterID) const;
    bool getBool(juce::StringRef parameterID) const;
    double getHostBpm() const;
    float getSyncedTimeMs(int syncMode, double bpm, float freeTimeMs) const;
    te2350::CoreParameters collectCoreParameters(double bpm) const;

    te2350::TE2350CoreWrapper core;
    te2350::MacroEngine macroEngine;
    te2350::OversamplingChain oversampling;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TE2350AudioProcessor)
};
