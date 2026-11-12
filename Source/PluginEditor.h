#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <vector>

class RackLook final : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
    void drawComboBox(juce::Graphics&,int,int,bool,int,int,int,int,juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
};
class TdpAudioProcessorEditor final: public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TdpAudioProcessorEditor(TdpAudioProcessor&);
    ~TdpAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    struct Control {juce::Component* component=nullptr;juce::Rectangle<int> area;};
    TdpAudioProcessor& processor;
    RackLook skin;
    juce::ComboBox presets;
    juce::TextButton advancedButton;
    bool advancedPage=false;
    size_t mainControlCount=0,mainLabelCount=0;
    void updatePage();
    void drawScope(juce::Graphics&,juce::Rectangle<float>) const;
    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<SA>> knobLinks;
    std::vector<std::unique_ptr<BA>> toggleLinks;
    std::vector<std::unique_ptr<CA>> comboLinks;
    std::vector<Control> positions;
    struct PrintLabel {juce::String text;int cx=0,y=0,width=0;};
    std::vector<PrintLabel> labels;
    void knob(const juce::String&,const juce::String&,int,int,int=82);
    void switchButton(const juce::String&,const juce::String&,int,int,int=105);
    void choice(const juce::String&,const juce::StringArray&,int,int,int=152);
    void addLabel(const juce::String&,int,int,int);
    void drawScrew(juce::Graphics&,float,float) const;
    void drawPanel(juce::Graphics&,juce::Rectangle<float>,const juce::String&) const;
    void drawMeter(juce::Graphics&,juce::Rectangle<float>,float,const juce::String&) const;
    void timerCallback() override { repaint(); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TdpAudioProcessorEditor)
};
