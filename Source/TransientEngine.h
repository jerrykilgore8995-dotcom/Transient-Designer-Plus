#pragma once
// Transient Designer Plus: independent, allocation-free audio engine.
// Inert default: neither distortion nor deliberate frequency coloration.
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace tdp {
constexpr float pi = 3.14159265358979323846f;
inline float clampf(float x,float lo,float hi) noexcept { return std::max(lo,std::min(hi,x)); }
inline float dbGain(float d) noexcept { return std::pow(10.0f,d*0.05f); }
inline float msSamples(float m,float sr) noexcept { return m*0.001f*sr; }
inline float onePole(float ms,float sr) noexcept {
    return 1.0f-std::exp(-1.0f/std::max(1.0f,msSamples(ms,sr)));
}
struct BandSettings {
    float attackDb=0.f, attackMs=18.f, attackCurve=1.0f;
    float bodyDb=0.f, bodyMs=100.f;
    float sustainDb=0.f, sustainHoldMs=60.f, sustainDecayMs=700.f;
    bool enabled=true, solo=false;
};
struct Settings {
    int bands=1;               // 1=FULL, 2=DUAL, 3=TRI
    int stereo=0;              // 0=LINK, 1=DUAL MONO, 2=M/S
    float link=1.f, lowHz=180.f, highHz=3200.f;
    float inputDb=0.f, outputDb=0.f, wet=1.f;
    float sensitivity=0.60f, peakBlend=0.75f, sidechainHPF=20.f;
    float edgeSoftness=0.35f;
    bool delta=false, autoGain=false, peakGuard=false;
    int lookahead=0;           // zero or 1/3ms ONLY with Peak Guard
    float ceilingDb=-0.5f;
    std::array<BandSettings,3> band {};
};
struct Biquad {
    float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    void lowpass(float hz,float fs) noexcept {
        hz=clampf(hz,12.f,fs*0.44f);
        float w=2*pi*hz/fs, c=std::cos(w), sn=std::sin(w);
        float a=1.f+sn/(2.f*0.70710678f);
        b0=(1.f-c)*.5f/a; b1=(1.f-c)/a; b2=b0;
        a1=-2.f*c/a; a2=(1.f-sn/(2.f*0.70710678f))/a;
    }
    float run(float x) noexcept {
        float y=b0*x+z1;
        z1=b1*x-a1*y+z2;
        z2=b2*x-a2*y;
        return y;
    }
    void reset() noexcept { z1=z2=0; }
};
struct DetectorCoefficients {
    float hpfc=.01f,fastAtk=.1f,fastRel=.01f,slowAtk=.001f,slowRel=.0001f;
    float rms=.003f,gainSmooth=.03f,decayMul=.999f,msPerSample=.0227f;
};
struct Detector {
    float fast=0,slow=0,rms2=0,hpfLP=0;
    float age=1.0e9f,eventStrength=0,gainDb=0,tail=1.f;
    bool armed=true;
    // Only multiplication, a square root and occasional curve shaping in sample loop.
    float tick(float x,const BandSettings& b,const Settings& p,
               const DetectorCoefficients& c,float sr) noexcept {
        hpfLP+=c.hpfc*(x-hpfLP);
        const float det=x-hpfLP;
        const float a=std::fabs(det);
        fast+=(a>fast?c.fastAtk:c.fastRel)*(a-fast);
        slow+=(a>slow?c.slowAtk:c.slowRel)*(a-slow);
        rms2+=c.rms*(det*det-rms2);
        float measured=p.peakBlend*fast+(1.f-p.peakBlend)*std::sqrt(std::max(0.f,rms2));
        const float onset=std::max(0.f,(measured-slow)/(slow+0.00002f));
        const float threshold=.18f+(1.f-p.sensitivity)*1.18f;
        const float refractory=msSamples(std::max(8.f,b.attackMs*.40f),sr);
        if(age>refractory && fast>.000035f && armed && onset>threshold) {
            age=0.f;
            tail=1.f;
            eventStrength=clampf((onset-threshold)/(.30f+p.edgeSoftness*.8f),.20f,1.f);
            armed=false;
        }
        if(onset<threshold*.55f) armed=true;
        const float tMs=age*c.msPerSample;
        const float aEnd=b.attackMs;
        const float aP=clampf(tMs/aEnd,0.f,1.f);
        float attackWindow=0.f;
        if(tMs<aEnd) {
            const float v=1.f-aP;
            if(b.attackCurve<1.f) {
                const float blend=clampf((b.attackCurve-.20f)/.8f,0.f,1.f);
                attackWindow=std::sqrt(std::sqrt(v))*(1.f-blend)+v*blend;
            } else {
                const float squared=v*v;
                const float quartic=squared*squared;
                const float blend=clampf((b.attackCurve-1.f)/3.f,0.f,1.f);
                attackWindow=v*(1.f-blend)+quartic*blend;
            }
        }
        const float bodyStart=aEnd*.42f;
        const float bodyP=(tMs-bodyStart)/b.bodyMs;
        float bodyWindow=0.f;
        if(bodyP>0.f && bodyP<1.f) {
            const float bump=4.f*bodyP*(1.f-bodyP);
            bodyWindow=bump*(1.f-p.edgeSoftness)+bump*bump*p.edgeSoftness;
        }
        const float sAge=tMs-(aEnd+b.sustainHoldMs);
        float sustainWindow=0.f;
        if(sAge>0.f && sAge<25000.f) {
            tail*=c.decayMul;
            sustainWindow=tail*(sAge/(sAge+6.f));
        }
        const float wanted=b.enabled?
            clampf((b.attackDb*attackWindow+b.bodyDb*bodyWindow+
                    b.sustainDb*sustainWindow)*eventStrength,-30.f,24.f):0.f;
        gainDb+=(wanted-gainDb)*c.gainSmooth;
        if(age<1.0e9f)age+=1.f;
        if(fast<1.0e-22f)fast=0.f;
        if(slow<1.0e-22f)slow=0.f;
        if(rms2<1.0e-36f)rms2=0.f;
        return gainDb;
    }
};
class Engine {
public:
    void prepare(float sampleRate,int maxBlock) {
        (void)maxBlock;
        fs=std::max(8000.f,sampleRate);
        smoothA=onePole(8.f,fs);
        rmsMatchA=onePole(750.f,fs);
        autoLevelA=onePole(200.f,fs);
        guardReleaseA=onePole(90.f,fs);
        guardOffA=onePole(60.f,fs);
        for(size_t i=0;i<gainTable.size();++i)
            gainTable[i]=dbGain(-30.f+(54.f*(float)i/(float)(gainTable.size()-1)));
        delay.assign((size_t)(fs*.004f)+128, {0.f,0.f});
        reset();
        filtersReady=false; // sample-rate changes must recalculate crossover coefficients
        setSettings(settings);
    }
    void reset() {
        for (auto& arr:filters) for (auto& ch:arr) for(auto& f:ch)f.reset();
        for (auto& band:detectors) for(auto& det:band)det=Detector{};
        for (auto& x:delay)x={0.f,0.f};
        delayPos=0; inputSmooth=outputSmooth=1.f; wetSmooth=1.f;
        guardGain=1.f; dry2=wet2=0.f; autoDb=0.f;
        activity={0.f,0.f,0.f};
    }
    void setSettings(Settings s) {
        s.bands=std::max(1,std::min(3,s.bands));
        s.stereo=std::max(0,std::min(2,s.stereo));
        s.lowHz=clampf(s.lowHz,60.f,900.f);
        s.highHz=clampf(s.highHz,std::max(950.f,s.lowHz*2.f),15000.f);
        s.link=clampf(s.link,0.f,1.f);
        s.peakBlend=clampf(s.peakBlend,0.f,1.f);
        s.sensitivity=clampf(s.sensitivity,0.f,1.f);
        s.sidechainHPF=clampf(s.sidechainHPF,20.f,250.f);
        s.edgeSoftness=clampf(s.edgeSoftness,0.f,1.f);
        s.lookahead=s.lookahead==1?1:(s.lookahead==3?3:0);
        for(auto& b:s.band) {
            b.attackMs=clampf(b.attackMs,.35f,90.f);
            b.attackCurve=clampf(b.attackCurve,.2f,4.f);
            b.bodyMs=clampf(b.bodyMs,5.f,500.f);
            b.sustainHoldMs=clampf(b.sustainHoldMs,0.f,500.f);
            b.sustainDecayMs=clampf(b.sustainDecayMs,40.f,3000.f);
        }
        if (s.lowHz!=settings.lowHz || s.highHz!=settings.highHz || !filtersReady) {
            for(int c=0;c<2;++c) for(int stage=0;stage<2;++stage) {
                filters[0][c][stage].lowpass(s.lowHz,fs);
                filters[1][c][stage].lowpass(s.highHz,fs);
            }
            filtersReady=true;
        }
        settings=s;
        inputTarget=dbGain(settings.inputDb);
        outputTarget=dbGain(settings.outputDb);
        guardCeiling=dbGain(clampf(settings.ceilingDb,-9.f,0.f));
        for(int i=0;i<3;++i){
            auto& c=detCoef[(size_t)i];
            c.hpfc=1.f-std::exp(-2*pi*s.sidechainHPF/fs);
            c.fastAtk=onePole(.22f,fs);c.fastRel=onePole(14.f,fs);
            c.slowAtk=onePole(14.f,fs);c.slowRel=onePole(95.f,fs);
            c.rms=onePole(9.f,fs);c.gainSmooth=onePole(.65f,fs);
            c.msPerSample=1000.f/fs;
            c.decayMul=std::exp(-1.f/msSamples(s.band[(size_t)i].sustainDecayMs,fs));
        }
        latency=s.peakGuard?(int)std::lround(fs*.001f*s.lookahead):0;
    }
    int latencySamples() const noexcept { return latency; }
    const std::array<float,3>& activityDb() const noexcept { return activity; }
    void process(float& left,float& right) noexcept {
        inputSmooth+=(inputTarget-inputSmooth)*smoothA;
        outputSmooth+=(outputTarget-outputSmooth)*smoothA;
        wetSmooth+=(clampf(settings.wet,0.f,1.f)-wetSmooth)*smoothA;
        float l=left*inputSmooth,r=right*inputSmooth;
        const float dryL=l,dryR=r;
        if(settings.stereo==2) {float m=(l+r)*.5f,sd=(l-r)*.5f;l=m;r=sd;}
        std::array<std::array<float,2>,3> src {};
        if(settings.bands==1) {src[1]={l,r};}
        else {
            for(int c=0;c<2;++c) {
                float x=c==0?l:r;
                float low=filters[0][c][1].run(filters[0][c][0].run(x));
                float rest=x-low;
                if(settings.bands==2) { src[0][c]=low; src[2][c]=rest; }
                else {
                    float mid=filters[1][c][1].run(filters[1][c][0].run(rest));
                    src[0][c]=low; src[1][c]=mid; src[2][c]=rest-mid;
                }
            }
        }
        bool anySolo=false;
        for(int b=0;b<3;++b) anySolo=anySolo||settings.band[b].solo;
        float out[2]={0.f,0.f};
        const int first=settings.bands==1?1:0;
        for(int b=first;b<3;++b) {
            if(settings.bands==2 && b==1)continue;
            const auto& bs=settings.band[b];
            const float link=settings.stereo==1?0.f:settings.link;
            float gl=0.f,gr=0.f;
            // 100%-linked mode: only one detector for both channels, not three.
            // This greatly reduces CPU load at the default stereo setting.
            if(link>=.999f) {
                gl=gr=detectors[b][2].tick(
                    std::max(std::fabs(src[b][0]),std::fabs(src[b][1])),
                    bs,settings,detCoef[(size_t)b],fs);
            } else {
                gl=detectors[b][0].tick(src[b][0],bs,settings,detCoef[(size_t)b],fs);
                gr=detectors[b][1].tick(src[b][1],bs,settings,detCoef[(size_t)b],fs);
                if(link>0.f) {
                    const float linked=detectors[b][2].tick(
                        std::max(std::fabs(src[b][0]),std::fabs(src[b][1])),
                        bs,settings,detCoef[(size_t)b],fs);
                    gl+=(linked-gl)*link;gr+=(linked-gr)*link;
                }
            }
            if(!anySolo || bs.solo) {
                const float lg=fastGain(gl);
                out[0]+=src[b][0]*lg;
                out[1]+=src[b][1]*(link>=.999f?lg:fastGain(gr));
            }
            activity[b]=std::max(std::fabs(gl),std::fabs(gr));
        }
        if(settings.stereo==2) { l=out[0]+out[1];r=out[0]-out[1];}
        else {l=out[0];r=out[1];}
        float wetL=l, wetR=r;
        if(settings.delta) {l=(wetL-dryL);r=(wetR-dryR);}
        else {l=dryL+(wetL-dryL)*wetSmooth;r=dryR+(wetR-dryR)*wetSmooth;}
        // Slow RMS comparison is optional; never active in the unity/null path.
        if(settings.autoGain && !settings.delta) {
            float alpha=rmsMatchA;
            dry2+=(.5f*(dryL*dryL+dryR*dryR)-dry2)*alpha;
            wet2+=(.5f*(l*l+r*r)-wet2)*alpha;
            float desired=clampf(10.f*std::log10((dry2+1.e-12f)/(wet2+1.e-12f)),-9.f,9.f);
            autoDb+=(desired-autoDb)*autoLevelA;
            float g=dbGain(autoDb);l*=g;r*=g;
        } else autoDb+=(0.f-autoDb)*autoLevelA;
        l*=outputSmooth;r*=outputSmooth;
        if(settings.peakGuard) {
            float ceiling=guardCeiling;
            float peak=std::max(std::fabs(l),std::fabs(r));
            float request=peak>ceiling?ceiling/(peak+1.e-20f):1.f;
            if(request<guardGain) guardGain=request;
            else guardGain+=(request-guardGain)*guardReleaseA;
        } else guardGain+=(1.f-guardGain)*guardOffA;
        if(latency>0) {
            const size_t len=delay.size();
            delay[delayPos]={l,r};
            size_t read=(delayPos+len-(size_t)latency)%len;
            l=delay[read][0];r=delay[read][1];
            delayPos=(delayPos+1)%len;
        }
        if(settings.peakGuard) {
            const float ceiling=guardCeiling;
            float g=guardGain;
            float peak=std::max(std::fabs(l),std::fabs(r));
            if(peak*g>ceiling)g=ceiling/(peak+1.e-20f);
            l*=g;r*=g;
        }
        // Everything with optional harmonic consequences defaults OFF.
        left=std::isfinite(l)?l:0.f;
        right=std::isfinite(r)?r:0.f;
    }
private:
    float fastGain(float d) const noexcept {
        if(std::fabs(d)<1.e-7f)return 1.f;
        const float x=clampf(d,-30.f,24.f)* (4096.f/54.f) + 30.f*(4096.f/54.f);
        const int i=std::min(4095,std::max(0,(int)x));
        return gainTable[(size_t)i]+(gainTable[(size_t)i+1]-gainTable[(size_t)i])*(x-i);
    }
    Settings settings;
    float fs=44100.f,smoothA=.001f;
    float rmsMatchA=.00001f,autoLevelA=.0001f,guardReleaseA=.0001f,guardOffA=.0001f;
    float inputTarget=1.f,outputTarget=1.f,guardCeiling=.944f;
    std::array<float,4097> gainTable {};
    std::array<DetectorCoefficients,3> detCoef {};
    bool filtersReady=false;
    // Two complementary crossover splits, each 4th-order lowpass.
    std::array<std::array<std::array<Biquad,2>,2>,2> filters {};
    std::array<std::array<Detector,3>,3> detectors {};
    std::vector<std::array<float,2>> delay;
    size_t delayPos=0; int latency=0;
    float inputSmooth=1.f,outputSmooth=1.f,wetSmooth=1.f,guardGain=1.f;
    float dry2=0.f,wet2=0.f,autoDb=0.f;
    std::array<float,3> activity {};
};
} // namespace tdp
