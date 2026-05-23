/*
  ==============================================================================
*/

#include "WaveformPreview.h"
#include "WaveformSynth.h"

#include <limits>

namespace
{
    constexpr auto waveformParamId = "waveform";
    constexpr auto pulseWidthParamId = "pulseWidth";
    constexpr auto overtonesParamId = "overtones";
    constexpr auto osc2WaveformParamId = "osc2Waveform";
    constexpr auto osc2PulseWidthParamId = "osc2PulseWidth";
    constexpr auto osc2OvertonesParamId = "osc2Overtones";
    constexpr auto osc1PitchParamId = "osc1Pitch";
    constexpr auto osc2PitchParamId = "osc2Pitch";
    constexpr auto oscMixParamId = "oscMix";

    constexpr int maxPreviewColumns = 128;
    constexpr int previewSamplesPerColumn = 8;
    constexpr int previewSamplesPerColumnHarmonics = 16;

    float mixOscillatorSamples (float osc1Sample, float osc2Sample, float mix)
    {
        const auto osc2Mix = juce::jlimit (0.0f, 1.0f, mix);
        const auto osc1Mix = 1.0f - osc2Mix;
        const auto sum = osc1Mix + osc2Mix;

        if (sum < 1.0e-5f)
            return 0.0f;

        return (osc1Sample * osc1Mix + osc2Sample * osc2Mix) / sum;
    }

    float samplePreviewWave (double t,
                             double basePhaseIncrement,
                             float pitchSemitones,
                             float morph,
                             float pulseWidth,
                             float overtones)
    {
        const auto phaseIncrement = basePhaseIncrement
                                  * std::pow (2.0, static_cast<double> (pitchSemitones) / 12.0);
        const auto phase = t * juce::MathConstants<double>::twoPi;
        const auto subPhase = phase * 0.5;
        const auto fifthPhase = phase * WaveformSynth::perfectFifthRatio;

        return WaveformSynth::computeOscillatorSample (phase,
                                                       phaseIncrement,
                                                       subPhase,
                                                       phaseIncrement * 0.5,
                                                       fifthPhase,
                                                       phaseIncrement * WaveformSynth::perfectFifthRatio,
                                                       morph,
                                                       pulseWidth,
                                                       overtones);
    }
}

WaveformPreview::WaveformPreview (juce::AudioProcessorValueTreeState& apvtsToUse)
    : apvts (apvtsToUse)
{
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
    attachParameterListeners();
}

WaveformPreview::~WaveformPreview()
{
    detachParameterListeners();
}

void WaveformPreview::attachParameterListeners()
{
    for (const auto* id : { waveformParamId, pulseWidthParamId, overtonesParamId,
                            osc1PitchParamId,
                            osc2WaveformParamId, osc2PulseWidthParamId, osc2OvertonesParamId,
                            osc2PitchParamId, oscMixParamId })
        apvts.addParameterListener (id, this);
}

void WaveformPreview::detachParameterListeners()
{
    for (const auto* id : { waveformParamId, pulseWidthParamId, overtonesParamId,
                            osc1PitchParamId,
                            osc2WaveformParamId, osc2PulseWidthParamId, osc2OvertonesParamId,
                            osc2PitchParamId, oscMixParamId })
        apvts.removeParameterListener (id, this);
}

void WaveformPreview::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (parameterID, newValue);

    if (isVisible())
        repaint();
}

void WaveformPreview::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (juce::Colour (0xff1a2024));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (juce::Colour (0xff323a40));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const auto morph = apvts.getRawParameterValue (waveformParamId)->load();
    const auto pulseWidth = apvts.getRawParameterValue (pulseWidthParamId)->load();
    const auto overtones = apvts.getRawParameterValue (overtonesParamId)->load();
    const auto osc2Morph = apvts.getRawParameterValue (osc2WaveformParamId)->load();
    const auto osc2PulseWidth = apvts.getRawParameterValue (osc2PulseWidthParamId)->load();
    const auto osc2Overtones = apvts.getRawParameterValue (osc2OvertonesParamId)->load();
    const auto osc1Pitch = apvts.getRawParameterValue (osc1PitchParamId)->load();
    const auto osc2Pitch = apvts.getRawParameterValue (osc2PitchParamId)->load();
    const auto oscMix = apvts.getRawParameterValue (oscMixParamId)->load();

    const auto centreY = bounds.getCentreY();
    const auto amplitudeScale = bounds.getHeight() * 0.42f;

    g.setColour (juce::Colour (0xff4a545c).withAlpha (0.55f));
    g.drawLine (bounds.getX(), centreY, bounds.getRight(), centreY, 1.0f);

    const auto numColumns = juce::jlimit (1, maxPreviewColumns, static_cast<int> (std::ceil (bounds.getWidth())));
    const auto samplesPerColumn = (overtones > 0.001f || osc2Overtones > 0.001f)
                                ? previewSamplesPerColumnHarmonics
                                : previewSamplesPerColumn;
    const auto phaseIncrement = juce::MathConstants<double>::twoPi
                                / (static_cast<double> (numColumns)
                                   * static_cast<double> (samplesPerColumn));

    g.setColour (juce::Colour (0xffff8c1a));

    for (int column = 0; column < numColumns; ++column)
    {
        const auto x = bounds.getX() + static_cast<float> (column) + 0.5f;
        const auto tStart = static_cast<double> (column) / static_cast<double> (numColumns);
        const auto tEnd = static_cast<double> (column + 1) / static_cast<double> (numColumns);

        auto minSample = std::numeric_limits<float>::max();
        auto maxSample = std::numeric_limits<float>::lowest();

        for (int sample = 0; sample < samplesPerColumn; ++sample)
        {
            const auto t = juce::jmap (static_cast<double> (sample),
                                       0.0,
                                       static_cast<double> (samplesPerColumn - 1),
                                       tStart,
                                       tEnd);

            const auto osc1Value = samplePreviewWave (t, phaseIncrement, osc1Pitch, morph, pulseWidth, overtones);
            const auto osc2Value = samplePreviewWave (t, phaseIncrement, osc2Pitch, osc2Morph, osc2PulseWidth, osc2Overtones);
            const auto value = mixOscillatorSamples (osc1Value, osc2Value, oscMix);
            minSample = juce::jmin (minSample, value);
            maxSample = juce::jmax (maxSample, value);
        }

        const auto yTop = centreY - maxSample * amplitudeScale;
        const auto yBottom = centreY - minSample * amplitudeScale;

        if (std::abs (yTop - yBottom) < 0.5f)
            g.fillRect (x, yTop - 0.5f, 1.0f, 1.0f);
        else
            g.drawLine (x, yTop, x, yBottom, 1.0f);
    }
}
