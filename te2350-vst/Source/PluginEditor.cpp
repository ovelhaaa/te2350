#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"
#include "UI/AntigravityKnob.h"

#include <cmath>

using namespace te2350::ui;

namespace
{
constexpr int margin = 16;
constexpr int gridGap = 8;
constexpr int userPresetIDBase = 1000;
constexpr int embeddedUserPresetID = 9000;

void populateComboChoices(juce::ComboBox& combo, juce::RangedAudioParameter* parameter)
{
    combo.clear(juce::dontSendNotification);

    if (parameter == nullptr)
        return;

    const auto choices = parameter->getAllValueStrings();
    for (int index = 0; index < choices.size(); ++index)
        combo.addItem(choices[index], index + 1);
}

void enableDefaultReset(juce::Slider& slider, juce::RangedAudioParameter* parameter)
{
    if (parameter != nullptr)
    {
        slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
        slider.getProperties().set("defaultProportion", parameter->getDefaultValue());
    }
}

}

class TE2350AudioProcessorEditor::SectionPanel final : public juce::Component
{
public:
    explicit SectionPanel(juce::String titleText) : title(std::move(titleText)) {}

    juce::Rectangle<int> getContentBounds() const
    {
        return getLocalBounds().reduced(12, 7).withTrimmedTop(title.isEmpty() ? 0 : 23);
    }

    void paint(juce::Graphics& g) override
    {
        drawPanel(g, getLocalBounds().toFloat());
        if (title.isEmpty())
            return;

        g.setColour(orange());
        g.fillRect(13, 10, 3, 10);
        g.setColour(textMain());
        g.setFont(uiFont(12.0f, juce::Font::bold));
        g.drawText(title, 23, 4, getWidth() - 36, 22, juce::Justification::centredLeft);
        g.setColour(panelLine());
        g.drawHorizontalLine(28, 12.0f, static_cast<float>(getWidth() - 12));
    }

private:
    juce::String title;
};

class TE2350AudioProcessorEditor::GroupLabel final : public juce::Component
{
public:
    explicit GroupLabel(juce::String labelText) : label(std::move(labelText)) {}

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(10, 10);
        g.setColour(orange());
        g.fillRect(bounds.removeFromTop(2).removeFromLeft(22));
        g.setColour(textMain());
        g.setFont(uiFont(12.0f, juce::Font::bold));
        g.drawText(label, bounds.removeFromTop(28), juce::Justification::centredLeft);
        g.setColour(panelLine());
        g.drawVerticalLine(getWidth() - 10, 8.0f, static_cast<float>(getHeight() - 8));
    }

private:
    juce::String label;
};

class TE2350AudioProcessorEditor::ComboTile final : public juce::Component
{
public:
    ComboTile(juce::String titleText, juce::String subtitleText)
        : title(std::move(titleText)), subtitle(std::move(subtitleText))
    {
        setName(title + " selector");
        setTitle(title);
        setDescription(subtitle);
        combo.setName(title);
        combo.setTitle(title);
        combo.setDescription(subtitle);
        combo.setWantsKeyboardFocus(true);
        combo.setTooltip(controlTooltip(title, subtitle, false));
        addAndMakeVisible(combo);
    }

    juce::ComboBox& getCombo() { return combo; }

    void paint(juce::Graphics& g) override
    {
        g.setColour(textMain());
        g.setFont(uiFont(11, juce::Font::bold));
        auto bounds = getLocalBounds();
        if (getHeight() < 35)
            g.drawText(title.toUpperCase(), bounds.removeFromLeft(32), juce::Justification::centredLeft);
        else
            g.drawText(title.toUpperCase(), bounds.removeFromTop(27), juce::Justification::centred);
    }
    void resized() override
    {
        auto bounds = getLocalBounds();
        if (getHeight() < 35)
            combo.setBounds(bounds.withTrimmedLeft(32));
        else
            combo.setBounds(bounds.reduced(7).withTrimmedTop(21).withSizeKeepingCentre(getWidth() - 14, 27));
    }

private:
    juce::String title;
    juce::String subtitle;
    juce::ComboBox combo;
};

class TE2350AudioProcessorEditor::ToggleTile final : public juce::Component
{
public:
    class ModeAwareToggleButton final : public juce::ToggleButton
    {
    public:
        void setMomentaryPredicate(std::function<bool()> predicate)
        {
            isMomentary = std::move(predicate);
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (isMomentary && isMomentary())
            {
                setToggleState(true, juce::sendNotificationSync);
                return;
            }

            juce::ToggleButton::mouseDown(event);
        }

        void mouseUp(const juce::MouseEvent& event) override
        {
            if (isMomentary && isMomentary())
            {
                setToggleState(false, juce::sendNotificationSync);
                return;
            }

            juce::ToggleButton::mouseUp(event);
        }

    private:
        std::function<bool()> isMomentary;
    };

    ToggleTile(juce::String titleText, juce::Colour accentColour)
        : title(std::move(titleText)), accent(accentColour)
    {
        setName(title + " switch");
        setTitle(title);
        setDescription("Toggle " + title.toLowerCase());
        button.setName(title);
        button.setTitle(title);
        button.setDescription("Toggle " + title.toLowerCase());
        button.setWantsKeyboardFocus(true);
        button.setTooltip(controlTooltip(title, juce::String("Toggle ") + title.toLowerCase(), false));
        button.setColour(juce::TextButton::buttonOnColourId, accent);
        button.onStateChange = [this] { updateButtonText(); repaint(); };
        updateButtonText();
        addAndMakeVisible(button);
    }

    juce::Button& getButton() { return button; }
    void setMomentaryPredicate(std::function<bool()> predicate)
    {
        button.setMomentaryPredicate(std::move(predicate));
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(4.0f);
        const auto active = button.getToggleState();
        const auto focused = button.hasKeyboardFocus(true);
        g.setColour(display().withAlpha(0.35f));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour((active || focused ? accent : accent.withAlpha(0.30f))
                        .withAlpha(focused ? 0.92f : active ? 0.72f : 0.30f));
        g.drawRoundedRectangle(r, 7.0f, focused ? 1.5f : active ? 1.2f : 0.9f);

        g.setColour(textMain());
        g.setFont(uiFont(11.0f, juce::Font::bold));
        g.drawFittedText(title.toUpperCase(), getLocalBounds().reduced(8, 6).removeFromTop(17),
                         juce::Justification::centred, 1);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10, 6);
        area.removeFromTop(21);
        button.setBounds(area.withSizeKeepingCentre(area.getWidth(), juce::jmin(30, area.getHeight())));
    }

private:
    void updateButtonText()
    {
        button.setButtonText(button.getToggleState() ? "ON" : "OFF");
    }

    juce::String title;
    juce::Colour accent;
    ModeAwareToggleButton button;
};

class TE2350AudioProcessorEditor::MeterStrip final : public juce::Component
{
public:
    MeterStrip(juce::String labelText, juce::Colour meterColour, bool warningMode = false)
        : label(std::move(labelText)), colour(meterColour), isWarningMeter(warningMode)
    {
    }

    void setLevel(float newLevel)
    {
        const auto next = juce::jlimit(0.0f, 1.0f, newLevel);
        if (next < level || next > level)
        {
            level = next;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(2.0f);
        const auto displayColour = getDisplayColour();
        auto labelArea = r.removeFromTop(13.0f).toNearestInt();

        g.setColour(textMuted());
        g.setFont(uiFont(10.0f, juce::Font::bold));
        g.drawText(label.toUpperCase(), labelArea, juce::Justification::centredLeft);

        g.setColour(isWarningMeter && level >= 0.82f ? displayColour : textMuted().withAlpha(0.88f));
        g.drawFittedText(formatLevel(), labelArea.withTrimmedLeft(115).removeFromLeft(62),
                         juce::Justification::centredRight, 1);

        auto track = r.reduced(0.0f, 5.0f);
        g.setColour(display());
        g.fillRoundedRectangle(track, 3.0f);

        auto fill = track;
        fill.setWidth(track.getWidth() * level);
        g.setColour(displayColour);
        g.fillRoundedRectangle(fill, 3.0f);

        if (isWarningMeter)
        {
            const auto warnX = track.getX() + track.getWidth() * 0.82f;
            g.setColour(orangeHover().withAlpha(0.52f));
            g.drawVerticalLine(juce::roundToInt(warnX), track.getY() - 2.0f, track.getBottom() + 2.0f);
        }

        g.setColour(panelLine().withAlpha(0.82f));
        g.drawRoundedRectangle(track, 3.0f, 1.0f);
    }

private:
    juce::Colour getDisplayColour() const
    {
        if (! isWarningMeter)
            return colour;

        if (level >= 0.82f)
            return orangeHover();

        return orange();
    }

    juce::String formatLevel() const
    {
        if (isWarningMeter)
            return juce::String(juce::roundToInt(level * 105.0f)) + "%";

        if (level <= 0.0001f)
            return "-inf";

        const auto db = juce::Decibels::gainToDecibels(level, -60.0f);
        return juce::String(db, db > -10.0f ? 1 : 0) + " dB";
    }

    juce::String label;
    juce::Colour colour;
    bool isWarningMeter = false;
    float level = 0.0f;
};

class TE2350AudioProcessorEditor::LogoMark final : public juce::Component
{
public:
    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        const auto centre = bounds.getCentre();
        g.setColour(panel());
        g.fillRoundedRectangle(bounds, 7.0f);
        const auto orbit = juce::Rectangle<float>(30.0f, 13.0f).withCentre(centre);
        for (const auto angle : { -0.72f, 0.72f })
        {
            juce::Graphics::ScopedSaveState state(g);
            g.addTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
            g.setColour(orange());
            g.drawEllipse(orbit, 1.25f);
        }
        g.setColour(textMain());
        g.fillEllipse(juce::Rectangle<float>(4.0f, 4.0f).withCentre(centre));
    }
};

TE2350AudioProcessorEditor::TE2350AudioProcessorEditor(TE2350AudioProcessor& processorRef)
    : AudioProcessorEditor(&processorRef),
      processor(processorRef),
      userPresetManager(processorRef.apvts),
      gravityField(processorRef)
{
    setLookAndFeel(&lookAndFeel);

    auto addOwned = [this] (std::unique_ptr<juce::Component> component) -> juce::Component*
    {
        auto* raw = component.get();
        ownedComponents.push_back(std::move(component));
        return raw;
    };

    logoMark = static_cast<LogoMark*>(addOwned(std::make_unique<LogoMark>()));

    corePanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("QUICK CONTROLS")));
    advancedPanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("")));
    utilityPanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("")));
    inputMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Input", orange())));
    outputMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Output", orange())));
    feedbackMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Feedback request", orange(), true)));

    macroLayer.setComponentID("panel-macros");
    corePanel->setComponentID("panel-perform");
    advancedPanel->setComponentID("panel-sculpt");
    utilityPanel->setComponentID("panel-system");

    addAndMakeVisible(logoMark);
    addAndMakeVisible(macroLayer);
    addAndMakeVisible(gravityField);
    addAndMakeVisible(corePanel);

    corePanel->addAndMakeVisible(coreLayer);
    advancedPanel->addAndMakeVisible(advancedLayer);
    utilityPanel->addAndMakeVisible(utilityLayer);
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(feedbackMeter);

    presetSelector.onChange = [this]
    {
        if (rebuildingPresetSelector)
            return;

        const auto selectedID = presetSelector.getSelectedId();
        if (selectedID > 0 && selectedID < userPresetIDBase)
        {
            selectedUserPreset = -1;
            processor.setCurrentProgram(selectedID - 1);
            observedProgramIndex = processor.getCurrentProgram();
            loadedPresetSnapshot = processor.getPresetBaseline().createCopy();
            presetDirty = false;
            rebuildPresetSelector();
            updatePresetStatus();
        }
        else if (selectedID >= userPresetIDBase && selectedID < embeddedUserPresetID)
        {
            loadUserPreset(selectedID - userPresetIDBase);
        }
    };
    presetSelector.setName("Preset browser");
    presetSelector.setTitle("Preset browser");
    presetSelector.setDescription("Browse categorized factory presets and presets saved by the user.");
    presetSelector.setComponentID("preset-selector");
    presetSelector.setWantsKeyboardFocus(true);
    addAndMakeVisible(presetSelector);

    presetCaption.setText("FACTORY PRESET", juce::dontSendNotification);
    presetCaption.setFont(uiFont(9.0f, juce::Font::bold));
    presetCaption.setColour(juce::Label::textColourId, panel());
    presetCaption.setJustificationType(juce::Justification::centredLeft);
    presetCaption.setAccessible(false);
    addAndMakeVisible(presetCaption);

    presetActionsButton.setColour(juce::TextButton::buttonOnColourId, orange());
    presetActionsButton.setComponentID("preset-actions");
    presetActionsButton.setName("Preset actions");
    presetActionsButton.setTitle("Preset actions");
    presetActionsButton.setDescription("Save or delete user presets, undo, redo, and choose mutation depth.");
    presetActionsButton.setTooltip("Preset actions, Undo/Redo, and mutation depth.");
    presetActionsButton.setWantsKeyboardFocus(true);
    presetActionsButton.onClick = [this] { showPresetActionsMenu(); };
    addAndMakeVisible(presetActionsButton);

    for (auto* button : { &abButton, &resetButton, &advancedToggle })
    {
        button->setClickingTogglesState(false);
        button->setWantsKeyboardFocus(true);
        addAndMakeVisible(button);
    }

    advancedToggle.setColour(juce::TextButton::buttonOnColourId, orange());
    advancedToggle.setComponentID("view-toggle");
    advancedToggle.setName("Sculpt view");
    advancedToggle.setTitle("Sculpt view");
    advancedToggle.setDescription("Switch between the performance and detailed sound-design views.");

    abButton.setColour(juce::TextButton::buttonOnColourId, orange());
    abButton.setComponentID("snapshot-toggle");
    abButton.setName("A/B snapshot");
    abButton.setTitle("A/B snapshot");
    abButton.setDescription("Compare two temporary parameter snapshots.");

    resetButton.setComponentID("reset-button");
    resetButton.setName("Reset parameters");
    resetButton.setTitle("Reset parameters");
    resetButton.setDescription("Restore every parameter to its declared default value.");

    bypassButton.setButtonText("BYPASS");
    bypassButton.setColour(juce::TextButton::buttonOnColourId, orange());
    bypassButton.setComponentID("bypass-button");
    bypassButton.setName("Bypass");
    bypassButton.setTitle("Bypass");
    bypassButton.setDescription("Crossfade to the latency-aligned dry signal.");
    bypassButton.setWantsKeyboardFocus(true);
    addAndMakeVisible(bypassButton);
    buttonAttachments.push_back(std::make_unique<ButtonAttachment>(processor.apvts, "bypass", bypassButton));

    presetSelector.setTooltip("Browse factory and user presets.");
    advancedToggle.setTooltip("Open the detailed sound-design view.");
    abButton.setTooltip("A/B: save the current side, then recall the other temporary snapshot.");
    resetButton.setTooltip("Reset all parameters to their default values.");
    bypassButton.setTooltip("Bypass the effect.");

    abButton.onClick = [this] { toggleAB(); };
    resetButton.onClick = [this] { resetParametersToDefault(); };
    advancedToggle.onClick = [this]
    {
        advancedExpanded = ! advancedExpanded;
        advancedToggle.setButtonText(advancedExpanded ? "SCULPT" : "PERFORM");
        advancedToggle.setToggleState(advancedExpanded, juce::dontSendNotification);
        advancedToggle.setTooltip(advancedExpanded ? "Return to the performance view."
                                                   : "Open the detailed sound-design view.");
        resized();
    };

    addSlider(macroLayer, macroControls, "space", "Space", "Expands delay time, diffusion, width and atmospheric depth.", orange(), true, "%", 100.0, 0);
    addSlider(macroLayer, macroControls, "wild", "Wild", "Introduces instability, modulation, chaos and regenerative feedback.", orange(), true, "%", 100.0, 0);
    addSlider(macroLayer, macroControls, "bloom", "Bloom", "Extends and opens the tail while adding shimmer and ducking.", orange(), true, "%", 100.0, 0);

    auto& timeSlider = addSlider(coreLayer, corePrimaryControls,
                                 "timeMs", "Time", "delay line", orange(), true, " ms", 1.0, 0);
    timeControl = timeSlider.getParentComponent();
    addSlider(coreLayer, corePrimaryControls, "feedback", "Feedback", "regeneration", orange(), true, "%", 100.0, 0);
    addSlider(coreLayer, corePrimaryControls, "mix", "Mix", "dry / wet", orange(), true, "%", 100.0, 0);
    addSlider(advancedLayer, advancedColorControls, "diffusion", "Diffusion", "cloud density", orange(), false, "%", 100.0, 0);
    addSlider(coreLayer, corePrimaryControls, "highCutHz", "Tone", "Wet high-cut / brightness; frequency mapping follows the voiced core filter.", orange(), true, " kHz", 0.001, 1);
    addSlider(advancedLayer, advancedColorControls, "lowCutHz", "Low Cut", "Removes low-frequency energy from the wet signal.", orange(), false, " Hz", 1.0, 0);
    addSlider(coreLayer, corePrimaryControls, "wetWidth", "Width", "stereo field", orange(), true, "%", 100.0, 0);
    syncSelector = &addCombo(coreLayer, coreSecondaryControls, "syncMode", "Sync", "Host tempo division; Free enables manual Time.");
    syncSelector->setName("Time sync");
    syncSelector->setTitle("Time sync");

    addSlider(advancedLayer, advancedMotionControls, "modRateHz", "Mod Rate", "slow orbit", orange(), false, " Hz", 1.0, 2);
    addSlider(advancedLayer, advancedMotionControls, "modDepth", "Mod Depth", "field bend", orange(), false, "%", 100.0, 0);
    addCombo(advancedLayer, advancedMotionControls, "modShape", "Mod Shape", "movement");
    addSlider(advancedLayer, advancedMotionControls, "chaos", "Chaos", "diffusion random", orange(), false, "%", 100.0, 0);
    addSlider(advancedLayer, advancedColorControls, "presence", "Presence", "front detail", orange(), false, "%", 100.0, 0);
    addSlider(advancedLayer, advancedMotionControls, "wobble", "Drift", "tape gravity", orange(), false, "%", 100.0, 0);
    addSlider(advancedLayer, advancedTextureControls, "shimmerAmount", "Shimmer", "octave veil", orange(), false, "%", 100.0, 0);
    auto& shimmerFeedback = addSlider(advancedLayer, advancedTextureControls,
                                      "shimmerFeedback", "Regen", "Amount of pitched signal fed back into the tail.",
                                      orange(), false, "%", 100.0, 0);
    shimmerFeedbackControl = shimmerFeedback.getParentComponent();
    auto& shimmerInterval = addCombo(advancedLayer, advancedTextureControls,
                                     "shimmerInterval", "Interval", "Pitch interval of the shimmer and regeneration lanes.");
    shimmerIntervalControl = shimmerInterval.getParentComponent();
    addSlider(advancedLayer, advancedDynamicsControls, "duckAmount", "Ducking", "clear centre", orange(), false, "%", 100.0, 0);
    auto& duckThreshold = addSlider(advancedLayer, advancedDynamicsControls,
                                    "duckThreshold", "Duck Threshold", "trigger level",
                                    orange(), false, " dB", 1.0, 1);
    duckThresholdControl = duckThreshold.getParentComponent();
    addToggle(advancedLayer, advancedFreezeControls, "freezeEngage", "Freeze", orange(),
              [this]
              {
                  if (const auto* mode = processor.apvts.getRawParameterValue("freezeMode"))
                      return mode->load() < 0.5f;
                  return false;
              });
    auto& freezeMode = addCombo(advancedLayer, advancedFreezeControls,
                                "freezeMode", "Freeze Mode", "hold / latch");
    freezeMode.onChange = [this]
    {
        const auto* mode = processor.apvts.getRawParameterValue("freezeMode");
        if (mode == nullptr || mode->load() >= 0.5f)
            return;

        if (auto* freeze = processor.apvts.getParameter("freezeEngage");
            freeze != nullptr && freeze->getValue() >= 0.5f)
        {
            freeze->beginChangeGesture();
            freeze->setValueNotifyingHost(0.0f);
            freeze->endChangeGesture();
        }
    };

    addFader(utilityLayer, utilityControls, "inputTrim", "Input", "trim", orange(), " dB", 1.0, 1);
    addFader(utilityLayer, utilityControls, "outputTrim", "Output", "trim", orange(), " dB", 1.0, 1);
    addToggle(utilityLayer, utilityControls, "killDry", "Kill Dry", orange()).setTooltip("Removes the direct signal inside the effect mix. Useful on aux/send buses.");
    addToggle(utilityLayer, utilityControls, "atmosFdnOn", "Atmos FDN", orange());
    addCombo(utilityLayer, utilityControls, "qualityMode", "Engine", "Hardware: Original fixed-point character. Studio: 2x oversampled output colour path.");

    addToggle(coreLayer, macroPerformanceControls, "freezeEngage", "Freeze", orange(),
              [this]
              {
                  if (const auto* mode = processor.apvts.getRawParameterValue("freezeMode"))
                      return mode->load() < 0.5f;
                  return false;
              });
    mutateButton.setColour(juce::TextButton::buttonOnColourId, orange());
    mutateButton.setComponentID("mutate-button");
    mutateButton.setName("Mutate");
    mutateButton.setTitle("Mutate");
    mutateButton.setDescription("Create a protected musical variation while preserving timing, mix, I/O, engine, and bypass.");
    mutateButton.setTooltip("Create a 20% musical variation. Timing, Mix, I/O, Engine, Bypass, and Freeze stay locked.");
    mutateButton.setWantsKeyboardFocus(true);
    mutateButton.onClick = [this]
    {
        processor.mutateParameters(0.20f);
        presetDirty = true;
        updatePresetStatus();
    };
    addAndMakeVisible(mutateButton);

    snapshotA = processor.apvts.copyState();
    snapshotB = snapshotA.createCopy();
    loadedPresetSnapshot = processor.getPresetBaseline().createCopy();

    resetButton.setVisible(false);
    advancedViewport.setComponentID("advanced-drawer");
    addAndMakeVisible(advancedViewport);
    advancedViewport.setViewedComponent(&advancedContent, false);
    advancedViewport.setScrollBarsShown(true, false);
    advancedContent.addAndMakeVisible(advancedPanel);
    advancedPanel->setVisible(advancedExpanded);
    advancedContent.addAndMakeVisible(utilityPanel);
    for (const auto& name : { "MOTION", "SHIMMER", "COLOR", "DYNAMICS", "FREEZE", "ENGINE / SYSTEM" })
    {
        auto label = std::make_unique<GroupLabel>(name);
        auto& layer = advancedGroupLabels.size() == 5 ? utilityLayer : advancedLayer;
        layer.addAndMakeVisible(label.get());
        advancedGroupLabels.push_back(label.get());
        ownedComponents.push_back(std::move(label));
    }
    gravityField.refresh();
    updateDependentControls();
    observedProgramIndex = processor.getCurrentProgram();
    rebuildPresetSelector();
    updatePresetStatus();

    setResizeLimits(960, 680, 1280, 820);
    setResizable(true, true);
    setSize(1040, 680);
    timerCallback();
    startTimerHz(30);
}

TE2350AudioProcessorEditor::~TE2350AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void TE2350AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(chassis());
    g.setColour(panel());
    g.setFont(uiFont(31.0f, juce::Font::bold));
    g.drawText("TE-2350", 77, 13, 182, 37, juce::Justification::centredLeft);
    g.setFont(readoutFont(13.0f));
    g.drawText("ANTIGRAVITY", 79, 48, 180, 19, juce::Justification::centredLeft);

    g.setColour(divider());
    g.drawHorizontalLine(77, static_cast<float>(margin), static_cast<float>(getWidth() - margin));
    drawPanel(g, meterBounds.toFloat());
    g.setColour(panelLine());
    for (int i = 1; i < 3; ++i)
        g.drawVerticalLine(meterBounds.getX() + i * meterBounds.getWidth() / 3,
                           static_cast<float>(meterBounds.getY() + 9),
                           static_cast<float>(meterBounds.getBottom() - 9));
}

void TE2350AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(margin);
    auto header = area.removeFromTop(54);
    logoMark->setBounds(header.removeFromLeft(48).reduced(0, 3));
    auto right = header.removeFromRight(juce::jmin(680, getWidth() - 280));
    bypassButton.setBounds(right.removeFromRight(83).reduced(3, 10));
    advancedToggle.setBounds(right.removeFromRight(96).reduced(3, 10));
    abButton.setBounds(right.removeFromRight(40).reduced(3, 10));
    mutateButton.setBounds(right.removeFromRight(78).reduced(3, 10));
    presetActionsButton.setBounds(right.removeFromRight(34).reduced(2, 10));
    presetCaption.setBounds(right.removeFromBottom(15).reduced(4, 0));
    presetSelector.setBounds(right.reduced(4, 4));

    area.removeFromTop(19);
    meterBounds = area.removeFromBottom(43);
    auto footer = meterBounds.reduced(0, 6);
    const auto meterWidth = footer.getWidth() / 3;
    inputMeter->setBounds(footer.removeFromLeft(meterWidth).reduced(14, 0));
    outputMeter->setBounds(footer.removeFromLeft(meterWidth).reduced(14, 0));
    feedbackMeter->setBounds(footer.reduced(14, 0));
    area.removeFromBottom(gridGap);

    advancedViewport.setVisible(advancedExpanded);
    advancedPanel->setVisible(advancedExpanded);
    if (advancedExpanded)
    {
        advancedViewport.setBounds(area.removeFromBottom(188 + (getHeight() - 680) / 2));
        area.removeFromBottom(gridGap);
    }

    auto quick = area.removeFromBottom(advancedExpanded ? 151 : 174);
    corePanel->setBounds(quick);
    coreLayer.setBounds(corePanel->getContentBounds());
    layoutCoreControls(coreLayer.getLocalBounds());
    area.removeFromBottom(gridGap);

    // The deck gives the three manual macros equal weight alongside the field.
    gravityField.setBounds(area.removeFromRight(area.getWidth() * 34 / 100));
    area.removeFromRight(gridGap);
    macroLayer.setBounds(area);
    layoutGrid(macroLayer.getLocalBounds(), macroControls, 3);

    constexpr int rowHeight = 112;
    constexpr int sectionGap = 6;
    constexpr int sculptHeight = 5 * (rowHeight + sectionGap);
    const auto contentWidth = getWidth() - 2 * margin - advancedViewport.getScrollBarThickness();
    advancedContent.setSize(contentWidth, sculptHeight + rowHeight);
    advancedPanel->setBounds(0, 0, contentWidth, sculptHeight - sectionGap);
    advancedLayer.setBounds(advancedPanel->getLocalBounds());
    layoutAdvancedControls(advancedLayer.getLocalBounds());
    utilityPanel->setBounds(0, sculptHeight, contentWidth, rowHeight);
    utilityLayer.setBounds(utilityPanel->getLocalBounds().reduced(8));
    auto utility = utilityLayer.getLocalBounds();
    advancedGroupLabels.back()->setBounds(utility.removeFromLeft(136));
    layoutGrid(utility, utilityControls, 5);
}

juce::Slider& TE2350AudioProcessorEditor::addSlider(juce::Component& parent,
                                                    std::vector<juce::Component*>& group,
                                                    const juce::String& parameterID,
                                                    const juce::String& title,
                                                    const juce::String& subtitle,
                                                    juce::Colour accent,
                                                    bool large,
                                                    const juce::String& suffix,
                                                    double displayScale,
                                                    int decimalPlaces)
{
    auto control = std::make_unique<AntigravityKnob>(title, subtitle, accent, large,
                                              &parent == &macroLayer,
                                              suffix, displayScale, decimalPlaces);
    auto* raw = control.get();
    parent.addAndMakeVisible(raw);
    raw->setComponentID(parameterID);
    jassert(processor.apvts.getParameter(parameterID) != nullptr);
    enableDefaultReset(raw->getSlider(), processor.apvts.getParameter(parameterID));
    raw->getSlider().getProperties().set("parameterID", parameterID);
    if (parameterID == "timeMs" || parameterID == "feedback" || parameterID == "highCutHz"
        || parameterID == "diffusion" || parameterID == "wetWidth" || parameterID == "shimmerAmount"
        || parameterID == "duckAmount")
    {
        auto& slider = raw->getSlider();
        slider.setTooltip(slider.getTooltip() + " Thin outer arc shows effective processor value.");
        effectiveSliders.push_back(&slider);
    }

    if (parameterID == "feedback")
    {
        raw->getSlider().getProperties().set("warningThresholdValue", 0.86);
    }
    else if (parameterID == "shimmerFeedback")
    {
        raw->getSlider().getProperties().set("warningThresholdValue", 0.76);
    }

    sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.apvts, parameterID, raw->getSlider()));
    raw->configureReadout();
    group.push_back(raw);
    ownedComponents.push_back(std::move(control));
    return raw->getSlider();
}

juce::Slider& TE2350AudioProcessorEditor::addFader(juce::Component& parent,
                                                   std::vector<juce::Component*>& group,
                                                   const juce::String& parameterID,
                                                   const juce::String& title,
                                                   const juce::String& subtitle,
                                                   juce::Colour accent,
                                                   const juce::String& suffix,
                                                   double displayScale,
                                                   int decimalPlaces)
{
    auto control = std::make_unique<AntigravityFader>(title, subtitle, accent,
                                               suffix, displayScale, decimalPlaces);
    auto* raw = control.get();
    parent.addAndMakeVisible(raw);
    raw->setComponentID(parameterID);
    jassert(processor.apvts.getParameter(parameterID) != nullptr);
    enableDefaultReset(raw->getSlider(), processor.apvts.getParameter(parameterID));
    raw->getSlider().getProperties().set("parameterID", parameterID);
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.apvts, parameterID, raw->getSlider()));
    raw->configureReadout();
    group.push_back(raw);
    ownedComponents.push_back(std::move(control));
    return raw->getSlider();
}

juce::ComboBox& TE2350AudioProcessorEditor::addCombo(juce::Component& parent,
                                                     std::vector<juce::Component*>& group,
                                                     const juce::String& parameterID,
                                                     const juce::String& title,
                                                     const juce::String& subtitle)
{
    auto control = std::make_unique<ComboTile>(title, subtitle);
    auto* raw = control.get();
    parent.addAndMakeVisible(raw);
    raw->setComponentID(parameterID);
    jassert(processor.apvts.getParameter(parameterID) != nullptr);
    populateComboChoices(raw->getCombo(), processor.apvts.getParameter(parameterID));
    comboAttachments.push_back(std::make_unique<ComboAttachment>(processor.apvts, parameterID, raw->getCombo()));
    group.push_back(raw);
    ownedComponents.push_back(std::move(control));
    return raw->getCombo();
}

juce::Button& TE2350AudioProcessorEditor::addToggle(juce::Component& parent,
                                                    std::vector<juce::Component*>& group,
                                                    const juce::String& parameterID,
                                                    const juce::String& title,
                                                    juce::Colour accent,
                                                    std::function<bool()> isMomentary)
{
    auto control = std::make_unique<ToggleTile>(title, accent);
    auto* raw = control.get();
    raw->setMomentaryPredicate(std::move(isMomentary));
    parent.addAndMakeVisible(raw);
    raw->setComponentID(parameterID);
    jassert(processor.apvts.getParameter(parameterID) != nullptr);
    buttonAttachments.push_back(std::make_unique<ButtonAttachment>(processor.apvts, parameterID, raw->getButton()));
    group.push_back(raw);
    ownedComponents.push_back(std::move(control));
    return raw->getButton();
}

void TE2350AudioProcessorEditor::layoutGrid(juce::Rectangle<int> area,
                                            const std::vector<juce::Component*>& group,
                                            int columns)
{
    if (group.empty())
        return;

    columns = juce::jmax(1, columns);
    const auto rows = (static_cast<int>(group.size()) + columns - 1) / columns;
    const auto cellW = (area.getWidth() - gridGap * (columns - 1)) / columns;
    const auto cellH = (area.getHeight() - gridGap * (rows - 1)) / rows;

    for (int i = 0; i < static_cast<int>(group.size()); ++i)
    {
        const auto row = i / columns;
        const auto column = i % columns;
        group[static_cast<size_t>(i)]->setBounds(area.getX() + column * (cellW + gridGap),
                                                 area.getY() + row * (cellH + gridGap),
                                                 cellW,
                                                 cellH);
    }
}

void TE2350AudioProcessorEditor::layoutCoreControls(juce::Rectangle<int> area)
{
    auto freezeArea = area.removeFromRight(116);
    layoutGrid(freezeArea.withSizeKeepingCentre(108, 69), macroPerformanceControls, 1);
    const auto width = area.getWidth() / static_cast<int>(corePrimaryControls.size());
    for (size_t i = 0; i < corePrimaryControls.size(); ++i)
    {
        auto cell = area.removeFromLeft(width);
        if (i == 0)
        {
            auto sync = cell.removeFromBottom(25);
            coreSecondaryControls.front()->setBounds(sync.withSizeKeepingCentre(110, 25));
        }
        else
        {
            cell.removeFromBottom(25);
        }
        corePrimaryControls[i]->setBounds(cell);
    }
}

void TE2350AudioProcessorEditor::layoutAdvancedControls(juce::Rectangle<int> area)
{
    const std::vector<juce::Component*>* groups[] = {
        &advancedMotionControls, &advancedTextureControls, &advancedColorControls,
        &advancedDynamicsControls, &advancedFreezeControls
    };
    for (size_t i = 0; i < std::size(groups); ++i)
    {
        auto row = area.removeFromTop(112).reduced(8);
        advancedGroupLabels[i]->setBounds(row.removeFromLeft(136));
        layoutGrid(row, *groups[i], 5);
        area.removeFromTop(6);
    }
}

void TE2350AudioProcessorEditor::resetParametersToDefault()
{
    processor.beginUndoTransaction("Reset parameters");
    for (auto* parameter : processor.getParameters())
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->getDefaultValue());
        parameter->endChangeGesture();
    }
    processor.apvts.copyState();

    snapshotA = processor.apvts.copyState();
    snapshotB = snapshotA.createCopy();
    showingSnapshotA = true;
    abButton.setButtonText("A");
    abButton.setToggleState(false, juce::dontSendNotification);
    presetDirty = hasPresetChanges();
    updatePresetStatus();
}

void TE2350AudioProcessorEditor::toggleAB()
{
    if (showingSnapshotA)
    {
        snapshotA = processor.apvts.copyState();
        applySnapshot(snapshotB);
        abButton.setButtonText("B");
    }
    else
    {
        snapshotB = processor.apvts.copyState();
        applySnapshot(snapshotA);
        abButton.setButtonText("A");
    }

    showingSnapshotA = ! showingSnapshotA;
    abButton.setToggleState(! showingSnapshotA, juce::dontSendNotification);
}

void TE2350AudioProcessorEditor::applySnapshot(const juce::ValueTree& snapshot)
{
    if (snapshot.isValid())
        processor.apvts.replaceState(snapshot.createCopy());
}

void TE2350AudioProcessorEditor::rebuildPresetSelector()
{
    const juce::ScopedValueSetter<bool> rebuilding(rebuildingPresetSelector, true);
    presetSelector.clear(juce::dontSendNotification);

    const auto& descriptors = te2350::getFactoryPresetDescriptors();
    juce::StringArray categories;
    for (const auto& descriptor : descriptors)
        categories.addIfNotAlreadyThere(descriptor.category);
    presetSelector.addSectionHeading("FACTORY");

    for (const auto& category : categories)
    {
        auto categoryHasItems = false;
        for (const auto& descriptor : descriptors)
            categoryHasItems = categoryHasItems || descriptor.category == category;

        if (!categoryHasItems)
            continue;

        presetSelector.addSectionHeading(category);
        for (int index = 0; index < static_cast<int>(descriptors.size()); ++index)
            if (descriptors[static_cast<size_t>(index)].category == category)
                presetSelector.addItem(descriptors[static_cast<size_t>(index)].name, index + 1);
    }

    const auto& userPresets = userPresetManager.refresh();
    if (!userPresets.empty())
    {
        presetSelector.addSeparator();
        presetSelector.addSectionHeading("USER PRESETS");
        for (int index = 0; index < static_cast<int>(userPresets.size()); ++index)
            presetSelector.addItem(userPresets[static_cast<size_t>(index)].name,
                                   userPresetIDBase + index);
    }

    if (processor.isActivePresetUser())
    {
        selectedUserPreset = -1;
        for (int index = 0; index < static_cast<int>(userPresets.size()); ++index)
        {
            if (userPresets[static_cast<size_t>(index)].name.equalsIgnoreCase(
                    processor.getActivePresetName()))
            {
                selectedUserPreset = index;
                break;
            }
        }

        if (selectedUserPreset >= 0)
        {
            presetSelector.setSelectedId(userPresetIDBase + selectedUserPreset,
                                         juce::dontSendNotification);
            presetSelector.setTooltip(
                "User preset: " + userPresets[static_cast<size_t>(selectedUserPreset)].name);
        }
        else
        {
            presetSelector.addSeparator();
            presetSelector.addItem(processor.getActivePresetName() + " (Embedded)",
                                   embeddedUserPresetID);
            presetSelector.setSelectedId(embeddedUserPresetID, juce::dontSendNotification);
            presetSelector.setTooltip(
                "This user preset is embedded in the project but is not present in the local library.");
        }
    }
    else
    {
        selectedUserPreset = -1;
        const auto programIndex = juce::jlimit(
            0, juce::jmax(0, static_cast<int>(descriptors.size()) - 1),
            processor.getCurrentProgram());
        presetSelector.setSelectedId(programIndex + 1, juce::dontSendNotification);
        if (juce::isPositiveAndBelow(programIndex, static_cast<int>(descriptors.size())))
            presetSelector.setTooltip(
                descriptors[static_cast<size_t>(programIndex)].description);
    }
}

void TE2350AudioProcessorEditor::showPresetActionsMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Save Preset As...");
    menu.addItem(6, "Save", selectedUserPreset >= 0);
    menu.addItem(7, "Rename...", selectedUserPreset >= 0);
    menu.addItem(8, "Import Preset...");
    menu.addItem(9, "Export Preset...", selectedUserPreset >= 0);
    menu.addItem(14, "Previous Preset");
    menu.addItem(15, "Next Preset");
    menu.addItem(13, "Reload Preset", !processor.isActivePresetUser() || selectedUserPreset >= 0);
    menu.addItem(2, "Delete selected user preset...",
                 selectedUserPreset >= 0);
    menu.addSeparator();

    auto undoLabel = juce::String("Undo");
    if (processor.getUndoDescription().isNotEmpty())
        undoLabel << " " << processor.getUndoDescription();
    auto redoLabel = juce::String("Redo");
    if (processor.getRedoDescription().isNotEmpty())
        redoLabel << " " << processor.getRedoDescription();
    menu.addItem(3, undoLabel, processor.canUndo());
    menu.addItem(4, redoLabel, processor.canRedo());
    menu.addItem(5, "Reset all parameters to defaults");
    menu.addSeparator();

    juce::PopupMenu mutationMenu;
    mutationMenu.addItem(10, "Gentle - 10%");
    mutationMenu.addItem(11, "Musical - 20%");
    mutationMenu.addItem(12, "Deep - 35%");
    menu.addSubMenu("Mutate current sound", mutationMenu);

    auto safeThis = juce::Component::SafePointer<TE2350AudioProcessorEditor>(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&presetActionsButton),
        [safeThis] (int result)
        {
            if (safeThis == nullptr || result == 0)
                return;

            if (result == 1)
                safeThis->promptToSaveUserPreset();
            else if (result == 2)
                safeThis->confirmDeleteUserPreset();
            else if (result == 6)
                safeThis->saveUserPreset(safeThis->processor.getActivePresetName(), true);
            else if (result == 7)
                safeThis->promptToSaveUserPreset(true);
            else if (result == 8 || result == 9)
                safeThis->choosePresetFile(result == 8);
            else if (result == 14 || result == 15) {
                const auto factoryCount = safeThis->processor.getNumPrograms();
                const auto count = factoryCount + static_cast<int>(safeThis->userPresetManager.getPresets().size());
                const auto current = safeThis->processor.isActivePresetUser() && safeThis->selectedUserPreset >= 0
                    ? factoryCount + safeThis->selectedUserPreset : safeThis->processor.getCurrentProgram();
                const auto next = (current + (result == 15 ? 1 : count - 1)) % count;
                if (next >= factoryCount) safeThis->loadUserPreset(next - factoryCount);
                else { safeThis->processor.setCurrentProgram(next); safeThis->rebuildPresetSelector(); }
            }
            else if (result == 13)
            {
                if (safeThis->selectedUserPreset >= 0) safeThis->loadUserPreset(safeThis->selectedUserPreset);
                else {
                    safeThis->processor.setCurrentProgram(safeThis->processor.getCurrentProgram());
                    safeThis->loadedPresetSnapshot = safeThis->processor.getPresetBaseline().createCopy();
                    safeThis->rebuildPresetSelector();
                }
            }
            else if (result == 3)
                safeThis->processor.undo();
            else if (result == 4)
                safeThis->processor.redo();
            else if (result == 5)
                safeThis->resetParametersToDefault();
            else if (result >= 10 && result <= 12)
            {
                constexpr float depths[] { 0.10f, 0.20f, 0.35f };
                safeThis->processor.mutateParameters(depths[result - 10]);
                safeThis->presetDirty = true;
                safeThis->updatePresetStatus();
            }
        });
}

void TE2350AudioProcessorEditor::promptToSaveUserPreset(bool rename)
{
    savePresetDialog = std::make_unique<juce::AlertWindow>(
        rename ? "Rename User Preset" : "Save User Preset",
        "Give this sound a memorable name.",
        juce::MessageBoxIconType::NoIcon,
        this);
    savePresetDialog->addTextEditor(
        "presetName",
        processor.getActivePresetName() + (presetDirty ? " Variation" : juce::String()),
        "Preset name");
    savePresetDialog->addButton("SAVE", 1, juce::KeyPress(juce::KeyPress::returnKey));
    savePresetDialog->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    auto safeThis = juce::Component::SafePointer<TE2350AudioProcessorEditor>(this);
    savePresetDialog->enterModalState(
        true,
        juce::ModalCallbackFunction::create(
            [safeThis, rename] (int result)
            {
                if (safeThis == nullptr)
                    return;

                if (safeThis->savePresetDialog == nullptr)
                    return;

                auto& dialog = *safeThis->savePresetDialog;
                dialog.exitModalState(result);
                dialog.setVisible(false);
                const auto name = dialog.getTextEditorContents("presetName");

                if (result != 1)
                    return;

                if (rename) {
                    const auto outcome = safeThis->userPresetManager.rename(safeThis->selectedUserPreset, name);
                    if (outcome.failed()) safeThis->showPresetError(outcome.getErrorMessage());
                    else { safeThis->processor.renameActiveUserPreset(te2350::UserPresetManager::sanitiseName(name)); safeThis->rebuildPresetSelector(); }
                } else safeThis->saveUserPreset(name, false);
            }),
        false);
}

void TE2350AudioProcessorEditor::saveUserPreset(const juce::String& name, bool overwrite)
{
    if (!overwrite && userPresetManager.findByName(name) >= 0) {
        auto safeThis = juce::Component::SafePointer<TE2350AudioProcessorEditor>(this);
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,
            "Overwrite User Preset", "Replace the saved user preset?", "Replace", "Cancel", this,
            juce::ModalCallbackFunction::create([safeThis, name](int result) {
                if (safeThis != nullptr && result == 1) safeThis->saveUserPreset(name, true);
            }));
        return;
    }
    const auto result = userPresetManager.save(name, overwrite);
    if (result.failed()) { showPresetError(result.getErrorMessage()); return; }
    processor.setActiveUserPreset(te2350::UserPresetManager::sanitiseName(name));
    loadedPresetSnapshot = processor.getPresetBaseline().createCopy();
    presetDirty = false;
    rebuildPresetSelector();
    updatePresetStatus();
}

void TE2350AudioProcessorEditor::choosePresetFile(bool importing)
{
    presetFileChooser = std::make_unique<juce::FileChooser>(importing ? "Import Preset" : "Export Preset",
        juce::File(), "*.te2350preset");
    auto safeThis = juce::Component::SafePointer<TE2350AudioProcessorEditor>(this);
    const auto index = selectedUserPreset;
    presetFileChooser->launchAsync(importing ? (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles)
        : (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting),
        [safeThis, importing, index](const juce::FileChooser& chooser) {
            if (safeThis == nullptr || chooser.getResult() == juce::File()) return;
            const auto result = importing ? safeThis->userPresetManager.importPreset(chooser.getResult())
                : safeThis->userPresetManager.exportPreset(index, chooser.getResult().withFileExtension(".te2350preset"));
            if (result.failed()) safeThis->showPresetError(result.getErrorMessage());
            else safeThis->rebuildPresetSelector();
        });
}

void TE2350AudioProcessorEditor::confirmDeleteUserPreset()
{
    const auto& presets = userPresetManager.getPresets();
    if (!juce::isPositiveAndBelow(selectedUserPreset, static_cast<int>(presets.size())))
        return;

    const auto presetName = presets[static_cast<size_t>(selectedUserPreset)].name;
    const auto presetIndex = selectedUserPreset;
    auto safeThis = juce::Component::SafePointer<TE2350AudioProcessorEditor>(this);
    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::WarningIcon,
        "Delete User Preset",
        "Delete \"" + presetName + "\" from the local preset library?\n\n"
        "The current sound will remain loaded in this project.",
        "DELETE",
        "CANCEL",
        this,
        juce::ModalCallbackFunction::create(
            [safeThis, presetIndex] (int result)
            {
                if (safeThis == nullptr || result == 0)
                    return;

                const auto removeResult = safeThis->userPresetManager.remove(presetIndex);
                if (removeResult.failed())
                {
                    safeThis->showPresetError(removeResult.getErrorMessage());
                    return;
                }

                safeThis->selectedUserPreset = -1;
                safeThis->rebuildPresetSelector();
                safeThis->updatePresetStatus();
            }));
}

void TE2350AudioProcessorEditor::loadUserPreset(int index)
{
    const auto& presets = userPresetManager.getPresets();
    if (!juce::isPositiveAndBelow(index, static_cast<int>(presets.size())))
        return;

    const auto presetName = presets[static_cast<size_t>(index)].name;
    processor.beginUndoTransaction("Load " + presetName);
    const auto result = userPresetManager.load(index);
    if (result.failed())
    {
        showPresetError(result.getErrorMessage());
        rebuildPresetSelector();
        return;
    }

    processor.setActiveUserPreset(presetName);
    selectedUserPreset = index;
    loadedPresetSnapshot = processor.getPresetBaseline().createCopy();
    presetDirty = false;
    rebuildPresetSelector();
    updatePresetStatus();
}

void TE2350AudioProcessorEditor::showPresetError(const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::WarningIcon,
        "Preset Error",
        message,
        "OK",
        this);
}

bool TE2350AudioProcessorEditor::hasPresetChanges() const
{
    return processor.isPresetModified();
}

void TE2350AudioProcessorEditor::updatePresetStatus()
{
    auto label = processor.isActivePresetUser() ? juce::String("USER") : juce::String("FACTORY");
    if (!processor.isActivePresetUser()) {
        const auto& descriptors = te2350::getFactoryPresetDescriptors();
        const auto index = processor.getCurrentProgram();
        if (juce::isPositiveAndBelow(index, static_cast<int>(descriptors.size())))
            label = descriptors[static_cast<size_t>(index)].category;
    }
    if (presetDirty) label << " / MODIFIED";

    presetCaption.setText(label, juce::dontSendNotification);
    presetCaption.setFont(uiFont(10.0f, presetDirty ? juce::Font::bold : juce::Font::plain));
}

bool TE2350AudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    const auto commandDown = key.getModifiers().isCommandDown();
    const auto character = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    if (commandDown && character == 'z')
    {
        if (key.getModifiers().isShiftDown())
            processor.redo();
        else
            processor.undo();
        return true;
    }

    if (commandDown && character == 'y')
    {
        processor.redo();
        return true;
    }

    return juce::AudioProcessorEditor::keyPressed(key);
}

void TE2350AudioProcessorEditor::setControlAvailable(juce::Component* component, bool available)
{
    if (component == nullptr)
        return;

    component->setEnabled(available);
    component->setAlpha(available ? 1.0f : 0.38f);
}

void TE2350AudioProcessorEditor::updateDependentControls()
{
    const auto read = [this] (juce::StringRef parameterID)
    {
        if (const auto* raw = processor.apvts.getRawParameterValue(parameterID))
            return raw->load();
        return 0.0f;
    };

    const auto syncIsFree = read("syncMode") < 0.5f;
    const auto effectiveShimmer = processor.getEffectiveShimmerMeterValue();
    const auto effectiveDucking = processor.getEffectiveDuckingMeterValue();

    setControlAvailable(timeControl, syncIsFree);
    setControlAvailable(shimmerFeedbackControl, effectiveShimmer > 0.001f);
    setControlAvailable(shimmerIntervalControl, effectiveShimmer > 0.001f);
    setControlAvailable(duckThresholdControl, effectiveDucking > 0.001f);
}

void TE2350AudioProcessorEditor::timerCallback()
{
    inputMeter->setLevel(processor.getInputMeterValue());
    outputMeter->setLevel(processor.getOutputMeterValue());

    feedbackMeter->setLevel(processor.getEffectiveFeedbackMeterValue() / 1.05f);
    if (gravityField.isShowing())
        gravityField.refresh();

    const auto programIndex = processor.getCurrentProgram();
    const auto selectorRepresentsUserPreset =
        presetSelector.getSelectedId() >= userPresetIDBase;
    if (observedProgramIndex != programIndex
        || selectorRepresentsUserPreset != processor.isActivePresetUser()
        || (presetSelector.getText() != processor.getActivePresetName()
            && presetSelector.getText() != processor.getActivePresetName() + " (Embedded)"))
    {
        observedProgramIndex = programIndex;
        selectedUserPreset = -1;
        rebuildPresetSelector();
        loadedPresetSnapshot = processor.getPresetBaseline().createCopy();
        presetDirty = hasPresetChanges();
        updatePresetStatus();
    }
    else
    {
        const auto changed = hasPresetChanges();
        if (changed != presetDirty)
        {
            presetDirty = changed;
            updatePresetStatus();
        }
    }

    for (auto* slider : effectiveSliders)
    {
        const auto id = slider->getProperties()["parameterID"].toString();
        const auto effective = processor.getEffectiveControlValue(id);
        const auto proportion = juce::jlimit(0.0, 1.0, slider->valueToProportionOfLength(effective));
        if (slider->getProperties()["effectiveProportion"] != juce::var(proportion))
        {
            slider->getProperties().set("effectiveProportion", proportion);
            slider->repaint();
        }
    }
    updateDependentControls();
}
