//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace Yonie {

//------------------------------------------------------------------------
static const Steinberg::FUID kWetEQProcessorUID (0x7A31F4C2, 0x1D8B4E56, 0xA0C39B7E, 0x2F6D8104);
static const Steinberg::FUID kWetEQControllerUID (0x3E9C05AB, 0x6F274D19, 0xB85A1C0F, 0x74E2D963);

#define WetEQVST3Category "Fx|EQ"

//------------------------------------------------------------------------
// Parameter IDs
//
// Every control is a stepped encoder. The dB knobs get 11 positions so that
// 0 dB sits at the centre and a band can actually be defeated; the frequency
// knobs get 10, which is what the briefing asked for and needs no centre.
//------------------------------------------------------------------------
enum WetEQParams : Steinberg::Vst::ParamID
{
    kGainParam    = 0,    // input drive, -20..+20 dB, 11 positions

    kHPFParam     = 1,    // 20 Hz .. 2 kHz,  10 positions
    kLPFParam     = 2,    // 2 kHz .. 20 kHz, 10 positions

    kHFGainParam  = 3,    // -15..+15 dB, 11 positions
    kHFFreqParam  = 4,    // 1.5 kHz .. 22 kHz, 10 positions
    kHMFGainParam = 5,
    kHMFFreqParam = 6,    // 400 Hz .. 10 kHz
    kLMFGainParam = 7,
    kLMFFreqParam = 8,    // 100 Hz .. 4 kHz
    kLFGainParam  = 9,
    kLFFreqParam  = 10,   // 30 Hz .. 600 Hz

    // Output-only, for the LED strips. Each bus has a left and a right column,
    // so an imbalance between the channels is visible instead of being hidden
    // behind a max() of the two.
    kInputMeterL  = 11,
    kInputMeterR  = 12,
    kOutputMeterL = 13,
    kOutputMeterR = 14,

    kParamCount   = 15
};

//------------------------------------------------------------------------
} // namespace Yonie
