/*
  ==============================================================================
*/

#include "ModulationEnvelope.h"

namespace
{
    struct LaneDefinition
    {
        const char* prefix;
        float defaultValue;
        float minValue;
        float maxValue;
        float timeSkew;
        bool logScale;
    };

    constexpr LaneDefinition laneDefinitions[ModulationEnvelope::numLanes] =
    {
        { "modShape",     1.0f,      0.0f,      1.0f,      0.0f,  false }, // multiplier on shape knob
        { "modWidth",     0.0f,     -1.0f,      1.0f,      0.0f,  false }, // offset added to width knob
        { "modOvertones", 1.0f,      0.0f,      1.0f,      0.0f,  false }, // multiplier on harmonics knob
        { "modCutoff",    1.0f,      0.0f,      1.0f,      0.0f,  false }, // multiplier on cutoff knob
        { "modResonance", 1.0f,      0.0f,      1.0f,      0.0f,  false }, // multiplier on resonance knob
        { "modAmplitude", 1.0f,      0.0f,      1.0f,      0.0f,  false }, // multiplier on amplitude knob
    };

    ModulationEnvelope::Lane laneFromIndex (int index)
    {
        return static_cast<ModulationEnvelope::Lane> (index);
    }

    const LaneDefinition& getLaneDefinition (ModulationEnvelope::Lane lane)
    {
        return laneDefinitions[static_cast<size_t> (lane)];
    }

    bool isKnobMultiplierLane (ModulationEnvelope::Lane lane)
    {
        switch (lane)
        {
            case ModulationEnvelope::Lane::shape:
            case ModulationEnvelope::Lane::overtones:
            case ModulationEnvelope::Lane::cutoff:
            case ModulationEnvelope::Lane::resonance:
            case ModulationEnvelope::Lane::amplitude:
                return true;
            case ModulationEnvelope::Lane::width:
                return false;
        }

        return false;
    }

    bool isKnobOffsetLane (ModulationEnvelope::Lane lane)
    {
        return lane == ModulationEnvelope::Lane::width;
    }
}

juce::String ModEnvelopeParamIds::laneEnabled (ModulationEnvelope::Lane lane)
{
    return juce::String (getLaneDefinition (lane).prefix) + "Enabled";
}

juce::String ModEnvelopeParamIds::laneLoop (ModulationEnvelope::Lane lane)
{
    return juce::String (getLaneDefinition (lane).prefix) + "Loop";
}

juce::String ModEnvelopeParamIds::numPoints (ModulationEnvelope::Lane lane)
{
    return juce::String (getLaneDefinition (lane).prefix) + "NumPoints";
}

juce::String ModEnvelopeParamIds::pointTime (ModulationEnvelope::Lane lane, int index)
{
    return juce::String (getLaneDefinition (lane).prefix) + "Pt" + juce::String (index) + "_time";
}

juce::String ModEnvelopeParamIds::pointValue (ModulationEnvelope::Lane lane, int index)
{
    return juce::String (getLaneDefinition (lane).prefix) + "Pt" + juce::String (index) + "_value";
}

juce::String ModEnvelopeParamIds::segmentCurve (ModulationEnvelope::Lane lane, int segmentIndex)
{
    return juce::String (getLaneDefinition (lane).prefix) + "Seg" + juce::String (segmentIndex) + "_curve";
}

void ModEnvelopeParamIds::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    for (int laneIndex = 0; laneIndex < ModulationEnvelope::numLanes; ++laneIndex)
    {
        const auto lane = laneFromIndex (laneIndex);
        const auto& definition = getLaneDefinition (lane);

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { laneEnabled (lane), 1 },
            juce::String (definition.prefix) + " enabled",
            false));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { laneLoop (lane), 1 },
            juce::String (definition.prefix) + " loop",
            false));

        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { numPoints (lane), 1 },
            juce::String (definition.prefix) + " Points",
            2, ModulationEnvelope::maxPoints, 2));

        for (int i = 0; i < ModulationEnvelope::maxPoints; ++i)
        {
            const auto defaultTime = (i == 0) ? 0.0f : (i == 1 ? 2.0f : 2.0f + static_cast<float> (i - 1));

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { pointTime (lane, i), 1 },
                juce::String (definition.prefix) + " pt " + juce::String (i + 1) + " time",
                juce::NormalisableRange<float> { 0.0f, 10.0f, 0.001f, 0.35f },
                defaultTime,
                juce::AudioParameterFloatAttributes().withLabel ("s")));

            juce::NormalisableRange<float> valueRange { definition.minValue, definition.maxValue, 0.001f };

            if (definition.logScale)
            {
                valueRange = juce::NormalisableRange<float> { definition.minValue, definition.maxValue };
                valueRange.setSkewForCentre (std::sqrt (definition.minValue * definition.maxValue));
            }

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { pointValue (lane, i), 1 },
                juce::String (definition.prefix) + " pt " + juce::String (i + 1) + " value",
                valueRange,
                definition.defaultValue,
                definition.logScale ? juce::AudioParameterFloatAttributes().withLabel ("Hz")
                                    : juce::AudioParameterFloatAttributes()));
        }

        for (int seg = 0; seg < ModulationEnvelope::maxSegments; ++seg)
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { segmentCurve (lane, seg), 1 },
                juce::String (definition.prefix) + " seg " + juce::String (seg + 1) + " curve",
                juce::NormalisableRange<float> { -1.0f, 1.0f, 0.001f },
                0.0f));
        }
    }
}

float ModulationEnvelope::applySegmentT (float t, float curve)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    curve = juce::jlimit (-1.0f, 1.0f, curve);

    if (std::abs (curve) < 0.001f)
        return t;

    // Exponential ramp: (e^(k*t) - 1) / (e^k - 1)  →  t when k → 0 (straight line at handle centre).
    constexpr float kMax = 7.0f;
    const auto k = -curve * kMax;

    if (std::abs (k) < 1.0e-5f)
        return t;

    const auto expK = std::exp (k);
    return (std::exp (k * t) - 1.0f) / (expK - 1.0f);
}

float ModulationEnvelope::evaluateSegment (float valueA, float valueB, float t, float curve)
{
    // Flip curve when the segment falls so drag-up / drag-down match screen direction.
    if (valueB < valueA)
        curve = -curve;

    return juce::jmap (applySegmentT (t, curve), valueA, valueB);
}

float ModulationEnvelope::sampleSegment (float timeA, float valueA, float timeB, float valueB,
                                         float elapsedSeconds, float curve)
{
    const auto span = timeB - timeA;

    if (span <= 0.0f)
        return valueB;

    const auto t = juce::jlimit (0.0f, 1.0f, (elapsedSeconds - timeA) / span);
    return evaluateSegment (valueA, valueB, t, curve);
}

void ModulationEnvelope::updateFromApvts (juce::AudioProcessorValueTreeState& apvts)
{
    for (int laneIndex = 0; laneIndex < numLanes; ++laneIndex)
    {
        const auto lane = laneFromIndex (laneIndex);
        auto& laneEnvelope = lanes[static_cast<size_t> (laneIndex)];

        laneEnvelope.numPoints = juce::jlimit (2, maxPoints,
                                               static_cast<int> (std::round (apvts.getRawParameterValue (ModEnvelopeParamIds::numPoints (lane))->load())));

        for (int i = 0; i < maxPoints; ++i)
        {
            auto& point = laneEnvelope.points[i];
            point.timeSeconds = apvts.getRawParameterValue (ModEnvelopeParamIds::pointTime (lane, i))->load();
            auto value = apvts.getRawParameterValue (ModEnvelopeParamIds::pointValue (lane, i))->load();

            if (isKnobMultiplierLane (lane) && (value > 1.0f || value < 0.0f))
                value = 1.0f;
            else if (isKnobOffsetLane (lane))
                value = juce::jlimit (-1.0f, 1.0f, value);

            point.value = value;
        }

        for (int seg = 0; seg < maxSegments; ++seg)
            laneEnvelope.segmentCurves[seg] = apvts.getRawParameterValue (ModEnvelopeParamIds::segmentCurve (lane, seg))->load();

        for (int i = 1; i < laneEnvelope.numPoints; ++i)
        {
            if (laneEnvelope.points[i].timeSeconds <= laneEnvelope.points[i - 1].timeSeconds)
                laneEnvelope.points[i].timeSeconds = laneEnvelope.points[i - 1].timeSeconds + 0.001f;
        }

        laneEnvelope.points[0].timeSeconds = 0.0f;

        if (auto* loopParam = apvts.getRawParameterValue (ModEnvelopeParamIds::laneLoop (lane)))
            laneEnvelope.loopEnabled = loopParam->load() >= 0.5f;
    }
}

bool ModulationEnvelope::isLaneLoopEnabled (Lane lane) const
{
    return lanes[static_cast<size_t> (lane)].loopEnabled;
}

int ModulationEnvelope::getNumPoints (Lane lane) const
{
    return lanes[static_cast<size_t> (lane)].numPoints;
}

const ModLanePoint& ModulationEnvelope::getPoint (Lane lane, int index) const
{
    return lanes[static_cast<size_t> (lane)].points[static_cast<size_t> (index)];
}

float ModulationEnvelope::getSegmentCurve (Lane lane, int segmentIndex) const
{
    return lanes[static_cast<size_t> (lane)].segmentCurves[static_cast<size_t> (segmentIndex)];
}

float ModulationEnvelope::getMaxTimeSeconds (Lane lane) const
{
    const auto& laneEnvelope = lanes[static_cast<size_t> (lane)];
    return laneEnvelope.points[static_cast<size_t> (laneEnvelope.numPoints - 1)].timeSeconds;
}

bool ModulationEnvelope::isLaneEnabled (Lane lane, juce::AudioProcessorValueTreeState& apvts) const
{
    return apvts.getRawParameterValue (ModEnvelopeParamIds::laneEnabled (lane))->load() >= 0.5f;
}

float ModKnobSnapshot::getValue (ModulationEnvelope::Lane lane) const
{
    switch (lane)
    {
        case ModulationEnvelope::Lane::shape:     return shape;
        case ModulationEnvelope::Lane::width:     return pulseWidth;
        case ModulationEnvelope::Lane::overtones: return overtones;
        case ModulationEnvelope::Lane::cutoff:    return cutoffHz;
        case ModulationEnvelope::Lane::resonance: return resonance;
        case ModulationEnvelope::Lane::amplitude: return amplitude;
    }

    return 0.0f;
}

juce::String ModEnvelopeParamIds::knobParameterId (ModulationEnvelope::Lane lane)
{
    switch (lane)
    {
        case ModulationEnvelope::Lane::shape:     return "waveform";
        case ModulationEnvelope::Lane::width:     return "pulseWidth";
        case ModulationEnvelope::Lane::overtones: return "overtones";
        case ModulationEnvelope::Lane::cutoff:    return "filterCutoff";
        case ModulationEnvelope::Lane::resonance: return "filterResonance";
        case ModulationEnvelope::Lane::amplitude: return "amplitude";
    }

    return {};
}

float ModEnvelopeParamIds::readKnobValue (ModulationEnvelope::Lane lane, juce::AudioProcessorValueTreeState& apvts)
{
    if (auto* param = apvts.getRawParameterValue (knobParameterId (lane)))
        return param->load();

    return 0.0f;
}

void ModEnvelopeParamIds::setKnobValue (ModulationEnvelope::Lane lane, juce::AudioProcessorValueTreeState& apvts, float value)
{
    if (auto* param = apvts.getParameter (knobParameterId (lane)))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

ModKnobSnapshot ModEnvelopeParamIds::readKnobSnapshot (juce::AudioProcessorValueTreeState& apvts)
{
    ModKnobSnapshot snapshot;
    snapshot.shape = readKnobValue (ModulationEnvelope::Lane::shape, apvts);
    snapshot.pulseWidth = readKnobValue (ModulationEnvelope::Lane::width, apvts);
    snapshot.overtones = readKnobValue (ModulationEnvelope::Lane::overtones, apvts);
    snapshot.cutoffHz = readKnobValue (ModulationEnvelope::Lane::cutoff, apvts);
    snapshot.resonance = readKnobValue (ModulationEnvelope::Lane::resonance, apvts);
    snapshot.amplitude = readKnobValue (ModulationEnvelope::Lane::amplitude, apvts);
    return snapshot;
}

void ModEnvelopeParamIds::syncFirstPointFromKnob (ModulationEnvelope::Lane lane, juce::AudioProcessorValueTreeState& apvts)
{
    if (auto* param = apvts.getParameter (pointValue (lane, 0)))
    {
        const auto knobValue = readKnobValue (lane, apvts);
        param->setValueNotifyingHost (param->convertTo0to1 (knobValue));
    }
}

void ModEnvelopeParamIds::syncAllFirstPointsFromKnobs (juce::AudioProcessorValueTreeState& apvts)
{
    juce::ignoreUnused (apvts);
}

float ModulationEnvelope::mapLoopingLaneTime (const ModLaneEnvelope& lane, float elapsedSeconds, bool loop)
{
    if (! loop || lane.numPoints < 2)
        return elapsedSeconds;

    const auto loopStart = lane.points[0].timeSeconds;
    const auto loopEnd = lane.points[static_cast<size_t> (lane.numPoints - 1)].timeSeconds;
    const auto loopLength = loopEnd - loopStart;

    if (loopLength <= 1.0e-6f)
        return elapsedSeconds;

    if (elapsedSeconds <= loopEnd)
        return elapsedSeconds;

    return loopStart + std::fmod (elapsedSeconds - loopStart, loopLength);
}

float ModulationEnvelope::getEffectivePointValue (const ModLaneEnvelope& lane, int index, bool loop)
{
    const auto safeIndex = juce::jlimit (0, lane.numPoints - 1, index);

    if (loop && lane.numPoints >= 2 && safeIndex == lane.numPoints - 1)
        return lane.points[0].value;

    return lane.points[static_cast<size_t> (safeIndex)].value;
}

float ModulationEnvelope::interpolateLaneAbsolute (const ModLaneEnvelope& lane, float elapsedSeconds, bool loop)
{
    const auto laneTime = mapLoopingLaneTime (lane, elapsedSeconds, loop);
    const auto lastIndex = lane.numPoints - 1;

    if (lane.numPoints <= 1 || laneTime <= lane.points[0].timeSeconds)
        return getEffectivePointValue (lane, 0, loop);

    const auto& last = lane.points[static_cast<size_t> (lastIndex)];

    if (laneTime >= last.timeSeconds)
        return getEffectivePointValue (lane, lastIndex, loop);

    for (int i = 0; i < lane.numPoints - 1; ++i)
    {
        const auto& a = lane.points[static_cast<size_t> (i)];
        const auto& b = lane.points[static_cast<size_t> (i + 1)];

        if (laneTime >= a.timeSeconds && laneTime < b.timeSeconds)
        {
            const auto valueA = getEffectivePointValue (lane, i, loop);
            const auto valueB = getEffectivePointValue (lane, i + 1, loop);
            const auto curve = lane.segmentCurves[static_cast<size_t> (i)];
            return sampleSegment (a.timeSeconds, valueA, b.timeSeconds, valueB, laneTime, curve);
        }
    }

    return getEffectivePointValue (lane, lastIndex, loop);
}

float ModulationEnvelope::interpolateLane (const ModLaneEnvelope& lane, float elapsedSeconds, float startValue)
{
    const auto& first = lane.points[0];

    if (lane.numPoints <= 1 || elapsedSeconds <= first.timeSeconds)
        return startValue;

    const auto& last = lane.points[static_cast<size_t> (lane.numPoints - 1)];

    if (elapsedSeconds >= last.timeSeconds)
        return last.value;

    for (int i = 0; i < lane.numPoints - 1; ++i)
    {
        const auto& a = lane.points[static_cast<size_t> (i)];
        const auto& b = lane.points[static_cast<size_t> (i + 1)];

        if (elapsedSeconds >= a.timeSeconds && elapsedSeconds < b.timeSeconds)
        {
            const auto valueA = (i == 0) ? startValue : a.value;
            const auto curve = lane.segmentCurves[static_cast<size_t> (i)];
            return sampleSegment (a.timeSeconds, valueA, b.timeSeconds, b.value, elapsedSeconds, curve);
        }
    }

    return last.value;
}

ModulatedParams ModulationEnvelope::evaluate (float elapsedSeconds, const ModKnobSnapshot& knobSnapshot) const
{
    ModulatedParams result;
    const auto& shapeLane = lanes[static_cast<size_t> (Lane::shape)];
    const auto shapeMultiplier = interpolateLaneAbsolute (shapeLane, elapsedSeconds, shapeLane.loopEnabled);
    result.shape = juce::jlimit (0.0f, 1.0f, knobSnapshot.shape * shapeMultiplier);

    const auto& widthLane = lanes[static_cast<size_t> (Lane::width)];
    const auto widthOffset = interpolateLaneAbsolute (widthLane, elapsedSeconds, widthLane.loopEnabled);
    result.pulseWidth = juce::jlimit (-1.0f, 1.0f, knobSnapshot.pulseWidth + widthOffset);

    const auto& overtonesLane = lanes[static_cast<size_t> (Lane::overtones)];
    const auto overtonesMultiplier = interpolateLaneAbsolute (overtonesLane, elapsedSeconds, overtonesLane.loopEnabled);
    result.overtones = juce::jlimit (0.0f, 1.0f, knobSnapshot.overtones * overtonesMultiplier);

    const auto& cutoffLane = lanes[static_cast<size_t> (Lane::cutoff)];
    const auto cutoffMultiplier = interpolateLaneAbsolute (cutoffLane, elapsedSeconds, cutoffLane.loopEnabled);
    result.cutoffHz = juce::jlimit (20.0f, 20000.0f, knobSnapshot.cutoffHz * cutoffMultiplier);

    const auto& resonanceLane = lanes[static_cast<size_t> (Lane::resonance)];
    const auto resonanceMultiplier = interpolateLaneAbsolute (resonanceLane, elapsedSeconds, resonanceLane.loopEnabled);
    result.resonance = juce::jlimit (0.0f, 1.0f, knobSnapshot.resonance * resonanceMultiplier);

    const auto& amplitudeLane = lanes[static_cast<size_t> (Lane::amplitude)];
    const auto amplitudeMultiplier = interpolateLaneAbsolute (amplitudeLane, elapsedSeconds, amplitudeLane.loopEnabled);
    result.amplitude = juce::jlimit (0.0f, 1.0f, knobSnapshot.amplitude * amplitudeMultiplier);
    return result;
}
