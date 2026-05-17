/*
  ==============================================================================
*/

#include "NafTachyonLookAndFeel.h"

namespace
{
    const auto knobFaceColour      = juce::Colour (0xff3d454b);
    const auto knobHighlightColour = juce::Colour (0xff4f585f);
    const auto knobShadowColour    = juce::Colour (0xff2a3136);
    const auto tickColour          = juce::Colour (0xff6d767e);
    const auto indicatorColour     = juce::Colour (0xffff8c1a);
    const auto valueTextColour     = juce::Colour (0xffb8c0c8);

    const int numMajorTicks = 11;
    const int minorTicksPerSegment = 4;
}

//==============================================================================
NafTachyonLookAndFeel::NafTachyonLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, getPluginBackgroundColour());
    setColour (juce::Label::textColourId, valueTextColour);
}

juce::Colour NafTachyonLookAndFeel::getPluginBackgroundColour()
{
    return juce::Colour (0xff2c3539);
}

void NafTachyonLookAndFeel::drawTick (juce::Graphics& g,
                                       juce::Point<float> centre,
                                       float angle,
                                       float innerRadius,
                                       float outerRadius,
                                       float lineWidth,
                                       juce::Colour colour)
{
    const auto sinA = std::sin (angle - juce::MathConstants<float>::halfPi);
    const auto cosA = std::cos (angle - juce::MathConstants<float>::halfPi);

    const juce::Line<float> tick (
        { centre.x + innerRadius * cosA, centre.y + innerRadius * sinA },
        { centre.x + outerRadius * cosA, centre.y + outerRadius * sinA });

    g.setColour (colour);
    g.drawLine (tick, lineWidth);
}

void NafTachyonLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                               int x,
                                               int y,
                                               int width,
                                               int height,
                                               float sliderPos,
                                               const float rotaryStartAngle,
                                               const float rotaryEndAngle,
                                               juce::Slider& slider)
{
    juce::ignoreUnused (slider);

    auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                          static_cast<float> (y),
                                          static_cast<float> (width),
                                          static_cast<float> (height));

    bounds = bounds.reduced (4.0f, 2.0f);
    bounds.removeFromBottom (20.0f);

    const auto centre = bounds.getCentre();
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.46f;

    const auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto angleRange = rotaryEndAngle - rotaryStartAngle;

    const auto outerTickOuter = radius * 1.02f;
    const auto outerTickInner = radius * 0.88f;
    const auto innerTickOuter = radius * 0.86f;
    const auto innerTickInner = radius * 0.80f;
    const auto knobRadius = radius * 0.70f;

    const auto totalMinorTicks = (numMajorTicks - 1) * minorTicksPerSegment;

    for (int i = 0; i < numMajorTicks; ++i)
    {
        const auto proportion = static_cast<float> (i) / static_cast<float> (numMajorTicks - 1);
        const auto angle = rotaryStartAngle + proportion * angleRange;
        drawTick (g, centre, angle, outerTickInner, outerTickOuter, 1.6f, tickColour);
    }

    for (int i = 0; i <= totalMinorTicks; ++i)
    {
        const auto proportion = static_cast<float> (i) / static_cast<float> (totalMinorTicks);
        const auto angle = rotaryStartAngle + proportion * angleRange;

        if (i % minorTicksPerSegment == 0)
            continue;

        drawTick (g, centre, angle, innerTickInner, innerTickOuter, 1.0f, tickColour.withAlpha (0.75f));
    }

    const auto knobBounds = juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre);

    juce::ColourGradient faceGradient (
        knobHighlightColour,
        knobBounds.getTopLeft(),
        knobShadowColour,
        knobBounds.getBottomRight(),
        false);
    g.setGradientFill (faceGradient);
    g.fillEllipse (knobBounds);

    g.setColour (knobFaceColour.withAlpha (0.35f));
    g.fillEllipse (knobBounds.reduced (knobRadius * 0.08f));

    g.setColour (knobHighlightColour.withAlpha (0.25f));
    g.drawEllipse (knobBounds.reduced (0.5f), 1.0f);

    const auto indicatorInner = knobRadius * 0.52f;
    const auto indicatorOuter = knobRadius * 0.88f;
    const auto indicatorHalfWidth = juce::jmax (2.0f, knobRadius * 0.07f);

    // Use the same polar mapping as the tick marks (not a rotated local rectangle).
    const auto pointerCos = std::cos (toAngle - juce::MathConstants<float>::halfPi);
    const auto pointerSin = std::sin (toAngle - juce::MathConstants<float>::halfPi);

    const auto innerPoint = centre + juce::Point<float> (indicatorInner * pointerCos, indicatorInner * pointerSin);
    const auto outerPoint = centre + juce::Point<float> (indicatorOuter * pointerCos, indicatorOuter * pointerSin);
    const auto perpendicular = juce::Point<float> (-pointerSin, pointerCos) * indicatorHalfWidth;

    juce::Path indicator;
    indicator.startNewSubPath (innerPoint + perpendicular);
    indicator.lineTo (outerPoint + perpendicular);
    indicator.lineTo (outerPoint - perpendicular);
    indicator.lineTo (innerPoint - perpendicular);
    indicator.closeSubPath();

    g.setColour (indicatorColour);
    g.fillPath (indicator);
}

juce::Label* NafTachyonLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);

    label->setColour (juce::Label::textColourId, valueTextColour);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setJustificationType (juce::Justification::centred);

    return label;
}
