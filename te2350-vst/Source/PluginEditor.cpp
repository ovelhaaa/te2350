#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"

#include <cmath>

namespace
{
constexpr int margin = 16;
constexpr int gridGap = 8;
constexpr int userPresetIDBase = 1000;
constexpr int embeddedUserPresetID = 9000;

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

int chooseColumns(juce::Rectangle<int> area, int itemCount, int minCellWidth, int maxColumns)
{
    if (itemCount <= 0)
        return 1;

    const auto fit = (area.getWidth() + gridGap) / (minCellWidth + gridGap);
    return juce::jlimit(1, juce::jmin(itemCount, maxColumns), fit);
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
        highlighted = highlighted || button.hasKeyboardFocus(true);
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
        const auto tooltip = makeControlTooltip(title, subtitle, true);
        setName(title + " control");
        setTitle(title);
        setDescription(subtitle);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setColour(juce::Slider::textBoxTextColourId, textMain());
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setName(title);
        slider.setTitle(title);
        slider.setDescription(subtitle + ". Double-click to reset.");
        slider.setWantsKeyboardFocus(true);
        slider.setTooltip(tooltip);
        slider.textFromValueFunction = [this] (double value)
        {
            return formatDisplayValue(value, displayScale, decimalPlaces, suffix, false);
        };
        slider.setPopupDisplayEnabled(true, true, nullptr);
        slider.setMouseDragSensitivity(isLarge ? 240 : 180);
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
        auto r = getLocalBounds().toFloat().reduced(isLarge ? 3.0f : 4.0f);
        const auto focused = slider.hasKeyboardFocus(true);
        g.setColour(juce::Colour(0xff080d16).withAlpha(isLarge ? 0.64f : 0.38f));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(accent.withAlpha(focused ? 0.92f : isLarge ? 0.58f : 0.28f));
        g.drawRoundedRectangle(r, 7.0f, focused ? 1.6f : isLarge ? 1.2f : 0.9f);

        if (useMacroCard)
        {
            auto content = getLocalBounds().reduced(13, 9);
            const auto knobColumn = juce::jlimit(78, 96, content.getWidth() / 3);
            auto textArea = content.withTrimmedRight(knobColumn + 6);

            g.setColour(textMain());
            g.setFont(uiFont(18.0f, juce::Font::bold));
            g.drawFittedText(title.toUpperCase(), textArea.removeFromTop(28),
                             juce::Justification::centredLeft, 1);

            g.setColour(textMuted());
            g.setFont(uiFont(9.0f));
            g.drawFittedText(subtitle.toUpperCase(), textArea.removeFromTop(13),
                             juce::Justification::centredLeft, 1);

            auto value = textArea.removeFromBottom(20).toFloat();
            g.setColour(juce::Colour(0xff05080d).withAlpha(0.72f));
            g.fillRoundedRectangle(value, 4.0f);
            g.setColour(accent.withAlpha(focused ? 0.72f : 0.42f));
            g.drawRoundedRectangle(value, 4.0f, focused ? 1.2f : 0.8f);
            g.setColour(textMain().withAlpha(0.96f));
            g.setFont(uiFont(10.5f, juce::Font::bold));
            g.drawFittedText(formatValue(), value.toNearestInt().reduced(4, 0),
                             juce::Justification::centred, 1);
            return;
        }

        auto labelArea = getLocalBounds().reduced(isLarge ? 10 : 7);

        g.setColour(textMain());
        g.setFont(uiFont(isLarge ? 18.5f : 11.5f, juce::Font::bold));
        g.drawFittedText(title.toUpperCase(), labelArea.removeFromTop(isLarge ? 25 : 17),
                         juce::Justification::centred, 1);

        g.setColour(textMuted());
        g.setFont(uiFont(isLarge ? 10.0f : 9.0f, juce::Font::plain));
        g.drawFittedText(subtitle.toUpperCase(), labelArea.removeFromTop(isLarge ? 14 : 12),
                         juce::Justification::centred, 1);

        const auto engaged = isLarge || focused || isMouseOver(true)
                          || slider.isMouseOver(true) || slider.isMouseButtonDown(true);
        const auto valueBgAlpha = engaged ? 0.72f : 0.30f;
        const auto valueBorderAlpha = engaged ? 0.42f : 0.18f;
        const auto valueTextAlpha = engaged ? 0.96f : 0.78f;
        auto value = getLocalBounds().reduced(isLarge ? 17 : 10)
                    .removeFromBottom(isLarge ? 20 : 17).toFloat();
        g.setColour(juce::Colour(0xff05080d).withAlpha(valueBgAlpha));
        g.fillRoundedRectangle(value, 4.0f);
        g.setColour(accent.withAlpha(valueBorderAlpha));
        g.drawRoundedRectangle(value, 4.0f, 0.8f);
        g.setColour(textMain().withAlpha(valueTextAlpha));
        g.setFont(uiFont(isLarge ? 11.0f : 9.5f, juce::Font::bold));
        g.drawFittedText(formatValue(), value.toNearestInt().reduced(4, 0),
                         juce::Justification::centred, 1);
    }

    void resized() override
    {
        if (useMacroCard)
        {
            auto area = getLocalBounds().reduced(8);
            const auto knobColumn = juce::jlimit(78, 96, area.getWidth() / 3);
            auto knobArea = area.removeFromRight(knobColumn);
            const auto side = juce::jmin(84, juce::jmin(knobArea.getWidth(), knobArea.getHeight()));
            slider.setBounds(juce::Rectangle<int>(side, side).withCentre(knobArea.getCentre()));
            return;
        }

        auto area = getLocalBounds().reduced(isLarge ? 8 : 5);
        area.removeFromTop(isLarge ? 43 : 31);
        area.removeFromBottom(isLarge ? 23 : 20);

        const auto maxSide = isLarge ? 118 : 62;
        const auto availableSide = juce::jmin(area.getWidth(), area.getHeight());
        const auto side = juce::jmin(maxSide, juce::jmax(24, availableSide));
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
    bool isLarge = false;
    bool useMacroCard = false;
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
        setName(title + " control");
        setTitle(title);
        setDescription(subtitle);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setName(title);
        slider.setTitle(title);
        slider.setDescription(subtitle + ". Double-click to reset.");
        slider.setWantsKeyboardFocus(true);
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
        const auto focused = slider.hasKeyboardFocus(true);
        g.setColour(juce::Colour(0xff080d16).withAlpha(0.32f));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(accent.withAlpha(focused ? 0.90f : 0.26f));
        g.drawRoundedRectangle(r, 7.0f, focused ? 1.5f : 0.9f);

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
        setName(title + " selector");
        setTitle(title);
        setDescription(subtitle);
        combo.setName(title);
        combo.setTitle(title);
        combo.setDescription(subtitle);
        combo.setWantsKeyboardFocus(true);
        combo.setTooltip(makeControlTooltip(title, subtitle, false));
        addAndMakeVisible(combo);
    }

    juce::ComboBox& getCombo() { return combo; }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(4.0f);
        const auto focused = combo.hasKeyboardFocus(true);
        g.setColour(juce::Colour(0xff080d16).withAlpha(0.36f));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour((focused ? cyan() : panelLine()).withAlpha(focused ? 0.90f : 0.54f));
        g.drawRoundedRectangle(r, 7.0f, focused ? 1.5f : 0.9f);

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
        button.setTooltip(makeControlTooltip(title, juce::String("Toggle ") + title.toLowerCase(), false));
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
        g.setColour(juce::Colour(0xff080d16).withAlpha(0.35f));
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
    : AudioProcessorEditor(&processorRef),
      processor(processorRef),
      userPresetManager(processorRef.apvts)
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
    corePanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("Delay / Reverb", "Perform")));
    advancedPanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("Advanced", "Sculpt")));
    utilityPanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("I/O", "System")));
    gravityMeter = static_cast<GravityMeter*>(addOwned(std::make_unique<GravityMeter>()));
    inputMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Input", cyan())));
    outputMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Output", spectral())));
    feedbackMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Feedback Safety", amber(), true)));
    corePerformLabel = static_cast<GroupLabel*>(addOwned(std::make_unique<GroupLabel>("Perform", cyan())));
    coreColorLabel = static_cast<GroupLabel*>(addOwned(std::make_unique<GroupLabel>("Color", spectral())));
    advancedMotionLabel = static_cast<GroupLabel*>(addOwned(std::make_unique<GroupLabel>("Motion", violet())));
    advancedTextureLabel = static_cast<GroupLabel*>(addOwned(std::make_unique<GroupLabel>("Texture", amber())));

    macroPanel->setComponentID("panel-macros");
    corePanel->setComponentID("panel-perform");
    advancedPanel->setComponentID("panel-sculpt");
    utilityPanel->setComponentID("panel-system");

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
            loadedPresetSnapshot = processor.apvts.copyState();
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
    presetCaption.setColour(juce::Label::textColourId, textMuted().withAlpha(0.92f));
    presetCaption.setJustificationType(juce::Justification::centredLeft);
    presetCaption.setAccessible(false);
    addAndMakeVisible(presetCaption);

    presetActionsButton.setColour(juce::TextButton::buttonOnColourId, violet());
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

    advancedToggle.setColour(juce::TextButton::buttonOnColourId, violet());
    advancedToggle.setComponentID("view-toggle");
    advancedToggle.setName("Sculpt view");
    advancedToggle.setTitle("Sculpt view");
    advancedToggle.setDescription("Switch between the performance and detailed sound-design views.");

    abButton.setColour(juce::TextButton::buttonOnColourId, violet());
    abButton.setComponentID("snapshot-toggle");
    abButton.setName("A/B snapshot");
    abButton.setTitle("A/B snapshot");
    abButton.setDescription("Compare two temporary parameter snapshots.");

    resetButton.setComponentID("reset-button");
    resetButton.setName("Reset parameters");
    resetButton.setTitle("Reset parameters");
    resetButton.setDescription("Restore every parameter to its declared default value.");

    bypassButton.setButtonText("BYPASS");
    bypassButton.setColour(juce::TextButton::buttonOnColourId, spectral());
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
        advancedToggle.setButtonText(advancedExpanded ? "PERFORM" : "SCULPT");
        advancedToggle.setToggleState(advancedExpanded, juce::dontSendNotification);
        advancedToggle.setTooltip(advancedExpanded ? "Return to the performance view."
                                                   : "Open the detailed sound-design view.");
        resized();
    };

    addSlider(macroLayer, macroControls, "space", "Space", "depth / size", cyan(), true, "%", 100.0, 0);
    addSlider(macroLayer, macroControls, "wild", "Wild", "instability", violet(), true, "%", 100.0, 0);
    addSlider(macroLayer, macroControls, "bloom", "Bloom", "tail / expand", amber(), true, "%", 100.0, 0);

    auto& timeSlider = addSlider(coreLayer, corePrimaryControls,
                                 "timeMs", "Time", "delay line", cyan(), true, " ms", 1.0, 0);
    timeControl = timeSlider.getParentComponent();
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
    auto& shimmerFeedback = addSlider(advancedLayer, advancedTextureControls,
                                      "shimmerFeedback", "Shimmer FB", "recirculate",
                                      amber(), false, "%", 100.0, 0);
    shimmerFeedbackControl = shimmerFeedback.getParentComponent();
    auto& shimmerInterval = addCombo(advancedLayer, advancedTextureControls,
                                     "shimmerInterval", "Octave", "interval");
    shimmerIntervalControl = shimmerInterval.getParentComponent();
    addSlider(advancedLayer, advancedTextureControls, "duckAmount", "Ducking", "clear centre", cyan(), false, "%", 100.0, 0);
    auto& duckThreshold = addSlider(advancedLayer, advancedTextureControls,
                                    "duckThreshold", "Duck Thr", "trigger level",
                                    cyan(), false, " dB", 1.0, 1);
    duckThresholdControl = duckThreshold.getParentComponent();
    addToggle(advancedLayer, advancedTextureControls, "freezeEngage", "Freeze", spectral(),
              [this]
              {
                  if (const auto* mode = processor.apvts.getRawParameterValue("freezeMode"))
                      return mode->load() < 0.5f;
                  return false;
              });
    auto& freezeMode = addCombo(advancedLayer, advancedTextureControls,
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

    addFader(utilityLayer, utilityControls, "inputTrim", "Input", "trim", cyan(), " dB", 1.0, 1);
    addFader(utilityLayer, utilityControls, "outputTrim", "Output", "trim", spectral(), " dB", 1.0, 1);
    addToggle(utilityLayer, utilityControls, "killDry", "Kill Dry", amber());
    addToggle(utilityLayer, utilityControls, "atmosFdnOn", "Atmos", violet());
    addCombo(utilityLayer, utilityControls, "qualityMode", "Engine", "fixed / 2x");

    addToggle(macroLayer, macroPerformanceControls, "freezeEngage", "Freeze", spectral(),
              [this]
              {
                  if (const auto* mode = processor.apvts.getRawParameterValue("freezeMode"))
                      return mode->load() < 0.5f;
                  return false;
              });
    mutateButton.setColour(juce::TextButton::buttonOnColourId, violet());
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
    macroLayer.addAndMakeVisible(mutateButton);
    macroPerformanceControls.push_back(&mutateButton);

    snapshotA = processor.apvts.copyState();
    snapshotB = snapshotA.createCopy();
    loadedPresetSnapshot = snapshotA.createCopy();

    corePanel->setVisible(! advancedExpanded);
    advancedPanel->setVisible(advancedExpanded);

    updateDependentControls();
    observedProgramIndex = processor.getCurrentProgram();
    rebuildPresetSelector();
    updatePresetStatus();

    setResizeLimits(960, 680, 1280, 820);
    setResizable(true, true);
    setSize(1040, 680);
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
    g.drawFittedText("TE-2350 ANTIGRAVITY",
                     header.withTrimmedLeft(76).withTrimmedRight(480),
                     juce::Justification::centredLeft,
                     1,
                     0.86f);

    g.setColour(cyan().withAlpha(0.58f));
    g.drawHorizontalLine(header.getBottom() + 4, static_cast<float>(margin), static_cast<float>(getWidth() - margin));
}

void TE2350AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(margin);
    auto header = area.removeFromTop(54);

    logoMark->setBounds(header.removeFromLeft(52).reduced(2));
    header.removeFromLeft(12);

    auto headerRight = header.removeFromRight(480);
    bypassButton.setBounds(headerRight.removeFromRight(74).reduced(3, 10));
    resetButton.setBounds(headerRight.removeFromRight(64).reduced(3, 10));
    abButton.setBounds(headerRight.removeFromRight(66).reduced(3, 10));
    advancedToggle.setBounds(headerRight.removeFromRight(78).reduced(3, 10));
    headerRight.removeFromRight(8);
    auto presetArea = headerRight.removeFromRight(190).reduced(2, 3);
    presetCaption.setBounds(presetArea.removeFromTop(12));
    auto presetActionArea = presetArea.removeFromRight(28);
    presetArea.removeFromRight(4);
    presetSelector.setBounds(presetArea.reduced(0, 1));
    presetActionsButton.setBounds(presetActionArea.reduced(0, 1));

    area.removeFromTop(12);
    const auto utilityHeight = juce::jlimit(116, 136, area.getHeight() / 5);
    auto utility = area.removeFromBottom(utilityHeight);
    area.removeFromBottom(12);

    const auto leftWidth = juce::jlimit(252, 304, area.getWidth() / 3);
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
    auto meterArea = utilityContent.removeFromRight(
        juce::jlimit(370, 445, static_cast<int>(utilityContent.getWidth() * 0.43f)));
    utilityContent.removeFromRight(10);
    utilityLayer.setBounds(utilityContent);
    layoutGrid(utilityLayer.getLocalBounds(), utilityControls,
               static_cast<int>(utilityControls.size()));

    gravityMeter->setBounds(meterArea.removeFromRight(76).reduced(2));
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
                                              &parent == &macroLayer,
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
                                                    juce::Colour accent,
                                                    std::function<bool()> isMomentary)
{
    auto control = std::make_unique<ToggleTile>(title, accent);
    auto* raw = control.get();
    raw->setMomentaryPredicate(std::move(isMomentary));
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
    if (corePrimaryControls.empty() && coreSecondaryControls.empty())
        return;

    const auto gap = 10;
    const auto primaryHeight = juce::jlimit(160, 230, static_cast<int>(static_cast<float>(area.getHeight()) * 0.55f));
    auto primaryArea = area.removeFromTop(primaryHeight);
    area.removeFromTop(gap);

    corePerformLabel->setBounds(primaryArea.removeFromTop(16));
    primaryArea.removeFromTop(4);
    coreColorLabel->setBounds(area.removeFromTop(16));
    area.removeFromTop(4);

    layoutGrid(primaryArea, corePrimaryControls, chooseColumns(primaryArea, static_cast<int>(corePrimaryControls.size()), 150, 3));

    const auto secondaryColumns = chooseColumns(area, static_cast<int>(coreSecondaryControls.size()), 96, 5);
    layoutGrid(area, coreSecondaryControls, secondaryColumns);
}

void TE2350AudioProcessorEditor::layoutAdvancedControls(juce::Rectangle<int> area)
{
    if (advancedMotionControls.empty() && advancedTextureControls.empty())
        return;

    const auto gap = 10;
    const auto motionHeight = juce::jlimit(140, 210, static_cast<int>(static_cast<float>(area.getHeight()) * 0.45f));
    auto motionArea = area.removeFromTop(motionHeight);
    area.removeFromTop(gap);

    advancedMotionLabel->setBounds(motionArea.removeFromTop(16));
    motionArea.removeFromTop(4);
    advancedTextureLabel->setBounds(area.removeFromTop(16));
    area.removeFromTop(4);

    const auto motionColumns = chooseColumns(motionArea, static_cast<int>(advancedMotionControls.size()), 104, 6);
    const auto textureColumns = chooseColumns(area, static_cast<int>(advancedTextureControls.size()), 72, 7);

    layoutGrid(motionArea, advancedMotionControls, motionColumns);
    layoutGrid(area, advancedTextureControls, textureColumns);
}

void TE2350AudioProcessorEditor::layoutMacroControls(juce::Rectangle<int> area)
{
    if (macroControls.empty())
        return;

    if (! macroPerformanceControls.empty())
    {
        auto performanceArea = area.removeFromBottom(48);
        area.removeFromBottom(8);
        layoutGrid(performanceArea, macroPerformanceControls,
                   static_cast<int>(macroPerformanceControls.size()));
    }

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
    abButton.setButtonText("A/B  A");
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
        abButton.setButtonText("A/B  B");
    }
    else
    {
        snapshotB = processor.apvts.copyState();
        applySnapshot(snapshotA);
        abButton.setButtonText("A/B  A");
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
    const juce::StringArray categories {
        "FOUNDATIONS", "RHYTHMIC", "MOTION", "SHIMMER", "DEEP SPACE", "EXPERIMENTAL"
    };

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
    menu.addItem(1, "Save current sound as user preset...");
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
    menu.addSeparator();

    juce::PopupMenu mutationMenu;
    mutationMenu.addItem(10, "Gentle  ·  10%");
    mutationMenu.addItem(11, "Musical  ·  20%");
    mutationMenu.addItem(12, "Deep  ·  35%");
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
            else if (result == 3)
                safeThis->processor.undo();
            else if (result == 4)
                safeThis->processor.redo();
            else if (result >= 10 && result <= 12)
            {
                constexpr float depths[] { 0.10f, 0.20f, 0.35f };
                safeThis->processor.mutateParameters(depths[result - 10]);
                safeThis->presetDirty = true;
                safeThis->updatePresetStatus();
            }
        });
}

void TE2350AudioProcessorEditor::promptToSaveUserPreset()
{
    savePresetDialog = std::make_unique<juce::AlertWindow>(
        "Save User Preset",
        "Give this sound a memorable name. Saving an existing name replaces that preset.",
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
            [safeThis] (int result)
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

                const auto saveResult = safeThis->userPresetManager.save(name);
                if (saveResult.failed())
                {
                    safeThis->showPresetError(saveResult.getErrorMessage());
                    return;
                }

                safeThis->processor.setActiveUserPreset(name.trim().substring(0, 48));
                safeThis->loadedPresetSnapshot = safeThis->processor.apvts.copyState();
                safeThis->presetDirty = false;
                safeThis->rebuildPresetSelector();
                safeThis->updatePresetStatus();
            }),
        false);
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
    loadedPresetSnapshot = processor.apvts.copyState();
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
    if (! loadedPresetSnapshot.isValid())
        return true;

    for (const auto& parameterState : loadedPresetSnapshot)
    {
        const auto parameterID = parameterState.getProperty("id").toString();
        const auto* raw = processor.apvts.getRawParameterValue(parameterID);
        if (raw == nullptr || ! parameterState.hasProperty("value"))
            continue;

        const auto savedValue = static_cast<float>(
            static_cast<double>(parameterState.getProperty("value")));
        if (std::fabs(raw->load() - savedValue) > 0.0001f)
            return true;
    }

    return false;
}

void TE2350AudioProcessorEditor::updatePresetStatus()
{
    auto label = processor.isActivePresetUser() ? juce::String("USER PRESET")
                                                : juce::String("FACTORY PRESET");
    if (!processor.isActivePresetUser())
    {
        const auto& descriptors = te2350::getFactoryPresetDescriptors();
        const auto program = processor.getCurrentProgram();
        if (juce::isPositiveAndBelow(program, static_cast<int>(descriptors.size())))
            label << "  ·  " << descriptors[static_cast<size_t>(program)].category;
    }
    if (presetDirty)
        label << "  •  MODIFIED";

    presetCaption.setText(label, juce::dontSendNotification);
    presetCaption.setColour(juce::Label::textColourId,
                            (presetDirty ? amber() : textMuted()).withAlpha(0.92f));
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
    const auto effectiveShimmer = read("shimmerAmount")
                                + read("space") * 0.32f
                                + read("bloom") * 0.18f;
    const auto bloom = read("bloom");
    const auto effectiveDucking = read("duckAmount") + bloom * bloom * 0.45f;

    setControlAvailable(timeControl, syncIsFree);
    setControlAvailable(shimmerFeedbackControl, effectiveShimmer > 0.001f);
    setControlAvailable(shimmerIntervalControl, effectiveShimmer > 0.001f);
    setControlAvailable(duckThresholdControl, effectiveDucking > 0.001f);
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

    const auto programIndex = processor.getCurrentProgram();
    const auto selectorRepresentsUserPreset =
        presetSelector.getSelectedId() >= userPresetIDBase;
    if (observedProgramIndex != programIndex
        || selectorRepresentsUserPreset != processor.isActivePresetUser())
    {
        observedProgramIndex = programIndex;
        selectedUserPreset = -1;
        rebuildPresetSelector();
        loadedPresetSnapshot = processor.apvts.copyState();
        presetDirty = false;
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

    updateDependentControls();
}
