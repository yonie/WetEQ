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
// LinearResampler - copied from WetDelay unchanged.
//------------------------------------------------------------------------
class LinearResampler
{
public:
    LinearResampler() : phase(0.0), lastSample(0.0f) {}

    void reset() { phase = 0.0; lastSample = 0.0f; }

    int downsample(const float* input, int inputSamples,
                   float* output, int maxOutputSamples,
                   double inputRate, double outputRate)
    {
        double ratio = inputRate / outputRate;
        int outCount = 0;
        for (int i = 0; i < inputSamples && outCount < maxOutputSamples; ++i)
        {
            float currentSample = input[i];
            while (phase < 1.0 && outCount < maxOutputSamples)
            {
                float t = static_cast<float>(phase);
                output[outCount++] = lastSample + t * (currentSample - lastSample);
                phase += ratio;
            }
            phase -= 1.0;
            lastSample = currentSample;
        }
        return outCount;
    }

    int upsample(const float* input, int inputSamples,
                 float* output, int outputSamples,
                 double inputRate, double outputRate)
    {
        double ratio = inputRate / outputRate;
        int inIndex = 0;
        for (int i = 0; i < outputSamples; ++i)
        {
            float t = static_cast<float>(phase);
            float currentSample = (inIndex < inputSamples) ? input[inIndex] : lastSample;
            output[i] = lastSample + t * (currentSample - lastSample);
            phase += ratio;
            while (phase >= 1.0 && inIndex < inputSamples)
            {
                lastSample = input[inIndex++];
                phase -= 1.0;
            }
        }
        if (inIndex > 0 && inIndex <= inputSamples)
            lastSample = input[inIndex - 1];
        return outputSamples;
    }

private:
    double phase;
    float lastSample;
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
