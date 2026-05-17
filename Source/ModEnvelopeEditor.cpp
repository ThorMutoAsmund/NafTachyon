/*
  ==============================================================================
*/

#include "ModEnvelopeEditor.h"

namespace
{
    constexpr float minTimeSpan = 0.25f;
    constexpr int toolbarHeight = 26;
    constexpr int editingRowHeight = 18;
    constexpr int timeAxisHeight = 16;
}

ModEnvelopeEditor::ModEnvelopeEditor (juce::AudioProcessorValueTreeState& apvtsToUse)
    : apvts (apvtsToUse)
{
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);

    enabledToggle.setWantsKeyboardFocus (false);
    enabledToggle.setMouseClickGrabsKeyboardFocus (false);
    addAndMakeVisible (enabledToggle);

    laneSelector.addItem ("Shape",     1);
    laneSelector.addItem ("Width",     2);
    laneSelector.addItem ("Overtones", 3);
    laneSelector.addItem ("Cutoff",    4);
    laneSelector.addItem ("Resonance", 5);
    laneSelector.setSelectedId (1, juce::dontSendNotification);
    laneSelector.onChange = [this]
    {
        activeLane = static_cast<Lane> (laneSelector.getSelectedId() - 1);
        updateEditingLabel();
        updateEnabledAttachment();
        refreshEnvelopeFromApvts();
        repaint();
    };
    laneSelector.setWantsKeyboardFocus (false);
    laneSelector.setMouseClickGrabsKeyboardFocus (false);
    addAndMakeVisible (laneSelector);

    addPointButton.onClick = [this]
    {
        const auto count = envelope.getNumPoints (activeLane);
        if (count >= ModulationEnvelope::maxPoints)
            return;

        const auto newIndex = count;
        const auto prevTime = apvts.getRawParameterValue (ModEnvelopeParamIds::pointTime (activeLane, count - 1))->load();
        const auto newTime = prevTime + 0.5f;

        if (auto* param = apvts.getParameter (ModEnvelopeParamIds::numPoints (activeLane)))
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (count + 1)));

        setPointTime (activeLane, newIndex, newTime);
        setPointValue (activeLane, newIndex, getPointValue (activeLane, count - 1));

        refreshEnvelopeFromApvts();
        repaint();
    };

    removePointButton.onClick = [this]
    {
        const auto count = envelope.getNumPoints (activeLane);
        if (count <= 2)
            return;

        if (auto* param = apvts.getParameter (ModEnvelopeParamIds::numPoints (activeLane)))
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (count - 1)));

        refreshEnvelopeFromApvts();
        repaint();
    };

    for (auto* button : { &addPointButton, &removePointButton })
    {
        button->setWantsKeyboardFocus (false);
        button->setMouseClickGrabsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }

    pointCountLabel.setJustificationType (juce::Justification::centredLeft);
    pointCountLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa3ab));
    pointCountLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (pointCountLabel);

    editingLabel.setJustificationType (juce::Justification::centredLeft);
    editingLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc8d0d6));
    editingLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (editingLabel);
    updateEditingLabel();
    updateEnabledAttachment();

    ModEnvelopeParamIds::syncAllFirstPointsFromKnobs (apvts);
    attachKnobListeners();
    refreshEnvelopeFromApvts();
    startTimerHz (15);
}

ModEnvelopeEditor::~ModEnvelopeEditor()
{
    for (const auto& id : { "waveform", "pulseWidth", "overtones", "filterCutoff", "filterResonance" })
        apvts.removeParameterListener (id, this);
}

bool ModEnvelopeEditor::isMainKnobParameter (const juce::String& parameterID)
{
    return parameterID == "waveform"
        || parameterID == "pulseWidth"
        || parameterID == "overtones"
        || parameterID == "filterCutoff"
        || parameterID == "filterResonance";
}

void ModEnvelopeEditor::attachKnobListeners()
{
    for (const auto& id : { "waveform", "pulseWidth", "overtones", "filterCutoff", "filterResonance" })
        apvts.addParameterListener (id, this);
}

void ModEnvelopeEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);

    if (! isMainKnobParameter (parameterID))
        return;

    ModEnvelopeParamIds::syncAllFirstPointsFromKnobs (apvts);
    refreshEnvelopeFromApvts();
    repaint();
}

void ModEnvelopeEditor::updateEnabledAttachment()
{
    enabledAttachment.reset();
    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ModEnvelopeParamIds::laneEnabled (activeLane), enabledToggle);
}

void ModEnvelopeEditor::updateEditingLabel()
{
    editingLabel.setText ("Editing: " + laneLabel (activeLane), juce::dontSendNotification);
}

void ModEnvelopeEditor::timerCallback()
{
    repaint();
}

void ModEnvelopeEditor::refreshEnvelopeFromApvts()
{
    envelope.updateFromApvts (apvts);

    pointCountLabel.setText ("Points: " + juce::String (envelope.getNumPoints (activeLane)),
                             juce::dontSendNotification);
}

void ModEnvelopeEditor::resized()
{
    auto area = getLocalBounds().reduced (4);

    auto toolbar = area.removeFromTop (toolbarHeight);
    enabledToggle.setBounds (toolbar.removeFromLeft (44));
    toolbar.removeFromLeft (6);
    laneSelector.setBounds (toolbar.removeFromLeft (108));
    toolbar.removeFromLeft (6);
    addPointButton.setBounds (toolbar.removeFromLeft (26));
    toolbar.removeFromLeft (4);
    removePointButton.setBounds (toolbar.removeFromLeft (26));
    toolbar.removeFromLeft (8);
    pointCountLabel.setBounds (toolbar);

    editingLabel.setBounds (area.removeFromTop (editingRowHeight));

    area.removeFromBottom (timeAxisHeight);
    graphBounds = area.toFloat().reduced (2.0f);
}

juce::Rectangle<float> ModEnvelopeEditor::getGraphBounds() const
{
    return graphBounds;
}

float ModEnvelopeEditor::laneToNormalized (Lane lane, float value) const
{
    switch (lane)
    {
        case Lane::shape:
        case Lane::overtones:
        case Lane::resonance:
            return juce::jlimit (0.0f, 1.0f, value);
        case Lane::width:
            return juce::jlimit (0.0f, 1.0f, (value + 1.0f) * 0.5f);
        case Lane::cutoff:
        {
            const auto logMin = std::log (20.0f);
            const auto logMax = std::log (20000.0f);
            return juce::jlimit (0.0f, 1.0f, (std::log (value) - logMin) / (logMax - logMin));
        }
    }

    return 0.0f;
}

float ModEnvelopeEditor::normalizedToLane (Lane lane, float normalized) const
{
    normalized = juce::jlimit (0.0f, 1.0f, normalized);

    switch (lane)
    {
        case Lane::shape:
        case Lane::overtones:
        case Lane::resonance:
            return normalized;
        case Lane::width:
            return normalized * 2.0f - 1.0f;
        case Lane::cutoff:
        {
            const auto logMin = std::log (20.0f);
            const auto logMax = std::log (20000.0f);
            return std::exp (juce::jmap (normalized, logMin, logMax));
        }
    }

    return 0.0f;
}

juce::String ModEnvelopeEditor::laneLabel (Lane lane) const
{
    switch (lane)
    {
        case Lane::shape:     return "Shape";
        case Lane::width:     return "Width";
        case Lane::overtones: return "Overtones";
        case Lane::cutoff:    return "Cutoff";
        case Lane::resonance: return "Resonance";
    }

    return {};
}

juce::Colour ModEnvelopeEditor::laneColour (Lane lane) const
{
    switch (lane)
    {
        case Lane::shape:     return juce::Colour (0xffff8c1a);
        case Lane::width:     return juce::Colour (0xff5eb8ff);
        case Lane::overtones: return juce::Colour (0xff9b7bff);
        case Lane::cutoff:    return juce::Colour (0xff5fd38d);
        case Lane::resonance: return juce::Colour (0xffff6b9d);
    }

    return juce::Colours::white;
}

float ModEnvelopeEditor::timeToX (float timeSeconds, juce::Rectangle<float> graph) const
{
    return timeToXForLane (timeSeconds, graph, activeLane);
}

float ModEnvelopeEditor::timeToXForLane (float timeSeconds, juce::Rectangle<float> graph, Lane lane) const
{
    const auto maxTime = juce::jmax (envelope.getMaxTimeSeconds (lane), minTimeSpan);
    return graph.getX() + (timeSeconds / maxTime) * graph.getWidth();
}

float ModEnvelopeEditor::xToTime (float x, juce::Rectangle<float> graph) const
{
    const auto maxTime = juce::jmax (envelope.getMaxTimeSeconds (activeLane), minTimeSpan);
    const auto proportion = juce::jlimit (0.0f, 1.0f, (x - graph.getX()) / graph.getWidth());
    return proportion * maxTime;
}

float ModEnvelopeEditor::valueToY (float normalized, juce::Rectangle<float> graph) const
{
    return graph.getBottom() - normalized * graph.getHeight();
}

float ModEnvelopeEditor::yToNormalized (float y, juce::Rectangle<float> graph) const
{
    return juce::jlimit (0.0f, 1.0f, (graph.getBottom() - y) / graph.getHeight());
}

float ModEnvelopeEditor::getPointValue (Lane lane, int index) const
{
    if (index == 0)
        return ModEnvelopeParamIds::readKnobValue (lane, apvts);

    return apvts.getRawParameterValue (ModEnvelopeParamIds::pointValue (lane, index))->load();
}

void ModEnvelopeEditor::setPointTime (Lane lane, int index, float timeSeconds)
{
    if (auto* param = apvts.getParameter (ModEnvelopeParamIds::pointTime (lane, index)))
        param->setValueNotifyingHost (param->convertTo0to1 (timeSeconds));
}

void ModEnvelopeEditor::setPointValue (Lane lane, int index, float value)
{
    if (auto* param = apvts.getParameter (ModEnvelopeParamIds::pointValue (lane, index)))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

int ModEnvelopeEditor::hitTestPoint (juce::Point<float> pos) const
{
    const auto graph = getGraphBounds();
    const auto hitRadius = 8.0f;
    const auto numPoints = envelope.getNumPoints (activeLane);

    for (int i = 0; i < numPoints; ++i)
    {
        const auto& point = envelope.getPoint (activeLane, i);
        const auto x = timeToX (point.timeSeconds, graph);
        const auto y = valueToY (laneToNormalized (activeLane, getPointValue (activeLane, i)), graph);

        if (pos.getDistanceFrom ({ x, y }) <= hitRadius)
            return i;
    }

    return -1;
}

void ModEnvelopeEditor::paint (juce::Graphics& g)
{
    refreshEnvelopeFromApvts();

    const auto graph = getGraphBounds();
    const auto numPoints = envelope.getNumPoints (activeLane);

    g.setColour (juce::Colour (0xff1a2024));
    g.fillRoundedRectangle (graph, 4.0f);

    g.setColour (juce::Colour (0xff323a40));
    g.drawRoundedRectangle (graph, 4.0f, 1.0f);

    for (int grid = 1; grid < 4; ++grid)
    {
        const auto y = graph.getY() + (graph.getHeight() * static_cast<float> (grid) / 4.0f);
        g.setColour (juce::Colour (0xff2a3238));
        g.drawHorizontalLine (juce::roundToInt (y), graph.getX(), graph.getRight());
    }

    const auto maxTime = juce::jmax (envelope.getMaxTimeSeconds (activeLane), minTimeSpan);
    const auto timeAxis = juce::Rectangle<float> (graph.getX(), graph.getBottom() + 2.0f,
                                                  graph.getWidth(), static_cast<float> (timeAxisHeight));
    g.setColour (juce::Colour (0xff6d767e));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("0 s", timeAxis.getX(), timeAxis.getY(), 36.0f, timeAxis.getHeight(), juce::Justification::centredLeft);
    g.drawText (juce::String (maxTime, 1) + " s",
                timeAxis.getRight() - 40.0f, timeAxis.getY(), 40.0f, timeAxis.getHeight(), juce::Justification::centredRight);

    for (int laneIndex = 0; laneIndex < ModulationEnvelope::numLanes; ++laneIndex)
    {
        const auto lane = static_cast<Lane> (laneIndex);
        const auto lanePointCount = envelope.getNumPoints (lane);
        juce::Path path;
        const auto laneIsEnabled = apvts.getRawParameterValue (ModEnvelopeParamIds::laneEnabled (lane))->load() >= 0.5f;
        const auto baseAlpha = laneIsEnabled ? (lane == activeLane ? 0.95f : 0.22f) : 0.1f;
        const auto colour = laneColour (lane).withAlpha (baseAlpha);
        const auto strokeWidth = lane == activeLane ? 2.0f : 1.0f;

        for (int i = 0; i < lanePointCount; ++i)
        {
            const auto& point = envelope.getPoint (lane, i);
            const auto x = timeToXForLane (point.timeSeconds, graph, lane);
            const auto y = valueToY (laneToNormalized (lane, getPointValue (lane, i)), graph);

            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }

        if (lanePointCount > 0)
        {
            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    for (int i = 0; i < numPoints; ++i)
    {
        const auto& point = envelope.getPoint (activeLane, i);
        const auto x = timeToX (point.timeSeconds, graph);
        const auto y = valueToY (laneToNormalized (activeLane, getPointValue (activeLane, i)), graph);
        const auto isActive = (i == drag.pointIndex);

        g.setColour (laneColour (activeLane).brighter (isActive ? 0.35f : 0.1f));
        g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colour (0xff1a2024));
        g.drawEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f, 1.0f);

        if (i == 0)
        {
            g.setColour (juce::Colour (0xff9aa3ab));
            g.setFont (juce::FontOptions (9.0f));
            const auto labelX = juce::jmin (x + 8.0f, graph.getRight() - 36.0f);
            const auto labelY = juce::jmax (graph.getY() + 2.0f, y - 18.0f);
            g.drawText ("start", labelX, labelY, 32.0f, 10.0f, juce::Justification::centredLeft);
        }
    }
}

void ModEnvelopeEditor::mouseDown (const juce::MouseEvent& e)
{
    drag.pointIndex = hitTestPoint (e.position);
    drag.active = drag.pointIndex >= 0;
}

void ModEnvelopeEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! drag.active || drag.pointIndex < 0)
        return;

    const auto graph = getGraphBounds();
    const auto index = drag.pointIndex;
    const auto numPoints = envelope.getNumPoints (activeLane);

    auto newTime = xToTime (e.position.x, graph);

    if (index == 0)
        newTime = 0.0f;
    else
        newTime = juce::jmax (newTime, apvts.getRawParameterValue (ModEnvelopeParamIds::pointTime (activeLane, index - 1))->load() + 0.01f);

    if (index < numPoints - 1)
        newTime = juce::jmin (newTime, apvts.getRawParameterValue (ModEnvelopeParamIds::pointTime (activeLane, index + 1))->load() - 0.01f);

    setPointTime (activeLane, index, newTime);

    if (index > 0)
    {
        const auto normalized = yToNormalized (e.position.y, graph);
        setPointValue (activeLane, index, normalizedToLane (activeLane, normalized));
    }

    refreshEnvelopeFromApvts();
    repaint();
}

void ModEnvelopeEditor::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    drag.active = false;
    drag.pointIndex = -1;
}
