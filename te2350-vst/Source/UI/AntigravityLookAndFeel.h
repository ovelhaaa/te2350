#pragma once

#include "AntigravityPalette.h"

#include <cmath>

namespace te2350::ui
{
class AntigravityLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    AntigravityLookAndFeel()
    {
        auto scheme = getCurrentColourScheme();
        scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::windowBackground, panel());
        scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::widgetBackground, display());
        scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::highlightedFill, orange());
        scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::defaultText, textMain());
        setColourScheme(scheme);
        setColour(juce::Label::textColourId, textMain());
        setColour(juce::TooltipWindow::backgroundColourId, panel());
        setColour(juce::TooltipWindow::outlineColourId, orange());
        setColour(juce::TooltipWindow::textColourId, textMain());
        setColour(juce::PopupMenu::highlightedBackgroundColourId, orange());
        setColour(juce::PopupMenu::highlightedTextColourId, display());
        setColour(juce::ScrollBar::thumbColourId, textMuted().withAlpha(0.60f));
        setColour(juce::Slider::textBoxHighlightColourId, orange().withAlpha(0.45f));
        setColour(juce::ComboBox::textColourId, textMain());
        setColour(juce::ComboBox::backgroundColourId, display());
        setColour(juce::ComboBox::outlineColourId, panelLine());
        setColour(juce::ComboBox::arrowColourId, orange());
        setColour(juce::PopupMenu::backgroundColourId, display());
        setColour(juce::PopupMenu::textColourId, textMain());
        setColour(juce::TextButton::textColourOffId, textMain());
        setColour(juce::TextButton::textColourOnId, display());
        setColour(juce::Slider::textBoxTextColourId, textMain());
        setColour(juce::Slider::textBoxBackgroundColourId, display());
        setColour(juce::Slider::textBoxOutlineColourId, panelLine());
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
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
        const auto centre = bounds.getCentre();
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.43f;
        if (radius < 6.0f)
            return;

        const auto sweep = rotaryEndAngle - rotaryStartAngle;
        const auto angle = rotaryStartAngle + sliderPos * sweep;
        const auto dragging = slider.isMouseButtonDown();
        const auto warning = slider.getProperties()["warningThresholdValue"];
        const auto accent = slider.isMouseOverOrDragging()
                            || (!warning.isVoid() && slider.getValue() >= static_cast<double>(warning))
            ? orangeHover() : orange();

        const auto point = [centre] (float r, float a)
        {
            return centre + juce::Point<float>(std::sin(a) * r, -std::cos(a) * r);
        };
        const auto circle = [centre] (float r)
        {
            return juce::Rectangle<float>(r * 2.0f, r * 2.0f).withCentre(centre);
        };

        const int divisions = radius >= 45.0f ? 20 : 10;
        for (int i = 0; i <= divisions; ++i)
        {
            const auto position = static_cast<float>(i) / static_cast<float>(divisions);
            const auto tickAngle = rotaryStartAngle + position * sweep;
            const bool major = i % (divisions / 5) == 0;
            g.setColour((position <= sliderPos ? accent : textMuted()).withAlpha(major ? 0.78f : 0.42f));
            g.drawLine({ point(radius * (major ? 0.94f : 0.98f), tickAngle),
                         point(radius * 1.04f, tickAngle) }, major ? 1.35f : 0.8f);
        }

        const auto arc = [&] (float r, float end, juce::Colour colour, float thickness)
        {
            if (end <= rotaryStartAngle)
                return;
            juce::Path path;
            path.addCentredArc(centre.x, centre.y, r, r, 0.0f, rotaryStartAngle, end, true);
            g.setColour(colour);
            g.strokePath(path, juce::PathStrokeType(thickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        };
        const auto trackWidth = juce::jlimit(2.2f, 5.0f, radius * 0.085f);
        arc(radius * 0.81f, rotaryEndAngle, display(), trackWidth + 1.5f);
        arc(radius * 0.81f, angle, accent, trackWidth);

        const auto effective = slider.getProperties()["effectiveProportion"];
        if (!effective.isVoid())
            arc(radius * 0.90f, rotaryStartAngle + static_cast<float>(effective) * sweep,
                orange().withAlpha(0.52f), 1.0f);

        const auto defaultValue = slider.getProperties()["defaultProportion"];
        if (!defaultValue.isVoid())
        {
            const auto position = point(radius * 1.12f, rotaryStartAngle + static_cast<float>(defaultValue) * sweep);
            g.setColour(textMain());
            const auto size = radius >= 45.0f ? 3.3f : 2.3f;
            g.fillEllipse(juce::Rectangle<float>(size, size).withCentre(position));
        }

        const auto collar = circle(radius * 0.69f);
        g.setColour(display());
        g.fillEllipse(collar.translated(0, 1.5f));
        g.setColour(panelLine());
        g.fillEllipse(collar);
        g.setColour(display());
        g.drawEllipse(collar, 1.0f);
        const auto cap = circle(radius * 0.59f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff505153), centre.x, cap.getY(),
                                              display().interpolatedWith(panel(), 0.6f),
                                              centre.x, cap.getBottom(), false));
        g.fillEllipse(cap);
        g.setColour(dragging || slider.hasKeyboardFocus(true) ? accent : panelLine());
        g.drawEllipse(cap, 1.0f);
        g.setColour(textMain());
        g.drawLine({ point(radius * 0.20f, angle), point(radius * 0.48f, angle) },
                   juce::jmax(1.5f, radius * 0.047f));
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
        label->setFont(readoutFont(13.0f));
        label->setMinimumHorizontalScale(0.85f);
        label->setColour(juce::Label::backgroundColourId, display());
        label->setColour(juce::Label::outlineColourId, panelLine());
        label->setColour(juce::TextEditor::focusedOutlineColourId, orange());
        label->setColour(juce::Label::backgroundWhenEditingColourId, display());
        label->setColour(juce::Label::textWhenEditingColourId, textMain());
        label->setColour(juce::Label::outlineWhenEditingColourId, orange());
        return label;
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
        const auto accent = highlighted || down ? orangeHover() : orange();
        const auto base = active ? accent : display();

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

        g.setColour(button.getToggleState() ? display() : textMain());
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
                      juce::ComboBox& combo) override
    {
        auto r = juce::Rectangle<float>(0.5f, 0.5f,
                                       static_cast<float>(width) - 1.0f,
                                       static_cast<float>(height) - 1.0f);
        g.setColour(display());
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(highlighted || combo.hasKeyboardFocus(true) || combo.isMouseOver(true)
                    ? orangeHover() : panelLine());
        g.drawRoundedRectangle(r, 4.0f, 1.1f);

        juce::Path arrow;
        const auto cx = static_cast<float>(width) - 15.0f;
        const auto cy = static_cast<float>(height) * 0.52f;
        arrow.addTriangle(cx - 4.5f, cy - 2.5f, cx + 4.5f, cy - 2.5f, cx, cy + 3.5f);
        g.setColour(orange().withAlpha(0.86f));
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

        g.setColour(display());
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
        g.setColour(accent.withAlpha(slider.hasKeyboardFocus(true) ? 1.0f : 0.72f));
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
}
