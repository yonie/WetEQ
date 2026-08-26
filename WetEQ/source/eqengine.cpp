//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "eqengine.h"

#include <algorithm>

namespace Yonie {

//------------------------------------------------------------------------
EQEngine::EQEngine()
: rng(20260826u)
, ditherDist(-1.0f, 1.0f)
{
}

//------------------------------------------------------------------------
void EQEngine::prepare(double hostSampleRate, int maxBlockSize)
{
    hostRate = hostSampleRate > 0.0 ? hostSampleRate : 44100.0;

    const int internalMax =
        static_cast<int>(maxBlockSize * kInternalRate / hostRate) + 16;

    preL.assign(maxBlockSize, 0.0f);
    preR.assign(maxBlockSize, 0.0f);
    downBufL.assign(internalMax, 0.0f);
    downBufR.assign(internalMax, 0.0f);
    procL.assign(internalMax, 0.0f);
    procR.assign(internalMax, 0.0f);

    // Anti-alias before the 24 kHz downsample and reconstruct after the
    // upsample. Same one-pole at 10 kHz as WetDelay, deliberately - this chain
    // and its ripple are the house sound, not something to improve.
    antiAliasL.setCoefficients(hostRate, kAntiAliasFreq, OnePoleFilter::Type::LowPass);
    antiAliasR.setCoefficients(hostRate, kAntiAliasFreq, OnePoleFilter::Type::LowPass);
    reconstructL.setCoefficients(hostRate, kAntiAliasFreq, OnePoleFilter::Type::LowPass);
    reconstructR.setCoefficients(hostRate, kAntiAliasFreq, OnePoleFilter::Type::LowPass);

    coeffsDirty = true;
    reset();
    updateCoefficients();
}

//------------------------------------------------------------------------
void EQEngine::reset()
{
    antiAliasL.reset();   antiAliasR.reset();
    reconstructL.reset(); reconstructR.reset();
    downL.reset(); downR.reset();
    upL.reset();   upR.reset();
    chainL.reset(); chainR.reset();
}

//------------------------------------------------------------------------
void EQEngine::setSettings(const Settings& s)
{
    if (s != current)
    {
        current = s;
        coeffsDirty = true;
    }
}

//------------------------------------------------------------------------
void EQEngine::updateCoefficients()
{
    if (!coeffsDirty)
        return;

    const double sr = kInternalRate;

    const double hp = hpfHz();
    const double lp = lpfHz();

    // A high-pass parked at its lowest position and a low-pass at its highest
    // should be out of circuit, not a gentle tilt across the whole band.
    const bool hpActive = current.hpf > 0;
    const bool lpActive = current.lpf < (EQRange::kFreqSteps - 1);

    for (Chain* c : { &chainL, &chainR })
    {
        if (hpActive) c->hpf.setHighPass(sr, hp);
        else          c->hpf.setBypass();

        if (lpActive) c->lpf.setLowPass(sr, lp);
        else          c->lpf.setBypass();

        const double lfDb  = bandGainDb(current.lfGain);
        const double lmfDb = bandGainDb(current.lmfGain);
        const double hmfDb = bandGainDb(current.hmfGain);
        const double hfDb  = bandGainDb(current.hfGain);

        c->lf.setLowShelf(sr, lfHz(), lfDb);
        c->lmf.setPeaking(sr, lmfHz(), lmfDb, proportionalQ(lmfDb));
        c->hmf.setPeaking(sr, hmfHz(), hmfDb, proportionalQ(hmfDb));
        c->hf.setHighShelf(sr, hfHz(), hfDb);
    }

    coeffsDirty = false;
}

//------------------------------------------------------------------------
void EQEngine::processStereo(const float* inL, const float* inR,
                             float* outL, float* outR, int numSamples)
{
    if (numSamples <= 0)
        return;

    if (static_cast<int>(preL.size()) < numSamples)
    {
        // A host handed us a bigger block than it promised. Grow rather than
        // read past the end.
        prepare(hostRate, numSamples);
    }

    updateCoefficients();

    // --- input drive, then anti-alias, at host rate ----------------------
    // GAIN is here on purpose: before the 24 kHz / 12-bit stage, so it sets
    // where the signal sits against the quantisation floor. It is not makeup
    // gain and is not undone at the output.
    const float drive = static_cast<float>(std::pow(10.0, gainDb() / 20.0));
    for (int i = 0; i < numSamples; ++i)
    {
        preL[i] = antiAliasL.process(inL[i] * drive);
        preR[i] = antiAliasR.process(inR[i] * drive);
    }

    // --- down to 24 kHz --------------------------------------------------
    int want = static_cast<int>(numSamples * kInternalRate / hostRate) + 1;
    want = std::min(want, static_cast<int>(downBufL.size()));

    const int gotL = downL.downsample(preL.data(), numSamples, downBufL.data(),
                                      want, hostRate, kInternalRate);
    const int gotR = downR.downsample(preR.data(), numSamples, downBufR.data(),
                                      want, hostRate, kInternalRate);
    const int n = std::min(gotL, gotR);

    // --- the lo-fi EQ path ----------------------------------------------
    for (int i = 0; i < n; ++i)
    {
        float l = downBufL[i];
        float r = downBufR[i];

        // Console channel bleed, -40 dB, same as WetDelay. On an insert this
        // narrows the stereo image very slightly; that was a deliberate call.
        if (crosstalk)
        {
            const float bl = l + kCrosstalkAmount * r;
            const float br = r + kCrosstalkAmount * l;
            l = bl;
            r = br;
        }

        l = chainL.hpf.process(l);
        r = chainR.hpf.process(r);
        l = chainL.lpf.process(l);
        r = chainR.lpf.process(r);
        l = chainL.lf.process(l);
        r = chainR.lf.process(r);
        l = chainL.lmf.process(l);
        r = chainR.lmf.process(r);
        l = chainL.hmf.process(l);
        r = chainR.hmf.process(r);
        l = chainL.hf.process(l);
        r = chainR.hf.process(r);

        if (quantize)
        {
            // Companded 12-bit with TPDF dither, as the SDE-3000 did and as
            // WetDelay does: boost, quantise, take the boost back out.
            l *= kQuantGain;
            r *= kQuantGain;

            constexpr float ditherAmp = 0.5f / kBitDepthLevels;
            const float dl = (ditherDist(rng) + ditherDist(rng)) * ditherAmp;
            const float dr = (ditherDist(rng) + ditherDist(rng)) * ditherAmp;
            l = std::floor((l + dl) * kBitDepthLevels + 0.5f) / kBitDepthLevels;
            r = std::floor((r + dr) * kBitDepthLevels + 0.5f) / kBitDepthLevels;

            l /= kQuantGain;
            r /= kQuantGain;
        }

        procL[i] = l;
        procR[i] = r;
    }

    // --- back up to host rate -------------------------------------------
    upL.upsample(procL.data(), n, outL, numSamples, kInternalRate, hostRate);
    upR.upsample(procR.data(), n, outR, numSamples, kInternalRate, hostRate);

    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] = reconstructL.process(outL[i]);
        outR[i] = reconstructR.process(outR[i]);
    }
}

//------------------------------------------------------------------------
} // namespace Yonie
