#pragma once
// LTA POCKET: dependency-free, realtime-safe core for playback approximations.
// All filter coefficients and delay storage are created in prepare(), never in render().
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace lta {
constexpr float pi = 3.14159265358979323846f;
inline float clampf(float v, float lo, float hi) noexcept { return std::max(lo, std::min(v, hi)); }
inline float dbToGain(float db) noexcept { return std::pow(10.0f, db / 20.0f); }

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set(float B0,float B1,float B2,float A0,float A1,float A2) noexcept {
        const float inv = 1.0f / std::max(1.0e-12f, A0);
        b0=B0*inv; b1=B1*inv; b2=B2*inv; a1=A1*inv; a2=A2*inv;
    }
    void lowpass(float sr,float f,float q=0.7071f) noexcept {
        f=clampf(f,12.0f,sr*0.46f); q=std::max(0.1f,q);
        float w=2*pi*f/sr, c=std::cos(w), a=std::sin(w)/(2*q);
        set((1-c)*.5f,1-c,(1-c)*.5f,1+a,-2*c,1-a);
    }
    void highpass(float sr,float f,float q=0.7071f) noexcept {
        f=clampf(f,12.0f,sr*0.46f); q=std::max(0.1f,q);
        float w=2*pi*f/sr, c=std::cos(w), a=std::sin(w)/(2*q);
        set((1+c)*.5f,-1-c,(1+c)*.5f,1+a,-2*c,1-a);
    }
    void peak(float sr,float f,float q,float gainDB) noexcept {
        f=clampf(f,12.0f,sr*0.46f); q=std::max(0.1f,q);
        float A=std::pow(10.f,gainDB/40.f), w=2*pi*f/sr, c=std::cos(w), a=std::sin(w)/(2*q);
        set(1+a*A,-2*c,1-a*A,1+a/A,-2*c,1-a/A);
    }
    float tick(float x) noexcept {
        const float y=b0*x+z1;
        z1=b1*x-a1*y+z2; z2=b2*x-a2*y;
        // Avoid infinitesimal denormal state in long silent sections.
        if (std::abs(z1)<1.e-20f) z1=0;
        if (std::abs(z2)<1.e-20f) z2=0;
        return y;
    }
    void reset() noexcept { z1=z2=0; }
};

struct DeviceModel {
    std::array<std::array<Biquad,4>,2> filters;
    float drive = 1.0f;
    float stereoBlend = 0.0f;
    void setup(float sr,float hp,float lp,float center1,float dB1,float q1,
               float center2,float dB2,float q2,float distortion,float stereoReduction) noexcept {
        drive=std::max(1.f,distortion); stereoBlend=clampf(stereoReduction,0,1);
        for (auto& ch: filters) {
            ch[0].highpass(sr,hp); ch[1].lowpass(sr,lp);
            ch[2].peak(sr,center1,q1,dB1); ch[3].peak(sr,center2,q2,dB2);
        }
    }
    void run(float inL,float inR,float& outL,float& outR) noexcept {
        const float mid=.5f*(inL+inR);
        outL=inL*(1-stereoBlend)+mid*stereoBlend;
        outR=inR*(1-stereoBlend)+mid*stereoBlend;
        for(auto& f: filters[0]) outL=f.tick(outL);
        for(auto& f: filters[1]) outR=f.tick(outR);
        if(drive>1.001f){
            const float inv=1.0f/std::tanh(drive);
            outL=std::tanh(outL*drive)*inv;
            outR=std::tanh(outR*drive)*inv;
        }
    }
    void reset() noexcept { for(auto& ch:filters) for(auto& f:ch) f.reset(); }
};

// Device indexes: 0..2 phone (modern/compact/vintage); 3 laptop; 4 bluetooth;
// 5 earbuds; 6 car; 7 television. These are illustrative generic signatures.
struct Controls {
    int device=0, phoneModel=0, environment=0;
    bool mono=false, quiet=false, original=false;
    float mix=1.f;
};

class PocketDSP {
public:
    void prepare(float sampleRate) {
        sr=std::max(22050.f,sampleRate);
        // Frequencies and peaks are intentionally approximate, not brand-specific measurements.
        models[0].setup(sr,245,9400,1550,2.5f,.9f,4400,-1.5f,1.1f,1.35f,.50f);
        models[1].setup(sr,385,7200,2300,4.0f,1.0f,4300,-2.2f,1.2f,1.60f,.78f);
        models[2].setup(sr,520,5600,1250,4.7f,1.3f,3800,-2.5f,1.1f,1.75f,.95f);
        models[3].setup(sr,170,14300,950,1.8f,.85f,4300,1.4f,.85f,1.11f,.18f);
        models[4].setup(sr,88,15600,120,2.3f,1.0f,2700,1.4f,.9f,1.14f,.24f);
        models[5].setup(sr,42,17400,120,2.4f,.75f,5200,-1.5f,.9f,1.02f,0.f);
        models[6].setup(sr,45,13300,85,3.2f,.8f,360, -2.5f,1.0f,1.09f,.25f);
        models[7].setup(sr,135,12700,275,-2.0f,.9f,3300,3.0f,1.0f,1.16f,.30f);
        const int delaySize=std::max(32,(int)(sr*.18f));
        roomL.assign((size_t)delaySize,0.f);
        roomR.assign((size_t)delaySize,0.f);
        roomPos=0; noiseSlow=0; noiseFast=0; noiseState=0x6f8c9d31u;
        dryRms=wetRms=1.e-6f; levelGain=1;
        monoSmooth=0; quietSmooth=1; mixSmooth=1; originalSmooth=0;
        environmentSmooth.fill(0); environmentSmooth[0]=1;
        deviceSmooth.fill(0); deviceSmooth[0]=1;
        for(auto& m:models) m.reset();
    }

    void reset() noexcept {
        for(auto& m:models) m.reset();
        std::fill(roomL.begin(),roomL.end(),0.f);
        std::fill(roomR.begin(),roomR.end(),0.f);
        dryRms=wetRms=1.e-6f; levelGain=1; roomPos=0;
    }

    void process(float& l,float& r,const Controls& c) noexcept {
        const float inL=l, inR=r;
        int device=clampi(c.device,0,5);
        int model=device==0?clampi(c.phoneModel,0,2):(device+2);
        const float tune=1.f-std::exp(-1.f/(.045f*sr));
        for(int i=0;i<8;++i) deviceSmooth[(size_t)i]+=( (i==model?1.f:0.f)-deviceSmooth[(size_t)i])*tune;
        for(int i=0;i<4;++i) environmentSmooth[(size_t)i]+=( (i==clampi(c.environment,0,3)?1.f:0.f)-environmentSmooth[(size_t)i])*tune;
        monoSmooth+=((c.mono?1.f:0.f)-monoSmooth)*tune;
        quietSmooth+=( (c.quiet?dbToGain(-16.f):1.f)-quietSmooth)*tune;
        mixSmooth+=(clampf(c.mix,0.f,1.f)-mixSmooth)*tune;
        originalSmooth+=((c.original?1.f:0.f)-originalSmooth)*tune;

        // All models remain warm, so rapidly switching device profiles doesn't restart filters.
        float outL=0, outR=0;
        for(int i=0;i<8;++i) {
            float dl=0,dr=0; models[(size_t)i].run(inL,inR,dl,dr);
            outL+=deviceSmooth[(size_t)i]*dl;
            outR+=deviceSmooth[(size_t)i]*dr;
        }
        // RMS-match original vs simulated signal, deliberately limited to ±7 dB.
        const float envelope=std::exp(-1.f/(.30f*sr));
        const float dryPower=.5f*(inL*inL+inR*inR);
        const float wetPower=.5f*(outL*outL+outR*outR);
        dryRms=envelope*dryRms+(1-envelope)*dryPower;
        wetRms=envelope*wetRms+(1-envelope)*wetPower;
        const float match=clampf(std::sqrt((dryRms+1.e-8f)/(wetRms+1.e-8f)),dbToGain(-7.f),dbToGain(7.f));
        levelGain+=(match-levelGain)*(1-envelope);
        outL*=levelGain; outR*=levelGain;

        // Physical environments: stylised real-world listening tests, not room IR replicas.
        noiseState^=noiseState<<13; noiseState^=noiseState>>17; noiseState^=noiseState<<5;
        const float n=(float)((int32_t)noiseState) / 2147483648.f;
        noiseSlow=noiseSlow*.999f+n*.001f;
        noiseFast=noiseFast*.985f+n*.015f;
        const size_t size=roomL.size();
        float roomEchoL=0,roomEchoR=0;
        if(size>0) {
            const size_t d1=(size_t)std::min((int)size-1,(int)(sr*.037f));
            const size_t d2=(size_t)std::min((int)size-1,(int)(sr*.083f));
            roomEchoL=.18f*roomR[(roomPos+size-d1)%size]+.09f*roomL[(roomPos+size-d2)%size];
            roomEchoR=.18f*roomL[(roomPos+size-d1)%size]+.09f*roomR[(roomPos+size-d2)%size];
            roomL[roomPos]=outL;roomR[roomPos]=outR;
            if(++roomPos>=size) roomPos=0;
        }
        const float busNoise=noiseSlow*.58f+noiseFast*.12f+n*.0030f;
        const float windNoise=noiseSlow*.31f+noiseFast*.08f;
        const float bus=environmentSmooth[1];
        const float outside=environmentSmooth[2];
        const float room=environmentSmooth[3];
        outL=outL*(1-.08f*room) + room*roomEchoL + bus*busNoise + outside*windNoise;
        outR=outR*(1-.08f*room) + room*roomEchoR + bus*(busNoise*.91f+n*.0006f) + outside*(windNoise*.92f);
        // Mono audition is a sum, not just the left channel.
        const float mid=.5f*(outL+outR);
        outL=outL*(1-monoSmooth)+mid*monoSmooth;
        outR=outR*(1-monoSmooth)+mid*monoSmooth;
        const float audition=mixSmooth*(1-originalSmooth);
        l=(inL*(1-audition)+outL*audition)*quietSmooth;
        r=(inR*(1-audition)+outR*audition)*quietSmooth;
        if(!std::isfinite(l)) l=0;
        if(!std::isfinite(r)) r=0;
    }

private:
    static int clampi(int v,int lo,int hi) noexcept { return std::max(lo,std::min(v,hi)); }
    float sr=48000.f, dryRms=1.e-6f,wetRms=1.e-6f,levelGain=1;
    float monoSmooth=0, quietSmooth=1, mixSmooth=1, originalSmooth=0;
    std::array<DeviceModel,8> models;
    std::array<float,8> deviceSmooth{};
    std::array<float,4> environmentSmooth{};
    std::vector<float> roomL,roomR;
    size_t roomPos=0;
    uint32_t noiseState=0x6f8c9d31u;
    float noiseSlow=0, noiseFast=0;
};
} // namespace lta
