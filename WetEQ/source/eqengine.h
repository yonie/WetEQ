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

namespace Yonie {

//------------------------------------------------------------------------
// Parameter ranges. These are exactly the values printed on the panel.
//------------------------------------------------------------------------
namespace EQRange {

// dB knobs: 11 positions so 0 dB is reachable at the centre detent. A band you
// cannot defeat is a defect rather than a constraint, and real SSL gain pots
// have a centre detent at unity.
constexpr int    kBandGainSteps = 11;
constexpr double kBandGainMin   = -15.0;
constexpr double kBandGainMax   =  15.0;

constexpr int    kMasterGainSteps = 11;
constexpr double kMasterGainMin   = -20.0;
constexpr double kMasterGainMax   =  20.0;

// Frequency knobs: 10 positions, log spaced. No centre value is needed.
constexpr int    kFreqSteps = 10;

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
        int gain     = 5;   // 11 positions, 5 = 0 dB
        int hpf      = 0;   // 10 positions, 0 = 20 Hz (effectively out)
        int lpf      = 9;   // 10 positions, 9 = 20 kHz (effectively out)
        int hfGain   = 5;   int hfFreq  = 5;
        int hmfGain  = 5;   int hmfFreq = 5;
        int lmfGain  = 5;   int lmfFreq = 5;
        int lfGain   = 5;   int lfFreq  = 5;

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

private:
    // SSL-style proportional Q: the bell tightens as it is pushed harder, so a
    // small boost is broad and musical and a large one is surgical. No Q knob.
    static double proportionalQ(double gainDb)
    {
        const double mag = std::abs(gainDb) / 15.0;      // 0..1
        return 0.70 + 1.30 * mag;                        // 0.70..2.00
    }

    void updateCoefficients();

    Settings current;
    double hostRate = 44100.0;
    bool coeffsDirty = true;

    // per channel
    struct Chain
    {
        Biquad hpf, lpf, lf, lmf, hmf, hf;
        void reset() { hpf.reset(); lpf.reset(); lf.reset(); lmf.reset(); hmf.reset(); hf.reset(); }
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
    static constexpr float  kQuantGain = 2.0f;          // companding
    static constexpr float  kBitDepthLevels = 4096.0f;  // 2^12
};

//------------------------------------------------------------------------
} // namespace Yonie
