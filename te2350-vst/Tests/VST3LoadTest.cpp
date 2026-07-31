#include <JuceHeader.h>

#include <cmath>
#include <cstdio>

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc != 2)
    {
        std::fprintf(stderr, "usage: TE2350VST3LoadTest <plugin.vst3>\n");
        return 1;
    }

    const juce::File pluginBundle(argv[1]);
    if (!pluginBundle.exists())
    {
        std::fprintf(stderr, "VST3 bundle does not exist: %s\n", argv[1]);
        return 1;
    }

    juce::VST3PluginFormat format;
    if (!format.fileMightContainThisPluginType(pluginBundle.getFullPathName()))
    {
        std::fprintf(stderr, "JUCE did not recognise the bundle as VST3\n");
        return 1;
    }

    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, pluginBundle.getFullPathName());
    if (descriptions.isEmpty())
    {
        std::fprintf(stderr, "VST3 scan returned no plugin descriptions\n");
        return 1;
    }

    const auto& description = *descriptions.getFirst();
    juce::String error;
    auto instance = format.createInstanceFromDescription(description, 48000.0, 128, error);
    if (instance == nullptr)
    {
        std::fprintf(stderr,
                     "VST3 instantiation failed: %s\n",
                     error.toRawUTF8());
        return 1;
    }

    if (description.name != "TE-2350 Antigravity"
        || description.version != TE2350_EXPECTED_PLUGIN_VERSION
        || instance->getTotalNumInputChannels() != 2
        || instance->getTotalNumOutputChannels() != 2
        || instance->getParameters().size() < 30)
    {
        std::fprintf(stderr,
                     "unexpected VST3 metadata: name=%s version=%s in=%d out=%d params=%d\n",
                     description.name.toRawUTF8(),
                     description.version.toRawUTF8(),
                     instance->getTotalNumInputChannels(),
                     instance->getTotalNumOutputChannels(),
                     instance->getParameters().size());
        return 1;
    }

    instance->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer midi;
    buffer.clear();
    buffer.setSample(0, 0, 0.5f);
    buffer.setSample(1, 0, -0.5f);
    instance->processBlock(buffer, midi);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            if (!std::isfinite(buffer.getSample(channel, sample)))
            {
                std::fprintf(stderr,
                             "VST3 produced non-finite output: channel=%d sample=%d\n",
                             channel,
                             sample);
                return 1;
            }
        }
    }

    juce::MemoryBlock state;
    instance->getStateInformation(state);
    if (state.isEmpty())
    {
        std::fprintf(stderr, "VST3 returned an empty state chunk\n");
        return 1;
    }
    instance->setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    if (instance->getLatencySamples() <= 0 || instance->getTailLengthSeconds() < 60.0)
    {
        std::fprintf(stderr,
                     "unexpected VST3 timing metadata: latency=%d tail=%.3f\n",
                     instance->getLatencySamples(),
                     instance->getTailLengthSeconds());
        return 1;
    }

    std::printf("VST3 load test passed: %s %s, params=%d latency=%d tail=%.1f s\n",
                description.name.toRawUTF8(),
                description.version.toRawUTF8(),
                instance->getParameters().size(),
                instance->getLatencySamples(),
                instance->getTailLengthSeconds());
    instance->releaseResources();
    return 0;
}
