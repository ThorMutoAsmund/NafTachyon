/*

  ==============================================================================



    This file contains the basic framework code for a JUCE plugin processor.



  ==============================================================================

*/



#pragma once



#include <JuceHeader.h>



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

    enum class EnvelopeStage

    {

        idle,

        attack,

        decay,

        sustain,

        release

    };



    enum class FilterSlope

    {

        sixDb = 0,

        twelveDb,

        twentyFourDb

    };



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

        float level = 0.0f;

        float envelopeLevel = 0.0f;

        float releaseIncrement = 0.0f;

        EnvelopeStage stage = EnvelopeStage::idle;

        float onePoleState = 0.0f;

        float biquad1Z1 = 0.0f;

        float biquad1Z2 = 0.0f;

        float biquad2Z1 = 0.0f;

        float biquad2Z2 = 0.0f;

    };



    struct FilterCoefficients

    {

        float onePoleCoeff = 1.0f;

        float b0 = 1.0f;

        float b1 = 0.0f;

        float b2 = 0.0f;

        float a1 = 0.0f;

        float a2 = 0.0f;

    };



    static constexpr int maxVoices = 16;



    void startVoice (int midiNote, int velocity);

    void releaseVoice (int midiNote, float releaseSeconds);

    void releaseAllVoices (float releaseSeconds);

    void advanceEnvelope (OscillatorVoice& voice, float attackDelta, float decayDelta, float sustainLevel);

    void resetVoiceFilter (OscillatorVoice& voice);

    FilterCoefficients makeFilterCoefficients (float cutoffHz, float resonance, FilterSlope slope) const;

    float filterSample (float input, OscillatorVoice& voice, const FilterCoefficients& coeffs, FilterSlope slope) const;



    OscillatorVoice voices[maxVoices];

    double currentSampleRate = 44100.0;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> filterCutoffSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> filterResonanceSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> overtonesSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pulseWidthSmoother;

    juce::AudioProcessorValueTreeState apvts;



    //==============================================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NafTachyonAudioProcessor)

};

