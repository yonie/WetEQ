//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "weteqprocessor.h"
#include "weteqcontroller.h"
#include "weteqcids.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

#define stringPluginName "WetEQ"

using namespace Steinberg::Vst;
using namespace Yonie;

//------------------------------------------------------------------------
//  VST Plug-in Entry
//------------------------------------------------------------------------

BEGIN_FACTORY_DEF ("Yonie",
                   "https://github.com/yonie",
                   "mailto:contact@wetvst.com")

	DEF_CLASS2 (INLINE_UID_FROM_FUID(kWetEQProcessorUID),
				PClassInfo::kManyInstances,
				kVstAudioEffectClass,
				stringPluginName,
				Vst::kDistributable,
				WetEQVST3Category,
				FULL_VERSION_STR,
				kVstVersionString,
				WetEQProcessor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (kWetEQControllerUID),
				PClassInfo::kManyInstances,
				kVstComponentControllerClass,
				stringPluginName "Controller",
				0,
				"",
				FULL_VERSION_STR,
				kVstVersionString,
				WetEQController::createInstance)

END_FACTORY
