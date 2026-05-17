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

    float samplePreviewWave (double t,
                             double phaseIncrement,
                             float morph,
                             float pulseWidth,
                             float overtones)
    {
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
    startTimerHz (30);
}

void WaveformPreview::timerCallback()
{
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

    const auto centreY = bounds.getCentreY();
    const auto amplitudeScale = bounds.getHeight() * 0.42f;

    g.setColour (juce::Colour (0xff4a545c).withAlpha (0.55f));
    g.drawLine (bounds.getX(), centreY, bounds.getRight(), centreY, 1.0f);

    const auto numColumns = juce::jmax (1, static_cast<int> (std::ceil (bounds.getWidth())));
    const auto samplesPerColumn = overtones > 0.001f ? 48 : 16;
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

            const auto value = samplePreviewWave (t, phaseIncrement, morph, pulseWidth, overtones);
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
