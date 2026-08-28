# WetEQ — how it works

A technical description of the signal path, the controls and the design
decisions behind them. Every number here is measured off the built plugin, not
read off the source.

Companion diagram: `docs/signal-path.png`.

---

## 1. The chain, in order

```
in ─→ GAIN ─→ anti-alias ─→ ↓24 kHz ─→ crosstalk ─→ HPF ─→ LPF ─→ LF ─→ LMF ─→ HMF ─→ HF ─→ ↑host ─→ reconstruct ─→ out
      drive     10 kHz LP                −40 dB     shelf/bell chain, 24 kHz internal        12-bit          10 kHz LP
```

Three things about that order matter.

**GAIN is first, and it is drive.** It sits *before* the rate and word-length
reduction, so it decides where the signal sits against the quantisation floor.
It is not makeup gain and it is not undone at the output. Pushing it does not
just make the plugin louder, it changes how hard the signal hits the lo-fi
stage.

**The EQ runs at 24 kHz internally**, so the whole band-shaping happens inside
the house core rather than around it. Nyquist is 12 kHz.

**There is no fixed high-pass or low-pass in the chain**, unlike WetDelay, which
applies 80 Hz and 9 kHz unconditionally. Here that job belongs to the HPF and
LPF knobs, and a fixed one would fight them and the LF band.

---

## 2. Controls

Eleven knobs, **nine detents each**. Nine because the panel art paints nine tick
dots per knob and the pointer has to land on one; odd, so the centre detent is a
real position and 0 dB is reachable on every gain control.

Coarse is the point. A stepped control forces a decision instead of inviting a
0.5 dB fiddle — the same argument as WetDelay's six fixed delay times.

### Gain controls

| control | range | per detent |
|---|---|---|
| GAIN | ±20 dB | 5 dB |
| LF / LMF / HMF / HF | ±15 dB | 3.75 dB |

3.75 dB is not a round number. It is what ±15 dB over nine positions gives, and
the pointer landing on a painted dot was judged to matter more than the number
reading nicely.

### Frequency controls

Log-spaced between the two values printed on the panel. **The endpoints are
exact; the number printed at each knob's twelve o'clock is decoration and does
not match the centre detent** — LF centres on 134 Hz, not the painted 150.

| control | range | ×/detent |
|---|---|---|
| HPF | 20 Hz – 2 kHz | 1.74 |
| LPF | 2 kHz – 20 kHz | 1.33 |
| LF | 30 – 600 Hz | 1.45 |
| LMF | 100 Hz – 4 kHz | 1.55 |
| HMF | 400 Hz – 10 kHz | 1.49 |
| HF | 1.5 – 22 kHz | 1.39 |

**HF's top does nothing.** Its nominal ceiling of 22 kHz is above the 12 kHz
Nyquist of the 24 kHz core; above roughly 10.8 kHz the shelf clamps. That is a
known and accepted cost of keeping the house core — see §5.

### Filters out of circuit

HPF at its lowest detent and LPF at its highest are *bypassed*, not merely set
wide. Without that they would apply a gentle tilt across the whole band at all
times.

---

## 3. Band shapes

**LF and HF are shelves** (Baxandall-style, slope S = 1.0 — no resonant bump).
**LMF and HMF are bells.** There is no Q control.

Bell Q is **proportional to gain**:

```
Q = 0.32 + 0.15 × |gain| / 15        →  0.32 flat, 0.47 at ±15 dB
```

which is about **3.3 octaves** at a gentle move and **2.7 octaves** at full.

This is deliberately much broader than the textbook console range of 0.70–2.00.
That range measured 1.4 and 0.7 octaves here and was audibly resonant at full
boost while being too surgical at small moves to draw a smooth curve with. An EQ
with nine fixed detents and no Q control is for shaping a sound, not correcting
one, and a bell that tightens hard under boost fights that — it starts ringing
exactly when you ask it for the most.

The narrowing with gain is real but gentle. A harder push is still the narrower
bell, across a narrow spread of very wide curves.

---

## 4. Changing a setting

Every control is stepped, so every change is a jump, and a jump is a click.
Two of them, both smoothed:

- **The master drive** is a scalar on the signal and one detent is 5 dB — a
  factor of 1.78 applied as a step. Smoothed **per sample** with a 12 ms
  one-pole. Per block would still tick at small buffer sizes.
- **The biquad coefficients** were swapped outright while the filter state still
  held the old response. The engine resolves each setting to continuous values
  and glides those over **22 ms**, rebuilding coefficients from the glide.
  Frequencies glide in the **log domain**, so 100→160 Hz takes the same time as
  1→1.6 kHz.

The controls stay stepped. What moves smoothly is the value behind the detent.

Measured as the largest sample-to-sample step at the change against the largest
step the test tone itself makes — 1.0 means no worse than the signal:

| | before | after |
|---|---|---|
| GAIN, one detent | 1.89× | 0.65× |
| HF gain, two detents | 1.65× | 1.01× |

`prepare()` snaps rather than glides: the glide exists to hide a knob move, not
to fade the plugin in when the host starts.

---

## 5. The house core, and what it costs

Shared with WetDelay and WetReverb:

- **24 kHz internal rate** — Nyquist 12 kHz
- **12-bit companded quantisation** with TPDF dither
- **−40 dB channel crosstalk**
- **Linear-interpolation resampling**, whose HF loss is part of the character
- 10 kHz one-pole anti-alias before, 10 kHz reconstruction after

Flat, with every control out of circuit, the plugin measures:

| | |
|---|---|
| 1 kHz | −0.1 dB |
| 2 kHz | −0.8 dB |
| 4 kHz | −3.0 dB |
| 8 kHz | −10.7 dB |
| 11.5 kHz | −28.9 dB |
| 16 kHz | −46.7 dB |

**Inserting WetEQ dulls a source.** That is not a bug and it is not tuneable
away without abandoning the core.

It is also a genuine open question. The core is authentic on WetDelay and
WetReverb, which model *digital* units — a 24 kHz rate and 12-bit words are how
those boxes really worked. An analog console EQ has no sample rate and no word
length, so the same core puts a digital fingerprint on a device that never had
one. Two decisions were taken on the same day and the second overrode the first:
run full-rate because the core damages an insert path, then keep the core and
accept the cost because the constraints are the product.

Work on a circuit-level analog core — gyrator tanks, op-amp saturation,
component tolerance, oversampled — is prospective and **not in this build**.

---

## 6. Verification

Three layers, none of which need a DAW:

| | what it proves |
|---|---|
| `tools/eqtest.cpp` | the DSP maths, with no plugin and no host |
| `vstrunner --sweep` | the built plugin's real response through a VST3 host |
| Steinberg validator | 47/47 |

`eqtest` checks the resolved knob values against the panel legends, flatness,
every band at its own centre, the proportional-Q relationship, the 12-bit noise
floor, stability with HF parked above Nyquist, and that no control clicks.
