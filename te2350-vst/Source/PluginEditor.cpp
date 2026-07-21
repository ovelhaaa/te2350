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
    return juce::Font("Segoe UI", size, style);
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

void enableDefaultReset(juce::Slider& slider, juce::RangedAudioParameter* parameter)
{
    if (parameter != nullptr)
    {
        slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
        slider.getProperties().set("defaultProportion", parameter->getDefaultValue());
    }
}

juce::String makeControlTooltip(const juce::String& title, const juce::String& subtitle, bool canReset)
{
    auto tip = title;
    if (subtitle.isNotEmpty())
        tip << " - " << subtitle;

    if (canReset)
        tip << ". Double-click to reset.";

    return tip;
}

juce::String formatDisplayValue(double value,
                                double displayScale,
                                int decimalPlaces,
                                const juce::String& suffix,
                                bool showPositiveSign)
{
    const auto displayValue = value * displayScale;
    auto text = juce::String(displayValue, decimalPlaces);
    if (showPositiveSign && displayValue > 0.0)
        text = "+" + text;

    return text + suffix;
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
        auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
        const auto warningThreshold = slider.getProperties()["warningThresholdValue"];
        if (! warningThreshold.isVoid())
        {
            const auto warningValue = static_cast<double>(warningThreshold);
            const auto criticalValue = static_cast<double>(slider.getProperties().getWithDefault("criticalThresholdValue",
                                                                                                  warningValue));
            if (slider.getValue() >= criticalValue)
                accent = juce::Colour(0xffff5c5c);
            else if (slider.getValue() >= warningValue)
                accent = amber();
        }
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

        const auto defaultValue = slider.getProperties()["defaultProportion"];
        if (! defaultValue.isVoid())
        {
            const auto defaultPos = juce::jlimit(0.0f, 1.0f, static_cast<float>(defaultValue));
            const auto defaultAngle = rotaryStartAngle + defaultPos * (rotaryEndAngle - rotaryStartAngle);
            const auto inner = radius * 0.66f;
            const auto outer = radius * 0.86f;
            const auto p1 = centre + juce::Point<float>(std::cos(defaultAngle - juce::MathConstants<float>::halfPi) * inner,
                                                        std::sin(defaultAngle - juce::MathConstants<float>::halfPi) * inner);
            const auto p2 = centre + juce::Point<float>(std::cos(defaultAngle - juce::MathConstants<float>::halfPi) * outer,
                                                        std::sin(defaultAngle - juce::MathConstants<float>::halfPi) * outer);
            g.setColour(textMuted().withAlpha(0.55f));
            g.drawLine({ p1, p2 }, 1.3f);
        }

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
        const auto controlAccent = button.findColour(juce::TextButton::buttonOnColourId);
        const auto accent = active ? controlAccent : cyan();
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

    void drawLinearSlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float,
                          float,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                                                   0.0f, 0.0f, style, slider);
            return;
        }

        auto r = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                        static_cast<float>(width), static_cast<float>(height)).reduced(5.0f, 8.0f);
        const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
        const auto track = r.withHeight(7.0f).withCentre({ r.getCentreX(), r.getCentreY() });
        const auto clampedPos = juce::jlimit(track.getX(), track.getRight(), sliderPos);

        g.setColour(juce::Colour(0xff05080d));
        g.fillRoundedRectangle(track, 3.5f);

        auto fill = track.withRight(clampedPos);
        if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        {
            const auto zeroPos = track.getX()
                + track.getWidth() * static_cast<float>(slider.valueToProportionOfLength(0.0));
            fill = juce::Rectangle<float>(juce::jmin(zeroPos, clampedPos),
                                          track.getY(),
                                          std::abs(clampedPos - zeroPos),
                                          track.getHeight());
        }

        g.setColour(accent.withAlpha(0.82f));
        g.fillRoundedRectangle(fill, 3.5f);

        g.setColour(panelLine().withAlpha(0.82f));
        g.drawRoundedRectangle(track, 3.5f, 1.0f);

        if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        {
            const auto zeroPos = track.getX()
                + track.getWidth() * static_cast<float>(slider.valueToProportionOfLength(0.0));
            g.setColour(textMuted().withAlpha(0.70f));
            g.drawVerticalLine(juce::roundToInt(zeroPos), track.getY() - 4.0f, track.getBottom() + 4.0f);
        }

        const auto defaultValue = slider.getProperties()["defaultProportion"];
        if (! defaultValue.isVoid())
        {
            const auto defaultPos = track.getX()
                + track.getWidth() * juce::jlimit(0.0f, 1.0f, static_cast<float>(defaultValue));
            g.setColour(textMuted().withAlpha(0.50f));
            g.drawVerticalLine(juce::roundToInt(defaultPos), track.getY() - 5.0f, track.getBottom() + 5.0f);
        }

        const auto thumb = juce::Rectangle<float>(10.0f, 10.0f).withCentre({ clampedPos, track.getCentreY() });
        g.setColour(textMain());
        g.fillEllipse(thumb);
        g.setColour(accent.withAlpha(0.72f));
        g.drawEllipse(thumb.expanded(2.0f), 1.0f);
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
        juce::ColourGradient bg(panel().withAlpha(0.94f), r.getX(), r.getY(),
                                juce::Colour(0xff090d16), r.getRight(), r.getBottom(), false);
        bg.addColour(0.62, juce::Colour(0xff0f1421));
        g.setGradientFill(bg);
        g.fillRoundedRectangle(r, 8.0f);

        g.setColour(panelLine().withAlpha(0.62f));
        g.drawRoundedRectangle(r, 8.0f, 1.0f);

        g.setColour(cyan().withAlpha(0.24f));
        g.drawLine(r.getX() + 12.0f, r.getY() + 25.0f, r.getRight() - 12.0f, r.getY() + 25.0f, 1.0f);

        g.setFont(uiFont(12.0f, juce::Font::bold));
        g.setColour(textMain());
        g.drawText(title.toUpperCase(), 12, 4, getWidth() - 24, 18, juce::Justification::centredLeft);

        if (code.isNotEmpty())
        {
            g.setColour(textMuted().withAlpha(0.78f));
            g.drawText(code.toUpperCase(), 12, 4, getWidth() - 24, 18, juce::Justification::centredRight);
        }
    }

private:
    juce::String title;
    juce::String code;
};

class TE2350AudioProcessorEditor::GroupLabel final : public juce::Component
{
public:
    GroupLabel(juce::String labelText, juce::Colour accentColour)
        : label(std::move(labelText)), accent(accentColour)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const auto lineY = r.getCentreY();

        g.setColour(accent.withAlpha(0.72f));
        g.fillRoundedRectangle(r.removeFromLeft(4.0f).reduced(0.0f, 3.0f), 1.0f);

        g.setColour(textMuted().withAlpha(0.92f));
        g.setFont(uiFont(10.0f, juce::Font::bold));
        g.drawText(label.toUpperCase(), getLocalBounds().withTrimmedLeft(10), juce::Justification::centredLeft);

        g.setColour(panelLine().withAlpha(0.34f));
        g.drawLine(static_cast<float>(juce::jmin(92, getWidth() / 3)),
                   lineY,
                   static_cast<float>(getWidth()),
                   lineY,
                   1.0f);
    }

private:
    juce::String label;
    juce::Colour accent;
};

class TE2350AudioProcessorEditor::KnobTile final : public juce::Component
{
public:
    KnobTile(juce::String titleText,
             juce::String subtitleText,
             juce::Colour accentColour,
             bool macro,
             juce::String suffixText,
             double scale,
             int decimals)
        : title(std::move(titleText)),
          subtitle(std::move(subtitleText)),
          suffix(std::move(suffixText)),
          displayScale(scale),
          decimalPlaces(decimals),
          accent(accentColour),
          isMacro(macro)
    {
        const auto tooltip = makeControlTooltip(title, subtitle, true);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setColour(juce::Slider::textBoxTextColourId, textMain());
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setTooltip(tooltip);
        slider.textFromValueFunction = [this] (double value)
        {
            return formatDisplayValue(value, displayScale, decimalPlaces, suffix, false);
        };
        slider.setPopupDisplayEnabled(true, true, nullptr);
        slider.setMouseDragSensitivity(isMacro ? 240 : 180);
        slider.onValueChange = [this] { repaint(); };
        slider.onDragStart = [this] { repaint(); };
        slider.onDragEnd = [this] { repaint(); };
        slider.addMouseListener(this, true);
        addAndMakeVisible(slider);
    }

    juce::Slider& getSlider() { return slider; }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(isMacro ? 3.0f : 4.0f);
        g.setColour(juce::Colour(0xff080d16).withAlpha(isMacro ? 0.64f : 0.38f));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(accent.withAlpha(isMacro ? 0.58f : 0.28f));
        g.drawRoundedRectangle(r, 7.0f, isMacro ? 1.2f : 0.9f);

        auto labelArea = getLocalBounds().reduced(isMacro ? 10 : 7);

        g.setColour(textMain());
        g.setFont(uiFont(isMacro ? 18.5f : 11.5f, juce::Font::bold));
        g.drawFittedText(title.toUpperCase(), labelArea.removeFromTop(isMacro ? 25 : 17),
                         juce::Justification::centred, 1);

        g.setColour(textMuted());
        g.setFont(uiFont(isMacro ? 10.0f : 9.0f, juce::Font::plain));
        g.drawFittedText(subtitle.toUpperCase(), labelArea.removeFromTop(isMacro ? 14 : 12),
                         juce::Justification::centred, 1);

        const auto engaged = isMacro || isMouseOver(true) || slider.isMouseOver(true) || slider.isMouseButtonDown(true);
        const auto valueBgAlpha = engaged ? 0.72f : 0.30f;
        const auto valueBorderAlpha = engaged ? 0.42f : 0.18f;
        const auto valueTextAlpha = engaged ? 0.88f : 0.50f;
        auto value = getLocalBounds().reduced(isMacro ? 17 : 10).removeFromBottom(isMacro ? 20 : 17).toFloat();
        g.setColour(juce::Colour(0xff05080d).withAlpha(valueBgAlpha));
        g.fillRoundedRectangle(value, 4.0f);
        g.setColour(accent.withAlpha(valueBorderAlpha));
        g.drawRoundedRectangle(value, 4.0f, 0.8f);
        g.setColour(textMain().withAlpha(valueTextAlpha));
        g.setFont(uiFont(isMacro ? 11.0f : 9.5f, juce::Font::bold));
        g.drawFittedText(formatValue(), value.toNearestInt().reduced(4, 0),
                         juce::Justification::centred, 1);
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
    juce::String formatValue() const
    {
        return formatDisplayValue(slider.getValue(), displayScale, decimalPlaces, suffix, false);
    }

    juce::String title;
    juce::String subtitle;
    juce::String suffix;
    double displayScale = 1.0;
    int decimalPlaces = 1;
    juce::Colour accent;
    bool isMacro = false;
    juce::Slider slider;
};

class TE2350AudioProcessorEditor::FaderTile final : public juce::Component
{
public:
    FaderTile(juce::String titleText,
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
        const auto tooltip = makeControlTooltip(title, subtitle, true);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setTooltip(tooltip);
        slider.textFromValueFunction = [this] (double value)
        {
            const auto isBipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
            return formatDisplayValue(value, displayScale, decimalPlaces, suffix, isBipolar);
        };
        slider.setPopupDisplayEnabled(true, true, nullptr);
        slider.setMouseDragSensitivity(180);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible(slider);
    }

    juce::Slider& getSlider() { return slider; }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(4.0f);
        g.setColour(juce::Colour(0xff080d16).withAlpha(0.32f));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(accent.withAlpha(0.26f));
        g.drawRoundedRectangle(r, 7.0f, 0.9f);

        auto top = getLocalBounds().reduced(8, 5).removeFromTop(17);
        g.setColour(textMain());
        g.setFont(uiFont(11.5f, juce::Font::bold));
        g.drawText(title.toUpperCase(), top.removeFromLeft(top.getWidth() / 2), juce::Justification::centredLeft);

        g.setColour(textMain().withAlpha(0.88f));
        g.setFont(uiFont(10.5f, juce::Font::bold));
        g.drawFittedText(formatValue(), top, juce::Justification::centredRight, 1);

        g.setColour(textMuted());
        g.setFont(uiFont(9.0f));
        g.drawText(subtitle.toUpperCase(), 8, getHeight() - 17, getWidth() - 16, 12,
                   juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(7, 4);
        area.removeFromTop(20);
        area.removeFromBottom(15);
        slider.setBounds(area);
    }

private:
    juce::String formatValue() const
    {
        const auto isBipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        return formatDisplayValue(slider.getValue(), displayScale, decimalPlaces, suffix, isBipolar);
    }

    juce::String title;
    juce::String subtitle;
    juce::String suffix;
    double displayScale = 1.0;
    int decimalPlaces = 1;
    juce::Colour accent;
    juce::Slider slider;
};

class TE2350AudioProcessorEditor::ComboTile final : public juce::Component
{
public:
    ComboTile(juce::String titleText, juce::String subtitleText)
        : title(std::move(titleText)), subtitle(std::move(subtitleText))
    {
        combo.setTooltip(makeControlTooltip(title, subtitle, false));
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
        g.setFont(uiFont(11.0f, juce::Font::bold));
        g.drawFittedText(title.toUpperCase(), 7, 7, getWidth() - 14, 14,
                         juce::Justification::centred, 1);

        g.setColour(textMuted());
        g.setFont(uiFont(8.8f));
        g.drawFittedText(subtitle.toUpperCase(), 7, 21, getWidth() - 14, 12,
                         juce::Justification::centred, 1);
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
        : title(std::move(titleText)), accent(accentColour)
    {
        button.setTooltip(makeControlTooltip(title, juce::String("Toggle ") + title.toLowerCase(), false));
        button.setColour(juce::TextButton::buttonOnColourId, accent);
        button.onStateChange = [this] { updateButtonText(); repaint(); };
        updateButtonText();
        addAndMakeVisible(button);
    }

    juce::Button& getButton() { return button; }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(4.0f);
        const auto active = button.getToggleState();
        g.setColour(juce::Colour(0xff080d16).withAlpha(0.35f));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour((active ? accent : accent.withAlpha(0.30f)).withAlpha(active ? 0.72f : 0.30f));
        g.drawRoundedRectangle(r, 7.0f, active ? 1.2f : 0.9f);

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
    juce::ToggleButton button;
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
        level = juce::jlimit(0.0f, 1.0f, newLevel);
        repaint();
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
        g.drawFittedText(formatLevel(), labelArea, juce::Justification::centredRight, 1);

        auto track = r.reduced(0.0f, 5.0f);
        g.setColour(juce::Colour(0xff05080d));
        g.fillRoundedRectangle(track, 3.0f);

        auto fill = track;
        fill.setWidth(track.getWidth() * level);
        juce::ColourGradient grad(displayColour.withAlpha(0.76f), track.getX(), track.getCentreY(),
                                  displayColour.brighter(0.55f), track.getRight(), track.getCentreY(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fill, 3.0f);

        if (isWarningMeter)
        {
            const auto warnX = track.getX() + track.getWidth() * 0.82f;
            g.setColour(amber().withAlpha(0.52f));
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

        if (level >= 0.92f)
            return juce::Colour(0xffff5c5c);

        if (level >= 0.82f)
            return amber();

        return spectral();
    }

    juce::String formatLevel() const
    {
        if (isWarningMeter)
            return level >= 0.92f ? "LIMIT" : juce::String(juce::roundToInt(level * 100.0f)) + "%";

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
    macroPanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("Macros", "Performance")));
    corePanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("Delay / Reverb", "Main")));
    advancedPanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("Advanced", "Edit")));
    utilityPanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("I/O", "Levels")));
    gravityMeter = static_cast<GravityMeter*>(addOwned(std::make_unique<GravityMeter>()));
    inputMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Input", cyan())));
    outputMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Output", spectral())));
    feedbackMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Feedback Safety", amber(), true)));
    corePerformLabel = static_cast<GroupLabel*>(addOwned(std::make_unique<GroupLabel>("Perform", cyan())));
    coreColorLabel = static_cast<GroupLabel*>(addOwned(std::make_unique<GroupLabel>("Color", spectral())));
    advancedMotionLabel = static_cast<GroupLabel*>(addOwned(std::make_unique<GroupLabel>("Motion", violet())));
    advancedTextureLabel = static_cast<GroupLabel*>(addOwned(std::make_unique<GroupLabel>("Texture", amber())));

    addAndMakeVisible(logoMark);
    addAndMakeVisible(macroPanel);
    addAndMakeVisible(corePanel);
    addAndMakeVisible(advancedPanel);
    addAndMakeVisible(utilityPanel);

    macroPanel->addAndMakeVisible(macroLayer);
    corePanel->addAndMakeVisible(coreLayer);
    advancedPanel->addAndMakeVisible(advancedLayer);
    utilityPanel->addAndMakeVisible(utilityLayer);
    coreLayer.addAndMakeVisible(corePerformLabel);
    coreLayer.addAndMakeVisible(coreColorLabel);
    advancedLayer.addAndMakeVisible(advancedMotionLabel);
    advancedLayer.addAndMakeVisible(advancedTextureLabel);
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

    advancedToggle.setColour(juce::TextButton::buttonOnColourId, violet());

    bypassButton.setButtonText("BYPASS");
    bypassButton.setColour(juce::TextButton::buttonOnColourId, spectral());
    addAndMakeVisible(bypassButton);
    buttonAttachments.push_back(std::make_unique<ButtonAttachment>(processor.apvts, "bypass", bypassButton));

    presetSelector.setTooltip("Load a factory preset.");
    advancedToggle.setTooltip("Switch to the advanced edit view.");
    abButton.setTooltip("Compare snapshot A and snapshot B.");
    resetButton.setTooltip("Reset all parameters to their default values.");
    bypassButton.setTooltip("Bypass the effect.");

    abButton.onClick = [this] { toggleAB(); };
    resetButton.onClick = [this] { resetParametersToDefault(); };
    advancedToggle.onClick = [this]
    {
        advancedExpanded = ! advancedExpanded;
        advancedToggle.setButtonText(advancedExpanded ? "MAIN" : "EDIT");
        advancedToggle.setToggleState(advancedExpanded, juce::dontSendNotification);
        advancedToggle.setTooltip(advancedExpanded ? "Return to the main performance view."
                                                   : "Switch to the advanced edit view.");
        resized();
    };

    addSlider(macroLayer, macroControls, "space", "Space", "depth / size", cyan(), true, "%", 100.0, 0);
    addSlider(macroLayer, macroControls, "wild", "Wild", "instability", violet(), true, "%", 100.0, 0);
    addSlider(macroLayer, macroControls, "bloom", "Bloom", "tail / expand", amber(), true, "%", 100.0, 0);

    addSlider(coreLayer, corePrimaryControls, "timeMs", "Time", "delay line", cyan(), true, " ms", 1.0, 0);
    addSlider(coreLayer, corePrimaryControls, "feedback", "Feedback", "regeneration", amber(), true, "%", 100.0, 0);
    addSlider(coreLayer, corePrimaryControls, "mix", "Mix", "dry / wet", spectral(), true, "%", 100.0, 0);
    addSlider(coreLayer, coreSecondaryControls, "diffusion", "Diffusion", "cloud density", violet(), false, "%", 100.0, 0);
    addSlider(coreLayer, coreSecondaryControls, "highCutHz", "Tone", "air / damp", spectral(), false, " kHz", 0.001, 1);
    addSlider(coreLayer, coreSecondaryControls, "lowCutHz", "Damp", "low orbit", amber(), false, " Hz", 1.0, 0);
    addSlider(coreLayer, coreSecondaryControls, "wetWidth", "Width", "stereo field", cyan(), false, "%", 100.0, 0);
    addCombo(coreLayer, coreSecondaryControls, "syncMode", "Sync", "host grid");

    addSlider(advancedLayer, advancedMotionControls, "modRateHz", "Mod Rate", "slow orbit", cyan(), false, " Hz", 1.0, 2);
    addSlider(advancedLayer, advancedMotionControls, "modDepth", "Mod Depth", "field bend", violet(), false, "%", 100.0, 0);
    addCombo(advancedLayer, advancedMotionControls, "modShape", "Mod Shape", "movement");
    addSlider(advancedLayer, advancedMotionControls, "chaos", "Chaos", "diffusion random", violet(), false, "%", 100.0, 0);
    addSlider(advancedLayer, advancedMotionControls, "presence", "Presence", "front detail", spectral(), false, "%", 100.0, 0);
    addSlider(advancedLayer, advancedMotionControls, "wobble", "Pitch Drift", "tape gravity", spectral(), false, "%", 100.0, 0);
    addSlider(advancedLayer, advancedTextureControls, "shimmerAmount", "Shimmer", "octave veil", amber(), false, "%", 100.0, 0);
    addSlider(advancedLayer, advancedTextureControls, "shimmerFeedback", "Shimmer FB", "recirculate", amber(), false, "%", 100.0, 0);
    addCombo(advancedLayer, advancedTextureControls, "shimmerInterval", "Octave", "interval");
    addSlider(advancedLayer, advancedTextureControls, "duckAmount", "Ducking", "clear centre", cyan(), false, "%", 100.0, 0);
    addSlider(advancedLayer, advancedTextureControls, "duckThreshold", "Duck Thr", "trigger level", cyan(), false, " dB", 1.0, 1);
    addToggle(advancedLayer, advancedTextureControls, "freezeEngage", "Freeze", spectral());
    addCombo(advancedLayer, advancedTextureControls, "freezeMode", "Freeze Mode", "gesture");
    addCombo(advancedLayer, advancedTextureControls, "qualityMode", "Oversampling", "mode");

    addFader(utilityLayer, utilityControls, "inputTrim", "Input", "trim", cyan(), " dB", 1.0, 1);
    addFader(utilityLayer, utilityControls, "outputTrim", "Output", "trim", spectral(), " dB", 1.0, 1);
    addToggle(utilityLayer, utilityControls, "killDry", "Kill Dry", amber());
    addToggle(utilityLayer, utilityControls, "atmosFdnOn", "Atmos", violet());

    snapshotA = processor.apvts.copyState();
    snapshotB = snapshotA.createCopy();

    corePanel->setVisible(! advancedExpanded);
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

    for (int i = 0; i < 22; ++i)
    {
        const auto x = std::fmod(static_cast<float>(i * 97 + 29), juce::jmax(1.0f, bounds.getWidth()));
        const auto y = std::fmod(static_cast<float>(i * 53 + 41), juce::jmax(1.0f, bounds.getHeight()));
        const auto alpha = 0.03f + 0.08f * (0.5f + 0.5f * std::sin(animationPhase * 0.25f + static_cast<float>(i)));
        g.setColour((i % 4 == 0 ? amber() : i % 3 == 0 ? violet() : cyan()).withAlpha(alpha));
        g.fillEllipse(x, y, i % 5 == 0 ? 1.8f : 1.1f, i % 5 == 0 ? 1.8f : 1.1f);
    }

    auto header = getLocalBounds().reduced(margin).removeFromTop(54);
    g.setColour(textMuted());
    g.setFont(uiFont(11.0f, juce::Font::bold));
    g.drawText("AMBIENT DELAY / REVERB TEXTURE INSTRUMENT",
               header.withTrimmedLeft(76).removeFromBottom(16), juce::Justification::centredLeft);

    g.setColour(textMain());
    g.setFont(uiFont(27.0f, juce::Font::bold));
    g.drawText("TE-2350 ANTIGRAVITY", header.withTrimmedLeft(76).withTrimmedRight(452),
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

    auto headerRight = header.removeFromRight(442);
    bypassButton.setBounds(headerRight.removeFromRight(74).reduced(3, 10));
    resetButton.setBounds(headerRight.removeFromRight(66).reduced(3, 10));
    abButton.setBounds(headerRight.removeFromRight(48).reduced(3, 10));
    advancedToggle.setBounds(headerRight.removeFromRight(62).reduced(3, 10));
    headerRight.removeFromRight(8);
    presetSelector.setBounds(headerRight.removeFromRight(184).reduced(2, 10));

    area.removeFromTop(12);
    auto utility = area.removeFromBottom(124);
    area.removeFromBottom(12);

    const auto leftWidth = juce::jlimit(250, 330, area.getWidth() / 3);
    auto left = area.removeFromLeft(leftWidth);
    area.removeFromLeft(12);

    macroPanel->setBounds(left);
    utilityPanel->setBounds(utility);

    corePanel->setVisible(! advancedExpanded);
    advancedPanel->setVisible(advancedExpanded);
    corePanel->setBounds(area);
    advancedPanel->setBounds(area);

    macroLayer.setBounds(macroPanel->getContentBounds());
    coreLayer.setBounds(corePanel->getContentBounds());
    advancedLayer.setBounds(advancedPanel->getContentBounds());

    layoutMacroControls(macroLayer.getLocalBounds());
    layoutCoreControls(coreLayer.getLocalBounds());
    layoutAdvancedControls(advancedLayer.getLocalBounds());

    auto utilityContent = utilityPanel->getContentBounds();
    auto meterArea = utilityContent.removeFromRight(utilityContent.getWidth() / 2);
    utilityContent.removeFromRight(10);
    layoutGrid(utilityContent, utilityControls, 4);

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
                                                    bool large,
                                                    const juce::String& suffix,
                                                    double displayScale,
                                                    int decimalPlaces)
{
    auto control = std::make_unique<KnobTile>(title, subtitle, accent, large,
                                              suffix, displayScale, decimalPlaces);
    auto* raw = control.get();
    parent.addAndMakeVisible(raw);
    enableDefaultReset(raw->getSlider(), processor.apvts.getParameter(parameterID));

    if (parameterID == "feedback")
    {
        raw->getSlider().getProperties().set("warningThresholdValue", 0.86);
        raw->getSlider().getProperties().set("criticalThresholdValue", 0.97);
    }
    else if (parameterID == "shimmerFeedback")
    {
        raw->getSlider().getProperties().set("warningThresholdValue", 0.76);
        raw->getSlider().getProperties().set("criticalThresholdValue", 0.88);
    }

    sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.apvts, parameterID, raw->getSlider()));
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
    auto control = std::make_unique<FaderTile>(title, subtitle, accent,
                                               suffix, displayScale, decimalPlaces);
    auto* raw = control.get();
    parent.addAndMakeVisible(raw);
    enableDefaultReset(raw->getSlider(), processor.apvts.getParameter(parameterID));
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

void TE2350AudioProcessorEditor::layoutCoreControls(juce::Rectangle<int> area)
{
    if (corePrimaryControls.empty() && coreSecondaryControls.empty())
        return;

    const auto gap = 10;
    auto primaryArea = area.removeFromTop(static_cast<int>(static_cast<float>(area.getHeight()) * 0.58f));
    area.removeFromTop(gap);

    corePerformLabel->setBounds(primaryArea.removeFromTop(16));
    primaryArea.removeFromTop(4);
    coreColorLabel->setBounds(area.removeFromTop(16));
    area.removeFromTop(4);

    layoutGrid(primaryArea, corePrimaryControls, corePrimaryControls.size() >= 3 ? 3 : 1);

    const auto secondaryColumns = area.getWidth() >= 600 ? 5 : 3;
    layoutGrid(area, coreSecondaryControls, secondaryColumns);
}

void TE2350AudioProcessorEditor::layoutAdvancedControls(juce::Rectangle<int> area)
{
    if (advancedMotionControls.empty() && advancedTextureControls.empty())
        return;

    const auto gap = 10;
    auto motionArea = area.removeFromTop(static_cast<int>(static_cast<float>(area.getHeight()) * 0.46f));
    area.removeFromTop(gap);

    advancedMotionLabel->setBounds(motionArea.removeFromTop(16));
    motionArea.removeFromTop(4);
    advancedTextureLabel->setBounds(area.removeFromTop(16));
    area.removeFromTop(4);

    const auto motionColumns = motionArea.getWidth() >= 760 ? 6 : motionArea.getWidth() >= 600 ? 4 : 3;
    const auto textureColumns = area.getWidth() >= 760 ? 4 : area.getWidth() >= 600 ? 4 : 3;

    layoutGrid(motionArea, advancedMotionControls, motionColumns);
    layoutGrid(area, advancedTextureControls, textureColumns);
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
    abButton.setButtonText("A");
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
}
