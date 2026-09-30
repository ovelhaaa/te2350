#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
extern "C"
{
#include "te2350.h"
}
// Deterministic 30 s renders: 3 s excitation, 25 s hold, 2 s release.
// LF/HF are energy ratios of first-order analysis bands (<200 Hz/>6 kHz).
struct Window
{
    double e = 0, lf = 0, hf = 0, peak = 0, step = 0;
};
// Benchmark uses precomputed input; setup and allocation are outside timing.
int benchmark()
{
    constexpr int sr = 48000, count = sr * 8;
    std::vector<q31_t> memory(TE2350_REQUIRED_MEMORY_BYTES / sizeof(q31_t));
    std::vector<q31_t> input(count);
    for (int n = 0; n < count; ++n)
        input[n] = FLOAT_TO_Q31(0.15f * std::sin(n * 0.0288));
    for (int atmos = 0; atmos < 2; ++atmos)
        for (int freeze = 0; freeze < 2; ++freeze)
        {
            std::array<double, 7> timings{};
            volatile q31_t sink = 0;
            for (auto &timing : timings)
            {
                te2350_t core{};
                if (!te2350_init(&core, memory.data(), memory.size() * sizeof(q31_t), sr))
                    return 2;
                te2350_set_fdn_enabled(&core, atmos != 0);
                te2350_set_time_samples(&core, sr / 2);
                te2350_set_shimmer(&core, FLOAT_TO_Q31(0.9f));
                te2350_set_octave_feedback_enabled(&core, true);
                te2350_set_octave_feedback_amount(&core, FLOAT_TO_Q31(0.9f));
                q31_t l, r;
                for (int n = 0; n < sr; ++n)
                    te2350_process(&core, input[n], &l, &r);
                te2350_set_freeze(&core, freeze != 0);
                auto start = std::chrono::steady_clock::now();
                for (int n = 0; n < count; ++n)
                {
                    te2350_process(&core, input[n], &l, &r);
                    sink = l;
                }
                timing = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() * 1e9 / count;
            }
            std::sort(timings.begin(), timings.end());
            std::printf("benchmark atmos=%d freeze=%d median_ns_per_frame=%.3f checksum=%d\n", atmos, freeze,
                        timings[3], static_cast<int>(sink));
        }
    return 0;
}
// Edge-rate/short-delay hold and new-input isolation. Compare actual storage
// fields, because output ducking/Bloom may legitimately respond to input.
bool edgeCases()
{
    bool ok = true;
    for (int sr : {44100, 48000, 96000, 192000})
        for (int ms : {20, 50})
        {
            std::vector<q31_t> a(TE2350_REQUIRED_MEMORY_BYTES / sizeof(q31_t)), b(a.size());
            te2350_t ca{}, cb{};
            for (auto *c : {&ca, &cb})
            {
                auto &mem = c == &ca ? a : b;
                if (!te2350_init(c, mem.data(), mem.size() * sizeof(q31_t), static_cast<float>(sr)))
                    return false;
                te2350_set_time_samples(c, sr * ms / 1000);
                te2350_set_fdn_enabled(c, false);
                te2350_set_mix(c, Q31_MAX);
                te2350_set_shimmer(c, FLOAT_TO_Q31(0.9f));
                te2350_set_shimmer_interval(c, 2);
                te2350_set_octave_feedback_enabled(c, true);
                te2350_set_octave_feedback_amount(c, FLOAT_TO_Q31(0.9f));
            }
            std::vector<q31_t> input(sr);
            for (int n = 0; n < sr; ++n)
                input[n] = FLOAT_TO_Q31(0.18f * std::sin(2 * 3.141592653589793 * 330 * n / sr));
            double initial = 0, final = 0;
            for (int n = 0; n < 25 * sr; ++n)
            {
                if (n == 3 * sr)
                {
                    te2350_set_freeze(&ca, true);
                    te2350_set_freeze(&cb, true);
                }
                q31_t l, r, u, v;
                q31_t excitation = n < 3 * sr ? input[n % sr] : 0;
                te2350_process(&ca, excitation, &l, &r);
                te2350_process(&cb, n >= 6 * sr && n < 9 * sr ? input[n % sr] : excitation, &u, &v);
                double e = static_cast<double>(Q31_TO_FLOAT(l)) * Q31_TO_FLOAT(l) +
                           static_cast<double>(Q31_TO_FLOAT(r)) * Q31_TO_FLOAT(r);
                if (n >= 4 * sr && n < 5 * sr)
                    initial += e;
                if (n >= 24 * sr)
                    final += e;
            }
            double diff = 0, energy = 0;
            for (int n = 0; n < sr * ms / 1000; ++n)
            {
                double x = Q31_TO_FLOAT(dsp_delay_read(&ca.main_delay, n + 1));
                double y = Q31_TO_FLOAT(dsp_delay_read(&cb.main_delay, n + 1));
                diff += (x - y) * (x - y);
                energy += x * x;
            }
            double retention = std::sqrt(final / initial), replacement = std::sqrt(diff / energy);
            bool pass = std::isfinite(retention) && retention >= 0.65 && retention <= 1.1 && replacement < 0.05;
            std::printf("edge sr=%d delay_ms=%d retention=%.6f injected_field_delta=%.6f %s\n", sr, ms, retention,
                        replacement, pass ? "PASS" : "FAIL");
            ok = ok && pass;
        }
    return ok;
}
int main(int argc, char **argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--benchmark") == 0)
        return benchmark();
    bool audit = argc > 1 && std::strcmp(argv[1], "--audit") == 0;
    bool ok = true;
    constexpr int sr = 48000;
    std::vector<q31_t> memory(TE2350_REQUIRED_MEMORY_BYTES / sizeof(q31_t));
    std::vector<float> excitation(3 * sr);
    uint32_t seed = 8123;
    for (int n = 0; n < 3 * sr; ++n)
    {
        seed = 1664525u * seed + 1013904223u;
        excitation[n] = 0.12f * std::sin(2 * 3.141592653589793 * 220 * n / sr) +
                        0.07f * std::sin(2 * 3.141592653589793 * 880 * n / sr) +
                        0.025f * std::sin(2 * 3.141592653589793 * 7000 * n / sr) +
                        0.025f * (static_cast<double>(seed) / 4294967296.0 - 0.5);
    }
    auto *summary = std::fopen("freeze_summary.csv", "w");
    auto *windows = std::fopen("freeze_windows.csv", "w");
    if (!summary || !windows)
        return 2;
    std::fprintf(summary, "atmos,interval,shimmer,regen,bloom,retention,energy_ratio,captured_rms_ratio,captured_"
                          "energy_ratio,peak,lf_initial,lf_final,hf_"
                          "initial,hf_final,max_growth,entry_step,exit_step\n");
    std::fprintf(windows, "atmos,interval,shimmer,regen,bloom,second,rms,peak,lf_ratio,hf_ratio,max_step\n");
    for (int atmos = 0; atmos < 2; ++atmos)
        for (int interval = 0; interval < 3; ++interval)
            for (float shimmer : {0.15f, 0.9f})
                for (float regen : {0.1f, 0.9f})
                    for (float bloom : {0.1f, 0.9f})
                    {
                        te2350_t core{};
                        if (!te2350_init(&core, memory.data(), memory.size() * sizeof(q31_t), sr))
                            return 2;
                        te2350_set_fdn_enabled(&core, atmos != 0);
                        te2350_set_time_samples(&core, sr / 2);
                        te2350_set_mix(&core, Q31_MAX);
                        te2350_set_feedback(&core, FLOAT_TO_Q31(0.65f));
                        te2350_set_tail(&core, FLOAT_TO_Q31(bloom));
                        te2350_set_tail_feedback(&core, FLOAT_TO_Q31(bloom < 0.5f ? 0.078f : 0.713f));
                        te2350_set_shimmer(&core, FLOAT_TO_Q31(shimmer));
                        te2350_set_shimmer_interval(&core, interval);
                        te2350_set_octave_feedback_enabled(&core, true);
                        te2350_set_octave_feedback_amount(&core, FLOAT_TO_Q31(regen));
                        te2350_set_diffusion(&core, FLOAT_TO_Q31(0.45f));
                        te2350_set_mod(&core, FLOAT_TO_Q31(0.2f), FLOAT_TO_Q31(0.15f));
                        std::array<Window, 30> w{};
                        double low[2]{}, highLP[2]{}, prev[2]{};
                        double entry = 0, leave = 0;
                        for (int n = 0; n < 30 * sr; ++n)
                        {
                            if (n == 3 * sr || n == 28 * sr)
                            {
                                te2350_t unchanged;
                                std::memcpy(&unchanged, &core, sizeof(core));
                                unchanged.freeze_mode = n == 3 * sr;
                                te2350_set_freeze(&core, n == 3 * sr);
                                // Transition setters must preserve every delay/filter/pitch state.
                                ok = ok && std::memcmp(&unchanged, &core, sizeof(core)) == 0;
                            }
                            q31_t l, r;
                            te2350_process(&core, n < 3 * sr ? FLOAT_TO_Q31(excitation[n]) : 0, &l, &r);
                            if (core.fdn_enabled != (atmos != 0))
                                ok = false;
                            auto &a = w[n / sr];
                            double x[2]{Q31_TO_FLOAT(l), Q31_TO_FLOAT(r)};
                            for (int ch = 0; ch < 2; ++ch)
                            {
                                ok = ok && std::isfinite(x[ch]);
                                low[ch] += 0.02584 * (x[ch] - low[ch]);
                                highLP[ch] += 0.54406 * (x[ch] - highLP[ch]);
                                a.e += x[ch] * x[ch];
                                a.lf += low[ch] * low[ch];
                                a.hf += (x[ch] - highLP[ch]) * (x[ch] - highLP[ch]);
                                a.peak = std::max(a.peak, std::abs(x[ch]));
                                double step = std::abs(x[ch] - prev[ch]);
                                a.step = std::max(a.step, step);
                                if (n >= 3 * sr && n < 3 * sr + 1200)
                                    entry = std::max(entry, step);
                                if (n >= 28 * sr && n < 28 * sr + 1200)
                                    leave = std::max(leave, step);
                                prev[ch] = x[ch];
                            }
                        }
                        double ratio = std::sqrt(w[24].e / w[4].e), peak = 0, growth = 0;
                        for (int t = 3; t < 28; ++t)
                        {
                            peak = std::max(peak, w[t].peak);
                            if (t >= 4)
                                growth = std::max(growth, std::sqrt(w[t].e / w[4].e));
                        }
                        bool pass = ratio >= 0.65 && ratio <= 1.1 && peak < 0.95 && growth < 1.15 && w[4].e > 1e-8 &&
                                    entry < std::max(0.005, w[2].step * 1.5) &&
                                    leave < std::max(0.005, w[27].step * 1.5) &&
                                    w[24].hf / w[24].e <= w[4].hf / w[4].e + 0.02;
                        if (!audit && !pass)
                        {
                            ok = false;
                            std::fprintf(stderr,
                                         "FAIL atmos=%d interval=%d shimmer=%.2f regen=%.2f bloom=%.2f retention=%.6f "
                                         "growth=%.6f peak=%.6f\n",
                                         atmos, interval, shimmer, regen, bloom, ratio, growth, peak);
                        }
                        std::fprintf(
                            summary,
                            "%d,%d,%.2f,%.2f,%.2f,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n", atmos,
                            interval, shimmer, regen, bloom, ratio, ratio * ratio, std::sqrt(w[24].e / w[2].e),
                            w[24].e / w[2].e, peak, w[4].lf / w[4].e, w[24].lf / w[24].e, w[4].hf / w[4].e,
                            w[24].hf / w[24].e, growth, entry, leave);
                        for (int t = 0; t < 30; ++t)
                            std::fprintf(windows, "%d,%d,%.2f,%.2f,%.2f,%d,%.9g,%.9g,%.9g,%.9g,%.9g\n", atmos, interval,
                                         shimmer, regen, bloom, t, std::sqrt(w[t].e / (2 * sr)), w[t].peak,
                                         w[t].lf / std::max(1e-30, w[t].e), w[t].hf / std::max(1e-30, w[t].e),
                                         w[t].step);
                    }
    std::fclose(summary);
    std::fclose(windows);
    if (!audit)
        ok = edgeCases() && ok;
    std::printf("Freeze matrix: 48 x 30 s, %s\n",
                audit ? (ok ? "AUDIT COMPLETE (retention gates disabled)" : "FAIL") : (ok ? "PASS" : "FAIL"));
    return ok ? 0 : 1;
}
