/*
  ==============================================================================
*/

#include "WaveformSynth.h"

namespace
{
    constexpr float waveformSegment = 1.0f / 3.0f;

    double polyBlep (double t, double dt)
    {
        if (dt <= 0.0)
            return 0.0;

        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }

        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }

        return 0.0;
    }

    void wrapCyclePosition (double& cyclePos)
    {
        cyclePos = std::fmod (cyclePos, 1.0);

        if (cyclePos < 0.0)
            cyclePos += 1.0;
    }

    float waveSine (double phase)
    {
        return static_cast<float> (std::sin (phase));
    }

    float waveTriangle (double phase, double phaseIncrement)
    {
        juce::ignoreUnused (phaseIncrement);

        // Band-limited triangle via asin(sin); peaks at quarter-cycle, not inverted V.
        return static_cast<float> ((2.0 / juce::MathConstants<double>::pi)
                                   * std::asin (std::sin (phase)));
    }

    float waveSaw (double phase, double phaseIncrement)
    {
        auto cyclePos = phase / juce::MathConstants<double>::twoPi;
        wrapCyclePosition (cyclePos);

        const auto dt = phaseIncrement / juce::MathConstants<double>::twoPi;
        auto sample = static_cast<float> (2.0 * cyclePos - 1.0);

        sample -= static_cast<float> (polyBlep (cyclePos, dt));

        return sample;
    }

    float waveSquare (double phase, double phaseIncrement)
    {
        auto cyclePos = phase / juce::MathConstants<double>::twoPi;
        wrapCyclePosition (cyclePos);

        const auto dt = phaseIncrement / juce::MathConstants<double>::twoPi;
        auto sample = cyclePos < 0.5 ? 1.0f : -1.0f;

        sample += static_cast<float> (polyBlep (cyclePos, dt));

        auto fallingEdge = cyclePos + 0.5;

        if (fallingEdge >= 1.0)
            fallingEdge -= 1.0;

        sample -= static_cast<float> (polyBlep (fallingEdge, dt));

        return sample;
    }

    double warpPhaseForPulseWidth (double phase, float pulseWidth)
    {
        pulseWidth = juce::jlimit (-1.0f, 1.0f, pulseWidth);

        if (std::abs (pulseWidth) < 1.0e-5f)
            return phase;

        auto cyclePos = std::fmod (phase / juce::MathConstants<double>::twoPi, 1.0);

        if (cyclePos < 0.0)
            cyclePos += 1.0;

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

    void warpPhaseAndIncrement (double phase,
                                double phaseIncrement,
                                float pulseWidth,
                                double& warpedPhase,
                                double& warpedIncrement)
    {
        pulseWidth = juce::jlimit (-1.0f, 1.0f, pulseWidth);

        if (std::abs (pulseWidth) < 1.0e-5f || phaseIncrement <= 0.0)
        {
            warpedPhase = phase;
            warpedIncrement = phaseIncrement;
            return;
        }

        warpedPhase = warpPhaseForPulseWidth (phase, pulseWidth);

        auto nextWarpedPhase = warpPhaseForPulseWidth (phase + phaseIncrement, pulseWidth);
        warpedIncrement = nextWarpedPhase - warpedPhase;

        if (warpedIncrement > juce::MathConstants<double>::pi)
            warpedIncrement -= juce::MathConstants<double>::twoPi;
        else if (warpedIncrement < -juce::MathConstants<double>::pi)
            warpedIncrement += juce::MathConstants<double>::twoPi;
    }

    float morphWaveforms (double phase,
                          double phaseIncrement,
                          float morph,
                          float pulseWidth)
    {
        morph = juce::jlimit (0.0f, 1.0f, morph);

        double warpedPhase = phase;
        double warpedIncrement = phaseIncrement;
        warpPhaseAndIncrement (phase, phaseIncrement, pulseWidth, warpedPhase, warpedIncrement);

        const auto sine = waveSine (warpedPhase);
        const auto tri  = waveTriangle (warpedPhase, warpedIncrement);
        const auto saw  = waveSaw (warpedPhase, warpedIncrement);
        const auto sq   = waveSquare (warpedPhase, warpedIncrement);

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
                                              double phaseIncrement,
                                              double subPhase,
                                              double subPhaseIncrement,
                                              double fifthPhase,
                                              double fifthPhaseIncrement,
                                              float waveformMorph,
                                              float pulseWidth,
                                              float overtones)
{
    overtones = juce::jlimit (0.0f, 1.0f, overtones);

    const auto mainSample = morphWaveforms (phase, phaseIncrement, waveformMorph, pulseWidth);
    const auto subBlend = juce::jmin (overtones * 2.0f, 1.0f);
    const auto fifthBlend = juce::jmax (0.0f, (overtones - 0.5f) * 2.0f);

    auto mixLevel = 1.0f;
    auto mixedSample = mainSample;

    if (subBlend > 0.0f)
    {
        mixedSample += morphWaveforms (subPhase, subPhaseIncrement, waveformMorph, pulseWidth) * subBlend;
        mixLevel += subBlend;
    }

    if (fifthBlend > 0.0f)
    {
        mixedSample += morphWaveforms (fifthPhase, fifthPhaseIncrement, waveformMorph, pulseWidth) * fifthBlend;
        mixLevel += fifthBlend;
    }

    return mixedSample / mixLevel;
}
