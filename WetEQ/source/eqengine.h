//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//
// WetEQ's DSP. No VST3 headers, so tools/eqtest can compile and measure this
// directly without loading a plugin host.
//
// Signal chain, per the design decisions of 2026-08-26:
//
//   in -> GAIN (input drive) -> anti-alias LP -> downsample to 24 kHz
//      -> L/R crosstalk -> HPF -> LPF -> LF shelf -> LMF bell -> HMF bell
//      -> HF shelf -> compand x2 -> 12-bit quantise + TPDF dither -> /2
//      -> upsample to host rate -> reconstruction LP -> out
//
// GAIN sits BEFORE the quantiser deliberately: it is how you place the signal
// against the 12-bit noise floor, the same job an input trim does on real gear.
// It is not makeup gain and is not compensated at the output.
//
// The 24 kHz internal rate is the house sound and is kept even though it puts
// the HF band's upper range (to 22 kHz) above the 12 kHz Nyquist. Above roughly
// 10.8 kHz the HF shelf clamps and stops having an audible effect. That cost
// was accepted knowingly - constraints are the product.
//
// Unlike WetDelay this does NOT apply fixed 80 Hz high-pass / 9 kHz low-pass
// character filters. On a delay those band-limit an effect tail; on an EQ they
// would fight the user's own HPF/LPF and LF band. The HPF and LPF knobs do that
// job here, under the user's control.
//------------------------------------------------------------------------

#pragma once

#include "wetcore.h"
#include "analogcore.h"

namespace Yonie {

//------------------------------------------------------------------------
// Parameter ranges. These are exactly the values printed on the panel.
//------------------------------------------------------------------------
namespace EQRange {

// Every knob has SIXTY-FIVE positions, but you only land on seventeen of them
// unless you hold Shift. The knob is still a stepped encoder that forces a
// decision; Shift is the power-user door out of it, for the 0.5 dB moves that
// vocals and masters need. Nine was too coarse (3.75 dB a step), seventeen was
// better (1.875 dB) and still too heavy-handed on a lot of sources - both
// reported by the same user, who was right twice.
//
// 65 = 4*16+1, so the grid quadruples and every earlier grid survives inside
// it: an old step lands exactly on a multiple of kCoarseStep, the centre detent
// is still a real position (0 dB, unity reachable), and the pointer still lands
// on a painted tick dot every eighth step. A count that is not 4*16+1 breaks
// all three.
//
// Band steps are 0.469 dB, master 0.625 dB. Not exactly 0.5 - 61 steps would be
// exactly 0.5 on the bands and would break the alignment above, which matters
// more than a round number in a spec sheet.
constexpr int    kSteps = 65;

// Detents reachable WITHOUT a modifier: every kCoarseStep-th position, which is
// the seventeen-detent grid v1.1.0 shipped. Shift while dragging, scrolling or
// arrowing reaches every step. The UI reads this; the DSP does not care.
constexpr int    kCoarseStep = 4;

constexpr int    kBandGainSteps = kSteps;
constexpr double kBandGainMin   = -15.0;
constexpr double kBandGainMax   =  15.0;

constexpr int    kMasterGainSteps = kSteps;
constexpr double kMasterGainMin   = -20.0;
constexpr double kMasterGainMax   =  20.0;

// Frequency knobs: log spaced between the two values printed on the panel.
// The endpoints are exact; the number printed at each knob's twelve o'clock is
// the render's decoration and does NOT match the centre detent (LF centres on
// 134 Hz, not the painted 150). Endpoints lead, the centre label is ignored.
constexpr int    kFreqSteps = kSteps;

constexpr double kHPFMin =    20.0, kHPFMax =  2000.0;
constexpr double kLPFMin =  2000.0, kLPFMax = 20000.0;
constexpr double kHFMin  =  1500.0, kHFMax  = 22000.0;
constexpr double kHMFMin =   400.0, kHMFMax = 10000.0;
constexpr double kLMFMin =   100.0, kLMFMax =  4000.0;
constexpr double kLFMin  =    30.0, kLFMax  =   600.0;

// index (0..steps-1) -> value
inline double stepToLinear(int index, int steps, double lo, double hi)
{
    if (steps <= 1) return lo;
    if (index < 0) index = 0;
    if (index >= steps) index = steps - 1;
    return lo + (hi - lo) * (static_cast<double>(index) / (steps - 1));
}

inline double stepToLog(int index, int steps, double lo, double hi)
{
    if (steps <= 1) return lo;
    if (index < 0) index = 0;
    if (index >= steps) index = steps - 1;
    const double t = static_cast<double>(index) / (steps - 1);
    return lo * std::pow(hi / lo, t);
}


// Saved state carries step INDICES, so every change to kSteps silently moves
// every stored session unless the grid it was written on is known. From state
// version 2 the stream says so outright. Version 1 does not - it was written by
// both the nine-detent v1.0.0 and the seventeen-detent v1.1.0, which is exactly
// the bug this closes - so it is inferred: an index above 8 cannot have come
// from a nine-position knob.
//
// tools/upgrade-weteq.py in the hub proves this against blobs the older builds
// actually wrote. Run it for every release. An upgrade that moves a control is
// somebody's mix quietly changing underneath them.
constexpr int kLegacySteps9  = 9;
constexpr int kLegacySteps17 = 17;

inline int guessLegacySteps(int maxIndexSeen)
{
    return maxIndexSeen > (kLegacySteps9 - 1) ? kLegacySteps17 : kLegacySteps9;
}

// Move an index from the grid it was saved on to the current one. Every grid
// this plugin has shipped divides into the current one, so this is exact.
inline int rescaleStep(int index, int fromSteps)
{
    if (fromSteps <= 1) return 0;
    if (index < 0) index = 0;
    if (index > fromSteps - 1) index = fromSteps - 1;
    if (fromSteps == kSteps) return index;
    return (index * (kSteps - 1)) / (fromSteps - 1);
}

} // namespace EQRange

//------------------------------------------------------------------------
// EQEngine
//------------------------------------------------------------------------
class EQEngine
{
public:
    EQEngine();

    // Discrete knob positions. Index based, because every WetEQ control is a
    // stepped encoder rather than a continuous pot.
    struct Settings
    {
        // Derived from the step counts, never written as literals: a fresh
        // insert must be audibly neutral, so every band sits at its centre
        // detent (0 dB) and both filters are parked out of circuit.
        static constexpr int kCentreGain = EQRange::kBandGainSteps / 2;
        static constexpr int kCentreFreq = EQRange::kFreqSteps / 2;

        int gain     = EQRange::kMasterGainSteps / 2;   // 0 dB
        int hpf      = 0;                               // 20 Hz, out of circuit
        int lpf      = EQRange::kFreqSteps - 1;         // 20 kHz, out of circuit
        int hfGain   = kCentreGain;   int hfFreq  = kCentreFreq;
        int hmfGain  = kCentreGain;   int hmfFreq = kCentreFreq;
        int lmfGain  = kCentreGain;   int lmfFreq = kCentreFreq;
        int lfGain   = kCentreGain;   int lfFreq  = kCentreFreq;

        bool operator==(const Settings& o) const
        {
            return gain == o.gain && hpf == o.hpf && lpf == o.lpf &&
                   hfGain == o.hfGain && hfFreq == o.hfFreq &&
                   hmfGain == o.hmfGain && hmfFreq == o.hmfFreq &&
                   lmfGain == o.lmfGain && lmfFreq == o.lmfFreq &&
                   lfGain == o.lfGain && lfFreq == o.lfFreq;
        }
        bool operator!=(const Settings& o) const { return !(*this == o); }
    };

    void prepare(double hostSampleRate, int maxBlockSize);
    void reset();

    void setSettings(const Settings& s);
    const Settings& settings() const { return current; }

    void processStereo(const float* inL, const float* inR,
                       float* outL, float* outR, int numSamples);

    // Resolved engineering values, for display and for the test harness.
    double gainDb()  const { return EQRange::stepToLinear(current.gain, EQRange::kMasterGainSteps,
                                                          EQRange::kMasterGainMin, EQRange::kMasterGainMax); }
    double hpfHz()   const { return EQRange::stepToLog(current.hpf, EQRange::kFreqSteps,
                                                       EQRange::kHPFMin, EQRange::kHPFMax); }
    double lpfHz()   const { return EQRange::stepToLog(current.lpf, EQRange::kFreqSteps,
                                                       EQRange::kLPFMin, EQRange::kLPFMax); }
    double bandGainDb(int stepIndex) const { return EQRange::stepToLinear(stepIndex, EQRange::kBandGainSteps,
                                                                          EQRange::kBandGainMin, EQRange::kBandGainMax); }
    double hfHz()    const { return EQRange::stepToLog(current.hfFreq,  EQRange::kFreqSteps, EQRange::kHFMin,  EQRange::kHFMax); }
    double hmfHz()   const { return EQRange::stepToLog(current.hmfFreq, EQRange::kFreqSteps, EQRange::kHMFMin, EQRange::kHMFMax); }
    double lmfHz()   const { return EQRange::stepToLog(current.lmfFreq, EQRange::kFreqSteps, EQRange::kLMFMin, EQRange::kLMFMax); }
    double lfHz()    const { return EQRange::stepToLog(current.lfFreq,  EQRange::kFreqSteps, EQRange::kLFMin,  EQRange::kLFMax); }

    static constexpr double kInternalRate = 24000.0;

    // Test hooks: let the harness isolate the EQ from the lo-fi stages so a
    // measured curve can be compared against the intended filter response.
    void setQuantizationEnabled(bool on) { quantize = on; }
    void setCrosstalkEnabled(bool on)    { crosstalk = on; }
    void setSaturationEnabled(bool on)   { saturate = on; }
    void setHissEnabled(bool on)         { hiss = on; }
    void setToleranceEnabled(bool on);

private:
    // SSL-style proportional Q: the bell tightens as it is pushed harder, so a
    // small boost is broad and musical and a large one is surgical. No Q knob.
    // Q is not chosen here. It falls out of the circuit.
    //
    // A console bell is a gyrator tank - an op-amp faking an inductor - sitting
    // on the wiper of the boost/cut pot. The wiper sets how much of the tank is
    // in circuit AND the resistance in series with it, and that series
    // resistance is what damps the tank. Near the centre there is a lot of it,
    // so the tank is heavily damped and the curve is wide. Towards the ends it
    // collapses and the curve narrows. Proportional Q is a CONSEQUENCE of that
    // topology, which is why real desks all have it and no digital EQ does
    // unless someone puts it there by hand.
    //
    // Rp is the pot, R_end the wiper and end-stop resistance a real pot cannot
    // go below, Z0 the tank's characteristic impedance sqrt(L/Cs). Widening
    // R_end is how a designer keeps a desk from ringing at full boost, and it
    // is the honest place to tune the feel - a component value, not a fudge on
    // the output of a formula.
    static constexpr double kPotOhms   = 10000.0;
    static constexpr double kEndOhms   = 3600.0;   // wiper + end stop + op-amp Zout
    static constexpr double kTankZ0    = 2600.0;   // sqrt(L/Cs)
    static constexpr double kTankLoss  = 90.0;     // series loss in the tank itself

    static double proportionalQ(double gainDb)
    {
        const double d = 0.5 * std::abs(gainDb) / 15.0;      // wiper travel, 0..0.5
        const double Rs = kPotOhms * (0.5 - d) + kTankLoss + kEndOhms;
        return kTankZ0 / Rs;
    }

    void updateCoefficients();

    // Op-amp soft clipping. A 5534-class part does not hard-limit, it runs out
    // of loop gain and compresses, mostly odd harmonics. tanh is the standard
    // stand-in and is well behaved: unity slope at zero, so quiet signal is
    // untouched, and monotonic, so it cannot fold.
    //
    // kHeadroom sets where it starts to bite. GAIN is calibrated so that centre
    // detent leaves the stage clean and the top of its travel is audibly into
    // the knee - which is the whole reason the knob is called DRIVE.
    static float opAmp(float x)
    {
        constexpr float kHeadroom = 1.9f;
        return kHeadroom * std::tanh(x / kHeadroom);
    }

    // Every knob is stepped, so a change is a JUMP, and a jump is a click.
    // Two of them: the master drive is a scalar that leaps by up to a factor of
    // 1.78 in one detent, and the biquad coefficients get swapped while the
    // filter state still holds the old response.
    //
    // Both are glided rather than switched. The controls stay stepped - what
    // moves smoothly is the value behind the detent, over a few milliseconds,
    // which is short enough that the change still feels instant and long enough
    // that nothing steps.
    struct Resolved
    {
        double gainDb = 0.0;
        double hpHz = 0.0, lpHz = 0.0;
        double lfG = 0.0, lfF = 0.0, lmfG = 0.0, lmfF = 0.0;
        double hmfG = 0.0, hmfF = 0.0, hfG = 0.0, hfF = 0.0;
    };
    Resolved resolve(const Settings& s) const;
    bool glideToward(Resolved& v, const Resolved& target, double a) const;

    static constexpr double kGlideMs = 22.0;   // parameter glide
    static constexpr int    kGlideChunk = 32;  // samples between coefficient updates
    static constexpr double kDriveMs = 12.0;   // master drive, per sample

    Resolved smooth;            // what the filters are actually built from
    bool smoothPrimed = false;  // first prepare() snaps instead of gliding
    float driveCurrent = 1.0f;  // per-sample smoothed master drive
    int lastBlock = 512;        // block just handed to us, for the glide rate

    Settings current;
    double hostRate = 44100.0;
    bool coeffsDirty = true;

    // per channel
    // Six independent circuit stages per channel. Each has its own op-amp, its
    // own noise and its own crosstalk into the other channel - a console is
    // many small circuits, not one big filter.
    struct Chain
    {
        AnalogStage hpf, lpf, lf, lmf, hmf, hf;
        AnalogStage* all[6] = { &hpf, &lpf, &lf, &lmf, &hmf, &hf };
        void reset() { for (auto* s : all) s->reset(); }
    };
    Chain chainL, chainR;

    OnePoleFilter antiAliasL, antiAliasR;
    OnePoleFilter reconstructL, reconstructR;
    LinearResampler downL, downR, upL, upR;

    std::vector<float> preL, preR;       // host rate, after gain + anti-alias
    std::vector<float> downBufL, downBufR;
    std::vector<float> procL, procR;

    std::mt19937 rng;
    std::uniform_real_distribution<float> ditherDist;

    bool quantize = true;
    bool crosstalk = true;

    static constexpr double kAntiAliasFreq = 10000.0;
    static constexpr float  kCrosstalkAmount = 0.01f;   // -40 dB, as WetDelay

    // Analog hiss. -88 dBFS, which is a quiet console channel; the first pass
    // sat at -100 and read as suspiciously clean, because that is converter
    // territory rather than op-amp territory.
    //
    // It is also TILTED, not flat. Op-amp and resistor noise carries a 1/f
    // component, so analog hiss is warmer than the white noise a dither
    // generator makes - mixing in a low-passed copy gets most of the way there
    // for the price of one filter. Flat white noise is a tell.
    static constexpr float  kHissAmp = 7.0e-5f;

    // Per-stage crosstalk and noise. The totals are unchanged - about -40 dB of
    // bleed and -91 dBFS of hiss - but they are injected at every stage rather
    // than once at the input. On a desk adjacent channels leak the whole way
    // down the strip, so the bleed a band sees has already been through the
    // bands before it, and every stage's own resistors contribute their own
    // uncorrelated noise. Six small independent sources sound like a console;
    // one at the output sounds like a converter.
    static constexpr float  kStageBleed = 0.0041f;   // 6 stages -> about -40 dB
    static constexpr float  kStageHiss  = 2.9e-5f;   // 6 uncorrelated -> -91 dBFS
    static constexpr float  kHeadroom   = 2.6f;      // where each op-amp bites
    static constexpr double kHissTiltHz = 1800.0;   // corner of the warm half
    OnePoleFilter hissTiltL, hissTiltR;

    // Component tolerance. Real resistors and capacitors are +/-5%, so the two
    // channels of a console are never the same channel twice and every corner
    // frequency sits slightly off its nominal value. This is a real part of why
    // analog reads as wide, and it is nearly free to model.
    static constexpr double kTolerance = 0.025;   // +/-2.5% per channel
    double tolL[6] = {1,1,1,1,1,1};
    double tolR[6] = {1,1,1,1,1,1};

    bool saturate = true;
    bool hiss = true;
    static constexpr float  kQuantGain = 2.0f;          // companding
    static constexpr float  kBitDepthLevels = 4096.0f;  // 2^12
};

//------------------------------------------------------------------------
} // namespace Yonie
