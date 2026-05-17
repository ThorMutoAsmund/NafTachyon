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
    int numPoints = 2;
};

struct ModKnobSnapshot;

class ModulationEnvelope
{
public:
    static constexpr int maxPoints = 6;
    static constexpr int numLanes = 5;

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

    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

    float readKnobValue (ModulationEnvelope::Lane lane, juce::AudioProcessorValueTreeState& apvts);

    ModKnobSnapshot readKnobSnapshot (juce::AudioProcessorValueTreeState& apvts);

    void syncFirstPointFromKnob (ModulationEnvelope::Lane lane, juce::AudioProcessorValueTreeState& apvts);

    void syncAllFirstPointsFromKnobs (juce::AudioProcessorValueTreeState& apvts);
}
