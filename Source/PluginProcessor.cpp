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
    constexpr auto amplitudeParamId     = "amplitude";
    constexpr auto releaseTimeParamId   = "releaseTime";
    constexpr auto waveformParamId      = "waveform";
    constexpr auto filterCutoffParamId  = "filterCutoff";
    constexpr auto filterResonanceParamId = "filterResonance";
    constexpr auto filterSlopeParamId   = "filterSlope";
    constexpr auto overtonesParamId     = "overtones";
    constexpr auto pulseWidthParamId  = "pulseWidth";
    constexpr auto unisonParamId        = "unison";
    constexpr auto unisonSpreadParamId  = "unisonSpread";

    constexpr int unisonStackSize = 5;

    struct UnisonSettings
    {
        int count = 1;
        float normalise = 1.0f;
        float detuneCents[unisonStackSize] {};
    };

    constexpr float unisonOffThreshold = 0.02f;

    int unisonVoicesToCount (float unisonVoices)
    {
        unisonVoices = juce::jlimit (0.0f, 1.0f, unisonVoices);

        if (unisonVoices < unisonOffThreshold)
            return 1;

        const auto t = (unisonVoices - unisonOffThreshold) / (1.0f - unisonOffThreshold);
        const auto step = juce::jmin (3, static_cast<int> (std::floor (t * 4.0f)));
        return 2 + step;
    }

    UnisonSettings calcUnisonSettings (float unisonVoices, float unisonSpread)
    {
        UnisonSettings settings;
        unisonVoices = juce::jlimit (0.0f, 1.0f, unisonVoices);
        unisonSpread = juce::jlimit (0.0f, 1.0f, unisonSpread);

        settings.count = unisonVoicesToCount (unisonVoices);

        if (settings.count <= 1)
            return settings;

        const auto spreadCents = unisonSpread * 40.0f;

        for (int i = 0; i < settings.count; ++i)
        {
            const auto position = settings.count > 1
                                ? static_cast<float> (i) / static_cast<float> (settings.count - 1)
                                : 0.5f;
            settings.detuneCents[i] = juce::jmap (position, -spreadCents, spreadCents);
        }

        settings.normalise = 1.0f / std::sqrt (static_cast<float> (settings.count));
        return settings;
    }

    constexpr float waveformSegment = 1.0f / 3.0f;

    /** ~60 dB decay over releaseTimeSeconds. */
    float exponentialReleaseGain (float releaseElapsedSeconds, float releaseTimeSeconds)
    {
        if (releaseTimeSeconds <= 0.001f)
            return 0.0f;

        const auto gain = std::exp (-releaseElapsedSeconds * 6.90775527898f / releaseTimeSeconds);
        return gain < 1.0e-4f ? 0.0f : gain;
    }

    void wrapPhase (double& phase)
    {
        while (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;
    }

    constexpr float vibratoRateHz = 5.5f;
    constexpr float maxVibratoCents = 50.0f;
    /** Standard MIDI pitch-bend wheel range (±2 semitones). */
    constexpr float pitchBendRangeSemitones = 2.0f;
    double vibratoPitchRatio (double lfoPhase, float modWheelDepth)
    {
        const auto cents = std::sin (lfoPhase) * modWheelDepth * maxVibratoCents;
        return std::pow (2.0, cents / 1200.0);
    }

    float normalisedPitchWheel (int pitchWheelValue)
    {
        return juce::jlimit (-1.0f,
                             1.0f,
                             static_cast<float> (pitchWheelValue - 8192) / 8192.0f);
    }

    double pitchBendRatio (float pitchWheelNormalised)
    {
        return std::pow (2.0, static_cast<double> (pitchWheelNormalised * pitchBendRangeSemitones) / 12.0);
    }

    juce::NormalisableRange<float> makeFilterCutoffRange()
    {
        juce::NormalisableRange<float> range { 20.0f, 20000.0f };
        range.setSkewForCentre (std::sqrt (20.0f * 20000.0f));
        return range;
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
                return "Tri";
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
        juce::ParameterID { amplitudeParamId, 1 },
        "Amplitude",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { releaseTimeParamId, 1 },
        "Release Time",
        juce::NormalisableRange<float> { 0.0f, 5.0f, 0.001f, 0.35f },
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
        "Width",
        juce::NormalisableRange<float> { -1.0f, 1.0f, 0.001f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (juce::roundToInt (value * 100.0f)) + "%";
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { overtonesParamId, 1 },
        "Harmonics",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { filterCutoffParamId, 1 },
        "Cutoff",
        makeFilterCutoffRange(),
        20000.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 2);
            })));

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

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { unisonParamId, 1 },
        "Voices",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                const auto voices = unisonVoicesToCount (value);

                if (voices <= 1)
                    return juce::String ("Off");

                return juce::String (voices) + " voices";
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { unisonSpreadParamId, 1 },
        "Amount",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                juce::ignoreUnused (value);

                if (value < 0.01f)
                    return juce::String ("Off");

                return juce::String (juce::roundToInt (value * 40.0f)) + " ct";
            })));

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
    if (auto* releaseTime = apvts.getRawParameterValue (releaseTimeParamId))
        return static_cast<double> (releaseTime->load()) + 0.05;

    return 0.0;
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
    releaseAllVoices (true);

    constexpr auto filterSmoothingSeconds = 0.03;

    filterCutoffSmoother.reset (sampleRate, filterSmoothingSeconds);
    filterResonanceSmoother.reset (sampleRate, filterSmoothingSeconds);
    overtonesSmoother.reset (sampleRate, filterSmoothingSeconds);
    pulseWidthSmoother.reset (sampleRate, filterSmoothingSeconds);
    amplitudeSmoother.reset (sampleRate, filterSmoothingSeconds);
    modWheelSmoother.reset (sampleRate, 0.05);
    pitchBendSmoother.reset (sampleRate, 0.01);
    pitchBendSmoother.setCurrentAndTargetValue (0.0f);

    if (auto* cutoff = apvts.getRawParameterValue (filterCutoffParamId))
        filterCutoffSmoother.setCurrentAndTargetValue (cutoff->load());

    if (auto* resonance = apvts.getRawParameterValue (filterResonanceParamId))
        filterResonanceSmoother.setCurrentAndTargetValue (resonance->load());

    if (auto* overtones = apvts.getRawParameterValue (overtonesParamId))
        overtonesSmoother.setCurrentAndTargetValue (overtones->load());

    if (auto* pulseWidth = apvts.getRawParameterValue (pulseWidthParamId))
        pulseWidthSmoother.setCurrentAndTargetValue (pulseWidth->load());

    if (auto* amplitude = apvts.getRawParameterValue (amplitudeParamId))
        amplitudeSmoother.setCurrentAndTargetValue (amplitude->load());

    updateDcHighpassCoefficients();
    resetDcHighpassState();
}

void NafTachyonAudioProcessor::releaseResources()
{
    releaseAllVoices (true);
    resetDcHighpassState();
}

void NafTachyonAudioProcessor::updateDcHighpassCoefficients()
{
    constexpr auto cutoffHz = 20.0f;
    constexpr auto q = 0.70710678118f;

    const auto w0 = juce::MathConstants<float>::twoPi * cutoffHz
                    / static_cast<float> (currentSampleRate);
    const auto cosW0 = std::cos (w0);
    const auto sinW0 = std::sin (w0);
    const auto alpha = sinW0 / (2.0f * q);
    const auto a0 = 1.0f + alpha;

    dcHighpassCoeffs.b0 = (1.0f + cosW0) / (2.0f * a0);
    dcHighpassCoeffs.b1 = -(1.0f + cosW0) / a0;
    dcHighpassCoeffs.b2 = (1.0f + cosW0) / (2.0f * a0);
    dcHighpassCoeffs.a1 = (-2.0f * cosW0) / a0;
    dcHighpassCoeffs.a2 = (1.0f - alpha) / a0;
}

void NafTachyonAudioProcessor::resetDcHighpassState()
{
    dcHighpassZ1[0] = dcHighpassZ1[1] = 0.0f;
    dcHighpassZ2[0] = dcHighpassZ2[1] = 0.0f;
}

void NafTachyonAudioProcessor::applyDcHighpass (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const auto numSamples = buffer.getNumSamples();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* samples = buffer.getWritePointer (channel);
        auto z1 = dcHighpassZ1[channel];
        auto z2 = dcHighpassZ2[channel];

        for (int i = 0; i < numSamples; ++i)
        {
            const auto input = samples[i];
            const auto output = dcHighpassCoeffs.b0 * input + z1;
            z1 = dcHighpassCoeffs.b1 * input - dcHighpassCoeffs.a1 * output + z2;
            z2 = dcHighpassCoeffs.b2 * input - dcHighpassCoeffs.a2 * output;
            samples[i] = output;
        }

        dcHighpassZ1[channel] = z1;
        dcHighpassZ2[channel] = z2;
    }
}

void NafTachyonAudioProcessor::resetVoiceFilter (OscillatorVoice& voice)
{
    voice.onePoleState = 0.0f;
    voice.biquad1Z1 = 0.0f;
    voice.biquad1Z2 = 0.0f;
    voice.biquad2Z1 = 0.0f;
    voice.biquad2Z2 = 0.0f;
}

void NafTachyonAudioProcessor::resetUnisonPhases (OscillatorVoice& voice)
{
    for (int i = 0; i < maxUnisonStack; ++i)
    {
        voice.unisonPhase[i] = 0.0;
        voice.unisonSubPhase[i] = 0.0;
        voice.unisonFifthPhase[i] = 0.0;
    }
}

void NafTachyonAudioProcessor::updateUnisonIncrements (OscillatorVoice& voice,
                                                       float unisonVoices,
                                                       float unisonSpread)
{
    const auto settings = calcUnisonSettings (unisonVoices, unisonSpread);
    const auto baseIncrement = voice.phaseIncrement;

    for (int i = 0; i < maxUnisonStack; ++i)
    {
        const auto detuneRatio = i < settings.count
                               ? std::pow (2.0, settings.detuneCents[i] / 1200.0)
                               : 1.0;

        voice.unisonPhaseIncrement[i] = baseIncrement * detuneRatio;
        voice.unisonSubPhaseIncrement[i] = voice.unisonPhaseIncrement[i] * 0.5;
        voice.unisonFifthPhaseIncrement[i] = voice.unisonPhaseIncrement[i] * WaveformSynth::perfectFifthRatio;
    }
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

    const auto resonanceShaped = resonance * resonance * resonance;

    if (slope == FilterSlope::sixDb)
    {
        coeffs.sixDbResonantBlend = resonanceShaped;

        if (resonanceShaped < 0.001f)
            return coeffs;
    }

    const auto q = juce::jmap (resonanceShaped, 0.5f, 12.0f);
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
        const auto onePoleOut = voice.onePoleState;

        if (coeffs.sixDbResonantBlend < 0.001f)
            return onePoleOut;

        auto processBiquad = [&] (float sample, float& z1, float& z2) -> float
        {
            const auto output = coeffs.b0 * sample + z1;
            z1 = coeffs.b1 * sample - coeffs.a1 * output + z2;
            z2 = coeffs.b2 * sample - coeffs.a2 * output;
            return output;
        };

        const auto biquadOut = processBiquad (input, voice.biquad1Z1, voice.biquad1Z2);
        return juce::jmap (coeffs.sixDbResonantBlend, onePoleOut, biquadOut);
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
            voice.inRelease = false;
            voice.phase = 0.0;
            voice.subPhase = 0.0;
            voice.fifthPhase = 0.0;
            resetUnisonPhases (voice);
            voice.noteOnSample = globalSampleCounter;
            voice.modKnobSnapshot = knobSnapshot;
            updateUnisonIncrements (voice,
                                    apvts.getRawParameterValue (unisonParamId)->load(),
                                    apvts.getRawParameterValue (unisonSpreadParamId)->load());
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
            voice.inRelease = false;
            voice.noteOnSample = globalSampleCounter;
            voice.modKnobSnapshot = knobSnapshot;
            voice.phase = 0.0;
            voice.subPhase = 0.0;
            voice.fifthPhase = 0.0;
            resetUnisonPhases (voice);
            voice.level = static_cast<float> (velocity) / 127.0f;
            voice.phaseIncrement = juce::MathConstants<double>::twoPi * frequency / currentSampleRate;
            voice.subPhaseIncrement = voice.phaseIncrement * 0.5;
            voice.fifthPhaseIncrement = voice.phaseIncrement * WaveformSynth::perfectFifthRatio;
            updateUnisonIncrements (voice,
                                    apvts.getRawParameterValue (unisonParamId)->load(),
                                    apvts.getRawParameterValue (unisonSpreadParamId)->load());
            resetVoiceFilter (voice);
            return;
        }
    }
}

float NafTachyonAudioProcessor::getVoiceAmplitudeForRelease (const OscillatorVoice& voice)
{
    const auto noteOnElapsed = static_cast<float> (globalSampleCounter - voice.noteOnSample)
                             / static_cast<float> (currentSampleRate);

    if (modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::amplitude, apvts))
        return modulationEnvelope.evaluate (noteOnElapsed, voice.modKnobSnapshot).amplitude;

    return apvts.getRawParameterValue (amplitudeParamId)->load();
}

void NafTachyonAudioProcessor::beginVoiceRelease (OscillatorVoice& voice, bool immediate)
{
    if (! voice.isActive)
        return;

    const auto releaseTimeSeconds = apvts.getRawParameterValue (releaseTimeParamId)->load();

    if (immediate || releaseTimeSeconds <= 0.001f)
    {
        voice.isActive = false;
        voice.inRelease = false;
        resetVoiceFilter (voice);
        return;
    }

    if (voice.inRelease)
        return;

    voice.releaseStartAmplitude = getVoiceAmplitudeForRelease (voice);
    voice.inRelease = true;
    voice.noteOffSample = globalSampleCounter;
}

void NafTachyonAudioProcessor::releaseVoice (int midiNote)
{
    for (auto& voice : voices)
    {
        if (voice.isActive && voice.midiNote == midiNote)
            beginVoiceRelease (voice, false);
    }
}

void NafTachyonAudioProcessor::releaseAllVoices (bool immediate)
{
    for (auto& voice : voices)
        beginVoiceRelease (voice, immediate);
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

    const auto amplitudeKnob = apvts.getRawParameterValue (amplitudeParamId)->load();
    const auto waveformMorphKnob = apvts.getRawParameterValue (waveformParamId)->load();
    const auto pulseWidthKnob = apvts.getRawParameterValue (pulseWidthParamId)->load();
    const auto overtonesKnob = apvts.getRawParameterValue (overtonesParamId)->load();
    const auto cutoffKnob = apvts.getRawParameterValue (filterCutoffParamId)->load();
    const auto resonanceKnob = apvts.getRawParameterValue (filterResonanceParamId)->load();
    const auto unisonVoices = apvts.getRawParameterValue (unisonParamId)->load();
    const auto unisonSpread = apvts.getRawParameterValue (unisonSpreadParamId)->load();
    const auto unisonSettings = calcUnisonSettings (unisonVoices, unisonSpread);

    modulationEnvelope.updateFromApvts (apvts);

    const auto shapeModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::shape, apvts);
    const auto widthModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::width, apvts);
    const auto overtonesModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::overtones, apvts);
    const auto cutoffModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::cutoff, apvts);
    const auto resonanceModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::resonance, apvts);
    const auto amplitudeModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::amplitude, apvts);

    if (! widthModEnabled)
        pulseWidthSmoother.setTargetValue (pulseWidthKnob);

    if (! overtonesModEnabled)
        overtonesSmoother.setTargetValue (overtonesKnob);

    if (! cutoffModEnabled)
        filterCutoffSmoother.setTargetValue (cutoffKnob);

    if (! resonanceModEnabled)
        filterResonanceSmoother.setTargetValue (resonanceKnob);

    if (! amplitudeModEnabled)
        amplitudeSmoother.setTargetValue (amplitudeKnob);

    const auto anyLaneModEnabled = shapeModEnabled || widthModEnabled || overtonesModEnabled
                               || cutoffModEnabled || resonanceModEnabled || amplitudeModEnabled;

    const auto filterSlopeIndex = juce::jlimit (0, 2, static_cast<int> (std::round (apvts.getRawParameterValue (filterSlopeParamId)->load())));
    const auto filterSlope = static_cast<FilterSlope> (filterSlopeIndex);
    const auto releaseTimeSeconds = apvts.getRawParameterValue (releaseTimeParamId)->load();

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
            startVoice (message.getNoteNumber(), message.getVelocity());
        else if (message.isNoteOff())
            releaseVoice (message.getNoteNumber());
        else if (message.isAllNotesOff())
            releaseAllVoices (false);
        else if (message.isAllSoundOff())
            releaseAllVoices (true);
        else if (message.isController())
        {
            if (message.getControllerNumber() == 1)
                modWheelSmoother.setTargetValue (message.getControllerValue() / 127.0f);
        }
        else if (message.isPitchWheel())
            pitchBendSmoother.setTargetValue (normalisedPitchWheel (message.getPitchWheelValue()));
    }

    const auto numSamples = buffer.getNumSamples();
    const auto numOutputChannels = getTotalNumOutputChannels();
    constexpr float voiceGain = 0.2f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto modWheelDepth = modWheelSmoother.getNextValue();
        const auto pitchWheelNormalised = pitchBendSmoother.getNextValue();
        const auto pitchRatio = vibratoPitchRatio (vibratoLfoPhase, modWheelDepth)
                              * pitchBendRatio (pitchWheelNormalised);

        vibratoLfoPhase += juce::MathConstants<double>::twoPi * static_cast<double> (vibratoRateHz)
                         / currentSampleRate;
        wrapPhase (vibratoLfoPhase);

        float outputSample = 0.0f;

        for (auto& voice : voices)
        {
            if (! voice.isActive)
                continue;

            float releaseGain = 1.0f;

            if (voice.inRelease)
            {
                const auto releaseElapsed = static_cast<float> (globalSampleCounter + sample - voice.noteOffSample)
                                          / static_cast<float> (currentSampleRate);
                releaseGain = exponentialReleaseGain (releaseElapsed, releaseTimeSeconds);

                if (releaseGain <= 0.0f)
                {
                    voice.isActive = false;
                    voice.inRelease = false;
                    resetVoiceFilter (voice);
                    continue;
                }
            }

            auto waveformMorph = waveformMorphKnob;
            auto pulseWidth = pulseWidthKnob;
            auto overtones = overtonesKnob;
            auto cutoffHz = cutoffKnob;
            auto resonance = resonanceKnob;
            auto amplitude = amplitudeKnob;

            if (anyLaneModEnabled)
            {
                const auto elapsedSeconds = static_cast<float> (globalSampleCounter + sample - voice.noteOnSample)
                                          / static_cast<float> (currentSampleRate);
                auto knobSnapshot = voice.modKnobSnapshot;

                if (cutoffModEnabled)
                    knobSnapshot.cutoffHz = cutoffKnob;

                const auto modParams = modulationEnvelope.evaluate (elapsedSeconds, knobSnapshot);

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

                if (amplitudeModEnabled && ! voice.inRelease)
                    amplitude = modParams.amplitude;
            }

            if (voice.inRelease)
                amplitude = voice.releaseStartAmplitude * releaseGain;
            else if (! amplitudeModEnabled)
                amplitude = amplitudeSmoother.getNextValue();

            if (! widthModEnabled)
                pulseWidth = pulseWidthSmoother.getNextValue();

            if (! overtonesModEnabled)
                overtones = overtonesSmoother.getNextValue();

            if (! cutoffModEnabled)
                cutoffHz = filterCutoffSmoother.getNextValue();

            if (! resonanceModEnabled)
                resonance = filterResonanceSmoother.getNextValue();

            updateUnisonIncrements (voice, unisonVoices, unisonSpread);

            float oscSample = 0.0f;

            const auto mainPhaseInc = voice.phaseIncrement * pitchRatio;
            const auto subPhaseInc = voice.subPhaseIncrement * pitchRatio;
            const auto fifthPhaseInc = voice.fifthPhaseIncrement * pitchRatio;

            if (unisonSettings.count <= 1)
            {
                oscSample = WaveformSynth::computeOscillatorSample (voice.phase,
                                                                      mainPhaseInc,
                                                                      voice.subPhase,
                                                                      subPhaseInc,
                                                                      voice.fifthPhase,
                                                                      fifthPhaseInc,
                                                                      waveformMorph,
                                                                      pulseWidth,
                                                                      overtones);
            }
            else
            {
                for (int u = 0; u < unisonSettings.count; ++u)
                {
                    oscSample += WaveformSynth::computeOscillatorSample (voice.unisonPhase[u],
                                                                         voice.unisonPhaseIncrement[u] * pitchRatio,
                                                                         voice.unisonSubPhase[u],
                                                                         voice.unisonSubPhaseIncrement[u] * pitchRatio,
                                                                         voice.unisonFifthPhase[u],
                                                                         voice.unisonFifthPhaseIncrement[u] * pitchRatio,
                                                                         waveformMorph,
                                                                         pulseWidth,
                                                                         overtones);
                }

                oscSample *= unisonSettings.normalise;
            }

            oscSample *= voice.level * amplitude * voiceGain;

            const auto filterCoeffs = makeFilterCoefficients (cutoffHz, resonance, filterSlope);

            outputSample += filterSample (oscSample, voice, filterCoeffs, filterSlope);

            if (unisonSettings.count <= 1)
            {
                voice.phase += mainPhaseInc;
                voice.subPhase += subPhaseInc;
                voice.fifthPhase += fifthPhaseInc;
                wrapPhase (voice.phase);
                wrapPhase (voice.subPhase);
                wrapPhase (voice.fifthPhase);
            }
            else
            {
                for (int u = 0; u < unisonSettings.count; ++u)
                {
                    voice.unisonPhase[u] += voice.unisonPhaseIncrement[u] * pitchRatio;
                    voice.unisonSubPhase[u] += voice.unisonSubPhaseIncrement[u] * pitchRatio;
                    voice.unisonFifthPhase[u] += voice.unisonFifthPhaseIncrement[u] * pitchRatio;
                    wrapPhase (voice.unisonPhase[u]);
                    wrapPhase (voice.unisonSubPhase[u]);
                    wrapPhase (voice.unisonFifthPhase[u]);
                }
            }
        }

        if (numOutputChannels >= 2)
        {
            buffer.setSample (0, sample, outputSample);
            buffer.setSample (1, sample, outputSample);
        }
        else if (numOutputChannels == 1)
        {
            buffer.setSample (0, sample, outputSample);
        }
    }

    applyDcHighpass (buffer);

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
