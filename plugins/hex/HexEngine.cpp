#include "HexEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include "../../third_party/msfa/synth.h"
#include "../../third_party/msfa/dx7note.h"
#include "../../third_party/msfa/EngineMkI.h"
#include "../../third_party/msfa/exp2.h"
#include "../../third_party/msfa/freqlut.h"
#include "../../third_party/msfa/sin.h"
#include "../../third_party/msfa/lfo.h"

namespace triad {
namespace {
constexpr double internalRate = 49096.0;
constexpr double pi = 3.14159265358979323846;
constexpr int taps = 32, phases = 1024, ringSize = 256, voicesCount = 16;
float safe(float v, float lo, float hi, float fallback) noexcept {
    return std::isfinite(v) ? std::clamp(v,lo,hi) : fallback;
}
std::array<unsigned char,155> initialPatch() {
    // Original program: two FM stacks with a bright transient and mellow body.
    // These are hand-authored parameters, not a Yamaha factory ROM extraction.
    std::array<unsigned char,155> p{};
    for(int op=0;op<6;++op) {
        const int o=21*op;
        p[o]=96; p[o+1]=52; p[o+2]=30; p[o+3]=65;
        p[o+4]=99; p[o+5]=72; p[o+6]=0; p[o+7]=0;
        p[o+8]=50; p[o+9]=0; p[o+10]=12;
        p[o+11]=0; p[o+12]=0; p[o+13]=2;
        p[o+14]=0; p[o+15]=3; p[o+16]=70;
        p[o+17]=0; p[o+18]=1; p[o+19]=0; p[o+20]=7;
    }
    // Algorithm 1: operators 6->5->4->3 and 2->1.
    p[16]=58; p[18]=7;
    p[37]=58; p[39]=1;
    p[58]=68; p[60]=1;
    p[79]=92; p[81]=1; p[3*21+1]=38;
    p[100]=78; p[102]=1;
    p[121]=96; p[123]=1; p[5*21+1]=34;
    p[126]=p[127]=p[128]=p[129]=99;
    p[130]=p[131]=p[132]=p[133]=50;
    p[134]=0; p[135]=0; p[136]=1;
    p[137]=32; p[138]=0; p[139]=0; p[140]=0;
    p[141]=1; p[142]=4; p[143]=0; p[144]=24;
    const char name[11]="GLASS KEYS";
    std::copy_n(name,10,p.begin()+145);
    return p;
}
void sanitize(std::array<unsigned char,155>& p) noexcept {
    for(int op=0;op<6;++op) {
        const int o=op*21;
        for(int i=0;i<11;++i) p[o+i]=std::min<int>(p[o+i],99);
        p[o+11]&=3; p[o+12]&=3; p[o+13]&=7; p[o+14]&=3;
        p[o+15]&=7; p[o+16]=std::min<int>(p[o+16],99);
        p[o+17]&=1; p[o+18]&=31; p[o+19]=std::min<int>(p[o+19],99);
        p[o+20]=std::min<int>(p[o+20],14);
    }
    for(int i=126;i<134;++i) p[i]=std::min<int>(p[i],99);
    p[134]&=31; p[135]&=7; p[136]&=1;
    for(int i=137;i<141;++i) p[i]=std::min<int>(p[i],99);
    p[141]&=1; p[142]=std::min<int>(p[142],5); p[143]&=7;
    p[144]=std::min<int>(p[144],48);
    for(int i=145;i<155;++i) p[i]&=127;
}
bool validPatch(const std::array<unsigned char,155>& p) noexcept {
    auto q=p; sanitize(q); return q==p;
}
}

struct HexEngine::Impl {
    struct Voice {
        Dx7Note dx;
        int note=-1, playedNote=0, velocity=0, quietBlocks=0;
        bool live=false, held=false, deferred=false;
        std::uint64_t age=0;
    };
    EngineMkI core;
    Controllers controls{};
    Lfo lfo{};
    std::array<Voice,voicesCount> voices;
    std::array<unsigned char,155> program=initialPatch();
    std::array<unsigned char,156> internalPatch{};
    std::array<std::array<float,taps>,phases> kernel{};
    std::array<float,ringSize> ring{};
    HexParameters parameters;
    double hostRate=48000.0, sourceTime=0.0;
    std::int64_t generated=0;
    std::uint64_t age=0;
    float bend=0.0f, gain=0.25118864f, targetGain=0.25118864f, gainStep=0.001f;
    bool pedal=false;

    Impl() {
        static std::once_flag tables;
        std::call_once(tables,[] {
            Freqlut::init(internalRate); Exp2::init(); Sin::init();
            Lfo::init(internalRate); PitchEnv::init(internalRate); Env::init_sr(internalRate);
        });
        std::fill(std::begin(controls.values_),std::end(controls.values_),0);
        controls.values_[kControllerPitch]=8192;
        controls.values_[kControllerPitchRange]=2;
        controls.values_[kControllerPitchStep]=0;
        controls.aftertouch_cc=controls.breath_cc=controls.foot_cc=controls.modwheel_cc=0;
        controls.masterTune=0; controls.core=&core; controls.refresh();
        syncPatch();
    }
    void syncPatch() noexcept {
        std::copy(program.begin(),program.end(),internalPatch.begin());
        internalPatch[155]=63;
        lfo.reset(internalPatch.data()+137);
    }
    void makeKernel() noexcept {
        const double cutoff=std::min(1.0,hostRate/internalRate)*0.94;
        for(int ph=0;ph<phases;++ph) {
            const double frac=static_cast<double>(ph)/phases;
            double total=0;
            for(int j=0;j<taps;++j) {
                const double d=j+frac-15.5;
                const double sinc=std::abs(d)<1e-10?cutoff:std::sin(pi*cutoff*d)/(pi*d);
                const double window=0.42+0.5*std::cos(pi*d/16.0)+0.08*std::cos(2*pi*d/16.0);
                kernel[ph][j]=static_cast<float>(sinc*window);
                total+=kernel[ph][j];
            }
            for(auto& h:kernel[ph]) h=static_cast<float>(h/total);
        }
    }
    void tune() noexcept {
        // Feed bend and fine tune as an exact Q24-octave master offset. This
        // avoids the original MIDI pitch-bend integer-range quantization.
        controls.masterTune=static_cast<int32_t>((bend/12.0+parameters.tuneCents/1200.0)*16777216.0);
    }
    void block() noexcept {
        std::array<float,64> mix{};
        const auto lv=lfo.getsample(), ld=lfo.getdelay();
        for(auto& v:voices) if(v.live) {
            std::array<int32_t,64> tmp{};
            v.dx.compute(tmp.data(),lv,ld,&controls);
            float peak=0;
            for(int i=0;i<64;++i) {
                const float x=static_cast<float>(tmp[i])*(1.0f/134217728.0f);
                mix[i]+=x; peak=std::max(peak,std::abs(x));
            }
            // Honour long DX7 release envelopes, including deliberately nonzero L4.
            if(!v.held && !v.deferred && peak<0.000003f) {
                if(++v.quietBlocks>64) v.live=false;
            } else v.quietBlocks=0;
        }
        for(int i=0;i<64;++i) ring[static_cast<std::size_t>(generated+i)&(ringSize-1)]=mix[i];
        generated+=64;
    }
};

HexEngine::HexEngine():impl(std::make_unique<Impl>()) { prepare(48000); }
HexEngine::~HexEngine()=default;
void HexEngine::prepare(double sampleRate) {
    impl->hostRate=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.0,384000.0):48000.0;
    impl->gainStep=static_cast<float>(1.0-std::exp(-1.0/(0.015*impl->hostRate)));
    impl->makeKernel(); reset();
}
void HexEngine::reset() {
    auto& d=*impl;
    for(auto& v:d.voices) {
        v.live=v.held=v.deferred=false; v.note=-1; v.quietBlocks=0;
        v.dx.oscSync(); v.dx.resetFeedback();
    }
    d.ring.fill(0); d.generated=0; d.sourceTime=0; d.pedal=false; d.age=0;
    d.lfo=Lfo{}; d.syncPatch(); d.bend=0; d.tune(); d.gain=d.targetGain;
}
void HexEngine::setParameters(const HexParameters& p) {
    auto& d=*impl;
    auto q=p; q.outputDb=safe(p.outputDb,-60,12,-12); q.tuneCents=safe(p.tuneCents,-100,100,0);
    q.algorithm=std::clamp(p.algorithm,0,31); q.feedback=std::clamp(p.feedback,0,7);
    const bool change=q.algorithm!=d.parameters.algorithm || q.feedback!=d.parameters.feedback;
    d.parameters=q; d.targetGain=std::pow(10.0f,q.outputDb/20.0f); d.tune();
    if(change) {
        d.program[134]=static_cast<unsigned char>(q.algorithm); d.program[135]=static_cast<unsigned char>(q.feedback);
        d.internalPatch[134]=d.program[134]; d.internalPatch[135]=d.program[135];
        for(auto& v:d.voices) if(v.live) v.dx.setAlgorithmFeedback(q.algorithm,q.feedback);
    }
}
void HexEngine::noteOn(int note,int velocity) {
    if(note<0||note>127) return;
    if(velocity<=0) { noteOff(note); return; }
    auto& d=*impl;
    auto* voice=&d.voices[0];
    bool found=false;
    for(auto& v:d.voices) if(!v.live) {voice=&v;found=true;break;}
    if(!found) {
        // Prefer released voices, then steal the oldest held voice.
        for(auto& v:d.voices)
            if((voice->held&&!v.held) || (voice->held==v.held&&v.age<voice->age)) voice=&v;
    }
    voice->dx.resetFeedback();
    voice->note=note; voice->playedNote=std::clamp(note+static_cast<int>(d.program[144])-24,0,127);
    voice->velocity=std::clamp(velocity,1,127);
    voice->dx.init(d.internalPatch.data(),voice->playedNote,voice->velocity);
    if(d.program[136]) voice->dx.oscSync();
    voice->live=voice->held=true; voice->deferred=false;
    voice->quietBlocks=0; voice->age=++d.age;
    d.lfo.keydown();
}
void HexEngine::noteOff(int note) {
    auto& d=*impl;
    for(auto& v:d.voices) if(v.live&&v.note==note&&v.held) {
        v.held=false; v.deferred=d.pedal;
        if(!v.deferred) v.dx.keyup();
    }
}
void HexEngine::sustain(bool down) {
    auto& d=*impl; d.pedal=down;
    if(!down) for(auto& v:d.voices) if(v.live&&v.deferred&&!v.held) {v.deferred=false;v.dx.keyup();}
}
void HexEngine::pitchBend(float semitones) {
    impl->bend=safe(semitones,-24,24,0); impl->tune();
}
void HexEngine::render(float* left,float* right,int count) noexcept {
    if(count<=0||left==nullptr||right==nullptr) return;
    auto& d=*impl;
    const double step=internalRate/d.hostRate;
    for(int i=0;i<count;++i) {
        const auto k=static_cast<std::int64_t>(d.sourceTime);
        while(d.generated<=k) d.block();
        const auto phase=std::clamp(static_cast<int>((d.sourceTime-k)*phases),0,phases-1);
        float value=0;
        for(int j=0;j<taps;++j) {
            if(k>=j) value+=d.ring[static_cast<std::size_t>(k-j)&(ringSize-1)]*d.kernel[phase][j];
        }
        d.gain+=(d.targetGain-d.gain)*d.gainStep;
        value*=d.gain;
        // No extra saturation: preserve the DX7 digital engine's spectral content.
        left[i]=right[i]=std::isfinite(value)?value:0.0f;
        d.sourceTime+=step;
    }
}
const std::array<unsigned char,155>& HexEngine::patch() const {return impl->program;}
void HexEngine::setPatch(const std::array<unsigned char,155>& patch) {
    auto& d=*impl; d.program=patch; sanitize(d.program);
    d.parameters.algorithm=d.program[134]; d.parameters.feedback=d.program[135];
    reset();
}
int HexEngine::latencySamples() const noexcept {return static_cast<int>(std::ceil(15.5*impl->hostRate/internalRate));}
bool HexEngine::loadSysex(const unsigned char* data,std::size_t size) {
    std::array<unsigned char,155> p{};
    if(!parseSysex(data,size,p)) return false;
    setPatch(p); return true;
}
bool HexEngine::parseSysex(const unsigned char* data,std::size_t size,std::array<unsigned char,155>& result) noexcept {
    if(data==nullptr || (size!=163&&size!=4104)) return false;
    if(data[0]!=0xf0||data[1]!=0x43||data[2]>0x0f||data[size-1]!=0xf7) return false;
    const std::size_t payload=size-8;
    if(data[3]!=(size==163?0x00:0x09)||data[4]!=(payload>>7)||data[5]!=(payload&127)) return false;
    unsigned sum=0;
    for(std::size_t i=6;i<size-2;++i) {if(data[i]>127)return false;sum+=data[i];}
    if(data[size-2]>127||((sum+data[size-2])&127)!=0) return false;
    std::array<unsigned char,155> p{};
    if(size==163) std::copy_n(data+6,155,p.begin());
    else {
        // DX7 VMEM packed voice -> VCED unpacked edit buffer. Load bank slot 1.
        const auto* b=data+6;
        for(int op=0;op<6;++op) {
            const int u=op*21,v=op*17;
            std::copy_n(b+v,11,p.begin()+u);
            p[u+11]=b[v+11]&3; p[u+12]=(b[v+11]>>2)&3;
            p[u+13]=b[v+12]&7; p[u+20]=(b[v+12]>>3)&15;
            p[u+14]=b[v+13]&3; p[u+15]=(b[v+13]>>2)&7;
            p[u+16]=b[v+14]; p[u+17]=b[v+15]&1;
            p[u+18]=(b[v+15]>>1)&31; p[u+19]=b[v+16];
        }
        std::copy_n(b+102,9,p.begin()+126);
        p[135]=b[111]&7; p[136]=(b[111]>>3)&1;
        std::copy_n(b+112,4,p.begin()+137);
        p[141]=b[116]&1; p[142]=(b[116]>>1)&7; p[143]=(b[116]>>4)&7;
        std::copy_n(b+117,11,p.begin()+144);
    }
    if(!validPatch(p)) return false;
    result=p; return true;
}
}
