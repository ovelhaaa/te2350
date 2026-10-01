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
juce::Colour panelLine()  { return juce::Colour(0xff203248); }
juce::Colour textMain()   { return juce::Colour(0xffe8f5ff); }
juce::Colour textMuted()  { return juce::Colour(0xff8192a9); }
juce::Colour cyan()       { return juce::Colour(0xff27d9ff); }
juce::Colour violet()     { return juce::Colour(0xff9b62ff); }
juce::Colour amber()      { return juce::Colour(0xffffb451); }

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

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius * 0.78f, radius * 0.78f, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff172538));
        g.strokePath(track, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        const auto effective = slider.getProperties()["effectiveProportion"];
        if (!effective.isVoid())
        {
            juce::Path ghost;
            ghost.addCentredArc(centre.x, centre.y, radius * 0.95f, radius * 0.95f, 0.0f,
                               rotaryStartAngle, rotaryStartAngle + static_cast<float>(effective) * (rotaryEndAngle-rotaryStartAngle), true);
            g.setColour(cyan().withAlpha(0.25f));
            g.strokePath(ghost, juce::PathStrokeType(2.0f));
        }
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
        g.setColour(textMuted());
        g.setFont(uiFont(10, juce::Font::bold));
        g.drawText(title.toUpperCase(), 12, 4, getWidth()-24, 18, juce::Justification::centredLeft);
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
        auto area = getLocalBounds().reduced(8);
        if (slider.hasKeyboardFocus(true))
        {
            g.setColour(accent.withAlpha(0.6f));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2), 5, 1);
        }
        g.setColour(textMain());
        g.setFont(uiFont(useMacroCard ? 20.0f : 12.0f, juce::Font::bold));
        g.drawText(title.toUpperCase(), area.removeFromTop(26), juce::Justification::centred);
        g.setColour(textMuted());
        g.setFont(uiFont(12));
        g.drawText(formatValue(), area.removeFromBottom(22), juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(26);
        area.removeFromBottom(22);
        const auto side = juce::jmin(useMacroCard ? 170 : 76,
                                    juce::jmin(area.getWidth(), area.getHeight()));
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
        g.setColour(textMain());
        g.setFont(uiFont(11, juce::Font::bold));
        g.drawText(title.toUpperCase(), 7, 7, getWidth()-14, 18, juce::Justification::centred);
    }
    void resized() override
    {
        combo.setBounds(getLocalBounds().reduced(8).removeFromBottom(27));
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
            return amber();

        if (level >= 0.82f)
            return amber();

        return cyan();
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

            g.setColour((pass == 0 ? cyan() : pass == 1 ? violet() : cyan()).withAlpha(0.48f));
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
    utilityPanel = static_cast<SectionPanel*>(addOwned(std::make_unique<SectionPanel>("Engine / System", "")));
    inputMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Input", cyan())));
    outputMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Output", cyan())));
    feedbackMeter = static_cast<MeterStrip*>(addOwned(std::make_unique<MeterStrip>("Feedback", amber(), true)));
    corePerformLabel = static_cast<GroupLabel*>(addOwned(std::make_unique<GroupLabel>("Perform", cyan())));
    coreColorLabel = static_cast<GroupLabel*>(addOwned(std::make_unique<GroupLabel>("Color", cyan())));
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
    bypassButton.setColour(juce::TextButton::buttonOnColourId, cyan());
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
        advancedToggle.setButtonText(advancedExpanded ? "CLOSE" : "ADVANCED");
        advancedToggle.setToggleState(advancedExpanded, juce::dontSendNotification);
        advancedToggle.setTooltip(advancedExpanded ? "Return to the performance view."
                                                   : "Open the detailed sound-design view.");
        resized();
    };

    addSlider(macroLayer, macroControls, "space", "Space", "Expands delay time, diffusion, width and atmospheric depth.", cyan(), true, "%", 100.0, 0);
    addSlider(macroLayer, macroControls, "wild", "Wild", "Introduces instability, modulation, chaos and regenerative feedback.", violet(), true, "%", 100.0, 0);
    addSlider(macroLayer, macroControls, "bloom", "Bloom", "Extends and opens the tail while adding shimmer and ducking.", cyan(), true, "%", 100.0, 0);

    auto& timeSlider = addSlider(coreLayer, corePrimaryControls,
                                 "timeMs", "Time", "delay line", cyan(), true, " ms", 1.0, 0);
    timeControl = timeSlider.getParentComponent();
    addSlider(coreLayer, corePrimaryControls, "feedback", "Feedback", "regeneration", amber(), true, "%", 100.0, 0);
    addSlider(coreLayer, corePrimaryControls, "mix", "Mix", "dry / wet", cyan(), true, "%", 100.0, 0);
    addSlider(advancedLayer, advancedColorControls, "diffusion", "Diffusion", "cloud density", violet(), false, "%", 100.0, 0);
    addSlider(coreLayer, corePrimaryControls, "highCutHz", "Tone", "Wet high-cut / brightness; frequency mapping follows the voiced core filter.", cyan(), true, " kHz", 0.001, 1);
    addSlider(advancedLayer, advancedColorControls, "lowCutHz", "Low Cut", "Removes low-frequency energy from the wet signal.", amber(), false, " Hz", 1.0, 0);
    addSlider(coreLayer, corePrimaryControls, "wetWidth", "Width", "stereo field", cyan(), true, "%", 100.0, 0);
    syncSelector = &addCombo(coreLayer, coreSecondaryControls, "syncMode", "", "Host tempo division; Free enables manual Time.");
    syncSelector->setName("Time sync");
    syncSelector->setTitle("Time sync");

    addSlider(advancedLayer, advancedMotionControls, "modRateHz", "Mod Rate", "slow orbit", cyan(), false, " Hz", 1.0, 2);
    addSlider(advancedLayer, advancedMotionControls, "modDepth", "Mod Depth", "field bend", violet(), false, "%", 100.0, 0);
    addCombo(advancedLayer, advancedMotionControls, "modShape", "Mod Shape", "movement");
    addSlider(advancedLayer, advancedMotionControls, "chaos", "Chaos", "diffusion random", violet(), false, "%", 100.0, 0);
    addSlider(advancedLayer, advancedColorControls, "presence", "Presence", "front detail", cyan(), false, "%", 100.0, 0);
    addSlider(advancedLayer, advancedMotionControls, "wobble", "Drift", "tape gravity", cyan(), false, "%", 100.0, 0);
    addSlider(advancedLayer, advancedTextureControls, "shimmerAmount", "Shimmer", "octave veil", amber(), false, "%", 100.0, 0);
    auto& shimmerFeedback = addSlider(advancedLayer, advancedTextureControls,
                                      "shimmerFeedback", "Regen", "Amount of pitched signal fed back into the tail.",
                                      amber(), false, "%", 100.0, 0);
    shimmerFeedbackControl = shimmerFeedback.getParentComponent();
    auto& shimmerInterval = addCombo(advancedLayer, advancedTextureControls,
                                     "shimmerInterval", "Interval", "Pitch interval of the shimmer and regeneration lanes.");
    shimmerIntervalControl = shimmerInterval.getParentComponent();
    addSlider(advancedLayer, advancedDynamicsControls, "duckAmount", "Ducking", "clear centre", cyan(), false, "%", 100.0, 0);
    auto& duckThreshold = addSlider(advancedLayer, advancedDynamicsControls,
                                    "duckThreshold", "Duck Threshold", "trigger level",
                                    cyan(), false, " dB", 1.0, 1);
    duckThresholdControl = duckThreshold.getParentComponent();
    addToggle(advancedLayer, advancedFreezeControls, "freezeEngage", "Freeze", cyan(),
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

    addFader(utilityLayer, utilityControls, "inputTrim", "Input", "trim", cyan(), " dB", 1.0, 1);
    addFader(utilityLayer, utilityControls, "outputTrim", "Output", "trim", cyan(), " dB", 1.0, 1);
    addToggle(utilityLayer, utilityControls, "killDry", "Kill Dry", cyan()).setTooltip("Removes the direct signal inside the effect mix. Useful on aux/send buses.");
    addToggle(utilityLayer, utilityControls, "atmosFdnOn", "Atmos FDN", violet());
    addCombo(utilityLayer, utilityControls, "qualityMode", "Engine", "Hardware: Original fixed-point character. Studio: 2x oversampled output colour path.");

    addToggle(coreLayer, macroPerformanceControls, "freezeEngage", "Freeze", cyan(),
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
    addAndMakeVisible(mutateButton);

    snapshotA = processor.apvts.copyState();
    snapshotB = snapshotA.createCopy();
    loadedPresetSnapshot = processor.getPresetBaseline().createCopy();

    corePanel->setVisible(true);
    advancedPanel->setVisible(advancedExpanded);

    resetButton.setVisible(false);
    corePerformLabel->setVisible(false);
    coreColorLabel->setVisible(false);
    advancedMotionLabel->setVisible(false);
    advancedTextureLabel->setVisible(false);
    advancedViewport.setComponentID("advanced-drawer");
    addAndMakeVisible(advancedViewport);
    advancedViewport.setViewedComponent(&advancedContent, false);
    advancedViewport.setScrollBarsShown(true, false);
    advancedContent.addAndMakeVisible(advancedPanel);
    advancedPanel->setVisible(advancedExpanded);
    advancedContent.addAndMakeVisible(utilityPanel);
    for (const auto& name : { "MOTION", "SHIMMER", "COLOR", "DYNAMICS", "FREEZE", "ENGINE / SYSTEM" })
    {
        auto label = std::make_unique<GroupLabel>(name, cyan());
        advancedLayer.addAndMakeVisible(label.get());
        advancedGroupLabels.push_back(label.get());
        ownedComponents.push_back(std::move(label));
    }
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
    g.fillAll(night());
    g.setColour(textMain());
    g.setFont(uiFont(25, juce::Font::bold));
    g.drawText("TE-2350", 80, 18, 160, 30, juce::Justification::centredLeft);
    g.setColour(textMuted());
    g.setFont(uiFont(10));
    g.drawText("ANTIGRAVITY", 82, 48, 160, 16, juce::Justification::centredLeft);
}

void TE2350AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(margin);
    auto header = area.removeFromTop(54);
    logoMark->setBounds(header.removeFromLeft(52));
    auto right = header.removeFromRight(getWidth() - 270);
    bypassButton.setBounds(right.removeFromRight(80).reduced(3,10));
    advancedToggle.setBounds(right.removeFromRight(100).reduced(3,10));
    abButton.setBounds(right.removeFromRight(40).reduced(3,10));
    mutateButton.setBounds(right.removeFromRight(78).reduced(3,10));
    presetActionsButton.setBounds(right.removeFromRight(32).reduced(2,10));
    presetCaption.setBounds(right.removeFromBottom(14));
    presetSelector.setBounds(right.reduced(4,4));
    area.removeFromTop(12);
    auto footer = area.removeFromBottom(34);
    const auto meterWidth = footer.getWidth()/3;
    inputMeter->setBounds(footer.removeFromLeft(meterWidth).reduced(12,0));
    outputMeter->setBounds(footer.removeFromLeft(meterWidth).reduced(12,0));
    feedbackMeter->setBounds(footer.reduced(12,0));
    area.removeFromBottom(8);
    advancedViewport.setVisible(advancedExpanded);
    advancedPanel->setVisible(advancedExpanded);
    if (advancedExpanded)
    {
        advancedViewport.setBounds(area.removeFromBottom(150));
        area.removeFromBottom(8);
    }
    auto macros = area.removeFromTop(advancedExpanded ? 185 : area.getHeight()*3/5);
    macroPanel->setBounds(macros);
    macroLayer.setBounds(macroPanel->getContentBounds());
    layoutMacroControls(macroLayer.getLocalBounds());
    corePanel->setBounds(area);
    coreLayer.setBounds(corePanel->getContentBounds());
    layoutCoreControls(coreLayer.getLocalBounds());
    const auto contentWidth = getWidth()-margin*2-18;
    advancedContent.setSize(contentWidth, 770);
    advancedPanel->setBounds(0,0,contentWidth,640);
    advancedLayer.setBounds(advancedPanel->getContentBounds());
    layoutAdvancedControls(advancedLayer.getLocalBounds());
    utilityPanel->setBounds(0,640,contentWidth,130);
    utilityLayer.setBounds(utilityPanel->getContentBounds());
    layoutGrid(utilityLayer.getLocalBounds(),utilityControls,5);
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
    raw->setComponentID(parameterID);
    jassert(processor.apvts.getParameter(parameterID) != nullptr);
    enableDefaultReset(raw->getSlider(), processor.apvts.getParameter(parameterID));
    raw->getSlider().getProperties().set("parameterID", parameterID);

    if (parameterID == "feedback")
    {
        raw->getSlider().getProperties().set("warningThresholdValue", 0.86);
        raw->getSlider().getProperties().set("criticalThresholdValue", 2.0);
    }
    else if (parameterID == "shimmerFeedback")
    {
        raw->getSlider().getProperties().set("warningThresholdValue", 0.76);
        raw->getSlider().getProperties().set("criticalThresholdValue", 2.0);
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
    raw->setComponentID(parameterID);
    jassert(processor.apvts.getParameter(parameterID) != nullptr);
    enableDefaultReset(raw->getSlider(), processor.apvts.getParameter(parameterID));
    raw->getSlider().getProperties().set("parameterID", parameterID);
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
    layoutGrid(freezeArea.withSizeKeepingCentre(110,50), macroPerformanceControls,1);
    const auto width = area.getWidth()/5;
    for (size_t i=0; i<corePrimaryControls.size(); ++i)
    {
        auto cell = area.removeFromLeft(width);
        if (i==0)
        {
            coreSecondaryControls.front()->setBounds(cell.removeFromBottom(34));
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
    for (int i=0; i<5; ++i)
    {
        auto row = area.removeFromTop(120);
        advancedGroupLabels[static_cast<size_t>(i)]->setBounds(row.removeFromLeft(110));
        layoutGrid(row,*groups[i],static_cast<int>(groups[i]->size()));
    }
    advancedGroupLabels.back()->setVisible(false);
}

void TE2350AudioProcessorEditor::layoutMacroControls(juce::Rectangle<int> area)
{
    layoutGrid(area,macroControls,3);
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

    for (const auto& component : ownedComponents)
    {
        if (auto* knob = dynamic_cast<KnobTile*>(component.get()))
        {
            auto& slider = knob->getSlider();
            const auto id = slider.getProperties()["parameterID"].toString();
            if (id == "timeMs" || id == "feedback" || id == "highCutHz" || id == "diffusion"
                || id == "wetWidth" || id == "shimmerAmount" || id == "duckAmount")
            {
                const auto effective = processor.getEffectiveControlValue(id);
                slider.getProperties().set("effectiveProportion", juce::jlimit(0.0, 1.0, slider.valueToProportionOfLength(effective)));
                knob->repaint();
            }
        }
    }
    updateDependentControls();
}
