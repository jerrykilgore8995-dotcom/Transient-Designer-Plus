#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "TransientEngine.h"

class TdpAudioProcessor final : public juce::AudioProcessor {
public:
    TdpAudioProcessor();
    ~TdpAudioProcessor() override = default;
    const juce::String getName() const override { return "Transient Designer Plus"; }
    void prepareToPlay(double sampleRate,int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Current"; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;

    juce::AudioProcessorValueTreeState state;
    std::array<std::atomic<float>,3> bandActivity;
    std::atomic<float> inputPeak{0},outputPeak{0};
    std::atomic<int> presetIndex{-1};
    void applyFactoryPreset(int index);
    static const std::array<const char*,8>& presetNames();
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    tdp::Settings readSettings() const noexcept;
    tdp::Engine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TdpAudioProcessor)
};
