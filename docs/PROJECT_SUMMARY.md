# Nafai's Tachyon — Project Summary

Handoff document for continuing development. Last updated from implementation work through waveform scope, pulse width, and overtones.

---

## What it is

**Nafai's Tachyon** is a JUCE **MIDI polyphonic synthesizer** plugin (VST3, etc.) at `c:\Repos\_Music\JUCE\NafTachyon`. Display name in hosts (e.g. FL Studio): **"Nafai's Tachyon"** (`targetName` in `NafTachyon.jucer`).

| | |
|---|---|
| **Company** | TMA Research |
| **Author** | Thor Muto Asmund (2026) |
| **Build** | Visual Studio 2022 — primary target **`Nafai's Tachyon_SharedCode.vcxproj`** (+ VST3 wrapper) |
| **Editor size** | 580×600 px |

---

## Source layout

| File | Role |
|------|------|
| `Source/PluginProcessor.cpp/h` | Voices, ADSR, MIDI, filter, APVTS, `processBlock` |
| `Source/PluginEditor.cpp/h` | UI layout, knob attachments, section panels |
| `Source/WaveformSynth.cpp/h` | Shared oscillator math (audio + scope) |
| `Source/WaveformPreview.cpp/h` | Static scope display (WAVEFORM section) |
| `Source/NafTachyonLookAndFeel.cpp/h` | Custom rotary knobs (dark face, orange arc, tick rings) |
| `Source/SectionPanel.cpp/h` | Grouped panels with title badge (ADSR / WAVEFORM / FILTER) |
| `NafTachyon.jucer` | Projucer project — re-export updates VS projects |

**After Projucer re-export:** confirm **`Builds/VisualStudio2022/Nafai's Tachyon_SharedCode.vcxproj`** still lists all `Source/*.cpp` files. LookAndFeel, SectionPanel, WaveformSynth, and WaveformPreview were added manually once when a link error occurred.

---

## Features implemented

### Synthesis core

- **16-voice** polyphony, MIDI note on/off, velocity → level
- **Oscillator:** phase accumulator per voice; morph **Sine → Triangle → Saw → Square** via `waveform` (0–1, three equal segments)
- **Pulse width** (`pulseWidth`, −1…1, default 0): asymmetric **phase warp** (duty 6%–94%) before waveform lookup — applies to all shapes; 0 = symmetric
- **Overtones** (`overtones`, 0–1):
  - **0–0.5:** blend in **sub-oscillator** at **½×** (one octave down)
  - **0.5–1.0:** full sub + blend **fifth** at **1.5×** (perfect fifth up)
  - Output normalized by `1 + subBlend + fifthBlend`
- **ADSR** per voice: attack, decay, sustain, release (saved in APVTS)
- **Output gain:** `voiceGain = 0.2f` per voice, summed mono to all output channels

### Filter (per voice)

- **Cutoff** 20 Hz–20 kHz (log-skewed range)
- **Resonance** 0–1
- **Slope:** 6 dB (one-pole), 12 dB (one biquad), 24 dB (two biquads in series)
- **Smoothing:** cutoff ~30 ms multiplicative; resonance, overtones, pulse width ~30 ms linear — filter coefficients recalculated **per sample** to avoid zipper noise

### UI

- Three sections: **ADSR** (4 knobs), **WAVEFORM** (Shape, Width, Overtones + scope), **FILTER** (cutoff, resonance, slope combo)
- **Uniform dial size** (max 120 px), derived from WAVEFORM dial column area
- **Waveform scope** to the right of the knobs: one fundamental period; **min/max per pixel column** (oscilloscope-style) so discontinuities are not drawn as false spikes
- **Keyboard focus:** controls do not steal host/standalone computer keyboard; `mouseUp` restores focus to the top-level window
- Custom **NafTachyonLookAndFeel** rotary sliders

### Parameters (APVTS)

| ID | Range | Default | UI label |
|----|--------|---------|----------|
| `attack` | 0.001–2 s | 0.01 | Attack |
| `decay` | 0.001–2 s | 0.2 | Decay |
| `sustain` | 0–1 | 0.7 | Sustain |
| `release` | 0.001–3 s | 0.3 | Release |
| `waveform` | 0–1 | 0 | Shape |
| `pulseWidth` | −1–1 | 0 | Width |
| `overtones` | 0–1 | 0 | Overtones |
| `filterCutoff` | 20–20000 Hz | 20000 | Cutoff |
| `filterResonance` | 0–1 | 0 | Resonance |
| `filterSlope` | 0 / 1 / 2 | 0 | Slope (6 / 12 / 24 dB) |

Plugin state is stored via APVTS (`PARAMETERS` ValueTree).

---

## Architecture (audio path)

```
MIDI → startVoice / releaseVoice
         ↓
Per sample, per active voice:
  ADSR envelope
    → WaveformSynth::computeOscillatorSample (shape + width + overtones)
    → × velocity × envelope × 0.2
    → per-voice filter (state in OscillatorVoice)
         ↓
Sum all voices → output buffer (mono sum to all channels)
```

**WaveformSynth** is the single source of truth for oscillator math. The scope uses the same `computeOscillatorSample` with `phase`, `phase × 0.5`, and `phase × 1.5`.

### Pulse width (how it works)

Before reading the waveform, cycle position is warped by duty cycle:

- **0** → 50% duty (no warp)
- **Positive** → more time in the first half of the wave (e.g. wider high on square)
- **Negative** → more time in the second half

Shape morphing still happens on the warped phase.

### Overtones (how it works)

| Overtones | Sub (½×) | Fifth (1.5×) |
|-----------|----------|----------------|
| 0.0 | off | off |
| 0.25 | 50% | off |
| 0.5 | 100% | off |
| 0.75 | 100% | 50% |
| 1.0 | 100% | 100% |

---

## Issues solved during development

| Issue | Fix |
|--------|-----|
| Keyboard notes stop after using knobs | `setWantsKeyboardFocus(false)` on controls; `mouseUp` focus restore |
| Orange knob indicator on wrong part of arc | Polar angle mapping aligned with tick marks |
| FL Studio shows "NafTachyon" | `targetName` = `Nafai's Tachyon` in `.jucer` |
| Unresolved `NafTachyonLookAndFeel` linker symbol | Added `.cpp` files to SharedCode vcxproj |
| Filter zipper when moving cutoff/resonance | Per-sample smoothed coefficients |
| Dials different sizes across sections | Shared `uniformDialSize`; taller ADSR/FILTER panels |
| Scope spike with overtones on square | Column min/max rendering instead of connected polyline |

---

## Known limitations

1. **Naive oscillators** — no band-limiting; aliasing on saw/square at high pitch and extreme pulse width.
2. **Scope vs audio** — scope plots one **main** cycle while sub/fifth run at different rates; with overtones > 0 the trace is indicative, not a single stable repeating shape.
3. **`waveSquare`** uses `copysign(1, sin(phase))` — at exact zero crossings the value is +1.
4. **Shape (`waveform`)** is read once per buffer, not smoothed — fast automation may click.
5. **Retrigger** on same note resets phases but does not refresh increments if sample rate changed (edge case).
6. **Windows build** may warn about Mac-style line endings in some sources (`C4335`) — non-fatal.

---

## Build and test

1. Open `Builds/VisualStudio2022/Nafai's Tachyon.sln` (or build SharedCode + VST3).
2. Configuration: **Release \| x64**.
3. Load the VST3 in a host; scan for **Nafai's Tachyon**.

**Quick test checklist**

- [ ] Polyphonic MIDI
- [ ] ADSR tail on release
- [ ] Shape morph Sine → Square
- [ ] Width on square (narrow/wide pulses)
- [ ] Overtones: sub only, then sub + fifth
- [ ] Filter slopes 6 / 12 / 24 dB, smooth cutoff drag
- [ ] Computer keyboard still works after tweaking knobs
- [ ] Preset save and reload

---

## Suggested next steps

### Sound quality (high impact)

- PolyBLEP / BLAMP or wavetables for saw/square
- Smooth the `waveform` morph parameter
- Optional detune on overtone oscillators

### Synth features

- Master volume (+ optional limiter)
- LFO → pulse width, morph, or cutoff
- Portamento / legato, mono mode
- Separate sub/fifth level controls

### UI

- Scope label; tooltips for Width / Overtones
- Resizable editor
- Pause scope timer when editor is hidden

### Engineering

- Shared header for APVTS parameter ID strings
- Small tests for `WaveformSynth` (PWM at 0, overtones mix normalization)
- Post–Projucer-export checklist for vcxproj `ClCompile` entries

---

## Key constants (editor)

Defined in `PluginEditor.cpp` anonymous namespace:

- `maxDialSize = 120`
- `waveformPanelHeight = 160`
- `minWaveformPreviewWidth = 108`
- `sectionChromeHeight = 46` (matches `SectionPanel::getContentBounds` padding)

---

## Related paths

- Projucer: `NafTachyon.jucer`
- VS solution: `Builds/VisualStudio2022/`
- This doc: `docs/PROJECT_SUMMARY.md`
