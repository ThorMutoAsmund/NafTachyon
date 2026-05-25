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
    constexpr auto filterLimiterParamId = "filterLimiter";
    constexpr auto overtonesParamId     = "overtones";
    constexpr auto pitchTuneParamId     = "pitchTune";
    constexpr auto osc1PitchParamId     = "osc1Pitch";
    constexpr auto osc2WaveformParamId  = "osc2Waveform";
    constexpr auto osc2PulseWidthParamId = "osc2PulseWidth";
    constexpr auto osc2OvertonesParamId = "osc2Overtones";
    constexpr auto osc2PitchTuneParamId = "osc2PitchTune";
    constexpr auto osc2PitchParamId     = "osc2Pitch";
    constexpr auto oscMixParamId        = "oscMix";
    constexpr float maxOscPitchSemitones = 48.0f;
    constexpr auto oscSyncParamId        = "oscSync";
    constexpr auto pulseWidthParamId  = "pulseWidth";
    constexpr auto unisonParamId        = "unison";
    constexpr auto unisonSpreadParamId  = "unisonSpread";
    constexpr auto amplitudeVelSensitivityParamId = "amplitudeVelSensitivity";
    constexpr auto cutoffVelSensitivityParamId    = "cutoffVelSensitivity";

    constexpr float gainMinDb = -60.0f;
    constexpr float gainMaxDb = 6.0f;
    constexpr float gainMinLinear = 0.001f;
    const float gainMaxLinear = juce::Decibels::decibelsToGain (gainMaxDb);

    constexpr float maxCutoffVelocityOctaves = 4.0f;

    juce::String linearGainToDbString (float linearGain, int)
    {
        if (linearGain <= gainMinLinear * 1.01f)
            return juce::String (gainMinDb, 0) + " dB";

        return juce::String (juce::Decibels::gainToDecibels (linearGain), 1) + " dB";
    }

    void computeVoiceVelocityScales (float velocityNorm,
                                     float ampVelSensitivity,
                                     float cutoffVelSensitivity,
                                     float& outAmpScale,
                                     float& outCutoffScale)
    {
        outAmpScale = juce::jmap (juce::jlimit (0.0f, 1.0f, ampVelSensitivity),
                                  0.0f, 1.0f, 1.0f, velocityNorm);

        if (cutoffVelSensitivity <= 0.0f)
        {
            outCutoffScale = 1.0f;
            return;
        }

        const auto sensitivity = juce::jlimit (0.0f, 1.0f, cutoffVelSensitivity);
        const auto octaveSpan = maxCutoffVelocityOctaves * sensitivity;
        outCutoffScale = std::pow (2.0f, (velocityNorm - 1.0f) * octaveSpan);
    }

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

    double pitchFineTuneRatio (float cents)
    {
        return std::pow (2.0, static_cast<double> (cents) / 1200.0);
    }

    double pitchSemitoneRatio (float semitones)
    {
        return std::pow (2.0, static_cast<double> (semitones) / 12.0);
    }

    juce::String semitonePitchToString (float semitones, int)
    {
        const auto rounded = juce::roundToInt (semitones * 10.0f) / 10.0f;

        if (std::abs (rounded) < 0.05f)
            return "0 st";

        const auto sign = rounded > 0.0f ? "+" : "";
        return sign + juce::String (rounded, 1) + " st";
    }

    float mixOscillatorSamples (float osc1Sample, float osc2Sample, float mix)
    {
        const auto osc2Mix = juce::jlimit (0.0f, 1.0f, mix);
        const auto osc1Mix = 1.0f - osc2Mix;
        const auto sum = osc1Mix + osc2Mix;

        if (sum < 1.0e-5f)
            return 0.0f;

        return (osc1Sample * osc1Mix + osc2Sample * osc2Mix) / sum;
    }

    float computeOscLayerSample (double phase,
                                 double phaseIncrement,
                                 double subPhase,
                                 double subPhaseIncrement,
                                 double fifthPhase,
                                 double fifthPhaseIncrement,
                                 float waveformMorph,
                                 float pulseWidth,
                                 float overtones)
    {
        return WaveformSynth::computeOscillatorSample (phase,
                                                       phaseIncrement,
                                                       subPhase,
                                                       subPhaseIncrement,
                                                       fifthPhase,
                                                       fifthPhaseIncrement,
                                                       waveformMorph,
                                                       pulseWidth,
                                                       overtones);
    }

    void updateUnisonIncrementsForBase (double baseIncrement,
                                        double* unisonPhaseIncrement,
                                        double* unisonSubPhaseIncrement,
                                        double* unisonFifthPhaseIncrement,
                                        const UnisonSettings& settings)
    {
        for (int i = 0; i < unisonStackSize; ++i)
        {
            const auto detuneRatio = i < settings.count
                                   ? std::pow (2.0, settings.detuneCents[i] / 1200.0)
                                   : 1.0;

            unisonPhaseIncrement[i] = baseIncrement * detuneRatio;
            unisonSubPhaseIncrement[i] = unisonPhaseIncrement[i] * 0.5;
            unisonFifthPhaseIncrement[i] = unisonPhaseIncrement[i] * WaveformSynth::perfectFifthRatio;
        }
    }

    void advanceMasterPhase (double& phase, double increment, bool& wrappedOut)
    {
        phase += increment;

        if (phase >= juce::MathConstants<double>::twoPi)
        {
            phase -= juce::MathConstants<double>::twoPi;
            wrappedOut = true;
        }
        else
        {
            wrappedOut = false;
        }
    }

    void advanceFreeRunningPhases (double& phase,
                                   double& subPhase,
                                   double& fifthPhase,
                                   double mainInc,
                                   double subInc,
                                   double fifthInc)
    {
        phase += mainInc;
        subPhase += subInc;
        fifthPhase += fifthInc;

        while (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;

        while (subPhase >= juce::MathConstants<double>::twoPi)
            subPhase -= juce::MathConstants<double>::twoPi;

        while (fifthPhase >= juce::MathConstants<double>::twoPi)
            fifthPhase -= juce::MathConstants<double>::twoPi;
    }

    void syncSlavePhasesToMaster (double masterPhase,
                                  double masterSubPhase,
                                  double masterFifthPhase,
                                  double syncRatio,
                                  double& slavePhase,
                                  double& slaveSubPhase,
                                  double& slaveFifthPhase)
    {
        auto wrap = [] (double value)
        {
            while (value >= juce::MathConstants<double>::twoPi)
                value -= juce::MathConstants<double>::twoPi;

            while (value < 0.0)
                value += juce::MathConstants<double>::twoPi;

            return value;
        };

        slavePhase = wrap (masterPhase * syncRatio);
        slaveSubPhase = wrap (masterSubPhase * syncRatio);
        slaveFifthPhase = wrap (masterFifthPhase * syncRatio);
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

    {
        juce::NormalisableRange<float> gainRange { gainMinLinear, gainMaxLinear };
        gainRange.setSkewForCentre (1.0f);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { amplitudeParamId, 1 },
            "Gain",
            gainRange,
            1.0f,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction (linearGainToDbString)));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { amplitudeVelSensitivityParamId, 1 },
        "Amplitude velocity sensitivity",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (juce::roundToInt (value * 100.0f)) + "%";
            })));

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
        juce::ParameterID { osc1PitchParamId, 1 },
        "Oscillator 1 pitch",
        juce::NormalisableRange<float> { -maxOscPitchSemitones, maxOscPitchSemitones, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (semitonePitchToString)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pitchTuneParamId, 1 },
        "Oscillator 1 fine tune",
        juce::NormalisableRange<float> { -50.0f, 50.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                const auto rounded = juce::roundToInt (value * 10.0f) / 10.0f;

                if (std::abs (rounded) < 0.05f)
                    return juce::String ("0 ct");

                const auto sign = rounded > 0.0f ? "+" : "";
                return sign + juce::String (rounded, 1) + " ct";
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { osc2WaveformParamId, 1 },
        "Oscillator 2 waveform",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("")
            .withStringFromValueFunction ([] (float value, int) { return waveformMorphToString (value); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { osc2PulseWidthParamId, 1 },
        "Oscillator 2 width",
        juce::NormalisableRange<float> { -1.0f, 1.0f, 0.001f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (juce::roundToInt (value * 100.0f)) + "%";
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { osc2OvertonesParamId, 1 },
        "Oscillator 2 harmonics",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { osc2PitchParamId, 1 },
        "Oscillator 2 pitch",
        juce::NormalisableRange<float> { -maxOscPitchSemitones, maxOscPitchSemitones, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (semitonePitchToString)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { osc2PitchTuneParamId, 1 },
        "Oscillator 2 fine tune",
        juce::NormalisableRange<float> { -50.0f, 50.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                const auto rounded = juce::roundToInt (value * 10.0f) / 10.0f;

                if (std::abs (rounded) < 0.05f)
                    return juce::String ("0 ct");

                const auto sign = rounded > 0.0f ? "+" : "";
                return sign + juce::String (rounded, 1) + " ct";
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { oscMixParamId, 1 },
        "Oscillator mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (juce::roundToInt (value * 100.0f)) + "% 2";
            })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { oscSyncParamId, 1 },
        "Oscillator sync",
        false));

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
        juce::ParameterID { cutoffVelSensitivityParamId, 1 },
        "Cutoff velocity sensitivity",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (juce::roundToInt (value * 100.0f)) + "%";
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

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { filterLimiterParamId, 1 },
        "Filter limiter",
        juce::StringArray { "Off", "Light", "Normal", "Tight" },
        2));

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
    osc1PitchSmoother.reset (sampleRate, 0.02);
    osc1PitchSmoother.setCurrentAndTargetValue (0.0f);
    pitchTuneSmoother.reset (sampleRate, 0.02);
    pitchTuneSmoother.setCurrentAndTargetValue (0.0f);
    osc2PitchSmoother.reset (sampleRate, 0.02);
    osc2PitchSmoother.setCurrentAndTargetValue (0.0f);
    osc2PitchTuneSmoother.reset (sampleRate, 0.02);
    osc2PitchTuneSmoother.setCurrentAndTargetValue (0.0f);
    osc2PulseWidthSmoother.reset (sampleRate, filterSmoothingSeconds);
    osc2OvertonesSmoother.reset (sampleRate, filterSmoothingSeconds);
    oscMixSmoother.reset (sampleRate, filterSmoothingSeconds);

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

    if (auto* osc1Pitch = apvts.getRawParameterValue (osc1PitchParamId))
        osc1PitchSmoother.setCurrentAndTargetValue (osc1Pitch->load());

    if (auto* pitchTune = apvts.getRawParameterValue (pitchTuneParamId))
        pitchTuneSmoother.setCurrentAndTargetValue (pitchTune->load());

    if (auto* osc2Pitch = apvts.getRawParameterValue (osc2PitchParamId))
        osc2PitchSmoother.setCurrentAndTargetValue (osc2Pitch->load());

    if (auto* osc2PitchTune = apvts.getRawParameterValue (osc2PitchTuneParamId))
        osc2PitchTuneSmoother.setCurrentAndTargetValue (osc2PitchTune->load());

    if (auto* osc2PulseWidth = apvts.getRawParameterValue (osc2PulseWidthParamId))
        osc2PulseWidthSmoother.setCurrentAndTargetValue (osc2PulseWidth->load());

    if (auto* osc2Overtones = apvts.getRawParameterValue (osc2OvertonesParamId))
        osc2OvertonesSmoother.setCurrentAndTargetValue (osc2Overtones->load());

    if (auto* oscMix = apvts.getRawParameterValue (oscMixParamId))
        oscMixSmoother.setCurrentAndTargetValue (oscMix->load());

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
    voice.limiterEnvelope = 0.0f;
    voice.cachedFilterCutoff = -1.0f;
    voice.cachedFilterResonance = -1.0f;
}

void NafTachyonAudioProcessor::updateVoiceFilterCache (OscillatorVoice& voice,
                                                       float cutoffHz,
                                                       float resonance,
                                                       FilterSlope slope) const
{
    constexpr auto cutoffEpsilon = 5.0f;
    constexpr auto resonanceEpsilon = 0.002f;

    if (std::abs (cutoffHz - voice.cachedFilterCutoff) > cutoffEpsilon
        || std::abs (resonance - voice.cachedFilterResonance) > resonanceEpsilon
        || slope != voice.cachedFilterSlope)
    {
        voice.cachedFilterCoeffs = makeFilterCoefficients (cutoffHz, resonance, slope);
        voice.cachedFilterCutoff = cutoffHz;
        voice.cachedFilterResonance = resonance;
        voice.cachedFilterSlope = slope;
    }
}

void NafTachyonAudioProcessor::resetUnisonPhases (OscillatorVoice& voice)
{
    for (int i = 0; i < maxUnisonStack; ++i)
    {
        voice.unisonPhase[i] = 0.0;
        voice.unisonSubPhase[i] = 0.0;
        voice.unisonFifthPhase[i] = 0.0;
        voice.unisonPhase2[i] = 0.0;
        voice.unisonSubPhase2[i] = 0.0;
        voice.unisonFifthPhase2[i] = 0.0;
    }
}

void NafTachyonAudioProcessor::updateUnisonIncrements (OscillatorVoice& voice,
                                                       float unisonVoices,
                                                       float unisonSpread)
{
    const auto settings = calcUnisonSettings (unisonVoices, unisonSpread);

    updateUnisonIncrementsForBase (voice.phaseIncrement,
                                   voice.unisonPhaseIncrement,
                                   voice.unisonSubPhaseIncrement,
                                   voice.unisonFifthPhaseIncrement,
                                   settings);

    updateUnisonIncrementsForBase (voice.phase2Increment,
                                   voice.unisonPhase2Increment,
                                   voice.unisonSubPhase2Increment,
                                   voice.unisonFifthPhase2Increment,
                                   settings);
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

    if (slope == FilterSlope::twentyFourDb)
        coeffs.peakGainCompensation = 1.0f / (1.0f + resonanceShaped);

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
    {
        sample = processBiquad (sample, voice.biquad2Z1, voice.biquad2Z2);
        sample *= coeffs.peakGainCompensation;
    }

    return sample;
}

NafTachyonAudioProcessor::FilterLimiterCoeffs NafTachyonAudioProcessor::makeFilterLimiterCoeffs (FilterLimiterMode mode) const
{
    FilterLimiterCoeffs coeffs;

    float ceiling = 1.0f;
    float attackMs = 2.0f;
    float releaseMs = 80.0f;

    switch (mode)
    {
        case FilterLimiterMode::light:
            ceiling = 0.55f;
            attackMs = 3.0f;
            releaseMs = 110.0f;
            break;
        case FilterLimiterMode::normal:
            ceiling = 0.4f;
            attackMs = 1.5f;
            releaseMs = 75.0f;
            break;
        case FilterLimiterMode::tight:
            ceiling = 0.28f;
            attackMs = 0.8f;
            releaseMs = 55.0f;
            break;
        case FilterLimiterMode::off:
            return coeffs;
    }

    coeffs.active = true;
    coeffs.ceiling = ceiling;

    const auto attackSamples = static_cast<float> (attackMs * 0.001 * currentSampleRate);
    const auto releaseSamples = static_cast<float> (releaseMs * 0.001 * currentSampleRate);
    coeffs.attackCoeff = attackSamples > 1.0f ? std::exp (-1.0f / attackSamples) : 0.0f;
    coeffs.releaseCoeff = releaseSamples > 1.0f ? std::exp (-1.0f / releaseSamples) : 0.0f;

    return coeffs;
}

float NafTachyonAudioProcessor::applyFilterLimiter (float sample,
                                                    OscillatorVoice& voice,
                                                    const FilterLimiterCoeffs& coeffs) const
{
    if (! coeffs.active)
        return sample;

    const auto magnitude = std::abs (sample);

    if (magnitude > voice.limiterEnvelope)
        voice.limiterEnvelope = coeffs.attackCoeff * voice.limiterEnvelope + (1.0f - coeffs.attackCoeff) * magnitude;
    else
        voice.limiterEnvelope = coeffs.releaseCoeff * voice.limiterEnvelope + (1.0f - coeffs.releaseCoeff) * magnitude;

    constexpr auto epsilon = 1.0e-6f;
    auto gain = 1.0f;

    if (voice.limiterEnvelope > coeffs.ceiling)
        gain = coeffs.ceiling / (voice.limiterEnvelope + epsilon);

    return sample * gain;
}

void NafTachyonAudioProcessor::startVoice (int midiNote, int velocity)
{
    const auto knobSnapshot = ModEnvelopeParamIds::readKnobSnapshot (apvts);
    const auto velocityNorm = static_cast<float> (velocity) / 127.0f;
    const auto ampVelSensitivity = apvts.getRawParameterValue (amplitudeVelSensitivityParamId)->load();
    const auto cutoffVelSensitivity = apvts.getRawParameterValue (cutoffVelSensitivityParamId)->load();
    float ampVelScale = 1.0f;
    float cutoffVelScale = 1.0f;
    computeVoiceVelocityScales (velocityNorm, ampVelSensitivity, cutoffVelSensitivity, ampVelScale, cutoffVelScale);

    for (auto& voice : voices)
    {
        if (voice.isActive && voice.midiNote == midiNote)
        {
            voice.ampVelScale = ampVelScale;
            voice.cutoffVelScale = cutoffVelScale;
            voice.inRelease = false;
            voice.phase = 0.0;
            voice.subPhase = 0.0;
            voice.fifthPhase = 0.0;
            voice.phase2 = 0.0;
            voice.subPhase2 = 0.0;
            voice.fifthPhase2 = 0.0;
            resetUnisonPhases (voice);
            voice.noteOnSample = globalSampleCounter;
            voice.modKnobSnapshot = knobSnapshot;
            updateUnisonIncrements (voice,
                                    apvts.getRawParameterValue (unisonParamId)->load(),
                                    apvts.getRawParameterValue (unisonSpreadParamId)->load());
            resetVoiceFilter (voice);
            markModulationEnvelopeDirty();
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
            voice.phase2 = 0.0;
            voice.subPhase2 = 0.0;
            voice.fifthPhase2 = 0.0;
            resetUnisonPhases (voice);
            voice.ampVelScale = ampVelScale;
            voice.cutoffVelScale = cutoffVelScale;
            voice.phaseIncrement = juce::MathConstants<double>::twoPi * frequency / currentSampleRate;
            voice.subPhaseIncrement = voice.phaseIncrement * 0.5;
            voice.fifthPhaseIncrement = voice.phaseIncrement * WaveformSynth::perfectFifthRatio;
            voice.phase2Increment = voice.phaseIncrement;
            voice.subPhase2Increment = voice.subPhaseIncrement;
            voice.fifthPhase2Increment = voice.fifthPhaseIncrement;
            updateUnisonIncrements (voice,
                                    apvts.getRawParameterValue (unisonParamId)->load(),
                                    apvts.getRawParameterValue (unisonSpreadParamId)->load());
            resetVoiceFilter (voice);
            markModulationEnvelopeDirty();
            return;
        }
    }
}

float NafTachyonAudioProcessor::getVoiceAmplitudeForRelease (const OscillatorVoice& voice)
{
    const auto noteOnElapsed = static_cast<float> (globalSampleCounter - voice.noteOnSample)
                             / static_cast<float> (currentSampleRate);

    if (modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::amplitude, apvts))
    {
        auto knobSnapshot = voice.modKnobSnapshot;
        knobSnapshot.amplitude = apvts.getRawParameterValue (amplitudeParamId)->load();
        return modulationEnvelope.evaluate (noteOnElapsed, knobSnapshot).amplitude;
    }

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

    const auto numSamples = buffer.getNumSamples();

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

    auto activeVoiceCount = 0;

    for (const auto& voice : voices)
        if (voice.isActive)
            ++activeVoiceCount;

    globalSampleCounter += numSamples;

    if (activeVoiceCount == 0)
        return;

    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
            if (const auto bpm = position->getBpm())
                cachedHostBpm.store (static_cast<float> (*bpm), std::memory_order_relaxed);

    const auto amplitudeKnob = apvts.getRawParameterValue (amplitudeParamId)->load();
    const auto waveformMorphKnob = apvts.getRawParameterValue (waveformParamId)->load();
    const auto pulseWidthKnob = apvts.getRawParameterValue (pulseWidthParamId)->load();
    const auto overtonesKnob = apvts.getRawParameterValue (overtonesParamId)->load();
    const auto osc2WaveformKnob = apvts.getRawParameterValue (osc2WaveformParamId)->load();
    const auto osc2PulseWidthKnob = apvts.getRawParameterValue (osc2PulseWidthParamId)->load();
    const auto osc2OvertonesKnob = apvts.getRawParameterValue (osc2OvertonesParamId)->load();
    const auto oscSyncEnabled = apvts.getRawParameterValue (oscSyncParamId)->load() >= 0.5f;
    const auto cutoffKnob = apvts.getRawParameterValue (filterCutoffParamId)->load();
    const auto resonanceKnob = apvts.getRawParameterValue (filterResonanceParamId)->load();
    const auto unisonVoices = apvts.getRawParameterValue (unisonParamId)->load();
    const auto unisonSpread = apvts.getRawParameterValue (unisonSpreadParamId)->load();
    const auto unisonSettings = calcUnisonSettings (unisonVoices, unisonSpread);

    const auto shapeModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::shape, apvts);
    const auto widthModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::width, apvts);
    const auto overtonesModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::overtones, apvts);
    const auto pitchModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::pitch, apvts);
    const auto osc2ShapeModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::osc2Shape, apvts);
    const auto osc2WidthModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::osc2Width, apvts);
    const auto osc2OvertonesModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::osc2Overtones, apvts);
    const auto osc2PitchModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::osc2Pitch, apvts);
    const auto cutoffModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::cutoff, apvts);
    const auto resonanceModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::resonance, apvts);
    const auto amplitudeModEnabled = modulationEnvelope.isLaneEnabled (ModulationEnvelope::Lane::amplitude, apvts);

    const auto anyLaneModEnabled = shapeModEnabled || widthModEnabled || overtonesModEnabled
                               || pitchModEnabled
                               || osc2ShapeModEnabled || osc2WidthModEnabled || osc2OvertonesModEnabled || osc2PitchModEnabled
                               || cutoffModEnabled || resonanceModEnabled || amplitudeModEnabled;

    if (anyLaneModEnabled
        && modulationEnvelopeApvtsDirty.exchange (false, std::memory_order_acq_rel))
        modulationEnvelope.updateFromApvts (apvts);

    if (! widthModEnabled)
        pulseWidthSmoother.setTargetValue (pulseWidthKnob);

    if (! overtonesModEnabled)
        overtonesSmoother.setTargetValue (overtonesKnob);

    if (! osc2WidthModEnabled)
        osc2PulseWidthSmoother.setTargetValue (osc2PulseWidthKnob);

    if (! osc2OvertonesModEnabled)
        osc2OvertonesSmoother.setTargetValue (osc2OvertonesKnob);

    if (! cutoffModEnabled)
        filterCutoffSmoother.setTargetValue (cutoffKnob);

    if (! resonanceModEnabled)
        filterResonanceSmoother.setTargetValue (resonanceKnob);

    if (! amplitudeModEnabled)
        amplitudeSmoother.setTargetValue (amplitudeKnob);

    const auto filterSlopeIndex = juce::jlimit (0, 2, static_cast<int> (std::round (apvts.getRawParameterValue (filterSlopeParamId)->load())));
    const auto filterSlope = static_cast<FilterSlope> (filterSlopeIndex);
    const auto filterLimiterIndex = juce::jlimit (0, 3, static_cast<int> (std::round (apvts.getRawParameterValue (filterLimiterParamId)->load())));
    const auto filterLimiterMode = static_cast<FilterLimiterMode> (filterLimiterIndex);
    const auto filterLimiterCoeffs = makeFilterLimiterCoeffs (filterLimiterMode);
    const auto releaseTimeSeconds = apvts.getRawParameterValue (releaseTimeParamId)->load();
    osc1PitchSmoother.setTargetValue (apvts.getRawParameterValue (osc1PitchParamId)->load());
    pitchTuneSmoother.setTargetValue (apvts.getRawParameterValue (pitchTuneParamId)->load());
    osc2PitchSmoother.setTargetValue (apvts.getRawParameterValue (osc2PitchParamId)->load());
    osc2PitchTuneSmoother.setTargetValue (apvts.getRawParameterValue (osc2PitchTuneParamId)->load());
    oscMixSmoother.setTargetValue (apvts.getRawParameterValue (oscMixParamId)->load());

    for (auto& voice : voices)
    {
        if (voice.isActive)
            updateUnisonIncrements (voice, unisonVoices, unisonSpread);
    }

    const auto numOutputChannels = getTotalNumOutputChannels();
    constexpr float voiceGain = 0.2f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto modWheelDepth = modWheelSmoother.getNextValue();
        const auto pitchWheelNormalised = pitchBendSmoother.getNextValue();
        const auto pitchBendVibrato = vibratoPitchRatio (vibratoLfoPhase, modWheelDepth)
                                    * pitchBendRatio (pitchWheelNormalised);
        const auto sharedOsc1PitchSemitones = osc1PitchSmoother.getNextValue();
        const auto sharedOsc1FineTuneRatio = pitchFineTuneRatio (pitchTuneSmoother.getNextValue());
        const auto sharedOsc2PitchSemitones = osc2PitchSmoother.getNextValue();
        const auto sharedOsc2FineTuneRatio = pitchFineTuneRatio (osc2PitchTuneSmoother.getNextValue());
        const auto oscMix = oscMixSmoother.getNextValue();

        vibratoLfoPhase += juce::MathConstants<double>::twoPi * static_cast<double> (vibratoRateHz)
                         / currentSampleRate;
        wrapPhase (vibratoLfoPhase);

        const auto sharedPulseWidth = widthModEnabled ? pulseWidthKnob : pulseWidthSmoother.getNextValue();
        const auto sharedOvertones = overtonesModEnabled ? overtonesKnob : overtonesSmoother.getNextValue();
        const auto sharedCutoffHz = cutoffModEnabled ? cutoffKnob : filterCutoffSmoother.getNextValue();
        const auto sharedResonance = resonanceModEnabled ? resonanceKnob : filterResonanceSmoother.getNextValue();
        const auto sharedAmplitude = amplitudeModEnabled ? amplitudeKnob : amplitudeSmoother.getNextValue();
        const auto sharedOsc2PulseWidth = osc2WidthModEnabled ? osc2PulseWidthKnob : osc2PulseWidthSmoother.getNextValue();
        const auto sharedOsc2Overtones = osc2OvertonesModEnabled ? osc2OvertonesKnob : osc2OvertonesSmoother.getNextValue();

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
            auto pulseWidth = sharedPulseWidth;
            auto overtones = sharedOvertones;
            auto osc1PitchSemitones = sharedOsc1PitchSemitones;
            auto osc2WaveformMorph = osc2WaveformKnob;
            auto osc2PulseWidth = sharedOsc2PulseWidth;
            auto osc2Overtones = sharedOsc2Overtones;
            auto osc2PitchSemitones = sharedOsc2PitchSemitones;
            auto cutoffHz = sharedCutoffHz;
            auto resonance = sharedResonance;
            auto amplitude = sharedAmplitude;

            if (anyLaneModEnabled)
            {
                const auto elapsedSeconds = static_cast<float> (globalSampleCounter + sample - voice.noteOnSample)
                                          / static_cast<float> (currentSampleRate);
                auto knobSnapshot = voice.modKnobSnapshot;

                if (shapeModEnabled)
                    knobSnapshot.shape = waveformMorphKnob;

                if (widthModEnabled)
                    knobSnapshot.pulseWidth = pulseWidthKnob;

                if (overtonesModEnabled)
                    knobSnapshot.overtones = overtonesKnob;

                if (pitchModEnabled)
                    knobSnapshot.pitch = sharedOsc1PitchSemitones;

                if (osc2ShapeModEnabled)
                    knobSnapshot.osc2Shape = osc2WaveformKnob;

                if (osc2WidthModEnabled)
                    knobSnapshot.osc2PulseWidth = osc2PulseWidthKnob;

                if (osc2OvertonesModEnabled)
                    knobSnapshot.osc2Overtones = osc2OvertonesKnob;

                if (osc2PitchModEnabled)
                    knobSnapshot.osc2Pitch = sharedOsc2PitchSemitones;

                if (cutoffModEnabled)
                    knobSnapshot.cutoffHz = cutoffKnob;

                if (resonanceModEnabled)
                    knobSnapshot.resonance = resonanceKnob;

                if (amplitudeModEnabled)
                    knobSnapshot.amplitude = amplitudeKnob;

                const auto modParams = modulationEnvelope.evaluate (elapsedSeconds, knobSnapshot);

                if (shapeModEnabled)
                    waveformMorph = modParams.shape;

                if (widthModEnabled)
                    pulseWidth = modParams.pulseWidth;

                if (overtonesModEnabled)
                    overtones = modParams.overtones;

                if (pitchModEnabled)
                    osc1PitchSemitones = modParams.pitch;

                if (osc2ShapeModEnabled)
                    osc2WaveformMorph = modParams.osc2Shape;

                if (osc2WidthModEnabled)
                    osc2PulseWidth = modParams.osc2PulseWidth;

                if (osc2OvertonesModEnabled)
                    osc2Overtones = modParams.osc2Overtones;

                if (osc2PitchModEnabled)
                    osc2PitchSemitones = modParams.osc2Pitch;

                if (cutoffModEnabled)
                    cutoffHz = modParams.cutoffHz;

                if (resonanceModEnabled)
                    resonance = modParams.resonance;

                if (amplitudeModEnabled && ! voice.inRelease)
                    amplitude = modParams.amplitude;
            }

            if (voice.inRelease)
                amplitude = voice.releaseStartAmplitude * releaseGain;

            cutoffHz *= voice.cutoffVelScale;
            cutoffHz = juce::jlimit (20.0f, 20000.0f, cutoffHz);

            const auto osc1PitchRatio = pitchBendVibrato
                                      * pitchSemitoneRatio (osc1PitchSemitones)
                                      * sharedOsc1FineTuneRatio;
            const auto osc2PitchRatio = pitchBendVibrato
                                      * pitchSemitoneRatio (osc2PitchSemitones)
                                      * sharedOsc2FineTuneRatio;

            float osc1Sample = 0.0f;
            float osc2Sample = 0.0f;

            const auto osc1MainInc = voice.phaseIncrement * osc1PitchRatio;
            const auto osc1SubInc = voice.subPhaseIncrement * osc1PitchRatio;
            const auto osc1FifthInc = voice.fifthPhaseIncrement * osc1PitchRatio;
            const auto osc2MainInc = voice.phase2Increment * osc2PitchRatio;
            const auto osc2SubInc = voice.subPhase2Increment * osc2PitchRatio;
            const auto osc2FifthInc = voice.fifthPhase2Increment * osc2PitchRatio;

            if (unisonSettings.count <= 1)
            {
                osc1Sample = computeOscLayerSample (voice.phase,
                                                    osc1MainInc,
                                                    voice.subPhase,
                                                    osc1SubInc,
                                                    voice.fifthPhase,
                                                    osc1FifthInc,
                                                    waveformMorph,
                                                    pulseWidth,
                                                    overtones);

                osc2Sample = computeOscLayerSample (voice.phase2,
                                                    osc2MainInc,
                                                    voice.subPhase2,
                                                    osc2SubInc,
                                                    voice.fifthPhase2,
                                                    osc2FifthInc,
                                                    osc2WaveformMorph,
                                                    osc2PulseWidth,
                                                    osc2Overtones);
            }
            else
            {
                for (int u = 0; u < unisonSettings.count; ++u)
                {
                    osc1Sample += computeOscLayerSample (voice.unisonPhase[u],
                                                         voice.unisonPhaseIncrement[u] * osc1PitchRatio,
                                                         voice.unisonSubPhase[u],
                                                         voice.unisonSubPhaseIncrement[u] * osc1PitchRatio,
                                                         voice.unisonFifthPhase[u],
                                                         voice.unisonFifthPhaseIncrement[u] * osc1PitchRatio,
                                                         waveformMorph,
                                                         pulseWidth,
                                                         overtones);

                    osc2Sample += computeOscLayerSample (voice.unisonPhase2[u],
                                                         voice.unisonPhase2Increment[u] * osc2PitchRatio,
                                                         voice.unisonSubPhase2[u],
                                                         voice.unisonSubPhase2Increment[u] * osc2PitchRatio,
                                                         voice.unisonFifthPhase2[u],
                                                         voice.unisonFifthPhase2Increment[u] * osc2PitchRatio,
                                                         osc2WaveformMorph,
                                                         osc2PulseWidth,
                                                         osc2Overtones);
                }

                osc1Sample *= unisonSettings.normalise;
                osc2Sample *= unisonSettings.normalise;
            }

            auto oscSample = mixOscillatorSamples (osc1Sample, osc2Sample, oscMix);
            oscSample *= voice.ampVelScale * amplitude * voiceGain;

            updateVoiceFilterCache (voice, cutoffHz, resonance, filterSlope);

            auto filteredSample = filterSample (oscSample, voice, voice.cachedFilterCoeffs, filterSlope);
            filteredSample = applyFilterLimiter (filteredSample, voice, filterLimiterCoeffs);
            outputSample += filteredSample;

            const auto syncRatio = osc2MainInc > 1.0e-12
                                 ? osc1MainInc / osc2MainInc
                                 : 1.0;

            if (unisonSettings.count <= 1)
            {
                bool masterWrapped = false;
                advanceMasterPhase (voice.phase2, osc2MainInc, masterWrapped);
                juce::ignoreUnused (masterWrapped);
                voice.subPhase2 += osc2SubInc;
                voice.fifthPhase2 += osc2FifthInc;
                wrapPhase (voice.subPhase2);
                wrapPhase (voice.fifthPhase2);

                if (oscSyncEnabled)
                {
                    syncSlavePhasesToMaster (voice.phase2,
                                             voice.subPhase2,
                                             voice.fifthPhase2,
                                             syncRatio,
                                             voice.phase,
                                             voice.subPhase,
                                             voice.fifthPhase);
                }
                else
                {
                    advanceFreeRunningPhases (voice.phase,
                                              voice.subPhase,
                                              voice.fifthPhase,
                                              osc1MainInc,
                                              osc1SubInc,
                                              osc1FifthInc);
                }
            }
            else
            {
                for (int u = 0; u < unisonSettings.count; ++u)
                {
                    const auto osc1MainIncU = voice.unisonPhaseIncrement[u] * osc1PitchRatio;
                    const auto osc2MainIncU = voice.unisonPhase2Increment[u] * osc2PitchRatio;
                    const auto syncRatioU = osc2MainIncU > 1.0e-12 ? osc1MainIncU / osc2MainIncU : 1.0;

                    bool masterWrapped = false;
                    advanceMasterPhase (voice.unisonPhase2[u], osc2MainIncU, masterWrapped);
                    juce::ignoreUnused (masterWrapped);
                    voice.unisonSubPhase2[u] += voice.unisonSubPhase2Increment[u] * osc2PitchRatio;
                    voice.unisonFifthPhase2[u] += voice.unisonFifthPhase2Increment[u] * osc2PitchRatio;
                    wrapPhase (voice.unisonSubPhase2[u]);
                    wrapPhase (voice.unisonFifthPhase2[u]);

                    if (oscSyncEnabled)
                    {
                        syncSlavePhasesToMaster (voice.unisonPhase2[u],
                                                 voice.unisonSubPhase2[u],
                                                 voice.unisonFifthPhase2[u],
                                                 syncRatioU,
                                                 voice.unisonPhase[u],
                                                 voice.unisonSubPhase[u],
                                                 voice.unisonFifthPhase[u]);
                    }
                    else
                    {
                        advanceFreeRunningPhases (voice.unisonPhase[u],
                                                  voice.unisonSubPhase[u],
                                                  voice.unisonFifthPhase[u],
                                                  osc1MainIncU,
                                                  voice.unisonSubPhaseIncrement[u] * osc1PitchRatio,
                                                  voice.unisonFifthPhaseIncrement[u] * osc1PitchRatio);
                    }
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
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        markModulationEnvelopeDirty();
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NafTachyonAudioProcessor();
}
