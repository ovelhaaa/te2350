#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "Presets/FactoryPresets.h"
#include "Presets/UserPresetManager.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>

namespace
{
bool setParameter(TE2350AudioProcessor& processor,
                  const juce::String& parameterID,
                  float plainValue)
{
    auto* parameter = processor.apvts.getParameter(parameterID);
    if (parameter == nullptr)
        return false;

    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
    processor.apvts.copyState();
    return true;
}

float getParameter(const TE2350AudioProcessor& processor,
                   const juce::String& parameterID)
{
    if (const auto* parameter = processor.apvts.getParameter(parameterID))
        return parameter->convertFrom0to1(parameter->getValue());

    return 0.0f;
}

bool approximatelyEqual(float left, float right, float tolerance = 0.001f)
{
    return std::fabs(left - right) <= tolerance;
}

struct TemporaryPresetDirectory
{
    TemporaryPresetDirectory()
        : directory(juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("TE2350PresetWorkflow", {}, true))
    {
        directory.createDirectory();
    }

    ~TemporaryPresetDirectory()
    {
        directory.deleteRecursively();
    }

    juce::File directory;
};

bool verifyFactoryCatalog()
{
    const auto& descriptors = te2350::getFactoryPresetDescriptors();
    if (descriptors.size() != 20)
    {
        std::fprintf(stderr, "expected 20 factory presets, got %zu\n", descriptors.size());
        return false;
    }

    std::set<std::string> names;
    std::set<std::string> categories;
    for (const auto& descriptor : descriptors)
    {
        if (descriptor.name.isEmpty()
            || descriptor.category.isEmpty()
            || descriptor.description.isEmpty()
            || !names.insert(descriptor.name.toStdString()).second)
        {
            std::fprintf(stderr, "factory preset metadata is incomplete or duplicated\n");
            return false;
        }
        categories.insert(descriptor.category.toStdString());
    }

    for (const auto* required : {
             "FOUNDATIONS", "RHYTHMIC", "MOTION",
             "SHIMMER", "DEEP SPACE", "EXPERIMENTAL" })
    {
        if (categories.count(required) == 0)
        {
            std::fprintf(stderr, "factory category missing: %s\n", required);
            return false;
        }
    }

    TE2350AudioProcessor processor;
    setParameter(processor, "bypass", 1.0f);
    processor.setCurrentProgram(0);
    if (!approximatelyEqual(getParameter(processor, "bypass"), 0.0f)
        || !approximatelyEqual(getParameter(processor, "space"), 0.35f))
    {
        std::fprintf(stderr, "factory preset did not reset unspecified parameters\n");
        return false;
    }

    return true;
}

bool verifyUserPresetRoundTrip()
{
    TemporaryPresetDirectory temporaryDirectory;
    TE2350AudioProcessor processor;
    te2350::UserPresetManager manager(processor.apvts, temporaryDirectory.directory);

    processor.setCurrentProgram(8);
    setParameter(processor, "mix", 0.37f);
    if (const auto result = manager.save("My Orbit"); result.failed())
    {
        std::fprintf(stderr, "could not save user preset: %s\n",
                     result.getErrorMessage().toRawUTF8());
        return false;
    }

    if (manager.getPresets().size() != 1
        || manager.getPresets().front().name != "My Orbit"
        || manager.getPresets().front().file.getFileExtension() != ".te2350preset")
    {
        std::fprintf(stderr, "saved user preset was not indexed correctly\n");
        return false;
    }

    setParameter(processor, "mix", 0.91f);
    setParameter(processor, "wild", 0.01f);
    setParameter(processor, "bypass", 1.0f);
    processor.beginUndoTransaction("Load My Orbit");
    if (const auto result = manager.load(0); result.failed())
    {
        std::fprintf(stderr, "could not load user preset: %s\n",
                     result.getErrorMessage().toRawUTF8());
        return false;
    }
    processor.setActiveUserPreset("My Orbit");

    if (!approximatelyEqual(getParameter(processor, "mix"), 0.37f)
        || !approximatelyEqual(getParameter(processor, "wild"), 0.46f)
        || !approximatelyEqual(getParameter(processor, "bypass"), 0.0f))
    {
        std::fprintf(stderr, "user preset round-trip changed parameter values\n");
        return false;
    }

    setParameter(processor, "mix", 0.41f);
    if (const auto result = manager.save("My Orbit", true); result.failed()
        || manager.getPresets().size() != 1)
    {
        std::fprintf(stderr, "saving an existing preset did not replace it atomically\n");
        return false;
    }

    temporaryDirectory.directory.getChildFile("Malformed.te2350preset")
        .replaceWithText("<not-a-preset");
    manager.refresh();
    if (manager.getPresets().size() != 1)
    {
        std::fprintf(stderr, "malformed preset was included in the user library\n");
        return false;
    }

    juce::ValueTree partialState("PARAMETERS");
    juce::ValueTree mixState("PARAM");
    mixState.setProperty("id", "mix", nullptr);
    mixState.setProperty("value", 0.77f, nullptr);
    partialState.addChild(mixState, -1, nullptr);
    setParameter(processor, "wild", 0.81f);
    if (const auto result =
            te2350::UserPresetManager::applyParameterState(processor.apvts, partialState);
        result.failed()
        || !approximatelyEqual(getParameter(processor, "mix"), 0.77f)
        || !approximatelyEqual(
            getParameter(processor, "wild"),
            te2350::getParameterDefault("wild")))
    {
        std::fprintf(stderr, "partial preset did not use declared defaults safely\n");
        return false;
    }

    if (const auto result = manager.remove(0); result.failed()
        || !manager.getPresets().empty())
    {
        std::fprintf(stderr, "could not remove user preset\n");
        return false;
    }

    return true;
}

bool verifyM12Library()
{
    TemporaryPresetDirectory temporary;
    TE2350AudioProcessor processor;
    const auto folder = temporary.directory.getChildFile("new-library");
    te2350::UserPresetManager manager(processor.apvts, folder);
    if (!manager.getPresets().empty() || manager.save("").wasOk()
        || manager.save("..").wasOk() || manager.save("CON").wasOk()) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    const auto blocked = temporary.directory.getChildFile("blocked");
    blocked.replaceWithText("not a directory");
    te2350::UserPresetManager unavailable(processor.apvts, blocked);
    if (!unavailable.getPresets().empty() || unavailable.save("Unavailable").wasOk()) return false;
    // Snapshot every parameter, including macros, switches, advanced controls and bypass.
    for (auto* parameter : processor.getParameters())
        parameter->setValueNotifyingHost(0.73f);
    const auto expected = processor.apvts.copyState();
    if (manager.save("Orbit").failed() || manager.save("orbit").wasOk()) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    processor.setCurrentProgram(0);
    if (manager.load(0).failed()) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    for (const auto& saved : expected) {
        const auto id = saved.getProperty("id").toString();
        const auto* parameter = processor.apvts.getParameter(id);
        if (parameter != nullptr && !approximatelyEqual(parameter->getValue(),
            parameter->convertTo0to1(static_cast<float>(static_cast<double>(saved.getProperty("value")))), 0.00001f)) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    }
    if (manager.save("Orbit", true).failed() || manager.rename(0, "New Orbit").failed()) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    const auto exported = temporary.directory.getChildFile("export.te2350preset");
    if (manager.exportPreset(0, exported).failed() || manager.remove(0).failed()
        || manager.importPreset(exported).failed() || manager.importPreset(exported).wasOk()) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    te2350::UserPresetManager reopened(processor.apvts, folder);
    if (reopened.getPresets().size() != 1 || reopened.load(0).failed()) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    if (reopened.save("Other").failed() || reopened.rename(0, "Other").wasOk()
        || reopened.remove(-1).wasOk() || reopened.rename(-1, "Factory").wasOk()) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    const auto bad = temporary.directory.getChildFile("bad.te2350preset");
    for (const auto* text : { "", "<", "<WRONG/>",
        "<TE2350_PRESET formatVersion=\"999\"><PARAMETERS/></TE2350_PRESET>",
        "<TE2350_PRESET formatVersion=\"1\"><PARAMETERS><PARAM id=\"mix\" value=\"nan\"/></PARAMETERS></TE2350_PRESET>",
        "<TE2350_PRESET formatVersion=\"1\"><PARAMETERS><PARAM id=\"mix\" value=\"oops\"/></PARAMETERS></TE2350_PRESET>" }) {
        bad.replaceWithText(text);
        if (manager.importPreset(bad).wasOk()) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    }
    processor.setActiveUserPreset("New Orbit");
    const auto baseline = processor.getPresetBaseline().createCopy();
    if (processor.isPresetModified()) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    for (auto* parameter : processor.getParameters()) {
        const auto original = parameter->getValue();
        parameter->setValueNotifyingHost(original > 0.5f ? 0.0f : 1.0f);
        if (!processor.isPresetModified()) return false;
        parameter->setValueNotifyingHost(original);
        if (processor.isPresetModified()) return false;
    }
    const auto originalSpace = getParameter(processor, "space");
    setParameter(processor, "space", 0.12f);
    if (!processor.isPresetModified()) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    setParameter(processor, "space", originalSpace);
    if (processor.isPresetModified()) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    setParameter(processor, "space", 0.12f);
    juce::MemoryBlock data;
    processor.getStateInformation(data);
    manager.remove(manager.findByName("New Orbit"));
    TE2350AudioProcessor restored;
    restored.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    if (!approximatelyEqual(getParameter(restored, "space"), 0.12f)
        || !restored.isPresetModified()
        || restored.getPresetBaseline().getNumChildren() != baseline.getNumChildren()) { std::fprintf(stderr, "M12 failure at line %d\n", __LINE__); return false; }
    for (const auto& saved : baseline) {
        const auto id = saved.getProperty("id").toString();
        bool found = false;
        for (const auto& loaded : restored.getPresetBaseline())
            if (loaded.getProperty("id").toString() == id) {
                found = approximatelyEqual(static_cast<float>(static_cast<double>(saved.getProperty("value"))),
                    static_cast<float>(static_cast<double>(loaded.getProperty("value"))), 0.00001f);
                break;
            }
        if (!found) return false;
    }
    return true;
}

bool verifyMutationSafetyAndUndo()
{
    TE2350AudioProcessor first;
    TE2350AudioProcessor second;
    first.setCurrentProgram(4);
    second.setCurrentProgram(4);

    constexpr std::array<const char*, 10> protectedParameters {{
        "timeMs", "syncMode", "mix", "inputTrim", "outputTrim",
        "qualityMode", "bypass", "freezeEngage", "freezeMode", "killDry"
    }};
    std::array<float, protectedParameters.size()> protectedValues {};
    for (size_t index = 0; index < protectedParameters.size(); ++index)
        protectedValues[index] = getParameter(first, protectedParameters[index]);

    std::map<std::string, float> valuesBeforeMutation;
    for (auto* parameter : first.getParameters())
        if (const auto* identified =
                dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
            valuesBeforeMutation[identified->paramID.toStdString()] = parameter->getValue();

    first.mutateParameters(0.20f, 2350);
    second.mutateParameters(0.20f, 2350);

    auto changedParameters = 0;
    for (auto* parameter : first.getParameters())
    {
        const auto* identified =
            dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        if (identified == nullptr)
            continue;

        const auto firstValue = parameter->getValue();
        const auto* secondParameter = second.apvts.getParameter(identified->paramID);
        if (secondParameter == nullptr
            || !approximatelyEqual(firstValue, secondParameter->getValue(), 0.00001f))
        {
            std::fprintf(stderr, "mutation is not deterministic for a fixed seed\n");
            return false;
        }

        if (!approximatelyEqual(
                firstValue,
                valuesBeforeMutation[identified->paramID.toStdString()]))
            ++changedParameters;
    }

    for (size_t index = 0; index < protectedParameters.size(); ++index)
    {
        if (!approximatelyEqual(getParameter(first, protectedParameters[index]),
                                protectedValues[index]))
        {
            std::fprintf(stderr, "mutation changed protected parameter: %s\n",
                         protectedParameters[index]);
            return false;
        }
    }

    if (changedParameters == 0
        || getParameter(first, "wild") > 0.861f
        || getParameter(first, "feedback") > 0.921f
        || getParameter(first, "shimmerFeedback") > 0.801f)
    {
        std::fprintf(stderr, "mutation did not change the sound or exceeded safety caps\n");
        return false;
    }

    TE2350AudioProcessor undoProcessor;
    undoProcessor.setCurrentProgram(0);
    const auto originalMix = getParameter(undoProcessor, "mix");
    undoProcessor.setCurrentProgram(6);
    const auto eventHorizonMix = getParameter(undoProcessor, "mix");
    if (!undoProcessor.canUndo() || !undoProcessor.undo()
        || !approximatelyEqual(getParameter(undoProcessor, "mix"), originalMix))
    {
        std::fprintf(stderr, "preset load was not reversible through Undo\n");
        return false;
    }
    if (!undoProcessor.canRedo() || !undoProcessor.redo()
        || !approximatelyEqual(getParameter(undoProcessor, "mix"), eventHorizonMix))
    {
        std::fprintf(stderr, "preset load was not reversible through Redo\n");
        return false;
    }

    return true;
}

bool verifyEmbeddedUserPresetState()
{
    TE2350AudioProcessor source;
    source.setCurrentProgram(3);
    setParameter(source, "mix", 0.49f);
    source.setActiveUserPreset("Nebula Local");

    juce::MemoryBlock stateData;
    source.getStateInformation(stateData);

    TE2350AudioProcessor restored;
    restored.setStateInformation(stateData.getData(),
                                 static_cast<int>(stateData.getSize()));
    if (!restored.isActivePresetUser()
        || restored.getActivePresetName() != "Nebula Local"
        || !approximatelyEqual(getParameter(restored, "mix"), 0.49f))
    {
        std::fprintf(stderr, "embedded user preset identity did not survive host state\n");
        return false;
    }

    return true;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    if (!verifyFactoryCatalog()
        || !verifyUserPresetRoundTrip()
        || !verifyM12Library()
        || !verifyMutationSafetyAndUndo()
        || !verifyEmbeddedUserPresetState())
    {
        return 1;
    }

    std::printf("Preset workflow test passed: factory catalog, user library, "
                "embedded state, mutation safety, Undo/Redo\n");
    return 0;
}
