//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "weteqcontroller.h"
#include "weteqcids.h"
#include "eqengine.h"
#include "customviewcreator.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "vstgui/plugin-bindings/vst3editor.h"

#include <cmath>
#include <cstdio>

using namespace Steinberg;

namespace Yonie {

namespace {

//------------------------------------------------------------------------
// A stepped parameter that prints engineering units rather than a percentage.
// The strings come from the same EQRange helpers the DSP uses, so the readout
// and the filter can never disagree.
//------------------------------------------------------------------------
class SteppedUnitParameter : public Vst::Parameter
{
public:
    enum class Scale { Linear, Log };

    SteppedUnitParameter(const Vst::TChar* title, Vst::ParamID id, int steps,
                         double lo, double hi, Scale scale, int defaultStep,
                         const char* unit)
    : steps(steps), lo(lo), hi(hi), scale(scale), unit(unit)
    {
        Vst::ParameterInfo& i = info;
        UString(i.title, str16BufferSize(Vst::String128)).assign(title);
        i.id = id;
        i.stepCount = steps - 1;
        i.defaultNormalizedValue =
            steps > 1 ? static_cast<double>(defaultStep) / (steps - 1) : 0.0;
        i.unitId = Vst::kRootUnitId;
        i.flags = Vst::ParameterInfo::kCanAutomate;
        setNormalized(i.defaultNormalizedValue);
    }

    double valueAt(Vst::ParamValue normalized) const
    {
        int idx = static_cast<int>(normalized * (steps - 1) + 0.5);
        if (idx < 0) idx = 0;
        if (idx > steps - 1) idx = steps - 1;
        return scale == Scale::Linear
                   ? EQRange::stepToLinear(idx, steps, lo, hi)
                   : EQRange::stepToLog(idx, steps, lo, hi);
    }

    void toString(Vst::ParamValue normalized, Vst::String128 string) const SMTG_OVERRIDE
    {
        const double v = valueAt(normalized);
        char text[64];
        if (scale == Scale::Log && v >= 1000.0)
            std::snprintf(text, sizeof(text), "%.2f k%s", v / 1000.0, unit);
        else if (scale == Scale::Log)
            std::snprintf(text, sizeof(text), "%.0f %s", v, unit);
        else
            std::snprintf(text, sizeof(text), "%+.1f %s", v, unit);
        UString(string, str16BufferSize(Vst::String128)).fromAscii(text);
    }

    bool fromString(const Vst::TChar* string, Vst::ParamValue& normalized) const SMTG_OVERRIDE
    {
        if (!string)
            return false;

        UString wrapper(const_cast<Vst::TChar*>(string), str16BufferSize(Vst::String128));
        double want = 0.0;
        if (!wrapper.scanFloat(want))
            return false;

        // Accept "2k" as well as "2000". Scanned directly off the UTF-16
        // buffer to avoid dragging in Steinberg's String class for one lookup.
        if (scale == Scale::Log)
        {
            for (int i = 0; i < 128 && string[i] != 0; ++i)
            {
                if (string[i] == u'k' || string[i] == u'K')
                {
                    want *= 1000.0;
                    break;
                }
            }
        }

        // Snap whatever was typed to the nearest detent.
        int best = 0;
        double bestErr = 1e30;
        for (int i = 0; i < steps; ++i)
        {
            const double v = scale == Scale::Linear
                                 ? EQRange::stepToLinear(i, steps, lo, hi)
                                 : EQRange::stepToLog(i, steps, lo, hi);
            const double err = std::abs(v - want);
            if (err < bestErr) { bestErr = err; best = i; }
        }
        normalized = steps > 1 ? static_cast<double>(best) / (steps - 1) : 0.0;
        return true;
    }

private:
    int steps;
    double lo, hi;
    Scale scale;
    const char* unit;
};

} // namespace

//------------------------------------------------------------------------
void WetEQController::addSteppedGain(Vst::ParamID id, const char16_t* title,
                                     int steps, double lo, double hi, int defaultStep)
{
    parameters.addParameter(new SteppedUnitParameter(
        reinterpret_cast<const Vst::TChar*>(title), id, steps, lo, hi,
        SteppedUnitParameter::Scale::Linear, defaultStep, "dB"));
}

//------------------------------------------------------------------------
void WetEQController::addSteppedFreq(Vst::ParamID id, const char16_t* title,
                                     int steps, double lo, double hi, int defaultStep)
{
    parameters.addParameter(new SteppedUnitParameter(
        reinterpret_cast<const Vst::TChar*>(title), id, steps, lo, hi,
        SteppedUnitParameter::Scale::Log, defaultStep, "Hz"));
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQController::initialize(FUnknown* context)
{
    tresult result = EditControllerEx1::initialize(context);
    if (result != kResultOk)
        return result;

    registerCustomViews();

    using namespace EQRange;

    // Defaults put every band at its centre detent (0 dB) and park the two
    // filters out of circuit, so a freshly inserted WetEQ does nothing but add
    // its own character.
    addSteppedGain(kGainParam, u"Gain", kMasterGainSteps,
                   kMasterGainMin, kMasterGainMax, kMasterGainSteps / 2);

    addSteppedFreq(kHPFParam, u"HPF", kFreqSteps, kHPFMin, kHPFMax, 0);
    addSteppedFreq(kLPFParam, u"LPF", kFreqSteps, kLPFMin, kLPFMax, kFreqSteps - 1);

    addSteppedGain(kHFGainParam, u"HF Gain", kBandGainSteps, kBandGainMin, kBandGainMax, kBandGainSteps / 2);
    addSteppedFreq(kHFFreqParam, u"HF Freq", kFreqSteps, kHFMin, kHFMax, kFreqSteps / 2);
    addSteppedGain(kHMFGainParam, u"HMF Gain", kBandGainSteps, kBandGainMin, kBandGainMax, kBandGainSteps / 2);
    addSteppedFreq(kHMFFreqParam, u"HMF Freq", kFreqSteps, kHMFMin, kHMFMax, kFreqSteps / 2);
    addSteppedGain(kLMFGainParam, u"LMF Gain", kBandGainSteps, kBandGainMin, kBandGainMax, kBandGainSteps / 2);
    addSteppedFreq(kLMFFreqParam, u"LMF Freq", kFreqSteps, kLMFMin, kLMFMax, kFreqSteps / 2);
    addSteppedGain(kLFGainParam, u"LF Gain", kBandGainSteps, kBandGainMin, kBandGainMax, kBandGainSteps / 2);
    addSteppedFreq(kLFFreqParam, u"LF Freq", kFreqSteps, kLFMin, kLFMax, kFreqSteps / 2);

    // Meter feeds. Read-only so a host never tries to automate them.
    parameters.addParameter(STR16("Input Meter"), nullptr, 0, 0,
                            Vst::ParameterInfo::kIsReadOnly, kInputMeter);
    parameters.addParameter(STR16("Output Meter"), nullptr, 0, 0,
                            Vst::ParameterInfo::kIsReadOnly, kOutputMeter);

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQController::terminate()
{
    return EditControllerEx1::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQController::setComponentState(IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);

    int32 version = 0;
    if (!streamer.readInt32(version))
        return kResultFalse;

    using namespace EQRange;

    auto restore = [&](Vst::ParamID id, int steps) {
        int32 v = 0;
        if (streamer.readInt32(v))
        {
            if (v < 0) v = 0;
            if (v > steps - 1) v = steps - 1;
            setParamNormalized(id, steps > 1 ? static_cast<double>(v) / (steps - 1) : 0.0);
        }
    };

    restore(kGainParam, kMasterGainSteps);
    restore(kHPFParam, kFreqSteps);
    restore(kLPFParam, kFreqSteps);
    restore(kHFGainParam, kBandGainSteps);  restore(kHFFreqParam, kFreqSteps);
    restore(kHMFGainParam, kBandGainSteps); restore(kHMFFreqParam, kFreqSteps);
    restore(kLMFGainParam, kBandGainSteps); restore(kLMFFreqParam, kFreqSteps);
    restore(kLFGainParam, kBandGainSteps);  restore(kLFFreqParam, kFreqSteps);

    return kResultOk;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API WetEQController::createView(FIDString name)
{
    if (FIDStringsEqual(name, Vst::ViewType::kEditor))
    {
        auto* editor = new VSTGUI::VST3Editor(this, "view", "weteqeditor.uidesc");

        // Discrete zoom steps rather than a draggable window. VSTGUI puts these
        // in the editor's context menu and handles the resize itself.
        //
        // 75 / 100 / 125 only. 150 and 200 were offered first and dropped:
        // the assets are baked at 1x, so anything above 125 interpolates and
        // goes soft, and offering a step that looks worse is not a feature.
        //
        // Discrete on purpose: the panel is a fixed layout, so a free resize
        // buys nothing a step does not, and fixed steps are the only way to
        // ship bitmaps that match the scale being drawn. 533x800 at 100% is
        // small on a 4K display, which is the actual complaint.
        editor->setAllowedZoomFactors({0.75, 1.0, 1.25});
        return editor;
    }
    return nullptr;
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQController::setState(IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQController::getState(IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
} // namespace Yonie
