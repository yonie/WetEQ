//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "eqengine.h"
#include "weteqcids.h"
#include <atomic>

namespace Yonie {

//------------------------------------------------------------------------
class WetEQProcessor : public Steinberg::Vst::AudioEffect
{
public:
    WetEQProcessor();
    ~WetEQProcessor() SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance(void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new WetEQProcessor;
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;

protected:
    // Normalised parameter value -> discrete knob position.
    static int toStep(Steinberg::Vst::ParamValue normalized, int stepCount);

    void applyParameter(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue value);

    EQEngine engine;
    EQEngine::Settings pending;

    // Peak meters. The panel has one IN and one OUT strip, so each follows the
    // louder of the two channels.
    std::atomic<float> inputPeak{0.0f};
    std::atomic<float> outputPeak{0.0f};
    float oldInputMeter = 0.0f;
    float oldOutputMeter = 0.0f;

    static constexpr float kMeterDecay = 0.9995f;

    void updatePeak(float sample, std::atomic<float>& peak);
};

//------------------------------------------------------------------------
} // namespace Yonie
