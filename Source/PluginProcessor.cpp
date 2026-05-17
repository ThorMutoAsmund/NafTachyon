/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WaveformSynth.h"
#include "ModulationEnvelope.h"

namespace
{
    constexpr auto attackParamId    = "attack";
    constexpr auto decayParamId     = "decay";
    constexpr auto sustainParamId   = "sustain";
    constexpr auto releaseParamId   = "release";
    constexpr auto waveformParamId      = "waveform";
    constexpr auto filterCutoffParamId  = "filterCutoff";
    constexpr auto filterResonanceParamId = "filterResonance";
    constexpr auto filterSlopeParamId   = "filterSlope";
    constexpr auto overtonesParamId     = "overtones";
    constexpr auto pulseWidthParamId  = "pulseWidth";

    constexpr float waveformSegment = 1.0f / 3.0f;

    void wrapPhase (double& phase)
    {
        while (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;
    }

    juce::String waveformMorphToString (float morph)
    {
        morph = juce::jlimit (0.0f, 1.0f, morph);

        if (morph < waveformSegment)
        {
            const auto blend = morph / waveformSegment;
            return juce::String (blend < 0.5f ? "Sine" : "Sine/Tri");
        }

        if (morph < 2.0f * waveformSegment)
        {
            const auto blend = (morph - waveformSegment) / waveformSegment;
            if (blend < 0.5f)
                return "Triangle";
            return "Tri/Saw";
        }

        const auto blend = (morph - 2.0f * waveformSegment) / waveformSegment;
        return juce::String (blend < 0.5f ? "Saw" : "Saw/Sq");
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout NafTachyonAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { attackParamId, 1 },
        "Attack",
        juce::NormalisableRange<float> { 0.001f, 2.0f, 0.001f, 0.4f },
        0.01f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { decayParamId, 1 },
        "Decay",
        juce::NormalisableRange<float> { 0.001f, 2.0f, 0.001f, 0.4f },
        0.2f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { sustainParamId, 1 },
        "Sustain",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.7f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { releaseParamId, 1 },
        "Release",
        juce::NormalisableRange<float> { 0.001f, 3.0f, 0.001f, 0.4f },
        0.3f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { waveformParamId, 1 },
        "Waveform",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("")
            .withStringFromValueFunction ([] (float value, int) { return waveformMorphToString (value); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pulseWidthParamId, 1 },
        "Pulse Width",
        juce::NormalisableRange<float> { -1.0f, 1.0f, 0.001f },
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { overtonesParamId, 1 },
        "Overtones",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { filterCutoffParamId, 1 },
        "Cutoff",
        juce::NormalisableRange<float> { 20.0f, 20000.0f, 0.0f, 0.3f },
        20000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { filterResonanceParamId, 1 },
        "Resonance",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("")));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { filterSlopeParamId, 1 },
        "Filter slope",
        juce::StringArray { "6 dB", "12 dB", "24 dB" },
        0));

    ModEnvelopeParamIds::addParameters (layout);

    return layout;
}

//==============================================================================
NafTachyonAudioProcessor::NafTachyonAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
       apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

NafTachyonAudioProcessor::~NafTachyonAudioProcessor()
{
}

//==============================================================================
const juce::String NafTachyonAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NafTachyonAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NafTachyonAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NafTachyonAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NafTachyonAudioProcessor::getTailLengthSeconds() const
{
    return 3.0;
}

int NafTachyonAudioProcessor::getNumPrograms()
{
    return 1;
}

int NafTachyonAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NafTachyonAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String NafTachyonAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void NafTachyonAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void NafTachyonAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate;
    globalSampleCounter = 0;
    releaseAllVoices (0.0f);

    constexpr auto filterSmoothingSeconds = 0.03;

    filterCutoffSmoother.reset (sampleRate, filterSmoothingSeconds);
    filterResonanceSmoother.reset (sampleRate, filterSmoothingSeconds);
    overtonesSmoother.reset (sampleRate, filterSmoothingSeconds);
    pulseWidthSmoother.reset (sampleRate, filterSmoothingSeconds);

    if (auto* cutoff = apvts.getRawParameterValue (filterCutoffParamId))
        filterCutoffSmoother.setCurrentAndTargetValue (cutoff->load());

    if (auto* resonance = apvts.getRawParameterValue (filterResonanceParamId))
        filterResonanceSmoother.setCurrentAndTargetValue (resonance->load());

    if (auto* overtones = apvts.getRawParameterValue (overtonesParamId))
        overtonesSmoother.setCurrentAndTargetValue (overtones->load());

    if (auto* pulseWidth = apvts.getRawParameterValue (pulseWidthParamId))
        pulseWidthSmoother.setCurrentAndTargetValue (pulseWidth->load());
}

void NafTachyonAudioProcessor::releaseResources()
{
    releaseAllVoices (0.0f);
}

void NafTachyonAudioProcessor::resetVoiceFilter (OscillatorVoice& voice)
{
    voice.onePoleState = 0.0f;
    voice.biquad1Z1 = 0.0f;
    voice.biquad1Z2 = 0.0f;
    voice.biquad2Z1 = 0.0f;
    voice.biquad2Z2 = 0.0f;
}

NafTachyonAudioProcessor::FilterCoefficients NafTachyonAudioProcessor::makeFilterCoefficients (float cutoffHz,
                                                                                                 float resonance,
                                                                                                 FilterSlope slope) const
{
    FilterCoefficients coeffs;

    const auto frequency = juce::jlimit (20.0f,
                                         static_cast<float> (currentSampleRate * 0.49f),
                                         cutoffHz);

    coeffs.onePoleCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * frequency
                                           / static_cast<float> (currentSampleRate));

    if (slope == FilterSlope::sixDb)
        return coeffs;

    const auto q = juce::jmap (resonance, 0.5f, 12.0f);
    const auto w0 = juce::MathConstants<float>::twoPi * frequency / static_cast<float> (currentSampleRate);
    const auto cosW0 = std::cos (w0);
    const auto sinW0 = std::sin (w0);
    const auto alpha = sinW0 / (2.0f * q);
    const auto a0 = 1.0f + alpha;

    coeffs.b0 = (1.0f - cosW0) / (2.0f * a0);
    coeffs.b1 = (1.0f - cosW0) / a0;
    coeffs.b2 = coeffs.b0;
    coeffs.a1 = (-2.0f * cosW0) / a0;
    coeffs.a2 = (1.0f - alpha) / a0;

    return coeffs;
}

float NafTachyonAudioProcessor::filterSample (float input,
                                                OscillatorVoice& voice,
                                                const FilterCoefficients& coeffs,
                                                FilterSlope slope) const
{
    if (slope == FilterSlope::sixDb)
    {
        voice.onePoleState += coeffs.onePoleCoeff * (input - voice.onePoleState);
        return voice.onePoleState;
    }

    auto processBiquad = [&] (float sample, float& z1, float& z2) -> float
    {
        const auto output = coeffs.b0 * sample + z1;
        z1 = coeffs.b1 * sample - coeffs.a1 * output + z2;
        z2 = coeffs.b2 * sample - coeffs.a2 * output;
        return output;
    };

    auto sample = processBiquad (input, voice.biquad1Z1, voice.biquad1Z2);

    if (slope == FilterSlope::twentyFourDb)
        sample = processBiquad (sample, voice.biquad2Z1, voice.biquad2Z2);

    return sample;
}

void NafTachyonAudioProcessor::startVoice (int midiNote, int velocity)
{
    const auto knobSnapshot = ModEnvelopeParamIds::readKnobSnapshot (apvts);

    for (auto& voice : voices)
    {
        if (voice.isActive && voice.midiNote == midiNote)
        {
            voice.level = static_cast<float> (velocity) / 127.0f;
            voice.envelopeLevel = 0.0f;
            voice.stage = EnvelopeStage::attack;
            voice.phase = 0.0;
            voice.subPhase = 0.0;
            voice.fifthPhase = 0.0;
            voice.noteOnSample = globalSampleCounter;
            voice.modKnobSnapshot = knobSnapshot;
            resetVoiceFilter (voice);
            return;
        }
    }

    for (auto& voice : voices)
    {
        if (! voice.isActive)
        {
            const auto frequency = juce::MidiMessage::getMidiNoteInHertz (midiNote);

            voice.isActive = true;
            voice.midiNote = midiNote;
            voice.noteOnSample = globalSampleCounter;
            voice.modKnobSnapshot = knobSnapshot;
            voice.phase = 0.0;
            voice.subPhase = 0.0;
            voice.fifthPhase = 0.0;
            voice.level = static_cast<float> (velocity) / 127.0f;
            voice.envelopeLevel = 0.0f;
            voice.stage = EnvelopeStage::attack;
            voice.phaseIncrement = juce::MathConstants<double>::twoPi * frequency / currentSampleRate;
            voice.subPhaseIncrement = voice.phaseIncrement * 0.5;
            voice.fifthPhaseIncrement = voice.phaseIncrement * WaveformSynth::perfectFifthRatio;
            resetVoiceFilter (voice);
            return;
        }
    }
}

void NafTachyonAudioProcessor::releaseVoice (int midiNote, float releaseSeconds)
{
    for (auto& voice : voices)
    {
        if (voice.isActive && voice.midiNote == midiNote && voice.stage != EnvelopeStage::release)
        {
            voice.stage = EnvelopeStage::release;
            const auto releaseSamples = juce::jmax (releaseSeconds * static_cast<float> (currentSampleRate), 1.0f);
            voice.releaseIncrement = voice.envelopeLevel / releaseSamples;
        }
    }
}

void NafTachyonAudioProcessor::releaseAllVoices (float releaseSeconds)
{
    for (auto& voice : voices)
    {
        if (! voice.isActive)
            continue;

        if (releaseSeconds <= 0.0f)
        {
            voice.isActive = false;
            voice.stage = EnvelopeStage::idle;
            voice.envelopeLevel = 0.0f;
            resetVoiceFilter (voice);
            continue;
        }

        if (voice.stage != EnvelopeStage::release)
        {
            voice.stage = EnvelopeStage::release;
            const auto releaseSamples = juce::jmax (releaseSeconds * static_cast<float> (currentSampleRate), 1.0f);
            voice.releaseIncrement = voice.envelopeLevel / releaseSamples;
        }
    }
}

void NafTachyonAudioProcessor::advanceEnvelope (OscillatorVoice& voice,
                                                  float attackDelta,
                                                  float decayDelta,
                                                  float sustainLevel)
{
    switch (voice.stage)
    {
        case EnvelopeStage::attack:
            voice.envelopeLevel += attackDelta;

            if (voice.envelopeLevel >= 1.0f)
            {
                voice.envelopeLevel = 1.0f;
                voice.stage = EnvelopeStage::decay;
            }
            break;

        case EnvelopeStage::decay:
            if (decayDelta <= 0.0f)
            {
                voice.envelopeLevel = sustainLevel;
                voice.stage = EnvelopeStage::sustain;
                break;
            }

            voice.envelopeLevel -= decayDelta;

            if (voice.envelopeLevel <= sustainLevel)
            {
                voice.envelopeLevel = sustainLevel;
                voice.stage = EnvelopeStage::sustain;
            }
            break;

        case EnvelopeStage::sustain:
            voice.envelopeLevel = sustainLevel;
            break;

        case EnvelopeStage::release:
            voice.envelopeLevel -= voice.releaseIncrement;

            if (voice.envelopeLevel <= 0.0f)
            {
                voice.envelopeLevel = 0.0f;
                voice.stage = EnvelopeStage::idle;
                voice.isActive = false;
            }
            break;

        case EnvelopeStage::idle:
            break;
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NafTachyonAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void NafTachyonAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    const auto attackSeconds  = apvts.getRawParameterValue (attackParamId)->load();
    const auto decaySeconds   = apvts.getRawParameterValue (decayParamId)->load();
    const auto sustainLevel   = apvts.getRawParameterValue (sustainParamId)->load();
    const auto releaseSeconds = apvts.getRawParameterValue (releaseParamId)->load();
    const auto waveformMorphKnob = apvts.getRawParameterValue (waveformParamId)->load();
    const auto pulseWidthKnob = apvts.getRawParameterValue (pulseWidthParamId)->load();
    const auto overtonesKnob = apvts.getRawParameterValue (overtonesParamId)->load();
    const auto cutoffKnob = apvts.getRawParameterValue (filterCutoffParamId)->load();
    const auto resonanceKnob = apvts.getRawParameterValue (filterResonanceParamId)->load();

    modulationEnvelope.updateFromApvts (apvts);

    const auto shapeModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::shape, apvts);
    const auto widthModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::width, apvts);
    const auto overtonesModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::overtones, apvts);
    const auto cutoffModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::cutoff, apvts);
    const auto resonanceModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::resonance, apvts);

    if (! widthModEnabled)
        pulseWidthSmoother.setTargetValue (pulseWidthKnob);

    if (! overtonesModEnabled)
        overtonesSmoother.setTargetValue (overtonesKnob);

    if (! cutoffModEnabled)
        filterCutoffSmoother.setTargetValue (cutoffKnob);

    if (! resonanceModEnabled)
        filterResonanceSmoother.setTargetValue (resonanceKnob);

    const auto anyLaneModEnabled = shapeModEnabled || widthModEnabled || overtonesModEnabled
                               || cutoffModEnabled || resonanceModEnabled;

    const auto filterSlopeIndex = juce::jlimit (0, 2, static_cast<int> (std::round (apvts.getRawParameterValue (filterSlopeParamId)->load())));
    const auto filterSlope = static_cast<FilterSlope> (filterSlopeIndex);

    const auto attackSamples = juce::jmax (attackSeconds * static_cast<float> (currentSampleRate), 1.0f);
    const auto decaySamples  = juce::jmax (decaySeconds  * static_cast<float> (currentSampleRate), 1.0f);
    const auto attackDelta   = 1.0f / attackSamples;
    const auto decayDelta    = (1.0f - sustainLevel) / decaySamples;

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
            startVoice (message.getNoteNumber(), message.getVelocity());
        else if (message.isNoteOff())
            releaseVoice (message.getNoteNumber(), releaseSeconds);
        else if (message.isAllNotesOff())
            releaseAllVoices (releaseSeconds);
        else if (message.isAllSoundOff())
            releaseAllVoices (0.0f);
    }

    const auto numSamples = buffer.getNumSamples();
    const auto numOutputChannels = getTotalNumOutputChannels();
    constexpr float voiceGain = 0.2f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float outputSample = 0.0f;

        for (auto& voice : voices)
        {
            if (! voice.isActive)
                continue;

            advanceEnvelope (voice, attackDelta, decayDelta, sustainLevel);

            auto waveformMorph = waveformMorphKnob;
            auto pulseWidth = pulseWidthKnob;
            auto overtones = overtonesKnob;
            auto cutoffHz = cutoffKnob;
            auto resonance = resonanceKnob;

            if (anyLaneModEnabled)
            {
                const auto elapsedSeconds = static_cast<float> (globalSampleCounter + sample - voice.noteOnSample)
                                          / static_cast<float> (currentSampleRate);
                const auto modParams = modulationEnvelope.evaluate (elapsedSeconds, voice.modKnobSnapshot);

                if (shapeModEnabled)
                    waveformMorph = modParams.shape;

                if (widthModEnabled)
                    pulseWidth = modParams.pulseWidth;

                if (overtonesModEnabled)
                    overtones = modParams.overtones;

                if (cutoffModEnabled)
                    cutoffHz = modParams.cutoffHz;

                if (resonanceModEnabled)
                    resonance = modParams.resonance;
            }

            if (! widthModEnabled)
                pulseWidth = pulseWidthSmoother.getNextValue();

            if (! overtonesModEnabled)
                overtones = overtonesSmoother.getNextValue();

            if (! cutoffModEnabled)
                cutoffHz = filterCutoffSmoother.getNextValue();

            if (! resonanceModEnabled)
                resonance = filterResonanceSmoother.getNextValue();

            const auto oscSample = WaveformSynth::computeOscillatorSample (voice.phase,
                                                                           voice.subPhase,
                                                                           voice.fifthPhase,
                                                                           waveformMorph,
                                                                           pulseWidth,
                                                                           overtones)
                                 * voice.level
                                 * voice.envelopeLevel
                                 * voiceGain;

            const auto filterCoeffs = makeFilterCoefficients (cutoffHz, resonance, filterSlope);

            outputSample += filterSample (oscSample, voice, filterCoeffs, filterSlope);

            voice.phase += voice.phaseIncrement;
            voice.subPhase += voice.subPhaseIncrement;
            voice.fifthPhase += voice.fifthPhaseIncrement;
            wrapPhase (voice.phase);
            wrapPhase (voice.subPhase);
            wrapPhase (voice.fifthPhase);
        }

        for (int channel = 0; channel < numOutputChannels; ++channel)
            buffer.setSample (channel, sample, outputSample);
    }

    globalSampleCounter += numSamples;
}

//==============================================================================
bool NafTachyonAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NafTachyonAudioProcessor::createEditor()
{
    return new NafTachyonAudioProcessorEditor (*this);
}

//==============================================================================
void NafTachyonAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NafTachyonAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NafTachyonAudioProcessor();
}
