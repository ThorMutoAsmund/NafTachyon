/*
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace WaveformSynth
{
    constexpr double perfectFifthRatio = 1.5;

    float computeOscillatorSample (double phase,
                                   double subPhase,
                                   double fifthPhase,
                                   float waveformMorph,
                                   float pulseWidth,
                                   float overtones);
}
