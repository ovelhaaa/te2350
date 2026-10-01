// M11 offline musical audit. Production DSP and macro mappings are unchanged.
#include "PluginProcessor.h"
#include "Presets/FactoryPresets.h"
#include "M10FactorySnapshot.h"
#include <cstring>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace
{
constexpr int sr = 48000, bs = 128, active = 4, duration = 24;
const std::array<const char *, 12> sources{{"impulse", "pluck", "vocal", "pad", "percussion", "chords", "piano", "major", "minor", "suspended", "cluster", "bass"}};
void set(TE2350AudioProcessor &p, const char *id, float v)
{
    auto *a = p.apvts.getParameter(id);
    if (!a)
        throw std::runtime_error(id);
    a->setValueNotifyingHost(a->convertTo0to1(v));
}
float source(int n, int type)
{
    if (n >= active * sr)
        return 0;
    const double t = double(n) / sr, tau = juce::MathConstants<double>::twoPi;
    const double fade = std::min(1., t * 100) * std::min(1., (active - t) * 100);
    if (type == 0)
        return n == 0 ? .5f : 0;
    if (type == 1)
    {
        const std::array<double, 8> notes{
            {220, 277.1826, 329.6276, 440, 246.9417, 293.6648, 369.9944, 493.8833}};
        double u = std::fmod(t, .5), f = notes[static_cast<size_t>(t / .5) % 8], x = 0;
        for (int h = 1; h <= 8; ++h)
            x += std::sin(tau * f * h * u) * std::exp(-u * (4 + h)) * .16 / (h * h);
        return float(x * std::min(1., u * 2000));
    }
    if (type == 11)
        return float(.18 * std::sin(tau * 55 * t) * fade + .05 * std::sin(tau * 110 * t) * fade);
    if (type == 6)
    {
        const double u = std::fmod(t, .5), f = 261.6256 * std::pow(2., int(t / .5) % 5 / 12.);
        double x = 0;
        for (int h = 1; h <= 12; ++h)
            x += .13 / (h * h) * std::sin(tau * f * (h + .0003 * h * h) * u) * std::exp(-u * (2.5 + h));
        return float(x * std::min(1., u * 1500));
    }
    if (type == 2)
    {
        double x = 0;
        for (int h = 1; h <= 12; ++h)
        {
            double hz = 220. * h, formant = std::exp(-std::pow((hz - 750) / 250, 2)) +
                                            .6 * std::exp(-std::pow((hz - 1250) / 300, 2)) + .12;
            x += std::sin(tau * hz * t) * formant * .08 / h;
        }
        return float(x * fade * (.75 + .25 * std::sin(tau * 2 * t)));
    }
    if (type == 4)
    {
        double u = std::fmod(t, .5);
        unsigned r = static_cast<unsigned>(n) * 747796405u + 2891336453u;
        r = ((r >> ((r >> 28u) + 4u)) ^ r) * 277803737u;
        r = (r >> 22u) ^ r;
        double noise = double(r) / 2147483648. - 1;
        return float(.22 * std::sin(tau * 65 * u) * std::exp(-u * 22) + .09 * noise * std::exp(-u * 45));
    }
    std::array<std::array<double, 4>, 4> chords{{{{130.8128, 164.8138, 195.9977, 261.6256}},
                                                 {{110, 130.8128, 164.8138, 220}},
                                                 {{87.3071, 110, 130.8128, 174.6141}},
                                                 {{97.9989, 123.4708, 146.8324, 195.9977}}}};
    if (type >= 7)
        chords = {{{{130.8128,164.8138,195.9977,261.6256}}, {{130.8128,155.5635,195.9977,261.6256}},
                   {{130.8128,174.6141,195.9977,261.6256}}, {{130.8128,138.5913,146.8324,195.9977}}}};
    const auto &notes = chords[type >= 7 ? type - 7 : (type == 3 ? 0 : static_cast<size_t>(t) % 4)];
    double u = std::fmod(t, 1.), x = 0;
    for (double f : notes)
        x += .038 * (std::sin(tau * f * t) + .2 * std::sin(tau * f * 2 * t));
    return float(x * (type == 3 ? std::min(1., t * 3) * std::min(1., (active - t) * 3)
                                : std::exp(-u * 1.8) * std::min(1., u * 500)));
}
bool wav(const juce::File &f, const juce::AudioBuffer<float> &b)
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

te2350::CoreParameters resolved(TE2350AudioProcessor &p, te2350::MacroEngine *running = nullptr)
{
    te2350::MacroEngine settled;
    settled.prepare(sr, 64);
    if (!running)
        for (int i = 0; i < 100; ++i)
            settled.update(p.apvts, bs);
    auto &e = running ? *running : settled;
    te2350::CoreParameters c;
#define E(field, id) c.field = e.getEffectiveValue(id, 0)
#define R(field, id) c.field = p.apvts.getRawParameterValue(id)->load()
    R(space, "space");
    R(wild, "wild");
    R(bloom, "bloom");
    E(timeMs, "timeMs");
    E(feedback, "feedback");
    E(mix, "mix");
    E(lowCutHz, "lowCutHz");
    E(highCutHz, "highCutHz");
    E(diffusion, "diffusion");
    E(chaos, "chaos");
    E(wobble, "wobble");
    E(presence, "presence");
    E(modRateHz, "modRateHz");
    E(modDepth, "modDepth");
    R(modShape, "modShape");
    R(shimmerInterval, "shimmerInterval");
    E(shimmerAmount, "shimmerAmount");
    E(shimmerFeedback, "shimmerFeedback");
    R(duckThresholdDb, "duckThreshold");
    E(duckAmount, "duckAmount");
    R(inputTrimDb, "inputTrim");
    R(outputTrimDb, "outputTrim");
    R(freeze, "freezeEngage");
    R(atmosFdnOn, "atmosFdnOn");
    E(wetWidth, "wetWidth");
    R(killDry, "killDry");
#undef E
#undef R
    // The standalone processor uses its documented 120 BPM fallback.
    const int sync = int(p.apvts.getRawParameterValue("syncMode")->load());
    const float times[] = {0, 500, 250, 375, 500.f / 3, 125};
    if (sync)
        c.timeMs = times[sync];
    return c;
}
void safety(const juce::AudioBuffer<float> &a, const Metrics &m, const std::string &label)
{
    for (int channel = 0; channel < a.getNumChannels(); ++channel)
    {
        double sum = 0;
        for (int n = 0; n < a.getNumSamples(); ++n)
            sum += a.getSample(channel, n);
        if (std::abs(sum / a.getNumSamples()) > .005)
            throw std::runtime_error("channel DC: " + label);
    }
    // Input peak <= .5. Conservative clipping/DC/runaway gates, not loudness targets.
    if (m.peak > .98 || std::abs(m.dc) > .005 || m.last > .12)
        throw std::runtime_error("unsafe render: " + label);
    double penultimate = 0;
    for (int c = 0; c < 2; ++c)
        for (int n = (duration - 4) * sr; n < (duration - 2) * sr; ++n)
        {
            const double x = a.getSample(c, n);
            penultimate += x * x;
        }
    const double previous = std::sqrt(penultimate / (4 * sr));
    if (m.last > std::max(.003, previous * 2))
        throw std::runtime_error("growing late tail: " + label);
}
juce::AudioBuffer<float> render(int index, int type, int mode, bool freezePerformance = false, bool lowDiffusion = false)
{
    TE2350AudioProcessor p;
    p.setCurrentProgram(index);
    if (lowDiffusion)
        set(p, "diffusion", .08f);
    if (mode != 0)
        set(p, "killDry", 1);
    p.prepareToPlay(sr, bs);
    te2350::MacroEngine engine;
    engine.prepare(sr, 64);
    te2350::OversamplingChain alignment;
    alignment.prepare(sr, bs, 2);
    auto params = resolved(p);
    te2350::TE2350CoreWrapper diagnostic;
    if (mode == 2)
    {
        if (!diagnostic.prepare(sr, bs))
            throw std::runtime_error("core prepare");
        params.shimmerAmount = 0;
        params.shimmerFeedback = 0;
    }
    juce::AudioBuffer<float> out(2, duration * sr), block(2, bs);
    juce::MidiBuffer midi;
    for (int i = 0; i < 100; ++i)
    {
        block.clear();
        if (mode == 2)
        {
            engine.update(p.apvts, block.getNumSamples());
            params = resolved(p, &engine);
            params.shimmerAmount = 0;
            params.shimmerFeedback = 0;
            diagnostic.setParameters(params);
            diagnostic.processBlock(block);
            alignment.processEffectBlock(block);
        }
        else
            p.processBlock(block, midi);
    }
    for (int pos = 0; pos < out.getNumSamples(); pos += bs)
    {
        if (freezePerformance && pos == active * sr)
            set(p, "freezeEngage", 1);
        if (freezePerformance && pos == 12 * sr)
            set(p, "freezeEngage", 0);
        const int count = std::min(bs, out.getNumSamples() - pos);
        block.setSize(2, count, false, false, true);
        for (int n = 0; n < count; ++n)
            for (int ch = 0; ch < 2; ++ch)
                block.setSample(ch, n, source(pos + n, type));
        if (mode == 2)
        {
            engine.update(p.apvts, block.getNumSamples());
            params = resolved(p, &engine);
            params.shimmerAmount = 0;
            params.shimmerFeedback = 0;
            diagnostic.setParameters(params);
            diagnostic.processBlock(block);
            alignment.processEffectBlock(block);
        }
        else
            p.processBlock(block, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom(ch, pos, block, ch, 0, count);
    }
    return out;
}
void verifyLegacy()
{
    for (int i = 0; i < 13; ++i)
    {
        TE2350AudioProcessor actual, expected;
        actual.setCurrentProgram(i);
        m10Snapshot::applyFactoryPreset(expected.apvts, i);
        if (actual.getProgramName(i) != m10Snapshot::getFactoryPresetNames()[i])
            throw std::runtime_error("legacy name/order changed");
        for (auto* a : actual.getParameters())
        {
            auto* id = dynamic_cast<juce::AudioProcessorParameterWithID*>(a);
            const float x = a->getValue(), y = expected.apvts.getParameter(id->paramID)->getValue();
            const float rawX = actual.apvts.getRawParameterValue(id->paramID)->load();
            const float rawY = expected.apvts.getRawParameterValue(id->paramID)->load();
            if (std::memcmp(&x, &y, sizeof(float)) || std::memcmp(&rawX, &rawY, sizeof(float)))
                throw std::runtime_error("legacy bits changed: " + id->paramID.toStdString());
        }
    }
    std::cout << "M10 legacy: all 13 names/order and all saved parameter bits preserved" << std::endl;
}
void verifyNewRoles()
{
    const char* names[] = {"Long Shadow", "Afterimage", "Diffuse Halo", "Fifth Nebula", "Submerged Choir", "Prism Drift", "Ghost Room"};
    for (int i = 13; i < 20; ++i)
    {
        TE2350AudioProcessor p; p.setCurrentProgram(i);
        const auto c = resolved(p);
        if (p.getProgramName(i) != names[i-13] || c.space >= .9 || c.wild >= .9 || c.bloom >= .9 || c.freeze)
            throw std::runtime_error("new preset identity/headroom");
        if ((i == 13 || i == 14 || i == 19) && (c.mix > .25 || c.shimmerFeedback > .00001))
            throw std::runtime_error("low-mix ambience role");
        if (i == 14 && c.duckAmount < .7) throw std::runtime_error("afterimage ducking");
        if (i >= 15 && i <= 17 && (c.diffusion < .9 || c.shimmerAmount < .2 || c.shimmerInterval != (i == 15 ? 2 : i == 16 ? 1 : 0)))
            throw std::runtime_error("diffuse harmonic role");
        if (i == 18 && (c.wild < .6 || c.shimmerFeedback > .2)) throw std::runtime_error("pitch motion role");
    }
}
void verifyDiagnosticParity()
{
    for (int index : {0, 5, 9, 10})
    {
        TE2350AudioProcessor p;
        p.setCurrentProgram(index);
        set(p, "killDry", 1);
        p.prepareToPlay(sr, bs);
        te2350::MacroEngine engine;
        engine.prepare(sr, 64);
        te2350::OversamplingChain alignment;
        alignment.prepare(sr, bs, 2);
        te2350::TE2350CoreWrapper core;
        if (!core.prepare(sr, bs))
            throw std::runtime_error("core prepare");
        juce::AudioBuffer<float> a(2, bs), b(2, bs);
        juce::MidiBuffer midi;
        for (int k = -100; k < 1500; ++k)
        {
            for (int n = 0; n < bs; ++n)
                for (int ch = 0; ch < 2; ++ch)
                    a.setSample(ch, n, k < 0 ? 0 : source(k * bs + n, 1));
            b.makeCopyOf(a);
            p.processBlock(a, midi);
            engine.update(p.apvts, bs);
            core.setParameters(resolved(p, &engine));
            core.processBlock(b);
            alignment.processEffectBlock(b);
            for (int n = 0; n < bs; ++n)
                for (int ch = 0; ch < 2; ++ch)
                    if (std::abs(a.getSample(ch, n) - b.getSample(ch, n)) > 2.e-5f)
                        throw std::runtime_error("diagnostic path differs from processor");
        }
    }
}
void transitions(const juce::File &dir)
{
    TE2350AudioProcessor p;
    p.prepareToPlay(sr, bs);
    juce::AudioBuffer<float> block(2, bs);
    juce::MidiBuffer midi;
    std::ofstream log(dir.getChildFile("transitions.csv").getFullPathName().toStdString());
    log << "from,to,peak,rms_db,adjacent_rms_delta_db\n";
    double previousDb = 0;
    int from = -1;
    for (int k = 0; k < 40; ++k)
    {
        int index = k < 20 ? k : 39 - k;
        p.setCurrentProgram(index);
        TE2350AudioProcessor expected;
        expected.setCurrentProgram(index);
        if (p.getCurrentProgram() != index || p.getActivePresetName() != expected.getActivePresetName())
            throw std::runtime_error("transition identity");
        for (auto *a : p.getParameters())
        {
            auto *id = dynamic_cast<juce::AudioProcessorParameterWithID *>(a);
            if (!id || std::abs(a->getValue() - expected.apvts.getParameter(id->paramID)->getValue()) > 1.e-6)
                throw std::runtime_error("stuck parameter");
        }
        double e = 0, peak = 0;
        constexpr int blocks = 750;
        for (int j = 0; j < blocks; ++j)
        {
            for (int n = 0; n < bs; ++n)
                for (int ch = 0; ch < 2; ++ch)
                    block.setSample(ch, n, source((j * bs + n) % (active * sr), 5));
            p.processBlock(block, midi);
            for (int n = 0; n < bs; ++n)
                for (int ch = 0; ch < 2; ++ch)
                {
                    double x = block.getSample(ch, n);
                    if (!std::isfinite(x))
                        throw std::runtime_error("transition nonfinite");
                    peak = std::max(peak, std::abs(x));
                    e += x * x;
                }
        }
        const auto c = resolved(expected);
        const std::array<std::pair<const char *, float>, 7> controls{{{"timeMs", c.timeMs},
                                                                      {"feedback", c.feedback},
                                                                      {"highCutHz", c.highCutHz},
                                                                      {"diffusion", c.diffusion},
                                                                      {"wetWidth", c.wetWidth},
                                                                      {"shimmerAmount", c.shimmerAmount},
                                                                      {"duckAmount", c.duckAmount}}};
        for (const auto &control : controls)
            if (std::abs(p.getEffectiveControlValue(control.first) - control.second) >
                .002f * std::max(1.f, std::abs(control.second)))
                throw std::runtime_error("stuck effective parameter");
        const double db = 20 * std::log10(std::max(1.e-12, std::sqrt(e / (2 * blocks * bs))));
        log << from << "," << index << "," << peak << "," << db << "," << (k ? db - previousDb : 0) << "\n";
        if (peak > .98 || (k && std::abs(db - previousDb) > 6))
            throw std::runtime_error("transition level jump");
        previousDb = db;
        from = index;
    }
}
void diversity()
{
    bool shortTail = false, longTail = false, veryLong = false, dark = false, bright = false, focused = false,
         wide = false, stable = false, motion = false, shimmer = false, atmos = false;
    for (int i = 0; i < 20; ++i)
    {
        TE2350AudioProcessor p;
        p.setCurrentProgram(i);
        auto c = resolved(p);
        shortTail |= c.timeMs < 200 && c.feedback < .5;
        longTail |= c.timeMs > 600 && c.feedback > .6;
        veryLong |= c.timeMs > 1200 && c.feedback > .85;
        dark |= c.highCutHz < 5500;
        bright |= c.highCutHz > 11000;
        focused |= c.wetWidth < .45;
        wide |= c.wetWidth > .85;
        stable |= c.modDepth < .12 && c.chaos < .05;
        motion |= c.modDepth > .6 && c.wild > .5;
        shimmer |= c.shimmerAmount > .3 && c.shimmerFeedback > .4;
        atmos |= c.atmosFdnOn && c.shimmerAmount < .07 && c.shimmerFeedback < .001;
    }
    if (!(shortTail && longTail && veryLong && dark && bright && focused && wide && stable && motion &&
          shimmer && atmos))
        throw std::runtime_error("missing bank diversity role (effective controls)");
}
} // namespace
int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        const auto dir = argc > 1
                             ? juce::File(juce::String::fromUTF8(argv[1]))
                             : juce::File::getCurrentWorkingDirectory().getChildFile("PresetVoicingOutput");
        if (!dir.createDirectory())
            throw std::runtime_error("output directory");
        const bool auditOnly = argc > 2 && std::string(argv[2]) == "--audit-only";
        const auto names = te2350::getFactoryPresetNames();
        if (names.size() != 20)
            throw std::runtime_error("20 presets required");
        std::ofstream raw(dir.getChildFile("parameters.csv").getFullPathName().toStdString());
        raw << "preset,parameter,raw,effective\n";
        for (int i = 0; i < 20; ++i)
        {
            TE2350AudioProcessor p;
            p.setCurrentProgram(i);
            te2350::MacroEngine e;
            e.prepare(sr, bs);
            for (int k = 0; k < 100; ++k)
                e.update(p.apvts, bs);
            for (auto *a : p.getParameters())
            {
                auto *r = dynamic_cast<juce::RangedAudioParameter *>(a);
                auto *id = dynamic_cast<juce::AudioProcessorParameterWithID *>(a);
                if (!r || !id)
                    throw std::runtime_error("parameter type");
                float v = r->convertFrom0to1(r->getValue());
                raw << names[i] << "," << id->paramID << "," << v << ","
                    << e.getEffectiveValue(id->paramID, v) << "\n";
            }
        }
        raw.close();
        verifyLegacy();
        verifyNewRoles();
        if (argc > 2 && std::string(argv[2]) == "--verify-only")
            return 0;
        verifyDiagnosticParity();
        for (int type = 0; type < 12; ++type)
        {
            juce::AudioBuffer<float> dry(2, duration * sr);
            for (int n = 0; n < dry.getNumSamples(); ++n)
                for (int ch = 0; ch < 2; ++ch)
                    dry.setSample(ch, n, source(n, type));
            if (!wav(dir.getChildFile(juce::String("dry_") + sources[type] + ".wav"), dry))
                throw std::runtime_error("dry WAV write");
        }
        std::ofstream metrics(dir.getChildFile("safety.csv").getFullPathName().toStdString());
        metrics << "preset,source,mode,peak,dc,final_tail_rms\n";
        std::vector<Metrics> impulseMetrics, percussionMetrics;
        for (int i = 0; i < 20; ++i)
        {
            for (int type = 0; type < (i < 13 ? 6 : 12); ++type)
                for (int mode = 0; mode < 3; ++mode)
                {
                    auto audio = render(i, type, mode);
                    const auto m = measure(audio);
                    if (mode == 1 && type == 0)
                        impulseMetrics.push_back(m);
                    if (mode == 1 && type == 4)
                        percussionMetrics.push_back(m);
                    const std::string label =
                        names[i].toStdString() + "/" + sources[type] + "/" + std::to_string(mode);
                    if (!auditOnly)
                        safety(audio, m, label);
                    metrics << names[i] << "," << sources[type] << "," << mode << "," << m.peak << "," << m.dc
                            << "," << m.last << "\n";
                    const auto file = juce::String(i).paddedLeft('0', 2) + "_" + sources[type] + "_" +
                                      juce::String(mode) + ".wav";
                    if (!wav(dir.getChildFile(file), audio))
                        throw std::runtime_error("WAV write");
                }
            std::cout << names[i] << " rendered (twelve sources (legacy: six), mix/wet/no-shimmer)" << std::endl;
        }
        for (int i : {15, 16})
            for (int type : {0, 2, 7, 8, 9, 10})
            {
                auto audio = render(i, type, 1, false, true);
                safety(audio, measure(audio), "low diffusion A/B");
                if (!wav(dir.getChildFile(juce::String(i) + "_" + sources[type] + "_lowdiff.wav"), audio))
                    throw std::runtime_error("diffusion WAV write");
            }
        transitions(dir);
        if (!auditOnly)
        {
            diversity();
            auto minDecay = 24., maxDecay = 0., minWidth = 1., maxWidth = 0., minCentroid = 1.e9,
                 maxCentroid = 0.;
            for (const auto &m : impulseMetrics)
            {
                minDecay = std::min(minDecay, m.decay);
                maxDecay = std::max(maxDecay, m.decay);
                minWidth = std::min(minWidth, m.width);
                maxWidth = std::max(maxWidth, m.width);
            }
            for (const auto &m : percussionMetrics)
            {
                minCentroid = std::min(minCentroid, m.centroid);
                maxCentroid = std::max(maxCentroid, m.centroid);
            }
            // Broad audible-response gates complement control-based musical roles.
            if (minDecay > 2 || maxDecay < 8 || maxDecay < minDecay * 4 || maxWidth < minWidth * 2 ||
                maxWidth - minWidth < .05 || maxCentroid < minCentroid * 1.2)
                throw std::runtime_error("insufficient measured tail/width/spectral diversity");
            std::cout << "Measured diversity: decay " << minDecay << ".." << maxDecay << " s, impulse width "
                      << minWidth << ".." << maxWidth << ", percussion centroid " << minCentroid << ".."
                      << maxCentroid << " Hz" << std::endl;
            auto held = render(10, 3, 0, true);
            const auto fm = measure(held);
            safety(held, fm, "Frozen Choir performance");
            const double early = held.getRMSLevel(0, 5 * sr, 2 * sr),
                         late = held.getRMSLevel(0, 10 * sr, 2 * sr);
            if (early < 1.e-5 || late < early * .1 || late > early * 5)
                throw std::runtime_error("Frozen Choir hold collapsed or grew excessively");
            if (!wav(dir.getChildFile("Frozen_Choir_freeze_performance.wav"), held))
                throw std::runtime_error("freeze WAV write");
            std::ofstream freezeLog(
                dir.getChildFile("freeze_performance.csv").getFullPathName().toStdString());
            freezeLog << "engage_s,release_s,early_hold_rms,late_hold_rms,peak,final_rms\n4,12," << early
                      << "," << late << "," << fm.peak << "," << fm.last << "\n";
        }
        std::cout << "Preset voicing " << (auditOnly ? "baseline audit" : "test passed")
                  << ": 20 presets, twelve sources (legacy: six), diagnostic parity, safety, transitions" << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
