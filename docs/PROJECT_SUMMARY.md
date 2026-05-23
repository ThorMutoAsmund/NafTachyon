# Nafai's Tachyon — Project Summary

Handoff document for continuing development. Covers the full synth through **V1.3.0 / V1.4.0** work: dual oscillators, EVOLVE modulation, UI layout, and performance fixes.

---

## What it is

**Nafai's Tachyon** is a JUCE **MIDI polyphonic synthesizer** plugin (VST3, standalone, etc.) at `c:\Repos\_Music\JUCE\NafTachyon`. Display name in hosts (e.g. FL Studio): **"Nafai's Tachyon"** (`targetName` in `NafTachyon.jucer`).

| | |
|---|---|
| **Company** | TMA Research |
| **Author** | Thor Muto Asmund (2026) |
| **Version** | 1.3.0 in Projucer (readme also lists **V1.4.0** bullets for latest DSP/UI) |
| **Build** | Visual Studio 2022 — primary target **`Nafai's Tachyon_SharedCode.vcxproj`** (+ VST3 / Standalone) |
| **Editor size** | **700×~1020–1190 px** (height from `minimumEditorHeightForDialSize`) |

---

## Source layout

| File | Role |
|------|------|
| `Source/PluginProcessor.cpp/h` | Voices, dual OSC, filter, limiter, APVTS, `processBlock`, EVOLVE modulation |
| `Source/PluginEditor.cpp/h` | UI layout, knob attachments, section panels |
| `Source/WaveformSynth.cpp/h` | Shared oscillator math (audio + scope) |
| `Source/WaveformPreview.cpp/h` | Waveform preview (WAVE section); mixed OSC 1 + OSC 2 |
| `Source/ModEnvelopeEditor.cpp/h` | EVOLVE lane editor |
| `Source/NafTachyonLookAndFeel.cpp/h` | Custom rotary knobs (dark face, orange indicator, tick rings) |
| `Source/NafTachyonKnob.h` | Rotary slider with Ctrl = fine drag |
| `Source/SectionPanel.cpp/h` | Grouped panels with title badge |
| `NafTachyon.jucer` | Projucer project — re-export updates VS projects |
| `readme.md` | Release notes by version |

**After Projucer re-export:** confirm **`Builds/VisualStudio2022/Nafai's Tachyon_SharedCode.vcxproj`** still lists all `Source/*.cpp` files and re-check version defines in generated headers.

---

## UI sections

| Section | Contents |
|---------|----------|
| **MAIN** | Gain (dB), Mix, Release Time, Amp velocity; **Sync** toggle below Mix |
| **WAVE** | **OSC 1** / **OSC 2** rows: Shape, Width, Harmonics, Pitch, Finetune + waveform preview |
| **FILTER** | Cutoff, Resonance, Cutoff velocity, **Slope** combo, **Limiter** combo |
| **DETUNE** | Unison voices, spread amount |
| **EVOLVE** | Multi-lane modulation envelopes (baseline modulation of parameters) |

### Layout rules (`PluginEditor.cpp`)

- **Uniform dial size** across MAIN, WAVE, FILTER, DETUNE (~94–130 px; from column width and row height)
- Per-control column: label row (18 px) → gap (2 px) → knob + value box (`dialSize` + 16 px text box)
- `layoutKnobInColumn()` — explicit slider bounds, top-aligned; no vertical centering in oversized columns
- **MAIN** panel height = `mainPanelHeightForDialSize` (knob stack + Sync row + chrome)
- **WAVE** placed directly under MAIN (`main bottom + sectionGap`), not at the bottom of a tall empty region
- **FILTER** Slope/Limiter combos live in the **4th column of the knob row** (not below it — that strip had zero height and hid them)
- **LookAndFeel:** removed extra 20 px bottom strip in `drawRotarySlider` so value boxes sit closer to knobs

### Key layout constants

- `minDialSize` / `maxDialSize` = 94 / 130
- `labelRowHeight` = 18, `sliderTextBoxHeight` = 16, `labelToKnobGap` = 2
- `sectionChromeHeight` = 46 (matches `SectionPanel::getContentBounds`)
- `waveformPanelHeight` = `sectionChromeHeight + 2 × knobStackHeightForDialSize(dialSize)`
- `minWaveformPreviewWidth` = 108

---

## Features implemented

### Synthesis core

- **16-voice** polyphony, MIDI note on/off, velocity → level
- **Dual oscillators (OSC 1 / OSC 2):**
  - Per-OSC: Shape, Width, Harmonics, Pitch (±48 semitones), Finetune (±50 cents)
  - **Mix** (`oscMix`): crossfade (0 = OSC 1 only, 1 = OSC 2 only)
  - **Sync** (`oscSync`): hard sync — OSC 2 phase follows OSC 1 (audible with Mix > 0 and detuned OSC 2)
  - Per-voice OSC 2 phase; unison mirrors sync
- **Oscillator shapes:** morph **Sine → Triangle → Saw → Square** via `waveform` / `osc2Waveform` (0–1)
- **Pulse width** (−1…1): phase warp (duty ~6%–94%) before waveform lookup
- **Overtones** (0–1): sub at ½×, fifth at 1.5×, normalized mix
- **Release** envelope time on MAIN; EVOLVE lanes can modulate parameter baselines
- **Output:** **Gain** on MAIN (`amplitude`, dB); per-voice gain scaling in DSP

### Filter (per voice)

- **Cutoff** 20 Hz–20 kHz (log-skewed), **Resonance** 0–1
- **Slope:** 6 / 12 / 24 dB
- **24 dB resonance fix:** peak-gain compensation so 24 dB is not ~2× louder than 6/12 dB at the same resonance
- **Limiter** after filter: Off / Light / Normal / Tight
- **Cutoff velocity** sensitivity
- Smoothed coefficients (cutoff ~30 ms multiplicative; resonance etc. ~30 ms linear)

### EVOLVE

- Multiple lanes modulating knob baselines
- Up to 12 points per lane; optional loop (locks level of last point)
- Toolbar: s / Bar toggle; 0.5 s / bar vertical lines; Ctrl snaps points to lines
- Timer only while dragging points (no idle 15 Hz repaint)

### Detune (unison)

- Voice count and spread amount

### Waveform preview

- One period, column min/max rendering (avoids false spikes on discontinuous waves)
- Shows **mixed** OSC 1 + OSC 2 via `mixOscillatorSamples`
- Repaints on APVTS listeners (no 30 Hz idle timer)

### CPU / performance

- `processBlock` early exit when no active voices
- Skip heavy EVOLVE APVTS update when no lanes in use
- Shared smoothers and unison increment once per block (not per voice per sample)

---

## Architecture (audio path)

```
MIDI → startVoice / releaseVoice
         ↓
Per block (when voices active):
  Update shared smoothers, unison, EVOLVE baselines
         ↓
Per sample, per active voice:
  OSC 1 sample (shape, width, harmonics, pitch, finetune, bend, vibrato)
  OSC 2 sample (same + optional hard sync to OSC 1 phase)
    → mixOscillatorSamples(osc1, osc2, oscMix)
    → × velocity × envelope × gain
    → per-voice filter (+ limiter)
         ↓
Sum all voices → output buffer (mono sum to all channels)
```

**WaveformSynth** is the single source of truth for oscillator math. The preview uses the same helpers with mixed phases/rates when overtones are active.

### Pulse width

- **0** → symmetric; positive/negative warps duty before shape lookup

### Overtones

| Overtones | Sub (½×) | Fifth (1.5×) |
|-----------|----------|----------------|
| 0.0 | off | off |
| 0.25 | 50% | off |
| 0.5 | 100% | off |
| 0.75 | 100% | 50% |
| 1.0 | 100% | 100% |

---

## Parameters (APVTS) — selected IDs

| ID | Notes |
|----|--------|
| `amplitude` | Output gain (dB), MAIN |
| `releaseTime` | Amp release, MAIN |
| `amplitudeVelSensitivity` | Velocity → level |
| `waveform`, `pulseWidth`, `overtones` | OSC 1 |
| `pitchTune` | OSC 1 finetune (cents) |
| `osc1Pitch`, `osc2Pitch` | Semitones per OSC |
| `osc2Waveform`, `osc2PulseWidth`, `osc2Overtones`, `osc2PitchTune` | OSC 2 |
| `oscMix`, `oscSync` | Mix and hard sync |
| `filterCutoff`, `filterResonance`, `filterSlope` | Filter |
| `filterLimiter` | Post-filter limiter mode |
| `cutoffVelSensitivity` | Velocity → cutoff |
| `unison`, `unisonSpread` | Detune section |
| EVOLVE lane params | Per-lane envelopes (see processor) |

Plugin state is stored via APVTS (`PARAMETERS` ValueTree).

---

## Issues solved during development

| Issue | Fix |
|--------|-----|
| Keyboard notes stop after using knobs | `setWantsKeyboardFocus(false)`; `mouseUp` focus restore |
| Orange knob indicator on wrong part of arc | Polar angle mapping aligned with tick marks |
| FL Studio shows "NafTachyon" | `targetName` = `Nafai's Tachyon` in `.jucer` |
| Unresolved linker symbols after new `.cpp` files | Add sources to SharedCode vcxproj |
| Filter zipper when moving cutoff/resonance | Per-sample smoothed coefficients |
| Dials different sizes across sections | Shared `uniformDialSize` + `layoutKnobInColumn` |
| Scope spike with overtones on square | Column min/max rendering |
| 24 dB resonance much louder than 6/12 dB | Peak-gain compensation in 24 dB path |
| ~33% CPU idle from UI timers | Removed idle timers on scope + EVOLVE editor |
| MAIN knobs tiny; huge gap MAIN↔WAVE | Window min height; stack WAVE under MAIN; cap MAIN panel height |
| Slope / Limiter not visible | Combos in 4th knob column, not below zero-height strip |
| Label/knob/value box spacing inconsistent | Unified `splitLabelAndKnobRows`; L&F draw bounds fix |

---

## Known limitations

1. **Naive oscillators** — no band-limiting; aliasing on saw/square at high pitch and extreme pulse width.
2. **Scope vs audio** — preview is indicative when overtones/sync/detune differ from a single stable period.
3. **Shape morph** may click under very fast automation (not all targets smoothed).
4. **Plugin window** is taller than early builds (~600–900 px) to fit dual OSC rows and consistent knob stacks.
5. **readme Future ideas** may still list items now implemented (fine tune, second oscillator, 24 dB loudness) — tidy when cutting a release.
6. **Windows build** may warn about line endings (`C4335`) — non-fatal.

---

## Build and test

1. Open `Builds/VisualStudio2022/Nafai's Tachyon.sln` (or build **SharedCode** + VST3 / Standalone).
2. Configuration: **Debug or Release \| x64**.
3. Load in a host; scan for **Nafai's Tachyon**.

**Quick test checklist**

- [ ] Polyphonic MIDI; release tail
- [ ] OSC 1 / OSC 2 shape, width, harmonics, pitch, finetune
- [ ] Mix and Sync (detune OSC 2, raise Mix)
- [ ] Gain in dB; amp velocity
- [ ] Filter 6 / 12 / 24 dB; resonance balance across slopes
- [ ] Slope and Limiter combos visible and working
- [ ] Unison voices / amount
- [ ] EVOLVE lanes modulate targets; no idle CPU spike from UI
- [ ] Computer keyboard works after tweaking knobs
- [ ] Preset save and reload

---

## Suggested next steps

### Sound quality

- PolyBLEP / BLAMP or wavetables for saw/square
- Smooth remaining morph parameters
- Better unison/detune (e.g. Vital-style)

### Synth features

- Portamento / legato, mono mode
- LFO routing
- Optional per-OSC level before Mix

### UI

- Resizable editor; tooltips
- Scope label; pause work when editor hidden

### Engineering

- Shared header for APVTS parameter ID strings
- Unit tests for `WaveformSynth` and mix/sync helpers
- Post–Projucer-export vcxproj checklist

---

## Related paths

- Release notes: `readme.md`
- Projucer: `NafTachyon.jucer`
- VS solution: `Builds/VisualStudio2022/`
- This doc: `docs/PROJECT_SUMMARY.md`
