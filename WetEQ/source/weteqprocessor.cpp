//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "weteqprocessor.h"
#include "weteqcids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Steinberg;

namespace Yonie {

//------------------------------------------------------------------------
WetEQProcessor::WetEQProcessor()
{
    setControllerClass(kWetEQControllerUID);
}

//------------------------------------------------------------------------
WetEQProcessor::~WetEQProcessor() {}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQProcessor::initialize(FUnknown* context)
{
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk)
        return result;

    addAudioInput(STR16("Stereo In"), Steinberg::Vst::SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQProcessor::terminate()
{
    return AudioEffect::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQProcessor::setActive(TBool state)
{
    if (state)
    {
        engine.reset();
        inputPeakL = 0.0f;
        inputPeakR = 0.0f;
        outputPeakL = 0.0f;
        outputPeakR = 0.0f;
        oldInputMeterL = 0.0f;
        oldInputMeterR = 0.0f;
        oldOutputMeterL = 0.0f;
        oldOutputMeterR = 0.0f;
    }
    return AudioEffect::setActive(state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQProcessor::setupProcessing(Vst::ProcessSetup& newSetup)
{
    engine.prepare(newSetup.sampleRate, newSetup.maxSamplesPerBlock);
    return AudioEffect::setupProcessing(newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQProcessor::canProcessSampleSize(int32 symbolicSampleSize)
{
    if (symbolicSampleSize == Vst::kSample32)
        return kResultTrue;
    return kResultFalse;
}

//------------------------------------------------------------------------
int WetEQProcessor::toStep(Vst::ParamValue normalized, int stepCount)
{
    if (stepCount <= 1)
        return 0;
    int idx = static_cast<int>(normalized * (stepCount - 1) + 0.5);
    if (idx < 0) idx = 0;
    if (idx > stepCount - 1) idx = stepCount - 1;
    return idx;
}

//------------------------------------------------------------------------
void WetEQProcessor::applyParameter(Vst::ParamID id, Vst::ParamValue value)
{
    using namespace EQRange;

    switch (id)
    {
        case kGainParam:    pending.gain    = toStep(value, kMasterGainSteps); break;

        case kHPFParam:     pending.hpf     = toStep(value, kFreqSteps); break;
        case kLPFParam:     pending.lpf     = toStep(value, kFreqSteps); break;

        case kHFGainParam:  pending.hfGain  = toStep(value, kBandGainSteps); break;
        case kHFFreqParam:  pending.hfFreq  = toStep(value, kFreqSteps); break;
        case kHMFGainParam: pending.hmfGain = toStep(value, kBandGainSteps); break;
        case kHMFFreqParam: pending.hmfFreq = toStep(value, kFreqSteps); break;
        case kLMFGainParam: pending.lmfGain = toStep(value, kBandGainSteps); break;
        case kLMFFreqParam: pending.lmfFreq = toStep(value, kFreqSteps); break;
        case kLFGainParam:  pending.lfGain  = toStep(value, kBandGainSteps); break;
        case kLFFreqParam:  pending.lfFreq  = toStep(value, kFreqSteps); break;

        default: break;
    }
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQProcessor::process(Vst::ProcessData& data)
{
    //--- parameter changes ------------------------------------------------
    if (data.inputParameterChanges)
    {
        const int32 numChanged = data.inputParameterChanges->getParameterCount();
        for (int32 index = 0; index < numChanged; ++index)
        {
            if (auto* queue = data.inputParameterChanges->getParameterData(index))
            {
                const int32 numPoints = queue->getPointCount();
                if (numPoints <= 0)
                    continue;
                Vst::ParamValue value;
                int32 sampleOffset;
                // Take the last point in the block: these are stepped knobs,
                // so sample-accurate interpolation would only produce zipper
                // steps part-way through a block.
                if (queue->getPoint(numPoints - 1, sampleOffset, value) == kResultTrue)
                    applyParameter(queue->getParameterId(), value);
            }
        }
        engine.setSettings(pending);
    }

    //--- audio ------------------------------------------------------------
    if (data.numInputs == 0 || data.numOutputs == 0 || data.numSamples <= 0)
        return kResultOk;

    Vst::AudioBusBuffers& input = data.inputs[0];
    Vst::AudioBusBuffers& output = data.outputs[0];

    if (input.numChannels < 2 || output.numChannels < 2)
    {
        for (int32 c = 0; c < output.numChannels; ++c)
            std::memset(output.channelBuffers32[c], 0,
                        data.numSamples * sizeof(Vst::Sample32));
        output.silenceFlags = ((uint64)1 << output.numChannels) - 1;
        return kResultOk;
    }

    float* inL = input.channelBuffers32[0];
    float* inR = input.channelBuffers32[1];
    float* outL = output.channelBuffers32[0];
    float* outR = output.channelBuffers32[1];

    for (int32 i = 0; i < data.numSamples; ++i)
    {
        updatePeak(std::abs(inL[i]), inputPeakL);
        updatePeak(std::abs(inR[i]), inputPeakR);
    }

    engine.processStereo(inL, inR, outL, outR, data.numSamples);

    for (int32 i = 0; i < data.numSamples; ++i)
    {
        updatePeak(std::abs(outL[i]), outputPeakL);
        updatePeak(std::abs(outR[i]), outputPeakR);
    }

    //--- meters -----------------------------------------------------------
    // Same scheme as WetDelay: push through outputParameterChanges only when
    // the value actually moved, and let the host relay it to the controller on
    // the UI thread. Decay constant is identical, so the LEDs fall at the same
    // rate across the line.
    if (data.outputParameterChanges)
    {
        auto sendMeter = [&](Vst::ParamID id, float value, float& oldValue) {
            if (oldValue != value)
            {
                int32 index = 0;
                if (auto* queue = data.outputParameterChanges->addParameterData(id, index))
                {
                    int32 pointIndex = 0;
                    queue->addPoint(0, value, pointIndex);
                }
                oldValue = value;
            }
        };

        sendMeter(kInputMeterL, inputPeakL.load(), oldInputMeterL);
        sendMeter(kInputMeterR, inputPeakR.load(), oldInputMeterR);
        sendMeter(kOutputMeterL, outputPeakL.load(), oldOutputMeterL);
        sendMeter(kOutputMeterR, outputPeakR.load(), oldOutputMeterR);
    }

    output.silenceFlags = 0;
    return kResultOk;
}

//------------------------------------------------------------------------
void WetEQProcessor::updatePeak(float sample, std::atomic<float>& peak)
{
    const float absSample = std::abs(sample);
    const float currentPeak = peak.load();
    if (absSample > currentPeak)
        peak.store(absSample);            // attack: instant
    else
        peak.store(currentPeak * kMeterDecay);   // decay: exponential
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQProcessor::setState(IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);

    int32 version = 0;
    if (!streamer.readInt32(version))
        return kResultFalse;

    EQEngine::Settings s;
    int32 v = 0;
    auto rd = [&](int& dst) {
        if (streamer.readInt32(v)) dst = v;
    };
    rd(s.gain);
    rd(s.hpf);   rd(s.lpf);
    rd(s.hfGain);  rd(s.hfFreq);
    rd(s.hmfGain); rd(s.hmfFreq);
    rd(s.lmfGain); rd(s.lmfFreq);
    rd(s.lfGain);  rd(s.lfFreq);

    pending = s;
    engine.setSettings(s);
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API WetEQProcessor::getState(IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);

    // Versioned from the start so a later parameter addition can still load an
    // old session. WetDelay wrote a bare int and has no room to grow.
    streamer.writeInt32(1);

    const EQEngine::Settings& s = engine.settings();
    streamer.writeInt32(s.gain);
    streamer.writeInt32(s.hpf);     streamer.writeInt32(s.lpf);
    streamer.writeInt32(s.hfGain);  streamer.writeInt32(s.hfFreq);
    streamer.writeInt32(s.hmfGain); streamer.writeInt32(s.hmfFreq);
    streamer.writeInt32(s.lmfGain); streamer.writeInt32(s.lmfFreq);
    streamer.writeInt32(s.lfGain);  streamer.writeInt32(s.lfFreq);

    return kResultOk;
}

//------------------------------------------------------------------------
} // namespace Yonie
