<div align="center">

# WetEQ

**A four-band console equaliser. Nine detents per knob. No Q control, no menus, no launcher.**

![VST3](https://img.shields.io/badge/VST3-Windows%20·%20macOS%20·%20Linux-blue)
![Licence](https://img.shields.io/badge/licence-MIT-green)
![Price](https://img.shields.io/badge/price-free-brightgreen)

<img src="docs/panel.png" width="380" alt="WetEQ">

</div>

---

## What it is

An analog console EQ, modelled as a circuit rather than as a curve.

Every stage is its own amplifier: it saturates inside its own filter loop, bleeds
into the other channel, and adds its own noise. The bells get their bandwidth
from the boost/cut pot damping a gyrator tank — which is why a gentle move is
broad and a hard one is tighter, without a Q knob anywhere on the panel.

Eleven controls. Every one of them stepped, nine positions each, because an EQ
you can nudge by 0.4 dB is an EQ you will spend an hour nudging.

## Sound

**Flat is flat** — ±0.1 dB from 20 Hz to 20 kHz with everything out of circuit.
Nothing is resampled, quantised or band-limited. Insert it and nothing happens
until you turn something.

**GAIN is drive, not makeup.** It sits ahead of the equaliser and pushes the
input amplifier: clean at the centre detent, 2.8% THD at +10, and audibly
working by +20.

**Boost and cut mirror each other**, and the filters leave circuit entirely at
the ends of their travel rather than tilting the band from a parked position.

![Specification](docs/signal-path.png)

## Controls

| | Range | Per detent |
|---|---|---|
| **GAIN** | ±20 dB | 5 dB |
| **HPF** | 20 Hz – 2 kHz | ×1.74 |
| **LPF** | 2 – 20 kHz | ×1.33 |
| **LF** shelf | 30 – 600 Hz | ×1.45 |
| **LMF** bell | 100 Hz – 4 kHz | ×1.55 |
| **HMF** bell | 400 Hz – 10 kHz | ×1.49 |
| **HF** shelf | 1.5 – 22 kHz | ×1.39 |
| **Band gain** ×4 | ±15 dB | 3.75 dB |

Frequency knobs are log-spaced with the printed endpoints exact. Mouse wheel
steps one detent per notch. Right-click the panel for **UI Zoom** — 75%, 100%
or 125%.

## Install

Grab the latest release. One download covers every platform.

| | |
|---|---|
| **Windows** | copy `WetEQ.vst3` to `C:\Program Files\Common Files\VST3\` |
| **macOS** | copy `WetEQ.vst3` to `/Library/Audio/Plug-Ins/VST3/` |
| **Linux** | copy `WetEQ.vst3` to `~/.vst3/` |

Rescan for plugins in your DAW. Intel and Apple Silicon are both in the macOS
build.

## Build it yourself

Needs CMake and a C++17 compiler. The Steinberg VST3 SDK is pulled in as a
submodule.

```
git clone --recursive https://github.com/yonie/WetEQ.git
cd WetEQ
./build.sh          # or build.bat on Windows
```

The build runs the Steinberg validator: 47 tests, all passing.

`tools/eqtest.cpp` measures the DSP with no plugin and no host — resolved knob
values against the panel legends, every band at its own centre, the
gain/bandwidth relationship, the noise floor, and that no control clicks when
you move it.

```
tools/build-eqtest.bat
```

## Licence

MIT. Free, no copy protection, no activation, no account, no launcher app.

---

<div align="center">

Part of **[WET](https://wetvst.com)** — with [WetDelay](https://github.com/yonie/WetDelay) and [WetReverb](https://github.com/yonie/WetReverb)

</div>
