#include "../Source/PocketDSP.h"
#include <cmath>
#include <iostream>
#include <vector>
#include <cassert>

float signalGain(lta::PocketDSP& engine,int device,int phoneModel,float frequency) {
    engine.prepare(48000.f);
    lta::Controls c; c.device=device;c.phoneModel=phoneModel;c.mix=1;
    double totalIn=0,totalOut=0;
    for(int i=0;i<48000;++i) {
       const float x=0.16f*std::sin(2*3.14159265358979*frequency*i/48000.0);
       float l=x,r=x;engine.process(l,r,c);
       if(i>24000){ totalIn+=x*x; totalOut+=l*l; }
    }
    return (float) std::sqrt(totalOut/totalIn);
}
int main(){
    lta::PocketDSP e;
    auto phoneSub=signalGain(e,0,0,55.f);
    auto phoneMid=signalGain(e,0,0,1500.f);
    auto earbudSub=signalGain(e,3,0,55.f);
    std::cout<<"phone 55Hz "<<phoneSub<<" / 1.5kHz "<<phoneMid<<" / earbud 55Hz "<<earbudSub<<"\n";
    assert(phoneSub<phoneMid*.2f);
    assert(earbudSub>phoneSub);
    for(int device=0;device<6;++device){
      for(int env=0;env<4;++env){
       e.prepare(44100.f);lta::Controls c;c.device=device;c.environment=env;c.mono=(device%2)==0;
       c.quiet=(env%2)==1;
       for(int i=0;i<15000;++i){float l=std::sin(.1f*i)*.8f,r=std::cos(.12f*i)*.8f;
        e.process(l,r,c); assert(std::isfinite(l)&&std::isfinite(r));assert(std::fabs(l)<4.f&&std::fabs(r)<4.f);
       }
      }
    }
    std::cout<<"DSP smoke test passed\n";
}
