#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <atomic>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <new>
#include <vector>

namespace
{
std::atomic<bool> trackRealtimeAllocations { false };
std::atomic<size_t> realtimeAllocationCount { 0 };
}

void* operator new(std::size_t size)
{
    if (trackRealtimeAllocations.load(std::memory_order_relaxed))
        realtimeAllocationCount.fetch_add(1, std::memory_order_relaxed);

    if (auto* memory = std::malloc(size))
        return memory;

    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory) noexcept
{
    ::operator delete(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    ::operator delete(memory);
}

namespace
{
bool setParameter(TE2350AudioProcessor& processor, const char* parameterID, float plainValue)
{
    auto* parameter = processor.apvts.getParameter(parameterID);
    if (parameter == nullptr)
    {
        std::fprintf(stderr, "missing parameter: %s\n", parameterID);
        return false;
    }

    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
    return true;
}

bool configureNeutralPath(TE2350AudioProcessor& processor,
                          float qualityMode,
                          bool bypass)
{
    return setParameter(processor, "space", 0.0f)
        && setParameter(processor, "wild", 0.0f)
        && setParameter(processor, "bloom", 0.0f)
        && setParameter(processor, "feedback", 0.0f)
        && setParameter(processor, "mix", 0.0f)
        && setParameter(processor, "diffusion", 0.0f)
        && setParameter(processor, "modDepth", 0.0f)
        && setParameter(processor, "shimmerAmount", 0.0f)
        && setParameter(processor, "shimmerFeedback", 0.0f)
        && setParameter(processor, "inputTrim", 0.0f)
        && setParameter(processor, "outputTrim", 0.0f)
        && setParameter(processor, "qualityMode", qualityMode)
        && setParameter(processor, "bypass", bypass ? 1.0f : 0.0f);
}

bool isFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (!std::isfinite(buffer.getSample(channel, sample)))
                return false;

    return true;
}

float getPlainParameterValue(TE2350AudioProcessor& processor, const char* parameterID)
{
    if (auto* parameter = processor.apvts.getParameter(parameterID))
        return parameter->convertFrom0to1(parameter->getValue());

    return 0.0f;
}

void warmProcessor(TE2350AudioProcessor& processor, int channels, int blockSize)
{
    juce::AudioBuffer<float> buffer(channels, blockSize);
    juce::MidiBuffer midi;
    buffer.clear();

    for (int block = 0; block < 40; ++block)
        processor.processBlock(buffer, midi);
}

bool verifyOversizedBlockLatency(float qualityMode, bool bypass)
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto preparedBlockSize = 128;
    constexpr auto oversizedBlockSize = 1024;
    constexpr auto impulseSample = 500;

    TE2350AudioProcessor processor;
    if (!configureNeutralPath(processor, qualityMode, bypass))
        return false;

    processor.prepareToPlay(sampleRate, preparedBlockSize);
    warmProcessor(processor, 2, preparedBlockSize);

    juce::AudioBuffer<float> buffer(2, oversizedBlockSize);
    juce::MidiBuffer midi;
    buffer.clear();
    buffer.setSample(0, impulseSample, 0.5f);
    buffer.setSample(1, impulseSample, -0.5f);
    processor.processBlock(buffer, midi);

    const auto delayedSample = impulseSample + processor.getLatencySamples();
    const auto undelayedLeft = buffer.getSample(0, impulseSample);
    const auto undelayedRight = buffer.getSample(1, impulseSample);
    const auto delayedLeft = buffer.getSample(0, delayedSample);
    const auto delayedRight = buffer.getSample(1, delayedSample);
    const auto minimumDelayedLevel = qualityMode > 0.5f && !bypass ? 0.15f : 0.35f;

    if (std::fabs(undelayedLeft) > 0.03f || std::fabs(undelayedRight) > 0.03f
        || delayedLeft < minimumDelayedLevel || delayedRight > -minimumDelayedLevel)
    {
        std::fprintf(stderr,
                     "oversized block lost fixed latency: quality=%.0f bypass=%d "
                     "atImpulse=(%.6f, %.6f) atLatency=(%.6f, %.6f) latency=%d\n",
                     qualityMode,
                     bypass ? 1 : 0,
                     undelayedLeft,
                     undelayedRight,
                     delayedLeft,
                     delayedRight,
                     processor.getLatencySamples());
        return false;
    }

    return true;
}

bool verifyVariableBlockSequence()
{
    constexpr std::array<int, 8> blockSizes { 1, 7, 31, 128, 257, 1024, 4096, 63 };
    TE2350AudioProcessor processor;
    if (!configureNeutralPath(processor, 1.0f, false))
        return false;

    processor.prepareToPlay(48000.0, 128);
    juce::MidiBuffer midi;
    double phase = 0.0;

    for (size_t block = 0; block < blockSizes.size(); ++block)
    {
        const auto blockSize = blockSizes[block];
        juce::AudioBuffer<float> buffer(2, blockSize);

        setParameter(processor, "qualityMode", (block % 2) == 0 ? 0.0f : 1.0f);
        setParameter(processor, "modShape", static_cast<float>(block % 3));
        setParameter(processor, "freezeEngage", block == 4 ? 1.0f : 0.0f);
        setParameter(processor, "bypass", block == 5 ? 1.0f : 0.0f);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = static_cast<float>(0.3 * std::sin(phase));
            phase += juce::MathConstants<double>::twoPi * 997.0 / 48000.0;
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, -value);
        }

        processor.processBlock(buffer, midi);
        if (!isFinite(buffer))
        {
            std::fprintf(stderr, "non-finite variable-block output at size=%d\n", blockSize);
            return false;
        }
    }

    return true;
}

bool verifyMonoLayout()
{
    TE2350AudioProcessor processor;
    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses.add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());

    if (!processor.setBusesLayout(monoLayout))
    {
        std::fprintf(stderr, "processor rejected mono layout\n");
        return false;
    }

    if (!configureNeutralPath(processor, 1.0f, false))
        return false;

    processor.prepareToPlay(96000.0, 64);
    juce::AudioBuffer<float> buffer(1, 257);
    juce::MidiBuffer midi;
    buffer.clear();
    buffer.setSample(0, 173, 0.5f);
    processor.processBlock(buffer, midi);

    if (!isFinite(buffer))
    {
        std::fprintf(stderr, "non-finite mono output\n");
        return false;
    }

    return true;
}

bool verifySampleRateMatrix()
{
    constexpr std::array<double, 4> sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };

    for (const auto sampleRate : sampleRates)
    {
        for (const auto qualityMode : { 0.0f, 1.0f })
        {
            TE2350AudioProcessor processor;
            if (!configureNeutralPath(processor, qualityMode, false))
                return false;

            processor.prepareToPlay(sampleRate, 32);
            juce::AudioBuffer<float> buffer(2, 1024);
            juce::MidiBuffer midi;

            for (int block = 0; block < 4; ++block)
            {
                buffer.clear();
                buffer.setSample(0, 700, 0.2f);
                buffer.setSample(1, 700, -0.2f);
                processor.processBlock(buffer, midi);
                if (!isFinite(buffer))
                {
                    std::fprintf(stderr,
                                 "non-finite sample-rate matrix output: rate=%.0f quality=%.0f\n",
                                 sampleRate,
                                 qualityMode);
                    return false;
                }
            }
        }
    }

    return true;
}

bool verifyStateMigration()
{
    TE2350AudioProcessor source;
    source.setCurrentProgram(2);
    setParameter(source, "mix", 0.73f);
    setParameter(source, "qualityMode", 1.0f);
    setParameter(source, "modShape", 2.0f);

    juce::MemoryBlock currentStateData;
    source.getStateInformation(currentStateData);
    auto currentXml = juce::AudioProcessor::getXmlFromBinary(
        currentStateData.getData(),
        static_cast<int>(currentStateData.getSize()));
    if (currentXml == nullptr)
    {
        std::fprintf(stderr, "current state was not valid XML binary data\n");
        return false;
    }

    auto legacyState = juce::ValueTree::fromXml(*currentXml);
    if (static_cast<int>(legacyState.getProperty("stateVersion", 0)) != 2)
    {
        std::fprintf(stderr, "saved state did not contain stateVersion=2\n");
        return false;
    }

    legacyState.removeProperty("stateVersion", nullptr);
    for (int child = legacyState.getNumChildren(); --child >= 0;)
    {
        const auto parameterID = legacyState.getChild(child).getProperty("id").toString();
        if (parameterID == "qualityMode" || parameterID == "modShape")
            legacyState.removeChild(child, nullptr);
    }

    juce::MemoryBlock legacyStateData;
    if (auto legacyXml = legacyState.createXml())
        juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyStateData);

    TE2350AudioProcessor restored;
    setParameter(restored, "qualityMode", 1.0f);
    setParameter(restored, "modShape", 2.0f);
    restored.setStateInformation(legacyStateData.getData(),
                                 static_cast<int>(legacyStateData.getSize()));

    const auto mix = getPlainParameterValue(restored, "mix");
    const auto quality = getPlainParameterValue(restored, "qualityMode");
    const auto modShape = getPlainParameterValue(restored, "modShape");
    if (std::fabs(mix - 0.73f) > 0.001f
        || std::fabs(quality) > 0.001f
        || std::fabs(modShape - 1.0f) > 0.001f
        || restored.getCurrentProgram() != 2)
    {
        std::fprintf(stderr,
                     "legacy state migration failed: mix=%.3f quality=%.3f "
                     "modShape=%.3f program=%d\n",
                     mix,
                     quality,
                     modShape,
                     restored.getCurrentProgram());
        return false;
    }

    return true;
}

bool verifyAllocationFreeAudioPath()
{
    constexpr auto blockSize = 128;
    TE2350AudioProcessor processor;
    if (!configureNeutralPath(processor, 1.0f, false)
        || !setParameter(processor, "space", 1.0f)
        || !setParameter(processor, "wild", 1.0f)
        || !setParameter(processor, "bloom", 1.0f)
        || !setParameter(processor, "shimmerAmount", 1.0f)
        || !setParameter(processor, "shimmerFeedback", 0.95f))
    {
        return false;
    }

    processor.prepareToPlay(48000.0, blockSize);
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < 80; ++block)
    {
        buffer.clear();
        processor.processBlock(buffer, midi);
    }

    realtimeAllocationCount.store(0, std::memory_order_relaxed);
    for (int block = 0; block < 256; ++block)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = ((block * blockSize + sample) % 113) == 0 ? 0.5f : 0.0f;
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, -value);
        }

        trackRealtimeAllocations.store(true, std::memory_order_release);
        processor.processBlock(buffer, midi);
        trackRealtimeAllocations.store(false, std::memory_order_release);
    }

    const auto allocations = realtimeAllocationCount.load(std::memory_order_relaxed);
    if (allocations != 0)
    {
        std::fprintf(stderr,
                     "audio callback performed %zu tracked C++ allocations\n",
                     allocations);
        return false;
    }

    return true;
}

bool benchmarkRealtime(double sampleRate, int blockSize, float qualityMode)
{
    TE2350AudioProcessor processor;
    if (!configureNeutralPath(processor, qualityMode, false)
        || !setParameter(processor, "space", 1.0f)
        || !setParameter(processor, "wild", 1.0f)
        || !setParameter(processor, "bloom", 1.0f)
        || !setParameter(processor, "feedback", 1.05f)
        || !setParameter(processor, "mix", 1.0f)
        || !setParameter(processor, "diffusion", 1.0f)
        || !setParameter(processor, "chaos", 1.0f)
        || !setParameter(processor, "wobble", 1.0f)
        || !setParameter(processor, "modDepth", 1.0f)
        || !setParameter(processor, "shimmerAmount", 1.0f)
        || !setParameter(processor, "shimmerFeedback", 0.95f)
        || !setParameter(processor, "atmosFdnOn", 1.0f))
    {
        return false;
    }

    processor.prepareToPlay(sampleRate, blockSize);
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;
    double phase = 0.0;
    const auto phaseIncrement = juce::MathConstants<double>::twoPi * 997.0 / sampleRate;

    for (int block = 0; block < 40; ++block)
    {
        buffer.clear();
        processor.processBlock(buffer, midi);
    }

    constexpr auto audioSeconds = 2.0;
    const auto numBlocks = juce::roundToInt(std::ceil(audioSeconds * sampleRate / blockSize));
    const auto startMilliseconds = juce::Time::getMillisecondCounterHiRes();
    for (int block = 0; block < numBlocks; ++block)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = static_cast<float>(0.2 * std::sin(phase));
            phase += phaseIncrement;
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, value);
        }

        processor.processBlock(buffer, midi);
    }

    const auto elapsedSeconds =
        (juce::Time::getMillisecondCounterHiRes() - startMilliseconds) * 0.001;
    const auto renderedSeconds = static_cast<double>(numBlocks * blockSize) / sampleRate;
    const auto realtimeLoadPercent = elapsedSeconds / renderedSeconds * 100.0;
    std::printf("Performance: rate=%.0f block=%d mode=%s load=%.2f%% speed=%.2fx\n",
                sampleRate,
                blockSize,
                qualityMode > 0.5f ? "Studio" : "Hardware",
                realtimeLoadPercent,
                renderedSeconds / elapsedSeconds);

    if (realtimeLoadPercent > 200.0)
    {
        std::fprintf(stderr,
                     "processing exceeded 200%% of one real-time core: %.2f%%\n",
                     realtimeLoadPercent);
        return false;
    }

    return true;
}
}

int main()
{
    if (!verifyOversizedBlockLatency(0.0f, false)
        || !verifyOversizedBlockLatency(1.0f, false)
        || !verifyOversizedBlockLatency(0.0f, true)
        || !verifyVariableBlockSequence()
        || !verifyMonoLayout()
        || !verifySampleRateMatrix()
        || !verifyStateMigration()
        || !verifyAllocationFreeAudioPath()
        || !benchmarkRealtime(48000.0, 128, 0.0f)
        || !benchmarkRealtime(48000.0, 128, 1.0f)
        || !benchmarkRealtime(192000.0, 256, 1.0f))
    {
        return 1;
    }

    std::printf("Host compatibility test passed: oversized, variable blocks, "
                "fixed latency, bypass, mono, 44.1-192 kHz\n");
    return 0;
}
