#include "../plugins/hex/HexEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
namespace {
constexpr int rate=48000;
void u16(std::ostream& f,unsigned n) { f.put(static_cast<char>(n&255));f.put(static_cast<char>((n>>8)&255)); }
void u32(std::ostream& f,unsigned n) {u16(f,n&65535);u16(f,n>>16);}
void wave(const fs::path& path,const std::vector<float>& samples) {
    std::ofstream f(path,std::ios::binary);
    if(!f)throw std::runtime_error("Cannot write WAV");
    f.write("RIFF",4);u32(f,36+samples.size()*2);f.write("WAVEfmt ",8);u32(f,16);
    u16(f,1);u16(f,1);u32(f,rate);u32(f,rate*2);u16(f,2);u16(f,16);
    f.write("data",4);u32(f,samples.size()*2);
    for(float x:samples)u16(f,static_cast<unsigned>(static_cast<int16_t>(std::clamp(x,-1.f,1.f)*32767)));
}
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
struct Event {int sample,note,velocity;};
std::vector<float> render(triad::HexEngine& h,const std::vector<Event>& events,int length) {
    std::vector<float> l(length),r(length);
    h.reset();int position=0;
    for(const auto& event:events) {
        if(event.sample>position)h.render(l.data()+position,r.data()+position,event.sample-position);
        if(event.velocity>0)h.noteOn(event.note,event.velocity);else h.noteOff(event.note);
        position=event.sample;
    }
    h.render(l.data()+position,r.data()+position,length-position);
    for(size_t i=0;i<l.size();++i)require(std::isfinite(l[i])&&l[i]==r[i],"Invalid or non-mono DX7 audio");
    return l;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Usage: render-hex-sounds PACK_DIRECTORY");
        fs::path root=argv[1];fs::create_directories(root/"Listen");
        std::ifstream manifest(root/"voices.tsv");require(bool(manifest),"Missing voice manifest");
        std::ofstream stats(root/"Validation.tsv");stats<<"file\tpeak\trms\tchord_peak\tlate_release_peak\n";
        std::vector<float> medley;std::string line;int count=0;
        while(std::getline(manifest,line)) {
            const auto split=line.find('\t');require(split!=std::string::npos,"Bad manifest line");
            const auto filename=line.substr(0,split),type=line.substr(split+1);
            std::ifstream f(root/"Sounds"/filename,std::ios::binary);
            std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(f),{}};
            std::array<unsigned char,155> patch{};
            require(triad::HexEngine::parseSysex(bytes.data(),bytes.size(),patch),"SysEx parser rejected voice");
            triad::HexEngine h;h.prepare(rate);h.setPatch(patch);
            require(h.patch()==patch,"Voice changed during import");
            const int note=type=="bass"?36:(type=="bell"?72:60);
            const bool held=type=="pad"||type=="lead"||type=="fx";
            std::vector<Event> events;
            if(type=="pad") {
                for(int n:{48,55,60,64})events.push_back({0,n,100});
                for(int n:{48,55,60,64})events.push_back({rate*3,n,0});
            } else if(held) {
                events={{0,note,104},{rate*2,note,0},{rate*3,note+7,90},{rate*4,note+7,0}};
            } else {
                events={{0,note,76},{rate*3/4,note,0},{rate,note+7,108},{rate*7/4,note+7,0},
                        {rate*2,note+12,96},{rate*11/4,note+12,0}};
            }
            auto audio=render(h,events,rate*7);double peak=0,energy=0;
            for(float x:audio){peak=std::max(peak,static_cast<double>(std::abs(x)));energy+=double(x)*x;}
            require(energy/audio.size()>1e-9,"Silent patch");require(peak<.98,"Demo patch clipped");
            wave(root/"Listen"/(fs::path(filename).stem().string()+".wav"),audio);
            // A consistent middle-register note is used for cross-patch level/spectrum analysis.
            const auto probe=render(h,{{0,note,100},{rate*2,note,0}},rate*4);
            wave(root/"Listen"/("_probe-"+std::to_string(count)+".wav"),probe);
            double chordPeak=0;
            for(int velocity:{48,100,127})for(int key:{36,60,84}) {
                auto check=render(h,{{0,key,velocity},{rate/2,key,0}},rate);
                for(float x:check)chordPeak=std::max(chordPeak,static_cast<double>(std::abs(x)));
            }
            const auto chord=render(h,{{0,48,127},{0,55,127},{0,60,127},{0,64,127},
                                      {rate,48,0},{rate,55,0},{rate,60,0},{rate,64,0}},rate*3);
            for(float x:chord)chordPeak=std::max(chordPeak,static_cast<double>(std::abs(x)));
            require(chordPeak<.98,"High-velocity chord exceeds headroom");
            auto release=render(h,{{0,note,110},{rate,note,0}},rate*18);double tail=0;
            for(size_t i=release.size()-rate;i<release.size();++i)tail=std::max(tail,static_cast<double>(std::abs(release[i])));
            require(tail<.001,"Voice does not release sufficiently");
            stats<<filename<<'\t'<<peak<<'\t'<<std::sqrt(energy/audio.size())<<'\t'<<chordPeak<<'\t'<<tail<<'\n';
            // Demo montage uses unprocessed excerpts at each patch's actual rendered level.
            const int excerpt=held?rate*3:rate*2;
            for(int i=0;i<excerpt;++i) {
                const float fade=std::min({1.f,i/480.f,(excerpt-1-i)/480.f});
                medley.push_back(audio[static_cast<size_t>(i)]*std::max(0.f,fade));
            }
            medley.insert(medley.end(),rate/4,0.f);
            std::cout<<filename<<": PASS peak="<<peak<<" stress_peak="<<chordPeak<<" tail="<<tail<<'\n';++count;
        }
        require(count==16,"Expected sixteen patches");wave(root/"Listen"/"00 - All Sounds.wav",medley);
        std::cout<<"All sixteen patches imported and rendered successfully.\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
