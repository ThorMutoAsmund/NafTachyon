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



    constexpr float dialSizeUserScale = 0.75f;
    constexpr int layoutMinDialSize = 94;
    constexpr int layoutMaxDialSize = 130;
    constexpr int minDialSize = 70;
    constexpr int maxDialSize = 97;
    constexpr int mainControlColumns = 4;

    constexpr int labelRowHeight = 18;
    constexpr int sliderTextBoxHeight = 16;
    constexpr int labelToKnobGap = 2;

    constexpr int sectionChromeHeight = 46;

    constexpr int waveformOscRows = 2;
    constexpr int waveformOscKnobsPerRow = 5;
    constexpr int waveformRowLabelWidth = 36;

    constexpr int evolvePanelHeight = 294;

    constexpr int sectionGap = 6;

    constexpr int minWaveformPreviewWidth = 108;

    constexpr int waveformOscRowGap = 10;

    int knobStackHeightForDialSize (int dialSize)
    {
        return labelRowHeight + labelToKnobGap + dialSize + sliderTextBoxHeight;
    }

    int waveformPanelHeightForDialSize (int dialSize)
    {
        const auto rowStacks = knobStackHeightForDialSize (dialSize) * waveformOscRows;
        const auto rowGaps = waveformOscRowGap * (waveformOscRows - 1);
        return sectionChromeHeight + rowStacks + rowGaps;
    }

    int maxDialHeightInArea (int areaHeight)
    {
        return areaHeight - labelRowHeight - labelToKnobGap - sliderTextBoxHeight;
    }

    int waveformPreviewWidthForDialSize (int contentWidth, int dialSize)
    {
        const auto knobsWidth = waveformRowLabelWidth + waveformOscKnobsPerRow * dialSize;
        return juce::jmax (minWaveformPreviewWidth, contentWidth - knobsWidth);
    }

    int computeUniformDialSize (int contentWidth)
    {
        const auto mainHeightLimit = maxDialHeightInArea (knobStackHeightForDialSize (layoutMaxDialSize));
        const auto mainDialLimit = juce::jmin (contentWidth / mainControlColumns, mainHeightLimit);

        const auto waveColumnLimit = (contentWidth - minWaveformPreviewWidth - waveformRowLabelWidth)
                                   / waveformOscKnobsPerRow;

        auto dialSize = juce::jlimit (layoutMinDialSize, layoutMaxDialSize,
                                      juce::jmin (mainDialLimit, waveColumnLimit, contentWidth / 4));

        const auto unisonWidth = juce::jmax (200, dialSize * 2 + 60);
        const auto filterWidth = juce::jmax (1, contentWidth - unisonWidth);
        dialSize = juce::jlimit (layoutMinDialSize, layoutMaxDialSize,
                                 juce::jmin (dialSize, unisonWidth / 2, filterWidth / 4));

        return juce::jlimit (minDialSize, maxDialSize,
                             juce::roundToInt (static_cast<float> (dialSize) * dialSizeUserScale));
    }

    int panelHeightForDialSize (int dialSize)
    {
        return knobStackHeightForDialSize (dialSize) + sectionChromeHeight;
    }

    int mainPanelHeightForDialSize (int dialSize)
    {
        return panelHeightForDialSize (dialSize);
    }

    struct LabelKnobRows
    {
        juce::Rectangle<int> labelRow;
        juce::Rectangle<int> knobRow;
    };

    LabelKnobRows splitLabelAndKnobRows (juce::Rectangle<int>& area, int dialSize)
    {
        LabelKnobRows rows;
        rows.labelRow = area.removeFromTop (labelRowHeight);
        area.removeFromTop (labelToKnobGap);
        rows.knobRow = area.removeFromTop (dialSize + sliderTextBoxHeight);
        return rows;
    }

    void layoutKnobInColumn (juce::Slider& slider, juce::Rectangle<int> column, int dialSize)
    {
        const auto stackHeight = dialSize + sliderTextBoxHeight;
        const auto x = column.getX() + (column.getWidth() - dialSize) / 2;
        slider.setBounds (x, column.getY(), dialSize, stackHeight);
    }

    int minimumEditorHeightForDialSize (int dialSize)
    {
        return 40 + evolvePanelHeight + sectionGap * 3
             + mainPanelHeightForDialSize (dialSize)
             + waveformPanelHeightForDialSize (dialSize)
             + panelHeightForDialSize (dialSize);
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

    configureKnob (mainGroup, oscMixSlider, oscMixLabel, "Mix");

    configureToggleButton (oscSyncToggle);
    mainGroup.addAndMakeVisible (oscSyncToggle);

    configureKnob (mainGroup, releaseTimeSlider, releaseTimeLabel, "Release Time");

    configureKnob (mainGroup, amplitudeVelSensitivitySlider, amplitudeVelSensitivityLabel, "Amp velocity");



    configureKnob (waveformGroup, waveformSlider, waveformLabel, "Shape");

    configureKnob (waveformGroup, pulseWidthSlider, pulseWidthLabel, "Width");

    configureKnob (waveformGroup, overtonesSlider, overtonesLabel, "Harmonics");

    configureKnob (waveformGroup, osc1PitchSlider, osc1PitchLabel, "Pitch");
    configureKnob (waveformGroup, pitchTuneSlider, pitchTuneLabel, "Finetune");

    configureSectionLabel (osc1RowLabel, "OSC 1", 10.0f);
    configureSectionLabel (osc2RowLabel, "OSC 2", 10.0f);
    waveformGroup.addAndMakeVisible (osc1RowLabel);
    waveformGroup.addAndMakeVisible (osc2RowLabel);

    configureKnob (waveformGroup, osc2WaveformSlider, osc2WaveformLabel, "Shape");
    configureKnob (waveformGroup, osc2PulseWidthSlider, osc2PulseWidthLabel, "Width");
    configureKnob (waveformGroup, osc2OvertonesSlider, osc2OvertonesLabel, "Harmonics");
    configureKnob (waveformGroup, osc2PitchSlider, osc2PitchLabel, "Pitch");
    configureKnob (waveformGroup, osc2PitchTuneSlider, osc2PitchTuneLabel, "Finetune");

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

    osc1PitchAttachment    = std::make_unique<SliderAttachment> (apvts, "osc1Pitch",      osc1PitchSlider);
    pitchTuneAttachment    = std::make_unique<SliderAttachment> (apvts, "pitchTune",      pitchTuneSlider);
    osc2WaveformAttachment   = std::make_unique<SliderAttachment> (apvts, "osc2Waveform",   osc2WaveformSlider);
    osc2PulseWidthAttachment = std::make_unique<SliderAttachment> (apvts, "osc2PulseWidth", osc2PulseWidthSlider);
    osc2OvertonesAttachment  = std::make_unique<SliderAttachment> (apvts, "osc2Overtones",  osc2OvertonesSlider);
    osc2PitchAttachment      = std::make_unique<SliderAttachment> (apvts, "osc2Pitch",      osc2PitchSlider);
    osc2PitchTuneAttachment  = std::make_unique<SliderAttachment> (apvts, "osc2PitchTune",  osc2PitchTuneSlider);
    oscMixAttachment         = std::make_unique<SliderAttachment> (apvts, "oscMix",         oscMixSlider);
    oscSyncAttachment        = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "oscSync", oscSyncToggle);

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



    constexpr int editorWidth = 700;
    const auto dialSize = computeUniformDialSize (editorWidth - 40);
    setSize (editorWidth, minimumEditorHeightForDialSize (dialSize));

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



void NafTachyonAudioProcessorEditor::configureToggleButton (juce::ToggleButton& button)
{
    button.setWantsKeyboardFocus (false);
    button.setMouseClickGrabsKeyboardFocus (false);
    button.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffc8d0d6));
    button.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff8c1a));
    button.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff4a545c));
}

void NafTachyonAudioProcessorEditor::layoutWaveformOscRow (juce::Rectangle<int> rowArea,
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
                                                           juce::Label& fineTuneLabel)
{
    auto rowLabelArea = rowArea.removeFromLeft (waveformRowLabelWidth);
    rowLabel.setBounds (rowLabelArea);

    const int columnWidth = rowArea.getWidth() / waveformOscKnobsPerRow;

    auto rows = splitLabelAndKnobRows (rowArea, dialSize);

    shapeLabel.setBounds (rows.labelRow.removeFromLeft (columnWidth));
    widthLabel.setBounds (rows.labelRow.removeFromLeft (columnWidth));
    harmonicsLabel.setBounds (rows.labelRow.removeFromLeft (columnWidth));
    pitchLabel.setBounds (rows.labelRow.removeFromLeft (columnWidth));
    fineTuneLabel.setBounds (rows.labelRow);

    layoutKnobInColumn (shapeSlider, rows.knobRow.removeFromLeft (columnWidth), dialSize);
    layoutKnobInColumn (widthSlider, rows.knobRow.removeFromLeft (columnWidth), dialSize);
    layoutKnobInColumn (harmonicsSlider, rows.knobRow.removeFromLeft (columnWidth), dialSize);
    layoutKnobInColumn (pitchSlider, rows.knobRow.removeFromLeft (columnWidth), dialSize);
    layoutKnobInColumn (fineTuneSlider, rows.knobRow, dialSize);
}

void NafTachyonAudioProcessorEditor::configureKnob (juce::Component& parent,

                                                     juce::Slider& slider,

                                                     juce::Label& label,

                                                     const juce::String& text)

{

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);

    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, sliderTextBoxHeight);

    slider.setTextBoxIsEditable (false);

    slider.setWantsKeyboardFocus (false);

    slider.setMouseClickGrabsKeyboardFocus (false);

    parent.addAndMakeVisible (slider);



    configureSectionLabel (label, text);

    parent.addAndMakeVisible (label);

}



void NafTachyonAudioProcessorEditor::layoutMainControls (juce::Rectangle<int> area, int dialSize)

{

    const int columnWidth = area.getWidth() / mainControlColumns;

    auto rows = splitLabelAndKnobRows (area, dialSize);

    amplitudeLabel.setBounds (rows.labelRow.removeFromLeft (columnWidth));
    oscMixLabel.setBounds (rows.labelRow.removeFromLeft (columnWidth));
    releaseTimeLabel.setBounds (rows.labelRow.removeFromLeft (columnWidth));
    amplitudeVelSensitivityLabel.setBounds (rows.labelRow);

    layoutKnobInColumn (amplitudeSlider, rows.knobRow.removeFromLeft (columnWidth), dialSize);

    auto mixKnobColumn = rows.knobRow.removeFromLeft (columnWidth);
    layoutKnobInColumn (oscMixSlider, mixKnobColumn, dialSize);

    const auto releaseColumnX = rows.knobRow.getX();
    layoutKnobInColumn (releaseTimeSlider, rows.knobRow.removeFromLeft (columnWidth), dialSize);
    layoutKnobInColumn (amplitudeVelSensitivitySlider, rows.knobRow, dialSize);

    const auto syncWidth = juce::jmin (columnWidth, 52);
    const auto syncX = releaseColumnX - syncWidth / 2;
    oscSyncToggle.setBounds (syncX,
                             rows.knobRow.getY() + dialSize,
                             syncWidth,
                             sliderTextBoxHeight);

}



int NafTachyonAudioProcessorEditor::unisonPanelWidthForDialSize (int dialSize) const
{
    return juce::jmax (200, dialSize * 2 + 60);
}

void NafTachyonAudioProcessorEditor::layoutUnisonControls (juce::Rectangle<int> area, int dialSize)
{
    const int columnWidth = area.getWidth() / 2;

    auto rows = splitLabelAndKnobRows (area, dialSize);
    unisonLabel.setBounds (rows.labelRow.removeFromLeft (columnWidth));
    unisonSpreadLabel.setBounds (rows.labelRow);

    layoutKnobInColumn (unisonSlider, rows.knobRow.removeFromLeft (columnWidth), dialSize);
    layoutKnobInColumn (unisonSpreadSlider, rows.knobRow, dialSize);
}

void NafTachyonAudioProcessorEditor::layoutFilterControls (juce::Rectangle<int> area, int dialSize)

{

    const int columnWidth = area.getWidth() / 4;

    auto rows = splitLabelAndKnobRows (area, dialSize);

    cutoffLabel.setBounds (rows.labelRow.removeFromLeft (columnWidth));
    resonanceLabel.setBounds (rows.labelRow.removeFromLeft (columnWidth));
    cutoffVelSensitivityLabel.setBounds (rows.labelRow.removeFromLeft (columnWidth));
    filterSlopeLabel.setBounds (rows.labelRow.removeFromLeft (columnWidth));

    layoutKnobInColumn (cutoffSlider, rows.knobRow.removeFromLeft (columnWidth), dialSize);
    layoutKnobInColumn (resonanceSlider, rows.knobRow.removeFromLeft (columnWidth), dialSize);
    layoutKnobInColumn (cutoffVelSensitivitySlider, rows.knobRow.removeFromLeft (columnWidth), dialSize);

    auto comboColumn = rows.knobRow.reduced (6, 0);
    filterSlopeCombo.setBounds (comboColumn.removeFromTop (24));
    comboColumn.removeFromTop (4);
    filterLimiterLabel.setBounds (comboColumn.removeFromTop (labelRowHeight));
    comboColumn.removeFromTop (2);
    filterLimiterCombo.setBounds (comboColumn.removeFromTop (24));

}



void NafTachyonAudioProcessorEditor::layoutWaveformControls (juce::Rectangle<int> area, int dialSize, int previewWidth)

{

    auto previewArea = area.removeFromRight (previewWidth);

    waveformPreview.setBounds (previewArea);

    const auto rowHeight = knobStackHeightForDialSize (dialSize);

    auto osc1Row = area.removeFromTop (rowHeight);
    layoutWaveformOscRow (osc1Row,
                          dialSize,
                          osc1RowLabel,
                          waveformSlider,
                          waveformLabel,
                          pulseWidthSlider,
                          pulseWidthLabel,
                          overtonesSlider,
                          overtonesLabel,
                          osc1PitchSlider,
                          osc1PitchLabel,
                          pitchTuneSlider,
                          pitchTuneLabel);

    area.removeFromTop (waveformOscRowGap);

    auto osc2Row = area.removeFromTop (rowHeight);
    layoutWaveformOscRow (osc2Row,
                          dialSize,
                          osc2RowLabel,
                          osc2WaveformSlider,
                          osc2WaveformLabel,
                          osc2PulseWidthSlider,
                          osc2PulseWidthLabel,
                          osc2OvertonesSlider,
                          osc2OvertonesLabel,
                          osc2PitchSlider,
                          osc2PitchLabel,
                          osc2PitchTuneSlider,
                          osc2PitchTuneLabel);

}



void NafTachyonAudioProcessorEditor::paint (juce::Graphics& g)

{

    g.fillAll (NafTachyonLookAndFeel::getPluginBackgroundColour());

}



void NafTachyonAudioProcessorEditor::resized()

{

    auto area = getLocalBounds().reduced (20);

    const auto contentWidth = area.getWidth();

    uniformDialSize = computeUniformDialSize (contentWidth);

    const auto mainPanelHeight = mainPanelHeightForDialSize (uniformDialSize);
    const auto waveformPanelHeight = waveformPanelHeightForDialSize (uniformDialSize);
    const auto filterPanelHeight = panelHeightForDialSize (uniformDialSize);

    const auto previewWidth = waveformPreviewWidthForDialSize (contentWidth, uniformDialSize);

    const auto evolveArea = area.removeFromBottom (evolvePanelHeight);

    area.removeFromBottom (sectionGap);

    const auto unisonWidth = unisonPanelWidthForDialSize (uniformDialSize);

    auto filterRow = area.removeFromBottom (filterPanelHeight);

    area.removeFromBottom (sectionGap);

    const auto unisonArea = filterRow.removeFromRight (unisonWidth);

    const auto filterArea = filterRow;

    auto y = area.getY();

    mainGroup.setBounds (area.getX(), y, contentWidth, mainPanelHeight);

    layoutMainControls (mainGroup.getContentBounds(), uniformDialSize);

    y += mainPanelHeight + sectionGap;

    waveformGroup.setBounds (area.getX(), y, contentWidth, waveformPanelHeight);

    layoutWaveformControls (waveformGroup.getContentBounds(), uniformDialSize, previewWidth);

    y += waveformPanelHeight + sectionGap;

    filterGroup.setBounds (filterArea.getX(), y, filterArea.getWidth(), filterPanelHeight);

    layoutFilterControls (filterGroup.getContentBounds(), uniformDialSize);

    unisonGroup.setBounds (unisonArea.getX(), y, unisonArea.getWidth(), filterPanelHeight);

    layoutUnisonControls (unisonGroup.getContentBounds(), uniformDialSize);

    evolveGroup.setBounds (evolveArea);

    modEnvelopeEditor.setBounds (evolveGroup.getContentBounds());

}


