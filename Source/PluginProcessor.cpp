#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

using namespace juce;

juce::AudioProcessorValueTreeState::ParameterLayout LTAPocketAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"device",1},"Device",
        juce::StringArray{"Phone","Laptop","Speaker","Earbuds","Car","TV"},0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"phoneModel",1},"Phone Type",
        juce::StringArray{"Modern","Compact","Vintage"},0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"environment",1},"Environment",
        juce::StringArray{"None","Bus","Outdoor","Room"},0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"mono",1},"Mono",false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"quiet",1},"Quiet Listening",false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"original",1},"Original Comparison",false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"mix",1},"Simulation Mix",
        juce::NormalisableRange<float>{0.f,1.f,.001f},1.f));
    return {params.begin(),params.end()};
}

LTAPocketAudioProcessor::LTAPocketAudioProcessor()
    : juce::AudioProcessor(juce::AudioProcessor::BusesProperties()
          .withInput("Input",juce::AudioChannelSet::stereo(),true)
          .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      state(*this,nullptr,"LTA_POCKET_PARAMS",createParameterLayout()) {
    for(auto& s:scope) s.store(0.f,std::memory_order_relaxed);
}

bool LTAPocketAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    auto mainOut=layouts.getMainOutputChannelSet();
    return (mainOut==juce::AudioChannelSet::mono() || mainOut==juce::AudioChannelSet::stereo())
           && layouts.getMainInputChannelSet()==mainOut;
}

void LTAPocketAudioProcessor::prepareToPlay(double sampleRate,int) {
    activeSampleRate=sampleRate>20000?sampleRate:48000;
    dsp.prepare((float)activeSampleRate);
    subDryLP.lowpass((float)activeSampleRate,130.f); subWetLP.lowpass((float)activeSampleRate,130.f);
    subDryLP.reset();subWetLP.reset();subInAverage=subOutAverage=1.e-9f;
    const size_t frames=(size_t)std::ceil(activeSampleRate*12.0);
    captureL.assign(frames,0.f); captureR.assign(frames,0.f);
    captureWrite=0;captureValid=0;previewHead=0;previewStart=0;previewRemaining=0;previewLength=0;
    previewPlaying=false;previewCrossfade=0;
    previewRequested.store(false);previewActiveUI.store(false);capturedDuration.store(0.f);previewProgress.store(0.f);
    inLevel.store(0);outLevel.store(0);scopeDecimation=0;
}

void LTAPocketAudioProcessor::releaseResources() { previewRequested.store(false); }

bool LTAPocketAudioProcessor::requestPreviewToggle() noexcept {
    const bool currently=previewRequested.load(std::memory_order_acquire);
    if(!currently && capturedDuration.load(std::memory_order_relaxed)<.25f) return false;
    previewRequested.store(!currently,std::memory_order_release);
    return true;
}

std::array<float,128> LTAPocketAudioProcessor::scopeSnapshot() const noexcept {
    std::array<float,128> result{};
    unsigned write=scopeWrite.load(std::memory_order_acquire);
    for(size_t i=0;i<result.size();++i)
        result[i]=scope[(write+(unsigned)i)%result.size()].load(std::memory_order_relaxed);
    return result;
}

void LTAPocketAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) {
    juce::ScopedNoDenormals denormals;
    const int n=buffer.getNumSamples();
    if(n<=0 || buffer.getNumChannels()==0) return;
    for(int ch=2;ch<buffer.getNumChannels();++ch) buffer.clear(ch,0,n);

    // Monitoring tool: host-supported offline renders are passed through unchanged.
    // Real-time exports may not be reported as offline; see README.
    const bool offline=isNonRealtime();
    offlineBypassed.store(offline,std::memory_order_relaxed);
    if(offline) {
        previewRequested.store(false,std::memory_order_release);
        previewActiveUI.store(false,std::memory_order_relaxed);
        return;
    }

    if(auto* playHead=getPlayHead()) {
        if(auto pos=playHead->getPosition()) {
            hostPlaying.store(pos->getIsPlaying(),std::memory_order_relaxed);
            if(auto seconds=pos->getTimeInSeconds())
                hostTime.store((float)*seconds,std::memory_order_relaxed);
        }
    }
    lta::Controls c;
    c.device=(int)state.getRawParameterValue("device")->load();
    c.phoneModel=(int)state.getRawParameterValue("phoneModel")->load();
    c.environment=(int)state.getRawParameterValue("environment")->load();
    c.mono=state.getRawParameterValue("mono")->load()>=.5f;
    c.quiet=state.getRawParameterValue("quiet")->load()>=.5f;
    c.original=state.getRawParameterValue("original")->load()>=.5f;
    c.mix=state.getRawParameterValue("mix")->load();

    const bool wants=previewRequested.load(std::memory_order_acquire);
    if(wants && !previewPlaying && captureValid>0 && !captureL.empty()) {
        previewStart=(captureWrite+(int)captureL.size()-captureValid)%(int)captureL.size();
        previewHead=previewStart;
        previewRemaining=previewLength=captureValid;
        previewPlaying=true;
    }
    if(!wants) previewPlaying=false;
    previewActiveUI.store(previewPlaying,std::memory_order_relaxed);

    float blockIn=0.f,blockOut=0.f;
    auto* left=buffer.getWritePointer(0);
    auto* right=buffer.getNumChannels()>1?buffer.getWritePointer(1):nullptr;
    const int frames=(int)captureL.size();
    for(int i=0;i<n;++i) {
        const float dryL=left[i],dryR=right!=nullptr?right[i]:dryL;
        blockIn=std::max(blockIn,std::max(std::fabs(dryL),std::fabs(dryR)));
        if(!previewPlaying && previewCrossfade<=.002f && frames>0) {
            captureL[(size_t)captureWrite]=dryL;
            captureR[(size_t)captureWrite]=dryR;
            if(++captureWrite>=frames)captureWrite=0;
            if(captureValid<frames)++captureValid;
        }
        float sampleL=dryL,sampleR=dryR;
        const float smoothCoeff=std::min(1.f,1.f/((float)activeSampleRate*.008f));
        previewCrossfade+=((previewPlaying?1.f:0.f)-previewCrossfade)*smoothCoeff;
        if((previewPlaying || previewCrossfade>.002f) && frames>0) {
            const float audL=captureL[(size_t)previewHead];
            const float audR=captureR[(size_t)previewHead];
            sampleL=dryL*(1-previewCrossfade)+audL*previewCrossfade;
            sampleR=dryR*(1-previewCrossfade)+audR*previewCrossfade;
            if(++previewHead>=frames)previewHead=0;
            if(--previewRemaining<=0){previewRemaining=previewLength;previewHead=previewStart;}
        }
        dsp.process(sampleL,sampleR,c);
        const float subBefore=subDryLP.tick(.5f*(dryL+dryR));
        const float subAfter=subWetLP.tick(.5f*(sampleL+sampleR));
        const float e=std::exp(-1.f/((float)activeSampleRate*.30f));
        subInAverage=e*subInAverage+(1-e)*subBefore*subBefore;
        subOutAverage=e*subOutAverage+(1-e)*subAfter*subAfter;
        if(right!=nullptr){left[i]=sampleL;right[i]=sampleR;}
        else left[i]=.5f*(sampleL+sampleR);
        blockOut=std::max(blockOut,std::max(std::fabs(sampleL),std::fabs(sampleR)));
        if(++scopeDecimation>=64) {
            scopeDecimation=0;
            const unsigned pos=scopeWrite.load(std::memory_order_relaxed);
            scope[pos].store(.5f*(sampleL+sampleR),std::memory_order_relaxed);
            scopeWrite.store((pos+1u)%128u,std::memory_order_release);
        }
    }
    // UI values are atomics; audio thread never locks or calls paint.
    inLevel.store(blockIn,std::memory_order_relaxed);
    outLevel.store(blockOut,std::memory_order_relaxed);
    lowEndChange.store(10.f*std::log10((subOutAverage+1.e-9f)/(subInAverage+1.e-9f)),std::memory_order_relaxed);
    capturedDuration.store((float)captureValid/(float)activeSampleRate,std::memory_order_relaxed);
    if(previewPlaying && previewLength>0)
        previewProgress.store(1.f-(float)previewRemaining/(float)previewLength,std::memory_order_relaxed);
    else previewProgress.store(0.f,std::memory_order_relaxed);
}

juce::AudioProcessorEditor* LTAPocketAudioProcessor::createEditor() {
    return new LTAPocketAudioProcessorEditor(*this);
}

void LTAPocketAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
    if(auto xml=state.copyState().createXml()) copyXmlToBinary(*xml,destination);
}

void LTAPocketAudioProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) {
        auto restored=juce::ValueTree::fromXml(*xml);
        if(restored.isValid()) state.replaceState(restored);
    }
    stopPreview();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LTAPocketAudioProcessor(); }
