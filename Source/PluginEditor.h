/*

  ==============================================================================



    This file contains the basic framework code for a JUCE plugin editor.



  ==============================================================================

*/



#pragma once



#include <JuceHeader.h>

#include "PluginProcessor.h"

#include "NafTachyonLookAndFeel.h"

#include "SectionPanel.h"
#include "WaveformPreview.h"
#include "ModEnvelopeEditor.h"



//==============================================================================

/**

*/

class NafTachyonAudioProcessorEditor  : public juce::AudioProcessorEditor

{

public:

    NafTachyonAudioProcessorEditor (NafTachyonAudioProcessor&);

    ~NafTachyonAudioProcessorEditor() override;



    void paint (juce::Graphics&) override;

    void resized() override;



private:

    void configureKnob (juce::Component& parent, juce::Slider& slider, juce::Label& label, const juce::String& text);

    void configureFilterSlopeControl (juce::Component& parent);

    static void setComponentIgnoresKeyboard (juce::Component& component);

    void layoutAdsrKnobs (juce::Rectangle<int> area, int dialSize);

    void layoutFilterControls (juce::Rectangle<int> area, int dialSize);

    void layoutUnisonControls (juce::Rectangle<int> area, int dialSize);

    void layoutWaveformControls (juce::Rectangle<int> area, int dialSize, int previewWidth);

    int unisonPanelWidthForDialSize (int dialSize) const;

    int uniformDialSize = 108;



    void mouseUp (const juce::MouseEvent& e) override;



    NafTachyonAudioProcessor& audioProcessor;

    NafTachyonLookAndFeel customLookAndFeel;



    SectionPanel adsrGroup { "ADSR" };

    SectionPanel waveformGroup { "WAVE" };

    SectionPanel evolveGroup { "EVOLVE" };

    SectionPanel filterGroup { "FILTER" };

    SectionPanel unisonGroup { "UNISON" };



    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;

    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;



    juce::Slider waveformSlider;

    juce::Slider pulseWidthSlider;

    juce::Slider overtonesSlider;

    juce::Label waveformLabel;

    juce::Label pulseWidthLabel;

    juce::Label overtonesLabel;

    WaveformPreview waveformPreview;

    ModEnvelopeEditor modEnvelopeEditor;



    juce::Slider cutoffSlider, resonanceSlider;

    juce::Label cutoffLabel, resonanceLabel;

    juce::ComboBox filterSlopeCombo;

    juce::Label filterSlopeLabel;

    juce::Slider unisonSlider;

    juce::Label unisonLabel;



    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;



    std::unique_ptr<SliderAttachment> attackAttachment;

    std::unique_ptr<SliderAttachment> decayAttachment;

    std::unique_ptr<SliderAttachment> sustainAttachment;

    std::unique_ptr<SliderAttachment> releaseAttachment;

    std::unique_ptr<SliderAttachment> waveformAttachment;

    std::unique_ptr<SliderAttachment> pulseWidthAttachment;

    std::unique_ptr<SliderAttachment> overtonesAttachment;

    std::unique_ptr<SliderAttachment> cutoffAttachment;

    std::unique_ptr<SliderAttachment> resonanceAttachment;

    std::unique_ptr<ComboBoxAttachment> filterSlopeAttachment;

    std::unique_ptr<SliderAttachment> unisonAttachment;



    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NafTachyonAudioProcessorEditor)

};

