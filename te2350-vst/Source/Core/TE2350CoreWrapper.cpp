#include "TE2350CoreWrapper.h"

#include <cmath>

namespace te2350
{
namespace
{
constexpr size_t memoryPoolBytes = 320 * 1024;
}

bool TE2350CoreWrapper::prepare(double sampleRate, int maximumBlockSize)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlockSize = maximumBlockSize;
    memoryPool.assign(memoryPoolBytes / sizeof(q31_t), 0);

    ready = te2350_init(&core, memoryPool.data(), memoryPoolBytes, static_cast<float>(currentSampleRate));

    if (ready)
    {
        te2350_set_melody_enabled(&core, false);
        te2350_set_melody_only(&core, false);
    }

    return ready;
}

void TE2350CoreWrapper::reset()
{
    if (!memoryPool.empty())
        ready = te2350_init(&core, memoryPool.data(), memoryPoolBytes, static_cast<float>(currentSampleRate));
}

void TE2350CoreWrapper::setParameters(const CoreParameters& parameters)
{
    if (!ready)
        return;

    inputGain = juce::Decibels::decibelsToGain(parameters.inputTrimDb);
    outputGain = juce::Decibels::decibelsToGain(parameters.outputTrimDb);

    const auto wildFeedbackCeiling = 0.95f + juce::jlimit(0.0f, 1.0f, parameters.wild) * 0.10f;
    const auto feedback = juce::jmin(parameters.feedback, wildFeedbackCeiling);
    const auto mix = parameters.killDry ? 1.0f : parameters.mix;
    const auto tone = normaliseLog(parameters.highCutHz, 1000.0f, 18000.0f);

    te2350_set_time(&core, toQ31(normaliseLog(parameters.timeMs, 10.0f, 2000.0f)));
    te2350_set_feedback(&core, toQ31(feedback));
    te2350_set_mix(&core, toQ31(mix));
    te2350_set_tone(&core, toQ31(tone));
    te2350_set_diffusion(&core, toQ31(parameters.diffusion));
    te2350_set_chaos(&core, toQ31(parameters.chaos));
    te2350_set_wobble(&core, toQ31(parameters.wobble));
    te2350_set_presence(&core, toQ31(parameters.presence));
    te2350_set_ducking(&core, toQ31(parameters.duckAmount));
    te2350_set_shimmer(&core, toQ31(parameters.shimmerAmount));
    te2350_set_mod(&core,
                   toQ31(normaliseLog(parameters.modRateHz, 0.02f, 2.0f)),
                   toQ31(parameters.modDepth));
    te2350_set_octave_feedback_enabled(&core, parameters.shimmerInterval == 2 && parameters.shimmerFeedback > 0.001f);
    te2350_set_octave_feedback_amount(&core, toQ31(parameters.shimmerFeedback));
    te2350_set_fdn_enabled(&core, parameters.atmosFdnOn || parameters.space >= 0.40f);
    te2350_set_freeze(&core, parameters.freeze);
}

void TE2350CoreWrapper::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (!ready)
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numChannels == 0)
        return;

    auto* left = buffer.getWritePointer(0);
    auto* right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto inL = left[sample];
        const auto inR = right != nullptr ? right[sample] : inL;
        const auto mono = juce::jlimit(-1.0f, 1.0f, (inL + inR) * 0.5f * inputGain);

        q31_t outL = 0;
        q31_t outR = 0;
        te2350_process(&core, toQ31(mono), &outL, &outR);

        left[sample] = Q31_TO_FLOAT(outL) * outputGain;
        if (right != nullptr)
            right[sample] = Q31_TO_FLOAT(outR) * outputGain;
    }

    for (int channel = 2; channel < numChannels; ++channel)
        buffer.clear(channel, 0, numSamples);
}

float TE2350CoreWrapper::getEnvelope() const
{
    return ready ? Q31_TO_FLOAT(te2350_get_envelope(const_cast<::te2350_t*>(&core))) : 0.0f;
}

float TE2350CoreWrapper::getModulator() const
{
    return ready ? Q31_TO_FLOAT(te2350_get_modulator(const_cast<::te2350_t*>(&core))) : 0.0f;
}

q31_t TE2350CoreWrapper::toQ31(float value)
{
    return float_to_q31_safe(juce::jlimit(-1.0f, 1.0f, value));
}

float TE2350CoreWrapper::normaliseLinear(float value, float minimum, float maximum)
{
    return juce::jlimit(0.0f, 1.0f, (value - minimum) / (maximum - minimum));
}

float TE2350CoreWrapper::normaliseLog(float value, float minimum, float maximum)
{
    const auto safeValue = juce::jlimit(minimum, maximum, value);
    return juce::jlimit(0.0f, 1.0f, std::log(safeValue / minimum) / std::log(maximum / minimum));
}
}
