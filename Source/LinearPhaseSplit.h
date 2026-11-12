#pragma once
// Optional 257-tap linear-phase FIR frequency split.
// Frequency kernel bank precomputed once in prepare(), never sin/cos on audio thread.
// Group delay = 128 samples; all bands share the same delay.
#include <array>
#include <algorithm>
#include <cmath>

namespace tdp2 {
class LinearPhaseSplit final {
    static constexpr int Taps=257, Bank=48, Delay=(Taps-1)/2;
public:
    static constexpr int latency=Delay;
    void prepare(float fs) {
        rate=std::max(8000.f,fs);
        for(int b=0;b<Bank;++b) {
            const float t=(float)b/(Bank-1);
            makeKernel(bankLow[(size_t)b],60.f*std::pow(900.f/60.f,t));
            makeKernel(bankHigh[(size_t)b],950.f*std::pow(15000.f/950.f,t));
        }
        lastLo=lastHi=-1.f;
        setFrequencies(180,3200);
        reset();
    }
    void reset() noexcept {
        for(auto& x:hist)x.fill(0.f);
        pos=0;
    }
    void setFrequencies(float low,float high) noexcept {
        low=std::clamp(low,60.f,900.f);
        high=std::clamp(high,950.f,15000.f);
        if(low!=lastLo) {
            chooseKernel(bankLow,low,60.f,900.f,lowKernel);
            lastLo=low;
        }
        if(high!=lastHi) {
            chooseKernel(bankHigh,high,950.f,15000.f,highKernel);
            lastHi=high;
        }
    }
    // One-step delay-aligned additive decomposition.
    void process(float inL,float inR,int bands,
                 std::array<std::array<float,2>,3>& out,
                 std::array<float,2>& delayedDry) noexcept {
        hist[0][(size_t)pos]=inL;
        hist[1][(size_t)pos]=inR;
        for(int ch=0;ch<2;++ch) {
            const float delayed=hist[(size_t)ch][(size_t)((pos+Taps-Delay)%Taps)];
            delayedDry[(size_t)ch]=delayed;
            float low=0.f,belowHigh=0.f;
            for(int k=0;k<Taps;++k) {
                const float a=hist[(size_t)ch][(size_t)((pos+Taps-k)%Taps)];
                low+=a*lowKernel[(size_t)k];
                if(bands==3)belowHigh+=a*highKernel[(size_t)k];
            }
            out[0][(size_t)ch]=low;
            if(bands==3) {
                out[1][(size_t)ch]=belowHigh-low;
                out[2][(size_t)ch]=delayed-belowHigh;
            } else {
                out[1][(size_t)ch]=0.f;
                out[2][(size_t)ch]=delayed-low;
            }
        }
        pos=(pos+1)%Taps;
    }
private:
    void makeKernel(std::array<float,Taps>& h,float freq) const {
        const float fc=std::clamp(freq/rate,.0001f,.449f);
        float sum=0.f;
        for(int i=0;i<Taps;++i) {
            const int n=i-Delay;
            const float sinc=n==0?2.f*fc:
                std::sin(2.f*3.14159265358979323846f*fc*n)
                      /(3.14159265358979323846f*n);
            const float a=2.f*3.14159265358979323846f*i/(Taps-1);
            const float w=.42f-.50f*std::cos(a)+.08f*std::cos(2.f*a);
            h[(size_t)i]=sinc*w;
            sum+=h[(size_t)i];
        }
        for(auto& v:h)v/=sum;
    }
    static void chooseKernel(const std::array<std::array<float,Taps>,Bank>& bank,
                             float hz,float first,float last,
                             std::array<float,Taps>& result) noexcept {
        const float index=(Bank-1)*std::log(hz/first)/std::log(last/first);
        const float clamped=std::clamp(index,0.f,float(Bank-1));
        const int a=(int)clamped;
        const int b=std::min(a+1,Bank-1);
        const float t=clamped-a;
        for(int k=0;k<Taps;++k)
            result[(size_t)k]=bank[(size_t)a][(size_t)k]*(1.f-t)
                             +bank[(size_t)b][(size_t)k]*t;
    }
    float rate=44100.f,lastLo=-1,lastHi=-1;
    int pos=0;
    std::array<std::array<float,Taps>,Bank> bankLow {},bankHigh {};
    std::array<float,Taps> lowKernel {},highKernel {};
    std::array<std::array<float,Taps>,2> hist {};
};
}
