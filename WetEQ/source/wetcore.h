//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//
// Shared WET building blocks. No VST3 headers here on purpose, so the DSP can
// be compiled into a plain test executable and measured without a host.
//------------------------------------------------------------------------

#pragma once

#include <cmath>
#include <random>
#include <vector>

namespace Yonie {

//------------------------------------------------------------------------
// OnePoleFilter - 1st-order (6 dB/oct). Copied from WetDelay unchanged so the
// resampling chain behaves identically across the line.
//------------------------------------------------------------------------
class OnePoleFilter
{
public:
    enum class Type { LowPass, HighPass };

    OnePoleFilter() : z1(0.0f), x1(0.0f), coefficient(0.0f), type(Type::LowPass) {}

    void setCoefficients(double sampleRate, double cutoffHz, Type filterType)
    {
        type = filterType;
        double omega = 2.0 * 3.14159265358979323846 * cutoffHz / sampleRate;
        coefficient = static_cast<float>(std::exp(-omega));
    }

    float process(float input)
    {
        float output;
        if (type == Type::LowPass)
            output = (1.0f - coefficient) * input + coefficient * z1;
        else
            output = coefficient * (z1 + input - x1);
        z1 = output;
        x1 = input;
        return output;
    }

    void reset() { z1 = 0.0f; x1 = 0.0f; }

private:
    float z1;
    float x1;
    float coefficient;
    Type type;
};

//------------------------------------------------------------------------
// LinearResampler - linear interpolation, continuous across blocks.
//
// Still linear interpolation on purpose: its frequency-response ripple is part
// of the WET sound and must not be "improved" away.
//
// What IS fixed here is a block-boundary defect inherited from WetDelay's
// original version. That one tracked a `phase` accumulator and, when the input
// ran out mid-block, exited its inner loop with phase still >= 1. The next
// output then evaluated
//
//     out = last + t * (current - last)      with t = phase > 1
//
// which extrapolates instead of interpolating. Starvation recurs every block,
// so the error lands exactly on block boundaries and grows with how badly the
// block starves. Measured on the shipped WetDelay with a 0.5 amplitude sine:
// peak 13.59 at a 64-sample buffer, 1.01 at 512, 0.515 at 1024. WetEQ inherited
// it and reached 20.68 at 64 samples. Audible as a click train at buffer rate,
// and it clips.
//
// This version keeps an explicit fractional read position instead, carries one
// sample of history across blocks so interpolation is continuous, and clamps
// the read position inside the available input so it can never extrapolate.
//------------------------------------------------------------------------
class LinearResampler
{
public:
    LinearResampler() = default;

    void reset()
    {
        position = 0.0;
        history = 0.0f;
        primed = false;
    }

    // Produce as many output samples as `inputSamples` supports, up to
    // maxOutputSamples. Used host rate -> internal rate.
    int downsample(const float* input, int inputSamples,
                   float* output, int maxOutputSamples,
                   double inputRate, double outputRate)
    {
        const double ratio = inputRate / outputRate;      // > 1 when decimating
        int produced = 0;
        while (produced < maxOutputSamples && position < static_cast<double>(inputSamples))
        {
            output[produced++] = sampleAt(input, inputSamples, position);
            position += ratio;
        }
        finishBlock(input, inputSamples);
        return produced;
    }

    // Produce exactly `outputSamples`. Used internal rate -> host rate.
    int upsample(const float* input, int inputSamples,
                 float* output, int outputSamples,
                 double inputRate, double outputRate)
    {
        const double ratio = inputRate / outputRate;      // < 1 when expanding
        for (int i = 0; i < outputSamples; ++i)
        {
            output[i] = sampleAt(input, inputSamples, position);
            position += ratio;
        }
        finishBlock(input, inputSamples);
        return outputSamples;
    }

private:
    // Read at a fractional index. Index -1 is the last sample of the previous
    // block; anything past the end holds the final sample rather than running
    // away, which is what stops the boundary overshoot.
    inline float sampleAt(const float* input, int inputSamples, double pos) const
    {
        if (inputSamples <= 0)
            return history;

        double p = pos;
        const double maxPos = static_cast<double>(inputSamples - 1);
        if (p < -1.0) p = -1.0;
        if (p > maxPos) p = maxPos;

        const double floorPos = std::floor(p);
        const float frac = static_cast<float>(p - floorPos);
        const int i0 = static_cast<int>(floorPos);

        const float a = (i0 < 0) ? history : input[i0];
        const float b = (i0 + 1 <= inputSamples - 1) ? input[i0 + 1]
                                                     : input[inputSamples - 1];
        return a + frac * (b - a);
    }

    // Carry the leftover fraction and one sample of history into the next call,
    // so the interpolation has no seam at the boundary.
    inline void finishBlock(const float* input, int inputSamples)
    {
        if (inputSamples > 0)
        {
            history = input[inputSamples - 1];
            primed = true;
        }
        position -= static_cast<double>(inputSamples);
        // A host that hands us a shorter block than the previous one can leave
        // the position behind the window; clamp so it cannot wander.
        if (position < -1.0) position = -1.0;
    }

    double position = 0.0;   // fractional read index into the current block
    float history = 0.0f;    // input[-1] for this block
    bool primed = false;
};

//------------------------------------------------------------------------
// Biquad - RBJ cookbook. Shelves, bells and 2nd-order pass filters.
//
// Every setter clamps the corner frequency below Nyquist. WetEQ runs a 24 kHz
// internal rate, so its HF band can be asked for 22 kHz - a deliberate
// consequence of keeping the house sample rate. Clamped it goes inert; left
// unclamped the coefficients blow up and the plugin screams.
//------------------------------------------------------------------------
class Biquad
{
public:
    void reset() { z1 = z2 = 0.0; }

    void setBypass()
    {
        a0 = 1.0; a1 = a2 = b1 = b2 = 0.0;
        bypassed = true;
    }

    bool isBypassed() const { return bypassed; }

    void setPeaking(double sampleRate, double freqHz, double gainDb, double q)
    {
        if (std::abs(gainDb) < 0.01) { setBypass(); return; }
        const double fc = clampFreq(sampleRate, freqHz);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * PI * fc / sampleRate;
        const double alpha = std::sin(w0) / (2.0 * q);
        const double cosw = std::cos(w0);

        const double b0 = 1.0 + alpha * A;
        const double b1n = -2.0 * cosw;
        const double b2n = 1.0 - alpha * A;
        const double a0n = 1.0 + alpha / A;
        const double a1n = -2.0 * cosw;
        const double a2n = 1.0 - alpha / A;
        normalize(b0, b1n, b2n, a0n, a1n, a2n);
    }

    void setLowShelf(double sampleRate, double freqHz, double gainDb, double s = 1.0)
    {
        if (std::abs(gainDb) < 0.01) { setBypass(); return; }
        const double fc = clampFreq(sampleRate, freqHz);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * PI * fc / sampleRate;
        const double cosw = std::cos(w0);
        const double alpha = std::sin(w0) / 2.0 *
                             std::sqrt((A + 1.0 / A) * (1.0 / s - 1.0) + 2.0);
        const double twoSqrtAalpha = 2.0 * std::sqrt(A) * alpha;

        const double b0 = A * ((A + 1.0) - (A - 1.0) * cosw + twoSqrtAalpha);
        const double b1n = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosw);
        const double b2n = A * ((A + 1.0) - (A - 1.0) * cosw - twoSqrtAalpha);
        const double a0n = (A + 1.0) + (A - 1.0) * cosw + twoSqrtAalpha;
        const double a1n = -2.0 * ((A - 1.0) + (A + 1.0) * cosw);
        const double a2n = (A + 1.0) + (A - 1.0) * cosw - twoSqrtAalpha;
        normalize(b0, b1n, b2n, a0n, a1n, a2n);
    }

    void setHighShelf(double sampleRate, double freqHz, double gainDb, double s = 1.0)
    {
        if (std::abs(gainDb) < 0.01) { setBypass(); return; }
        const double fc = clampFreq(sampleRate, freqHz);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * PI * fc / sampleRate;
        const double cosw = std::cos(w0);
        const double alpha = std::sin(w0) / 2.0 *
                             std::sqrt((A + 1.0 / A) * (1.0 / s - 1.0) + 2.0);
        const double twoSqrtAalpha = 2.0 * std::sqrt(A) * alpha;

        const double b0 = A * ((A + 1.0) + (A - 1.0) * cosw + twoSqrtAalpha);
        const double b1n = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw);
        const double b2n = A * ((A + 1.0) + (A - 1.0) * cosw - twoSqrtAalpha);
        const double a0n = (A + 1.0) - (A - 1.0) * cosw + twoSqrtAalpha;
        const double a1n = 2.0 * ((A - 1.0) - (A + 1.0) * cosw);
        const double a2n = (A + 1.0) - (A - 1.0) * cosw - twoSqrtAalpha;
        normalize(b0, b1n, b2n, a0n, a1n, a2n);
    }

    void setHighPass(double sampleRate, double freqHz, double q = 0.7071)
    {
        const double fc = clampFreq(sampleRate, freqHz);
        const double w0 = 2.0 * PI * fc / sampleRate;
        const double cosw = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * q);
        const double b0 = (1.0 + cosw) / 2.0;
        const double b1n = -(1.0 + cosw);
        const double b2n = (1.0 + cosw) / 2.0;
        const double a0n = 1.0 + alpha;
        const double a1n = -2.0 * cosw;
        const double a2n = 1.0 - alpha;
        normalize(b0, b1n, b2n, a0n, a1n, a2n);
    }

    void setLowPass(double sampleRate, double freqHz, double q = 0.7071)
    {
        const double fc = clampFreq(sampleRate, freqHz);
        const double w0 = 2.0 * PI * fc / sampleRate;
        const double cosw = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * q);
        const double b0 = (1.0 - cosw) / 2.0;
        const double b1n = 1.0 - cosw;
        const double b2n = (1.0 - cosw) / 2.0;
        const double a0n = 1.0 + alpha;
        const double a1n = -2.0 * cosw;
        const double a2n = 1.0 - alpha;
        normalize(b0, b1n, b2n, a0n, a1n, a2n);
    }

    // Transposed direct form II
    inline float process(float in)
    {
        const double x = static_cast<double>(in);
        const double y = a0 * x + z1;
        z1 = a1 * x - b1 * y + z2;
        z2 = a2 * x - b2 * y;
        return static_cast<float>(y);
    }

private:
    static constexpr double PI = 3.14159265358979323846;

    // Keep the corner strictly inside the band. 0.45 * fs leaves the biquad
    // well conditioned; going closer to Nyquist makes sin(w0) tiny and the
    // coefficients ill-conditioned.
    static double clampFreq(double sampleRate, double freqHz)
    {
        const double maxF = sampleRate * 0.45;
        if (freqHz > maxF) return maxF;
        if (freqHz < 1.0) return 1.0;
        return freqHz;
    }

    void normalize(double b0, double b1n, double b2n,
                   double a0n, double a1n, double a2n)
    {
        a0 = b0 / a0n;
        a1 = b1n / a0n;
        a2 = b2n / a0n;
        b1 = a1n / a0n;
        b2 = a2n / a0n;
        bypassed = false;
    }

    double a0 = 1.0, a1 = 0.0, a2 = 0.0, b1 = 0.0, b2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
    bool bypassed = true;
};

//------------------------------------------------------------------------
} // namespace Yonie
