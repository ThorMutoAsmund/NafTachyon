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
        { "modShape",     0.0f,      0.0f,      1.0f,      0.0f,  false },
        { "modWidth",     0.0f,     -1.0f,      1.0f,      0.0f,  false },
        { "modOvertones", 0.0f,      0.0f,      1.0f,      0.0f,  false },
        { "modCutoff",    20000.0f, 20.0f,  20000.0f,      0.3f,  true  },
        { "modResonance", 0.0f,      0.0f,      1.0f,      0.0f,  false },
    };

    ModulationEnvelope::Lane laneFromIndex (int index)
    {
        return static_cast<ModulationEnvelope::Lane> (index);
    }

    const LaneDefinition& getLaneDefinition (ModulationEnvelope::Lane lane)
    {
        return laneDefinitions[static_cast<size_t> (lane)];
    }
}

juce::String ModEnvelopeParamIds::laneEnabled (ModulationEnvelope::Lane lane)
{
    return juce::String (getLaneDefinition (lane).prefix) + "Enabled";
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
                valueRange = juce::NormalisableRange<float> { definition.minValue, definition.maxValue, 0.0f, definition.timeSkew };

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { pointValue (lane, i), 1 },
                juce::String (definition.prefix) + " pt " + juce::String (i + 1) + " value",
                valueRange,
                definition.defaultValue,
                definition.logScale ? juce::AudioParameterFloatAttributes().withLabel ("Hz")
                                    : juce::AudioParameterFloatAttributes()));
        }
    }
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
            point.value = apvts.getRawParameterValue (ModEnvelopeParamIds::pointValue (lane, i))->load();
        }

        for (int i = 1; i < laneEnvelope.numPoints; ++i)
        {
            if (laneEnvelope.points[i].timeSeconds <= laneEnvelope.points[i - 1].timeSeconds)
                laneEnvelope.points[i].timeSeconds = laneEnvelope.points[i - 1].timeSeconds + 0.001f;
        }

        laneEnvelope.points[0].timeSeconds = 0.0f;
    }
}

int ModulationEnvelope::getNumPoints (Lane lane) const
{
    return lanes[static_cast<size_t> (lane)].numPoints;
}

const ModLanePoint& ModulationEnvelope::getPoint (Lane lane, int index) const
{
    return lanes[static_cast<size_t> (lane)].points[static_cast<size_t> (index)];
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
    }

    return 0.0f;
}

float ModEnvelopeParamIds::readKnobValue (ModulationEnvelope::Lane lane, juce::AudioProcessorValueTreeState& apvts)
{
    switch (lane)
    {
        case ModulationEnvelope::Lane::shape:     return apvts.getRawParameterValue ("waveform")->load();
        case ModulationEnvelope::Lane::width:     return apvts.getRawParameterValue ("pulseWidth")->load();
        case ModulationEnvelope::Lane::overtones: return apvts.getRawParameterValue ("overtones")->load();
        case ModulationEnvelope::Lane::cutoff:    return apvts.getRawParameterValue ("filterCutoff")->load();
        case ModulationEnvelope::Lane::resonance: return apvts.getRawParameterValue ("filterResonance")->load();
    }

    return 0.0f;
}

ModKnobSnapshot ModEnvelopeParamIds::readKnobSnapshot (juce::AudioProcessorValueTreeState& apvts)
{
    ModKnobSnapshot snapshot;
    snapshot.shape = readKnobValue (ModulationEnvelope::Lane::shape, apvts);
    snapshot.pulseWidth = readKnobValue (ModulationEnvelope::Lane::width, apvts);
    snapshot.overtones = readKnobValue (ModulationEnvelope::Lane::overtones, apvts);
    snapshot.cutoffHz = readKnobValue (ModulationEnvelope::Lane::cutoff, apvts);
    snapshot.resonance = readKnobValue (ModulationEnvelope::Lane::resonance, apvts);
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
    for (int laneIndex = 0; laneIndex < ModulationEnvelope::numLanes; ++laneIndex)
        syncFirstPointFromKnob (static_cast<ModulationEnvelope::Lane> (laneIndex), apvts);
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
            const auto span = b.timeSeconds - a.timeSeconds;
            const auto t = span > 0.0f ? (elapsedSeconds - a.timeSeconds) / span : 0.0f;
            const auto valueA = (i == 0) ? startValue : a.value;
            return juce::jmap (t, valueA, b.value);
        }
    }

    return last.value;
}

ModulatedParams ModulationEnvelope::evaluate (float elapsedSeconds, const ModKnobSnapshot& knobSnapshot) const
{
    ModulatedParams result;
    result.shape = interpolateLane (lanes[static_cast<size_t> (Lane::shape)], elapsedSeconds, knobSnapshot.shape);
    result.pulseWidth = interpolateLane (lanes[static_cast<size_t> (Lane::width)], elapsedSeconds, knobSnapshot.pulseWidth);
    result.overtones = interpolateLane (lanes[static_cast<size_t> (Lane::overtones)], elapsedSeconds, knobSnapshot.overtones);
    result.cutoffHz = interpolateLane (lanes[static_cast<size_t> (Lane::cutoff)], elapsedSeconds, knobSnapshot.cutoffHz);
    result.resonance = interpolateLane (lanes[static_cast<size_t> (Lane::resonance)], elapsedSeconds, knobSnapshot.resonance);
    return result;
}
