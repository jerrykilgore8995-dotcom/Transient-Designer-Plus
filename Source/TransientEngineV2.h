#pragma once
// Transient Designer Plus V2.0: genuine adaptive attack/body/sustain tracking.
// Real-time path: no allocation, no locking, no dynamic FFT planning.
// Optional FIR and STFT features are preallocated in prepare().
#include "TransientEngine.h"
#include "LinearPhaseSplit.h"
#include "SpectralSeparation.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tdp2 {
using tdp::clampf;
using tdp::dbGain;
using tdp::onePole;
constexpr float kPi=3.14159265358979323846f;
constexpr int kDelaySize=4096;
struct Band {
    float attackDb=0.f,attackMs=18.f,attackCurve=1.f;
    float bodyDb=0.f,bodyMs=100.f;
    float sustainDb=0.f,holdMs=60.f,decayMs=700.f;
    float follow=.75f;         // actual live envelope vs static decay
    float focusHz=1200.f,focusQ=1.f,focusMix=0.f; // detector-only band-pass
    bool enabled=true,solo=false;
};
struct Settings {
    int bands=1,stereo=0,crossover=0; // band count; 0 LINK 1 DUAL 2 M/S; 0 IIR 1 linear FIR
    float lowHz=180.f,highHz=3200.f,link=1.f;
    float inputDb=0.f,outputDb=0.f,mix=1.f;
    float sensitivity=.6f,peakBlend=.75f,sidechainHPF=20.f,edgeSoftness=.35f;
    float rearmMs=4.f,groupWindowMs=60.f,noiseGateDb=-75.f,smoothingMs=1.5f;
    int grouping=1;  // 0 separate hits, 1 adaptive, 2 group near hits
    float lookaheadMs=0.f;
    float midAttack=1.f,sideAttack=1.f,midSustain=1.f,sideSustain=1.f;
    float roomAmount=0.f,roomEarlyMs=25.f,roomBloomMs=220.f,roomDamp=.5f,roomWidth=1.f;
    bool autoGain=false,delta=false,peakGuard=false,peakGuard4x=false;
    float guardLookaheadMs=0.f,ceilingDb=-.5f;
    bool spectral=false;
    float spectralAttackDb=0.f,spectralTonalDb=0.f,spectralSense=.6f;
    std::array<Band,3> band {};
};
struct FocusFilter {
    float b0=1.f,b1=0.f,b2=0.f,a1=0.f,a2=0.f,z1=0.f,z2=0.f;
    void configure(float fs,float frequency,float q) noexcept {
        frequency=clampf(frequency,35.f,fs*.45f);
        const float w=2*kPi*frequency/fs;
        const float alpha=std::sin(w)/(2.f*clampf(q,.25f,8.f));
        const float a0=1.f+alpha;
        b0=alpha/a0; b1=0.f; b2=-alpha/a0;
        a1=-2.f*std::cos(w)/a0; a2=(1.f-alpha)/a0;
    }
    float run(float x) noexcept {
        const float y=b0*x+z1;
        z1=b1*x-a1*y+z2;
        z2=b2*x-a2*y;
        return y;
    }
    void reset() noexcept {z1=z2=0.f;}
};
struct Shape {
    float attack=0.f,body=0.f,sustain=0.f,room=0.f;
};
struct Detector {
    float fast=0.f,slow=0.f,rms2=0.f,noise=.00003f,peak=0.f;
    float hpLP=0.f,smoothedGain=0.f,eventStrength=0.f,ageMs=1.e7f;
    float previousFast=0.f,tail=1.f,gate=0.f;
    float roomRise=0.f,roomTail=1.f;
    bool armed=true;
    Shape last;
    void reset() noexcept {*this=Detector{};}
    Shape tick(float sample,const Settings& s,const Band& b,float fs,
               float aFast,float rFast,float aSlow,float rSlow,float aRms,
               float aGain,float hpCoef,float decayCoefficient,
               float noiseGateAmp,float roomRiseMul,float roomDecayMul) noexcept {
        hpLP+=hpCoef*(sample-hpLP);
        const float a=std::fabs(sample-hpLP);
        fast+=(a>fast?aFast:rFast)*(a-fast);
        slow+=(a>slow?aSlow:rSlow)*(a-slow);
        rms2+=aRms*(a*a-rms2);
        // adaptive noise floor: glides down slowly, up extremely slowly.
        const float nfRate=(a<noise)?.00025f:.000008f;
        noise+=nfRate*(a-noise);
        const float level=s.peakBlend*fast+(1.f-s.peakBlend)*std::sqrt(std::max(0.f,rms2));
        const float novelty=std::max(0.f,(level-slow)/(slow+std::max(noise*.14f,1.e-6f)));
        const float threshold=.12f+1.30f*(1.f-s.sensitivity);
        const float growth=std::max(0.f,fast-previousFast);
        previousFast=fast;
        const float elapsed=ageMs;
        const bool minimumGap=elapsed>=s.rearmMs;
        const bool eventStrong=level>std::max(noiseGateAmp,noise*2.f);
        const bool group=(s.grouping==2 && elapsed<s.groupWindowMs);
        const bool autoGroup=(s.grouping==1 && elapsed<s.groupWindowMs
                              && !(growth>peak*.007f && novelty>threshold*1.65f));
        bool trigger=minimumGap && !group && !autoGroup && eventStrong &&
                     novelty>threshold && (armed || growth>peak*.0035f);
        if(trigger){
            // Negative event age implements a pre-attack rise. At the delayed
            // audio onset, the envelope is at its maximum rather than decayed.
            ageMs=-s.lookaheadMs;
            peak=std::max(level,0.00003f);
            tail=1.f;roomRise=0.f;roomTail=1.f;
            eventStrength=clampf((novelty-threshold)*1.7f,.15f,1.f);
            armed=false;
        } else {
            peak=std::max(level,peak*.99975f);
            if(novelty<threshold*.45f)armed=true;
        }
        const float age=ageMs;
        const float atkLen=std::max(.35f,b.attackMs);
        const float atkP=clampf(age/atkLen,0.f,1.f);
        float attack=0.f;
        if(age<0.f && s.lookaheadMs>0.f){
            // Pre-transient rise is smooth and reaches unity at the true hit.
            const float u=clampf((age+s.lookaheadMs)/s.lookaheadMs,0.f,1.f);
            attack=(u*u*(3.f-2.f*u))*eventStrength;
        } else if(age<atkLen){
            const float x=1.f-atkP;
            const float exponent=clampf(b.attackCurve,.2f,4.f);
            attack=std::pow(x,exponent)*eventStrength;
        }
        const float bodyStart=atkLen*.40f;
        const float bp=(age-bodyStart)/std::max(5.f,b.bodyMs);
        float body=0.f;
        if(bp>0.f && bp<1.f){
            const float bump=4.f*bp*(1.f-bp);
            body=(bump*(1.f-s.edgeSoftness)+bump*bump*s.edgeSoftness)*eventStrength;
        }
        const float since=age-(atkLen+b.holdMs);
        float sustain=0.f,room=0.f;
        const float audibleGate=std::max(noiseGateAmp,noise*1.6f);
        const float gateTarget=clampf((level-audibleGate)/(audibleGate*1.5f+1.e-6f),0.f,1.f);
        gate+=onePoleCached*(gateTarget-gate);
        if(since>0.f && since<20000.f){
            tail*=decayCoefficient;
            const float tracked=clampf(level/std::max(peak*.25f,audibleGate*2.5f),0.f,1.f);
            const float shape=(1.f-b.follow)*tail+b.follow*tracked;
            sustain=(since/(since+8.f))*shape*gate*eventStrength;
        }
        if(s.roomAmount>0.f && age>s.roomEarlyMs){
            // Stateful exponential equivalent; no exp() inside the sample loop.
            roomRise=1.f-(1.f-roomRise)*roomRiseMul;
            roomTail*=roomDecayMul;
            room=roomRise*roomTail*gate*eventStrength;
        }
        last={attack,body,sustain,room};
        ageMs=std::min(1.e7f,ageMs+1000.f/fs);
        return last;
    }
    void setGateSmoothing(float x) noexcept {onePoleCached=x;}
private:
    float onePoleCached=.001f;
};
// Approximate inter-sample 4x peak estimate via precomputed windowed-sinc phases.
// This is not a certified ITU true-peak meter; output sample ceiling remains exact.
class PeakGuard {
public:
    void prepare(float fs) noexcept {
        rate=fs;
        for(int phase=0;phase<4;++phase){
            float sum=0;
            for(int k=0;k<17;++k){
                const float n=float(k-8)-(float)phase/4.f;
                const float sinc=(std::fabs(n)<1.e-6f)?1.f:std::sin(kPi*n)/(kPi*n);
                const float w=.5f+.5f*std::cos(kPi*(k-8)/8.f);
                interp[(size_t)phase][(size_t)k]=sinc*w;sum+=sinc*w;
            }
            for(auto& v:interp[(size_t)phase])v/=sum;
        }
        reset();
    }
    void reset() noexcept {for(auto& v:hist)v.fill(0.f);for(auto& v:del)v.fill(0.f);
        phasePos=0;delPos=0;smoothed=1.f;}
    void run(float& l,float& r,const Settings& s,int lookSamples,float rel,
             float ceiling) noexcept {
        if(!s.peakGuard)return;
        hist[0][(size_t)phasePos]=l;hist[1][(size_t)phasePos]=r;
        float peak=std::max(std::fabs(l),std::fabs(r));
        if(s.peakGuard4x){
            for(int ph=0;ph<4;++ph){
                float a=0.f,b=0.f;
                for(int i=0;i<17;++i){
                    const int n=(phasePos+17-i)%17;
                    a+=hist[0][(size_t)n]*interp[(size_t)ph][(size_t)i];
                    b+=hist[1][(size_t)n]*interp[(size_t)ph][(size_t)i];
                }
                peak=std::max(peak,std::max(std::fabs(a),std::fabs(b)));
            }
        }
        phasePos=(phasePos+1)%17;
        const float requested=peak>ceiling?ceiling/(peak+1.e-20f):1.f;
        if(requested<smoothed)smoothed=requested;
        else smoothed+=(requested-smoothed)*rel;
        // Delay is only active when guard is enabled.
        if(lookSamples>0) {
            del[0][(size_t)delPos]=l;del[1][(size_t)delPos]=r;
            const int read=(delPos+kDelaySize-lookSamples)%kDelaySize;
            l=del[0][(size_t)read];r=del[1][(size_t)read];
            delPos=(delPos+1)%kDelaySize;
        }
        float gain=smoothed;
        const float outPeak=std::max(std::fabs(l),std::fabs(r));
        if(outPeak*gain>ceiling)gain=ceiling/(outPeak+1.e-20f);
        l*=gain;r*=gain;
    }
private:
    float rate=44100.f,smoothed=1.f;
    int phasePos=0,delPos=0;
    std::array<std::array<float,17>,4> interp {};
    std::array<std::array<float,17>,2> hist {};
    std::array<std::array<float,kDelaySize>,2> del {};
};
class Engine final {
public:
    void prepare(float sampleRate,int) {
        fs=std::max(8000.f,sampleRate);
        for(int i=0;i<4097;++i) gainTable[(size_t)i]=dbGain(-36.f+i*(60.f/4096.f));
        fir.prepare(fs);spectral.prepare(fs);guard.prepare(fs);
        inputSlew=onePole(8.f,fs);gainSlew=onePole(1.5f,fs);
        autoRms=onePole(750.f,fs);autoSmooth=onePole(180.f,fs);
        guardRelease=onePole(70.f,fs);
        fastAtk=onePole(.13f,fs);fastRel=onePole(9.f,fs);
        slowAtk=onePole(16.f,fs);slowRel=onePole(160.f,fs);
        rmsCoef=onePole(11.f,fs);
        reset();
        configure(settings,true);
    }
    void reset() noexcept {
        for(auto& ch:iir)for(auto& bank:ch)for(auto& filt:bank)filt.reset();
        for(auto& band:detectors)for(auto& d:band)d.reset();
        for(auto& band:focus)for(auto& x:band)x.reset();
        for(auto& band:bandDelay)for(auto& c:band)c.fill(0.f);
        for(auto& x:dryDelay)x.fill(0.f);
        bandPos=0;wetSlew=1.f;inGain=outGain=1.f;
        rmsDry=rmsWet=0.f;autoGainDb=0.f;autoCounter=0;
        activity.fill(0.f);channelGain={{{1.f,1.f},{1.f,1.f},{1.f,1.f}}};
        shapeForScope={};fir.reset();spectral.reset();guard.reset();
    }
    void configure(Settings s,bool force=false) noexcept {
        s.bands=std::clamp(s.bands,1,3);
        s.stereo=std::clamp(s.stereo,0,2);
        s.crossover=std::clamp(s.crossover,0,1);
        s.link=clampf(s.link,0.f,1.f);
        s.lowHz=clampf(s.lowHz,60.f,900.f);
        s.highHz=clampf(s.highHz,std::max(950.f,s.lowHz*2.f),15000.f);
        s.mix=clampf(s.mix,0.f,1.f);
        s.lookaheadMs=clampf(s.lookaheadMs,0.f,3.f);
        s.guardLookaheadMs=clampf(s.guardLookaheadMs,0.f,3.f);
        s.smoothingMs=clampf(s.smoothingMs,.3f,15.f);
        s.roomAmount=clampf(s.roomAmount,0.f,2.f);
        s.ceilingDb=clampf(s.ceilingDb,-9.f,0.f);
        s.rearmMs=clampf(s.rearmMs,1.f,50.f);
        s.groupWindowMs=clampf(s.groupWindowMs,5.f,200.f);
        s.noiseGateDb=clampf(s.noiseGateDb,-100.f,-24.f);
        for(auto& b:s.band){
            b.attackMs=clampf(b.attackMs,.35f,90.f);
            b.attackCurve=clampf(b.attackCurve,.2f,4.f);
            b.bodyMs=clampf(b.bodyMs,5.f,500.f);
            b.holdMs=clampf(b.holdMs,0.f,500.f);
            b.decayMs=clampf(b.decayMs,40.f,4000.f);
            b.follow=clampf(b.follow,0.f,1.f);
            b.focusHz=clampf(b.focusHz,35.f,15000.f);
            b.focusQ=clampf(b.focusQ,.25f,8.f);
            b.focusMix=clampf(b.focusMix,0.f,1.f);
        }
        if(force||s.lowHz!=settings.lowHz||s.highHz!=settings.highHz) {
            for(int ch=0;ch<2;++ch)for(int stage=0;stage<2;++stage){
                iir[(size_t)ch][0][(size_t)stage].lowpass(s.lowHz,fs);
                iir[(size_t)ch][1][(size_t)stage].lowpass(s.highHz,fs);
            }
            fir.setFrequencies(s.lowHz,s.highHz);
        }
        if(force||s.sidechainHPF!=settings.sidechainHPF) {
            hpCoef=1.f-std::exp(-2*kPi*s.sidechainHPF/fs);
        }
        if(force||s.smoothingMs!=settings.smoothingMs) {
            gainSlew=onePole(s.smoothingMs,fs);
            for(auto& band:detectors)for(auto& d:band)d.setGateSmoothing(onePole(18.f,fs));
        }
        for(int i=0;i<3;++i) {
            const auto& b=s.band[(size_t)i];
            const auto& prev=settings.band[(size_t)i];
            if(force||b.focusHz!=prev.focusHz||b.focusQ!=prev.focusQ)
                for(auto& ch:focus[(size_t)i])ch.configure(fs,b.focusHz,b.focusQ);
            if(force||b.decayMs!=prev.decayMs)
                decay[(size_t)i]=std::exp(-1.f/(fs*b.decayMs*.001f));
        }
        const auto oldGrouping=settings.grouping;
        settings=s;
        // No FFT, allocation or parameter notifications from this method.
        if(force||s.inputDb!=previousInputDb){targetInput=dbGain(s.inputDb);previousInputDb=s.inputDb;}
        if(force||s.outputDb!=previousOutputDb){targetOutput=dbGain(s.outputDb);previousOutputDb=s.outputDb;}
        if(force||s.noiseGateDb!=previousFloorDb){noiseGateAmp=dbGain(s.noiseGateDb);previousFloorDb=s.noiseGateDb;}
        if(force||s.ceilingDb!=previousCeilingDb){guardCeiling=dbGain(s.ceilingDb);previousCeilingDb=s.ceilingDb;}
        if(force||s.roomBloomMs!=previousRoomBloom||s.roomDamp!=previousRoomDamp){
            roomRiseMul=std::exp(-1.f/(fs*std::max(4.f,s.roomBloomMs*.17f)*.001f));
            roomDecayMul=std::exp(-1.f/(fs*std::max(25.f,s.roomBloomMs*(1.f+3.f*s.roomDamp))*.001f));
            previousRoomBloom=s.roomBloomMs;previousRoomDamp=s.roomDamp;
        }
        lookSamples=(int)std::lround(fs*s.lookaheadMs*.001f);
        guardSamples=s.peakGuard?(int)std::lround(fs*s.guardLookaheadMs*.001f):0;
        firSamples=s.bands>1&&s.crossover==1?LinearPhaseSplit::latency:0;
        spectralSamples=s.spectral?512:0;
        latency=lookSamples+guardSamples+firSamples+spectralSamples;
        if(oldGrouping!=s.grouping)for(auto& band:detectors)for(auto& d:band)d.armed=true;
    }
    int latencySamples() const noexcept {return latency;}
    const std::array<float,3>& bandActivity() const noexcept {return activity;}
    Shape liveShape() const noexcept {return shapeForScope;}
    void process(float& left,float& right) noexcept {
        inGain+=(targetInput-inGain)*inputSlew;
        outGain+=(targetOutput-outGain)*inputSlew;
        wetSlew+=(settings.mix-wetSlew)*inputSlew;
        float l=left*inGain,r=right*inGain;
        if(settings.stereo==2){
            const float mid=(l+r)*.5f,side=(l-r)*.5f;
            l=mid;r=side;
        }
        std::array<std::array<float,2>,3> bands {};
        std::array<float,2> base{l,r};
        if(settings.bands==1)bands[1]=base;
        else if(settings.crossover==1)fir.process(l,r,settings.bands,bands,base);
        else splitIIR(l,r,bands);
        // Detector looks at current signal. Audio path is delayed for attack anticipation.
        const auto currentPos=bandPos;
        float wet[2]={0.f,0.f};
        const bool anySolo=settings.band[0].solo||settings.band[1].solo||settings.band[2].solo;
        for(int b=0;b<3;++b) {
            const bool active=(settings.bands==1?b==1:(settings.bands==2?b!=1:true));
            if(!active) {activity[(size_t)b]=0.f;continue;}
            const auto& p=settings.band[(size_t)b];
            float raw[2]={bands[(size_t)b][0],bands[(size_t)b][1]};
            float detect[2];
            for(int ch=0;ch<2;++ch){
                const float filtered=focus[(size_t)b][(size_t)ch].run(raw[ch]);
                detect[ch]=raw[ch]*(1.f-p.focusMix)+filtered*p.focusMix;
            }
            const float link=settings.stereo==1?0.f:settings.link;
            Shape shapes[2];
            if(link>=.999f){
                const float composite=std::fabs(detect[0])>std::fabs(detect[1])?detect[0]:detect[1];
                shapes[0]=shapes[1]=tickDetector(b,2,composite,p);
            } else {
                shapes[0]=tickDetector(b,0,detect[0],p);
                shapes[1]=tickDetector(b,1,detect[1],p);
                if(link>0.f){
                    const float composite=std::fabs(detect[0])>std::fabs(detect[1])?detect[0]:detect[1];
                    const Shape joint=tickDetector(b,2,composite,p);
                    for(auto& x:shapes){
                        x.attack+=(joint.attack-x.attack)*link;
                        x.body+=(joint.body-x.body)*link;
                        x.sustain+=(joint.sustain-x.sustain)*link;
                        x.room+=(joint.room-x.room)*link;
                    }
                }
            }
            if(b==1)shapeForScope=shapes[0];
            for(int ch=0;ch<2;++ch) {
                bandDelay[(size_t)b][(size_t)ch][(size_t)currentPos]=raw[ch];
                float audio=lookSamples>0?bandDelay[(size_t)b][(size_t)ch]
                    [(size_t)((currentPos+kDelaySize-lookSamples)%kDelaySize)]:raw[ch];
                const auto sh=shapes[ch];
                float amtAttack=1.f,amtSustain=1.f;
                if(settings.stereo==2){
                    amtAttack=ch==0?settings.midAttack:settings.sideAttack;
                    amtSustain=ch==0?settings.midSustain:settings.sideSustain;
                }
                const float db=(p.enabled?
                    p.attackDb*sh.attack*amtAttack+p.bodyDb*sh.body+
                    p.sustainDb*sh.sustain*amtSustain+
                    settings.roomAmount*12.f*sh.room*(ch==1?settings.roomWidth:1.f):0.f);
                const float target=fastGain(clampf(db,-36.f,24.f));
                auto& smoothed=channelGain[(size_t)b][(size_t)ch];
                smoothed+=(target-smoothed)*gainSlew;
                if(!anySolo||p.solo)wet[ch]+=audio*smoothed;
                activity[(size_t)b]=std::max(activity[(size_t)b]*.999f,std::fabs(db));
            }
        }
        for(int ch=0;ch<2;++ch)
            dryDelay[(size_t)ch][(size_t)currentPos]=base[(size_t)ch];
        float dry[2];
        for(int ch=0;ch<2;++ch)
            dry[ch]=lookSamples>0?dryDelay[(size_t)ch]
                [(size_t)((currentPos+kDelaySize-lookSamples)%kDelaySize)]:base[(size_t)ch];
        bandPos=(bandPos+1)%kDelaySize;
        if(settings.stereo==2){
            const float wetL=wet[0]+wet[1],wetR=wet[0]-wet[1];
            const float dryL=dry[0]+dry[1],dryR=dry[0]-dry[1];
            wet[0]=wetL;wet[1]=wetR;
            dry[0]=dryL;dry[1]=dryR;
        }
        float out[2];
        for(int ch=0;ch<2;++ch)
            out[ch]=settings.delta?wet[ch]-dry[ch]:dry[ch]+wetSlew*(wet[ch]-dry[ch]);
        if(settings.autoGain&&!settings.delta){
            const float dryEnergy=.5f*(dry[0]*dry[0]+dry[1]*dry[1]);
            const float wetEnergy=.5f*(out[0]*out[0]+out[1]*out[1]);
            rmsDry+=autoRms*(dryEnergy-rmsDry);
            rmsWet+=autoRms*(wetEnergy-rmsWet);
            if((++autoCounter&63)==0){
                autoTarget=clampf(10.f*std::log10((rmsDry+1.e-12f)/(rmsWet+1.e-12f)),-9.f,9.f);
            }
            autoGainDb+=(autoTarget-autoGainDb)*autoSmooth;
            const float g=fastGain(autoGainDb);
            out[0]*=g;out[1]*=g;
        } else {
            autoGainDb+=(0.f-autoGainDb)*autoSmooth;
        }
        out[0]*=outGain;out[1]*=outGain;
        if(settings.spectral) {
            float a=0,b=0;
            spectral.process(out[0],out[1],settings.spectralAttackDb,settings.spectralTonalDb,
                             settings.spectralSense,a,b);
            out[0]=a;out[1]=b;
        }
        if(settings.peakGuard)
            guard.run(out[0],out[1],settings,guardSamples,guardRelease,guardCeiling);
        left=std::isfinite(out[0])?out[0]:0.f;
        right=std::isfinite(out[1])?out[1]:0.f;
    }
private:
    Shape tickDetector(int b,int ch,float x,const Band& band) noexcept {
        return detectors[(size_t)b][(size_t)ch].tick(
            x,settings,band,fs,fastAtk,fastRel,slowAtk,slowRel,rmsCoef,
            gainSlew,hpCoef,decay[(size_t)b],
            noiseGateAmp,roomRiseMul,roomDecayMul);
    }
    float fastGain(float db) const noexcept {
        if(std::fabs(db)<1.e-7f)return 1.f;
        const float f=(clampf(db,-36.f,24.f)+36.f)*4096.f/60.f;
        const int i=std::clamp((int)f,0,4095);
        return gainTable[(size_t)i]+(gainTable[(size_t)(i+1)]-gainTable[(size_t)i])*(f-i);
    }
    void splitIIR(float l,float r,std::array<std::array<float,2>,3>& src) noexcept {
        for(int ch=0;ch<2;++ch) {
            float x=ch==0?l:r;
            float lo=iir[(size_t)ch][0][1].run(iir[(size_t)ch][0][0].run(x));
            if(settings.bands==2) {
                src[0][(size_t)ch]=lo;src[2][(size_t)ch]=x-lo;
            }else{
                float hi=iir[(size_t)ch][1][1].run(iir[(size_t)ch][1][0].run(x-lo));
                src[0][(size_t)ch]=lo;src[1][(size_t)ch]=hi;
                src[2][(size_t)ch]=x-lo-hi;
            }
        }
    }
    Settings settings;
    float fs=44100.f;
    float inputSlew=.001f,gainSlew=.01f,autoRms=.0001f,autoSmooth=.0001f;
    float guardRelease=.0001f,hpCoef=.01f;
    float noiseGateAmp=.0001f,guardCeiling=.944f,roomRiseMul=.99f,roomDecayMul=.999f;
    float previousFloorDb=999.f,previousCeilingDb=999.f;
    float previousRoomBloom=-1.f,previousRoomDamp=-1.f;
    float previousInputDb=999.f,previousOutputDb=999.f;
    float fastAtk=.1f,fastRel=.01f,slowAtk=.003f,slowRel=.0001f,rmsCoef=.003f;
    float inGain=1.f,outGain=1.f,wetSlew=1.f,targetInput=1.f,targetOutput=1.f;
    float rmsDry=0.f,rmsWet=0.f,autoGainDb=0.f,autoTarget=0.f;
    int autoCounter=0,lookSamples=0,guardSamples=0,latency=0,firSamples=0,spectralSamples=0;
    int bandPos=0;
    std::array<float,4097> gainTable {};
    std::array<float,3> decay {};
    std::array<float,3> activity {};
    Shape shapeForScope;
    std::array<std::array<Detector,3>,3> detectors {};
    std::array<std::array<FocusFilter,2>,3> focus {};
    std::array<std::array<std::array<tdp::Biquad,2>,2>,2> iir {};
    std::array<std::array<std::array<float,kDelaySize>,2>,3> bandDelay {};
    std::array<std::array<float,kDelaySize>,2> dryDelay {};
    std::array<std::array<float,2>,3> channelGain {{{1.f,1.f},{1.f,1.f},{1.f,1.f}}};
    LinearPhaseSplit fir;
    SpectralSeparation spectral;
    PeakGuard guard;
};
} // namespace tdp2
