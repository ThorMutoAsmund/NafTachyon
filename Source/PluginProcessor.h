/*

  ==============================================================================



    This file contains the basic framework code for a JUCE plugin processor.



  ==============================================================================

*/



#pragma once



#include <JuceHeader.h>

#include "ModulationEnvelope.h"



//==============================================================================

/**

*/

class NafTachyonAudioProcessor  : public juce::AudioProcessor

{

public:

    //==============================================================================

    NafTachyonAudioProcessor();

    ~NafTachyonAudioProcessor() override;



    //==============================================================================

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;

    void releaseResources() override;



   #ifndef JucePlugin_PreferredChannelConfigurations

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

   #endif



    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;



    //==============================================================================

    juce::AudioProcessorEditor* createEditor() override;

    bool hasEditor() const override;



    //==============================================================================

    const juce::String getName() const override;



    bool acceptsMidi() const override;

    bool producesMidi() const override;

    bool isMidiEffect() const override;

    double getTailLengthSeconds() const override;



    //==============================================================================

    int getNumPrograms() override;

    int getCurrentProgram() override;

    void setCurrentProgram (int index) override;

    const juce::String getProgramName (int index) override;

    void changeProgramName (int index, const juce::String& newName) override;



    //==============================================================================

    void getStateInformation (juce::MemoryBlock& destData) override;

    void setStateInformation (const void* data, int sizeInBytes) override;



    juce::AudioProcessorValueTreeState& getApvts() { return apvts; }



    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();



private:

    enum class FilterSlope

    {

        sixDb = 0,

        twelveDb,

        twentyFourDb

    };

    static constexpr int maxUnisonStack = 5;

    struct OscillatorVoice

    {

        bool isActive = false;

        int midiNote = 0;

        double phase = 0.0;

        double subPhase = 0.0;

        double fifthPhase = 0.0;

        double phaseIncrement = 0.0;

        double subPhaseIncrement = 0.0;

        double fifthPhaseIncrement = 0.0;

        double unisonPhase[maxUnisonStack] {};
        double unisonSubPhase[maxUnisonStack] {};
        double unisonFifthPhase[maxUnisonStack] {};
        double unisonPhaseIncrement[maxUnisonStack] {};
        double unisonSubPhaseIncrement[maxUnisonStack] {};
        double unisonFifthPhaseIncrement[maxUnisonStack] {};

        juce::int64 noteOnSample = 0;

        juce::int64 noteOffSample = 0;

        bool inRelease = false;

        float releaseStartAmplitude = 1.0f;

        float ampVelScale = 1.0f;

        float cutoffVelScale = 1.0f;

        float onePoleState = 0.0f;

        float biquad1Z1 = 0.0f;

        float biquad1Z2 = 0.0f;

        float biquad2Z1 = 0.0f;

        float biquad2Z2 = 0.0f;

        ModKnobSnapshot modKnobSnapshot;

    };



    struct FilterCoefficients

    {

        float onePoleCoeff = 1.0f;

        float b0 = 1.0f;

        float b1 = 0.0f;

        float b2 = 0.0f;

        float a1 = 0.0f;

        float a2 = 0.0f;

        /** 6 dB mode: 0 = one-pole only, 1 = full resonant biquad blend. */
        float sixDbResonantBlend = 0.0f;

    };



    static constexpr int maxVoices = 16;

    void startVoice (int midiNote, int velocity);

    void beginVoiceRelease (OscillatorVoice& voice, bool immediate);

    float getVoiceAmplitudeForRelease (const OscillatorVoice& voice);

    void releaseVoice (int midiNote);

    void releaseAllVoices (bool immediate);

    void resetVoiceFilter (OscillatorVoice& voice);

    void resetUnisonPhases (OscillatorVoice& voice);

    void updateUnisonIncrements (OscillatorVoice& voice, float unisonVoices, float unisonSpread);

    FilterCoefficients makeFilterCoefficients (float cutoffHz, float resonance, FilterSlope slope) const;

    float filterSample (float input, OscillatorVoice& voice, const FilterCoefficients& coeffs, FilterSlope slope) const;

    void updateDcHighpassCoefficients();
    void resetDcHighpassState();
    void applyDcHighpass (juce::AudioBuffer<float>& buffer);



    OscillatorVoice voices[maxVoices];

    double currentSampleRate = 44100.0;

    juce::int64 globalSampleCounter = 0;

    ModulationEnvelope modulationEnvelope;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> filterCutoffSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> filterResonanceSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> overtonesSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pulseWidthSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amplitudeSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> modWheelSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pitchBendSmoother;

    double vibratoLfoPhase = 0.0;

    FilterCoefficients dcHighpassCoeffs;
    float dcHighpassZ1[2] {};
    float dcHighpassZ2[2] {};

    juce::AudioProcessorValueTreeState apvts;



    //==============================================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NafTachyonAudioProcessor)

};

