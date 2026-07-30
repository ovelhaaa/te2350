#include "TE2350CoreWrapper.h"

#include <array>
#include <cmath>

namespace te2350
{
namespace
{
constexpr size_t memoryPoolBytes = TE2350_REQUIRED_MEMORY_BYTES;

float calibratedTailFeedback(float bloom, float delaySeconds)
{
    constexpr std::array<float, 6> bloomPoints {
        0.0f, 0.25f, 0.50f, 0.75f, 0.90f, 1.0f
    };
    constexpr std::array<float, 6> feedbackAt500Ms {
        0.0f, 0.195f, 0.550f, 0.690f, 0.713f, 0.721f
    };

    bloom = juce::jlimit(0.0f, 1.0f, bloom);
    auto referenceFeedback = feedbackAt500Ms.back();
    for (size_t point = 1; point < bloomPoints.size(); ++point)
    {
        if (bloom <= bloomPoints[point])
        {
            const auto amount = (bloom - bloomPoints[point - 1])
                              / (bloomPoints[point] - bloomPoints[point - 1]);
            referenceFeedback = juce::jmap(amount,
                                           feedbackAt500Ms[point - 1],
                                           feedbackAt500Ms[point]);
            break;
        }
    }

    if (referenceFeedback <= 0.0f)
        return 0.0f;

    const auto loopDurationScale = juce::jmax(0.001f, delaySeconds) / 0.5f;
    return juce::jlimit(0.0f, 0.995f,
                        std::pow(referenceFeedback, loopDurationScale));
}
}

bool TE2350CoreWrapper::prepare(double sampleRate, int maximumBlockSize)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlockSize = maximumBlockSize;
    memoryPool.assign(memoryPoolBytes / sizeof(q31_t), 0);
    inputGain.reset(currentSampleRate, 0.02);
    outputGain.reset(currentSampleRate, 0.02);
    wetWidth.reset(currentSampleRate, 0.02);
    inputGain.setCurrentAndTargetValue(1.0f);
    outputGain.setCurrentAndTargetValue(1.0f);
    wetWidth.setCurrentAndTargetValue(0.60f);

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

    inputGain.setTargetValue(juce::Decibels::decibelsToGain(parameters.inputTrimDb));
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(parameters.outputTrimDb));
    wetWidth.setTargetValue(juce::jlimit(0.0f, 1.0f, parameters.wetWidth));

    const auto wildFeedbackCeiling = 0.95f + juce::jlimit(0.0f, 1.0f, parameters.wild) * 0.10f;
    const auto feedback = juce::jmin(parameters.feedback, wildFeedbackCeiling);
    const auto mix = parameters.killDry ? 1.0f : parameters.mix;
    const auto tone = normaliseLog(parameters.highCutHz, 1000.0f, 18000.0f);
    const auto lowCutCoefficient = 1.0f
                                 - std::exp(-juce::MathConstants<float>::twoPi
                                            * juce::jlimit(20.0f, 1000.0f, parameters.lowCutHz)
                                            / static_cast<float>(currentSampleRate));
    const auto exactTimeSamples = juce::roundToInt(parameters.timeMs
                                                   * static_cast<float>(currentSampleRate)
                                                   * 0.001f);
    const auto delaySeconds = juce::jmax(0.001f, parameters.timeMs * 0.001f);
    const auto rt60Feedback = calibratedTailFeedback(parameters.bloom,
                                                     delaySeconds);

    te2350_set_time(&core, toQ31(normaliseLog(parameters.timeMs, 10.0f, 2000.0f)));
    te2350_set_time_samples(&core, exactTimeSamples);
    te2350_set_feedback(&core, toQ31(feedback));
    te2350_set_tail(&core, toQ31(parameters.bloom));
    te2350_set_tail_feedback(&core, toQ31(juce::jlimit(0.0f, 0.995f, rt60Feedback)));
    te2350_set_mix(&core, toQ31(mix));
    te2350_set_tone(&core, toQ31(tone));
    te2350_set_low_cut_coeff(&core, toQ31(lowCutCoefficient));
    te2350_set_diffusion(&core, toQ31(parameters.diffusion));
    te2350_set_chaos(&core, toQ31(parameters.chaos));
    te2350_set_wobble(&core, toQ31(parameters.wobble));
    te2350_set_presence(&core, toQ31(parameters.presence));
    te2350_set_ducking(&core, toQ31(parameters.duckAmount));
    te2350_set_duck_threshold(&core,
                              toQ31(juce::Decibels::decibelsToGain(parameters.duckThresholdDb)));
    te2350_set_shimmer(&core, toQ31(parameters.shimmerAmount));
    te2350_set_shimmer_interval(&core, parameters.shimmerInterval);
    te2350_set_mod(&core,
                   toQ31(normaliseLog(parameters.modRateHz, 0.02f, 2.0f)),
                   toQ31(parameters.modDepth));
    te2350_set_mod_shape(&core, parameters.modShape);
    te2350_set_mod_rate_hz(&core, parameters.modRateHz);
    te2350_set_octave_feedback_enabled(
        &core,
        parameters.shimmerAmount > 0.001f && parameters.shimmerFeedback > 0.001f);
    te2350_set_octave_feedback_amount(&core, toQ31(parameters.shimmerFeedback));
    te2350_set_fdn_enabled(&core, parameters.atmosFdnOn);
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
        const auto currentInputGain = inputGain.getNextValue();
        const auto currentOutputGain = outputGain.getNextValue();
        const auto currentWetWidth = wetWidth.getNextValue();
        const auto mono = juce::jlimit(-1.0f, 1.0f,
                                      (inL + inR) * 0.5f * currentInputGain);

        q31_t outL = 0;
        q31_t outR = 0;
        te2350_process(&core, toQ31(mono), &outL, &outR);

        const auto coreL = Q31_TO_FLOAT(outL);
        const auto coreR = Q31_TO_FLOAT(outR);

        if (right != nullptr)
        {
            const auto coreMid = (coreL + coreR) * 0.5f;
            const auto coreSide = (coreL - coreR) * 0.5f * currentWetWidth;
            const auto drySideGain = Q31_TO_FLOAT(te2350_get_dry_mix_gain(&core));
            const auto drySide = (inL - inR) * 0.5f * currentInputGain * drySideGain;

            left[sample] = (coreMid + coreSide + drySide) * currentOutputGain;
            right[sample] = (coreMid - coreSide - drySide) * currentOutputGain;
        }
        else
        {
            left[sample] = coreL * currentOutputGain;
        }
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
