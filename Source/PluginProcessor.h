#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "VocalCleanDSP.h"

class VocalEffectGabyProcessor : public juce::AudioProcessor
{
public:
    VocalEffectGabyProcessor();
    ~VocalEffectGabyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Vocal Effect Gaby"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Datos para la interfaz
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, reductionDb { 0.0f };
    std::atomic<bool> voiceActive { false };
    std::array<std::atomic<float>, gabyvx::kDisplayBands> specIn, specOut, specNoise;

private:
    gabyvx::VocalCleanEngine engine;
    juce::SmoothedValue<float> inGainSm, outGainSm;

    std::atomic<float>* pClarity = nullptr;
    std::atomic<float>* pAmbience = nullptr;
    std::atomic<float>* pGate = nullptr;
    std::atomic<float>* pBand[4] {};
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pInGain = nullptr;
    std::atomic<float>* pOutGain = nullptr;
    std::atomic<float>* pPower = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEffectGabyProcessor)
};
