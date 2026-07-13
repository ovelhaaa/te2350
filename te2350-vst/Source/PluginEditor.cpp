#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"

#include <cmath>

namespace
{
constexpr int margin = 16;

juce::Colour night()      { return juce::Colour(0xff06080e); }
juce::Colour panel()      { return juce::Colour(0xff0d1420); }
juce::Colour panelLine()  { return juce::Colour(0xff203248); }
juce::Colour textMain()   { return juce::Colour(0xffe8f5ff); }
juce::Colour textMuted()  { return juce::Colour(0xff8192a9); }
juce::Colour cyan()       { return juce::Colour(0xff27d9ff); }
juce::Colour violet()     { return juce::Colour(0xff9b62ff); }
juce::Colour amber()      { return juce::Colour(0xffffb451); }
juce::Colour spectral()   { return juce::Colour(0xff62f0b1); }

juce::Font uiFont(float size, int style = juce::Font::plain)
{
    return juce::Font("Arial Narrow", size, style);
}

void populateComboChoices(juce::ComboBox& combo, juce::RangedAudioParameter* parameter)
{
    combo.clear(juce::dontSendNotification);

    if (parameter == nullptr)
        return;

    const auto choices = parameter->getAllValueStrings();
    for (int index = 0; index < choices.size(); ++index)
        combo.addItem(choices[index], index + 1);
}

}

class TE2350AudioProcessorEditor::OrbitalLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    OrbitalLookAndFeel()
    {
        setColour(juce::Label::textColourId, textMain());
        setColour(juce::ComboBox::textColourId, textMain());
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0a1019));
        setColour(juce::ComboBox::outlineColourId, panelLine());
        setColour(juce::ComboBox::arrowColourId, cyan());
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff0b111b));
        setColour(juce::PopupMenu::textColourId, textMain());
        setColour(juce::TextButton::textColourOffId, textMain());
        setColour(juce::TextButton::textColourOnId, night());
        setColour(juce::Slider::textBoxTextColourId, textMain());
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                            static_cast<float>(width), static_cast<float>(height)).reduced(7.0f);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour(juce::Colour(0xff05080d));
        g.fillEllipse(bounds);

        juce::ColourGradient glow(accent.withAlpha(0.32f), centre.x, centre.y,
                                  juce::Colours::transparentBlack, centre.x + radius, centre.y + radius, true);
        g.setGradientFill(glow);
        g.fillEllipse(bounds.expanded(5.0f));

        g.setColour(panelLine().withAlpha(0.92f));
        g.drawEllipse(bounds, 1.2f);

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius * 0.78f, radius * 0.78f, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff172538));
        g.strokePath(track, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc(centre.x, centre.y, radius * 0.78f, radius * 0.78f, 0.0f,
                            rotaryStartAngle, angle, true);
        g.setColour(accent);
        g.strokePath(value, juce::PathStrokeType(5.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        g.setColour(accent.withAlpha(0.20f));
        g.drawEllipse(bounds.reduced(radius * 0.18f), 1.0f);

        const auto pointerLength = radius * 0.50f;
        const auto pointerThickness = juce::jmax(2.2f, radius * 0.055f);
        juce::Path pointer;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness,
                                    pointerLength * 0.58f, pointerThickness * 0.5f);
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
        g.setColour(textMain().withAlpha(0.92f));
        g.fillPath(pointer);

        g.setColour(accent.withAlpha(0.82f));
        g.fillEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre(centre));
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour&,
                              bool highlighted,
                              bool down) override
    {
        auto r = button.getLocalBounds().toFloat().reduced(0.5f);
        const auto active = button.getToggleState();
        const auto accent = active ? spectral() : cyan();
        const auto base = active ? accent.withAlpha(0.86f) : juce::Colour(0xff0b111b);

        g.setColour(base.brighter(down ? 0.08f : 0.0f));
        g.fillRoundedRectangle(r, 4.0f);

        g.setColour((highlighted || active ? accent : panelLine()).withAlpha(highlighted ? 0.95f : 0.70f));
        g.drawRoundedRectangle(r, 4.0f, 1.2f);
    }

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool highlighted,
                          bool down) override
    {
        drawButtonBackground(g, button, juce::Colours::transparentBlack, highlighted, down);

        g.setColour(button.getToggleState() ? night() : textMain());
        g.setFont(uiFont(12.0f, juce::Font::bold));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4),
                         juce::Justification::centred, 1);
    }

    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool highlighted,
                      int,
                      int,
                      int,
                      int,
                      juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float>(0.5f, 0.5f,
                                       static_cast<float>(width) - 1.0f,
                                       static_cast<float>(height) - 1.0f);
        g.setColour(juce::Colour(0xff08101a));
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour((highlighted ? cyan() : panelLine()).withAlpha(highlighted ? 0.92f : 0.74f));
        g.drawRoundedRectangle(r, 4.0f, 1.1f);

        juce::Path arrow;
        const auto cx = static_cast<float>(width) - 15.0f;
        const auto cy = static_cast<float>(height) * 0.52f;
        arrow.addTriangle(cx - 4.5f, cy - 2.5f, cx + 4.5f, cy - 2.5f, cx, cy + 3.5f);
        g.setColour(cyan().withAlpha(0.86f));
        g.fillPath(arrow);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(9, 1, box.getWidth() - 28, box.getHeight() - 2);
        label.setFont(uiFont(13.0f, juce::Font::bold));
        label.setJustificationType(juce::Justification::centredLeft);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        return uiFont(juce::jmin(13.0f, static_cast<float>(buttonHeight) * 0.45f), juce::Font::bold);
    }
};

class TE2350AudioProcessorEditor::SectionPanel final : public juce::Component
{
public:
    SectionPanel(juce::String titleText, juce::String codeText)
        : title(std::move(titleText)), code(std::move(codeText))
    {
    }

    juce::Rectangle<int> getContentBounds() const
    {
        return getLocalBounds().reduced(12).withTrimmedTop(22);
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        juce::ColourGradient bg(panel().withAlpha(0.96f), r.getX(), r.getY(),
                                juce::Colour(0xff090d16), r.getRight(), r.getBottom(), false);
        bg.addColour(0.62, juce::Colour(0xff101529));
        g.setGradientFill(bg);
        g.fillRoundedRectangle(r, 8.0f);

        g.setColour(panelLine().withAlpha(0.82f));
        g.drawRoundedRectangle(r, 8.0f, 1.0f);

        g.setColour(cyan().withAlpha(0.50f));
        g.drawLine(r.getX() + 12.0f, r.getY() + 25.0f, r.getRight() - 12.0f, r.getY() + 25.0f, 1.0f);

        g.setFont(uiFont(12.0f, juce::Font::bold));
        g.setColour(textMain());
        g.drawText(title.toUpperCase(), 12, 4, getWidth() - 24, 18, juce::Justification::centredLeft);

        g.setColour(textMuted());
        g.drawText(code, 12, 4, getWidth() - 24, 18, juce::Justification::centredRight);
    }

private:
    juce::String title;
    juce::String code;
};

class TE2350AudioProcessorEditor::KnobTile final : public juce::Component
{
public:
    KnobTile(juce::String titleText, juce::String subtitleText, juce::Colour accentColour, bool macro)
        : title(std::move(titleText)), subtitle(std::move(subtitleText)), accent(accentColour), isMacro(macro)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setColour(juce::Slider::textBoxTextColourId, textMain());
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible(slider);
    }

    juce::Slider& getSlider() { return slider; }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(isMacro ? 3.0f : 4.0f);
        g.setColour(juce::Colour(0xff080d16).withAlpha(isMacro ? 0.64f : 0.38f));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(accent.withAlpha(isMacro ? 0.58f : 0.28f));
        g.drawRoundedRectangle(r, 7.0f, isMacro ? 1.2f : 0.9f);

        auto labelArea = getLocalBounds().reduced(isMacro ? 10 : 7);

        g.setColour(textMain());
        g.setFont(uiFont(isMacro ? 20.0f : 12.5f, juce::Font::bold));
        g.drawText(title.toUpperCase(), labelArea.removeFromTop(isMacro ? 25 : 17),
                   juce::Justification::centred);

        g.setColour(textMuted());
        g.setFont(uiFont(isMacro ? 10.5f : 9.5f, juce::Font::plain));
        g.drawText(subtitle.toUpperCase(), labelArea.removeFromTop(isMacro ? 14 : 12),
                   juce::Justification::centred);

        auto value = getLocalBounds().reduced(isMacro ? 17 : 10).removeFromBottom(isMacro ? 20 : 17).toFloat();
        g.setColour(juce::Colour(0xff05080d).withAlpha(0.72f));
        g.fillRoundedRectangle(value, 4.0f);
        g.setColour(accent.withAlpha(0.42f));
        g.drawRoundedRectangle(value, 4.0f, 0.8f);
        g.setColour(textMain().withAlpha(0.88f));
        g.setFont(uiFont(isMacro ? 11.0f : 9.5f, juce::Font::bold));
        g.drawText(slider.getTextFromValue(slider.getValue()), value.toNearestInt().reduced(4, 0),
                   juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(isMacro ? 8 : 5);
        area.removeFromTop(isMacro ? 43 : 31);
        area.removeFromBottom(isMacro ? 23 : 20);

        const auto side = juce::jmax(32, juce::jmin(area.getWidth(), area.getHeight()));
        slider.setBounds(juce::Rectangle<int>(side, side).withCentre(area.getCentre()));
    }

private:
    juce::String title;
    juce::String subtitle;
    juce::Colour accent;
    bool isMacro = false;
    juce::Slider slider;
};

class TE2350AudioProcessorEditor::ComboTile final : public juce::Component
{
public:
    ComboTile(juce::String titleText, juce::String subtitleText)
        : title(std::move(titleText)), subtitle(std::move(subtitleText))
    {
        addAndMakeVisible(combo);
    }

    juce::ComboBox& getCombo() { return combo; }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(4.0f);
        g.setColour(juce::Colour(0xff080d16).withAlpha(0.36f));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(panelLine().withAlpha(0.54f));
        g.drawRoundedRectangle(r, 7.0f, 0.9f);

        g.setColour(textMain());
        g.setFont(uiFont(12.0f, juce::Font::bold));
        g.drawText(title.toUpperCase(), 7, 7, getWidth() - 14, 14, juce::Justification::centred);

        g.setColour(textMuted());
        g.setFont(uiFont(9.5f));
        g.drawText(subtitle.toUpperCase(), 7, 21, getWidth() - 14, 12, juce::Justification::centred);
    }

    void resized() override
    {
        combo.setBounds(getLocalBounds().reduced(11).removeFromBottom(27));
    }

private:
    juce::String title;
    juce::String subtitle;
    juce::ComboBox combo;
};

class TE2350AudioProcessorEditor::ToggleTile final : public juce::Component
{
public:
    ToggleTile(juce::String titleText, juce::Colour accentColour)
        : accent(accentColour)
    {
        button.setButtonText(std::move(titleText));
        addAndMakeVisible(button);
    }

    juce::Button& getButton() { return button; }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(4.0f);
        g.setColour(juce::Colour(0xff080d16).withAlpha(0.35f));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(accent.withAlpha(0.30f));
        g.drawRoundedRectangle(r, 7.0f, 0.9f);
    }

    void resized() override
    {
        button.setBounds(getLocalBounds().reduced(12).withSizeKeepingCentre(getWidth() - 24, 28));
    }

private:
    juce::Colour accent;
    juce::ToggleButton button;
};

class TE2350AudioProcessorEditor::MeterStrip final : public juce::Component
{
public:
    MeterStrip(juce::String labelText, juce::Colour meterColour)
        : label(std::move(labelText)), colour(meterColour)
    {
    }

    void setLevel(float newLevel)
    {
        level = juce::jlimit(0.0f, 1.0f, newLevel);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(textMuted());
        g.setFont(uiFont(10.0f, juce::Font::bold));
        g.drawText(label.toUpperCase(), r.removeFromTop(13.0f).toNearestInt(), juce::Justification::centredLeft);

        auto track = r.reduced(0.0f, 5.0f);
        g.setColour(juce::Colour(0xff05080d));
        g.fillRoundedRectangle(track, 3.0f);

        auto fill = track;
        fill.setWidth(track.getWidth() * level);
        juce::ColourGradient grad(colour.withAlpha(0.76f), track.getX(), track.getCentreY(),
                                  colour.brighter(0.55f), track.getRight(), track.getCentreY(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fill, 3.0f);

        g.setColour(panelLine().withAlpha(0.82f));
        g.drawRoundedRectangle(track, 3.0f, 1.0f);
    }

private:
    juce::String label;
    juce::Colour colour;
    float level = 0.0f;
};

class TE2350AudioProcessorEditor::GravityMeter final : public juce::Component
{
public:
    void setValues(float instabilityValue, float feedbackValue, float phaseValue)
    {
        instability = juce::jlimit(0.0f, 1.0f, instabilityValue);
        feedback = juce::jlimit(0.0f, 1.05f, feedbackValue) / 1.05f;
        phase = phaseValue;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(4.0f);
        const auto size = juce::jmin(r.getWidth(), r.getHeight());
        auto orbit = juce::Rectangle<float>(size, size).withCentre(r.getCentre());
        auto centre = orbit.getCentre();
        const auto radius = size * 0.39f;

        g.setColour(juce::Colour(0xff05080d));
        g.fillEllipse(orbit);
        g.setColour(panelLine().withAlpha(0.72f));
        g.drawEllipse(orbit, 1.0f);
        g.setColour(cyan().withAlpha(0.18f));
        g.drawEllipse(orbit.reduced(size * 0.17f), 1.0f);

        juce::Path field;
        constexpr int points = 150;
        for (int i = 0; i <= points; ++i)
        {
            const auto t = juce::MathConstants<float>::twoPi * static_cast<float>(i) / static_cast<float>(points);
            const auto wobble = 1.0f + 0.12f * std::sin(t * 5.0f + phase);
            const auto x = centre.x + std::sin(t * 2.0f + phase * 0.45f) * radius * wobble;
            const auto y = centre.y + std::sin(t * 3.0f + phase) * radius * wobble;
            if (i == 0)
                field.startNewSubPath(x, y);
            else
                field.lineTo(x, y);
        }

        g.setColour(violet().withAlpha(0.16f + instability * 0.32f));
        g.strokePath(field, juce::PathStrokeType(2.0f + instability * 2.0f));
        g.setColour(spectral().withAlpha(0.40f + instability * 0.45f));
        g.strokePath(field, juce::PathStrokeType(1.0f));

        const auto moonAngle = phase * 0.55f;
        const auto moon = juce::Point<float>(centre.x + std::cos(moonAngle) * radius * 0.82f,
                                             centre.y + std::sin(moonAngle) * radius * 0.58f);
        g.setColour(amber().withAlpha(0.88f));
        g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre(moon));

        g.setColour(textMain());
        g.setFont(uiFont(11.0f, juce::Font::bold));
        g.drawText("GRAVITY", getLocalBounds().removeFromTop(15), juce::Justification::centred);
        g.setColour(textMuted());
        g.setFont(uiFont(10.0f));
        g.drawText(juce::String(juce::roundToInt(instability * 100.0f)) + "% / " +
                   juce::String(juce::roundToInt(feedback * 100.0f)) + "%",
                   getLocalBounds().removeFromBottom(15), juce::Justification::centred);
    }

private:
    float instability = 0.0f;
    float feedback = 0.0f;
    float phase = 0.0f;
};

class TE2350AudioProcessorEditor::LogoMark final : public juce::Component
{
public:
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(3.0f);
        auto centre = r.getCentre();
        const auto rx = r.getWidth() * 0.42f;
        const auto ry = r.getHeight() * 0.33f;

        g.setColour(juce::Colour(0xff05080d));
        g.fillEllipse(r);
        g.setColour(panelLine().withAlpha(0.74f));
        g.drawEllipse(r, 1.0f);

        for (int pass = 0; pass < 3; ++pass)
        {
            juce::Path path;
            constexpr int points = 120;
            for (int i = 0; i <= points; ++i)
            {
                const auto t = juce::MathConstants<float>::twoPi * static_cast<float>(i) / static_cast<float>(points);
                const auto x = centre.x + std::sin(t * (2.0f + static_cast<float>(pass))) * rx;
                const auto y = centre.y + std::sin(t * (3.0f + static_cast<float>(pass)) + 0.7f) * ry;
                if (i == 0)
                    path.startNewSubPath(x, y);
                else
                    path.lineTo(x, y);
            }

            g.setColour((pass == 0 ? cyan() : pass == 1 ? violet() : spectral()).withAlpha(0.48f));
            g.strokePath(path, juce::PathStrokeType(pass == 0 ? 1.6f : 1.0f));
        }

        g.setColour(textMain().withAlpha(0.85f));
        g.fillEllipse(juce::Rectangle<float>(4.0f, 4.0f).withCentre(centre));
    }
};

TE2350AudioProcessorEditor::TE2350AudioProcessorEditor(TE2350AudioProcessor& processorRef)
    : AudioProcessorEditor(&processorRef), processor(processorRef)
{
    orbitalLookAndFeel = new OrbitalLookAndFeel();
    setLookAndFeel(orbitalLookAndFeel);

    auto addOwned = [this] (std::unique_ptr<juce::Component> component) -> juce::Component*
    {
        auto* raw = component.get();
        ownedComponents.push_back(std::move(component));
        return raw;
    };

    logoMark = static_cast<LogoMark*>(addOwned(std::make_unique<LogoMark>()));
    macroPanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("Macro Orbit", "M-01")));
    corePanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("Delay / Reverb Layer", "D-2350")));
    advancedPanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("Advanced Field", "X-02")));
    utilityPanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("Telemetry", "IO / SAFETY")));
    gravityMeter = static_cast<GravityMeter*>(addOwned(std::make_unique<GravityMeter>()));
    inputMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Input", cyan())));
    outputMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Output", spectral())));
    feedbackMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Feedback Safety", amber())));

    addAndMakeVisible(logoMark);
    addAndMakeVisible(macroPanel);
    addAndMakeVisible(corePanel);
    addAndMakeVisible(advancedPanel);
    addAndMakeVisible(utilityPanel);

    macroPanel->addAndMakeVisible(macroLayer);
    corePanel->addAndMakeVisible(coreLayer);
    advancedPanel->addAndMakeVisible(advancedLayer);
    utilityPanel->addAndMakeVisible(utilityLayer);
    utilityPanel->addAndMakeVisible(gravityMeter);
    utilityPanel->addAndMakeVisible(inputMeter);
    utilityPanel->addAndMakeVisible(outputMeter);
    utilityPanel->addAndMakeVisible(feedbackMeter);

    presetSelector.addItemList(te2350::getFactoryPresetNames(), 1);
    presetSelector.setSelectedId(processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetSelector.onChange = [this]
    {
        const auto selected = presetSelector.getSelectedId() - 1;
        if (selected >= 0)
            processor.setCurrentProgram(selected);
    };
    addAndMakeVisible(presetSelector);

    for (auto* button : { &abButton, &resetButton, &advancedToggle })
    {
        button->setClickingTogglesState(false);
        addAndMakeVisible(button);
    }

    bypassButton.setButtonText("BYP");
    addAndMakeVisible(bypassButton);
    buttonAttachments.push_back(std::make_unique<ButtonAttachment>(processor.apvts, "bypass", bypassButton));

    abButton.onClick = [this] { toggleAB(); };
    resetButton.onClick = [this] { resetParametersToDefault(); };
    advancedToggle.onClick = [this]
    {
        advancedExpanded = ! advancedExpanded;
        advancedPanel->setVisible(advancedExpanded);
        resized();
    };

    addSlider(macroLayer, macroControls, "space", "Space", "depth / size", cyan(), true);
    addSlider(macroLayer, macroControls, "wild", "Wild", "instability", violet(), true);
    addSlider(macroLayer, macroControls, "bloom", "Bloom", "tail / expand", amber(), true);

    addSlider(coreLayer, coreControls, "timeMs", "Time", "delay line", cyan());
    addSlider(coreLayer, coreControls, "feedback", "Feedback", "regeneration", amber());
    addSlider(coreLayer, coreControls, "mix", "Mix", "dry / wet", spectral());
    addSlider(coreLayer, coreControls, "diffusion", "Diffusion", "cloud density", violet());
    addSlider(coreLayer, coreControls, "highCutHz", "Tone", "air / damp", spectral());
    addSlider(coreLayer, coreControls, "lowCutHz", "Damp", "low orbit", amber());
    addSlider(coreLayer, coreControls, "wetWidth", "Width", "stereo field", cyan());
    addCombo(coreLayer, coreControls, "syncMode", "Sync", "host grid");

    addSlider(advancedLayer, advancedControls, "modRateHz", "Mod Rate", "slow orbit", cyan());
    addSlider(advancedLayer, advancedControls, "modDepth", "Mod Depth", "field bend", violet());
    addSlider(advancedLayer, advancedControls, "wobble", "Pitch Drift", "tape gravity", spectral());
    addSlider(advancedLayer, advancedControls, "shimmerAmount", "Shimmer", "octave veil", amber());
    addCombo(advancedLayer, advancedControls, "shimmerInterval", "Octave", "interval");
    addSlider(advancedLayer, advancedControls, "duckAmount", "Ducking", "clear centre", cyan());
    addToggle(advancedLayer, advancedControls, "freezeEngage", "Freeze", spectral());
    addCombo(advancedLayer, advancedControls, "qualityMode", "Oversampling", "mode");

    addSlider(utilityLayer, utilityControls, "inputTrim", "Input", "trim", cyan());
    addSlider(utilityLayer, utilityControls, "outputTrim", "Output", "trim", spectral());
    addToggle(utilityLayer, utilityControls, "killDry", "Kill Dry", amber());
    addToggle(utilityLayer, utilityControls, "atmosFdnOn", "Atmos", violet());

    snapshotA = processor.apvts.copyState();
    snapshotB = snapshotA.createCopy();

    advancedPanel->setVisible(advancedExpanded);

    setResizeLimits(900, 600, 1200, 780);
    setResizable(true, true);
    setSize(1000, 660);
    startTimerHz(30);
}

TE2350AudioProcessorEditor::~TE2350AudioProcessorEditor()
{
    setLookAndFeel(nullptr);
    delete orbitalLookAndFeel;
}

void TE2350AudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient bg(night(), bounds.getCentreX(), 0.0f,
                            juce::Colour(0xff121032), bounds.getRight(), bounds.getBottom(), true);
    bg.addColour(0.42, juce::Colour(0xff071626));
    bg.addColour(0.78, juce::Colour(0xff160d26));
    g.setGradientFill(bg);
    g.fillRect(bounds);

    g.setColour(violet().withAlpha(0.08f));
    g.fillEllipse(bounds.withSizeKeepingCentre(bounds.getWidth() * 0.92f, bounds.getHeight() * 1.10f));

    for (int i = 0; i < 48; ++i)
    {
        const auto x = std::fmod(static_cast<float>(i * 97 + 29), juce::jmax(1.0f, bounds.getWidth()));
        const auto y = std::fmod(static_cast<float>(i * 53 + 41), juce::jmax(1.0f, bounds.getHeight()));
        const auto alpha = 0.07f + 0.18f * (0.5f + 0.5f * std::sin(animationPhase * 0.25f + static_cast<float>(i)));
        g.setColour((i % 4 == 0 ? amber() : i % 3 == 0 ? violet() : cyan()).withAlpha(alpha));
        g.fillEllipse(x, y, i % 5 == 0 ? 1.8f : 1.1f, i % 5 == 0 ? 1.8f : 1.1f);
    }

    auto header = getLocalBounds().reduced(margin).removeFromTop(60);
    g.setColour(textMuted());
    g.setFont(uiFont(11.0f, juce::Font::bold));
    g.drawText("AMBIENT DELAY / REVERB  |  ORBITAL MODULATION UNIT",
               header.withTrimmedLeft(76).removeFromBottom(16), juce::Justification::centredLeft);

    g.setColour(textMain());
    g.setFont(uiFont(27.0f, juce::Font::bold));
    g.drawText("TE-2350 ANTIGRAVITY", header.withTrimmedLeft(76).withTrimmedRight(366),
               juce::Justification::centredLeft);

    g.setColour(cyan().withAlpha(0.58f));
    g.drawHorizontalLine(header.getBottom() + 4, static_cast<float>(margin), static_cast<float>(getWidth() - margin));
}

void TE2350AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(margin);
    auto header = area.removeFromTop(54);

    logoMark->setBounds(header.removeFromLeft(52).reduced(2));
    header.removeFromLeft(12);

    auto headerRight = header.removeFromRight(360);
    bypassButton.setBounds(headerRight.removeFromRight(48).reduced(3, 10));
    resetButton.setBounds(headerRight.removeFromRight(45).reduced(3, 10));
    abButton.setBounds(headerRight.removeFromRight(45).reduced(3, 10));
    advancedToggle.setBounds(headerRight.removeFromRight(48).reduced(3, 10));
    headerRight.removeFromRight(8);
    presetSelector.setBounds(headerRight.removeFromRight(160).reduced(2, 10));

    area.removeFromTop(12);
    auto utility = area.removeFromBottom(124);
    area.removeFromBottom(12);

    const auto leftWidth = juce::jlimit(250, 330, area.getWidth() / 3);
    auto left = area.removeFromLeft(leftWidth);
    area.removeFromLeft(12);

    macroPanel->setBounds(left);
    utilityPanel->setBounds(utility);

    if (advancedExpanded)
    {
        auto core = area.removeFromTop(juce::jmax(220, area.getHeight() / 2 + 20));
        area.removeFromTop(12);
        corePanel->setBounds(core);
        advancedPanel->setBounds(area);
    }
    else
    {
        corePanel->setBounds(area);
    }

    macroLayer.setBounds(macroPanel->getContentBounds());
    coreLayer.setBounds(corePanel->getContentBounds());
    advancedLayer.setBounds(advancedPanel->getContentBounds());

    layoutMacroControls(macroLayer.getLocalBounds());
    const auto coreColumns = coreLayer.getWidth() >= 620 ? 4 : 3;
    const auto advancedColumns = advancedLayer.getWidth() >= 600 ? 4 : 3;
    layoutGrid(coreLayer.getLocalBounds(), coreControls, coreColumns);
    layoutGrid(advancedLayer.getLocalBounds(), advancedControls, advancedColumns);

    auto utilityContent = utilityPanel->getContentBounds();
    auto meterArea = utilityContent.removeFromRight(utilityContent.getWidth() / 2);
    utilityContent.removeFromRight(10);
    layoutGrid(utilityContent, utilityControls, 2);

    gravityMeter->setBounds(meterArea.removeFromRight(86).reduced(2));
    meterArea.removeFromRight(8);
    inputMeter->setBounds(meterArea.removeFromTop(meterArea.getHeight() / 3).reduced(0, 1));
    outputMeter->setBounds(meterArea.removeFromTop(meterArea.getHeight() / 2).reduced(0, 1));
    feedbackMeter->setBounds(meterArea.reduced(0, 1));
}

juce::Slider& TE2350AudioProcessorEditor::addSlider(juce::Component& parent,
                                                    std::vector<juce::Component*>& group,
                                                    const juce::String& parameterID,
                                                    const juce::String& title,
                                                    const juce::String& subtitle,
                                                    juce::Colour accent,
                                                    bool large)
{
    auto control = std::make_unique<KnobTile>(title, subtitle, accent, large);
    auto* raw = control.get();
    parent.addAndMakeVisible(raw);
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.apvts, parameterID, raw->getSlider()));
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
                                                    juce::Colour accent)
{
    auto control = std::make_unique<ToggleTile>(title, accent);
    auto* raw = control.get();
    parent.addAndMakeVisible(raw);
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
    constexpr int gap = 8;
    const auto cellW = (area.getWidth() - gap * (columns - 1)) / columns;
    const auto cellH = (area.getHeight() - gap * (rows - 1)) / rows;

    for (int i = 0; i < static_cast<int>(group.size()); ++i)
    {
        const auto row = i / columns;
        const auto column = i % columns;
        group[static_cast<size_t>(i)]->setBounds(area.getX() + column * (cellW + gap),
                                                 area.getY() + row * (cellH + gap),
                                                 cellW,
                                                 cellH);
    }
}

void TE2350AudioProcessorEditor::layoutMacroControls(juce::Rectangle<int> area)
{
    if (macroControls.empty())
        return;

    const auto gap = 9;
    const auto cellH = (area.getHeight() - gap * 2) / 3;
    for (int i = 0; i < static_cast<int>(macroControls.size()); ++i)
    {
        macroControls[static_cast<size_t>(i)]->setBounds(area.removeFromTop(cellH).reduced(1));
        if (i + 1 < static_cast<int>(macroControls.size()))
            area.removeFromTop(gap);
    }
}

void TE2350AudioProcessorEditor::resetParametersToDefault()
{
    for (auto* parameter : processor.getParameters())
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->getDefaultValue());
        parameter->endChangeGesture();
    }

    snapshotA = processor.apvts.copyState();
    snapshotB = snapshotA.createCopy();
    showingSnapshotA = true;
    abButton.setButtonText("A/B");
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
}

void TE2350AudioProcessorEditor::applySnapshot(const juce::ValueTree& snapshot)
{
    if (snapshot.isValid())
        processor.apvts.replaceState(snapshot.createCopy());
}

void TE2350AudioProcessorEditor::timerCallback()
{
    animationPhase += 0.035f;
    if (animationPhase > juce::MathConstants<float>::twoPi * 100.0f)
        animationPhase = 0.0f;

    inputMeter->setLevel(processor.getInputMeterValue());
    outputMeter->setLevel(processor.getOutputMeterValue());

    auto feedbackValue = 0.0f;
    if (const auto* raw = processor.apvts.getRawParameterValue("feedback"))
        feedbackValue = raw->load();

    feedbackMeter->setLevel(juce::jlimit(0.0f, 1.0f, feedbackValue / 1.05f));
    gravityMeter->setValues(processor.getInstabilityMeterValue(), feedbackValue, animationPhase);
    repaint();
}
