#include "../plugins/hex/HexEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {
void requireHex(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
std::array<unsigned char,155> sinePatch(triad::HexEngine& h) {
    auto p=h.patch();
    for(int op=0;op<6;++op) {
        const int o=21*op;
        p[o]=p[o+1]=p[o+2]=p[o+3]=99;
        p[o+4]=p[o+5]=p[o+6]=99;p[o+7]=0;
        p[o+9]=p[o+10]=0;p[o+13]=p[o+14]=p[o+15]=0;
        p[o+16]=0;p[o+17]=0;p[o+18]=1;p[o+19]=0;p[o+20]=7;
    }
    p[121]=99;p[134]=31;p[135]=0;p[144]=24;
    return p;
}
std::vector<unsigned char> singleSysex(const std::array<unsigned char,155>& p) {
    std::vector<unsigned char> s(163);
    s[0]=0xf0;s[1]=0x43;s[2]=0;s[3]=0;s[4]=1;s[5]=27;
    std::copy(p.begin(),p.end(),s.begin()+6);
    unsigned sum=0;for(auto b:p)sum+=b;
    s[161]=static_cast<unsigned char>((128-(sum&127))&127);s[162]=0xf7;
    return s;
}
void checksum(std::vector<unsigned char>& s) {
    unsigned sum=0;for(std::size_t i=6;i<s.size()-2;++i)sum+=s[i];
    s[s.size()-2]=static_cast<unsigned char>((128-(sum&127))&127);
}
double energy(const std::vector<float>& x) {double e=0;for(float v:x)e+=v*v;return e/x.size();}
double frequency(triad::HexEngine& h,int note,float bend=0) {
    h.reset();h.noteOn(note,100);h.pitchBend(bend);
    std::vector<float> l(48000),r(48000);h.render(l.data(),r.data(),static_cast<int>(l.size()));
    int crossings=0;for(int i=12001;i<48000;++i)if(l[i-1]<=0&&l[i]>0)++crossings;
    return crossings/0.75;
}
}
void testHex() {
    triad::HexEngine h;h.prepare(48000);
    std::vector<float> l(48000),r(48000);
    h.render(l.data(),r.data(),48000);
    requireHex(energy(l)==0,"HEX must be silent before MIDI");
    const auto p=sinePatch(h);h.setPatch(p);
    requireHex(std::abs(frequency(h,69)-440.0)<2,"HEX A4 pitch calibration");
    requireHex(std::abs(frequency(h,69,12)-880.0)<3,"HEX octave pitch bend");
    h.reset();h.noteOn(60,100);h.render(l.data(),r.data(),48000);
    requireHex(energy(l)>1e-5,"HEX must sound after note-on");
    requireHex(l==r,"HEX authentic mono engine is dual-mono");
    h.sustain(true);h.noteOff(60);h.render(l.data(),r.data(),48000);
    requireHex(energy(l)>1e-5,"HEX sustain must hold released notes");
    h.sustain(false);
    for(int i=0;i<3;++i)h.render(l.data(),r.data(),48000);
    requireHex(energy(l)<1e-10,"HEX fast release must return to silence");

    auto sysex=singleSysex(p);std::array<unsigned char,155> decoded{};
    requireHex(triad::HexEngine::parseSysex(sysex.data(),sysex.size(),decoded)&&decoded==p,"HEX VCED SysEx round trip");
    auto bad=sysex;bad[20]^=1;
    requireHex(!h.loadSysex(bad.data(),bad.size()),"HEX must reject checksum mismatch");
    bad=sysex;bad[6]=128;checksum(bad);
    requireHex(!h.loadSysex(bad.data(),bad.size()),"HEX must reject non-seven-bit payload");
    bad=sysex;bad[6]=100;checksum(bad);
    requireHex(!h.loadSysex(bad.data(),bad.size()),"HEX must reject out-of-range operator parameter");
    requireHex(!h.loadSysex(nullptr,163),"HEX must reject null SysEx");
    requireHex(!h.loadSysex(sysex.data(),162),"HEX must reject truncated SysEx");
    std::vector<unsigned char> bank(4104);bank[0]=0xf0;bank[1]=0x43;bank[3]=9;bank[4]=32;bank[4103]=0xf7;
    for(int op=0;op<6;++op) {
        const int u=op*21,b=6+op*17;
        std::copy_n(p.begin()+u,11,bank.begin()+b);
        bank[b+11]=p[u+11]|(p[u+12]<<2);
        bank[b+12]=p[u+13]|(p[u+20]<<3);
        bank[b+13]=p[u+14]|(p[u+15]<<2);
        bank[b+14]=p[u+16];bank[b+15]=p[u+17]|(p[u+18]<<1);bank[b+16]=p[u+19];
    }
    std::copy_n(p.begin()+126,9,bank.begin()+108);
    bank[117]=p[135]|(p[136]<<3);
    std::copy_n(p.begin()+137,4,bank.begin()+118);
    bank[122]=p[141]|(p[142]<<1)|(p[143]<<4);
    std::copy_n(p.begin()+144,11,bank.begin()+123);
    checksum(bank);
    requireHex(triad::HexEngine::parseSysex(bank.data(),bank.size(),decoded)&&decoded==p,"HEX VMEM bank unpack round trip");

    // Every algorithm, feedback extremes, voice stealing, and unusual host rates.
    for(double sr:{8000.0,44100.0,48000.0,96000.0,192000.0}) {
        h.prepare(sr);h.setPatch(p);
        std::array<float,257> a{},b{};
        for(int alg=0;alg<32;++alg) {
            triad::HexParameters params;params.algorithm=alg;params.feedback=7;
            h.setParameters(params);
            h.noteOn(36+alg,127);
            h.render(a.data(),b.data(),257);
            for(float v:a)requireHex(std::isfinite(v)&&std::abs(v)<16,"HEX extreme rendering must stay finite and bounded");
        }
    }
    // Preparing another instance at another host rate must not detune this one.
    h.prepare(48000);h.setPatch(p);
    triad::HexEngine other;other.prepare(96000);
    requireHex(std::abs(frequency(h,69)-440.0)<2,"HEX instances must have independent sample rates");
}
