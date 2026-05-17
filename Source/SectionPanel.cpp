/*
  ==============================================================================
*/

#include "SectionPanel.h"

namespace
{
    const auto panelFillColour     = juce::Colour (0xff252c30);
    const auto panelBorderColour   = juce::Colour (0xff1a1f23);
    const auto titleBadgeFill      = juce::Colour (0xff1a2024);
    const auto titleBadgeBorder    = juce::Colour (0xff323a40);
    const auto titleTextColour     = juce::Colour (0xffe8ecef);
}

//==============================================================================
SectionPanel::SectionPanel (const juce::String& title)
    : sectionTitle (title)
{
}

juce::Rectangle<int> SectionPanel::getContentBounds() const
{
    auto bounds = getLocalBounds().reduced (14, 12);
    bounds.removeFromTop (22);
    return bounds;
}

void SectionPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (panelFillColour);
    g.fillRoundedRectangle (bounds, 5.0f);

    g.setColour (panelBorderColour);
    g.drawRoundedRectangle (bounds, 5.0f, 1.5f);

    g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    const auto badgeHeight = 20.0f;
    const auto badge = juce::Rectangle<float> (10.0f, 2.0f, 56.0f, badgeHeight);

    g.setColour (titleBadgeFill);
    g.fillRoundedRectangle (badge, 3.0f);

    g.setColour (titleBadgeBorder);
    g.drawRoundedRectangle (badge, 3.0f, 1.0f);

    g.setColour (titleTextColour);
    g.drawText (sectionTitle.toUpperCase(), badge.toNearestInt(), juce::Justification::centred);
}
