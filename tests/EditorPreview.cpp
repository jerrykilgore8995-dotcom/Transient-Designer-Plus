#include "Source/PluginProcessor.h"
#include "Source/PluginEditor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <iostream>

int main(){
    juce::ScopedJuceInitialiser_GUI initialiseGUI;
    TdpAudioProcessor processor;
    processor.prepareToPlay(44100.0,512);
    TdpAudioProcessorEditor editor(processor);
    editor.setVisible(true);
    editor.resized();
    auto capture=[&](const char* path)->bool{
        const auto snap=editor.createComponentSnapshot(editor.getLocalBounds(),false,1.f);
        if(!snap.isValid()||snap.getWidth()!=1620||snap.getHeight()!=930)return false;
        const auto f=juce::File::getCurrentWorkingDirectory().getChildFile(path);
        f.deleteFile();
        auto file=f.createOutputStream();
        if(file==nullptr)return false;
        juce::PNGImageFormat png;
        if(!png.writeImageToStream(snap,*file))return false;
        file->flush();
        std::cout<<"Authentic JUCE capture: "<<f.getFullPathName()<<"\n";
        return true;
    };
    if(!capture("Transient_Designer_Plus_Actual_JUCE_GUI.png"))return 1;
    editor.setAdvancedPageForPreview(true);
    if(!capture("Transient_Designer_Plus_V2_Advanced_JUCE_GUI.png"))return 2;
    return 0;
}
