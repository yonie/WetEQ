//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//
// eqtest - measures EQEngine directly, with no plugin and no host.
//
// Compiles against the same sources the plugin uses, so a band that does not
// match its printed label shows up here in a second rather than after a full
// VST3 build and a hand test in a DAW.
//
// Each band is swept twice: once flat, once with the band engaged, and the
// difference is reported. That cancels the shared lo-fi colouring (resampler
// ripple, anti-alias filters, quantiser) and isolates what the band itself
// does - the ripple is wanted character, so it must not be mistaken for error.
//
// Build (from the plugin repo root):
//   cl /EHsc /O2 /std:c++17 /I WetEQ/source tools/eqtest.cpp WetEQ/source/eqengine.cpp
//------------------------------------------------------------------------

#include "eqengine.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace Yonie;

namespace {

constexpr double kPI = 3.14159265358979323846;
constexpr double kRate = 44100.0;
constexpr int    kBlock = 512;

// Goertzel magnitude at one frequency, skipping the settling period.
double magnitudeAt(const std::vector<float>& x, double freq, double rate, int start)
{
    const int n = static_cast<int>(x.size()) - start;
    if (n <= 0) return 0.0;
    const double coeff = 2.0 * std::cos(2.0 * kPI * freq / rate);
    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double s0 = x[start + i] + coeff * s1 - s2;
        s2 = s1; s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return 2.0 * std::sqrt(power > 0.0 ? power : 0.0) / n;
}

// Run a steady tone through the engine and return the output magnitude at that
// frequency, relative to the input.
double toneResponse(EQEngine& eng, const EQEngine::Settings& s, double freq)
{
    eng.reset();
    eng.setSettings(s);

    const int settle = static_cast<int>(kRate * 0.30);
    const int measure = static_cast<int>(kRate * 0.30);
    const int total = settle + measure;

    std::vector<float> in(total), outL(total, 0.0f), outR(total, 0.0f);
    for (int i = 0; i < total; ++i)
        in[i] = static_cast<float>(0.25 * std::sin(2.0 * kPI * freq * i / kRate));

    for (int pos = 0; pos < total; pos += kBlock)
    {
        const int n = std::min(kBlock, total - pos);
        eng.processStereo(in.data() + pos, in.data() + pos,
                          outL.data() + pos, outR.data() + pos, n);
    }

    const double ref = magnitudeAt(in, freq, kRate, settle);
    const double got = magnitudeAt(outL, freq, kRate, settle);
    if (ref <= 0.0 || got <= 0.0) return -120.0;
    return 20.0 * std::log10(got / ref);
}

struct BandCase
{
    const char* name;
    int* gainField;
    int* freqField;
    double (EQEngine::*freqFn)() const;
};

int failures = 0;

void check(const char* what, double got, double want, double tol)
{
    const bool ok = std::abs(got - want) <= tol;
    if (!ok) ++failures;
    std::printf("  %-46s %+7.2f dB (want %+6.2f +/-%.2f)  %s\n",
                what, got, want, tol, ok ? "ok" : "FAIL");
}

} // namespace

int main()
{
    EQEngine eng;
    eng.prepare(kRate, kBlock);

    std::printf("EQEngine measured at %.0f Hz, internal rate %.0f Hz\n",
                kRate, EQEngine::kInternalRate);

    // --- resolved knob values against the panel legends ------------------
    std::printf("\n[1] knob ranges vs the values printed on the panel\n");
    {
        EQEngine::Settings s;
        s.hpf = 0; eng.setSettings(s);
        std::printf("  HPF  min %.1f Hz (panel 20)      max ", eng.hpfHz());
        s.hpf = EQRange::kFreqSteps - 1; eng.setSettings(s);
        std::printf("%.1f Hz (panel 2k)\n", eng.hpfHz());

        s = EQEngine::Settings{};
        s.lpf = 0; eng.setSettings(s);
        std::printf("  LPF  min %.1f Hz (panel 2k)      max ", eng.lpfHz());
        s.lpf = EQRange::kFreqSteps - 1; eng.setSettings(s);
        std::printf("%.1f Hz (panel 20k)\n", eng.lpfHz());

        s = EQEngine::Settings{};
        s.hfFreq = 0; eng.setSettings(s);
        std::printf("  HF   min %.1f Hz (panel 1.5k)    max ", eng.hfHz());
        s.hfFreq = EQRange::kFreqSteps - 1; eng.setSettings(s);
        std::printf("%.1f Hz (panel 22k)\n", eng.hfHz());

        s = EQEngine::Settings{};
        s.lfFreq = 0; eng.setSettings(s);
        std::printf("  LF   min %.1f Hz (panel 30)      max ", eng.lfHz());
        s.lfFreq = EQRange::kFreqSteps - 1; eng.setSettings(s);
        std::printf("%.1f Hz (panel 600)\n", eng.lfHz());

        // Positions derived from the step count, not written as 0/5/10. Those
        // literals were right for eleven steps and silently wrong after the move
        // to nine: the test then called +5 dB the centre, and failed.
        const int gTop = EQRange::kMasterGainSteps - 1;
        const int gCtr = EQRange::kMasterGainSteps / 2;
        s = EQEngine::Settings{};
        s.gain = 0;    eng.setSettings(s); const double gLo = eng.gainDb();
        s.gain = gCtr; eng.setSettings(s); const double gMid = eng.gainDb();
        s.gain = gTop; eng.setSettings(s); const double gHi = eng.gainDb();
        std::printf("  GAIN %.1f / %.1f / %.1f dB at positions 0/%d/%d "
                    "(panel -20 / 0 / +20)\n", gLo, gMid, gHi, gCtr, gTop);
        check("GAIN centre position is exactly 0 dB", gMid, 0.0, 0.001);
    }

    // --- flat should be flat (apart from the house colouring) ------------
    std::printf("\n[2] flat settings: bands must be out of circuit\n");
    {
        EQEngine::Settings flat;   // defaults: all gains centred, HPF/LPF parked
        // Quantiser off so this measures the filter path, not the dither floor.
        // Response is measured with the amplifiers linear. Saturation is real and
    // wanted, but it compresses a +15 dB boost by about 2 dB, and a curve
    // measurement should show the filter, not the filter plus its drive.
    eng.setQuantizationEnabled(false);
    eng.setSaturationEnabled(false);
    eng.setHissEnabled(false);
        eng.setSaturationEnabled(false);
        eng.setHissEnabled(false);
        const double at1k = toneResponse(eng, flat, 1000.0);
        const double at100 = toneResponse(eng, flat, 100.0);
        check("flat @ 1 kHz", at1k, 0.0, 1.5);
        check("flat @ 100 Hz", at100, 0.0, 1.5);
        eng.setQuantizationEnabled(true);
    eng.setSaturationEnabled(true);
    eng.setHissEnabled(true);
        eng.setSaturationEnabled(true);
        eng.setHissEnabled(true);
    }

    // --- each band, boost and cut, measured at its own centre ------------
    std::printf("\n[3] each band at its centre frequency, +/-15 dB\n"
                "    (difference against flat, so the house ripple cancels)\n");
    eng.setQuantizationEnabled(false);

    struct Band { const char* name; int EQEngine::Settings::* gain; double freq; };

    {
        EQEngine::Settings flat;

        // LF shelf at its default corner
        eng.setSettings(flat);
        const double lfF = eng.lfHz();
        // measure a shelf well below its corner, where it reaches full gain
        const double lfProbe = lfF * 0.25;

        eng.setSettings(flat);
        const double hmfF = eng.hmfHz();
        const double lmfF = eng.lmfHz();
        const double hfF = eng.hfHz();
        const double hfProbe = std::min(hfF * 2.0, EQEngine::kInternalRate * 0.40);

        struct Probe { const char* name; double freq; int idx; };
        // idx: 0=lf 1=lmf 2=hmf 3=hf
        Probe probes[4] = {
            { "LF shelf",  lfProbe, 0 },
            { "LMF bell",  lmfF,    1 },
            { "HMF bell",  hmfF,    2 },
            { "HF shelf",  hfProbe, 3 },
        };

        for (const Probe& p : probes)
        {
            for (int dir = 0; dir < 2; ++dir)
            {
                // Derived, never written as 10: that literal was right for
                // eleven steps, overshot to +22.5 dB at nine (and passed only
                // because saturation pulled it back inside tolerance), and
                // lands on +3.75 dB at seventeen.
                const int pos = (dir == 0) ? EQRange::kBandGainSteps - 1 : 0;
                const double want = (dir == 0) ? 15.0 : -15.0;

                EQEngine::Settings s = flat;
                switch (p.idx)
                {
                    case 0: s.lfGain = pos;  break;
                    case 1: s.lmfGain = pos; break;
                    case 2: s.hmfGain = pos; break;
                    case 3: s.hfGain = pos;  break;
                }

                const double withBand = toneResponse(eng, s, p.freq);
                const double flatRef  = toneResponse(eng, flat, p.freq);
                const double delta = withBand - flatRef;

                char label[128];
                std::snprintf(label, sizeof(label), "%s %+d dB @ %.0f Hz",
                              p.name, static_cast<int>(want), p.freq);
                // A bell reaches its full gain at centre; a shelf needs to be
                // probed off to the side and still only approaches it, so the
                // tolerance is loose for shelves.
                const double tol = (p.idx == 0 || p.idx == 3) ? 4.0 : 1.5;
                check(label, delta, want, tol);
            }
        }
    }

    // --- proportional Q: a bigger boost must be narrower -----------------
    std::printf("\n[4] proportional Q: a harder push must be a narrower bell\n");
    {
        EQEngine::Settings flat;
        eng.setSettings(flat);
        const double fc = eng.hmfHz();

        auto widthAt = [&](int pos) {
            EQEngine::Settings s = flat;
            s.hmfGain = pos;
            const double centre = toneResponse(eng, s, fc) - toneResponse(eng, flat, fc);
            const double octaveUp = toneResponse(eng, s, fc * 2.0) -
                                    toneResponse(eng, flat, fc * 2.0);
            // how much of the centre boost survives one octave away
            return centre > 0.1 ? octaveUp / centre : 0.0;
        };

        // One detent above centre against the top detent - derived, because
        // 6 and 10 were eleven-step literals and at seventeen the first of them
        // is a CUT, which makes the comparison meaningless rather than failing
        // it honestly.
        const int smallPos = EQRange::kBandGainSteps / 2 + 1;
        const int bigPos   = EQRange::kBandGainSteps - 1;
        const double smallPush = widthAt(smallPos);
        const double bigPush   = widthAt(bigPos);
        std::printf("  %+.1f dB keeps %.0f%% of its boost one octave up\n",
                    eng.bandGainDb(smallPos), smallPush * 100.0);
        std::printf("  %+.1f dB keeps %.0f%% of its boost one octave up\n",
                    eng.bandGainDb(bigPos), bigPush * 100.0);
        const bool ok = bigPush < smallPush;
        if (!ok) ++failures;
        std::printf("  %-46s %s\n", "larger boost is proportionally narrower",
                    ok ? "ok" : "FAIL");
    }

    eng.setQuantizationEnabled(true);

    // --- changing a setting must not click --------------------------------
    //
    // Ronald heard "a very tiny pop/click" on every knob move. Two hard
    // discontinuities can cause it: the master drive is recomputed once per
    // block and jumps outright (a detent is 5 dB, a factor of 1.78), and the
    // biquad coefficients are swapped while the filter state carries over from
    // the old response.
    //
    // Measured as the largest sample-to-sample step around the change against
    // the largest step the steady tone itself makes. Clean switching stays near
    // 1.0; a click shows up as a multiple of it.
    {
        std::printf("\n[6] changing a setting must not click\n");

        auto clickRatio = [](const EQEngine::Settings& a, const EQEngine::Settings& b) {
            EQEngine e;
            e.prepare(kRate, kBlock);
            e.setSettings(a);
            const int blocks = 40, half = blocks / 2;
            std::vector<float> inL(kBlock), inR(kBlock), oL(kBlock), oR(kBlock), out;
            out.reserve(static_cast<size_t>(blocks) * kBlock);
            double ph = 0.0;
            const double dp = 2.0 * kPI * 1000.0 / kRate;
            for (int blk = 0; blk < blocks; ++blk)
            {
                if (blk == half) e.setSettings(b);
                for (int i = 0; i < kBlock; ++i)
                {
                    inL[i] = inR[i] = static_cast<float>(0.35 * std::sin(ph));
                    ph += dp;
                }
                e.processStereo(inL.data(), inR.data(), oL.data(), oR.data(), kBlock);
                for (int i = 0; i < kBlock; ++i) out.push_back(oL[i]);
            }
            auto maxStep = [&out](int from, int to) {
                double m = 0.0;
                for (int i = from + 1; i < to; ++i)
                    m = std::max(m, std::abs(double(out[i]) - double(out[i - 1])));
                return m;
            };
            const int sw = half * kBlock;
            const double steady = maxStep(kBlock * 8, kBlock * 15);
            const double atSwitch = maxStep(sw - 8, sw + 2 * kBlock);
            return steady > 0.0 ? atSwitch / steady : 0.0;
        };

        auto one = [&](const char* what, const EQEngine::Settings& b, double tol) {
            const double r = clickRatio(EQEngine::Settings{}, b);
            const bool ok = r <= tol;
            if (!ok) ++failures;
            std::printf("  %-46s %6.2fx (want <=%.2f)  %s\n",
                        what, r, tol, ok ? "ok" : "FAIL");
        };

        EQEngine::Settings g;   g.gain    += 1;  one("master GAIN, one detent",  g,  1.30);
        EQEngine::Settings hf;  hf.hfGain += 2;  one("HF gain, two detents",     hf, 1.30);
        EQEngine::Settings hp;  hp.hpf     = 4;  one("HPF stepped into circuit", hp, 1.30);
    }

    // --- the house lo-fi character ---------------------------------------
    std::printf("\n[5] house character: 12-bit floor and the Nyquist consequence\n");
    {
        EQEngine::Settings flat;
        eng.reset();
        eng.setSettings(flat);

        const int n = static_cast<int>(kRate);
        std::vector<float> sil(n, 0.0f), oL(n, 0.0f), oR(n, 0.0f);
        for (int pos = 0; pos < n; pos += kBlock)
        {
            const int b = std::min(kBlock, n - pos);
            eng.processStereo(sil.data() + pos, sil.data() + pos,
                              oL.data() + pos, oR.data() + pos, b);
        }
        double sum = 0.0;
        for (int i = 0; i < n; ++i) sum += static_cast<double>(oL[i]) * oL[i];
        const double floorDb = 20.0 * std::log10(std::sqrt(sum / n) + 1e-20);
        std::printf("  quantiser noise floor with silence in: %.1f dBFS\n", floorDb);

        // The HF band's top positions sit above the 12 kHz Nyquist. Ronald
        // accepted that cost knowingly; what must NOT happen is instability.
        EQEngine::Settings s = flat;
        s.hfGain = EQRange::kBandGainSteps - 1;
        s.hfFreq = EQRange::kFreqSteps - 1;    // 22 kHz, above Nyquist
        eng.setSettings(s);
        std::printf("  HF asked for %.0f Hz at a %.0f Hz internal rate "
                    "(Nyquist %.0f)\n", eng.hfHz(), EQEngine::kInternalRate,
                    EQEngine::kInternalRate / 2.0);

        eng.reset();
        const int m = kBlock * 8;
        std::vector<float> tone(m), tL(m, 0.0f), tR(m, 0.0f);
        for (int i = 0; i < m; ++i)
            tone[i] = static_cast<float>(0.25 * std::sin(2.0 * kPI * 1000.0 * i / kRate));
        for (int pos = 0; pos < m; pos += kBlock)
            eng.processStereo(tone.data() + pos, tone.data() + pos,
                              tL.data() + pos, tR.data() + pos, kBlock);
        double peak = 0.0;
        bool finite = true;
        for (int i = 0; i < m; ++i)
        {
            if (!std::isfinite(tL[i])) { finite = false; break; }
            peak = std::max(peak, static_cast<double>(std::abs(tL[i])));
        }
        const bool stable = finite && peak < 4.0;
        if (!stable) ++failures;
        std::printf("  %-46s %s (peak %.3f)\n",
                    "stays stable with HF above Nyquist",
                    stable ? "ok" : "FAIL", peak);
    }

    std::printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
