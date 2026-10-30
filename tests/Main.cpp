#include "../plugins/hex/HexEngine.h"
#include "../plugins/stress/Dynamics.h"
#include "../plugins/lattice/Lattice.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
void testHex();
void testDynamics();
void testLattice();
namespace {
void u16(std::ostream& f,unsigned value) { f.put(static_cast<char>(value&255)); f.put(static_cast<char>((value>>8)&255)); }
void u32(std::ostream& f,unsigned value) { u16(f,value&65535); u16(f,value>>16); }
void wave(const std::string& file,const std::vector<float>& l,const std::vector<float>& r)
{
    std::ofstream f(file,std::ios::binary);
    if(!f) throw std::runtime_error("Cannot write "+file);
    f.write("RIFF",4); u32(f,36+static_cast<unsigned>(l.size()*4)); f.write("WAVEfmt ",8);
    u32(f,16); u16(f,1); u16(f,2); u32(f,48000); u32(f,192000); u16(f,4); u16(f,16);
    f.write("data",4); u32(f,static_cast<unsigned>(l.size()*4));
    for(size_t i=0;i<l.size();++i) { u16(f,static_cast<unsigned>(static_cast<int16_t>(std::clamp(l[i],-1.f,1.f)*32767))); u16(f,static_cast<unsigned>(static_cast<int16_t>(std::clamp(r[i],-1.f,1.f)*32767))); }
}
template<typename Synth> void demo(Synth& synth,const std::string& file)
{
    constexpr int rate=48000, length=rate*7;
    std::vector<float> l(length),r(length);
    synth.prepare(rate);
    for(int pos=0;pos<length;pos+=64)
    {
        if(pos==0) { synth.noteOn(48,95); synth.noteOn(55,85); synth.noteOn(62,100); synth.noteOn(69,75); }
        if(pos==rate*3) for(int note:{48,55,62,69}) synth.noteOff(note);
        synth.render(l.data()+pos,r.data()+pos,std::min(64,length-pos));
    }
    wave(file,l,r);
}
}
int main(int argc,char** argv)
{
    try
    {
        testHex(); std::cout<<"Hex DSP: PASS\n";
        testDynamics(); std::cout<<"Stress DSP: PASS\n";
        testLattice(); std::cout<<"Lattice DSP: PASS\n";
        if(argc==3 && std::string(argv[1])=="--render")
        {
            triad::HexEngine hex; demo(hex,std::string(argv[2])+"/Hex-MkI.wav");
            triad::Lattice lattice; demo(lattice,std::string(argv[2])+"/Lattice.wav");
            std::vector<float> l(48000*5),r(l.size());
            uint32_t noise=12345;
            for(size_t i=0;i<l.size();++i)
            {
                noise=noise*1664525+1013904223;
                const float t=static_cast<float>(i%24000)/48000;
                const float hit=std::exp(-t*22)*(std::sin(6.2831853f*65*t)+.2f*(static_cast<float>(noise>>8)/8388608.f-1));
                l[i]=r[i]=hit*.65f;
            }
            wave(std::string(argv[2])+"/Stress-dry.wav",l,r);
            triad::Dynamics dynamics; dynamics.prepare(48000);
            triad::DynamicsParameters p; p.inputDb=12;p.outputDb=-4;p.ratio=6;p.attackMs=2;p.releaseMs=90;p.distortion=1;
            dynamics.setParameters(p); dynamics.process(l.data(),r.data(),static_cast<int>(l.size()));
            wave(std::string(argv[2])+"/Stress-wet.wav",l,r);
            std::cout<<"Audio examples written to "<<argv[2]<<"\n";
        }
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
}
