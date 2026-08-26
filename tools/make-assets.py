#!/usr/bin/env python3
"""make-assets.py — build WetEQ's VSTGUI resources from the panel art.

Single source of truth for geometry: this script measures the panel once and
emits the backplate, the knob filmstrips AND the .uidesc, so the coordinates in
the UI can never drift from the coordinates in the art.

Knobs are cropped from the scaled panel itself, so each filmstrip overlays its
painted knob exactly and the surrounding art (tick dots, legends) shows through.

Each filmstrip gets exactly ONE FRAME PER DETENT. The controls are stepped, so
frame index == step index and every pointer angle is precisely right. A generic
31-frame strip would land most detents between frames.

    python tools/make-assets.py
"""
import json
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
SRC = os.path.join(REPO, 'design', 'panel-2026-08', 'weteq-panel.png')
OUT = os.path.join(REPO, 'WetEQ', 'resource')

TARGET_H = 800          # keep the editor small enough for older machines
SWEEP = 270.0           # pointer travel, min to max
SS = 4                  # supersample for rotation

GAIN_STEPS = 11         # dB knobs: odd, so 0 dB sits at the centre detent
FREQ_STEPS = 10         # frequency knobs: no centre value needed

# name, centre x, centre y, radius (measured on the 1024x1536 original),
# control tag, step count
#
# The nine coloured/light knobs were located with tools/backplate-map.py and
# confirmed by eye against the rendered hitboxes.
#
# The two black LF knobs defeat every direct measurement: Hough finds no circle
# (black body on a black panel), a dark-mass centroid is dragged off by the
# drop shadows, and a tick-dot ring fit picks up the numerals - that last method
# returned (590,1027) for a knob known to sit at (619,996), so it was discarded.
#
# They come from the grid instead, which is regular and independently checked:
# rows at y 296 / 649 / 996 give a mean pitch of 350, so LF sits at 1346; the
# two columns are x 619 and 867 in all three known bands. Verified afterwards
# by rendering the hitboxes and looking at them.
KNOBS = [
    ('hf_gain',   619,  296, 57, 'kHFGainParam',  GAIN_STEPS),
    ('hf_freq',   867,  297, 57, 'kHFFreqParam',  FREQ_STEPS),
    ('hmf_gain',  619,  649, 58, 'kHMFGainParam', GAIN_STEPS),
    ('hmf_freq',  867,  646, 62, 'kHMFFreqParam', FREQ_STEPS),
    ('gain',      249,  896, 72, 'kGainParam',    GAIN_STEPS),
    ('lmf_gain',  619,  996, 62, 'kLMFGainParam', GAIN_STEPS),
    ('lmf_freq',  866,  996, 62, 'kLMFFreqParam', FREQ_STEPS),
    ('hpf',       138, 1247, 62, 'kHPFParam',     FREQ_STEPS),
    ('lpf',       358, 1243, 59, 'kLPFParam',     FREQ_STEPS),
    ('lf_gain',   619, 1346, 62, 'kLFGainParam',  GAIN_STEPS),
    ('lf_freq',   867, 1346, 62, 'kLFFreqParam',  FREQ_STEPS),
]

# 12 segments each. Bounds are the LIT pixel extent padded outwards: the art has
# every LED lit, so LEDMeterView must cover the painted strip completely or a
# full-scale LED peeks out at the edge and the meter looks permanently pinned.
METERS = {
    'kInputMeter':  (153, 198, 211, 652),
    'kOutputMeter': (316, 198, 374, 652),
}
METER_SEGMENTS = 12


def main():
    os.makedirs(OUT, exist_ok=True)

    src = Image.open(SRC).convert('RGB')
    sw, sh = src.size
    tw = round(sw * TARGET_H / sh)
    scale = TARGET_H / sh

    panel = src.resize((tw, TARGET_H), Image.LANCZOS)
    panel.save(os.path.join(OUT, 'backplate.png'))
    print(f'backplate.png  {sw}x{sh} -> {tw}x{TARGET_H}  (scale {scale:.5f})')

    layout = {'panel': {'size': [tw, TARGET_H]}, 'knobs': {}, 'meters': {}}

    for name, cx, cy, r, tag, steps in KNOBS:
        rs = r - 4                                   # stay inside the tick dots
        knob = src.crop((cx - rs, cy - rs, cx + rs, cy + rs)).convert('RGBA')
        mask = Image.new('L', knob.size, 0)
        ImageDraw.Draw(mask).ellipse([0, 0, knob.size[0] - 1, knob.size[1] - 1], fill=255)
        knob.putalpha(mask)

        fd = max(8, int(round(2 * rs * scale)))      # on-screen diameter
        big = knob.resize((knob.size[0] * SS, knob.size[1] * SS), Image.LANCZOS)

        strip = Image.new('RGBA', (fd, fd * steps), (0, 0, 0, 0))
        step = SWEEP / (steps - 1)
        for i in range(steps):
            angle = -SWEEP / 2 + i * step            # frame 0 = min = -135 deg
            rot = big.rotate(-angle, resample=Image.BICUBIC,
                             center=(big.size[0] / 2, big.size[1] / 2))
            strip.paste(rot.resize((fd, fd), Image.LANCZOS), (0, i * fd),
                        rot.resize((fd, fd), Image.LANCZOS))
        strip.save(os.path.join(OUT, f'knob_{name}.png'))

        ox = int(round(cx * scale - fd / 2.0))
        oy = int(round(cy * scale - fd / 2.0))
        layout['knobs'][name] = dict(origin=[ox, oy], size=[fd, fd],
                                     frames=steps, tag=tag)
        print(f'knob_{name}.png  {fd}x{fd} x{steps} frames  '
              f'origin=({ox},{oy})  {tag}')

    for tag, (x0, y0, x1, y1) in METERS.items():
        ox, oy = int(round(x0 * scale)), int(round(y0 * scale))
        w, h = int(round((x1 - x0) * scale)), int(round((y1 - y0) * scale))
        layout['meters'][tag] = dict(origin=[ox, oy], size=[w, h],
                                     segments=METER_SEGMENTS)
        print(f'meter {tag}: origin=({ox},{oy}) size={w}x{h}')

    with open(os.path.join(OUT, 'layout.json'), 'w') as f:
        json.dump(layout, f, indent=2)

    write_uidesc(layout)


def write_uidesc(layout):
    tw, th = layout['panel']['size']

    tags = ['kGainParam', 'kHPFParam', 'kLPFParam',
            'kHFGainParam', 'kHFFreqParam', 'kHMFGainParam', 'kHMFFreqParam',
            'kLMFGainParam', 'kLMFFreqParam', 'kLFGainParam', 'kLFFreqParam',
            'kInputMeter', 'kOutputMeter']

    lines = []
    a = lines.append
    a('<?xml version="1.0" encoding="UTF-8"?>')
    a('<vstgui-ui-description version="1">')
    a('\t<fonts>')
    a('\t\t<font name="TinyFont" font-name="Arial" size="0"/>')
    a('\t</fonts>')
    a('')
    a('\t<colors>')
    a('\t\t<color name="Transparent" rgba="#00000000"/>')
    a('\t</colors>')
    a('')
    a('\t<control-tags>')
    for i, t in enumerate(tags):
        a(f'\t\t<control-tag name="{t}" tag="{i}"/>')
    a('\t</control-tags>')
    a('')
    a('\t<!-- Generated by tools/make-assets.py from the panel art.')
    a('\t     Do not hand-edit: re-run the script instead, so the geometry here')
    a('\t     stays tied to the measured positions of the painted controls. -->')
    a(f'\t<template background-color="~ BlackCColor"')
    a('\t          bitmap="backplate"')
    a('\t          class="CViewContainer"')
    a('\t          name="view"')
    a(f'\t          size="{tw}, {th}"')
    a('\t          transparent="false">')
    a('')

    a('\t\t<!-- LED meters: 12 segments, vertical, green at the bottom -->')
    for tag, m in layout['meters'].items():
        ox, oy = m['origin']
        w, h = m['size']
        a('\t\t<view class="LEDMeterView"')
        a(f'\t\t      control-tag="{tag}"')
        a(f'\t\t      origin="{ox}, {oy}"')
        a(f'\t\t      size="{w}, {h}"')
        a(f'\t\t      num-segments="{m["segments"]}"')
        a('\t\t      segment-gap="1"')
        a('\t\t      horizontal="false"')
        a('\t\t      transparent="false"/>')
        a('')

    a('\t\t<!-- Stepped knobs. One filmstrip frame per detent, so the frame')
    a('\t\t     index is the step index. Mouse wheel up = clockwise. -->')
    for name, k in layout['knobs'].items():
        ox, oy = k['origin']
        w, h = k['size']
        a('\t\t<view class="SteppedKnob"')
        a(f'\t\t      control-tag="{k["tag"]}"')
        a(f'\t\t      origin="{ox}, {oy}"')
        a(f'\t\t      size="{w}, {h}"')
        a(f'\t\t      bitmap="knob_{name}"')
        a(f'\t\t      step-count="{k["frames"]}"')
        a('\t\t      transparent="true"/>')
        a('')

    a('\t</template>')
    a('')
    a('\t<custom>')
    a('\t\t<attributes name="FocusDrawing"/>')
    a('\t\t<attributes Path="weteqeditor.uidesc" name="VST3Editor"/>')
    a('\t</custom>')
    a('')
    a('\t<bitmaps>')
    a('\t\t<bitmap name="backplate" path="backplate.png"/>')
    # The filmstrip is declared on the BITMAP, using VSTGUI's CMultiFrameBitmap
    # attributes rather than the deprecated per-view height-of-one-image.
    # "mulitframe-frames-per-row" is spelt that way inside VSTGUI itself
    # (uidescription/detail/uinode.cpp) - the typo has to be matched exactly.
    for name, k in layout['knobs'].items():
        w, h = k['size']
        a(f'\t\t<bitmap name="knob_{name}" path="knob_{name}.png"')
        a(f'\t\t        multiframe-size="{w}, {h}"')
        a(f'\t\t        multiframe-num-frames="{k["frames"]}"')
        a('\t\t        mulitframe-frames-per-row="1"/>')
    a('\t</bitmaps>')
    a('</vstgui-ui-description>')

    path = os.path.join(OUT, 'weteqeditor.uidesc')
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'\nwrote {path}')


if __name__ == '__main__':
    main()
