/*

  ==============================================================================



    This file contains the basic framework code for a JUCE plugin editor.



  ==============================================================================

*/



#pragma once



#include <JuceHeader.h>

#include "PluginProcessor.h"

#include "NafTachyonLookAndFeel.h"
#include "NafTachyonKnob.h"

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

    void configureFilterComboControl (juce::ComboBox& combo, juce::Label& label, const juce::String& labelText);

    static void setComponentIgnoresKeyboard (juce::Component& component);

    void layoutMainControls (juce::Rectangle<int> area, int dialSize);

    void layoutFilterControls (juce::Rectangle<int> area, int dialSize);

    void layoutUnisonControls (juce::Rectangle<int> area, int dialSize);

    void layoutWaveformControls (juce::Rectangle<int> area, int dialSize, int previewWidth);

    void layoutWaveformOscRow (juce::Rectangle<int> rowArea,
                               int dialSize,
                               juce::Label& rowLabel,
                               juce::Slider& shapeSlider,
                               juce::Label& shapeLabel,
                               juce::Slider& widthSlider,
                               juce::Label& widthLabel,
                               juce::Slider& harmonicsSlider,
                               juce::Label& harmonicsLabel,
                               juce::Slider& pitchSlider,
                               juce::Label& pitchLabel,
                               juce::Slider& fineTuneSlider,
                               juce::Label& fineTuneLabel);

    void configureToggleButton (juce::ToggleButton& button);

    int unisonPanelWidthForDialSize (int dialSize) const;

    int uniformDialSize = 108;



    void mouseUp (const juce::MouseEvent& e) override;



    NafTachyonAudioProcessor& audioProcessor;

    NafTachyonLookAndFeel customLookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 600 };



    SectionPanel mainGroup { "MAIN" };

    SectionPanel waveformGroup { "WAVE" };

    SectionPanel evolveGroup { "EVOLVE" };

    SectionPanel filterGroup { "FILTER" };

    SectionPanel unisonGroup { "DETUNE" };



    NafTachyonKnob amplitudeSlider;

    NafTachyonKnob releaseTimeSlider;

    NafTachyonKnob amplitudeVelSensitivitySlider;

    juce::Label amplitudeLabel;

    juce::Label releaseTimeLabel;

    juce::Label amplitudeVelSensitivityLabel;



    NafTachyonKnob waveformSlider;

    NafTachyonKnob pulseWidthSlider;

    NafTachyonKnob overtonesSlider;

    NafTachyonKnob osc1PitchSlider;

    NafTachyonKnob pitchTuneSlider;

    NafTachyonKnob osc2WaveformSlider;

    NafTachyonKnob osc2PulseWidthSlider;

    NafTachyonKnob osc2OvertonesSlider;

    NafTachyonKnob osc2PitchSlider;

    NafTachyonKnob osc2PitchTuneSlider;

    NafTachyonKnob oscMixSlider;

    juce::Label waveformLabel;

    juce::Label pulseWidthLabel;

    juce::Label overtonesLabel;

    juce::Label osc1PitchLabel;

    juce::Label pitchTuneLabel;

    juce::Label osc1RowLabel;

    juce::Label osc2RowLabel;

    juce::Label osc2WaveformLabel;

    juce::Label osc2PulseWidthLabel;

    juce::Label osc2OvertonesLabel;

    juce::Label osc2PitchLabel;

    juce::Label osc2PitchTuneLabel;

    juce::Label oscMixLabel;

    juce::ToggleButton oscSyncToggle { "Sync" };

    WaveformPreview waveformPreview;

    ModEnvelopeEditor modEnvelopeEditor;



    NafTachyonKnob cutoffSlider, resonanceSlider;

    NafTachyonKnob cutoffVelSensitivitySlider;

    juce::Label cutoffLabel, resonanceLabel;

    juce::Label cutoffVelSensitivityLabel;

    juce::ComboBox filterSlopeCombo;

    juce::ComboBox filterLimiterCombo;

    juce::Label filterSlopeLabel;

    juce::Label filterLimiterLabel;

    NafTachyonKnob unisonSlider;
    NafTachyonKnob unisonSpreadSlider;

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

    std::unique_ptr<SliderAttachment> osc1PitchAttachment;

    std::unique_ptr<SliderAttachment> pitchTuneAttachment;

    std::unique_ptr<SliderAttachment> osc2WaveformAttachment;

    std::unique_ptr<SliderAttachment> osc2PulseWidthAttachment;

    std::unique_ptr<SliderAttachment> osc2OvertonesAttachment;

    std::unique_ptr<SliderAttachment> osc2PitchAttachment;

    std::unique_ptr<SliderAttachment> osc2PitchTuneAttachment;

    std::unique_ptr<SliderAttachment> oscMixAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> oscSyncAttachment;

    std::unique_ptr<SliderAttachment> cutoffAttachment;

    std::unique_ptr<SliderAttachment> resonanceAttachment;

    std::unique_ptr<SliderAttachment> cutoffVelSensitivityAttachment;

    std::unique_ptr<ComboBoxAttachment> filterSlopeAttachment;

    std::unique_ptr<ComboBoxAttachment> filterLimiterAttachment;

    std::unique_ptr<SliderAttachment> unisonAttachment;
    std::unique_ptr<SliderAttachment> unisonSpreadAttachment;



    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NafTachyonAudioProcessorEditor)

};

