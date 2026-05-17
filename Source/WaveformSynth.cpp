/*
  ==============================================================================
*/

#include "WaveformSynth.h"

namespace
{
    constexpr float waveformSegment = 1.0f / 3.0f;

    float waveSine (double phase)
    {
        return static_cast<float> (std::sin (phase));
    }

    float waveTriangle (double phase)
    {
        return static_cast<float> ((2.0 / juce::MathConstants<double>::pi) * std::asin (std::sin (phase)));
    }

    float waveSaw (double phase)
    {
        const auto t = phase / juce::MathConstants<double>::twoPi;
        return static_cast<float> (2.0 * (t - std::floor (t + 0.5)));
    }

    float waveSquare (double phase)
    {
        return std::copysignf (1.0f, static_cast<float> (std::sin (phase)));
    }

    double warpPhaseForPulseWidth (double phase, float pulseWidth)
    {
        pulseWidth = juce::jlimit (-1.0f, 1.0f, pulseWidth);

        if (std::abs (pulseWidth) < 1.0e-5f)
            return phase;

        auto cyclePos = std::fmod (phase / juce::MathConstants<double>::twoPi, 1.0);

        if (cyclePos < 0.0)
            cyclePos += 1.0;

        // At ±100% the whole cycle stays in one waveform half (full rail, not a thin opposite blip).
        constexpr auto halfSpan = 0.5 - 1.0e-6;

        if (pulseWidth >= 1.0f - 1.0e-5f)
        {
            const auto warpedPos = cyclePos * halfSpan;
            return warpedPos * juce::MathConstants<double>::twoPi;
        }

        if (pulseWidth <= -1.0f + 1.0e-5f)
        {
            const auto warpedPos = 0.5 + cyclePos * halfSpan;
            return warpedPos * juce::MathConstants<double>::twoPi;
        }

        const auto duty = pulseWidth > 0.0f
                              ? juce::jmap (pulseWidth, 0.0f, 1.0f, 0.5f, 1.0f)
                              : juce::jmap (pulseWidth, -1.0f, 0.0f, 0.0f, 0.5f);

        double warpedPos = 0.0;

        if (cyclePos < duty)
            warpedPos = (cyclePos / static_cast<double> (duty)) * 0.5;
        else
            warpedPos = 0.5 + ((cyclePos - duty) / static_cast<double> (1.0f - duty)) * 0.5;

        return warpedPos * juce::MathConstants<double>::twoPi;
    }

    float morphWaveforms (double phase, float morph, float pulseWidth)
    {
        morph = juce::jlimit (0.0f, 1.0f, morph);
        phase = warpPhaseForPulseWidth (phase, pulseWidth);

        const auto sine = waveSine (phase);
        const auto tri  = waveTriangle (phase);
        const auto saw  = waveSaw (phase);
        const auto sq   = waveSquare (phase);

        if (morph < waveformSegment)
        {
            const auto blend = morph / waveformSegment;
            return sine + blend * (tri - sine);
        }

        if (morph < 2.0f * waveformSegment)
        {
            const auto blend = (morph - waveformSegment) / waveformSegment;
            return tri + blend * (saw - tri);
        }

        const auto blend = (morph - 2.0f * waveformSegment) / waveformSegment;
        return saw + blend * (sq - saw);
    }
}

float WaveformSynth::computeOscillatorSample (double phase,
                                              double subPhase,
                                              double fifthPhase,
                                              float waveformMorph,
                                              float pulseWidth,
                                              float overtones)
{
    overtones = juce::jlimit (0.0f, 1.0f, overtones);

    const auto mainSample = morphWaveforms (phase, waveformMorph, pulseWidth);
    const auto subBlend = juce::jmin (overtones * 2.0f, 1.0f);
    const auto fifthBlend = juce::jmax (0.0f, (overtones - 0.5f) * 2.0f);

    auto mixLevel = 1.0f;
    auto mixedSample = mainSample;

    if (subBlend > 0.0f)
    {
        mixedSample += morphWaveforms (subPhase, waveformMorph, pulseWidth) * subBlend;
        mixLevel += subBlend;
    }

    if (fifthBlend > 0.0f)
    {
        mixedSample += morphWaveforms (fifthPhase, waveformMorph, pulseWidth) * fifthBlend;
        mixLevel += fifthBlend;
    }

    return mixedSample / mixLevel;
}
