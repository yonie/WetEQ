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
    // Draw this unit's component tolerances once. A given build of the plugin
    // is a given build of the console: the values do not wander while you use
    // it, they were soldered in at the factory.
    {
        std::mt19937 tol(0xC0FFEE);
        std::uniform_real_distribution<double> spread(-kTolerance, kTolerance);
        for (int i = 0; i < 6; ++i)
        {
            tolL[i] = 1.0 + spread(tol);
            tolR[i] = 1.0 + spread(tol);
        }
    }

    // A different seed per stage per channel, so the twelve noise sources are
    // uncorrelated. Sharing one generator makes the whole strip hiss in unison,
    // which reads as a single added noise rather than as a floor.
    {
        uint32_t sd = 0x5EED0001u;
        for (int i = 0; i < 6; ++i) chainL.all[i]->setNoiseSeed(sd += 0x9E3779B9u);
        for (int i = 0; i < 6; ++i) chainR.all[i]->setNoiseSeed(sd += 0x9E3779B9u);
    }

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
    hissTiltL.setCoefficients(hostRate, kHissTiltHz, OnePoleFilter::Type::LowPass);
    hissTiltR.setCoefficients(hostRate, kHissTiltHz, OnePoleFilter::Type::LowPass);
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

    // Host rate. The EQ is analog; there is no internal rate any more.
    const double sr = hostRate;

    const bool hpActive = current.hpf > 0;
    const bool lpActive = current.lpf < (EQRange::kFreqSteps - 1);

    using M = AnalogStage::Mode;
    for (int ch = 0; ch < 2; ++ch)
    {
        Chain* c = ch == 0 ? &chainL : &chainR;
        const double* t = ch == 0 ? tolL : tolR;

        c->hpf.set(hpActive ? M::HighPass : M::Bypass, sr, smooth.hpHz * t[0], 0.0, 0.707);
        c->lpf.set(lpActive ? M::LowPass  : M::Bypass, sr, smooth.lpHz * t[1], 0.0, 0.707);
        c->lf .set(M::LowShelf,  sr, smooth.lfF  * t[2], smooth.lfG,  0.707);
        c->lmf.set(M::Bell,      sr, smooth.lmfF * t[3], smooth.lmfG, proportionalQ(smooth.lmfG));
        c->hmf.set(M::Bell,      sr, smooth.hmfF * t[4], smooth.hmfG, proportionalQ(smooth.hmfG));
        c->hf .set(M::HighShelf, sr, smooth.hfF  * t[5], smooth.hfG,  0.707);
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

    // Coefficients are glided in SUB-BLOCKS, not once per buffer.
    //
    // With the resampler gone there is nothing downstream to smooth a
    // coefficient step, so a 512-sample buffer meant the filter jumped once
    // every 11 ms and the click came straight back - measured at 2.28x on an
    // HF move. Updating every 32 samples cuts each step by a factor of 16 and
    // costs a coefficient recalculation per 0.7 ms, which is nothing next to
    // the six biquads it feeds.

    // --- ANALOG PATH, at host rate ---------------------------------------
    //
    // There is no downsample, no quantiser and no reconstruction filter here
    // any more. A console EQ is op-amps, resistors and capacitors: it has no
    // sample rate and no word length, so modelling one with sampler artefacts
    // put a digital fingerprint on a device that never had one. That core is
    // authentic on WetDelay and WetReverb, which model DIGITAL units. It was
    // never right here, and it cost 3 dB at 4 kHz and 10.7 dB at 8 kHz on a
    // signal that had not been touched.
    //
    // What replaces it is circuit behaviour: op-amp saturation on the drive
    // stage, component tolerance so the two channels are genuinely not
    // identical, and a broadband noise floor instead of a quantiser floor.

    const float driveA =
        static_cast<float>(1.0 - std::exp(-1.0 / (kDriveMs * 0.001 * hostRate)));

    for (int base = 0; base < numSamples; base += kGlideChunk)
    {
    const int chunk = std::min(kGlideChunk, numSamples - base);
    lastBlock = chunk;
    updateCoefficients();
    const float driveTarget = static_cast<float>(std::pow(10.0, smooth.gainDb / 20.0));

    for (int j = 0; j < chunk; ++j)
    {
        const int i = base + j;
        driveCurrent += (driveTarget - driveCurrent) * driveA;

        float l = inL[i] * driveCurrent;
        float r = inR[i] * driveCurrent;

        // Input amplifier. The first op-amp in the strip, before the EQ, which
        // is where GAIN drives.
        if (saturate)
        {
            l = opAmpCurve(l, kHeadroom);
            r = opAmpCurve(r, kHeadroom);
        }

        // Six stages, in strip order. Each one takes a little of the OTHER
        // channel's signal at that same point in the chain and adds its own
        // noise - so the bleed a later band receives has already been shaped by
        // the bands before it, exactly as it would be running down a desk.
        const float bleed = crosstalk ? kStageBleed : 0.0f;
        const float hs = hiss ? kStageHiss : 0.0f;
        for (int st = 0; st < 6; ++st)
        {
            const float pl = l, pr = r;
            l = chainL.all[st]->process(pl, saturate, kHeadroom, bleed * pr, hs);
            r = chainR.all[st]->process(pr, saturate, kHeadroom, bleed * pl, hs);
        }

        outL[i] = l;
        outR[i] = r;
    }
    }
}

//------------------------------------------------------------------------
void EQEngine::setToleranceEnabled(bool on)
{
    for (int i = 0; i < 6; ++i)
    {
        if (!on) { tolL[i] = tolR[i] = 1.0; }
    }
    coeffsDirty = true;
}

//------------------------------------------------------------------------
} // namespace Yonie
