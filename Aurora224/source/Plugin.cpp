#include "Reverb.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"
#include <algorithm>
#include <cmath>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
const FUID processorID(0x52B47C12, 0x936B44EE, 0xAFD29210, 0x11832240);
const FUID controllerID(0x62B47C12, 0x936B44EE, 0xAFD29210, 0x11832241);
constexpr ParamID bypassID = 10;
bool readState(IBStream* stream, std::array<double, aurora::parameterCount>& values, bool& bypass) {
    if (!stream) return false;
    IBStreamer s(stream, kLittleEndian);
    int32 version = 0, b = 0;
    auto next = values;
    if (!s.readInt32(version) || version != 1) return false;
    for (auto& v : next) if (!s.readDouble(v) || !std::isfinite(v) || v < 0. || v > 1.) return false;
    if (!s.readInt32(b)) return false;
    values = next; bypass = b != 0; return true;
}
class Processor final : public AudioEffect {
    aurora::Reverb engine;
    std::array<double, aurora::parameterCount> values = aurora::defaults;
    bool bypass = false;
public:
    Processor() { setControllerClass(controllerID); }
    static FUnknown* create(void*) { return static_cast<IAudioProcessor*>(new Processor); }
    tresult PLUGIN_API initialize(FUnknown* context) override {
        auto result = AudioEffect::initialize(context);
        if (result != kResultOk) return result;
        addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
        return kResultOk;
    }
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* ins, int32 ni, SpeakerArrangement* outs, int32 no) override {
        if (ni != 1 || no != 1 || ins[0] != SpeakerArr::kStereo || outs[0] != SpeakerArr::kStereo) return kResultFalse;
        return AudioEffect::setBusArrangements(ins, ni, outs, no);
    }
    tresult PLUGIN_API canProcessSampleSize(int32 size) override { return size == kSample32 ? kResultTrue : kResultFalse; }
    tresult PLUGIN_API setActive(TBool active) override {
        if (active) engine.prepare(processSetup.sampleRate, values);
        return AudioEffect::setActive(active);
    }
    uint32 PLUGIN_API getTailSamples() override { return uint32(processSetup.sampleRate * 22.); }
    tresult PLUGIN_API process(ProcessData& data) override {
        // Consume automation at its sample offset, including changes in parameter-only flushes.
        auto apply = [&](int32 sample) {
            if (!data.inputParameterChanges) return;
            for (int32 q = 0; q < data.inputParameterChanges->getParameterCount(); ++q) {
                auto* queue = data.inputParameterChanges->getParameterData(q);
                if (!queue) continue;
                for (int32 j = 0; j < queue->getPointCount(); ++j) {
                    int32 offset = 0; ParamValue v = 0.;
                    if (queue->getPoint(j, offset, v) != kResultTrue || !std::isfinite(v)) continue;
                    if (offset != sample && !(data.numSamples == 0)) continue;
                    auto id = queue->getParameterId();
                    if (id < values.size()) values[id] = std::clamp(v, 0., 1.);
                    else if (id == bypassID) bypass = v >= .5;
                }
            }
        };
        if (data.numSamples == 0) { apply(0); return kResultOk; }
        if (data.numInputs < 1 || data.numOutputs < 1 || data.inputs[0].numChannels != 2 || data.outputs[0].numChannels != 2) return kResultOk;
        if (data.symbolicSampleSize != kSample32) return kResultFalse;
        auto** in = data.inputs[0].channelBuffers32; auto** out = data.outputs[0].channelBuffers32;
        if (!in || !out || !in[0] || !in[1] || !out[0] || !out[1]) return kResultFalse;
        data.outputs[0].silenceFlags = 0;
        for (int32 i = 0; i < data.numSamples; ++i) {
            apply(i);
            float l = (data.inputs[0].silenceFlags & 1) ? 0.f : in[0][i];
            float r = (data.inputs[0].silenceFlags & 2) ? 0.f : in[1][i];
            if (!std::isfinite(l)) l = 0.f;
            if (!std::isfinite(r)) r = 0.f;
            float wl = 0.f, wr = 0.f; engine.tick(l, r, wl, wr, values);
            out[0][i] = bypass ? l : wl; out[1][i] = bypass ? r : wr;
        }
        return kResultOk;
    }
    tresult PLUGIN_API getState(IBStream* stream) override {
        if (!stream) return kResultFalse;
        IBStreamer s(stream, kLittleEndian);
        if (!s.writeInt32(1)) return kResultFalse;
        for (auto v : values) if (!s.writeDouble(v)) return kResultFalse;
        return s.writeInt32(bypass ? 1 : 0) ? kResultOk : kResultFalse;
    }
    tresult PLUGIN_API setState(IBStream* stream) override { return readState(stream, values, bypass) ? kResultOk : kResultFalse; }
};
class Controller final : public EditController {
public:
    static FUnknown* create(void*) { return static_cast<IEditController*>(new Controller); }
    tresult PLUGIN_API initialize(FUnknown* context) override {
        auto result = EditController::initialize(context); if (result != kResultOk) return result;
        auto* mode = new StringListParameter(STR16("Algorithm"), 0);
        mode->appendString(STR16("Concert Hall")); mode->appendString(STR16("Plate")); mode->appendString(STR16("Chamber"));
        parameters.addParameter(mode);
        parameters.addParameter(new RangeParameter(STR16("Decay"), 1, STR16("s"), .3, 12., .3 + aurora::defaults[1] * 11.7));
        parameters.addParameter(new RangeParameter(STR16("Predelay"), 2, STR16("ms"), 0., 250., 20.));
        const TChar* names[]{STR16("Size"), STR16("Diffusion"), STR16("Damping"), STR16("Bass Decay"), STR16("Modulation"), STR16("Width"), STR16("Mix")};
        for (int i = 3; i < aurora::parameterCount; ++i)
            parameters.addParameter(new RangeParameter(names[i - 3], i, STR16("%"), 0., 100., aurora::defaults[i] * 100.));
        parameters.addParameter(STR16("Bypass"), nullptr, 1, 0., ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass, bypassID);
        return kResultOk;
    }
    tresult PLUGIN_API setComponentState(IBStream* stream) override {
        auto values = aurora::defaults; bool bypass = false;
        if (!readState(stream, values, bypass)) return kResultFalse;
        for (int i = 0; i < aurora::parameterCount; ++i) setParamNormalized(i, values[i]);
        setParamNormalized(bypassID, bypass ? 1. : 0.); return kResultOk;
    }
};
}
BEGIN_FACTORY_DEF("Triad Audio", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(processorID), PClassInfo::kManyInstances, kVstAudioEffectClass,
           "Aurora 224", Vst::kDistributable, "Fx|Reverb", "1.0.0", kVstVersionString, Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(controllerID), PClassInfo::kManyInstances, kVstComponentControllerClass,
           "Aurora 224 Controller", 0, "", "1.0.0", kVstVersionString, Controller::create)
END_FACTORY
