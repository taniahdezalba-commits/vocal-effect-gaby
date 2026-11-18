#include "PluginProcessor.h"
#include "PluginEditor.h"

VocalEffectGabyProcessor::VocalEffectGabyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VocalEffectGaby", createLayout())
{
    pClarity  = apvts.getRawParameterValue ("clarity");
    pAmbience = apvts.getRawParameterValue ("ambience");
    pGate     = apvts.getRawParameterValue ("gate");
    for (int b = 0; b < 4; ++b)
        pBand[b] = apvts.getRawParameterValue ("band" + juce::String (b));
    pMode     = apvts.getRawParameterValue ("mode");
    pInGain   = apvts.getRawParameterValue ("inGain");
    pOutGain  = apvts.getRawParameterValue ("outGain");
    pPower    = apvts.getRawParameterValue ("power");

    for (auto* arr : { &specIn, &specOut, &specNoise })
        for (auto& v : *arr)
            v.store (-120.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout VocalEffectGabyProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [] (const char* id, const char* name, float def, float max = 100.0f)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                      NormalisableRange<float> (0.0f, max, 0.1f), def,
                                                      AudioParameterFloatAttributes().withLabel ("%"));
    };

    p.push_back (pct ("clarity", "Claridad", 60.0f));
    p.push_back (pct ("ambience", "Ambiente", 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 }, "Compuerta",
                                                        NormalisableRange<float> (0.0f, 40.0f, 0.1f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));
    const char* bandNames[] = { "Graves", "Medios", "Presencia", "Aire" };
    for (int b = 0; b < 4; ++b)
        p.push_back (pct (("band" + String (b)).toRawUTF8(), bandNames[b], 100.0f, 200.0f));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Modo",
                                                         StringArray { "Voz", "Ambiente" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "inGain", 1 }, "Ganancia",
                                                        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "outGain", 1 }, "Volumen",
                                                        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "power", 1 }, "Encendido", true));

    return { p.begin(), p.end() };
}

bool VocalEffectGabyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VocalEffectGabyProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, getTotalNumOutputChannels());
    setLatencySamples (engine.latency());

    inGainSm.reset (sampleRate, 0.05);
    outGainSm.reset (sampleRate, 0.05);
    inGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    outGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
}

static void storeMax (std::atomic<float>& a, float v)
{
    float prev = a.load();
    while (v > prev && ! a.compare_exchange_weak (prev, v)) {}
}

void VocalEffectGabyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    for (int c = getTotalNumInputChannels(); c < numCh; ++c)
        buffer.clear (c, 0, n);

    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainSm.getNextValue();
        for (int c = 0; c < numCh; ++c)
            buffer.getWritePointer (c)[i] *= g;
    }
    float pk = 0.0f;
    for (int c = 0; c < numCh; ++c) pk = juce::jmax (pk, buffer.getMagnitude (c, 0, n));
    storeMax (inputPeak, pk);

    gabyvx::Settings s;
    const bool on = pPower->load() > 0.5f;
    s.clarity = on ? pClarity->load() * 0.01 : 0.0;   // apagado = transparente (misma latencia)
    s.ambience = pAmbience->load() * 0.01;
    s.gateDepthDb = on ? pGate->load() : 0.0;
    for (int b = 0; b < 4; ++b) s.bands[(size_t) b] = pBand[b]->load() * 0.01;
    s.ambienceMode = on && pMode->load() > 0.5f;
    if (! on) s.ambience = 0.0;

    engine.process (buffer.getArrayOfWritePointers(), numCh, n, s);

    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainSm.getNextValue();
        for (int c = 0; c < numCh; ++c)
            buffer.getWritePointer (c)[i] *= g;
    }
    pk = 0.0f;
    for (int c = 0; c < numCh; ++c) pk = juce::jmax (pk, buffer.getMagnitude (c, 0, n));
    storeMax (outputPeak, pk);

    const auto& d = engine.display();
    for (int b = 0; b < gabyvx::kDisplayBands; ++b)
    {
        specIn[(size_t) b].store (d.displayIn[(size_t) b]);
        specOut[(size_t) b].store (d.displayOut[(size_t) b]);
        specNoise[(size_t) b].store (d.displayNoise[(size_t) b]);
    }
    reductionDb.store (d.reductionDb);
    voiceActive.store (d.voiceActive);
}

juce::AudioProcessorEditor* VocalEffectGabyProcessor::createEditor()
{
    return new VocalEffectGabyEditor (*this);
}

void VocalEffectGabyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalEffectGabyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalEffectGabyProcessor();
}
