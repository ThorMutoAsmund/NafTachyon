/*
  ==============================================================================
*/

#include "ModEnvelopeEditor.h"

namespace
{
    /** Fixed graph timeline (matches mod point time parameter max). */
    constexpr float graphTimeMaxSeconds = 10.0f;
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
    laneSelector.addItem ("Harmonics", 3);
    laneSelector.addItem ("Cutoff",    4);
    laneSelector.addItem ("Resonance", 5);
    laneSelector.addItem ("Amplitude", 6);
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
    for (const auto& id : { "waveform", "pulseWidth", "overtones", "filterCutoff", "filterResonance", "amplitude" })
        apvts.removeParameterListener (id, this);
}

bool ModEnvelopeEditor::isMainKnobParameter (const juce::String& parameterID)
{
    return parameterID == "waveform"
        || parameterID == "pulseWidth"
        || parameterID == "overtones"
        || parameterID == "filterCutoff"
        || parameterID == "filterResonance"
        || parameterID == "amplitude";
}

void ModEnvelopeEditor::attachKnobListeners()
{
    for (const auto& id : { "waveform", "pulseWidth", "overtones", "filterCutoff", "filterResonance", "amplitude" })
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

    const auto lengthSeconds = envelope.getMaxTimeSeconds (activeLane);
    pointCountLabel.setText ("Points: " + juce::String (envelope.getNumPoints (activeLane))
                             + "  |  Length: " + juce::String (lengthSeconds, 1) + " s",
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
        case Lane::amplitude:
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
        case Lane::amplitude:
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
        case Lane::overtones: return "Harmonics";
        case Lane::cutoff:    return "Cutoff";
        case Lane::resonance: return "Resonance";
        case Lane::amplitude: return "Amplitude";
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
        case Lane::amplitude: return juce::Colour (0xfff0d060);
    }

    return juce::Colours::white;
}

float ModEnvelopeEditor::timeToX (float timeSeconds, juce::Rectangle<float> graph) const
{
    return timeToXForLane (timeSeconds, graph, activeLane);
}

float ModEnvelopeEditor::timeToXForLane (float timeSeconds, juce::Rectangle<float> graph, Lane lane) const
{
    juce::ignoreUnused (lane);
    return graph.getX() + (timeSeconds / graphTimeMaxSeconds) * graph.getWidth();
}

float ModEnvelopeEditor::xToTime (float x, juce::Rectangle<float> graph) const
{
    const auto proportion = juce::jlimit (0.0f, 1.0f, (x - graph.getX()) / graph.getWidth());
    return proportion * graphTimeMaxSeconds;
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
    if (index == 0)
    {
        ModEnvelopeParamIds::setKnobValue (lane, apvts, value);
        return;
    }

    if (auto* param = apvts.getParameter (ModEnvelopeParamIds::pointValue (lane, index)))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void ModEnvelopeEditor::setSegmentCurve (Lane lane, int segmentIndex, float curve)
{
    if (auto* param = apvts.getParameter (ModEnvelopeParamIds::segmentCurve (lane, segmentIndex)))
        param->setValueNotifyingHost (param->convertTo0to1 (juce::jlimit (-1.0f, 1.0f, curve)));
}

float ModEnvelopeEditor::getSegmentCurve (Lane lane, int segmentIndex) const
{
    return envelope.getSegmentCurve (lane, segmentIndex);
}

juce::Point<float> ModEnvelopeEditor::getSegmentHandlePosition (Lane lane, int segmentIndex, juce::Rectangle<float> graph) const
{
    const auto& ptA = envelope.getPoint (lane, segmentIndex);
    const auto& ptB = envelope.getPoint (lane, segmentIndex + 1);
    const auto valueA = getPointValue (lane, segmentIndex);
    const auto valueB = getPointValue (lane, segmentIndex + 1);
    const auto curve = envelope.getSegmentCurve (lane, segmentIndex);
    const auto timeMid = (ptA.timeSeconds + ptB.timeSeconds) * 0.5f;
    const auto valueMid = ModulationEnvelope::evaluateSegment (valueA, valueB, 0.5f, curve);

    return { timeToXForLane (timeMid, graph, lane),
             valueToY (laneToNormalized (lane, valueMid), graph) };
}

float ModEnvelopeEditor::curveFromHandleDrag (float startCurve, float startMouseY, juce::Point<float> pos,
                                              juce::Rectangle<float> graph) const
{
    const auto sensitivity = juce::jmax (graph.getHeight() * 0.18f, 28.0f);
    const auto delta = startMouseY - pos.y;

    return juce::jlimit (-1.0f, 1.0f, startCurve + delta / sensitivity);
}

void ModEnvelopeEditor::buildLanePath (juce::Path& path, Lane lane, juce::Rectangle<float> graph) const
{
    const auto numPoints = envelope.getNumPoints (lane);

    if (numPoints < 1)
        return;

    constexpr int stepsPerSegment = 40;

    const auto x0 = timeToXForLane (envelope.getPoint (lane, 0).timeSeconds, graph, lane);
    const auto y0 = valueToY (laneToNormalized (lane, getPointValue (lane, 0)), graph);
    path.startNewSubPath (x0, y0);

    for (int seg = 0; seg < numPoints - 1; ++seg)
    {
        const auto& ptA = envelope.getPoint (lane, seg);
        const auto& ptB = envelope.getPoint (lane, seg + 1);
        const auto valueA = getPointValue (lane, seg);
        const auto valueB = getPointValue (lane, seg + 1);
        const auto curve = envelope.getSegmentCurve (lane, seg);

        for (int step = 1; step <= stepsPerSegment; ++step)
        {
            const auto t = static_cast<float> (step) / static_cast<float> (stepsPerSegment);
            const auto time = juce::jmap (t, ptA.timeSeconds, ptB.timeSeconds);
            const auto value = ModulationEnvelope::evaluateSegment (valueA, valueB, t, curve);
            path.lineTo (timeToXForLane (time, graph, lane),
                         valueToY (laneToNormalized (lane, value), graph));
        }
    }
}

int ModEnvelopeEditor::hitTestSegment (juce::Point<float> pos) const
{
    const auto graph = getGraphBounds();
    const auto hitRadius = 7.0f;
    const auto numPoints = envelope.getNumPoints (activeLane);

    for (int seg = 0; seg < numPoints - 1; ++seg)
    {
        if (pos.getDistanceFrom (getSegmentHandlePosition (activeLane, seg, graph)) <= hitRadius)
            return seg;
    }

    return -1;
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

    const auto timeAxis = juce::Rectangle<float> (graph.getX(), graph.getBottom() + 2.0f,
                                                  graph.getWidth(), static_cast<float> (timeAxisHeight));
    g.setColour (juce::Colour (0xff6d767e));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("0 s", timeAxis.getX(), timeAxis.getY(), 36.0f, timeAxis.getHeight(), juce::Justification::centredLeft);
    g.drawText (juce::String (graphTimeMaxSeconds, 0) + " s",
                timeAxis.getRight() - 40.0f, timeAxis.getY(), 40.0f, timeAxis.getHeight(), juce::Justification::centredRight);

    const auto endTime = envelope.getMaxTimeSeconds (activeLane);
    if (endTime > 0.05f && endTime < graphTimeMaxSeconds)
    {
        const auto endX = timeToX (endTime, graph);
        g.setColour (juce::Colour (0xff5a6369));
        g.drawVerticalLine (juce::roundToInt (endX), graph.getY(), graph.getBottom());
    }

    for (int laneIndex = 0; laneIndex < ModulationEnvelope::numLanes; ++laneIndex)
    {
        const auto lane = static_cast<Lane> (laneIndex);
        const auto lanePointCount = envelope.getNumPoints (lane);
        juce::Path path;
        const auto laneIsEnabled = apvts.getRawParameterValue (ModEnvelopeParamIds::laneEnabled (lane))->load() >= 0.5f;
        const auto baseAlpha = laneIsEnabled ? (lane == activeLane ? 0.95f : 0.22f) : 0.1f;
        const auto colour = laneColour (lane).withAlpha (baseAlpha);
        const auto strokeWidth = lane == activeLane ? 2.0f : 1.0f;

        if (lanePointCount > 0)
        {
            buildLanePath (path, lane, graph);
            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    for (int i = 0; i < numPoints; ++i)
    {
        const auto& point = envelope.getPoint (activeLane, i);
        const auto x = timeToX (point.timeSeconds, graph);
        const auto y = valueToY (laneToNormalized (activeLane, getPointValue (activeLane, i)), graph);
        const auto isActive = (drag.target == DragTarget::point && i == drag.index);

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

    for (int seg = 0; seg < numPoints - 1; ++seg)
    {
        const auto handle = getSegmentHandlePosition (activeLane, seg, graph);
        const auto isActive = (drag.target == DragTarget::segment && seg == drag.index);
        const auto isCurved = std::abs (envelope.getSegmentCurve (activeLane, seg)) > 0.02f;

        g.setColour (laneColour (activeLane).withAlpha (isCurved ? 0.85f : 0.5f).brighter (isActive ? 0.25f : 0.0f));
        g.fillRoundedRectangle (handle.x - 4.0f, handle.y - 4.0f, 8.0f, 8.0f, 2.0f);
        g.setColour (juce::Colour (0xff1a2024));
        g.drawRoundedRectangle (handle.x - 4.0f, handle.y - 4.0f, 8.0f, 8.0f, 2.0f, 1.0f);
    }
}

void ModEnvelopeEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto pointHit = hitTestPoint (e.position);

    if (pointHit >= 0)
    {
        drag.target = DragTarget::point;
        drag.index = pointHit;
    }
    else
    {
        const auto segmentHit = hitTestSegment (e.position);
        drag.target = segmentHit >= 0 ? DragTarget::segment : DragTarget::none;
        drag.index = segmentHit;
    }

    drag.active = drag.target != DragTarget::none;

    if (drag.target == DragTarget::segment)
    {
        if (e.getNumberOfClicks() >= 2)
        {
            setSegmentCurve (activeLane, drag.index, 0.0f);
            refreshEnvelopeFromApvts();
            repaint();
        }
        else
        {
            drag.segmentDragStartCurve = envelope.getSegmentCurve (activeLane, drag.index);
            drag.segmentDragStartY = e.position.y;
        }
    }
}

void ModEnvelopeEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! drag.active || drag.index < 0)
        return;

    const auto graph = getGraphBounds();

    if (drag.target == DragTarget::segment)
    {
        setSegmentCurve (activeLane, drag.index,
                         curveFromHandleDrag (drag.segmentDragStartCurve, drag.segmentDragStartY, e.position, graph));
        refreshEnvelopeFromApvts();
        repaint();
        return;
    }

    const auto index = drag.index;
    const auto numPoints = envelope.getNumPoints (activeLane);

    auto newTime = xToTime (e.position.x, graph);

    if (index == 0)
        newTime = 0.0f;
    else
        newTime = juce::jmax (newTime, apvts.getRawParameterValue (ModEnvelopeParamIds::pointTime (activeLane, index - 1))->load() + 0.01f);

    if (index < numPoints - 1)
        newTime = juce::jmin (newTime, apvts.getRawParameterValue (ModEnvelopeParamIds::pointTime (activeLane, index + 1))->load() - 0.01f);

    setPointTime (activeLane, index, newTime);

    const auto normalized = yToNormalized (e.position.y, graph);
    setPointValue (activeLane, index, normalizedToLane (activeLane, normalized));

    refreshEnvelopeFromApvts();
    repaint();
}

void ModEnvelopeEditor::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    drag.active = false;
    drag.target = DragTarget::none;
    drag.index = -1;
}
