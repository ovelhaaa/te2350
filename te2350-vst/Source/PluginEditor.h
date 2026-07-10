#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class TE2350AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit TE2350AudioProcessorEditor(TE2350AudioProcessor&);
    ~TE2350AudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    juce::Component* addSlider(juce::Component& parent, std::vector<juce::Component*>& group, const juce::String& parameterID, const juce::String& title);
    juce::Component* addCombo(juce::Component& parent, std::vector<juce::Component*>& group, const juce::String& parameterID, const juce::String& title);
    juce::Component* addToggle(juce::Component& parent, std::vector<juce::Component*>& group, const juce::String& parameterID, const juce::String& title);
    void layoutGrid(juce::Rectangle<int> area, const std::vector<juce::Component*>& group, int columns);
    void timerCallback() override;

    TE2350AudioProcessor& processor;
    juce::Component layer1;
    juce::Component layer2;
    juce::Component utility;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Label instabilityLabel;

    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<juce::Component*> layer1Controls;
    std::vector<juce::Component*> layer2Controls;
    std::vector<juce::Component*> utilityControls;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TE2350AudioProcessorEditor)
};
