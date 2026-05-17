/*
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace WaveformSynth
{
    constexpr double perfectFifthRatio = 1.5;

    float computeOscillatorSample (double phase,
                                   double phaseIncrement,
                                   double subPhase,
                                   double subPhaseIncrement,
                                   double fifthPhase,
                                   double fifthPhaseIncrement,
                                   float waveformMorph,
                                   float pulseWidth,
                                   float overtones);
}
