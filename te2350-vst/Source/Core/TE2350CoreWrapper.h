#pragma once

#include <JuceHeader.h>
#include <vector>

extern "C"
{
#include "../../../include/te2350.h"
#include "../../../include/dsp_math.h"
}

namespace te2350
{
struct CoreParameters
{
    float space = 0.30f;
    float wild = 0.0f;
    float timeMs = 420.0f;
    float feedback = 0.45f;
    float mix = 0.35f;
    bool killDry = false;
    float lowCutHz = 80.0f;
    float highCutHz = 9000.0f;
    float diffusion = 0.40f;
    float chaos = 0.0f;
    float wobble = 0.10f;
    float presence = 0.50f;
    float modRateHz = 0.15f;
    float modDepth = 0.10f;
    int shimmerInterval = 2;
    float shimmerAmount = 0.0f;
    float shimmerFeedback = 0.30f;
    float duckAmount = 0.10f;
    float inputTrimDb = 0.0f;
    float outputTrimDb = 0.0f;
    bool freeze = false;
    bool atmosFdnOn = false;
    float wetWidth = 0.60f;
};

class TE2350CoreWrapper
{
public:
    bool prepare(double sampleRate, int maximumBlockSize);
    void reset();
    void setParameters(const CoreParameters& parameters);
    void processBlock(juce::AudioBuffer<float>& buffer);

    bool isReady() const noexcept { return ready; }
    float getEnvelope() const;
    float getModulator() const;

private:
    static q31_t toQ31(float value);
    static float normaliseLinear(float value, float minimum, float maximum);
    static float normaliseLog(float value, float minimum, float maximum);

    ::te2350_t core {};
    std::vector<q31_t> memoryPool;
    double currentSampleRate = 48000.0;
    int maxBlockSize = 0;
    float inputGain = 1.0f;
    float outputGain = 1.0f;
    bool ready = false;
};
}
