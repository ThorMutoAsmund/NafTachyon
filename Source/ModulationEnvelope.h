/*
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

struct ModulatedParams
{
    float shape = 0.0f;
    float pulseWidth = 0.0f;
    float overtones = 0.0f;
    float cutoffHz = 20000.0f;
    float resonance = 0.0f;
};

struct ModLanePoint
{
    float timeSeconds = 0.0f;
    float value = 0.0f;
};

struct ModLaneEnvelope
{
    ModLanePoint points[6];
    float segmentCurves[5] {};
    int numPoints = 2;
};

struct ModKnobSnapshot;

class ModulationEnvelope
{
public:
    static constexpr int maxPoints = 6;
    static constexpr int maxSegments = maxPoints - 1;
    static constexpr int numLanes = 5;

    /** Segment curve in [-1, 1]: 0 = linear, positive = slow start, negative = slow end. */
    static float applySegmentT (float t, float curve);

    static float evaluateSegment (float valueA, float valueB, float t, float curve);

    static float sampleSegment (float timeA, float valueA, float timeB, float valueB,
                                float elapsedSeconds, float curve);

    enum class Lane
    {
        shape = 0,
        width,
        overtones,
        cutoff,
        resonance
    };

    void updateFromApvts (juce::AudioProcessorValueTreeState& apvts);

    ModulatedParams evaluate (float elapsedSeconds, const ModKnobSnapshot& knobSnapshot) const;

    int getNumPoints (Lane lane) const;

    const ModLanePoint& getPoint (Lane lane, int index) const;

    float getMaxTimeSeconds (Lane lane) const;

    bool isLaneEnabled (Lane lane, juce::AudioProcessorValueTreeState& apvts) const;

    const ModLaneEnvelope& getLane (Lane lane) const { return lanes[static_cast<size_t> (lane)]; }

    float getSegmentCurve (Lane lane, int segmentIndex) const;

private:
    static float interpolateLane (const ModLaneEnvelope& lane, float elapsedSeconds, float startValue);

    ModLaneEnvelope lanes[numLanes];
};

struct ModKnobSnapshot
{
    float shape = 0.0f;
    float pulseWidth = 0.0f;
    float overtones = 0.0f;
    float cutoffHz = 20000.0f;
    float resonance = 0.0f;

    float getValue (ModulationEnvelope::Lane lane) const;
};

namespace ModEnvelopeParamIds
{
    juce::String laneEnabled (ModulationEnvelope::Lane lane);

    juce::String numPoints (ModulationEnvelope::Lane lane);
    juce::String pointTime (ModulationEnvelope::Lane lane, int index);
    juce::String pointValue (ModulationEnvelope::Lane lane, int index);

    juce::String segmentCurve (ModulationEnvelope::Lane lane, int segmentIndex);

    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

    juce::String knobParameterId (ModulationEnvelope::Lane lane);

    float readKnobValue (ModulationEnvelope::Lane lane, juce::AudioProcessorValueTreeState& apvts);

    void setKnobValue (ModulationEnvelope::Lane lane, juce::AudioProcessorValueTreeState& apvts, float value);

    ModKnobSnapshot readKnobSnapshot (juce::AudioProcessorValueTreeState& apvts);

    void syncFirstPointFromKnob (ModulationEnvelope::Lane lane, juce::AudioProcessorValueTreeState& apvts);

    void syncAllFirstPointsFromKnobs (juce::AudioProcessorValueTreeState& apvts);
}
