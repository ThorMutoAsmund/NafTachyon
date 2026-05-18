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

    void layoutMainControls (juce::Rectangle<int> area, int dialSize);

    void layoutFilterControls (juce::Rectangle<int> area, int dialSize);

    void layoutUnisonControls (juce::Rectangle<int> area, int dialSize);

    void layoutWaveformControls (juce::Rectangle<int> area, int dialSize, int previewWidth);

    int unisonPanelWidthForDialSize (int dialSize) const;

    int uniformDialSize = 108;



    void mouseUp (const juce::MouseEvent& e) override;



    NafTachyonAudioProcessor& audioProcessor;

    NafTachyonLookAndFeel customLookAndFeel;



    SectionPanel mainGroup { "MAIN" };

    SectionPanel waveformGroup { "WAVE" };

    SectionPanel evolveGroup { "EVOLVE" };

    SectionPanel filterGroup { "FILTER" };

    SectionPanel unisonGroup { "DETUNE" };



    juce::Slider amplitudeSlider;

    juce::Slider releaseTimeSlider;

    juce::Slider amplitudeVelSensitivitySlider;

    juce::Label amplitudeLabel;

    juce::Label releaseTimeLabel;

    juce::Label amplitudeVelSensitivityLabel;



    juce::Slider waveformSlider;

    juce::Slider pulseWidthSlider;

    juce::Slider overtonesSlider;

    juce::Label waveformLabel;

    juce::Label pulseWidthLabel;

    juce::Label overtonesLabel;

    WaveformPreview waveformPreview;

    ModEnvelopeEditor modEnvelopeEditor;



    juce::Slider cutoffSlider, resonanceSlider;

    juce::Slider cutoffVelSensitivitySlider;

    juce::Label cutoffLabel, resonanceLabel;

    juce::Label cutoffVelSensitivityLabel;

    juce::ComboBox filterSlopeCombo;

    juce::Label filterSlopeLabel;

    juce::Slider unisonSlider;
    juce::Slider unisonSpreadSlider;

    juce::Label unisonLabel;
    juce::Label unisonSpreadLabel;



    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;



    std::unique_ptr<SliderAttachment> amplitudeAttachment;

    std::unique_ptr<SliderAttachment> releaseTimeAttachment;

    std::unique_ptr<SliderAttachment> amplitudeVelSensitivityAttachment;

    std::unique_ptr<SliderAttachment> waveformAttachment;

    std::unique_ptr<SliderAttachment> pulseWidthAttachment;

    std::unique_ptr<SliderAttachment> overtonesAttachment;

    std::unique_ptr<SliderAttachment> cutoffAttachment;

    std::unique_ptr<SliderAttachment> resonanceAttachment;

    std::unique_ptr<SliderAttachment> cutoffVelSensitivityAttachment;

    std::unique_ptr<ComboBoxAttachment> filterSlopeAttachment;

    std::unique_ptr<SliderAttachment> unisonAttachment;
    std::unique_ptr<SliderAttachment> unisonSpreadAttachment;



    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NafTachyonAudioProcessorEditor)

};

