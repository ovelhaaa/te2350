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

    // Every attached UI component carries its public ParameterID.
    const char* expected[] = { "space", "wild", "bloom", "timeMs", "syncMode", "feedback", "mix", "killDry", "lowCutHz", "highCutHz", "diffusion", "chaos", "wobble", "presence", "modRateHz", "modDepth", "modShape", "shimmerInterval", "shimmerAmount", "shimmerFeedback", "duckThreshold", "duckAmount", "inputTrim", "outputTrim", "bypass", "qualityMode", "freezeEngage", "freezeMode", "atmosFdnOn", "wetWidth" };
    if (processor.getParameters().size() != 30) return 1;
    for (const auto* id : expected)
    {
        if (processor.apvts.getParameter(id) == nullptr
            || (juce::String(id) != "bypass" && findComponent(*editor, id) == nullptr))
        {
            std::fprintf(stderr, "Missing parameter or UI attachment: %s\n", id);
            return 1;
        }
    }
    struct Contract { const char* id; float minimum, maximum, initial; };
    const Contract contract[] = {
        { "space", 0.0f, 1.0f, 0.30f },
        { "wild", 0.0f, 1.0f, 0.00f },
        { "bloom", 0.0f, 1.0f, 0.20f },
        { "timeMs", 10.0f, 2000.0f, 420.0f },
        { "feedback", 0.0f, 1.05f, 0.45f },
        { "mix", 0.0f, 1.0f, 0.35f },
        { "lowCutHz", 20.0f, 1000.0f, 80.0f },
        { "highCutHz", 1000.0f, 18000.0f, 9000.0f },
        { "diffusion", 0.0f, 1.0f, 0.40f },
        { "chaos", 0.0f, 1.0f, 0.00f },
        { "wobble", 0.0f, 1.0f, 0.10f },
        { "presence", 0.0f, 1.0f, 0.50f },
        { "modRateHz", 0.02f, 2.0f, 0.15f },
        { "modDepth", 0.0f, 1.0f, 0.10f },
        { "shimmerAmount", 0.0f, 1.0f, 0.00f },
        { "shimmerFeedback", 0.0f, 0.95f, 0.30f },
        { "duckThreshold", -60.0f, 0.0f, -24.0f },
        { "duckAmount", 0.0f, 1.0f, 0.10f },
        { "inputTrim", -24.0f, 24.0f, 0.0f },
        { "outputTrim", -24.0f, 24.0f, 0.0f },
        { "wetWidth", 0.0f, 1.0f, 0.60f }
    };
    for (const auto& spec : contract)
    {
        auto* parameter = processor.apvts.getParameter(spec.id);
        if (parameter == nullptr || parameter->getNormalisableRange().start != spec.minimum
            || parameter->getNormalisableRange().end != spec.maximum
            || std::abs(parameter->convertFrom0to1(parameter->getDefaultValue())-spec.initial) > 0.001f)
            return 1;
    }
    auto* editButton = findButtonWithText(*editor, "ADVANCED");
    if (editButton == nullptr)
    {
        std::fprintf(stderr, "could not find ADVANCED button\n");
        return 1;
    }

    if (editButton->onClick)
        editButton->onClick();
    if (editButton->getButtonText() != "CLOSE")
    {
        std::fprintf(stderr, "ADVANCED button did not open the drawer\n");
        return 1;
    }
    if (!performPanel->isVisible() || !sculptPanel->isVisible())
    {
        std::fprintf(stderr, "Perform must remain visible while Advanced is open\n");
        return 1;
    }
    if (!renderEditor(*editor, outputDirectory.getChildFile("advanced_1040x680.png"), 1040, 680))
    {
        std::fprintf(stderr, "failed to render advanced editor image\n");
        return 1;
    }

    if (!renderEditor(*editor, outputDirectory.getChildFile("advanced_960x680.png"), 960, 680)
        || !renderEditor(*editor, outputDirectory.getChildFile("advanced_1280x820.png"), 1280, 820)) return 1;

    auto* drawer = dynamic_cast<juce::Viewport*>(findComponent(*editor,"advanced-drawer"));
    if (drawer == nullptr) return 1;
    const char* sectionNames[] { "shimmer", "color", "dynamics", "freeze", "system" };
    const int sectionOffsets[] { 120, 240, 360, 480, 620 };
    for (int i=0; i<5; ++i)
    {
        editor->setSize(1040,680);
        drawer->setViewPosition(0,sectionOffsets[i]);
        if (!renderEditor(*editor,outputDirectory.getChildFile(juce::String("advanced_")+sectionNames[i]+".png"),1040,680)) return 1;
    }
    drawer->setViewPosition(0,0);

    const auto set = [&processor] (const char* id, float value)
    {
        auto* parameter = processor.apvts.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    juce::AudioBuffer<float> audio(2,128);
    juce::MidiBuffer midi;
    const auto settle = [&]
    {
        for (int i=0;i<200;++i) { audio.clear(); processor.processBlock(audio,midi); }
        juce::Timer::callPendingTimersSynchronously();
        juce::Thread::sleep(150);
        juce::Timer::callPendingTimersSynchronously();
    };
    set("space",0); set("wild",0); set("bloom",0);
    set("shimmerAmount",0); set("duckAmount",0); set("feedback",0.2f); set("syncMode",1);
    settle();
    if (findComponent(*editor,"timeMs")->isEnabled()
        || findComponent(*editor,"shimmerInterval")->isEnabled()
        || findComponent(*editor,"shimmerFeedback")->isEnabled()
        || findComponent(*editor,"duckThreshold")->isEnabled()) {
        std::fprintf(stderr,"Dependencies off: time %d interval %d regen %d duck %d; telemetry %f %f\n",
            findComponent(*editor,"timeMs")->isEnabled(),findComponent(*editor,"shimmerInterval")->isEnabled(),
            findComponent(*editor,"shimmerFeedback")->isEnabled(),findComponent(*editor,"duckThreshold")->isEnabled(),
            processor.getEffectiveShimmerMeterValue(),processor.getEffectiveDuckingMeterValue()); return 1; }
    set("wild",1); set("space",1); set("bloom",1); set("syncMode",0);
    settle();
    if (processor.getEffectiveFeedbackMeterValue() < 0.79f
        || std::abs(processor.apvts.getRawParameterValue("feedback")->load()-0.2f)>0.001f
        || !findComponent(*editor,"timeMs")->isEnabled()
        || !findComponent(*editor,"shimmerInterval")->isEnabled()
        || !findComponent(*editor,"duckThreshold")->isEnabled()) {
        std::fprintf(stderr,"Dependencies on or feedback telemetry failed: %f\n",processor.getEffectiveFeedbackMeterValue()); return 1; }

    if (!renderEditor(*editor,outputDirectory.getChildFile("effective_macros_1040x680.png"),1040,680)) return 1;
    auto* snapshotButton = dynamic_cast<juce::Button*>(findComponent(*editor,"snapshot-toggle"));
    if (snapshotButton == nullptr || !snapshotButton->onClick) return 1;
    const auto before = processor.apvts.getRawParameterValue("feedback")->load();
    snapshotButton->onClick();
    set("feedback",0.1f);
    snapshotButton->onClick();
    if (snapshotButton->getButtonText() != "A"
        || std::abs(processor.apvts.getRawParameterValue("feedback")->load()-before)>0.001f) return 1;

    set("feedback", 0.31f);
    settle();
    const auto verifyText = [](const auto& self, juce::Component& component, bool& modified) -> bool
    {
        if (auto* label = dynamic_cast<juce::Label*>(&component))
        {
            const auto text = label->getText();
            if (text.contains("MODIFIED"))
            {
                if (text != "MODIFIED") return false;
                modified = true;
            }
            for (auto character : text)
                if (character == 0xfffd || (character >= 0x80 && character <= 0x9f)
                    || character == 0x00c2 || character == 0x00c3) return false;
        }
        for (auto* child : component.getChildren())
            if (!self(self, *child, modified)) return false;
        return true;
    };
    bool modified = false;
    if (!verifyText(verifyText, *editor, modified) || !modified) return 1;
    if (!renderEditor(*editor, outputDirectory.getChildFile("modified_960x680.png"), 960, 680)
        || !renderEditor(*editor, outputDirectory.getChildFile("modified_1040x680.png"), 1040, 680)
        || !renderEditor(*editor, outputDirectory.getChildFile("modified_1280x820.png"), 1280, 820)) return 1;

    std::printf("Rendered JUCE editor references to %s\n",
                outputDirectory.getFullPathName().toRawUTF8());
    return 0;
}
