> **STALE — April 2026. Do not build from this file.**
> Superseded by `../design-notes.md`, which rejects this briefing's reference hardware,
> its control count and its "traction-dependent" status. Kept for the UI mockups and history.
> **The `### Prompt Template` below is NOT stale** — it produced a good panel on Copilot,
> first try, unmodified, on 2026-08-26. Use it as-is.

# WetEQ Development Briefing

**Status:** Planned (Traction-dependent)  
**Last Updated:** April 5, 2026

---

## Overview

WetEQ is a parametric equalizer VST3 plugin - the fourth in the WET VST "Essentials" line.

**Development decision pending:** Will only proceed if BPB article generates strong market traction for existing plugins.

---

## Product Specs

### Core Concept
SSL 4000 E-series style parametric EQ with Proportional Q (like API 550/Maag EQ4).

### Controls (11 knobs total)

**Visual Layout (TWO COLUMN DESIGN):**

```
+------------------------------------------+
|                    WetEQ                 |
+------------------------------------------+
|                                          |
|  [IN]   [OUT]    |   [HF]                |
|  [12]   [12]     |   [dB]    [kHz]       |
|   SEG    SEG     |    (red)   (red)      |
|                  |                      |
|  [GAIN]          |   [HMF]              |
|   (silver)       |   [dB]    [kHz]       |
|                  |    (green) (green)   |
|  [HPF]  [LPF]    |                      |
|   (white) (white)|   [HMF]              |
|                  |   [dB]    [kHz]       |
|                  |    (green) (green)   |
|                  |   [LF]               |
|                  |   [dB]    [Hz]        |
|                  |    (black) (black)   |
+------------------------------------------+

Group Logic:
Left Side (Control Section):
├─ Group 1: IN meter + OUT meter (side by side at top)
├─ Group 2: GAIN knob (silver) - input/output level
└─ Group 3: HPF + LPF filters (white) - filter pair

Right Side (EQ Section):
├─ Section 1: HF (High Frequency) - 2 red knobs
├─ Section 2: HMF (High-Mid Frequency) - 2 green knobs
├─ Section 3: LMF (Low-Mid Frequency) - 2 blue knobs
└─ Section 4: LF (Low Frequency) - 2 black knobs
```

**Left Side (from top to bottom):**
- **Meters:** IN meter (12-seg LED) + OUT meter (12-seg LED), positioned side by side at top
- **GAIN:** Silver knob - controls input/output level
- **Filters:** HPF knob (white) + LPF knob (white) - high-pass/low-pass filter pair

**Right Side (4 EQ sections stacked vertically):**
- **HF:** Label + two red knobs (dB, kHz) - High Frequency shelving
- **HMF:** Label + two green knobs (dB, kHz) - High-Mid Frequency parametric
- **LMF:** Label + two blue knobs (dB, kHz) - Low-Mid Frequency parametric
- **LF:** Label + two black knobs (dB, Hz) - Low Frequency shelving

**Meter Colors:** Green (bottom) → Yellow (middle) → Red (top)

### Technical Specs

- **Knobs:** 11 total rotary encoders
- **Steps:** 10 steps per encoder (discrete positions)
- **Knob colors:** White, Red, Green, Blue, Black, Silver
- **Knob style:** Solid colored SSL E-series style (matte plastic)
- **Layout:** Two-column design (~600px wide x ~500px tall)
- **Background:** Dark gray powder-coated metal
- **Proportional Q:** Automatic (no Q knob needed)
- **Format:** VST3 (Windows/Linux/Mac)

---

## Visual Design - Copilot Briefing

### Prompt Template

```
Generate an image of a VST plugin UI for "WetEQ" - an SSL 4000 E-series style parametric EQ.

**TWO COLUMN LAYOUT:**

**Top section:**
- "WetEQ" title centered at top
- Below title, left side: Two vertical LED meters side by side
  - Left meter labeled "IN" (12 segments: green bottom, yellow middle, red top)
  - Right meter labeled "OUT" (12 segments: green bottom, yellow middle, red top)

**Left column (below meters):**
1. Silver knob labeled "GAIN"
2. Two white knobs side by side: "HPF" (left), "LPF" (right)

**Right column (below meters, 4 sections stacked vertically):**

Section 1 - HF:
- Label "HF" at top
- Two red knobs side by side: "dB" (left), "kHz" (right)

Section 2 - HMF:
- Label "HMF" at top
- Two green knobs side by side: "dB" (left), "kHz" (right)

Section 3 - LMF:
- Label "LMF" at top
- Two blue knobs side by side: "dB" (left), "kHz" (right)

Section 4 - LF:
- Label "LF" at top
- Two black knobs side by side: "dB" (left), "Hz" (right)

**STYLE:**
- SSL 4000 E-series console aesthetic
- Solid colored matte knobs (no transparent rings)
- Dark powder-coated metal panel background
- Labels in clear text above each section
- 1980s hardware studio equipment look
```

### Design References

- SSL 4000 E-series EQ photos (search: "SSL 4000 E series brown knob EQ")
- Solid matte plastic knobs (not transparent, no rings)
- Dark gray powder-coated metal panel

### Implementation Notes

- VSTGUI4 framework
- Stepped encoders (10 positions each)
- Proportional Q calculated from gain setting
- Plugin dimensions: ~600px wide x ~500px tall

---

## Architecture

### Framework
- Steinberg VST3 SDK (NO JUCE)
- VSTGUI4 for UI
- CMake + GitHub Actions CI/CD
- C++17

### Platform Support
- Windows (VST3)
- Linux (VST3)
- macOS Intel + Apple Silicon

---

## Marketing

### Positioning
- Part of "WET Essentials" line
- Free, MIT licensed, open source

### Target Audiences
1. Bedroom producers
2. VST developers (reference EQ implementation)

---

## Resources

- **Business Plan:** `G:\code\WET\WET-VST-Business-Plan.md`
- **Status Tracker:** `G:\code\WET\STATUS.md`
- **WetDelay Source:** `G:\code\WET\wetdelay\` (framework reference)

---

**Contact:** contact@wetvst.com  
**Website:** https://wetvst.com  
**License:** MIT
