#include "PluginProcessor.h"
#include "Presets/FactoryPresets.h"
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
constexpr int sr = 48000, bs = 128, duration = 12, active = 3;
void set(TE2350AudioProcessor &p, const char *id, float v)
{
    auto *a = p.apvts.getParameter(id);
    if (!a)
        throw std::runtime_error(id);
    a->setValueNotifyingHost(a->convertTo0to1(v));
}
void defaults(TE2350AudioProcessor &p)
{
    for (const auto &s : te2350::getParameterSpecs())
        set(p, s.id.toRawUTF8(), s.defaultValue);
    set(p, "space", 0);
    set(p, "wild", 0);
    set(p, "bloom", 0);
}
void settle(te2350::MacroEngine &e, TE2350AudioProcessor &p)
{
    for (int i = 0; i < 100; ++i)
        e.update(p.apvts, bs);
}
struct Case
{
    juce::String name;
    float space = 0, wild = 0, bloom = 0, feedback = .45f, shimmer = 0, regen = .3f, width = .6f;
    int freeze = 0, atmos = 0, preset = -1;
    bool wetOnly = false;
    juce::String manualID;
    float manual = 0;
};
float source(int n, int type)
{
    double t = double(n) / sr;
    if (n >= active * sr)
        return 0;
    if (type == 0)
        return n == 0 ? .5f : 0;
    if (type == 1)
        return float(.12 *
                     (std::sin(t * 1382.3007676) + .3 * std::sin(t * 2764.6015352) +
                      .15 * std::sin(t * 4146.9023028)) *
                     std::min(1., t * 100) * std::min(1., (active - t) * 100));
    if (type == 2)
    {
        double phase = std::fmod(t, .5);
        unsigned int r = static_cast<unsigned int>(n) * 1664525u + 1013904223u;
        double noise = double(r) / 2147483648. - 1.;
        return float((.25 * std::sin(phase * 450) + .06 * noise) * std::exp(-phase * 25));
    }
    return float(.065 * (std::sin(t * 823.43356) + std::sin(t * 1036.725575) + std::sin(t * 1233.13277)) *
                 std::min(1., t * 4) * std::min(1., (active - t) * 4));
}
bool wav(juce::File f, const juce::AudioBuffer<float> &b)
{
    f.deleteFile();
    auto stream = f.createOutputStream();
    if (!stream)
        return false;
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::AudioFormatWriter> w(fmt.createWriterFor(stream.release(), sr, 2, 24, {}, 0));
    return w && w->writeFromAudioSampleBuffer(b, 0, b.getNumSamples());
}
struct Metrics
{
    double rms = 0, peak = 0, tail = 0, width = 0, centroid = 0, hf = 0, lf = 0, mod = 0, pitch = 0,
           transient = 0, decay = 0, density = 0, dc = 0, last = 0;
};
Metrics measure(const juce::AudioBuffer<float> &a)
{
    Metrics m;
    double e = 0, midE = 0, sideE = 0, te = 0, ae = 0, le = 0, sum = 0, difference = 0;
    int occupied = 0;
    std::vector<double> bins(duration * 10, 0);
    float previous = 0;
    for (int n = 0; n < a.getNumSamples(); ++n)
    {
        double l = a.getSample(0, n), r = a.getSample(1, n), mid = (l + r) * .5, side = (l - r) * .5;
        double energy = (l * l + r * r) * .5;
        if (!std::isfinite(energy))
            throw std::runtime_error("nonfinite audio");
        e += energy;
        midE += mid * mid;
        sideE += side * side;
        sum += mid;
        m.peak = std::max(m.peak, std::max(std::abs(l), std::abs(r)));
        if (n >= active * sr)
            te += energy;
        if (n < sr / 10)
            ae += energy;
        if (n >= (duration - 1) * sr)
            le += energy;
        bins[static_cast<size_t>(n / (sr / 10))] += energy;
        if (n >= active * sr && energy > 1.e-8)
            ++occupied;
        difference += (mid - previous) * (mid - previous);
        previous = float(mid);
    }
    m.rms = std::sqrt(e / a.getNumSamples());
    m.tail = te / sr;
    m.width = std::sqrt(sideE / std::max(1.e-20, midE + sideE));
    m.transient = std::sqrt(ae / (sr / 10));
    m.mod = std::sqrt(difference / a.getNumSamples());
    m.dc = sum / a.getNumSamples();
    m.last = std::sqrt(le / sr);
    m.density = double(occupied) / ((duration - active) * sr);
    // Last 100 ms bin above -40 dB of maximum: duration proxy, NOT fitted RT60.
    double maximum = *std::max_element(bins.begin(), bins.end());
    for (size_t i = 0; i < bins.size(); ++i)
        if (bins[i] > maximum * 1.e-4)
            m.decay = (i + 1) * .1;
    juce::dsp::FFT fft(11);
    std::array<float, 4096> data{};
    double power = 0, weighted = 0, high = 0, low = 0, pitchSum = 0, pitchSq = 0;
    int frames = 0;
    // Dominant-bin variance is a pitch/movement proxy; not a pitch tracker.
    for (int n = 0; n + 2048 < a.getNumSamples(); n += 2048)
    {
        data.fill(0);
        for (int j = 0; j < 2048; ++j)
            data[static_cast<size_t>(j)] =
                (a.getSample(0, n + j) + a.getSample(1, n + j)) * .5f *
                float(.5 - .5 * std::cos(juce::MathConstants<double>::twoPi * j / 2047));
        fft.performFrequencyOnlyForwardTransform(data.data());
        double frame = 0;
        int dominant = 1;
        for (int j = 1; j <= 1024; ++j)
        {
            double p = double(data[static_cast<size_t>(j)]) * data[static_cast<size_t>(j)];
            double hz = double(j) * sr / 2048;
            frame += p;
            power += p;
            weighted += p * hz;
            if (hz > 6000)
                high += p;
            if (hz < 200)
                low += p;
            if (data[static_cast<size_t>(j)] > data[static_cast<size_t>(dominant)])
                dominant = j;
        }
        if (frame > 1.e-7)
        {
            double hz = double(dominant) * sr / 2048;
            pitchSum += hz;
            pitchSq += hz * hz;
            ++frames;
        }
    }
    m.centroid = weighted / std::max(1.e-20, power);
    m.hf = high / std::max(1.e-20, power);
    m.lf = low / std::max(1.e-20, power);
    if (frames)
        m.pitch = std::max(0., pitchSq / frames - std::pow(pitchSum / frames, 2));
    return m;
}
juce::AudioBuffer<float> render(const Case &c, int type)
{
    TE2350AudioProcessor p;
    if (c.preset >= 0)
        p.setCurrentProgram(c.preset);
    else
    {
        defaults(p);
        set(p, "space", c.space);
        set(p, "wild", c.wild);
        set(p, "bloom", c.bloom);
        set(p, "feedback", c.feedback);
        set(p, "shimmerAmount", c.shimmer);
        set(p, "shimmerFeedback", c.regen);
        set(p, "wetWidth", c.width);
        set(p, "atmosFdnOn", float(c.atmos));
    }
    if (c.wetOnly)
        set(p, "killDry", 1);
    if (c.manualID.isNotEmpty())
        set(p, c.manualID.toRawUTF8(), c.manual);
    p.prepareToPlay(sr, bs);
    juce::AudioBuffer<float> b(2, bs), out(2, duration * sr);
    out.clear();
    juce::MidiBuffer midi;
    for (int i = 0; i < 100; ++i)
    {
        b.clear();
        p.processBlock(b, midi);
    }
    for (int n = 0; n < out.getNumSamples(); n += bs)
    {
        if (c.freeze && n == sr * 2)
            set(p, "freezeEngage", 1);
        for (int j = 0; j < bs; ++j)
            for (int ch = 0; ch < 2; ++ch)
                b.setSample(ch, j, source(n + j, type));
        p.processBlock(b, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom(ch, n, b, ch, 0, bs);
    }
    return out;
}
bool audit(juce::File dir, bool baseline)
{
    TE2350AudioProcessor p;
    te2350::MacroEngine e;
    e.prepare(sr, bs);
    std::ofstream csv(dir.getChildFile("effective_mapping.csv").getFullPathName().toStdString());
    csv << "macro,target,manual_fraction,manual,value@0,value@25,value@50,value@75,value@100\n";
    bool ok = true;
    for (const auto &d : te2350::MacroEngine::createFactoryDefinitions())
        for (const auto &t : d.targets)
        {
            const auto *spec = te2350::findParameterSpec(t.paramID);
            for (float manual : {-1.f, 0.f, .25f, .5f, .75f, 1.f})
            {
                defaults(p);
                float v = manual < 0 ? spec->defaultValue
                                     : spec->minimum + manual * (spec->maximum - spec->minimum);
                set(p, t.paramID.toRawUTF8(), v);
                csv << d.macroID << ',' << t.paramID << ',' << manual << ',' << v;
                float previous = 0;
                for (int k = 0; k <= 4; ++k)
                {
                    set(p, d.macroID.toRawUTF8(), k * .25f);
                    settle(e, p);
                    float actual = e.getEffectiveValue(t.paramID, 0);
                    csv << ',' << actual;
                    ok &= std::isfinite(actual) && actual >= spec->minimum - 1.e-3 &&
                          actual <= spec->maximum + 1.e-3;
                    if (!baseline && k && manual >= 0 && manual < 1 &&
                        previous > spec->minimum + 1.e-4 && previous < spec->maximum - 1.e-4)
                        ok &= std::abs(actual - previous) > 1.e-5;
                    previous = actual;
                }
                csv << '\n';
            }
        }
    // All three macros at all five positions; manual surface at five fractions.
    for (float s : {0.f, .25f, .5f, .75f, 1.f})
        for (float w : {0.f, .25f, .5f, .75f, 1.f})
            for (float b : {0.f, .25f, .5f, .75f, 1.f})
            {
                for (float manual : {0.f, .25f, .5f, .75f, 1.f})
                {
                    defaults(p);
                    set(p, "space", s);
                    set(p, "wild", w);
                    set(p, "bloom", b);
                    for (const auto &spec : te2350::getParameterSpecs())
                        if (spec.id != "space" && spec.id != "wild" && spec.id != "bloom")
                            set(p, spec.id.toRawUTF8(),
                                spec.minimum + manual * (spec.maximum - spec.minimum));
                    settle(e, p);
                    for (const auto &spec : te2350::getParameterSpecs())
                    {
                        float v = e.getEffectiveValue(spec.id, 0);
                        ok &= std::isfinite(v) && v >= spec.minimum - .001 && v <= spec.maximum + .001;
                    }
                }
            }
    // Instability must accelerate usefully across the complete WILD range.
    defaults(p);
    float lastInstability = -1;
    for (int w = 0; w <= 10; ++w)
    {
        set(p, "wild", w * .1f);
        settle(e, p);
        float v = e.getInstability();
        if (!baseline && w)
            ok &= v > lastInstability + .01f;
        lastInstability = v;
    }
    // Preserve manual authority at macro maximum, including endpoints and Tone.
    for (const char *id : {"feedback", "highCutHz", "wetWidth", "shimmerAmount", "duckAmount", "diffusion",
                           "modDepth", "chaos", "wobble", "modRateHz"})
    {
        defaults(p);
        set(p, "space", 1);
        set(p, "wild", 1);
        set(p, "bloom", 1);
        const auto *spec = te2350::findParameterSpec(id);
        float previous = -1;
        for (int k = 0; k <= 4; ++k)
        {
            set(p, id, spec->minimum + k * .25f * (spec->maximum - spec->minimum));
            settle(e, p);
            float v = e.getEffectiveValue(id, 0);
            if (!baseline && k)
                ok &= v > previous + 1.e-5;
            previous = v;
        }
    }
    std::ofstream surface(dir.getChildFile("duck_surface.csv").getFullPathName().toStdString());
    surface << "bloom,manual_duck,effective_duck\n";
    for (int b = 0; b <= 10; ++b)
    {
        float previous = -1;
        for (int d = 0; d <= 10; ++d)
        {
            defaults(p);
            set(p, "bloom", b * .1f);
            set(p, "duckAmount", d * .1f);
            settle(e, p);
            float v = e.getEffectiveValue("duckAmount", 0);
            surface << b * .1f << ',' << d * .1f << ',' << v << '\n';
            ok &= v > previous + .01f;
            previous = v;
        }
    }
    // Check the M8 equation and both axes, including exact saturation only at manual=1.
    for (int d = 0; d <= 10; ++d)
    {
        float previous = -1;
        for (int b = 0; b <= 10; ++b)
        {
            defaults(p);
            const float manual = d * .1f, bloom = b * .1f;
            set(p, "duckAmount", manual);
            set(p, "bloom", bloom);
            settle(e, p);
            const float actual = e.getEffectiveValue("duckAmount", 0);
            const float expected = manual + .45f * bloom * bloom * (1 - manual);
            ok &= std::abs(actual - expected) < 1.e-5f;
            if (b && d < 10)
                ok &= actual > previous + 1.e-5f;
            previous = actual;
        }
    }
    std::cout << "Effective mapping/range/manual headroom " << (ok ? "PASSED" : "FAILED") << '\n';
    return ok;
}
} // namespace
int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File dir = argc > 1
                         ? juce::File(juce::String::fromUTF8(argv[1]))
                         : juce::File::getCurrentWorkingDirectory().getChildFile("MacroCalibrationOutput");
    bool baseline = argc > 2 && juce::String(argv[2]) == "baseline";
    if (!dir.createDirectory())
        return 1;
    bool ok = audit(dir, baseline);
    std::vector<Case> cases;
    for (int macro = 0; macro < 3; ++macro)
        for (int k = 0; k <= 10; ++k)
        {
            Case c;
            c.name = juce::String(macro == 0   ? "space"
                                  : macro == 1 ? "wild"
                                               : "bloom") +
                     "_" + juce::String(k * 10);
            if (macro == 0)
                c.space = k * .1f;
            else if (macro == 1)
                c.wild = k * .1f;
            else
                c.bloom = k * .1f;
            cases.push_back(c);
        }
    for (int s = 0; s <= 4; ++s)
        for (int b = 0; b <= 4; ++b)
        {
            Case c;
            c.name = "space_bloom_" + juce::String(s * 25) + "_" + juce::String(b * 25);
            c.space = s * .25f;
            c.bloom = b * .25f;
            cases.push_back(c);
        }
    for (int w = 0; w <= 4; ++w)
        for (int f : {20, 45, 70, 90})
        {
            Case c;
            c.name = "wild_feedback_" + juce::String(w * 25) + "_" + juce::String(f);
            c.wild = w * .25f;
            c.feedback = f * .01f;
            cases.push_back(c);
        }
    for (int b = 0; b <= 4; ++b)
        for (int s = 0; s <= 3; ++s)
            for (float r : {.3f, .75f})
            {
                Case c;
                c.name = "bloom_shimmer_" + juce::String(b * 25) + "_" + juce::String(s * 25) + "_" +
                         juce::String(r);
                c.bloom = b * .25f;
                c.shimmer = s * .25f;
                c.regen = r;
                cases.push_back(c);
            }
    for (int w = 0; w <= 4; ++w)
        for (int a = 0; a <= 1; ++a)
        {
            Case c;
            c.name = "wild_freeze_" + juce::String(w * 25) + "_" + juce::String(a);
            c.wild = w * .25f;
            c.freeze = 1;
            c.atmos = a;
            cases.push_back(c);
        }
    for (int s = 0; s <= 2; ++s)
        for (int w = 0; w <= 2; ++w)
        {
            Case c;
            c.name = "space_width_" + juce::String(s * 50) + "_" + juce::String(w * 50);
            c.space = s * .5f;
            c.width = w * .5f;
            cases.push_back(c);
        }
    for (int s = 0; s <= 2; ++s)
        for (int w = 0; w <= 2; ++w)
            for (int b = 0; b <= 2; ++b)
            {
                Case c;
                c.name = "all_axes_" + juce::String(s * 50) + "_" + juce::String(w * 50) + "_" +
                         juce::String(b * 50);
                c.space = s * .5f;
                c.wild = w * .5f;
                c.bloom = b * .5f;
                c.wetOnly = true;
                cases.push_back(c);
            }
    for (const char *id : {"feedback", "highCutHz", "wetWidth", "shimmerAmount", "duckAmount", "diffusion",
                           "modDepth", "chaos", "wobble", "modRateHz"})
    {
        const auto *spec = te2350::findParameterSpec(id);
        for (int k = 0; k <= 4; ++k)
        {
            Case c;
            c.name = "manual_" + juce::String(id) + "_" + juce::String(k * 25);
            c.space = c.wild = c.bloom = 1;
            c.manualID = id;
            c.manual = spec->minimum + k * .25f * (spec->maximum - spec->minimum);
            cases.push_back(c);
        }
    }
    auto names = te2350::getFactoryPresetNames();
    for (int i = 0; i < names.size(); ++i)
    {
        Case c;
        c.name = "preset_" + juce::String(i);
        c.preset = i;
        cases.push_back(c);
    }
    std::ofstream csv(dir.getChildFile("audio_metrics.csv").getFullPathName().toStdString());
    csv << "case,source,rms,peak,tail_energy,width,centroid,hf_ratio,lf_ratio,modulation_proxy,pitch_"
           "variance_proxy,transient_rms,decay_proxy_seconds,tail_density,dc,last_second_rms,adjacent_"
           "relative_difference\n";
    std::array<juce::AudioBuffer<float>, 4> previousAudio;
    std::array<double, 4> previousBloomTail{}, initialBloomTail{};
    double previousSpaceDecay = 0;
    for (const auto &c : cases)
    {
        // Individual axes: four sources. Interactions/presets: harmonic and percussion.
        int first = c.name.startsWith("space_") && !c.name.startsWith("space_bloom") &&
                            !c.name.startsWith("space_width")
                        ? 0
                    : c.name.startsWith("wild_") && !c.name.startsWith("wild_feedback") &&
                            !c.name.startsWith("wild_freeze")
                        ? 0
                    : c.name.startsWith("bloom_") && !c.name.startsWith("bloom_shimmer") ? 0
                                                                                         : 1;
        int last = first == 0 ? 3 : 2;
        for (int type = first; type <= last; ++type)
        {
            auto audio = render(c, type);
            auto m = measure(audio);
            if (!baseline && first == 0 && c.name.startsWith("bloom_") && type > 0)
            {
                size_t index = static_cast<size_t>(type);
                if (c.name == "bloom_0")
                    initialBloomTail[index] = m.tail;
                else if (m.tail < previousBloomTail[index] * .995)
                {
                    std::cerr << "Bloom tail reversal " << c.name << " source=" << type << '\n';
                    ok = false;
                }
                if (c.name == "bloom_100" && m.tail < initialBloomTail[index] * 1.25)
                {
                    std::cerr << "Bloom tail expansion too small\n";
                    ok = false;
                }
                previousBloomTail[index] = m.tail;
            }
            if (!baseline && first == 0 && c.name.startsWith("space_") && type == 2)
            {
                if (c.name != "space_0" && m.decay + .1 < previousSpaceDecay)
                {
                    std::cerr << "Space percussive duration reversal\n";
                    ok = false;
                }
                previousSpaceDecay = m.decay;
            }
            double relative = 0;
            if (first == 0)
            {
                auto &prev = previousAudio[static_cast<size_t>(type)];
                if (!c.name.endsWith("_0") && prev.getNumSamples() == audio.getNumSamples())
                {
                    double delta = 0, energy = 0;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int n = 0; n < audio.getNumSamples(); ++n)
                        {
                            double v = audio.getSample(ch, n), old = prev.getSample(ch, n);
                            delta += (v - old) * (v - old);
                            energy += old * old;
                        }
                    relative = std::sqrt(delta / std::max(1.e-20, energy));
                    if (!baseline && relative < .001)
                    {
                        std::cerr << "Dead audio interval " << c.name << " source=" << type << '\n';
                        ok = false;
                    }
                }
                prev.makeCopyOf(audio);
            }
            csv << c.name << ',' << type << ',' << m.rms << ',' << m.peak << ',' << m.tail << ',' << m.width
                << ',' << m.centroid << ',' << m.hf << ',' << m.lf << ',' << m.mod << ',' << m.pitch << ','
                << m.transient << ',' << m.decay << ',' << m.density << ',' << m.dc << ',' << m.last << ','
                << relative << '\n';
            // Fixed input amplitude; no normalization. Freeze may sustain indefinitely.
            bool safe = m.peak < .99 && m.rms > 1.e-7 && std::abs(m.dc) < .02 && (c.freeze || m.last < .15);
            if (c.freeze)
            {
                // Whole-render DC can conceal opposite offsets in successive windows.
                // Check each channel separately after Freeze engages, without imposing decay.
                for (int start = 2 * sr; start < audio.getNumSamples(); start += sr)
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        double sum = 0;
                        for (int n = start; n < start + sr; ++n)
                            sum += audio.getSample(ch, n);
                        safe &= std::abs(sum / sr) < .02;
                    }
            }
            if (!safe)
            {
                std::cerr << "Unsafe/silent " << c.name << " source=" << type << " peak=" << m.peak
                          << " last=" << m.last << '\n';
                ok = false;
            }
            if (!wav(dir.getChildFile(c.name + "_source" + juce::String(type) + ".wav"), audio))
                return 1;
        }
        std::cout << c.name << " rendered\n" << std::flush;
    }
    std::cout << (ok ? "MacroCalibration PASSED" : "MacroCalibration FAILED") << '\n';
    return ok ? 0 : 1;
}
