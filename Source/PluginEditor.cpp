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

    constexpr int evolvePanelHeight = 196;

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
      modEnvelopeEditor (p.getApvts())

{

    setLookAndFeel (&customLookAndFeel);



    addAndMakeVisible (adsrGroup);

    addAndMakeVisible (waveformGroup);

    addAndMakeVisible (evolveGroup);

    addAndMakeVisible (filterGroup);



    configureKnob (adsrGroup, attackSlider,  attackLabel,  "Attack");

    configureKnob (adsrGroup, decaySlider,   decayLabel,   "Decay");

    configureKnob (adsrGroup, sustainSlider, sustainLabel, "Sustain");

    configureKnob (adsrGroup, releaseSlider, releaseLabel, "Release");



    configureKnob (waveformGroup, waveformSlider, waveformLabel, "Shape");

    configureKnob (waveformGroup, pulseWidthSlider, pulseWidthLabel, "Width");

    configureKnob (waveformGroup, overtonesSlider, overtonesLabel, "Overtones");

    waveformGroup.addAndMakeVisible (waveformPreview);

    evolveGroup.addAndMakeVisible (modEnvelopeEditor);



    configureKnob (filterGroup, cutoffSlider,    cutoffLabel,    "Cutoff");

    configureKnob (filterGroup, resonanceSlider, resonanceLabel, "Resonance");

    configureFilterSlopeControl (filterGroup);



    auto& apvts = audioProcessor.getApvts();



    attackAttachment     = std::make_unique<SliderAttachment> (apvts, "attack",          attackSlider);

    decayAttachment      = std::make_unique<SliderAttachment> (apvts, "decay",           decaySlider);

    sustainAttachment    = std::make_unique<SliderAttachment> (apvts, "sustain",         sustainSlider);

    releaseAttachment    = std::make_unique<SliderAttachment> (apvts, "release",         releaseSlider);

    waveformAttachment   = std::make_unique<SliderAttachment> (apvts, "waveform",        waveformSlider);

    pulseWidthAttachment = std::make_unique<SliderAttachment> (apvts, "pulseWidth",      pulseWidthSlider);

    overtonesAttachment  = std::make_unique<SliderAttachment> (apvts, "overtones",       overtonesSlider);

    cutoffAttachment     = std::make_unique<SliderAttachment> (apvts, "filterCutoff",    cutoffSlider);

    resonanceAttachment  = std::make_unique<SliderAttachment> (apvts, "filterResonance", resonanceSlider);

    filterSlopeAttachment = std::make_unique<ComboBoxAttachment> (apvts, "filterSlope", filterSlopeCombo);



    setWantsKeyboardFocus (false);

    setComponentIgnoresKeyboard (*this);

    setComponentIgnoresKeyboard (modEnvelopeEditor);



    setSize (580, 740);

}



void NafTachyonAudioProcessorEditor::configureFilterSlopeControl (juce::Component& parent)

{

    filterSlopeCombo.addItem ("6 dB",  1);

    filterSlopeCombo.addItem ("12 dB", 2);

    filterSlopeCombo.addItem ("24 dB", 3);

    filterSlopeCombo.setWantsKeyboardFocus (false);

    filterSlopeCombo.setMouseClickGrabsKeyboardFocus (false);

    filterSlopeCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1a2024));

    filterSlopeCombo.setColour (juce::ComboBox::textColourId, juce::Colour (0xffc8d0d6));

    filterSlopeCombo.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff323a40));

    filterSlopeCombo.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xffff8c1a));

    parent.addAndMakeVisible (filterSlopeCombo);



    configureSectionLabel (filterSlopeLabel, "Slope");

    parent.addAndMakeVisible (filterSlopeLabel);

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



void NafTachyonAudioProcessorEditor::layoutAdsrKnobs (juce::Rectangle<int> area, int dialSize)

{

    const int columnWidth = area.getWidth() / 4;



    auto labelRow = area.removeFromTop (labelRowHeight);

    attackLabel.setBounds  (labelRow.removeFromLeft (columnWidth));

    decayLabel.setBounds   (labelRow.removeFromLeft (columnWidth));

    sustainLabel.setBounds (labelRow.removeFromLeft (columnWidth));

    releaseLabel.setBounds (labelRow);



    layoutCentredDial (attackSlider,  area.removeFromLeft (columnWidth), dialSize);

    layoutCentredDial (decaySlider,   area.removeFromLeft (columnWidth), dialSize);

    layoutCentredDial (sustainSlider, area.removeFromLeft (columnWidth), dialSize);

    layoutCentredDial (releaseSlider, area, dialSize);

}



void NafTachyonAudioProcessorEditor::layoutFilterControls (juce::Rectangle<int> area, int dialSize)

{

    const int columnWidth = area.getWidth() / 3;



    auto labelRow = area.removeFromTop (labelRowHeight);

    cutoffLabel.setBounds (labelRow.removeFromLeft (columnWidth));

    resonanceLabel.setBounds (labelRow.removeFromLeft (columnWidth));

    filterSlopeLabel.setBounds (labelRow);



    layoutCentredDial (cutoffSlider, area.removeFromLeft (columnWidth), dialSize);

    layoutCentredDial (resonanceSlider, area.removeFromLeft (columnWidth), dialSize);



    auto slopeArea = area.reduced (8, 4);

    slopeArea.removeFromTop (8);

    filterSlopeCombo.setBounds (slopeArea.removeFromTop (26));

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

    const auto filterArea = area.removeFromBottom (sectionPanelHeight);

    area.removeFromBottom (sectionGap);

    const auto waveformArea = area.removeFromBottom (waveformPanelHeight);

    area.removeFromBottom (sectionGap);

    const auto adsrArea = area;



    adsrGroup.setBounds (adsrArea);

    layoutAdsrKnobs (adsrGroup.getContentBounds(), uniformDialSize);



    waveformGroup.setBounds (waveformArea);

    layoutWaveformControls (waveformGroup.getContentBounds(), uniformDialSize,

                            waveformPreviewWidthForArea (waveformGroup.getContentBounds().getWidth()));



    filterGroup.setBounds (filterArea);

    layoutFilterControls (filterGroup.getContentBounds(), uniformDialSize);



    evolveGroup.setBounds (evolveArea);

    modEnvelopeEditor.setBounds (evolveGroup.getContentBounds());

}


