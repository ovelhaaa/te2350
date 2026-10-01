#include "UserPresetManager.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

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
        return left.name.compareIgnoreCase(right.name) < 0;
    });
    return presets;
}

juce::Result UserPresetManager::save(const juce::String& requestedName, bool overwrite)
{
    auto name = sanitiseName(requestedName);
    const auto existing = findByName(name);
    if (existing >= 0 && !overwrite)
        return juce::Result::fail("A user preset with this name already exists.");
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
    document.setProperty("pluginVersion", "0.2.0", nullptr);
    document.setProperty("category", "USER", nullptr);
    document.setProperty("plugin", "TE-2350 Antigravity", nullptr);
    auto snapshot = juce::ValueTree(parameterStateType);
    for (const auto& parameter : state.copyState())
        if (parameter.hasType("PARAM")) snapshot.addChild(parameter.createCopy(), -1, nullptr);
    document.addChild(snapshot, -1, nullptr);

    const auto target = existing >= 0 ? presets[static_cast<size_t>(existing)].file
                                      : directory.getChildFile(legalName + presetExtension);
    if (target.existsAsFile() && !overwrite)
        return juce::Result::fail("A preset file with this filename already exists.");
    juce::TemporaryFile temporary(target);
    if (!temporary.getFile().replaceWithText(document.toXmlString()))
        return juce::Result::fail("Could not write the temporary preset file.");

    if (!temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not replace the preset file.");

    refresh();
    return juce::Result::ok();
}

juce::String UserPresetManager::sanitiseName(const juce::String& name)
{
    auto result = juce::File::createLegalFileName(name.trim()).substring(0, 48).trim();
    while (result.endsWithChar('.') || result.endsWithChar(' '))
        result = result.dropLastCharacters(1);
    if (result == "." || result == "..") return {};
    const auto stem = result.upToFirstOccurrenceOf(".", false, false).toUpperCase();
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL"
        || (stem.length() == 4 && (stem.startsWith("COM") || stem.startsWith("LPT"))
            && stem[3] >= '1' && stem[3] <= '9')) return {};
    return result;
}

int UserPresetManager::findByName(const juce::String& name) const
{
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name.equalsIgnoreCase(sanitiseName(name))) return static_cast<int>(i);
    return -1;
}

juce::Result UserPresetManager::rename(int index, const juce::String& requestedName)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(presets.size())))
        return juce::Result::fail("Select a user preset first.");
    const auto name = sanitiseName(requestedName);
    const auto existing = findByName(name);
    if (name.isEmpty() || (existing >= 0 && existing != index))
        return juce::Result::fail("Invalid or duplicate preset name.");
    const auto source = presets[static_cast<size_t>(index)].file;
    auto document = readPresetDocument(source);
    if (!document.isValid()) return juce::Result::fail("Invalid preset file.");
    const auto target = directory.getChildFile(name + presetExtension);
    if (target != source && target.existsAsFile())
        return juce::Result::fail("The destination already exists.");
    document.setProperty("name", name, nullptr);
    juce::TemporaryFile temporary(target);
    if (!temporary.getFile().replaceWithText(document.toXmlString())
        || !temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not write renamed preset.");
    if (source != target && !source.deleteFile())
    {
        target.deleteFile();
        return juce::Result::fail("Could not remove original preset.");
    }
    refresh();
    return juce::Result::ok();
}

juce::Result UserPresetManager::importPreset(const juce::File& file, bool overwrite)
{
    auto document = readPresetDocument(file);
    if (!document.isValid())
        return juce::Result::fail("Malformed preset or unsupported preset format version (supported: v1).");
    const auto name = sanitiseName(document.getProperty("name").toString());
    if (name.isEmpty()) return juce::Result::fail("Invalid preset name.");
    const auto existing = findByName(name);
    if (existing >= 0 && !overwrite) return juce::Result::fail("A user preset with this name already exists.");
    if (!directory.createDirectory()) return juce::Result::fail("Could not create preset directory.");
    const auto target = existing >= 0 ? presets[static_cast<size_t>(existing)].file
                                    : directory.getChildFile(name + presetExtension);
    if (target.existsAsFile() && !overwrite) return juce::Result::fail("Destination already exists.");
    document.setProperty("name", name, nullptr);
    juce::TemporaryFile temporary(target);
    if (!temporary.getFile().replaceWithText(document.toXmlString())
        || !temporary.overwriteTargetFileWithTemporary()) return juce::Result::fail("Could not import preset.");
    refresh();
    return juce::Result::ok();
}

juce::Result UserPresetManager::exportPreset(int index, const juce::File& file)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(presets.size())))
        return juce::Result::fail("Select a user preset first.");
    const auto document = readPresetDocument(presets[static_cast<size_t>(index)].file);
    if (!document.isValid()) return juce::Result::fail("Invalid preset file.");
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(document.toXmlString())
        || !temporary.overwriteTargetFileWithTemporary()) return juce::Result::fail("Could not export preset.");
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

    const auto parameters = document.getChildWithName(parameterStateType);
    if (parameters.getNumChildren() == 0) return {};
    juce::StringArray ids;
    for (const auto& parameter : parameters)
    {
        const auto id = parameter.getProperty("id").toString();
        const auto value = parameter.getProperty("value");
        const auto text = value.toString().trim();
        char* end = nullptr;
        const auto numeric = std::strtod(text.toRawUTF8(), &end);
        if (!parameter.hasType("PARAM") || id.isEmpty() || ids.contains(id)
            || text.isEmpty() || end == text.toRawUTF8() || *end != '\0'
            || !std::isfinite(numeric)) return {};
        ids.add(id);
    }
    return document;
}
}
