#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

namespace {
    using C=juce::Colour;
    const C cream=C::fromRGB(246,240,232), light=C::fromRGB(250,247,242),
            dark=C::fromRGB(43,45,48), coral=C::fromRGB(215,100,80),
            sage=C::fromRGB(201,213,198), ink=C::fromRGB(33,37,39),
            dusty=C::fromRGB(177,137,145);
    const char* names[]={"PHONE","LAPTOP","SPEAKER","EARBUDS","CAR","TV"};
    const char* envNames[]={"NONE","BUS","OUTDOOR","ROOM"};
    const char* phoneNames[]={"MODERN","COMPACT","VINTAGE"};
    inline bool hit(float x,float y,float l,float t,float r,float b){ return x>=l&&x<=r&&y>=t&&y<=b; }
    void text(juce::Graphics& g,const juce::String& label,float x,float y,float w,float h,float size,C col,
              juce::Justification just=juce::Justification::centredLeft,bool bold=false){
        g.setColour(col);
        g.setFont(juce::Font(size,bold?juce::Font::bold:juce::Font::plain));
        g.drawText(label,juce::Rectangle<float>(x,y,w,h),just,true);
    }
}

LTAPocketAudioProcessorEditor::LTAPocketAudioProcessorEditor(LTAPocketAudioProcessor& p)
    : juce::AudioProcessorEditor(&p),processor(p) {
    art=juce::ImageCache::getFromMemory(BinaryData::pocket_background_png,BinaryData::pocket_background_pngSize);
    setOpaque(true);
    setResizable(true,true);
    setResizeLimits(960,640,1536,1024);
    if(auto* constrainer=getConstrainer()) constrainer->setFixedAspectRatio(1.5);
    setSize(1152,768);
    startTimerHz(30);
}

int LTAPocketAudioProcessorEditor::value(const char* id) const {
    if(auto* f=processor.state.getRawParameterValue(id)) return (int)std::lround(f->load());
    return 0;
}
float LTAPocketAudioProcessorEditor::mixValue() const {
    if(auto* f=processor.state.getRawParameterValue("mix")) return f->load();
    return 1.f;
}
void LTAPocketAudioProcessorEditor::setChoice(const char* id,int target) {
    if(auto* p=processor.state.getParameter(id)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1((float)target));
        p->endChangeGesture();
    }
}
void LTAPocketAudioProcessorEditor::setBool(const char* id,bool target){ setChoice(id,target?1:0); }
void LTAPocketAudioProcessorEditor::updateMix(float x) {
    if(auto* p=processor.state.getParameter("mix"))
        p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(0.f,1.f,(x-264.f)/1013.f)));
}
juce::Point<float> LTAPocketAudioProcessorEditor::nativePos(juce::Point<float> p) const {
    return {p.x*1536.f/(float)juce::jmax(1,getWidth()),p.y*1024.f/(float)juce::jmax(1,getHeight())};
}

void LTAPocketAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(cream);
    g.saveState();
    g.addTransform(juce::AffineTransform::scale((float)getWidth()/1536.f,(float)getHeight()/1024.f));
    if(art.isValid()) g.drawImage(art,juce::Rectangle<float>(0,0,1536,1024),juce::RectanglePlacement::stretchToFit);
    paintDeviceCards(g);
    paintEnvironment(g);
    paintUtilities(g);
    paintPhone(g);
    paintMix(g);
    g.restoreState();
}

void LTAPocketAudioProcessorEditor::paintDeviceGlyph(juce::Graphics& g,int index,float x,float cy,C colour){
    g.setColour(colour);
    const float y=cy-18;
    switch(index){
      case 0: // phone
        g.drawRoundedRectangle(x+8,y-3,24,42,5,3);
        g.fillRoundedRectangle(x+18,y+33,5,3,1.5f);
        break;
      case 1: // laptop
        g.drawRoundedRectangle(x,y,39,27,2,3);
        g.drawLine(x-5,y+32,x+44,y+32,3);
        g.drawLine(x-5,y+32,x,y+28,3);g.drawLine(x+44,y+32,x+39,y+28,3);
        break;
      case 2: // speaker
        g.drawRoundedRectangle(x+1,y-3,39,40,4,3);
        g.drawEllipse(x+12,y+16,17,17,3);
        g.fillEllipse(x+18,y+5,5,5);
        break;
      case 3: // earbuds
        g.drawEllipse(x+1,y+2,13,14,3);
        g.drawEllipse(x+26,y+2,13,14,3);
        g.drawLine(x+7,y+16,x+7,y+36,3);
        g.drawLine(x+32,y+16,x+32,y+36,3);
        break;
      case 4: // car
        g.drawRoundedRectangle(x,y+10,41,21,5,3);
        g.drawLine(x+7,y+10,x+12,y+1,3);g.drawLine(x+12,y+1,x+30,y+1,3);
        g.drawLine(x+30,y+1,x+35,y+10,3);
        g.fillEllipse(x+5,y+28,8,8);g.fillEllipse(x+29,y+28,8,8);
        break;
      case 5: // tv
        g.drawRoundedRectangle(x,y+1,44,28,3,3);
        g.drawLine(x+22,y+29,x+22,y+37,3);g.drawLine(x+10,y+37,x+34,y+37,3);
        break;
    }
}
void LTAPocketAudioProcessorEditor::paintDeviceCards(juce::Graphics& g) {
    const int device=juce::jlimit(0,5,value("device"));
    constexpr float tops[]={210,314,415,516,615,715};
    constexpr float heights[]={90,85,84,83,83,82};
    for(int i=0;i<6;++i){
        juce::Rectangle<float> rc(45,tops[i],429,heights[i]);
        const bool selected=device==i;
        g.setColour(cream.interpolatedWith(dark,cardFade[(size_t)i]));
        g.fillRoundedRectangle(rc,23.f);
        g.setColour(selected?coral:C::fromRGB(166,165,160));
        g.drawRoundedRectangle(rc,23.f,selected?3.f:1.5f);
        paintDeviceGlyph(g,i,94,tops[i]+heights[i]/2,ink.interpolatedWith(light,cardFade[(size_t)i]));
        text(g,names[i],180,tops[i]+20,235,heights[i]-38,31,ink.interpolatedWith(light,cardFade[(size_t)i]),juce::Justification::centredLeft,true);
        if(selected) {
            g.setColour(coral);g.fillEllipse(439,tops[i]+heights[i]*.5f-9,18,18);
            if(i==0) {
                text(g,phoneNames[juce::jlimit(0,2,value("phoneModel"))],329,tops[i]+61,97,20,12,light.withAlpha(.7f),juce::Justification::centredRight);
            }
        } else {g.setColour(C::fromRGB(104,112,109));g.drawEllipse(438,tops[i]+heights[i]*.5f-8,16,16,1.5f);}
    }
}
void LTAPocketAudioProcessorEditor::paintEnvironment(juce::Graphics& g) {
    const int chosen=juce::jlimit(0,3,value("environment"));
    constexpr float x[]={1083,1180,1279,1378};
    constexpr float widths[]={97,99,99,97};
    for(int i=0;i<4;++i){
        auto rc=juce::Rectangle<float>(x[i],304.f,widths[i],87.f);
        g.setColour(cream.interpolatedWith(dark,envFade[(size_t)i]));g.fillRoundedRectangle(rc, i==0||i==3?17.f:5.f);
        g.setColour(C::fromRGB(171,167,161));g.drawRoundedRectangle(rc,i==0||i==3?17.f:5.f,1.f);
        text(g,envNames[i],x[i]+2,320,widths[i]-4,53,16,ink.interpolatedWith(light,envFade[(size_t)i]),juce::Justification::centred,true);
    }
}
void LTAPocketAudioProcessorEditor::paintUtilities(juce::Graphics& g) {
    if(on("mono")) {
        const auto rc=juce::Rectangle<float>(1082,552,397,83);
        g.setColour(dark);g.fillRoundedRectangle(rc,19);
        g.setColour(coral);g.drawRoundedRectangle(rc,19,2);
        g.setColour(light);
        for(int i=0;i<5;++i)g.fillRoundedRectangle(1149+i*12.f,584+(i%2)*4,5,31-(i%2)*8,2);
        text(g,"MONO",1241,562,199,60,30,light,juce::Justification::centred,true);
    }
    if(on("quiet")) {
        const auto rc=juce::Rectangle<float>(1082,651,397,80);
        g.setColour(dark);g.fillRoundedRectangle(rc,19);
        g.setColour(coral);g.drawRoundedRectangle(rc,19,2);
        text(g,"-16 dB  QUIET",1160,664,270,54,26,light,juce::Justification::centred,true);
    }
}

void LTAPocketAudioProcessorEditor::paintPhone(juce::Graphics& g) {
    // The mockup's baked waveform is removed from the background image.
    // Redraw waveform from audio arriving in this very plugin.
    auto samples=processor.scopeSnapshot();
    const float in=juce::jlimit(0.f,1.f,processor.outputLevel());
    for(int i=0;i<60;++i){
        const int a=i*2;
        const float peak=std::max(std::abs(samples[(size_t)a]),std::abs(samples[(size_t)a+1]));
        const float amp=juce::jlimit(2.f,38.f,3.f+peak*125.f+in*3.f);
        const float x=559.f+i*7.03f;
        g.setColour(light.withAlpha(.75f+std::min(.25f,peak*1.4f)));
        g.fillRoundedRectangle(x,669.f-amp,4.f,2*amp,2.f);
    }
    const bool original=on("original");
    const float progress=processor.isPreviewing()?processor.previewPosition():
       std::fmod(juce::jmax(0.f,processor.hostSeconds()),180.f)/180.f;
    // Timeline is visual only, MIDI host transports vary. Audition time is exact.
    g.setColour(C::fromRGB(120,89,94).withAlpha(.38f));
    g.fillRoundedRectangle(564,755,407,7,3.5f);
    g.setColour(light);
    g.fillRoundedRectangle(564,755,juce::jmax(2.f,407*progress),7,3.5f);
    g.fillEllipse(560+407*progress,752,12,12);
    // A/B switch is a genuine processor bypass comparison, not a fake power symbol.
    g.setColour(original?dark:coral);
    g.fillRoundedRectangle(634,802,135,36,16);
    text(g,original?"ORIGINAL":"SIMULATED",642,807,119,27,17,light,juce::Justification::centred,true);
    // In-plugin preview button: replays a recent 12 s capture. Doesn't transport-control FL.
    g.setColour(dark);
    g.fillEllipse(798,794,50,50);
    g.setColour(light);
    if(processor.isPreviewing()) {
        g.fillRoundedRectangle(814,808,5,22,1);g.fillRoundedRectangle(826,808,5,22,1);
    } else {
        juce::Path p;p.addTriangle(816,807,816,830,835,818.5f);g.fillPath(p);
    }
    // Label screen information honestly: host time only when FL reports it.
    if(processor.offlineMonitorBypassed())
        text(g,"OFFLINE: DRY",565,853,398,20,15,light,juce::Justification::centred);
    else if(warningFrames>0)
        text(g,warningText,561,853,414,20,16,light,juce::Justification::centred);
    else
        text(g,"LOW END " + juce::String(processor.lowEndChangeDb(),1) + " dB   ·   MONITOR ONLY",565,853,398,20,14,light.withAlpha(.85f),juce::Justification::centred);
}

void LTAPocketAudioProcessorEditor::paintMix(juce::Graphics& g) {
    const float v=juce::jlimit(0.f,1.f,mixValue());
    const float x=264+1014*v;
    g.setColour(cream);g.fillRoundedRectangle(250,928,1045,38,14);
    g.setColour(C::fromRGB(198,198,195));g.fillRoundedRectangle(264,937,1014,16,8);
    g.setColour(dark);g.fillRoundedRectangle(264,937,1014*v,16,8);
    g.setColour(light);g.fillEllipse(x-17,926,34,34);
    g.setColour(dark);g.drawEllipse(x-17,926,34,34,2.6f);
    g.setColour(cream);g.fillRect(1351,928,116,38);
    text(g,juce::String((int)std::lround(v*100))+"%",1340,919,132,55,29,ink,juce::Justification::centred,true);
}

void LTAPocketAudioProcessorEditor::mouseDown(const juce::MouseEvent& e) {
    auto p=nativePos(e.position);const float x=p.x,y=p.y;
    const float tops[]={210,314,415,516,615,715};
    const float bottoms[]={300,399,499,599,698,797};
    for(int i=0;i<6;++i) if(hit(x,y,45,tops[i],474,bottoms[i])) {
        if(i==0 && value("device")==0) setChoice("phoneModel",(value("phoneModel")+1)%3);
        else setChoice("device",i);
        repaint();return;
    }
    if(hit(x,y,1082,303,1477,391)) {setChoice("environment",juce::jlimit(0,3,(int)((x-1082)/99)));repaint();return;}
    if(hit(x,y,1082,550,1479,635)) {setBool("mono",!on("mono"));repaint();return;}
    if(hit(x,y,1082,649,1479,734)) {setBool("quiet",!on("quiet"));repaint();return;}
    if(hit(x,y,620,795,781,846) || hit(x,y,637,241,901,502)) {setBool("original",!on("original"));repaint();return;}
    if(hit(x,y,791,792,855,852)) {
        if(!processor.requestPreviewToggle()) {warningFrames=70;warningText="PLAY FL FIRST TO CAPTURE AUDIO";}
        repaint();return;
    }
    if(hit(x,y,240,911,1299,979)) {
        mixDragging=true;
        if(auto* param=processor.state.getParameter("mix"))param->beginChangeGesture();
        updateMix(x);repaint();return;
    }
}
void LTAPocketAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e){
    if(!mixDragging)return;
    updateMix(nativePos(e.position).x);repaint();
}
void LTAPocketAudioProcessorEditor::mouseUp(const juce::MouseEvent&){
    if(mixDragging){
        if(auto* param=processor.state.getParameter("mix"))param->endChangeGesture();
        mixDragging=false;
    }
}
void LTAPocketAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e){
    auto p=nativePos(e.position);
    if(hit(p.x,p.y,240,911,1299,979))setChoice("mix",1);
}
void LTAPocketAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel){
    auto p=nativePos(e.position);
    if(hit(p.x,p.y,240,911,1299,979)) {
        if(auto* param=processor.state.getParameter("mix")){
            const float target=juce::jlimit(0.f,1.f,mixValue()+wheel.deltaY*.05f);
            param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1(target));param->endChangeGesture();
        }
    } else juce::AudioProcessorEditor::mouseWheelMove(e,wheel);
}
void LTAPocketAudioProcessorEditor::mouseMove(const juce::MouseEvent& e){
    auto p=nativePos(e.position);
    bool h=hit(p.x,p.y,45,204,474,800) || hit(p.x,p.y,1080,301,1480,395)
       || hit(p.x,p.y,1080,550,1480,734) || hit(p.x,p.y,240,911,1300,980)
       || hit(p.x,p.y,620,795,855,850) || hit(p.x,p.y,637,241,901,502);
    setMouseCursor(h?juce::MouseCursor::PointingHandCursor:juce::MouseCursor::NormalCursor);
}
