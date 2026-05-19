/*
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

/** Rotary knob with Ctrl-held fine drag (higher mouse-drag sensitivity). */
class NafTachyonKnob : public juce::Slider
{
public:
    static constexpr int normalDragSensitivity = 250;
    static constexpr int fineDragSensitivity = 2500;

    NafTachyonKnob()
    {
        onDragStart = [this] { updateDragSensitivityFromModifiers(); };
        setMouseDragSensitivity (normalDragSensitivity);
    }

private:
    void updateDragSensitivityFromModifiers()
    {
        setMouseDragSensitivity (juce::ModifierKeys::currentModifiers.isCtrlDown()
                                     ? fineDragSensitivity
                                     : normalDragSensitivity);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NafTachyonKnob)
};
