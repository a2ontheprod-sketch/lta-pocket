#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PocketDSP.h"
#include <array>
#include <atomic>
#include <vector>

class LTAPocketAudioProcessor final : public juce::AudioProcessor {
public:
    LTAPocketAudioProcessor();
    ~LTAPocketAudioProcessor() override = default;
    void prepareToPlay(double sampleRate,int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return .15; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState state;

    // A rolling buffer of the most recent 12 seconds of incoming master audio.
    // Play re-auditions this buffer; it does NOT start or stop FL Studio's transport.
    bool requestPreviewToggle() noexcept;
    void stopPreview() noexcept { previewRequested.store(false,std::memory_order_release); }
    bool isPreviewing() const noexcept { return previewActiveUI.load(std::memory_order_relaxed); }
    float capturedSeconds() const noexcept { return capturedDuration.load(std::memory_order_relaxed); }
    float previewPosition() const noexcept { return previewProgress.load(std::memory_order_relaxed); }
    bool isHostPlaying() const noexcept { return hostPlaying.load(std::memory_order_relaxed); }
    float hostSeconds() const noexcept { return hostTime.load(std::memory_order_relaxed); }
    bool offlineMonitorBypassed() const noexcept { return offlineBypassed.load(std::memory_order_relaxed); }
    float inputLevel() const noexcept { return inLevel.load(std::memory_order_relaxed); }
    float outputLevel() const noexcept { return outLevel.load(std::memory_order_relaxed); }
    float lowEndChangeDb() const noexcept { return lowEndChange.load(std::memory_order_relaxed); }
    std::array<float,128> scopeSnapshot() const noexcept;

private:
    lta::PocketDSP dsp;
    std::vector<float> captureL,captureR;
    int captureWrite=0, captureValid=0, previewHead=0, previewStart=0, previewRemaining=0, previewLength=0;
    bool previewPlaying=false;
    float previewCrossfade=0;
    double activeSampleRate=48000;
    std::atomic<bool> previewRequested{false}, previewActiveUI{false};
    std::atomic<float> capturedDuration{0}, previewProgress{0};
    std::atomic<bool> hostPlaying{false}, offlineBypassed{false};
    std::atomic<float> hostTime{0}, inLevel{0}, outLevel{0},lowEndChange{0};
    lta::Biquad subDryLP,subWetLP;
    float subInAverage=1.e-9f,subOutAverage=1.e-9f;
    std::array<std::atomic<float>,128> scope{};
    std::atomic<unsigned> scopeWrite{0};
    unsigned scopeDecimation=0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LTAPocketAudioProcessor)
};
