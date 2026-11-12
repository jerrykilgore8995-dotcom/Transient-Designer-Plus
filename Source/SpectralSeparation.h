#pragma once
// Spectral transient-vs-tonal balancing: genuine STFT, 512 samples / 128 hop.
// No dynamic allocation, mutex, or FFT planning on the audio thread.
// Both channels share one frequency-dependent magnitude mask to preserve imaging.
#include <array>
#include <algorithm>
#include <cmath>
#include <complex>

namespace tdp2 {
class SpectralSeparation final {
    static constexpr int N=512, Hop=128, Ring=2048;
    using Complex=std::complex<float>;
public:
    void reset() noexcept {
        for(auto& a:input) a.fill(0.f);
        for(auto& a:ola) a.fill(0.f);
        previous.fill(0.f); steady.fill(0.f);
        inputHead=outputHead=0; clock=0;
        for(auto& a:work) a.fill(Complex{});
    }
    void prepare(float) noexcept {
        for(int i=0;i<N;++i) window[(size_t)i]
            =0.5f*(1.f-std::cos(6.283185307179586f*i/N));
        reset();
    }
    // Return 512-sample delayed pair. Caller reports PDC when enabled.
    void process(float left,float right,float transientDb,float tonalDb,
                 float sensitivity,float& outL,float& outR) noexcept {
        input[0][(size_t)inputHead]=left;
        input[1][(size_t)inputHead]=right;
        // OLA samples from older frames are read before writing this frame.
        outL=ola[0][(size_t)outputHead];
        outR=ola[1][(size_t)outputHead];
        ola[0][(size_t)outputHead]=0.f;
        ola[1][(size_t)outputHead]=0.f;
        ++clock;
        if(clock>=N && ((clock-N)%Hop)==0) {
            analyseAndResynthesise(transientDb,tonalDb,sensitivity);
        }
        inputHead=(inputHead+1)%N;
        outputHead=(outputHead+1)%Ring;
    }
private:
    static void fft(std::array<Complex,N>& a,bool inverse) noexcept {
        for(int i=1,j=0;i<N;++i) {
            int b=N>>1;
            for(;j&b;b>>=1)j^=b;
            j^=b;
            if(i<j)std::swap(a[(size_t)i],a[(size_t)j]);
        }
        for(int len=2;len<=N;len*=2) {
            const float angle=(inverse?2.f:-2.f)*3.14159265358979323846f/len;
            const Complex rotor(std::cos(angle),std::sin(angle));
            for(int start=0;start<N;start+=len) {
                Complex phase(1.f,0.f);
                for(int k=0;k<len/2;++k) {
                    const auto u=a[(size_t)(start+k)];
                    const auto v=a[(size_t)(start+k+len/2)]*phase;
                    a[(size_t)(start+k)]=u+v;
                    a[(size_t)(start+k+len/2)]=u-v;
                    phase*=rotor;
                }
            }
        }
        if(inverse)for(auto& z:a)z/=float(N);
    }
    void analyseAndResynthesise(float attackDb,float tonalDb,float sense) noexcept {
        // inputHead still holds the latest sample: earliest is inputHead+1.
        const int first=(inputHead+1)%N;
        for(int ch=0;ch<2;++ch) {
            for(int j=0;j<N;++j) {
                float v=input[(size_t)ch][(size_t)((first+j)%N)]*window[(size_t)j];
                work[(size_t)ch][(size_t)j]=Complex(v,0);
            }
            fft(work[(size_t)ch],false);
        }
        for(int bin=0;bin<=N/2;++bin) {
            const float mag=std::max(std::abs(work[0][(size_t)bin]),
                                     std::abs(work[1][(size_t)bin]));
            const float novelty=std::max(0.f,mag-previous[(size_t)bin]*.94f);
            const float baseline=steady[(size_t)bin];
            const float flux=novelty/(novelty+baseline*(.20f+(.85f*(1.f-sense)))+1.e-7f);
            const float prob=std::clamp(flux,0.f,1.f);
            const float gain=std::pow(10.f,(attackDb*prob+tonalDb*(1.f-prob))*.05f);
            previous[(size_t)bin]=mag;
            steady[(size_t)bin]=baseline*.91f+mag*.09f;
            for(int ch=0;ch<2;++ch) {
                work[(size_t)ch][(size_t)bin]*=gain;
                if(bin>0 && bin<N/2)work[(size_t)ch][(size_t)(N-bin)]*=gain;
            }
        }
        // Periodic Hann, four overlaps, sum Hann^2 = 1.5 at steady state.
        for(int ch=0;ch<2;++ch) {
            fft(work[(size_t)ch],true);
            for(int j=0;j<N;++j) {
                const int target=(outputHead+j+1)%Ring;
                ola[(size_t)ch][(size_t)target]
                    +=work[(size_t)ch][(size_t)j].real()*window[(size_t)j]*(1.f/1.5f);
            }
        }
    }
    std::array<std::array<float,N>,2> input {};
    std::array<std::array<float,Ring>,2> ola {};
    std::array<std::array<Complex,N>,2> work {};
    std::array<float,N> window {};
    std::array<float,N/2+1> previous {},steady {};
    int inputHead=0,outputHead=0;
    int clock=0;
};
} // namespace tdp2
