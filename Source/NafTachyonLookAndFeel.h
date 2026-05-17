/*
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
class NafTachyonLookAndFeel  : public juce::LookAndFeel_V4
{
public:
    NafTachyonLookAndFeel();

    static juce::Colour getPluginBackgroundColour();

    void drawRotarySlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPos,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

    juce::Label* createSliderTextBox (juce::Slider& slider) override;

private:
    static void drawTick (juce::Graphics& g,
                          juce::Point<float> centre,
                          float angle,
                          float innerRadius,
                          float outerRadius,
                          float lineWidth,
                          juce::Colour colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NafTachyonLookAndFeel)
};
