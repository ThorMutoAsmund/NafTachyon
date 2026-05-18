# V1.3.0
- Evolve lanes (when on) modulate knob baseline
- Increase available points on evolve lanes to 12
- Evolve lane loop option (locks level of last point)


# V1.2.0
- Fixed bug: Pulse width weird at ends
- Fixed bug: Aliasing at high notes
- Fixed bug: cutoff dial->value mapping
- Mod wheel changes vibrato
- Pitch bend should work now (+/- two semitones)
- Change the order of the things you can select in the EVOLVE section so it is:
- Changed height of evolve area
- Detune amount
- Remove detune 1 voice
- Evolve dropdown -> buttons

# V1.1.0
- Improve resonance dial to value mapping
- Improve curves in evolve section to be exponential
- Log time in evolve lanes
- Add release time dial

# V1.0.0
- ADSR
- Waveform morph
- Harmonics
- Evolve lanes
- Velocity sensitive
- Pulse width
- LP filter
- Wave graph
- Unison


# Future ideas
- Problem when recording automation: (Den opretter kurver for mange parametre på en gang. Den skal kun oprette een for den parameter man justerer. )
- Velocity to cutoff

```
class BetterSlider : public Slider
{
    this->onDragStart = [&]
    {
        this->setMouseDragSensitivity(ModifierKeys::currentModifiers.isCtrlDown() ? 2500 : 250);
    };
}
```
