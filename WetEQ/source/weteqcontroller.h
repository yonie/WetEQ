//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace Yonie {

//------------------------------------------------------------------------
class WetEQController : public Steinberg::Vst::EditControllerEx1
{
public:
    WetEQController() = default;
    ~WetEQController() SMTG_OVERRIDE = default;

    static Steinberg::FUnknown* createInstance(void* /*context*/)
    {
        return (Steinberg::Vst::IEditController*)new WetEQController;
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;

    DEFINE_INTERFACES
    END_DEFINE_INTERFACES(Steinberg::Vst::EditControllerEx1)
    DELEGATE_REFCOUNT(Steinberg::Vst::EditControllerEx1)

private:
    // Adds a stepped parameter whose readout is built from the engine's own
    // range maths, so the value the host shows can never drift from the value
    // the DSP uses.
    void addSteppedGain(Steinberg::Vst::ParamID id, const char16_t* title,
                        int steps, double lo, double hi, int defaultStep);
    void addSteppedFreq(Steinberg::Vst::ParamID id, const char16_t* title,
                        int steps, double lo, double hi, int defaultStep);
};

//------------------------------------------------------------------------
} // namespace Yonie
