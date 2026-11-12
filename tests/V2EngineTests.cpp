#include "Source/TransientEngineV2.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>

namespace {
using Engine=tdp2::Engine;using Settings=tdp2::Settings;
constexpr int SR=44100;
void check(bool ok,const char* what){if(!ok){std::cerr<<"FAILED: "<<what<<"\n";std::exit(1);}}
float burst(const Settings& setting,int from,int to,float frequency=300.f) {
    Engine p;p.prepare(SR,128);p.configure(setting,true);
    double sum=0;int count=0;
    for(int n=0;n<SR;++n){
        const int t=n-SR/8;
        const float amp=t<0?0.f:.25f*std::exp(-t/(SR*.18f));
        float l=amp*std::sin(2*3.14159265f*frequency*n/SR),r=l;
        p.process(l,r);
        if(n>=from&&n<to){sum+=l*l;++count;}
    }
    return std::sqrt(sum/std::max(1,count));
}
void unity(const char* name,Settings s,int latency,float tolerance) {
    Engine p;p.prepare(SR,128);p.configure(s,true);
    check(p.latencySamples()==latency,"expected PDC samples");
    std::array<float,16384> reference {};
    float error=0.f;
    for(int n=0;n<16384;++n){
        const float x=.17f*std::sin(n*.043f)+.23f*std::sin(n*.361f);
        reference[(size_t)n]=x;
        float l=x,r=x*.65f;p.process(l,r);
        if(n>=2048){
            float target=reference[(size_t)(n-latency)];
            error=std::max(error,std::fabs(l-target));
            check(std::isfinite(l)&&std::isfinite(r),"finite output");
        }
    }
    std::cout<<name<<" maxError="<<error<<"\n";
    check(error<tolerance,name);
}
}
int main(){
    Settings s;
    unity("V2 full range neutral",s,0,2.e-6f);
    s.bands=3;
    unity("V2 complementary IIR neutral",s,0,2.e-5f);
    s.crossover=1;
    unity("V2 linear-phase FIR neutral",s,128,3.e-5f);
    s.bands=1;s.crossover=0;s.lookaheadMs=1.5f;
    unity("V2 envelope lookahead neutral",s,66,2.e-6f);
    s.lookaheadMs=0;s.spectral=true;
    unity("V2 STFT unity/reconstruction",s,512,.003f);
    s.spectral=false;
    const int t=SR/8;
    const float initial=burst(s,t,t+600);
    s.band[1].attackDb=15;s.band[1].attackMs=30;
    const float strengthened=burst(s,t,t+600);
    std::cout<<"attack initial="<<initial<<" modified="<<strengthened<<"\n";
    check(strengthened>initial*1.12f,"adaptive attack gain must have audible action");
    s.band[1].attackDb=0;
    const float tail=burst(s,t+SR/20,t+SR/8);
    s.band[1].sustainDb=18;
    s.band[1].follow=1;
    s.band[1].holdMs=12;
    const float sustained=burst(s,t+SR/20,t+SR/8);
    std::cout<<"sustain initial="<<tail<<" modified="<<sustained<<"\n";
    check(sustained>tail*1.08f,"live sustain action");
    s.band[1].sustainDb=0;
    const float plainRoom=burst(s,t+SR/20,t+SR/8);
    s.roomAmount=2;s.roomEarlyMs=12;s.roomBloomMs=190;
    const float room=burst(s,t+SR/20,t+SR/8);
    check(room>plainRoom*1.1f,"room bloom must affect tail");
    s.roomAmount=0;
    s.band[1].focusMix=1;s.band[1].focusHz=1200;s.band[1].focusQ=1;
    s.band[1].attackDb=10;
    const float centered=burst(s,t,t+600,1200.f);
    const float off=burst(s,t,t+600,100.f);
    std::cout<<"focus center="<<centered<<" off="<<off<<" (different source levels)\n";
    check(std::isfinite(centered)&&std::isfinite(off),"band-focus output finite");
    s.band[1].attackDb=0;s.band[1].focusMix=0;
    s.peakGuard=true;s.peakGuard4x=true;s.ceilingDb=-6;s.guardLookaheadMs=1;
    Engine guard;guard.prepare(SR,128);guard.configure(s,true);
    check(guard.latencySamples()==44,"1ms guard latency");
    const float ceiling=tdp::dbGain(-6);
    float maxPeak=0;
    for(int n=0;n<SR;++n){
        float l=1.7f*std::sin(.05f*n),r=1.6f*std::sin(.11f*n);
        guard.process(l,r);
        maxPeak=std::max(maxPeak,std::max(std::fabs(l),std::fabs(r)));
        check(std::isfinite(l)&&std::isfinite(r),"guard output finite");
    }
    check(maxPeak<=ceiling+1.e-5f,"sample-peak guard ceiling");
    s.peakGuard=false;s.peakGuard4x=false;s.spectral=true;s.bands=3;s.crossover=1;
    s.spectralAttackDb=5;s.spectralTonalDb=-4;s.roomAmount=.5f;
    s.band[0].attackDb=5;s.band[1].sustainDb=12;s.band[2].bodyDb=-7;
    Engine all;all.prepare(SR,128);all.configure(s,true);
    check(all.latencySamples()==640,"FIR+STFT PDC");
    auto t1=std::chrono::high_resolution_clock::now();
    double outputEnergy=0;
    for(int n=0;n<SR*2;++n){
        float l=.15f*std::sin(.023f*n),r=.14f*std::sin(.37f*n);
        all.process(l,r);
        check(std::isfinite(l)&&std::isfinite(r),"full-feature finite output");
        outputEnergy+=double(l)*l+double(r)*r;
    }
    const double sec=std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now()-t1).count();
    std::cout<<"all-features processing 2s wall clock="<<sec<<"s energy="<<outputEnergy<<"\n";
    check(outputEnergy>1,"all-feature audio output energy");
    std::cout<<"V2 DSP PASS\n";
}
