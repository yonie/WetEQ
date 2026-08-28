//------------------------------------------------------------------------
// analogcore.h — WetEQ's analog stages, solved rather than approximated.
//
// WHY THIS IS NOT A BIQUAD CHAIN
//
// A cascade of biquads is a description of a circuit's frequency response. It
// is not a circuit. Three things it cannot do, all of which a console does:
//
//   1. Saturate INSIDE the loop. A real gyrator band has an op-amp in its
//      feedback network, so when the stage is driven the filter's own behaviour
//      changes - the resonance detunes and compresses as it works. Saturating
//      before or after a linear biquad cannot produce that; the filter stays
//      exactly as linear as it was.
//
//   2. Have a stage-by-stage identity. Each band on a desk is its own op-amp,
//      its own resistors, its own capacitors - so each one contributes its own
//      noise and its own crosstalk into the neighbouring channel. Lumping all
//      the noise and all the crosstalk into one point at the input is a
//      simplification that flattens exactly what makes a console sound like
//      many circuits rather than one.
//
//   3. Warp honestly. The bilinear transform squeezes the top octave; a
//      topology-preserving transform maps frequency exactly, which matters when
//      a shelf sits at 22 kHz and the resonance is what carries the character.
//
// So each band here is a state-variable stage in TPT form - the digital
// equivalent of the two integrators a gyrator actually contains - with the
// nonlinearity placed in the integrator path, which is where the op-amp sits in
// the real circuit.
//
// Ronald, 2026-08-28: "it needs to be as close to the real analog chain as we
// can get. that means, each stage should introduce its own crosstalk."
//------------------------------------------------------------------------
#pragma once

#include <cmath>
#include <cstdint>

namespace Yonie {

//------------------------------------------------------------------------
// One op-amp's transfer curve.
//
// A 5534-class part does not clip, it runs out of loop gain and compresses,
// mostly odd harmonics. tanh has unity slope at zero, so signal well inside the
// rails is untouched, and it is monotonic, so it can never fold back on itself.
//
// headroom is in units of full scale: how hard the stage has to be driven
// before it starts to bite.
inline float opAmpCurve(float x, float headroom)
{
    return headroom * std::tanh(x / headroom);
}

//------------------------------------------------------------------------
// A cheap deterministic noise source, one per stage.
//
// Each stage gets its own seed, so the four bands do not all hiss in unison -
// which they would if they shared a generator, and which is audible as a single
// correlated noise rather than a floor.
class StageNoise
{
public:
    void seed(uint32_t s) { state = s ? s : 0x9E3779B9u; }

    float next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(static_cast<int32_t>(state)) * 4.6566129e-10f;
    }

private:
    uint32_t state = 0x9E3779B9u;
};

//------------------------------------------------------------------------
// A state-variable stage in TPT form, with the op-amp inside the loop.
//
// The two integrator states are the two energy stores a gyrator tank has: the
// synthetic inductor and the series capacitor. Saturating the integrator input
// is therefore not an effect bolted on, it is where the amplifier physically
// is in the circuit.
class AnalogStage
{
public:
    enum class Mode { Bell, LowShelf, HighShelf, HighPass, LowPass, Bypass };

    void reset() { ic1 = ic2 = 0.0f; }

    void setNoiseSeed(uint32_t s) { noise.seed(s); }

    // fc in Hz, sr in Hz, gainDb only meaningful for Bell/shelf modes.
    void set(Mode m, double sr, double fc, double gainDb, double q)
    {
        mode = m;
        if (m == Mode::Bypass) return;

        // Topology-preserving: g is the exact prewarped integrator gain, so the
        // corner lands where it is asked to and not a few percent low.
        const double nyq = sr * 0.5;
        fc = fc < 10.0 ? 10.0 : (fc > nyq * 0.995 ? nyq * 0.995 : fc);

        const double a = std::pow(10.0, gainDb / 40.0);
        A = static_cast<float>(a);
        const double qq = q < 0.05 ? 0.05 : q;

        // The shelves prewarp with sqrt(A). Without it the shelf's corner walks
        // as its gain changes and boost stops mirroring cut - which is what put
        // a -15 dB HF cut at -10 dB on the first attempt.
        double gg = std::tan(3.14159265358979323846 * fc / sr);
        if (m == Mode::LowShelf)  gg /= std::sqrt(a);
        if (m == Mode::HighShelf) gg *= std::sqrt(a);
        g = static_cast<float>(gg);

        // k is 1/Q for every mode, bell included.
        //
        // The usual SVF bell divides k by A as well, which is a convention for
        // making Q mean "shelf-referenced Q". Here it would narrow the boost a
        // SECOND time on top of the narrowing the gyrator already produces, and
        // it makes boost and cut different widths - a +15 bell came out 1.9 dB
        // short at its own centre because it had quietly become far narrower
        // than asked. The pot decides the width; nothing else should.
        const double kk = 1.0 / qq;
        k = static_cast<float>(kk);

        // Output mix. One set of three coefficients selects what the stage is,
        // taken from the same two integrator outputs every time - which is also
        // how the real thing works: one tank, tapped differently.
        switch (m)
        {
            case Mode::Bell:      m0 = 1.0f;  m1 = static_cast<float>(kk * (a*a - 1.0)); m2 = 0.0f; break;
            case Mode::LowShelf:  m0 = 1.0f;  m1 = static_cast<float>(kk * (a - 1.0));   m2 = static_cast<float>(a*a - 1.0); break;
            case Mode::HighShelf: m0 = static_cast<float>(a*a);
                                  m1 = static_cast<float>(kk * (1.0 - a) * a);
                                  m2 = static_cast<float>(1.0 - a*a); break;
            case Mode::HighPass:  m0 = 1.0f;  m1 = -k;   m2 = -1.0f;  break;
            case Mode::LowPass:   m0 = 0.0f;  m1 = 0.0f; m2 = 1.0f;   break;
            default:              m0 = 1.0f;  m1 = 0.0f; m2 = 0.0f;   break;
        }

        const float d = 1.0f + g * (g + k);
        a1 = 1.0f / d;
        a2 = g * a1;
        a3 = g * a2;
    }

    // drive: how hard this stage's op-amp is being pushed. sat: enable the
    // nonlinearity. bleed: signal from the other channel, injected here rather
    // than once at the input.
    inline float process(float v0, bool sat, float headroom, float bleed,
                         float hissAmp)
    {
        if (mode == Mode::Bypass) return v0 + bleed + noise.next() * hissAmp;

        v0 += bleed;

        const float v3 = v0 - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;

        // The op-amp sits in the integrator path. Saturating HERE, rather than
        // before or after the filter, is what lets the stage's own response
        // change as it is driven: the resonance softens and detunes exactly the
        // way a real tank does when its amplifier runs out of room.
        if (sat)
        {
            v1 = opAmpCurve(v1, headroom);
            v2 = opAmpCurve(v2, headroom);
        }

        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;

        const float out = m0 * v0 + m1 * v1 + m2 * v2;

        // Every stage has its own resistors and its own amplifier, so every
        // stage contributes its own noise. One floor at the output would be a
        // converter; a chain of small independent ones is a console.
        return out + noise.next() * hissAmp;
    }

private:
    Mode mode = Mode::Bypass;
    float g = 0.0f, k = 1.0f, A = 1.0f;
    float m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;
    float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
    StageNoise noise;
};

} // namespace Yonie
