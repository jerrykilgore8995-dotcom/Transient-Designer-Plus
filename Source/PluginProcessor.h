#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "TransientEngineV2.h"

class TdpAudioProcessor final : public juce::AudioProcessor, private juce::AsyncUpdater {
public:
    TdpAudioProcessor();
    ~TdpAudioProcessor() override { cancelPendingUpdate(); }
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
    static constexpr int ScopeSize=512;
    std::array<std::atomic<float>,ScopeSize> scopeInput {};
    std::array<std::atomic<float>,ScopeSize> scopeOutput {};
    std::array<std::atomic<float>,ScopeSize> scopeAttack {};
    std::array<std::atomic<float>,ScopeSize> scopeSustain {};
    std::array<std::atomic<float>,ScopeSize> scopeBody {};
    std::array<std::atomic<float>,ScopeSize> scopeRoom {};
    std::array<std::atomic<float>,ScopeSize> scopeGain {};
    std::atomic<int> scopeWrite{0};
    std::atomic<int> pendingHostLatency{0};
    void applyFactoryPreset(int index);
    static const std::array<const char*,12>& presetNames();
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    tdp2::Settings readSettings() const noexcept;
    void handleAsyncUpdate() override;
    tdp2::Engine engine;
    int scopeDecimator=0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TdpAudioProcessor)
};
