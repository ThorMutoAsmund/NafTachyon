/*
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ModulationEnvelope.h"

class ModEnvelopeEditor : public juce::Component,
                          private juce::Timer,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    using Lane = ModulationEnvelope::Lane;

    explicit ModEnvelopeEditor (juce::AudioProcessorValueTreeState& apvtsToUse);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    void timerCallback() override;

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    ~ModEnvelopeEditor() override;

private:
    enum class DragTarget
    {
        none,
        point,
        segment
    };

    struct DragState
    {
        DragTarget target = DragTarget::none;
        int index = -1;
        bool active = false;
        float segmentDragStartCurve = 0.0f;
        float segmentDragStartY = 0.0f;
    };

    float laneToNormalized (Lane lane, float value) const;
    float normalizedToLane (Lane lane, float normalized) const;
    juce::String laneLabel (Lane lane) const;
    juce::Colour laneColour (Lane lane) const;

    juce::Rectangle<float> getGraphBounds() const;
    juce::Rectangle<float> getLaneRowBounds (int rowIndex) const;
    float timeToX (float timeSeconds, juce::Rectangle<float> graph) const;
    float timeToXForLane (float timeSeconds, juce::Rectangle<float> graph, Lane lane) const;
    float xToTime (float x, juce::Rectangle<float> graph) const;
    float valueToY (float normalized, juce::Rectangle<float> graph) const;
    float yToNormalized (float y, juce::Rectangle<float> graph) const;

    int hitTestPoint (juce::Point<float> pos) const;
    int hitTestSegment (juce::Point<float> pos) const;
    int hitTestLaneStripRow (juce::Point<float> pos) const;

    void refreshEnvelopeFromApvts();
    void setPointTime (Lane lane, int index, float timeSeconds);
    void setPointValue (Lane lane, int index, float value);
    void setSegmentCurve (Lane lane, int segmentIndex, float curve);
    float getPointValue (Lane lane, int index) const;
    float getStoredPointValue (Lane lane, int index) const;
    float getSegmentCurve (Lane lane, int segmentIndex) const;

    bool isLastPointIndex (Lane lane, int index) const;
    /** Last point value mirrors first while loop is on; time remains editable. */
    bool isLastPointValueLockedForLoop (Lane lane) const;

    void setActiveLane (Lane lane);
    void paintLaneStrip (juce::Graphics& g);

    juce::Point<float> getSegmentHandlePosition (Lane lane, int segmentIndex, juce::Rectangle<float> graph) const;

    float curveFromHandleDrag (float startCurve, float startMouseY, juce::Point<float> pos,
                               juce::Rectangle<float> graph) const;

    void buildLanePath (juce::Path& path, Lane lane, juce::Rectangle<float> graph) const;

    juce::AudioProcessorValueTreeState& apvts;
    ModulationEnvelope envelope;

    Lane activeLane = Lane::amplitude;
    DragState drag;

    juce::ToggleButton enabledToggle { "On" };
    juce::ToggleButton loopToggle { "Loop" };
    juce::TextButton addPointButton { "+" };
    juce::TextButton removePointButton { "-" };
    juce::Label pointCountLabel;

    juce::Rectangle<float> graphBounds;
    juce::Rectangle<float> laneStripBounds;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopAttachment;

    void updateLaneToggleAttachments();

    void attachKnobListeners();

    static bool isMainKnobParameter (const juce::String& parameterID);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModEnvelopeEditor)
};
