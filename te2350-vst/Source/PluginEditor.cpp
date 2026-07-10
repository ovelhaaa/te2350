#include "PluginEditor.h"

namespace
{
class SliderTile final : public juce::Component
{
public:
    SliderTile(const juce::String& title)
    {
        label.setText(title, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
        addAndMakeVisible(label);
        addAndMakeVisible(slider);
    }

    juce::Slider& getSlider() { return slider; }

    void resized() override
    {
        auto area = getLocalBounds().reduced(3);
        label.setBounds(area.removeFromTop(18));
        slider.setBounds(area);
    }

private:
    juce::Label label;
    juce::Slider slider;
};

class ComboTile final : public juce::Component
{
public:
    ComboTile(const juce::String& title)
    {
        label.setText(title, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
        addAndMakeVisible(combo);
    }

    juce::ComboBox& getCombo() { return combo; }

    void resized() override
    {
        auto area = getLocalBounds().reduced(5);
        label.setBounds(area.removeFromTop(18));
        combo.setBounds(area.removeFromTop(26));
    }

private:
    juce::Label label;
    juce::ComboBox combo;
};

class ToggleTile final : public juce::Component
{
public:
    ToggleTile(const juce::String& title)
    {
        button.setButtonText(title);
        addAndMakeVisible(button);
    }

    juce::Button& getButton() { return button; }

    void resized() override
    {
        button.setBounds(getLocalBounds().reduced(5));
    }

private:
    juce::ToggleButton button;
};
}

TE2350AudioProcessorEditor::TE2350AudioProcessorEditor(TE2350AudioProcessor& processorRef)
    : AudioProcessorEditor(&processorRef), processor(processorRef)
{
    addAndMakeVisible(layer1);
    addAndMakeVisible(tabs);
    addAndMakeVisible(utility);
    tabs.addTab("Layer 2", juce::Colours::transparentBlack, &layer2, false);

    addSlider(layer1, layer1Controls, "space", "Space");
    addSlider(layer1, layer1Controls, "wild", "Wild");
    addSlider(layer1, layer1Controls, "bloom", "Bloom");
    addSlider(layer1, layer1Controls, "timeMs", "Time");
    addCombo(layer1, layer1Controls, "syncMode", "Free / Sync");
    addSlider(layer1, layer1Controls, "feedback", "Feedback");
    addSlider(layer1, layer1Controls, "mix", "Mix");
    addToggle(layer1, layer1Controls, "killDry", "Kill Dry");
    addSlider(layer1, layer1Controls, "lowCutHz", "Low Cut");
    addSlider(layer1, layer1Controls, "highCutHz", "High Cut");

    addSlider(layer2, layer2Controls, "diffusion", "Diffusion");
    addSlider(layer2, layer2Controls, "chaos", "Chaos");
    addSlider(layer2, layer2Controls, "wobble", "Wobble");
    addSlider(layer2, layer2Controls, "presence", "Presence");
    addSlider(layer2, layer2Controls, "modRateHz", "Mod Rate");
    addSlider(layer2, layer2Controls, "modDepth", "Mod Depth");
    addCombo(layer2, layer2Controls, "modShape", "Mod Shape");
    addCombo(layer2, layer2Controls, "shimmerInterval", "Shimmer Int");
    addSlider(layer2, layer2Controls, "shimmerAmount", "Shimmer");
    addSlider(layer2, layer2Controls, "shimmerFeedback", "Shim FB");
    addSlider(layer2, layer2Controls, "duckThreshold", "Duck Thresh");
    addSlider(layer2, layer2Controls, "duckAmount", "Ducking");

    addSlider(utility, utilityControls, "inputTrim", "Input");
    addSlider(utility, utilityControls, "outputTrim", "Output");
    addCombo(utility, utilityControls, "qualityMode", "Mode");
    addToggle(utility, utilityControls, "freezeEngage", "Freeze");
    addCombo(utility, utilityControls, "freezeMode", "Freeze Mode");
    addToggle(utility, utilityControls, "atmosFdnOn", "Atmos FDN");
    addSlider(utility, utilityControls, "wetWidth", "Wet Width");

    instabilityLabel.setJustificationType(juce::Justification::centred);
    utility.addAndMakeVisible(instabilityLabel);

    setSize(980, 640);
    startTimerHz(20);
}

void TE2350AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff151719));
    g.setColour(juce::Colour(0xffe8ecef));
    g.setFont(18.0f);
    g.drawText("TE-2350 Antigravity", getLocalBounds().removeFromTop(34), juce::Justification::centred);

    g.setColour(juce::Colour(0xff30363a));
    g.drawHorizontalLine(38, 18.0f, static_cast<float>(getWidth() - 18));
}

void TE2350AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(34);

    layer1.setBounds(area.removeFromTop(250));
    tabs.setBounds(area.removeFromTop(230));
    utility.setBounds(area);

    layoutGrid(layer1.getLocalBounds(), layer1Controls, 5);
    layoutGrid(layer2.getLocalBounds().reduced(8).withTrimmedTop(28), layer2Controls, 6);

    auto utilityArea = utility.getLocalBounds();
    instabilityLabel.setBounds(utilityArea.removeFromRight(130).reduced(8));
    layoutGrid(utilityArea, utilityControls, 7);
}

juce::Component* TE2350AudioProcessorEditor::addSlider(juce::Component& parent,
                                                       std::vector<juce::Component*>& group,
                                                       const juce::String& parameterID,
                                                       const juce::String& title)
{
    auto control = std::make_unique<SliderTile>(title);
    auto* raw = control.get();
    parent.addAndMakeVisible(raw);
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.apvts, parameterID, raw->getSlider()));
    group.push_back(raw);
    controls.push_back(std::move(control));
    return raw;
}

juce::Component* TE2350AudioProcessorEditor::addCombo(juce::Component& parent,
                                                      std::vector<juce::Component*>& group,
                                                      const juce::String& parameterID,
                                                      const juce::String& title)
{
    auto control = std::make_unique<ComboTile>(title);
    auto* raw = control.get();
    parent.addAndMakeVisible(raw);
    comboAttachments.push_back(std::make_unique<ComboAttachment>(processor.apvts, parameterID, raw->getCombo()));
    group.push_back(raw);
    controls.push_back(std::move(control));
    return raw;
}

juce::Component* TE2350AudioProcessorEditor::addToggle(juce::Component& parent,
                                                       std::vector<juce::Component*>& group,
                                                       const juce::String& parameterID,
                                                       const juce::String& title)
{
    auto control = std::make_unique<ToggleTile>(title);
    auto* raw = control.get();
    parent.addAndMakeVisible(raw);
    buttonAttachments.push_back(std::make_unique<ButtonAttachment>(processor.apvts, parameterID, raw->getButton()));
    group.push_back(raw);
    controls.push_back(std::move(control));
    return raw;
}

void TE2350AudioProcessorEditor::layoutGrid(juce::Rectangle<int> area,
                                            const std::vector<juce::Component*>& group,
                                            int columns)
{
    if (group.empty())
        return;

    const auto rows = (static_cast<int>(group.size()) + columns - 1) / columns;
    const auto cellW = area.getWidth() / columns;
    const auto cellH = area.getHeight() / rows;

    for (int index = 0; index < static_cast<int>(group.size()); ++index)
    {
        const auto row = index / columns;
        const auto column = index % columns;
        group[static_cast<size_t>(index)]->setBounds(area.getX() + column * cellW,
                                                     area.getY() + row * cellH,
                                                     cellW,
                                                     cellH);
    }
}

void TE2350AudioProcessorEditor::timerCallback()
{
    instabilityLabel.setText("Instability\n" + juce::String(processor.getInstabilityMeterValue() * 100.0f, 0) + "%",
                             juce::dontSendNotification);
}
