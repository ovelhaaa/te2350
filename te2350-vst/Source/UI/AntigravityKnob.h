#pragma once

#include "AntigravityPalette.h"

namespace te2350::ui
{
inline juce::String controlTooltip(const juce::String& title, const juce::String& detail, bool reset)
{
    auto tip = title;
    if (detail.isNotEmpty())
        tip << " - " << detail;
    if (reset)
        tip << ". Double-click to reset. Ctrl/Command-drag for fine adjustment.";
    return tip;
}

// APVTS supplies the range and gestures. Display units apply only to the editor.
inline void setReadoutFormat(juce::Slider& slider, const juce::String& suffix, double scale, int decimals)
{
    slider.textFromValueFunction = [suffix, scale, decimals] (double value)
    {
        return juce::String(value * scale, decimals) + suffix;
    };
    slider.valueFromTextFunction = [suffix, scale, &slider] (const juce::String& text)
    {
        auto value = text.trim();
        const auto unit = suffix.trim();
        if (unit.isNotEmpty() && value.endsWithIgnoreCase(unit))
            value = value.dropLastCharacters(unit.length()).trimEnd();
        value = value.replaceCharacter(',', '.');
        if (!value.containsAnyOf("0123456789") || !value.containsOnly("0123456789.+-"))
            return slider.getValue();
        return value.getDoubleValue() / scale;
    };
    slider.updateText();
}

class AntigravityKnob final : public juce::Component
{
public:
    AntigravityKnob(juce::String titleText,
             juce::String subtitleText,
             juce::Colour accentColour,
             bool large,
             bool macroCard,
             juce::String suffixText,
             double scale,
             int decimals)
        : title(std::move(titleText)),
          subtitle(std::move(subtitleText)),
          suffix(std::move(suffixText)),
          displayScale(scale),
          decimalPlaces(decimals),
          accent(accentColour),
          isLarge(large),
          useMacroCard(macroCard)
    {
        const auto tooltip = controlTooltip(title, subtitle, true);
        setName(title + " control");
        setTitle(title);
        setDescription(subtitle);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, macroCard ? 82 : 74, 22);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setName(title);
        slider.setTitle(title);
        slider.setDescription(subtitle + ". Double-click to reset.");
        slider.setWantsKeyboardFocus(true);
        slider.setTooltip(tooltip);
        slider.setPopupDisplayEnabled(true, false, nullptr);
        slider.setVelocityModeParameters(1.0, 1, 0.08, true);
        slider.setMouseDragSensitivity(isLarge ? 240 : 180);
        slider.addMouseListener(this, true);
        addAndMakeVisible(slider);
    }

    juce::Slider& getSlider() { return slider; }

    void configureReadout()
    {
        setReadoutFormat(slider, suffix, displayScale, decimalPlaces);
    }

    void mouseEnter(const juce::MouseEvent&) override { slider.repaint(); }
    void mouseExit(const juce::MouseEvent&) override { slider.repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced(8, 4);
        if (useMacroCard)
        {
            drawPanel(g, getLocalBounds().toFloat());
            g.setColour(orange());
            g.fillRect(area.getX() + 4, area.getY() + 11, 3, 12);
            g.setColour(panelLine());
            g.drawHorizontalLine(area.getY() + 33, static_cast<float>(area.getX() + 4),
                                 static_cast<float>(area.getRight() - 4));
        }

        g.setColour(textMain());
        g.setFont(uiFont(useMacroCard ? 18.0f : 12.0f, juce::Font::bold));
        g.drawText(title.toUpperCase(), area.removeFromTop(useMacroCard ? 32 : 17),
                   juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(7, 5);
        area.removeFromTop(useMacroCard ? 34 : 17);
        const auto side = juce::jmin(useMacroCard ? 190 : 90,
                                    juce::jmin(area.getWidth(), area.getHeight() - 22));
        if (side > 0)
            slider.setBounds(area.withSizeKeepingCentre(juce::jmax(side, 84), side + 22));
    }

private:
    juce::String title;
    juce::String subtitle;
    juce::String suffix;
    double displayScale = 1.0;
    int decimalPlaces = 1;
    juce::Colour accent;
    bool isLarge = false;
    bool useMacroCard = false;
    juce::Slider slider;
};

class AntigravityFader final : public juce::Component
{
public:
    AntigravityFader(juce::String titleText,
              juce::String subtitleText,
              juce::Colour accentColour,
              juce::String suffixText,
              double scale,
              int decimals)
        : title(std::move(titleText)),
          subtitle(std::move(subtitleText)),
          suffix(std::move(suffixText)),
          displayScale(scale),
          decimalPlaces(decimals),
          accent(accentColour)
    {
        const auto tooltip = controlTooltip(title, subtitle, true);
        setName(title + " control");
        setTitle(title);
        setDescription(subtitle);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setName(title);
        slider.setTitle(title);
        slider.setDescription(subtitle + ". Double-click to reset.");
        slider.setWantsKeyboardFocus(true);
        slider.setTooltip(tooltip);
        slider.setVelocityModeParameters(1.0, 1, 0.08, true);
        slider.setMouseDragSensitivity(180);
        addAndMakeVisible(slider);
    }

    juce::Slider& getSlider() { return slider; }

    void configureReadout()
    {
        setReadoutFormat(slider, suffix, displayScale, decimalPlaces);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced(8, 6);
        g.setColour(textMain());
        g.setFont(uiFont(12.0f, juce::Font::bold));
        g.drawText(title.toUpperCase() + " TRIM", area.removeFromTop(18),
                   juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(6, 6).withTrimmedTop(21);
        slider.setBounds(area.withSizeKeepingCentre(area.getWidth(), 30));
    }

private:
    juce::String title;
    juce::String subtitle;
    juce::String suffix;
    double displayScale = 1.0;
    int decimalPlaces = 1;
    juce::Colour accent;
    juce::Slider slider;
};

}
