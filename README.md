# WET EQ VST3 Plugin

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![VST3](https://img.shields.io/badge/VST3-Compatible-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)
![Version](https://img.shields.io/badge/version-1.0.0-orange)

A four-band British console equaliser VST3 plugin, modelled as a circuit rather
than as a frequency response, with the saturation, crosstalk and gain-dependent
bandwidth of a large-format desk.

![WetEQ Plugin Screenshot](docs/panel.png)

## Features

- **Four Bands**: LF shelf, LMF bell, HMF bell and HF shelf, each +/-15 dB
- **Two Filters**: High-pass and low-pass, both leaving circuit at the ends of their travel
- **Input Drive**: GAIN pushes the input amplifier ahead of the equaliser
- **Stepped Controls**: All eleven knobs, nine positions each
- **Visual Metering**: Real-time peak level meters for input and output
- **VST3 Automation**: Full parameter automation support in DAWs

### Console Character

- **Circuit, Not Curve**: Every band is its own amplifier, saturating inside its own filter loop
- **Gain-Dependent Bandwidth**: The bells take their Q from the boost/cut pot damping a gyrator tank, so a gentle move is broad and a hard one is tight - which is why there is no Q control on the panel
- **Topology-Preserving Filters**: State-variable stages in TPT form, so a corner lands where it is asked to rather than a few percent low
- **Per-Stage Saturation**: The nonlinearity sits in the integrator path, where the op-amp physically is, so a driven band detunes and compresses the way a real tank does
- **Per-Stage Crosstalk and Noise**: Each of the six stages leaks into the other channel and contributes its own noise; -40 dB bleed, -87.7 dBFS floor
- **Component Tolerance**: 2.5% per channel, so the two sides are never quite the same channel twice
- **Nothing Resampled**: Flat is flat - +/-0.1 dB from 20 Hz to 20 kHz with every control out of circuit

## Download & Installation

### Windows

1. **Download** the latest release from [GitHub Releases](https://github.com/yonie/WetEQ/releases)
2. **Extract** the ZIP file
3. **Copy** `WetEQ.vst3` to your VST3 folder:
   - User: `C:\Users\[Username]\Documents\VST3\`
   - System: `C:\Program Files\Common Files\VST3\`
4. **Restart your DAW** and rescan plugins

### Linux

1. **Download** the latest release from [GitHub Releases](https://github.com/yonie/WetEQ/releases)
2. **Extract** the ZIP file
3. **Copy** `WetEQ.vst3` to your VST3 folder:
   - User: `~/.vst3/`
   - System: `/usr/lib/vst3/`
4. **Restart your DAW** and rescan plugins

### macOS

1. **Download** the latest release from [GitHub Releases](https://github.com/yonie/WetEQ/releases)
2. **Extract** the ZIP file
3. **Copy** `WetEQ.vst3` to your VST3 folder:
   ```
   ~/Library/Audio/Plug-Ins/VST3/
   ```
4. **Remove quarantine attribute** (see below)
5. **Restart your DAW** and rescan plugins

Note that by default, the Library folder may not be shown in the Finder. See the macOS documentation on how to make it visible.

#### ❗️ macOS Security Notice

macOS may block the plugin because it's unsigned. This **does not mean** the plugin is unsafe.

**Remove quarantine attribute:**

```bash
xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/WetEQ.vst3
```

**What this command does:**
- `xattr` = extended attribute tool
- `-r` = recursive (process all files in the bundle)
- `-d` = delete the specified attribute
- `com.apple.quarantine` = the quarantine attribute

Restart your DAW after running the command.

#### Why macOS Blocks This Plugin

When you try to load the plugin in your DAW, you may see an error:

> "WetEQ.vst3" cannot be opened because the developer cannot be verified.

This **does not mean** the plugin contains malware or is unsafe.

This is due to **Apple's security policy**, which requires developers to:
- Enroll in the Apple Developer Program
- Pay **$99/year** for a developer certificate
- Notarize each build with Apple

As an independent developer releasing **free, open-source software** under the MIT license, I currently don't have the budget for Apple's developer program. The complete source code is available on GitHub for anyone to inspect and build themselves.

This is a common issue with free audio plugins on macOS. You'll encounter the same message with many free, open-source VSTs.


## Usage

1. **Load the plugin** in your DAW (Reaper, Cubase, Ableton Live, FL Studio, etc.)
2. **Set the band frequency**, then the boost or cut - the bandwidth follows the amount, so there is nothing else to set
3. **Use HPF and LPF** to clear the extremes; both are out of circuit at the ends of their travel
4. **Drive with GAIN** if you want the input amplifier working - it is not makeup gain and is not compensated
5. **Automate** any control for creative effects

### Parameter Reference

| Parameter | Range | Per detent |
|-----------|-------|------------|
| GAIN | +/-20 dB | 5 dB |
| HPF | 20 Hz - 2 kHz | x1.74 |
| LPF | 2 - 20 kHz | x1.33 |
| LF shelf | 30 - 600 Hz | x1.45 |
| LMF bell | 100 Hz - 4 kHz | x1.55 |
| HMF bell | 400 Hz - 10 kHz | x1.49 |
| HF shelf | 1.5 - 22 kHz | x1.39 |
| Band gain (x4) | +/-15 dB | 3.75 dB |

Frequency knobs are log-spaced with the printed endpoints exact. Every control is
stepped, nine positions each: the smallest move available on a band is 3.75 dB,
which is coarse on purpose - the same reasoning behind WetDelay's six fixed delay
times.

### Mouse

- **Wheel**: one detent per notch
- **Right-click the panel**: UI Zoom - 75%, 100% or 125%

![Specification](docs/signal-path.png)

## Building from Source

If you want to build the plugin yourself, follow these instructions.

### System Requirements

#### Windows
- **Operating System**: Windows 10/11 (64-bit)
- **Build Tools**: 
  - Visual Studio 2022 Build Tools or Community Edition
  - CMake 3.15 or higher
  - Git

#### Linux
- **Operating System**: Linux (x86_64)
- **Build Tools**:
  - GCC or Clang with C++17 support
  - CMake 3.15 or higher
  - Git
- **Dependencies** (Ubuntu/Debian):
  ```
  sudo apt-get install cmake gcc g++ libstdc++6 libx11-xcb-dev libxcb-util-dev \
      libxcb-cursor-dev libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev \
      libfontconfig1-dev libcairo2-dev libgtkmm-3.0-dev libsqlite3-dev \
      libxcb-keysyms1-dev git
  ```

#### macOS
- **Operating System**: macOS 10.13 or higher (Intel) / macOS 11.0 or higher (Apple Silicon)
- **Build Tools**:
  - Xcode Command Line Tools or Xcode
  - CMake 3.15 or higher
  - Git

### Step 1: Clone VST3 SDK

If the `vst3sdk` folder is not present, clone it:

```batch
git clone --recursive https://github.com/steinbergmedia/vst3sdk.git
```

### Step 2: Build

#### Windows
Run the automated build script:

```batch
build.bat
```

This will:
- Configure CMake for Visual Studio 2022
- Build the plugin in Release mode
- Run the VST3 validator (47 automated tests)
- Output: `WetEQ\build\VST3\Release\WetEQ.vst3`

#### Linux
Run the automated build script:

```bash
chmod +x build.sh
./build.sh
```

This will:
- Configure CMake with GCC/Clang
- Build the plugin in Release mode
- Run the VST3 validator (47 automated tests)
- Output: `WetEQ/build/VST3/Release/WetEQ.vst3`

#### macOS
Run the automated build script:

```bash
chmod +x build.sh
./build.sh
```

This will:
- Configure CMake with Clang
- Build the plugin in Release mode
- Run the VST3 validator (47 automated tests)
- Output: `WetEQ/build/VST3/Release/WetEQ.vst3`

### Step 3: Install

#### Windows
To install the plugin to your system's VST3 folder:

```batch
install.bat
```

**Note**: You may need to run as Administrator if you encounter permission errors.

#### Linux
To install the plugin to your user VST3 folder:

```bash
chmod +x install.sh
./install.sh
```

This installs to `~/.vst3/WetEQ.vst3`

#### macOS
To install the plugin to your user VST3 folder:

```bash
chmod +x install.sh
./install.sh
```

This installs to `~/Library/Audio/Plug-Ins/VST3/WetEQ.vst3`

---


## Technical Details

### Architecture

- **Framework**: VST3 SDK (Official Steinberg)
- **Language**: C++17
- **Build System**: CMake (MSBuild on Windows, Make on Linux)
- **GUI**: VSTGUI4

### Audio Processing

- **Host Sample Rates**: Supports 22.05 kHz to 384 kHz
- **Internal Sample Rate**: Host rate - nothing is resampled, quantised or band-limited
- **Host Bit Depth**: 32-bit float processing
- **Frequency Response**: +/-0.1 dB, 20 Hz - 20 kHz, controls out of circuit
- **THD**: 0.30% at unity, 2.8% at +10 dB drive, 16.5% at +20 dB
- **Latency**: 0 samples
- **CPU Usage**: <0.5% (typical)

### Implementation Details

- **Filter Stages**: State-variable, topology-preserving (TPT), six per channel
- **Saturation**: Op-amp curve inside the integrator path, not before or after the filter
- **Proportional Q**: Derived from pot travel damping a gyrator tank, not applied as a formula
- **Shelf Prewarping**: sqrt(A) prewarp, so a shelf corner does not walk as its gain changes
- **Crosstalk**: Injected at every stage, so bleed a later band receives has already been shaped by the bands before it
- **Noise**: One uncorrelated source per stage per channel, plus a tilted component
- **Parameter Glide**: Stepped controls glide the value behind the detent over a few milliseconds, updated every 32 samples
- **Thread Safety**: Lock-free atomic operations for GUI communication

## Project Structure

```
WetEQ/
|-- vst3sdk/                    # VST3 SDK (git submodule)
|-- WetEQ/                      # Plugin source
|   |-- source/
|   |   |-- weteqprocessor.h/cpp       # Audio processing
|   |   |-- weteqcontroller.h/cpp      # Parameter control
|   |   |-- eqengine.h/cpp             # The equaliser
|   |   |-- analogcore.h               # Analog stages, solved rather than approximated
|   |   |-- steppedknob.h/cpp          # Stepped filmstrip knob
|   |   |-- ledmeterview.h/cpp         # LED meters
|   |   |-- weteqcids.h                # Plugin IDs
|   |   `-- version.h                  # Version info
|   |-- resource/
|   |   `-- weteqeditor.uidesc         # GUI definition
|   |-- CMakeLists.txt                 # Build configuration
|   `-- build/                         # Build output (generated)
|-- tools/
|   `-- eqtest.cpp              # DSP measurement harness, no host required
|-- docs/                       # Panel shot and specification sheet
|-- build.bat                   # Build automation script
|-- install.bat                 # Installation script
|-- LICENSE                     # MIT License
`-- README.md                   # This file
```

## Validation Results

The plugin passes all official VST3 validation tests:

**47 tests passed, 0 tests failed**

Key validations:
- Valid state transitions
- Proper bus configuration
- Correct parameter handling
- Sample rate support (22.05 kHz - 384 kHz)
- Thread safety
- Preset save/load
- Plugin suspend/resume

`tools/eqtest.cpp` measures the DSP directly, with no plugin and no host: resolved
knob values against the legends printed on the panel, every band at its own centre
frequency, the relationship between boost and bandwidth, the noise floor, and that
no control clicks when you move it.

```
tools/build-eqtest.bat
```

## Troubleshooting

### macOS Issues

**Plugin not appearing in DAW:**
- You forgot to remove the quarantine attribute - see Installation section above
- Restart your DAW after running the `xattr` command
- Check VST3 scan path: `~/Library/Audio/Plug-Ins/VST3/`
- Verify the folder contains `WetEQ.vst3`

**Still getting "cannot be verified" after running xattr:**
- Right-click the plugin → "Open" → "Open" to bypass Gatekeeper
- Check DAW console for error messages
- Report issue at [GitHub Issues](https://github.com/yonie/WetEQ/issues)

**Plugin crashes DAW:**
- macOS 10.13+ (Intel) or macOS 11.0+ (Apple Silicon) required
- Check DAW console for error messages
- Report issue at [GitHub Issues](https://github.com/yonie/WetEQ/issues)

### Runtime Issues

**Nothing seems to be happening:**
- With every control at its centre detent the plugin is meant to be inaudible.
  Turn a band up or down and it will not be
- HPF and LPF are out of circuit at the ends of their travel, by design

**A boost sounds narrower than expected:**
- That is the design. Bandwidth follows the amount of boost, as it does on the
  hardware, which is why there is no Q control

**Crackling/Clicking:**
- Stepped controls glide the value behind the detent, so a knob move should not
  click. If it does, report as a bug with your DAW and sample rate info


## Author

**Ronald Klarenbeek**
- Website: [https://wetvst.com](https://wetvst.com)
- Email: contact@wetvst.com
- GitHub: [https://github.com/yonie](https://github.com/yonie)

## Trademarks

All product names, trademarks and registered trademarks are property of their
respective owners, and any reference to them here describes only the kind of
equipment this plugin was inspired by. No manufacturer has endorsed, sponsored
or licensed this plugin, and no third-party intellectual property is used in it.

## License

MIT License - Copyright © 2026 Ronald Klarenbeek (Yonie)

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**Note:** This project uses the VST3 SDK which is licensed under a BSD-style license.
See the VST3 SDK license files for details on SDK licensing.

## Acknowledgments

- Steinberg Media Technologies for the VST3 SDK
- VSTGUI framework for cross-platform GUI support
- The audio plugin development community


## Version History

### v1.0.0 (2026-08-30)
- Initial release
- Four bands plus high-pass, low-pass and input drive
- Eleven stepped controls, nine positions each
- Input/output peak metering
- Full VST3 automation support
- Validated with official VST3 validator
- **Modelled as a circuit**:
  - State-variable stages in topology-preserving form
  - Saturation inside the filter loop, where the op-amp is
  - Bandwidth derived from the boost/cut pot damping a gyrator tank
- **Per-Stage Noise and Crosstalk**:
  - -40 dB channel bleed distributed across all six stages
  - 2.5% component tolerance per channel


---

**Built with ❤️ and precision engineering**

## Support

If you find this plugin helpful, consider buying me a coffee!

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-support-yellow?style=flat&logo=buy-me-a-coffee)](https://buymeacoffee.com/yonie)

---

Part of **[WET](https://wetvst.com)** - with [WetDelay](https://github.com/yonie/WetDelay), [WetReverb](https://github.com/yonie/WetReverb) and [WetCompressor](https://github.com/yonie/WetCompressor)
