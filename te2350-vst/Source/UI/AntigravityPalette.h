#pragma once

#include <JuceHeader.h>

namespace te2350::ui
{
inline juce::Colour chassis()     { return juce::Colour(0xfff0ede6); }
inline juce::Colour panel()       { return juce::Colour(0xff292b2d); }
inline juce::Colour display()     { return juce::Colour(0xff17191b); }
inline juce::Colour orange()      { return juce::Colour(0xfff36b2b); }
inline juce::Colour orangeHover() { return juce::Colour(0xfff6824b); }
inline juce::Colour textMain()    { return juce::Colour(0xfff3f0e9); }
inline juce::Colour textMuted()   { return juce::Colour(0xffbab6ad); }
inline juce::Colour divider()    { return juce::Colour(0xffd4cfc3); }
inline juce::Colour panelLine()  { return juce::Colour(0xff3b3e42); }

inline juce::Font uiFont(float size, int style = juce::Font::plain)
{
    return juce::Font(juce::Font::getDefaultSansSerifFontName(), size, style);
}

inline juce::Font readoutFont(float size)
{
    return juce::Font(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain);
}

inline void drawPanel(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour surface = panel())
{
    bounds = bounds.reduced(0.5f);
    g.setColour(surface);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(panelLine());
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
}
}
