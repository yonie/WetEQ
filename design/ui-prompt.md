# WetEQ — UI prompts

The April 2026 original is in `briefing.md` under `### Prompt Template`. It still exists; nothing
was lost for this plugin. Below is a retry version fixing what went wrong, plus what to check.

## What actually went wrong in April

The prompt is fine as a layout description. The failures came from what it left unsaid:

| Symptom in the mockups | Cause |
|---|---|
| Meter scales reading `-24 -12 -18 --0` and `1 5 3 6 6 5 3 6 6 6` | The prompt never asked for a scale. The generator **volunteered** numeric detail. Nothing forbade it. |
| Output 1536x1024 and one portrait 1024x1536 | No canvas or aspect specified. |
| Corner screws on every mockup | Not forbidden. Neither shipped panel has screws. |
| Knob sizes varying wildly; two unlabelled extra knobs in `Designer(10).png` | No "identical size" or "exactly N controls" constraint. |
| Labels above the knobs in one mockup, below in another | "Labels above each section" is ambiguous about the knob labels themselves. |

Isolated words rendered correctly throughout — `HPF`, `LPF`, `kHz`, `HMF`, `WetEQ` are all clean.
The generator is reliable on words and unreliable on sequences of small numerals.

## Retry prompt (2026)

```
A photorealistic product photograph of the front panel of a 1980s studio rack equaliser,
shot straight on from directly overhead. Perfectly square 1:1 framing, 702x702, flat
orthographic view, no perspective, no tilt. The panel fills the entire frame edge to edge.

PANEL: dark charcoal grey powder-coated metal, fine matte texture, one flat surface.
No screws, no rack ears, no bolts, no seams.

LAYOUT:
- Product name "WetEQ" centred across the top, silver condensed sans-serif capitals.
- Below the name on the left: two narrow vertical LED meter strips side by side, set into
  recessed windows. Green segments at the bottom, yellow in the middle, red at the top.
  A short label sits directly above each strip.
- Left column below the meters: one large silver knob, then two off-white knobs side by side.
- Right column: four rows of two knobs each. Row 1 both red, row 2 both green, row 3 both
  blue, row 4 both black. A section label sits above each row.
- Every knob is EXACTLY the same diameter except the single silver one, which is larger.
  Each knob is solid matte plastic with a thin light-coloured pointer line and no chrome ring.
  Exactly 11 knobs in total, no others.
- One short label directly beneath each knob.

TEXT — render exactly these strings and nothing else:
  WetEQ, IN, OUT, GAIN, HPF, LPF, HF, HMF, LMF, LF, dB, kHz, Hz

LIGHTING: soft and even from directly above. Every recessed window and every knob edge carries
a distinct dark bevel shadow in its groove. Gentle falloff toward the panel corners.

DO NOT INCLUDE: any text, letter, number or symbol not in the list above. No numeric scales,
no dB legends, no graduation marks, no tick marks, no dashes, no dots around the knobs, no
screws, no LEDs outside the two meter strips, no sliders, no buttons, no cables, no connectors,
no brand logos, no watermark, no perspective, no room reflections, no background — the panel
is the whole image.
```

## Better still: generate it blank

Everything above is one prompt away from going wrong again, because the text list is long and
repetitive. The safer route is to replace the TEXT block with:

```
TEXT: none. The panel carries no lettering, no numbers and no markings of any kind. Leave clean
bare panel above each meter strip, below each knob, and across the top where the name will go.
```

Then draw all lettering in a script. Correct font, exact positions, and every control coordinate
is then known by construction rather than measured afterwards.

## After generating

```
python tools/backplate-map.py weteq/<panel>.png          # exact uidesc geometry
python tools/knob-filmstrip.py <one-knob>.png --frames 31 # never ask the generator for a strip
```

## Caveat on the design itself

This prompt describes the **SSL 4000 E-series, 11-knob** design. `design-notes.md` rejects that
in favour of the **Maag EQ4** (far fewer controls, and unique rather than a saturated target), and
notes that 11 knobs will not look like it belongs to a line that currently has no knobs at all.
Use this prompt to test whether the 2026 generators handle the lettering — not as the spec for
what to build.
