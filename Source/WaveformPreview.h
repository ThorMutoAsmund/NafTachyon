/*
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class WaveformPreview : public juce::Component,
                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit WaveformPreview (juce::AudioProcessorValueTreeState& apvtsToUse);
    ~WaveformPreview() override;

    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    void attachParameterListeners();
    void detachParameterListeners();

    juce::AudioProcessorValueTreeState& apvts;

    bool showOsc2 = false;
};
