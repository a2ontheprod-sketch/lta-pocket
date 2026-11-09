#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>

class LTAPocketAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit LTAPocketAudioProcessorEditor(LTAPocketAudioProcessor&);
    ~LTAPocketAudioProcessorEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override {}
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void timerCallback() override {
        if(warningFrames>0)--warningFrames;
        const int device=value("device"),env=value("environment");
        for(int i=0;i<6;++i) cardFade[(size_t)i]+=( (i==device?1.f:0.f)-cardFade[(size_t)i])*.19f;
        for(int i=0;i<4;++i) envFade[(size_t)i]+=( (i==env?1.f:0.f)-envFade[(size_t)i])*.19f;
        repaint();
    }
    void paintDeviceCards(juce::Graphics&);
    void paintEnvironment(juce::Graphics&);
    void paintUtilities(juce::Graphics&);
    void paintPhone(juce::Graphics&);
    void paintMix(juce::Graphics&);
    void paintDeviceGlyph(juce::Graphics&, int index, float x, float centerY, juce::Colour ink);
    juce::Point<float> nativePos(juce::Point<float>) const;
    void setChoice(const char* id,int value);
    void setBool(const char* id,bool value);
    void updateMix(float x);
    int value(const char* id) const;
    bool on(const char* id) const { return value(id)!=0; }
    float mixValue() const;
    juce::Image art;
    LTAPocketAudioProcessor& processor;
    std::array<float,6> cardFade{{1.f,0.f,0.f,0.f,0.f,0.f}};
    std::array<float,4> envFade{{1.f,0.f,0.f,0.f}};
    bool mixDragging=false;
    float progressGlow=0;
    int warningFrames=0;
    juce::String warningText;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LTAPocketAudioProcessorEditor)
};
