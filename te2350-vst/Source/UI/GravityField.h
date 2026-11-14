#pragma once

#include "AntigravityPalette.h"
#include "PluginProcessor.h"

#include <array>
#include <cmath>

namespace te2350::ui
{
class GravityField final : public juce::Component
{
public:
    explicit GravityField(TE2350AudioProcessor& source)
        : processor(source), freeze(source.apvts.getRawParameterValue("freezeEngage")),
          sync(source.apvts.getRawParameterValue("syncMode"))
    {
        setName("Gravity Field");
        setTitle("Gravity Field");
        setDescription("Parametric view of effective delay time, requested feedback, diffusion, stereo width, and shimmer. Freeze holds the orbit marker.");
        setInterceptsMouseClicks(false, false);
    }

    void refresh()
    {
        const auto frozen = freeze->load() >= 0.5f;
        const auto synced = sync->load() >= 0.5f;
        const std::array<int, 5> next {
            juce::roundToInt(processor.getEffectiveControlValue("timeMs")),
            juce::roundToInt(processor.getEffectiveFeedbackMeterValue() * 100.0f),
            juce::roundToInt(processor.getEffectiveControlValue("diffusion") * 100.0f),
            juce::roundToInt(processor.getEffectiveControlValue("wetWidth") * 100.0f),
            juce::roundToInt(processor.getEffectiveShimmerMeterValue() * 100.0f)
        };

        for (size_t i = 0; i < next.size(); ++i)
            if (next[i] != values[i])
                readouts[i] = juce::String(next[i]) + (i == 0 ? " ms" : "%");

        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (!frozen)
        {
            const auto delta = juce::jlimit(0.0, 50.0, now - lastRefresh);
            const auto period = juce::jlimit(180.0, 4000.0, static_cast<double>(next[0]));
            phase = std::fmod(phase + delta * juce::MathConstants<double>::twoPi / period,
                              juce::MathConstants<double>::twoPi);
        }
        lastRefresh = now;

        if (next != values || frozen != isFrozen || synced != isSynced || !frozen)
            repaint();

        values = next;
        isFrozen = frozen;
        isSynced = synced;
    }

    void paint(juce::Graphics& g) override
    {
        drawPanel(g, getLocalBounds().toFloat(), display());
        auto area = getLocalBounds().reduced(12, 8);
        auto heading = area.removeFromTop(22);
        g.setFont(uiFont(12.0f, juce::Font::bold));
        g.setColour(textMain());
        g.drawText("GRAVITY FIELD", heading.removeFromLeft(105), juce::Justification::centredLeft);
        g.setColour(textMuted());
        g.setFont(readoutFont(10.0f));
        g.drawText("PARAMETRIC", heading, juce::Justification::centredRight);
        g.setColour(panelLine());
        g.drawHorizontalLine(area.getY() + 2, static_cast<float>(area.getX()),
                             static_cast<float>(area.getRight()));
        area.removeFromTop(9);

        auto left = area.removeFromLeft(67);
        auto right = area.removeFromRight(67);
        drawReadout(g, left.removeFromTop(30), isSynced ? "HOST TIME" : "TIME", readouts[0]);
        drawReadout(g, left.removeFromTop(30), "FB REQ.", readouts[1]);
        drawReadout(g, left.removeFromTop(30), "DIFFUSION", readouts[2]);
        drawReadout(g, right.removeFromTop(30), "WIDTH", readouts[3]);
        drawReadout(g, right.removeFromTop(30), "SHIMMER", readouts[4]);

        if (isFrozen)
        {
            auto badge = right.removeFromTop(22).removeFromLeft(62).toFloat();
            g.setColour(orange());
            g.fillRoundedRectangle(badge, 3.0f);
            g.setColour(display());
            g.setFont(uiFont(11.0f, juce::Font::bold));
            g.drawText("FREEZE", badge.toNearestInt(), juce::Justification::centred);
        }

        const auto plot = area.toFloat().reduced(3.0f, 7.0f);
        const auto centre = plot.getCentre();
        g.setColour(panelLine());
        g.drawLine(plot.getX(), centre.y, plot.getRight(), centre.y, 1.0f);
        g.drawLine(centre.x, plot.getY(), centre.x, plot.getBottom(), 1.0f);

        const auto width = juce::jlimit(0.0f, 1.0f, static_cast<float>(values[3]) * 0.01f);
        const auto diffusion = juce::jlimit(0.0f, 1.0f, static_cast<float>(values[2]) * 0.01f);
        const auto feedback = juce::jlimit(0.0f, 1.0f, static_cast<float>(values[1]) * 0.01f);
        const auto shimmer = juce::jlimit(0.0f, 1.0f, static_cast<float>(values[4]) * 0.01f);
        const auto xRadius = juce::jmin(plot.getWidth() * 0.48f, plot.getHeight() * 0.74f)
                             * (0.61f + 0.39f * width);
        const auto yRadius = juce::jmin(plot.getHeight() * 0.43f, xRadius * (0.88f - 0.34f * width));
        for (int i = 0; i < 5; ++i)
        {
            const auto ring = static_cast<float>(i);
            const auto scale = 1.0f - ring * (0.19f - 0.045f * diffusion);
            const auto opacity = i == 0 ? 0.65f : (0.13f + feedback * 0.48f) * (1.0f - ring * 0.11f);
            g.setColour(orange().withAlpha(opacity));
            g.drawEllipse(juce::Rectangle<float>(2.0f * xRadius * scale, 2.0f * yRadius * scale)
                              .withCentre(centre), 1.0f);
        }

        const auto marker = centre + juce::Point<float>(
            xRadius * static_cast<float>(std::cos(phase)), yRadius * static_cast<float>(std::sin(phase)));
        if (shimmer > 0.01f)
        {
            g.setColour(orange().withAlpha(0.2f + shimmer * 0.5f));
            g.drawEllipse(juce::Rectangle<float>(12.0f, 12.0f).withCentre(marker), 1.0f);
        }
        g.setColour(orange());
        g.fillEllipse(juce::Rectangle<float>(5.0f, 5.0f).withCentre(marker));
        g.setColour(textMain());
        g.fillEllipse(juce::Rectangle<float>(4.0f, 4.0f).withCentre(centre));
    }

private:
    static void drawReadout(juce::Graphics& g, juce::Rectangle<int> bounds,
                            const juce::String& label, const juce::String& value)
    {
        g.setFont(uiFont(9.0f, juce::Font::bold));
        g.setColour(textMuted());
        g.drawText(label, bounds.removeFromTop(11), juce::Justification::centredLeft);
        g.setFont(readoutFont(12.0f));
        g.setColour(textMain());
        g.drawText(value, bounds.removeFromTop(16), juce::Justification::centredLeft);
    }

    TE2350AudioProcessor& processor;
    const std::atomic<float>* const freeze;
    const std::atomic<float>* const sync;
    std::array<int, 5> values { -1, -1, -1, -1, -1 };
    std::array<juce::String, 5> readouts;
    bool isFrozen = false;
    bool isSynced = false;
    double phase = 0.0;
    double lastRefresh = juce::Time::getMillisecondCounterHiRes();
};
}
