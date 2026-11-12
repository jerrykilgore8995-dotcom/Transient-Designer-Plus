#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <memory>

namespace {
using Layout=juce::AudioProcessorValueTreeState::ParameterLayout;
using PID=juce::ParameterID;
void addFloat(Layout& l,const juce::String& id,const juce::String& title,float lo,float hi,float def,float step=0.01f) {
    l.add(std::make_unique<juce::AudioParameterFloat>(PID(id,1),title,
        juce::NormalisableRange<float>(lo,hi,step),def));
}
void addBool(Layout& l,const juce::String& id,const juce::String& title,bool def=false) {
    l.add(std::make_unique<juce::AudioParameterBool>(PID(id,1),title,def));
}
void addChoice(Layout& l,const juce::String& id,const juce::String& title,juce::StringArray choices,int def=0) {
    l.add(std::make_unique<juce::AudioParameterChoice>(PID(id,1),title,choices,def));
}
float rv(const juce::AudioProcessorValueTreeState& s,const juce::String& id) {
    auto* p=s.getRawParameterValue(id);return p==nullptr?0.f:p->load();
}
const char* const ids[3]={"lo","mid","hi"};
void setParam(juce::AudioProcessorValueTreeState& s,const juce::String& id,float value) {
    if(auto* p=s.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value));
}
}
juce::AudioProcessorValueTreeState::ParameterLayout TdpAudioProcessor::makeLayout() {
    Layout l;
    addChoice(l,"mode","Processing bands",{"Full","Dual","Tri"},0);
    addChoice(l,"stereo","Stereo routing",{"Stereo Link","Dual Mono","Mid / Side"},0);
    addChoice(l,"xquality","Crossover quality",{"Zero-latency complementary IIR","Linear-phase FIR"},0);
    addChoice(l,"grouping","Hit grouping",{"Separate hits","Adaptive grouping","Group near hits"},1);
    addChoice(l,"prelook","Transient lookahead",{"Zero latency","0.5 ms","1.5 ms","3 ms"},0);
    addFloat(l,"rearm","Minimum retrigger ms",1,50,4,0.5f);
    addFloat(l,"groupwindow","Hit grouping window ms",5,200,60,1);
    addFloat(l,"floor","Adaptive noise floor dBFS",-100,-24,-75);
    addFloat(l,"smooth","Gain envelope smoothing ms",0.3f,15,1.5f);
    addFloat(l,"matk","Mid attack trim percent",0,200,100,1);
    addFloat(l,"satk","Side attack trim percent",0,200,100,1);
    addFloat(l,"msus","Mid sustain trim percent",0,200,100,1);
    addFloat(l,"ssus","Side sustain trim percent",0,200,100,1);
    addFloat(l,"room","Room bloom/cut percent",-200,200,0,1);
    addFloat(l,"roomearly","Room onset delay ms",0,200,25,1);
    addFloat(l,"roombloom","Room bloom time ms",30,750,220,1);
    addFloat(l,"roomdamp","Room damping",0,100,50,1);
    addFloat(l,"roomwidth","Room width percent",0,200,100,1);
    addBool(l,"spectral","Spectral transient-tonal separation");
    addFloat(l,"sattack","Spectral transient gain dB",-12,12,0);
    addFloat(l,"stonal","Spectral tonal gain dB",-12,12,0);
    addFloat(l,"ssense","Spectral flux sensitivity",0,100,60,1);
    addBool(l,"guard4x","4x intersample peak estimation");
    addFloat(l,"link","Link amount",0,100,100,1);
    addFloat(l,"xlow","Low crossover Hz",60,900,180,1);
    addFloat(l,"xhigh","High crossover Hz",950,15000,3200,10);
    addFloat(l,"input","Input trim dB",-24,24,0);
    addFloat(l,"output","Output trim dB",-24,24,0);
    addFloat(l,"mix","Wet mix percent",0,100,100,1);
    addFloat(l,"sense","Detector sensitivity",0,100,60,1);
    addFloat(l,"peak","Peak / RMS blend",0,100,75,1);
    addFloat(l,"schpf","Detector HPF Hz",20,250,20,1);
    addFloat(l,"soft","Edge softness",0,100,35,1);
    addBool(l,"delta","Delta listen");
    addBool(l,"auto","Auto gain");
    addBool(l,"guard","Sample peak guard");
    addChoice(l,"lookahead","Peak guard lookahead",{"Off","1 ms","3 ms"},0);
    addFloat(l,"ceiling","Peak guard ceiling dB",-9,0,-0.5f);
    for(int b=0;b<3;++b) {
        const juce::String p=ids[b];
        addFloat(l,p+"atk",p+" attack gain dB",-18,18,0);
        addFloat(l,p+"atms",p+" attack time ms",0.35f,90,18);
        addFloat(l,p+"curve",p+" attack curve",0.2f,4,1);
        addFloat(l,p+"body",p+" body gain dB",-15,15,0);
        addFloat(l,p+"btms",p+" body length ms",5,500,100,1);
        addFloat(l,p+"sus",p+" sustain gain dB",-24,24,0);
        addFloat(l,p+"hold",p+" sustain onset delay ms",0,500,60,1);
        addFloat(l,p+"decay",p+" sustain recovery ms",40,3000,700,1);
        addFloat(l,p+"follow",p+" adaptive decay follow",0,100,75,1);
        addFloat(l,p+"freq",p+" detection focus Hz",35,15000,1200,1);
        addFloat(l,p+"fq",p+" detection focus Q",0.25f,8.f,1);
        addFloat(l,p+"focus",p+" detection focus blend",0,100,0,1);
        addBool(l,p+"on",p+" processing enabled",true);
        addBool(l,p+"solo",p+" solo");
    }
    return l;
}
TdpAudioProcessor::TdpAudioProcessor():
    juce::AudioProcessor(juce::AudioProcessor::BusesProperties()
        .withInput("Input",juce::AudioChannelSet::stereo(),true)
        .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    state(*this,nullptr,"TransientDesignerPlusState",makeLayout()) {
    for(auto& p:bandActivity)p.store(0.f);
    for(auto& p:scopeInput)p.store(0.f);
    for(auto& p:scopeOutput)p.store(0.f);
    for(auto& p:scopeAttack)p.store(0.f);
    for(auto& p:scopeSustain)p.store(0.f);
    for(auto& p:scopeBody)p.store(0.f);
    for(auto& p:scopeRoom)p.store(0.f);
    for(auto& p:scopeGain)p.store(0.f);
}
bool TdpAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto in=l.getMainInputChannelSet(),out=l.getMainOutputChannelSet();
    return (in==juce::AudioChannelSet::mono()||in==juce::AudioChannelSet::stereo()) && in==out;
}
void TdpAudioProcessor::prepareToPlay(double fs,int n) {
    engine.prepare((float)fs,n);
    engine.configure(readSettings(),true);
    pendingHostLatency.store(engine.latencySamples());
    setLatencySamples(engine.latencySamples());
    scopeDecimator=0;
}
void TdpAudioProcessor::handleAsyncUpdate(){
    const int desired=pendingHostLatency.load(std::memory_order_acquire);
    if(getLatencySamples()!=desired)setLatencySamples(desired);
}
tdp2::Settings TdpAudioProcessor::readSettings() const noexcept {
    tdp2::Settings s;
    s.bands=(int)rv(state,"mode")+1;
    s.stereo=(int)rv(state,"stereo");
    s.crossover=(int)rv(state,"xquality");
    s.grouping=(int)rv(state,"grouping");
    const int pre=(int)rv(state,"prelook");
    s.lookaheadMs=pre==1?.5f:pre==2?1.5f:pre==3?3.f:0.f;
    s.rearmMs=rv(state,"rearm");
    s.groupWindowMs=rv(state,"groupwindow");
    s.noiseGateDb=rv(state,"floor");
    s.smoothingMs=rv(state,"smooth");
    s.midAttack=rv(state,"matk")*.01f;
    s.sideAttack=rv(state,"satk")*.01f;
    s.midSustain=rv(state,"msus")*.01f;
    s.sideSustain=rv(state,"ssus")*.01f;
    s.roomAmount=rv(state,"room")*.01f;
    s.roomEarlyMs=rv(state,"roomearly");
    s.roomBloomMs=rv(state,"roombloom");
    s.roomDamp=rv(state,"roomdamp")*.01f;
    s.roomWidth=rv(state,"roomwidth")*.01f;
    s.spectral=rv(state,"spectral")>.5f;
    s.spectralAttackDb=rv(state,"sattack");
    s.spectralTonalDb=rv(state,"stonal");
    s.spectralSense=rv(state,"ssense")*.01f;
    s.peakGuard4x=rv(state,"guard4x")>.5f;
    s.link=rv(state,"link")*.01f;
    s.lowHz=rv(state,"xlow");s.highHz=rv(state,"xhigh");
    s.inputDb=rv(state,"input");s.outputDb=rv(state,"output");
    s.mix=rv(state,"mix")*.01f;
    s.sensitivity=rv(state,"sense")*.01f;
    s.peakBlend=rv(state,"peak")*.01f;
    s.sidechainHPF=rv(state,"schpf");
    s.edgeSoftness=rv(state,"soft")*.01f;
    s.delta=rv(state,"delta")>.5f;
    s.autoGain=rv(state,"auto")>.5f;
    s.peakGuard=rv(state,"guard")>.5f;
    int la=(int)rv(state,"lookahead");
    s.guardLookaheadMs=la==1?1.f:(la==2?3.f:0.f);
    s.ceilingDb=rv(state,"ceiling");
    for(int b=0;b<3;++b) {
        const juce::String p=ids[b];
        auto& a=s.band[b];
        a.attackDb=rv(state,p+"atk");
        a.attackMs=rv(state,p+"atms");
        a.attackCurve=rv(state,p+"curve");
        a.bodyDb=rv(state,p+"body");
        a.bodyMs=rv(state,p+"btms");
        a.sustainDb=rv(state,p+"sus");
        a.holdMs=rv(state,p+"hold");
        a.decayMs=rv(state,p+"decay");
        a.follow=rv(state,p+"follow")*.01f;
        a.focusHz=rv(state,p+"freq");
        a.focusQ=rv(state,p+"fq");
        a.focusMix=rv(state,p+"focus")*.01f;
        a.enabled=rv(state,p+"on")>.5f;
        a.solo=rv(state,p+"solo")>.5f;
    }
    return s;
}
void TdpAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    auto s=readSettings();
    engine.configure(s);
    const int newLatency=engine.latencySamples();
    if(newLatency!=pendingHostLatency.load(std::memory_order_relaxed)) {
        pendingHostLatency.store(newLatency,std::memory_order_release);
        triggerAsyncUpdate(); // PDC notification on the message thread, not audio thread
    }
    auto channels=buffer.getNumChannels();
    if(channels==0)return;
    auto* l=buffer.getWritePointer(0);
    auto* r=channels>1?buffer.getWritePointer(1):nullptr;
    float maxIn=0,maxOut=0;
    for(int n=0;n<buffer.getNumSamples();++n) {
        float a=l[n],b=r?r[n]:a;
        maxIn=std::max(maxIn,std::max(std::fabs(a),std::fabs(b)));
        const float original=.5f*(a+b);
        engine.process(a,b);
        l[n]=a;if(r)r[n]=b;
        maxOut=std::max(maxOut,std::max(std::fabs(a),std::fabs(b)));
        if((++scopeDecimator & 15)==0) {
            const int i=scopeWrite.load(std::memory_order_relaxed)&(ScopeSize-1);
            const auto sh=engine.liveShape();
            scopeInput[(size_t)i].store(original,std::memory_order_relaxed);
            scopeOutput[(size_t)i].store(.5f*(a+b),std::memory_order_relaxed);
            scopeAttack[(size_t)i].store(sh.attack,std::memory_order_relaxed);
            scopeSustain[(size_t)i].store(sh.sustain,std::memory_order_relaxed);
            scopeBody[(size_t)i].store(sh.body,std::memory_order_relaxed);
            scopeRoom[(size_t)i].store(sh.room,std::memory_order_relaxed);
            scopeGain[(size_t)i].store(engine.liveGainDb()*.03f,std::memory_order_relaxed);
            scopeWrite.store((i+1)&(ScopeSize-1),std::memory_order_release);
        }
    }
    inputPeak.store(maxIn,std::memory_order_relaxed);
    outputPeak.store(maxOut,std::memory_order_relaxed);
    auto acts=engine.bandActivity();
    for(int i=0;i<3;++i) bandActivity[i].store(acts[(size_t)i],std::memory_order_relaxed);
}
juce::AudioProcessorEditor* TdpAudioProcessor::createEditor() {
    return new TdpAudioProcessorEditor(*this);
}
void TdpAudioProcessor::getStateInformation(juce::MemoryBlock& block) {
    std::unique_ptr<juce::XmlElement> xml(state.copyState().createXml());
    copyXmlToBinary(*xml,block);
}
void TdpAudioProcessor::setStateInformation(const void* data,int size) {
    auto xml=getXmlFromBinary(data,size);
    if(xml && xml->hasTagName(state.state.getType()))
        state.replaceState(juce::ValueTree::fromXml(*xml));
}
const std::array<const char*,12>& TdpAudioProcessor::presetNames() {
    static const std::array<const char*,12> names {
        "Neutral / Clean","Kick - Precision","Snare - Crack",
        "Room - Bloom","Acoustic - Definition","Cymbals - Smooth",
        "Guitars - Pick","Drum Bus - Impact",
        "Double Kick - Focus","Room - Explosive","Transient - Pre-hit",
        "Spectral - Clean Presence"
    };
    return names;
}
void TdpAudioProcessor::applyFactoryPreset(int index) {
    if(index<0||index>=12)return;
    // Deterministic, complete reset of tone-shaping parameters.
    setParam(state,"mode",0);setParam(state,"stereo",0);
    setParam(state,"link",100);setParam(state,"mix",100);
    setParam(state,"input",0);setParam(state,"output",0);
    setParam(state,"auto",0);setParam(state,"delta",0);
    setParam(state,"guard",0);setParam(state,"lookahead",0);
    setParam(state,"sense",60);setParam(state,"peak",75);
    setParam(state,"soft",35);setParam(state,"schpf",20);
    setParam(state,"xlow",180);setParam(state,"xhigh",3200);
    setParam(state,"xquality",0);setParam(state,"grouping",1);setParam(state,"prelook",0);
    setParam(state,"rearm",4);setParam(state,"groupwindow",60);
    setParam(state,"floor",-75);setParam(state,"smooth",1.5f);
    setParam(state,"matk",100);setParam(state,"satk",100);
    setParam(state,"msus",100);setParam(state,"ssus",100);
    setParam(state,"room",0);setParam(state,"roomearly",25);
    setParam(state,"roombloom",220);setParam(state,"roomdamp",50);
    setParam(state,"roomwidth",100);setParam(state,"spectral",0);
    setParam(state,"sattack",0);setParam(state,"stonal",0);
    setParam(state,"ssense",60);setParam(state,"guard4x",0);
    for(int b=0;b<3;++b){
        const juce::String p=ids[b];
        setParam(state,p+"atk",0);setParam(state,p+"atms",18);
        setParam(state,p+"curve",1);setParam(state,p+"body",0);
        setParam(state,p+"btms",100);setParam(state,p+"sus",0);
        setParam(state,p+"hold",60);setParam(state,p+"decay",700);
        setParam(state,p+"follow",75);
        setParam(state,p+"freq",1200);setParam(state,p+"fq",1);
        setParam(state,p+"focus",0);
        setParam(state,p+"on",1);setParam(state,p+"solo",0);
    }
    auto shape=[this](const char* b,float attack,float body,float sustain,float atms,float btms,float hold,float decay){
        juce::String p(b);
        setParam(state,p+"atk",attack);setParam(state,p+"body",body);
        setParam(state,p+"sus",sustain);setParam(state,p+"atms",atms);
        setParam(state,p+"btms",btms);setParam(state,p+"hold",hold);
        setParam(state,p+"decay",decay);
    };
    switch(index){
        case 1:shape("mid",8,-2,-8,12,90,35,350);break;
        case 2:shape("mid",12,3,-7,8,115,50,450);break;
        case 3:shape("mid",-3,5,14,16,185,45,1700);break;
        case 4:shape("mid",4,1,-4,25,180,100,850);break;
        case 5:setParam(state,"mode",2);shape("hi",-9,-2,-11,9,80,35,550);break;
        case 6:shape("mid",6,5,-3,8,95,70,430);break;
        case 7:setParam(state,"mode",2);shape("lo",5,3,-3,20,120,90,520);
               shape("mid",7,2,2,12,110,85,820);shape("hi",2,1,-3,7,100,65,500);break;
        case 8: setParam(state,"grouping",0);setParam(state,"rearm",1.5f);
                setParam(state,"prelook",2);shape("mid",7,2,-5,5,45,20,240);break;
        case 9: setParam(state,"room",150);setParam(state,"roomwidth",160);
                setParam(state,"roomearly",18);setParam(state,"roombloom",320);
                shape("mid",3,4,11,13,160,35,1500);break;
        case 10:setParam(state,"prelook",3);setParam(state,"smooth",2);
                shape("mid",12,0,-5,4,50,18,280);break;
        case 11:setParam(state,"spectral",1);setParam(state,"sattack",3);
                setParam(state,"stonal",-2);setParam(state,"ssense",65);break;
        default:break;
    }
    presetIndex.store(index);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new TdpAudioProcessor();}
