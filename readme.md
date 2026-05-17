
V1.2
---------
Pulse width weird at ends
Aliasing at high notes
Mod wheel changes vibrato
Pitch bend should work (+/- two semitones)
Fixed cutoff dial->value mapping
Change the order of the things you can select in the EVOLVE section so it is:
Changed height of evolve area
Detune amount
Remove detune 1 voice
Evolve dropdown -> buttons

V1.1
---------
Improve resonance dial to value mapping
Improve curves in evolve section to be exponential
Log time in evolve lanes
Add release time dial

V1.0
---------
ADSR
Waveform morph
Harmonics
Evolve lanes
Velocity sensitive
Pulse width
LP filter
Wave graph
Unison


Future ideas
-----------

En "evolve ---> cutoff modulation amount" knap evt. Eller en stor knap

Der er et problem når man optager automation: Den opretter kurver for mange parametre på en gang. Den skal kun oprette én for den parameter man justerer. 

Det kan også være rart at kunne sende velocity til cutoff i stedet for bare volume.

class BetterSlider : public Slider
{
    this->onDragStart = [&]
    {
        this->setMouseDragSensitivity(ModifierKeys::currentModifiers.isCtrlDown() ? 2500 : 250);
    };
}