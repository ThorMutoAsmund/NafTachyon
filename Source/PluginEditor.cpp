/*

  ==============================================================================



    This file contains the basic framework code for a JUCE plugin editor.



  ==============================================================================

*/



#include "PluginProcessor.h"

#include "PluginEditor.h"



namespace

{

    void configureSectionLabel (juce::Label& label, const juce::String& text, float fontSize = 12.0f)

    {

        label.setText (text, juce::dontSendNotification);

        label.setJustificationType (juce::Justification::centred);

        label.setColour (juce::Label::textColourId, juce::Colour (0xffc8d0d6));

        label.setFont (juce::FontOptions (fontSize));

        label.setWantsKeyboardFocus (false);

    }



    constexpr int maxDialSize = 120;

    constexpr int labelRowHeight = 18;

    constexpr int sectionChromeHeight = 46;

    constexpr int waveformPanelHeight = 160;

    constexpr int evolvePanelHeight = 294;

    constexpr int sectionGap = 6;

    constexpr int minWaveformPreviewWidth = 108;



    int waveformPreviewWidthForArea (int contentWidth)

    {

        return juce::jmax (minWaveformPreviewWidth, contentWidth / 4);

    }



    int computeUniformDialSize (juce::Rectangle<int> waveformContentBounds, int previewWidth)

    {

        auto dialArea = waveformContentBounds;

        dialArea.removeFromRight (previewWidth);

        dialArea.removeFromTop (labelRowHeight);

        const auto columnWidth = dialArea.getWidth() / 3;

        return juce::jmin (columnWidth, dialArea.getHeight(), maxDialSize);

    }



    int panelHeightForDialSize (int dialSize)

    {

        return dialSize + labelRowHeight + sectionChromeHeight;

    }



    void layoutCentredDial (juce::Slider& slider, juce::Rectangle<int> area, int dialSize)

    {

        slider.setBounds (area.withSizeKeepingCentre (dialSize, dialSize));

    }

}



//==============================================================================

NafTachyonAudioProcessorEditor::NafTachyonAudioProcessorEditor (NafTachyonAudioProcessor& p)

    : AudioProcessorEditor (&p),
      audioProcessor (p),
      waveformPreview (p.getApvts()),
      modEnvelopeEditor (p.getApvts(), [&p]() { return p.getLiveHostBpm(); })

{

    setLookAndFeel (&customLookAndFeel);



    addAndMakeVisible (mainGroup);

    addAndMakeVisible (waveformGroup);

    addAndMakeVisible (evolveGroup);

    addAndMakeVisible (filterGroup);

    addAndMakeVisible (unisonGroup);



    configureKnob (mainGroup, amplitudeSlider, amplitudeLabel, "Gain");

    configureKnob (mainGroup, releaseTimeSlider, releaseTimeLabel, "Release Time");

    configureKnob (mainGroup, amplitudeVelSensitivitySlider, amplitudeVelSensitivityLabel, "Amp velocity");



    configureKnob (waveformGroup, waveformSlider, waveformLabel, "Shape");

    configureKnob (waveformGroup, pulseWidthSlider, pulseWidthLabel, "Width");

    configureKnob (waveformGroup, overtonesSlider, overtonesLabel, "Harmonics");

    waveformGroup.addAndMakeVisible (waveformPreview);

    evolveGroup.addAndMakeVisible (modEnvelopeEditor);



    configureKnob (filterGroup, cutoffSlider,    cutoffLabel,    "Cutoff");

    configureKnob (filterGroup, resonanceSlider, resonanceLabel, "Resonance");

    configureKnob (filterGroup, cutoffVelSensitivitySlider, cutoffVelSensitivityLabel, "Cutoff velocity");

    configureFilterComboControl (filterSlopeCombo, filterSlopeLabel, "Slope");
    filterGroup.addAndMakeVisible (filterSlopeCombo);
    filterGroup.addAndMakeVisible (filterSlopeLabel);

    configureFilterComboControl (filterLimiterCombo, filterLimiterLabel, "Limiter");
    filterGroup.addAndMakeVisible (filterLimiterCombo);
    filterGroup.addAndMakeVisible (filterLimiterLabel);

    filterSlopeCombo.addItem ("6 dB",  1);
    filterSlopeCombo.addItem ("12 dB", 2);
    filterSlopeCombo.addItem ("24 dB", 3);

    filterLimiterCombo.addItem ("Off", 1);
    filterLimiterCombo.addItem ("Light", 2);
    filterLimiterCombo.addItem ("Normal", 3);
    filterLimiterCombo.addItem ("Tight", 4);

    configureKnob (unisonGroup, unisonSlider, unisonLabel, "Voices");
    configureKnob (unisonGroup, unisonSpreadSlider, unisonSpreadLabel, "Amount");



    auto& apvts = audioProcessor.getApvts();



    amplitudeAttachment  = std::make_unique<SliderAttachment> (apvts, "amplitude",       amplitudeSlider);

    releaseTimeAttachment = std::make_unique<SliderAttachment> (apvts, "releaseTime",     releaseTimeSlider);

    amplitudeVelSensitivityAttachment = std::make_unique<SliderAttachment> (apvts, "amplitudeVelSensitivity", amplitudeVelSensitivitySlider);

    waveformAttachment   = std::make_unique<SliderAttachment> (apvts, "waveform",        waveformSlider);

    pulseWidthAttachment = std::make_unique<SliderAttachment> (apvts, "pulseWidth",      pulseWidthSlider);

    overtonesAttachment  = std::make_unique<SliderAttachment> (apvts, "overtones",       overtonesSlider);

    cutoffAttachment     = std::make_unique<SliderAttachment> (apvts, "filterCutoff",    cutoffSlider);

    resonanceAttachment  = std::make_unique<SliderAttachment> (apvts, "filterResonance", resonanceSlider);

    cutoffVelSensitivityAttachment = std::make_unique<SliderAttachment> (apvts, "cutoffVelSensitivity", cutoffVelSensitivitySlider);

    filterSlopeAttachment = std::make_unique<ComboBoxAttachment> (apvts, "filterSlope", filterSlopeCombo);

    filterLimiterAttachment = std::make_unique<ComboBoxAttachment> (apvts, "filterLimiter", filterLimiterCombo);

    unisonAttachment       = std::make_unique<SliderAttachment> (apvts, "unison", unisonSlider);
    unisonSpreadAttachment = std::make_unique<SliderAttachment> (apvts, "unisonSpread", unisonSpreadSlider);



    setWantsKeyboardFocus (false);

    setComponentIgnoresKeyboard (*this);

    setComponentIgnoresKeyboard (modEnvelopeEditor);



    setSize (700, 838);

}



void NafTachyonAudioProcessorEditor::configureFilterComboControl (juce::ComboBox& combo,
                                                                  juce::Label& label,
                                                                  const juce::String& labelText)
{
    combo.setWantsKeyboardFocus (false);
    combo.setMouseClickGrabsKeyboardFocus (false);
    combo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1a2024));
    combo.setColour (juce::ComboBox::textColourId, juce::Colour (0xffc8d0d6));
    combo.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff323a40));
    combo.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xffff8c1a));

    configureSectionLabel (label, labelText);
}



void NafTachyonAudioProcessorEditor::setComponentIgnoresKeyboard (juce::Component& component)

{

    component.setWantsKeyboardFocus (false);

    component.setMouseClickGrabsKeyboardFocus (false);



    for (auto* child : component.getChildren())

        if (child != nullptr)

            setComponentIgnoresKeyboard (*child);

}



void NafTachyonAudioProcessorEditor::mouseUp (const juce::MouseEvent& e)

{

    juce::AudioProcessorEditor::mouseUp (e);



    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())

    {

        if (focused == this || isParentOf (focused))

            if (auto* top = getTopLevelComponent())

                top->grabKeyboardFocus();

    }

}



NafTachyonAudioProcessorEditor::~NafTachyonAudioProcessorEditor()

{

    setLookAndFeel (nullptr);

}



void NafTachyonAudioProcessorEditor::configureKnob (juce::Component& parent,

                                                     juce::Slider& slider,

                                                     juce::Label& label,

                                                     const juce::String& text)

{

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);

    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);

    slider.setTextBoxIsEditable (false);

    slider.setWantsKeyboardFocus (false);

    slider.setMouseClickGrabsKeyboardFocus (false);

    parent.addAndMakeVisible (slider);



    configureSectionLabel (label, text);

    parent.addAndMakeVisible (label);

}



void NafTachyonAudioProcessorEditor::layoutMainControls (juce::Rectangle<int> area, int dialSize)

{

    const int columnWidth = area.getWidth() / 3;

    auto labelRow = area.removeFromTop (labelRowHeight);

    amplitudeLabel.setBounds (labelRow.removeFromLeft (columnWidth));

    releaseTimeLabel.setBounds (labelRow.removeFromLeft (columnWidth));

    amplitudeVelSensitivityLabel.setBounds (labelRow);

    layoutCentredDial (amplitudeSlider, area.removeFromLeft (columnWidth), dialSize);

    layoutCentredDial (releaseTimeSlider, area.removeFromLeft (columnWidth), dialSize);

    layoutCentredDial (amplitudeVelSensitivitySlider, area, dialSize);

}



int NafTachyonAudioProcessorEditor::unisonPanelWidthForDialSize (int dialSize) const
{
    return juce::jmax (200, dialSize * 2 + 60);
}

void NafTachyonAudioProcessorEditor::layoutUnisonControls (juce::Rectangle<int> area, int dialSize)
{
    const int columnWidth = area.getWidth() / 2;

    auto labelRow = area.removeFromTop (labelRowHeight);
    unisonLabel.setBounds (labelRow.removeFromLeft (columnWidth));
    unisonSpreadLabel.setBounds (labelRow);

    layoutCentredDial (unisonSlider, area.removeFromLeft (columnWidth), dialSize);
    layoutCentredDial (unisonSpreadSlider, area, dialSize);
}

void NafTachyonAudioProcessorEditor::layoutFilterControls (juce::Rectangle<int> area, int dialSize)

{

    const int columnWidth = area.getWidth() / 4;



    auto labelRow = area.removeFromTop (labelRowHeight);

    cutoffLabel.setBounds (labelRow.removeFromLeft (columnWidth));

    resonanceLabel.setBounds (labelRow.removeFromLeft (columnWidth));

    cutoffVelSensitivityLabel.setBounds (labelRow.removeFromLeft (columnWidth));

    filterSlopeLabel.setBounds (labelRow);



    layoutCentredDial (cutoffSlider, area.removeFromLeft (columnWidth), dialSize);

    layoutCentredDial (resonanceSlider, area.removeFromLeft (columnWidth), dialSize);

    layoutCentredDial (cutoffVelSensitivitySlider, area.removeFromLeft (columnWidth), dialSize);



    auto comboColumn = area.reduced (8, 4);
    comboColumn.removeFromTop (4);
    filterSlopeCombo.setBounds (comboColumn.removeFromTop (24));
    comboColumn.removeFromTop (6);
    filterLimiterLabel.setBounds (comboColumn.removeFromTop (labelRowHeight));
    filterLimiterCombo.setBounds (comboColumn.removeFromTop (24));

}



void NafTachyonAudioProcessorEditor::layoutWaveformControls (juce::Rectangle<int> area, int dialSize, int previewWidth)

{

    auto previewArea = area.removeFromRight (previewWidth);

    waveformPreview.setBounds (previewArea.reduced (2, 0));



    const int columnWidth = area.getWidth() / 3;



    auto shapeColumn = area.removeFromLeft (columnWidth);

    auto widthColumn = area.removeFromLeft (columnWidth);

    auto overtonesColumn = area;



    auto shapeLabelRow = shapeColumn.removeFromTop (labelRowHeight);

    waveformLabel.setBounds (shapeLabelRow);

    layoutCentredDial (waveformSlider, shapeColumn, dialSize);



    auto widthLabelRow = widthColumn.removeFromTop (labelRowHeight);

    pulseWidthLabel.setBounds (widthLabelRow);

    layoutCentredDial (pulseWidthSlider, widthColumn, dialSize);



    auto overtonesLabelRow = overtonesColumn.removeFromTop (labelRowHeight);

    overtonesLabel.setBounds (overtonesLabelRow);

    layoutCentredDial (overtonesSlider, overtonesColumn, dialSize);

}



void NafTachyonAudioProcessorEditor::paint (juce::Graphics& g)

{

    g.fillAll (NafTachyonLookAndFeel::getPluginBackgroundColour());

}



void NafTachyonAudioProcessorEditor::resized()

{

    auto layoutArea = getLocalBounds().reduced (20);

    {

        auto measureArea = layoutArea;

        const auto waveformArea = measureArea.removeFromBottom (waveformPanelHeight);

        waveformGroup.setBounds (waveformArea);

        const auto waveformContent = waveformGroup.getContentBounds();

        const auto previewWidth = waveformPreviewWidthForArea (waveformContent.getWidth());

        uniformDialSize = computeUniformDialSize (waveformContent, previewWidth);

    }



    auto area = getLocalBounds().reduced (20);

    const auto sectionPanelHeight = panelHeightForDialSize (uniformDialSize);

    const auto evolveArea = area.removeFromBottom (evolvePanelHeight);

    area.removeFromBottom (sectionGap);

    auto filterRow = area.removeFromBottom (sectionPanelHeight);

    area.removeFromBottom (sectionGap);

    const auto unisonWidth = unisonPanelWidthForDialSize (uniformDialSize);

    const auto unisonArea = filterRow.removeFromRight (unisonWidth);

    const auto filterArea = filterRow;

    const auto waveformArea = area.removeFromBottom (waveformPanelHeight);

    area.removeFromBottom (sectionGap);

    const auto mainArea = area;



    mainGroup.setBounds (mainArea);

    layoutMainControls (mainGroup.getContentBounds(), uniformDialSize);



    waveformGroup.setBounds (waveformArea);

    layoutWaveformControls (waveformGroup.getContentBounds(), uniformDialSize,

                            waveformPreviewWidthForArea (waveformGroup.getContentBounds().getWidth()));



    filterGroup.setBounds (filterArea);

    layoutFilterControls (filterGroup.getContentBounds(), uniformDialSize);

    unisonGroup.setBounds (unisonArea);

    layoutUnisonControls (unisonGroup.getContentBounds(), uniformDialSize);



    evolveGroup.setBounds (evolveArea);

    modEnvelopeEditor.setBounds (evolveGroup.getContentBounds());

}


