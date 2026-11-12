#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace {
const juce::Colour midnight=juce::Colour::fromRGB(19,24,28);
const juce::Colour steel=juce::Colour::fromRGB(117,125,132);
const juce::Colour blue=juce::Colour::fromRGB(45,132,213);
const juce::Colour ink=juce::Colour::fromRGB(19,27,37);
juce::Font font(float size,bool bold=false) {
    return juce::Font(juce::FontOptions(size,bold?juce::Font::bold:juce::Font::plain));
}
void text(juce::Graphics& g,const juce::String& s,int x,int y,int w,int h,float size,
          juce::Colour c,juce::Justification j=juce::Justification::centred) {
    g.setColour(c);g.setFont(font(size,true));
    g.drawFittedText(s,x,y,w,h,j,1);
}
}
void RackLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,
                               float start,float end,juce::Slider&) {
    const auto cx=(float)x+w*.5f,cy=(float)y+h*.5f;
    const float d=std::min((float)w,(float)h)-8.f,r=d*.5f;
    g.setColour(juce::Colour::fromRGB(24,28,33));
    g.fillEllipse(cx-r-3,cy-r-1,d+6,d+6);
    g.setColour(juce::Colours::black.withAlpha(.8f));
    g.drawEllipse(cx-r-3,cy-r-1,d+6,d+6,2.f);
    g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(222,227,230),
        cx-r,cy-r,juce::Colour::fromRGB(71,79,86),cx+r,cy+r,false));
    g.fillEllipse(cx-r,cy-r,d,d);
    g.setColour(juce::Colour::fromRGB(242,244,246).withAlpha(.70f));
    g.drawEllipse(cx-r+2,cy-r+2,d-4,d-4,1.1f);
    g.setColour(juce::Colour::fromRGB(66,74,83));
    juce::Path ticks;
    for(int i=0;i<=10;++i) {
        const float a=start+(end-start)*(float)i/10.f;
        ticks.startNewSubPath(cx+std::cos(a)*(r+4),cy+std::sin(a)*(r+4));
        ticks.lineTo(cx+std::cos(a)*(r+8),cy+std::sin(a)*(r+8));
    }
    g.strokePath(ticks,juce::PathStrokeType(1.3f));
    const float angle=start+pos*(end-start);
    g.setColour(juce::Colour::fromRGB(24,37,50));
    g.drawLine(cx+std::cos(angle)*r*.32f,cy+std::sin(angle)*r*.32f,
               cx+std::cos(angle)*r*.85f,cy+std::sin(angle)*r*.85f,3.8f);
    // Characteristic grey metal knob with a rich blue centre cap.
    const float cap=r*.45f;
    g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(89,185,247),
        cx-cap,cy-cap,juce::Colour::fromRGB(12,54,113),cx+cap,cy+cap,false));
    g.fillEllipse(cx-cap,cy-cap,cap*2,cap*2);
    g.setColour(juce::Colour::fromRGB(15,42,75));
    g.drawEllipse(cx-cap,cy-cap,cap*2,cap*2,1.5f);
    g.setColour(juce::Colours::white.withAlpha(.32f));
    g.drawEllipse(cx-cap+2,cy-cap+2,cap*2-4,cap*2-4,1.f);
}
void RackLook::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool hover,bool down) {
    auto area=b.getLocalBounds().toFloat();
    const bool on=b.getToggleState();
    auto sq=juce::Rectangle<float>(area.getX()+3,area.getCentreY()-11,22,22);
    g.setColour(juce::Colour::fromRGB(19,25,30));g.fillRoundedRectangle(sq.expanded(2),2.f);
    g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(83,92,102),
        sq.getX(),sq.getY(),juce::Colour::fromRGB(37,46,54),sq.getRight(),sq.getBottom(),false));
    g.fillRoundedRectangle(sq,2.f);
    auto inner=sq.reduced(4);
    g.setColour(on?juce::Colour::fromRGB(30,132,238):juce::Colour::fromRGB(34,40,45));
    g.fillRoundedRectangle(inner,1.f);
    if(on){
        g.setColour(blue.withAlpha(.34f));g.fillRoundedRectangle(sq.reduced(1),2.f);
        g.setColour(juce::Colours::white.withAlpha(.72f));g.drawLine(inner.getX()+2,inner.getY()+2,inner.getRight()-2,inner.getY()+2,1.f);
    }
    if(hover||down){g.setColour(juce::Colours::white.withAlpha(.2f));g.drawRoundedRectangle(sq,2.f,1.f);}
    text(g,b.getButtonText(),(int)area.getX()+30,(int)area.getY(),(int)area.getWidth()-30,
         (int)area.getHeight(),12.5f,ink,juce::Justification::centredLeft);
}
void RackLook::drawComboBox(juce::Graphics& g,int w,int h,bool,
                             int,int,int,int,juce::ComboBox&) {
    auto r=juce::Rectangle<float>(0,0,(float)w,(float)h);
    g.setColour(midnight);g.fillRoundedRectangle(r,3.f);
    g.setColour(juce::Colour::fromRGB(79,92,104));g.drawRoundedRectangle(r.reduced(.5f),3.f,1.f);
    g.setColour(juce::Colour::fromRGB(50,158,241));
    juce::Path p;
    float x=(float)w-18,y=(float)h*.5f;
    p.startNewSubPath(x-5,y-2);p.lineTo(x,y+3);p.lineTo(x+5,y-2);
    g.strokePath(p,juce::PathStrokeType(1.6f));
}
juce::Font RackLook::getComboBoxFont(juce::ComboBox&) {return font(13.0f,true);}

TdpAudioProcessorEditor::TdpAudioProcessorEditor(TdpAudioProcessor& p):
    AudioProcessorEditor(&p),processor(p) {
    setLookAndFeel(&skin);
    setOpaque(true);
    setSize(1620,930);
    presets.setColour(juce::ComboBox::textColourId,juce::Colour::fromRGB(127,206,255));
    for(int i=0;i<8;++i)presets.addItem(TdpAudioProcessor::presetNames()[(size_t)i],i+1);
    addAndMakeVisible(presets);
    presets.onChange=[this]{if(presets.getSelectedId()>0)processor.applyFactoryPreset(presets.getSelectedId()-1);};
    positions.push_back({&presets,{420,96,252,30}});
    choice("mode",{"FULL RANGE","DUAL BAND","TRI BAND"},716,96,160);
    choice("stereo",{"STEREO LINK","DUAL MONO","MID / SIDE"},901,96,168);
    knob("input","INPUT",1225,88,74);
    knob("output","OUTPUT",1335,88,74);
    knob("mix","DRY / WET",1445,88,74);
    const char* pfx[]={"lo","mid","hi"};
    const char* label[]={"LOW BAND","MID / FULL BAND","HIGH BAND"};
    (void)label;
    for(int b=0;b<3;++b) {
        int x=66+b*495;
        const juce::String pre=pfx[b];
        const int cx[4]={x+66,x+176,x+286,x+396};
        knob(pre+"atk","ATTACK",cx[0],331);
        knob(pre+"atms","ATTACK TIME",cx[1],331);
        knob(pre+"curve","ATTACK CURVE",cx[2],331);
        knob(pre+"body","BODY",cx[3],331);
        knob(pre+"btms","BODY LENGTH",cx[0],512);
        knob(pre+"sus","SUSTAIN",cx[1],512);
        knob(pre+"hold","SUSTAIN HOLD",cx[2],512);
        knob(pre+"decay","SUSTAIN DECAY",cx[3],512);
        switchButton(pre+"on","IN",x+23,640,88);
        switchButton(pre+"solo","SOLO",x+124,640,105);
    }
    knob("sense","SENSITIVITY",135,787,77);
    knob("peak","PEAK / RMS",288,787,77);
    knob("schpf","SC HPF",441,787,77);
    knob("soft","EDGE SHAPE",594,787,77);
    knob("xlow","LOW XOVER",772,787,77);
    knob("xhigh","HIGH XOVER",922,787,77);
    knob("link","STEREO LINK",1072,787,77);
    knob("ceiling","CEILING",1222,787,77);
    choice("lookahead",{"NO LATENCY","1 ms","3 ms"},1374,797,169);
    switchButton("delta","DELTA LISTEN",102,872,152);
    switchButton("auto","AUTO LEVEL",283,872,152);
    switchButton("guard","PEAK GUARD",464,872,152);
    startTimerHz(25);
}
TdpAudioProcessorEditor::~TdpAudioProcessorEditor() {
    stopTimer();setLookAndFeel(nullptr);
}
void TdpAudioProcessorEditor::addLabel(const juce::String& s,int cx,int y,int width) {
    labels.push_back({s,cx,y,width});
}
void TdpAudioProcessorEditor::knob(const juce::String& id,const juce::String& label,int cx,int cy,int size) {
    auto s=std::make_unique<juce::Slider>();
    s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s->setRotaryParameters(juce::MathConstants<float>::pi*1.27f,
        juce::MathConstants<float>::pi*2.73f,true);
    s->setTextBoxStyle(juce::Slider::TextBoxBelow,false,85,24);
    s->setColour(juce::Slider::textBoxTextColourId,ink);
    s->setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour::fromRGB(176,182,186));
    s->setColour(juce::Slider::textBoxOutlineColourId,juce::Colour::fromRGB(78,86,93));
    s->setPopupDisplayEnabled(true,true,this);
    const auto param=id.endsWith("atms")||id.endsWith("btms")||id.endsWith("hold")||id.endsWith("decay");
    const auto isDb=id.endsWith("atk")||id.endsWith("body")||id.endsWith("sus")||
                    id=="input"||id=="output"||id=="ceiling";
    if(param)s->setTextValueSuffix(" ms");
    else if(isDb)s->setTextValueSuffix(" dB");
    else if(id=="xlow"||id=="xhigh"||id=="schpf")s->setTextValueSuffix(" Hz");
    else if(id!="locurve"&&id!="midcurve"&&id!="hicurve")s->setTextValueSuffix(" %");
    s->setName(label);
    addAndMakeVisible(*s);
    knobLinks.push_back(std::make_unique<SA>(processor.state,id,*s));
    positions.push_back({s.get(),{cx-size/2,cy-size/2,size,112}});
    addLabel(label,cx,cy-size/2-22,109);
    knobs.push_back(std::move(s));
}
void TdpAudioProcessorEditor::switchButton(const juce::String& id,const juce::String& label,int x,int y,int w) {
    auto b=std::make_unique<juce::ToggleButton>(label);
    addAndMakeVisible(*b);
    toggleLinks.push_back(std::make_unique<BA>(processor.state,id,*b));
    positions.push_back({b.get(),{x,y,w,29}});
    toggles.push_back(std::move(b));
}
void TdpAudioProcessorEditor::choice(const juce::String& id,const juce::StringArray& names,int x,int y,int w) {
    auto c=std::make_unique<juce::ComboBox>();
    for(int i=0;i<names.size();++i)c->addItem(names[i],i+1);
    c->setColour(juce::ComboBox::textColourId,juce::Colour::fromRGB(128,202,251));
    addAndMakeVisible(*c);
    comboLinks.push_back(std::make_unique<CA>(processor.state,id,*c));
    positions.push_back({c.get(),{x,y,w,31}});
    combos.push_back(std::move(c));
}
void TdpAudioProcessorEditor::resized() {
    for(const auto& c:positions)c.component->setBounds(c.area);
}
void TdpAudioProcessorEditor::drawScrew(juce::Graphics& g,float x,float y) const {
    g.setColour(juce::Colour::fromRGB(17,21,26));g.fillEllipse(x-11,y-11,22,22);
    g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(197,203,208),
        x-9,y-9,juce::Colour::fromRGB(58,65,72),x+9,y+9,false));
    g.fillEllipse(x-9,y-9,18,18);
    g.setColour(ink);g.drawLine(x-5,y+3,x+5,y-3,2.f);
}
void TdpAudioProcessorEditor::drawPanel(juce::Graphics& g,juce::Rectangle<float> r,const juce::String& title) const {
    g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(155,160,164),
        r.getX(),r.getY(),juce::Colour::fromRGB(112,119,125),r.getRight(),r.getBottom(),false));
    g.fillRoundedRectangle(r,5.f);
    g.setColour(juce::Colour::fromRGB(48,58,65));g.drawRoundedRectangle(r.reduced(.8f),5.f,1.5f);
    g.setColour(juce::Colours::white.withAlpha(.45f));g.drawHorizontalLine((int)r.getY()+4,r.getX()+4,r.getRight()-4);
    auto bar=r.removeFromTop(44);
    g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(43,55,68),
        bar.getX(),bar.getY(),juce::Colour::fromRGB(26,35,45),bar.getX(),bar.getBottom(),false));
    g.fillRoundedRectangle(bar,5.f);
    text(g,title,(int)bar.getX()+15,(int)bar.getY()+4,(int)bar.getWidth()-30,32,
         18,juce::Colour::fromRGB(207,221,233),juce::Justification::centredLeft);
}
void TdpAudioProcessorEditor::drawMeter(juce::Graphics& g,juce::Rectangle<float> r,float db,const juce::String& label) const {
    g.setColour(juce::Colour::fromRGB(23,31,38));g.fillRoundedRectangle(r,3.f);
    g.setColour(juce::Colour::fromRGB(13,18,22));g.drawRoundedRectangle(r,3.f,1.f);
    const int n=24;
    float lit=juce::jlimit(0.f,1.f,std::fabs(db)/20.f)*n;
    for(int i=0;i<n;++i) {
        const float xx=r.getX()+10+i*(r.getWidth()-20)/n;
        juce::Colour c=i<19?blue:juce::Colour::fromRGB(243,150,64);
        g.setColour(c.withAlpha(i<lit?.98f:.11f));
        g.fillRoundedRectangle(xx,r.getY()+8,(r.getWidth()-24)/n-3,r.getHeight()-16,1.5f);
    }
    text(g,label,(int)r.getX()+8,(int)r.getY()-20,(int)r.getWidth()-16,18,
         10,ink,juce::Justification::centredLeft);
}
void TdpAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(midnight);
    auto face=juce::Rectangle<float>(40,14,1540,902);
    g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(190,195,198),
        40,20,juce::Colour::fromRGB(115,123,130),1580,916,false));
    g.fillRoundedRectangle(face,7.f);
    g.setColour(juce::Colour::fromRGB(27,36,43));g.drawRoundedRectangle(face.reduced(1),7.f,3.f);
    for(int y=22;y<910;y+=5){
        g.setColour(juce::Colours::white.withAlpha(y%2?.017f:.012f));
        g.drawHorizontalLine(y,49,1571);
    }
    // Rack ears and mounting points are integral to the faceplate.
    g.setColour(juce::Colour::fromRGB(50,57,62));
    g.fillRoundedRectangle(10.f,20.f,30.f,889.f,4.f);
    g.fillRoundedRectangle(1580.f,20.f,30.f,889.f,4.f);
    for(float xx: {23.f,1597.f})for(float yy:{53.f,462.f,881.f})drawScrew(g,xx,yy);
    g.setColour(juce::Colour::fromRGB(22,35,46));
    g.fillRoundedRectangle(62.f,31.f,1493.f,144.f,5.f);
    g.setColour(juce::Colour::fromRGB(90,132,166));g.drawRoundedRectangle(62.f,31.f,1493.f,144.f,1.f);
    text(g,"TRANSIENT",86,37,340,48,34,juce::Colours::white,juce::Justification::centredLeft);
    text(g,"DESIGNER  +",86,78,295,31,22,juce::Colour::fromRGB(83,183,250),juce::Justification::centredLeft);
    text(g,"3-STAGE ENVELOPE WORKSTATION",86,131,360,22,12,juce::Colour::fromRGB(174,187,197),juce::Justification::centredLeft);
    text(g,"FACTORY PATCH",420,70,252,20,12,juce::Colour::fromRGB(202,213,223));
    text(g,"PROCESSING",716,70,160,20,12,juce::Colour::fromRGB(202,213,223));
    text(g,"ROUTING",901,70,168,20,12,juce::Colour::fromRGB(202,213,223));
    const char* titles[]={"LOW • THUMP","MID • BODY / FULL","HIGH • SNAP"};
    for(int b=0;b<3;++b) {
        int x=65+b*495;
        drawPanel(g,{(float)x,198,470,509},titles[b]);
        g.setColour(juce::Colour::fromRGB(67,76,85));
        g.drawHorizontalLine(457,x+17,x+453);
        g.setColour(juce::Colour::fromRGB(64,73,83));
        g.drawHorizontalLine(626,x+17,x+453);
        float act=processor.bandActivity[(size_t)b].load(std::memory_order_relaxed);
        drawMeter(g,{(float)x+252,647,196,38},act,"ENVELOPE ACTION  /  dB");
        // Highlights are informative, not misleading: unused bands are marked dormant.
        int mode=(int)processor.state.getRawParameterValue("mode")->load();
        bool active=mode==0?b==1:(mode==1?b!=1:true);
        if(!active)text(g,"STANDBY",(int)x+255,255,195,22,12,juce::Colour::fromRGB(77,85,92));
    }
    drawPanel(g,{65,724,1490,179},"DETECTOR & GLOBAL CONTROL");
    g.setColour(juce::Colour::fromRGB(60,71,79));g.drawVerticalLine(719,766,875);
    g.drawVerticalLine(1157,766,875);
    text(g,"DETECTION",87,746,600,18,12,ink,juce::Justification::centredLeft);
    text(g,"BAND CROSSOVERS + LINK",733,746,405,18,12,ink,juce::Justification::centredLeft);
    text(g,"PEAK SAFETY",1172,746,355,18,12,ink,juce::Justification::centredLeft);
    for(const auto& lab:labels)
        text(g,lab.text,lab.cx-lab.width/2,lab.y,lab.width,20,12.3f,
             lab.y>720?ink:juce::Colour::fromRGB(31,41,50));
    text(g,"CLEAN ENGINE  •  ZERO SATURATION  •  44.1 kHz READY",651,872,560,17,
         12,juce::Colour::fromRGB(24,51,76));
    drawMeter(g,{1081,137,170,25},processor.inputPeak.load()*12.f,"INPUT PEAK");
    drawMeter(g,{1273,137,170,25},processor.outputPeak.load()*12.f,"OUTPUT PEAK");
}
