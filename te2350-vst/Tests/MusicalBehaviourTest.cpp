#include <cmath>
#include <vector>
#include <iostream>
#include <algorithm>
#include "PluginProcessor.h"

// Compare the actual plugin against continuously running wet and aligned dry
// references. Excitation during bypass catches input starvation, not just resets.
bool verifyPluginBypass(bool spillover, float quality, bool hostCallback)
{
    constexpr int blockSize = 128;
    constexpr int rampSamples = 960; // 20 ms at 48 kHz
    TE2350AudioProcessor toggled, continuous, dry;
    const auto set = [](TE2350AudioProcessor& p, const char* id, float value)
    {
        auto* parameter = p.apvts.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    for (auto* p : { &toggled, &continuous, &dry })
    {
        set(*p, "space", 0); set(*p, "wild", 0); set(*p, "bloom", 0);
        set(*p, "mix", spillover ? 1.0f : 0.0f);
        set(*p, "feedback", spillover ? 0.8f : 0.0f);
        set(*p, "timeMs", 50); set(*p, "syncMode", 0);
        set(*p, "modDepth", 0); set(*p, "shimmerAmount", 0);
        set(*p, "duckAmount", 0); set(*p, "inputTrim", 0);
        set(*p, "outputTrim", spillover ? 0.0f : -6.0f);
        set(*p, "qualityMode", quality);
        set(*p, "bypass", p == &dry ? 1.0f : 0.0f);
        p->prepareToPlay(48000.0, blockSize);
    }
    juce::AudioBuffer<float> a(2, blockSize), b(2, blockSize), d(2, blockSize);
    juce::MidiBuffer midi;
    float tailPeak = 0;
    for (int block = 0; block < 300; ++block)
    {
        if (block == 80) set(toggled, "bypass", 1);
        if (block == 140) set(toggled, "bypass", 0);
        for (int s = 0; s < blockSize; ++s)
        {
            const int n = block * blockSize + s;
            const float input = spillover
                ? ((block >= 90 && block < 120 && n % 503 == 0) ? 0.5f : 0.0f)
                : 0.2f * std::sin(static_cast<float>(n) * 0.037f);
            for (int ch = 0; ch < 2; ++ch)
                a.setSample(ch, s, spillover || ch == 0 ? input : -input);
        }
        b.makeCopyOf(a); d.makeCopyOf(a);
        if (hostCallback) toggled.processBlockBypassed(a, midi);
        else toggled.processBlock(a, midi);
        continuous.processBlock(b, midi); dry.processBlock(d, midi);
        for (int s = 0; s < blockSize; ++s)
        {
            const int n = block * blockSize + s;
            float bypass = 0;
            if (block >= 80 && block < 140)
                bypass = std::min(1.0f, static_cast<float>(n - 80 * blockSize + 1) / rampSamples);
            else if (block >= 140)
                bypass = std::max(0.0f, 1.0f - static_cast<float>(n - 140 * blockSize + 1) / rampSamples);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float expected = b.getSample(ch, s) * (1 - bypass) + d.getSample(ch, s) * bypass;
                if (!std::isfinite(a.getSample(ch, s)) || std::abs(a.getSample(ch, s) - expected) > 2.0e-5f)
                {
                    std::cerr << "Bypass " << (spillover ? "spillover" : "gain law")
                              << " FAILED: quality=" << quality << " host=" << hostCallback
                              << " sample=" << n << " expected=" << expected
                              << " actual=" << a.getSample(ch, s) << std::endl;
                    return false;
                }
                if (block >= 148) tailPeak = std::max(tailPeak, std::abs(b.getSample(ch, s)));
            }
        }
    }
    if (spillover && tailPeak < 1.0e-4f)
    {
        std::cerr << "Bypass spillover FAILED: reference tail is silent" << std::endl;
        return false;
    }
    std::cout << "Bypass " << (spillover ? "spillover" : "gain law")
              << " PASSED: quality=" << quality << " host=" << hostCallback << std::endl;
    return true;
}

extern "C" {
#include "../../include/te2350.h"
#include "../../include/dsp_pitch.h"
}

#define MEM_POOL_SIZE TE2350_REQUIRED_MEMORY_BYTES

double findPeakFrequency(const std::vector<float>& signal, double sampleRate)
{
    int zeroCrossings = 0;
    for (size_t i = 1; i < signal.size(); ++i)
    {
        if (signal[i - 1] <= 0.0f && signal[i] > 0.0f)
            zeroCrossings++;
    }
    return static_cast<double>(zeroCrossings) * sampleRate / static_cast<double>(signal.size());
}

double getSignalRMS(const std::vector<float>& signal) {
    double sumSq = 0.0;
    for (float val : signal) {
        sumSq += val * val;
    }
    return std::sqrt(sumSq / signal.size());
}

double getSignalPeak(const std::vector<float>& signal) {
    double peak = 0.0;
    for (float val : signal) {
        peak = std::max(peak, (double)std::abs(val));
    }
    return peak;
}

double getAMDepth(const std::vector<float>& signal, int windowSize) {
    std::vector<double> env;
    for(size_t i=0; i<signal.size() - windowSize; i += windowSize/4) {
        double max_val = 0;
        for(int j=0; j<windowSize; j++) {
            max_val = std::max(max_val, (double)std::abs(signal[i+j]));
        }
        env.push_back(max_val);
    }
    if (env.empty()) return 0.0;
    double max_env = *std::max_element(env.begin(), env.end());
    double min_env = *std::min_element(env.begin(), env.end());
    if (max_env < 1e-6) return 0.0;
    return (max_env - min_env) / max_env;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    bool allPassed = true;
    for (float quality : { 0.0f, 1.0f })
        for (bool hostCallback : { false, true })
            for (bool spillover : { false, true })
                allPassed = verifyPluginBypass(spillover, quality, hostCallback) && allPassed;
    constexpr double sampleRate = 48000.0;

    auto runPitchTest = [&](const char* name, int intervalIndex, double expectedFreq, double tolerance) {
        dsp_pitch_shifter_t ps;
        std::vector<q31_t> buffer(65536, 0);
        dsp_pitch_init(&ps, buffer.data(), 65536);

        q31_t pitch_ratio_q31;
        if (intervalIndex == 0) pitch_ratio_q31 = 0;
        else if (intervalIndex == 1) pitch_ratio_q31 = float_to_q31_safe(0.6666667f);
        else pitch_ratio_q31 = Q31_MAX;

        if (intervalIndex == 0) dsp_pitch_set_window_size(&ps, TE_SHIMMER_PITCH_SIZE);
        else if (intervalIndex == 1) dsp_pitch_set_window_size(&ps, (TE_SHIMMER_PITCH_SIZE * 3) / 4);
        else dsp_pitch_set_window_size(&ps, TE_SHIMMER_PITCH_SIZE / 2);

        const size_t numSamples = static_cast<size_t>(sampleRate * 2.0);
        std::vector<float> output;

        for (int i=0; i<100; i++) {
            dsp_pitch_process(&ps, 0, pitch_ratio_q31);
        }

        for (size_t i = 0; i < numSamples; i++)
        {
            float val = std::sin(2.0f * 3.1415926535f * 440.0f * (float)i / (float)sampleRate);
            q31_t in_q31 = float_to_q31_safe(val * 0.5f);

            q31_t out = dsp_pitch_process(&ps, in_q31, pitch_ratio_q31);

            if (i > numSamples / 2)
            {
                output.push_back(Q31_TO_FLOAT(out));
            }
        }

        double freq = findPeakFrequency(output, sampleRate);
        std::cout << "Pitch Test: " << name << " estimated frequency: " << freq << " Hz" << std::endl;
        if (std::abs(freq - expectedFreq) > tolerance)
        {
            std::cerr << "  FAILED: expected ~" << expectedFreq << " Hz (tol: " << tolerance << ")" << std::endl;
            allPassed = false;
        }
        else
        {
            std::cout << "  PASSED" << std::endl;
        }
    };

    auto runSpilloverTest = [&]() {
        std::vector<q31_t> memory_pool(MEM_POOL_SIZE / 4, 0);
        te2350_t pedal;
        std::fill(memory_pool.begin(), memory_pool.end(), 0);
        if (!te2350_init(&pedal, memory_pool.data(), MEM_POOL_SIZE, sampleRate)) {
            std::cerr << "Effect Init FAILED" << std::endl;
            allPassed = false;
            return;
        }

        te2350_set_mix(&pedal, float_to_q31_safe(1.0f));
        te2350_set_time(&pedal, float_to_q31_safe(0.1f));
        te2350_set_feedback(&pedal, float_to_q31_safe(0.8f));

        for (int i=0; i<100; i++) {
            q31_t out_l, out_r;
            te2350_process(&pedal, 0, &out_l, &out_r);
        }

        q31_t out_l, out_r;
        te2350_process(&pedal, float_to_q31_safe(0.99f), &out_l, &out_r);

        for (int i=0; i<10000; i++) {
            te2350_process(&pedal, 0, &out_l, &out_r);
        }

        float peak = 0.0f;
        for (int s=0; s<10000; s++) {
            te2350_process(&pedal, 0, &out_l, &out_r);
            peak = std::max(peak, std::abs(Q31_TO_FLOAT(out_l)));
        }

        if (peak <= 0.000001f) {
            std::cerr << "  FAILED Spillover Test: No initial tail. Peak=" << peak << std::endl;
            allPassed = false;
        } else {
            std::cout << "  Spillover Tail OK: Peak=" << peak << std::endl;
        }

        for (int i=0; i<100; i++) {
            te2350_process(&pedal, 0, &out_l, &out_r);
        }

        float newPeak = 0.0f;
        for (int s=0; s<10000; s++) {
            te2350_process(&pedal, 0, &out_l, &out_r);
            newPeak = std::max(newPeak, std::abs(Q31_TO_FLOAT(out_l)));
        }

        if (newPeak <= 0.0000001f) {
            std::cerr << "  FAILED Spillover Test: Tail died during core processing." << std::endl;
            allPassed = false;
        } else {
            std::cout << "  Spillover Test: PASSED" << std::endl;
        }
    };

    auto runFilterTest = [&]() {
        std::vector<q31_t> memory_pool(MEM_POOL_SIZE / 4, 0);
        te2350_t pedal;
        if (!te2350_init(&pedal, memory_pool.data(), MEM_POOL_SIZE, sampleRate)) {
            std::cerr << "Effect Init FAILED" << std::endl;
            allPassed = false;
            return;
        }
        // Test Low Cut
        std::fill(memory_pool.begin(), memory_pool.end(), 0);
        te2350_init(&pedal, memory_pool.data(), MEM_POOL_SIZE, sampleRate);
        te2350_set_mix(&pedal, float_to_q31_safe(1.0f));
        te2350_set_time(&pedal, float_to_q31_safe(0.1f));
        te2350_set_feedback(&pedal, float_to_q31_safe(0.0f));

        te2350_set_low_cut_coeff(&pedal, float_to_q31_safe(0.0f)); // Wide open
        float peakWideOpen = 0.0f;
        q31_t out_l, out_r;
        for (int i=0; i<48000; i++) {
            float val = std::sin(2.0f * 3.1415926535f * 50.0f * (float)i / (float)sampleRate);
            te2350_process(&pedal, float_to_q31_safe(val * 0.5f), &out_l, &out_r);
            if (i > 40000) peakWideOpen = std::max(peakWideOpen, std::abs(Q31_TO_FLOAT(out_l)));
        }

        if (peakWideOpen <= 1e-4f) {
            std::cerr << "  FAILED Low Cut Test: Baseline signal is silent." << std::endl;
            allPassed = false;
            return;
        }

        te2350_set_low_cut_coeff(&pedal, float_to_q31_safe(0.063f));
        float peakCut = 0.0f;
        for (int i=0; i<48000; i++) {
            float val = std::sin(2.0f * 3.1415926535f * 50.0f * (float)i / (float)sampleRate);
            te2350_process(&pedal, float_to_q31_safe(val * 0.5f), &out_l, &out_r);
            if (i > 40000) peakCut = std::max(peakCut, std::abs(Q31_TO_FLOAT(out_l)));
        }

        if (peakCut > peakWideOpen * 0.8f) {
            std::cerr << "  FAILED Low Cut Test: Low cut did not sufficiently attenuate 50Hz. Cut Peak=" << peakCut << " Open Peak=" << peakWideOpen << std::endl;
            allPassed = false;
        } else {
            std::cout << "  Low Cut Test: PASSED" << std::endl;
        }
    };

    auto runDuckingTest = [&]() {
        std::vector<q31_t> memory_pool(MEM_POOL_SIZE / 4, 0);
        te2350_t pedal;
        if (!te2350_init(&pedal, memory_pool.data(), MEM_POOL_SIZE, sampleRate)) {
            std::cerr << "Effect Init FAILED" << std::endl;
            allPassed = false;
            return;
        }
        std::fill(memory_pool.begin(), memory_pool.end(), 0);
        te2350_init(&pedal, memory_pool.data(), MEM_POOL_SIZE, sampleRate);
        te2350_set_mix(&pedal, float_to_q31_safe(1.0f));
        te2350_set_time(&pedal, float_to_q31_safe(0.1f));
        te2350_set_feedback(&pedal, float_to_q31_safe(0.8f));

        te2350_set_ducking(&pedal, 0);
        float peakNoDuck = 0.0f;
        q31_t out_l, out_r;

        for (int i=0; i<20000; i++) {
            float val = std::sin(2.0f * 3.1415926535f * 440.0f * (float)i / (float)sampleRate);
            te2350_process(&pedal, float_to_q31_safe(val * 0.5f), &out_l, &out_r);
        }

        for (int i=0; i<10000; i++) {
            float val = std::sin(2.0f * 3.1415926535f * 440.0f * (float)i / (float)sampleRate);
            te2350_process(&pedal, float_to_q31_safe(val * 0.5f), &out_l, &out_r);
            peakNoDuck = std::max(peakNoDuck, std::abs(Q31_TO_FLOAT(out_l)));
        }

        if (peakNoDuck <= 1e-4f) {
            std::cerr << "  FAILED Ducking Test: Baseline signal is silent." << std::endl;
            allPassed = false;
            return;
        }

        std::fill(memory_pool.begin(), memory_pool.end(), 0);
        te2350_init(&pedal, memory_pool.data(), MEM_POOL_SIZE, sampleRate);
        te2350_set_mix(&pedal, float_to_q31_safe(1.0f));
        te2350_set_time(&pedal, float_to_q31_safe(0.1f));
        te2350_set_feedback(&pedal, float_to_q31_safe(0.8f));

        te2350_set_ducking(&pedal, float_to_q31_safe(1.0f)); // Max ducking
        te2350_set_duck_threshold(&pedal, 0); // Threshold 0 to ALWAYS duck
        float peakDuck = 0.0f;

        for (int i=0; i<20000; i++) {
            float val = std::sin(2.0f * 3.1415926535f * 440.0f * (float)i / (float)sampleRate);
            te2350_process(&pedal, float_to_q31_safe(val * 0.5f), &out_l, &out_r);
        }

        for (int i=0; i<10000; i++) {
            float val = std::sin(2.0f * 3.1415926535f * 440.0f * (float)i / (float)sampleRate);
            te2350_process(&pedal, float_to_q31_safe(val * 0.5f), &out_l, &out_r);
            peakDuck = std::max(peakDuck, std::abs(Q31_TO_FLOAT(out_l)));
        }

        if (peakDuck > peakNoDuck * 0.95f) {
            std::cerr << "  FAILED Ducking Test: Signal was not properly ducked. Peak=" << peakDuck << " vs " << peakNoDuck << std::endl;
            allPassed = false;
        } else {
            std::cout << "  Ducking Test: PASSED" << std::endl;
        }
    };

    auto runPerlinTest = [&]() {
        dsp_perlin_t perlin;
        dsp_perlin_init(&perlin, 12345);

        q31_t v0 = dsp_perlin_1d(&perlin, 0);
        q31_t v25 = dsp_perlin_1d(&perlin, float_to_q31_safe(0.25f));
        q31_t v50 = dsp_perlin_1d(&perlin, float_to_q31_safe(0.50f));
        q31_t v75 = dsp_perlin_1d(&perlin, float_to_q31_safe(0.75f));
        q31_t v1 = dsp_perlin_1d(&perlin, Q31_MAX);

        std::cout << "  Perlin Test: " << v0 << ", " << v25 << ", " << v50 << ", " << v75 << ", " << v1 << std::endl;
        if (v0 == v25 && v25 == v50 && v50 == v75 && v75 == v1 && v0 == 0) {
            std::cerr << "  FAILED Perlin Test: All values are zero." << std::endl;
            allPassed = false;
        } else {
            std::cout << "  Perlin Test: PASSED" << std::endl;
        }
    };

    std::cout << "Running Pitch Shifter Tests..." << std::endl;
    runPitchTest("-1 oct", 0, 220.0, 20.0);
    runPitchTest("5th", 1, 660.0, 30.0);
    runPitchTest("+1 oct", 2, 880.0, 40.0);

    std::cout << "Running Spillover Tests..." << std::endl;
    runSpilloverTest();

    std::cout << "Running Filter Tests..." << std::endl;
    runFilterTest();

    std::cout << "Running Ducking Tests..." << std::endl;
    runDuckingTest();

    std::cout << "Running Perlin Tests..." << std::endl;
    runPerlinTest();

    return allPassed ? 0 : 1;
}
