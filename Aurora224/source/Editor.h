#pragma once
#include "pluginterfaces/gui/iplugview.h"
namespace Steinberg { namespace Vst { class EditController; } }
Steinberg::IPlugView* makeLarryEditor(Steinberg::Vst::EditController* controller);
