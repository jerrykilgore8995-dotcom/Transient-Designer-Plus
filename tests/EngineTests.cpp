#include "Source/TransientEngine.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>

namespace {
using tdp::Engine;
using tdp::Settings;
constexpr int fs=44100;
void check(bool x,const char* reason) {
    if(!x){std::cerr<<"FAIL: "<<reason<<"\n";std::exit(1);}
}
float runBurst(const Settings& s,int measurementStart,int measurementEnd) {
    Engine e;e.prepare(fs,512);e.setSettings(s);
    double energy=0;int nSamp=0;
    for(int n=0;n<fs*2;++n) {
        const int t=n-fs/4;
        float amp=t<0?0.f:std::exp(-(float)t/(fs*.21f));
        float v=amp*.25f*std::sin(2*tdp::pi*500.f*n/fs);
        float l=v,r=v;
        e.process(l,r);
        if(n>=measurementStart&&n<measurementEnd){energy+=l*l;++nSamp;}
    }
    return (float)std::sqrt(energy/std::max(1,nSamp));
}
}
int main(){
    // Fullband null: floating point unity without drive, clipping or crossover.
    Settings s;Engine eng;eng.prepare(fs,512);eng.setSettings(s);
    float maxError=0;
    for(int n=0;n<fs;++n) {
        float l=.35f*std::sin(n*.0631f),r=.20f*std::sin(n*.04113f);
        const float dl=l,dr=r;eng.process(l,r);
        maxError=std::max(maxError,std::max(std::fabs(l-dl),std::fabs(r-dr)));
    }
    std::cout<<"Fullband zero-gain null max error: "<<maxError<<"\n";
    check(maxError<1.e-6f,"Neutral fullband must null");

    // Complementary low / mid / high splitter must reconstruct at zero band gain.
    s.bands=3;s.lowHz=145;s.highHz=2770;eng.prepare(fs,512);eng.setSettings(s);
    maxError=0;
    for(int n=0;n<fs;++n) {
        float l=.4f*std::sin(n*.023f)+.23f*std::sin(n*.81f);
        float r=.21f*std::sin(n*.17f)+.15f*std::sin(n*1.1f);
        const float dl=l,dr=r;eng.process(l,r);
        maxError=std::max(maxError,std::max(std::fabs(l-dl),std::fabs(r-dr)));
    }
    std::cout<<"Three-band zero-gain null max error: "<<maxError<<"\n";
    check(maxError<2.e-5f,"Complementary 3-band null must be tight");

    s.bands=1;
    const float baseAttack=runBurst(s,fs/4,fs/4+350);
    s.band[1].attackDb=15.f;s.band[1].attackMs=38.f;
    const float raisedAttack=runBurst(s,fs/4,fs/4+350);
    std::cout<<"Attack rms baseline="<<baseAttack<<" enhanced="<<raisedAttack<<"\n";
    check(raisedAttack>baseAttack*1.12f,"Attack control must have audible action");

    s.band[1].attackDb=0.f;
    const float baseTail=runBurst(s,fs/4+fs/12,fs/4+fs/6);
    s.band[1].sustainDb=17.f;s.band[1].sustainHoldMs=20.f;
    s.band[1].sustainDecayMs=1200.f;
    const float raisedTail=runBurst(s,fs/4+fs/12,fs/4+fs/6);
    std::cout<<"Sustain rms baseline="<<baseTail<<" enhanced="<<raisedTail<<"\n";
    check(raisedTail>baseTail*1.13f,"Sustain must affect decay energy");

    s.band[1].sustainDb=0;s.band[1].bodyDb=12.f;
    const float raisedBody=runBurst(s,fs/4+fs/30,fs/4+fs/15);
    s.band[1].bodyDb=0;
    const float baseBody=runBurst(s,fs/4+fs/30,fs/4+fs/15);
    std::cout<<"Body rms baseline="<<baseBody<<" enhanced="<<raisedBody<<"\n";
    check(raisedBody>baseBody*1.12f,"Body must affect middle of hit");

    s.peakGuard=true;s.ceilingDb=-6;s.lookahead=3;
    eng.prepare(fs,512);eng.setSettings(s);
    check(eng.latencySamples()==132,"3ms at 44.1k rounds to 132 samples");
    const float ceiling=tdp::dbGain(-6);
    float maximum=0;
    for(int n=0;n<fs;++n){
        float l=1.8f*std::sin(n*.1f),r=-1.4f*std::sin(n*.13f);
        eng.process(l,r);
        maximum=std::max(maximum,std::max(std::fabs(l),std::fabs(r)));
        check(std::isfinite(l)&&std::isfinite(r),"Must remain finite");
    }
    std::cout<<"Sample peak guard maximum="<<maximum<<" ceiling="<<ceiling<<"\n";
    check(maximum<=ceiling+1.e-5f,"Peak Guard must respect sample ceiling");

    // Reference only: execution time here cannot predict a SER8 host's CPU meter.
    s.peakGuard=false;s.lookahead=0;s.bands=3;
    s.band[0].attackDb=8;s.band[1].bodyDb=-5;s.band[2].sustainDb=11;
    eng.prepare(fs,512);eng.setSettings(s);
    auto start=std::chrono::high_resolution_clock::now();
    for(int n=0;n<fs*5;++n) {
        float l=.1f*std::sin(n*.2f),r=.09f*std::sin(n*.41f);
        eng.process(l,r);
    }
    const double seconds=std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now()-start).count();
    std::cout<<"5 seconds of stereo tri-band DSP computed in "<<seconds<<" wall seconds\n";
    std::cout<<"PASS - core DSP checks\n";return 0;
}
