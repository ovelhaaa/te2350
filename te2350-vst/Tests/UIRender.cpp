#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <cstdio>
#include <memory>

namespace
{
juce::Button* findButtonWithText(juce::Component& parent, const juce::String& text)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* button = dynamic_cast<juce::Button*>(child);
            button != nullptr && button->getButtonText() == text)
        {
            return button;
        }

        if (auto* nested = findButtonWithText(*child, text))
            return nested;
    }

    return nullptr;
}

juce::Component* findComponent(juce::Component& parent,
                               const juce::String& componentID,
                               const juce::String& name = {})
{
    for (auto* child : parent.getChildren())
    {
        if ((!componentID.isEmpty() && child->getComponentID() == componentID)
            || (!name.isEmpty() && child->getName() == name))
        {
            return child;
        }

        if (auto* nested = findComponent(*child, componentID, name))
            return nested;
    }

    return nullptr;
}

bool renderEditor(juce::AudioProcessorEditor& editor,
                  const juce::File& output,
                  int width,
                  int height)
{
    editor.setSize(width, height);
    editor.resized();

    juce::Image image(juce::Image::ARGB, width, height, true);
    juce::Graphics graphics(image);
    editor.paintEntireComponent(graphics, true);

    if (auto stream = output.createOutputStream())
    {
        stream->setPosition(0);
        stream->truncate();
        juce::PNGImageFormat png;
        return png.writeImageToStream(image, *stream);
    }

    return false;
}
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto outputDirectory = argc > 1
        ? juce::File(argv[1])
        : juce::File::getCurrentWorkingDirectory().getChildFile("UIRenders");

    if (!outputDirectory.createDirectory())
    {
        std::fprintf(stderr,
                     "could not create UI output directory: %s\n",
                     outputDirectory.getFullPathName().toRawUTF8());
        return 1;
    }

    TE2350AudioProcessor processor;
    processor.setCurrentProgram(4);
    processor.prepareToPlay(48000.0, 128);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* performPanel = findComponent(*editor, "panel-perform");
    auto* sculptPanel = findComponent(*editor, "panel-sculpt");
    auto* inputControl = findComponent(*editor, {}, "Input control");
    if (performPanel == nullptr || sculptPanel == nullptr || inputControl == nullptr
        || !performPanel->isVisible() || sculptPanel->isVisible()
        || inputControl->getBounds().isEmpty())
    {
        std::fprintf(stderr, "invalid initial editor hierarchy or I/O layout\n");
        return 1;
    }

    if (!renderEditor(*editor, outputDirectory.getChildFile("main_1040x680.png"), 1040, 680)
        || !renderEditor(*editor, outputDirectory.getChildFile("main_compact_960x680.png"), 960, 680)
        || !renderEditor(*editor, outputDirectory.getChildFile("main_large_1280x820.png"), 1280, 820))
    {
        std::fprintf(stderr, "failed to render main editor images\n");
        return 1;
    }

    auto* editButton = findButtonWithText(*editor, "SCULPT");
    if (editButton == nullptr)
    {
        std::fprintf(stderr, "could not find SCULPT button\n");
        return 1;
    }

    if (editButton->onClick)
        editButton->onClick();
    if (editButton->getButtonText() != "PERFORM")
    {
        std::fprintf(stderr, "SCULPT button did not switch the editor view\n");
        return 1;
    }
    if (performPanel->isVisible() || !sculptPanel->isVisible())
    {
        std::fprintf(stderr, "Perform/Sculpt panel visibility did not switch\n");
        return 1;
    }
    if (!renderEditor(*editor, outputDirectory.getChildFile("advanced_1040x680.png"), 1040, 680))
    {
        std::fprintf(stderr, "failed to render advanced editor image\n");
        return 1;
    }

    std::printf("Rendered JUCE editor references to %s\n",
                outputDirectory.getFullPathName().toRawUTF8());
    return 0;
}
