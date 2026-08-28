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
    lastBlock = maxBlockSize > 0 ? maxBlockSize : 512;

    // Snap rather than glide on prepare: the glide exists to hide a knob move,
    // not to fade the plugin in every time the host starts.
    smooth = resolve(current);
    smoothPrimed = true;
    driveCurrent = static_cast<float>(std::pow(10.0, smooth.gainDb / 20.0));

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
EQEngine::Resolved EQEngine::resolve(const Settings& s) const
{
    using namespace EQRange;
    Resolved r;
    r.gainDb = stepToLinear(s.gain, kMasterGainSteps, kMasterGainMin, kMasterGainMax);
    r.hpHz = stepToLog(s.hpf, kFreqSteps, kHPFMin, kHPFMax);
    r.lpHz = stepToLog(s.lpf, kFreqSteps, kLPFMin, kLPFMax);
    r.lfG  = stepToLinear(s.lfGain,  kBandGainSteps, kBandGainMin, kBandGainMax);
    r.lmfG = stepToLinear(s.lmfGain, kBandGainSteps, kBandGainMin, kBandGainMax);
    r.hmfG = stepToLinear(s.hmfGain, kBandGainSteps, kBandGainMin, kBandGainMax);
    r.hfG  = stepToLinear(s.hfGain,  kBandGainSteps, kBandGainMin, kBandGainMax);
    r.lfF  = stepToLog(s.lfFreq,  kFreqSteps, kLFMin,  kLFMax);
    r.lmfF = stepToLog(s.lmfFreq, kFreqSteps, kLMFMin, kLMFMax);
    r.hmfF = stepToLog(s.hmfFreq, kFreqSteps, kHMFMin, kHMFMax);
    r.hfF  = stepToLog(s.hfFreq,  kFreqSteps, kHFMin,  kHFMax);
    return r;
}

//------------------------------------------------------------------------
// One-pole glide. Frequencies move in the LOG domain so a step from 100 Hz to
// 160 Hz takes the same time as one from 1 kHz to 1.6 kHz - a linear glide
// through frequency would crawl at the bottom and lurch at the top.
bool EQEngine::glideToward(Resolved& v, const Resolved& t, double a) const
{
    bool moving = false;
    auto lin = [&](double& x, double target) {
        const double d = target - x;
        if (std::abs(d) > 1e-6) { x += d * a; moving = true; }
        else x = target;
    };
    auto log = [&](double& x, double target) {
        if (x <= 0.0) { x = target; return; }
        const double d = std::log(target) - std::log(x);
        if (std::abs(d) > 1e-6) { x = std::exp(std::log(x) + d * a); moving = true; }
        else x = target;
    };
    lin(v.gainDb, t.gainDb);
    log(v.hpHz, t.hpHz);   log(v.lpHz, t.lpHz);
    lin(v.lfG,  t.lfG);    log(v.lfF,  t.lfF);
    lin(v.lmfG, t.lmfG);   log(v.lmfF, t.lmfF);
    lin(v.hmfG, t.hmfG);   log(v.hmfF, t.hmfF);
    lin(v.hfG,  t.hfG);    log(v.hfF,  t.hfF);
    return moving;
}

//------------------------------------------------------------------------
void EQEngine::updateCoefficients()
{
    const Resolved target = resolve(current);

    if (!smoothPrimed)
    {
        smooth = target;
        smoothPrimed = true;
    }
    else
    {
        // One block's worth of glide. Blocks vary in length, so the coefficient
        // is derived from the block duration rather than assumed.
        const double blockSec = static_cast<double>(lastBlock) / hostRate;
        const double a = 1.0 - std::exp(-blockSec / (kGlideMs * 0.001));
        const bool moving = glideToward(smooth, target, a);
        if (!moving && !coeffsDirty)
            return;
    }

    const double sr = kInternalRate;

    // A high-pass parked at its lowest position and a low-pass at its highest
    // should be out of circuit, not a gentle tilt across the whole band. Judged
    // on the TARGET, not the glide: a filter on its way in should not flick
    // between bypassed and active as it travels.
    const bool hpActive = current.hpf > 0;
    const bool lpActive = current.lpf < (EQRange::kFreqSteps - 1);

    for (Chain* c : { &chainL, &chainR })
    {
        if (hpActive) c->hpf.setHighPass(sr, smooth.hpHz);
        else          c->hpf.setBypass();

        if (lpActive) c->lpf.setLowPass(sr, smooth.lpHz);
        else          c->lpf.setBypass();

        c->lf.setLowShelf(sr, smooth.lfF, smooth.lfG);
        c->lmf.setPeaking(sr, smooth.lmfF, smooth.lmfG, proportionalQ(smooth.lmfG));
        c->hmf.setPeaking(sr, smooth.hmfF, smooth.hmfG, proportionalQ(smooth.hmfG));
        c->hf.setHighShelf(sr, smooth.hfF, smooth.hfG);
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

    lastBlock = numSamples;
    updateCoefficients();

    // --- input drive, then anti-alias, at host rate ----------------------
    // GAIN is here on purpose: before the 24 kHz / 12-bit stage, so it sets
    // where the signal sits against the quantisation floor. It is not makeup
    // gain and is not undone at the output.
    // Smoothed PER SAMPLE, not per block. A detent is 5 dB - a factor of 1.78 -
    // and applying that as a step at a block boundary is an audible click no
    // matter how short the block. A one-pole here costs one multiply-add per
    // sample and removes it outright.
    const float driveTarget = static_cast<float>(std::pow(10.0, smooth.gainDb / 20.0));
    const float driveA =
        static_cast<float>(1.0 - std::exp(-1.0 / (kDriveMs * 0.001 * hostRate)));
    for (int i = 0; i < numSamples; ++i)
    {
        driveCurrent += (driveTarget - driveCurrent) * driveA;
        preL[i] = antiAliasL.process(inL[i] * driveCurrent);
        preR[i] = antiAliasR.process(inR[i] * driveCurrent);
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
