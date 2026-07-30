#include "UserPresetManager.h"

#include <algorithm>
#include <cmath>

namespace te2350
{
namespace
{
const juce::Identifier presetType { "TE2350_PRESET" };
const juce::Identifier parameterStateType { "PARAMETERS" };
const juce::String presetExtension { ".te2350preset" };
}

UserPresetManager::UserPresetManager(juce::AudioProcessorValueTreeState& stateToUse,
                                     juce::File presetDirectory)
    : state(stateToUse),
      directory(presetDirectory == juce::File() ? defaultPresetDirectory()
                                                : std::move(presetDirectory))
{
    refresh();
}

const std::vector<UserPresetInfo>& UserPresetManager::refresh()
{
    presets.clear();

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*" + presetExtension);
    for (const auto& file : files)
    {
        const auto document = readPresetDocument(file);
        if (!document.isValid())
            continue;

        auto name = document.getProperty("name").toString().trim();
        if (name.isEmpty())
            name = file.getFileNameWithoutExtension();
        presets.push_back({ name, file });
    }

    std::sort(presets.begin(), presets.end(), [] (const auto& left, const auto& right)
    {
        return left.name.compareNatural(right.name) < 0;
    });
    return presets;
}

juce::Result UserPresetManager::save(const juce::String& requestedName)
{
    auto name = requestedName.trim().substring(0, 48);
    if (name.isEmpty())
        return juce::Result::fail("Preset name cannot be empty.");

    if (!directory.createDirectory())
        return juce::Result::fail("Could not create the user preset directory.");

    auto legalName = juce::File::createLegalFileName(name).trim();
    if (legalName.isEmpty())
        return juce::Result::fail("Preset name does not contain a valid filename.");

    auto document = juce::ValueTree(presetType);
    document.setProperty("formatVersion", presetFormatVersion, nullptr);
    document.setProperty("name", name, nullptr);
    document.setProperty("plugin", "TE-2350 Antigravity", nullptr);
    document.addChild(state.copyState(), -1, nullptr);

    const auto target = directory.getChildFile(legalName + presetExtension);
    juce::TemporaryFile temporary(target);
    if (!temporary.getFile().replaceWithText(document.toXmlString()))
        return juce::Result::fail("Could not write the temporary preset file.");

    if (!temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not replace the preset file.");

    refresh();
    return juce::Result::ok();
}

juce::Result UserPresetManager::load(int index)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(presets.size())))
        return juce::Result::fail("User preset index is out of range.");

    const auto document = readPresetDocument(presets[static_cast<size_t>(index)].file);
    if (!document.isValid())
        return juce::Result::fail("Preset file is malformed or incompatible.");

    const auto parameterState = document.getChildWithName(parameterStateType);
    if (!parameterState.isValid())
        return juce::Result::fail("Preset does not contain a parameter state.");

    return applyParameterState(state, parameterState);
}

juce::Result UserPresetManager::remove(int index)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(presets.size())))
        return juce::Result::fail("User preset index is out of range.");

    const auto file = presets[static_cast<size_t>(index)].file;
    if (!file.deleteFile())
        return juce::Result::fail("Could not delete the user preset file.");

    refresh();
    return juce::Result::ok();
}

juce::Result UserPresetManager::applyParameterState(
    juce::AudioProcessorValueTreeState& targetState,
    const juce::ValueTree& parameterState)
{
    if (!parameterState.hasType(parameterStateType))
        return juce::Result::fail("Parameter state has an unexpected type.");

    for (auto* parameter : targetState.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        if (ranged == nullptr || identified == nullptr)
            continue;

        auto plainValue = ranged->convertFrom0to1(ranged->getDefaultValue());
        for (const auto& savedParameter : parameterState)
        {
            if (savedParameter.getProperty("id").toString() != identified->paramID
                || !savedParameter.hasProperty("value"))
            {
                continue;
            }

            const auto candidate = static_cast<double>(savedParameter.getProperty("value"));
            if (std::isfinite(candidate))
                plainValue = static_cast<float>(candidate);
            break;
        }

        const auto normalised = juce::jlimit(0.0f, 1.0f, ranged->convertTo0to1(plainValue));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalised);
        parameter->endChangeGesture();
    }

    targetState.copyState();
    return juce::Result::ok();
}

juce::File UserPresetManager::defaultPresetDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("TE-2350")
        .getChildFile("Antigravity")
        .getChildFile("Presets");
}

juce::ValueTree UserPresetManager::readPresetDocument(const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName(presetType))
        return {};

    auto document = juce::ValueTree::fromXml(*xml);
    const auto version = static_cast<int>(document.getProperty("formatVersion", 0));
    if (version <= 0
        || version > presetFormatVersion
        || !document.getChildWithName(parameterStateType).isValid())
        return {};

    return document;
}
}
