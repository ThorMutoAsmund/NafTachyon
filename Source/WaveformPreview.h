/*
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class WaveformPreview : public juce::Component,
                        private juce::Timer
{
public:
    explicit WaveformPreview (juce::AudioProcessorValueTreeState& apvtsToUse);

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    juce::AudioProcessorValueTreeState& apvts;
};
