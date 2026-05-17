/*
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
class SectionPanel  : public juce::Component
{
public:
    explicit SectionPanel (const juce::String& title);

    juce::Rectangle<int> getContentBounds() const;

    void paint (juce::Graphics& g) override;

private:
    juce::String sectionTitle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionPanel)
};
