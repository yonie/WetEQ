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

Rotation is RELIT, not plain. Turning the whole crop drags the panel's lighting
round with the knob, so the specular highlight ends up on the underside at the
extremes and the knob reads as a sticker rather than a control. A real knob's
pointer slot, bevel and knurling do rotate; the lamp above the console does not.
So each frame is rotated in full and then divided by the lighting it carried
over and multiplied by the lighting of the un-rotated knob, which puts the
highlight back at the top while leaving every rotating detail alone.

    python tools/make-assets.py
"""
import argparse
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', 'tools'))
import knob3d

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
SRC = os.path.join(REPO, 'design', 'panel-2026-08', 'weteq-panel.png')
OUT = os.path.join(REPO, 'WetEQ', 'resource')

TARGET_H = 800          # keep the editor small enough for older machines
SWEEP = 270.0           # pointer travel, min to max
SS = 4                  # supersample for rotation

# NINE detents on every knob, because the art paints nine tick dots around each
# one. Odd, so the centre detent is a real position (0 dB on the gain knobs).
# Must stay in step with EQRange::kSteps in source/eqengine.h.
STEPS = 9
GAIN_STEPS = STEPS
FREQ_STEPS = STEPS

# Supersampling for the knob renders. OFF at Ronald's call, 2026-08-28.
# It does smooth the silhouette - at 1 the rim and the pointer stair-step
# visibly, see shots/weteq-supersampling.png - but it is not what caused the
# banding he was chasing, and he prefers the harder, grainier result. Set this
# back to 2 to restore it; nothing else needs to change.
SUPERSAMPLE = 1

CAP_SATURATION = 1.30   # how far past the painted colour the caps are pushed

# Caps that do NOT come from the art. The panel paints HPF and LPF a warm cream,
# which lands between the coloured caps and a white one and reads as neither.
# The SSL software look Ronald is after puts those two on a near-neutral white
# cap with a dark pointer, while every coloured cap keeps a white one - and
# knob3d already picks the pointer by cap luminance, so only the cap is set here.
CAP_OVERRIDE = {
    'hpf': (233, 232, 229),
    'lpf': (233, 232, 229),
}

# Caps that borrow another knob's colour rather than their own painted one.
# GAIN takes HF's red: the art paints it a warm grey, which reads as a fifth
# colour on a panel that already has four, and it is the master control so it
# should look like it belongs to the set. Aliased rather than copied, so it
# tracks if the art changes.
CAP_ALIAS = {
    'gain': 'hf_gain',
}

LIGHT_BLUR = 0.18       # lighting field = luminance blurred by this x diameter
LIGHT_CLIP = (0.55, 1.85)   # bound the correction so dark knobs cannot blow up

# name, centre x, centre y, CAP radius (on the 1024x1536 original), tag, steps
#
# Measured by casting rays out from each knob and fitting a circle to where the
# gradient peaks - the painted cap's EDGE. Two earlier methods were wrong in
# ways worth remembering:
#
#   Tick-ring fitting put the two HMF knobs 13.6 px apart in y when they are
#   painted level, because a stray blob joins or leaves the ring and drags the
#   fit. That is the "HMF right hand button placed too high" Ronald spotted.
#
#   Centroiding the cap is worse still, biased 15-20 px up and left on every
#   knob, because it finds the middle of the LIT part rather than the middle of
#   the cap. Anything that weights by brightness inherits the lighting.
#
# An edge fit cannot be pulled by lighting, and the result is self-checking:
# every pair now shares a y within 2 px, both columns hold their x within 3 px,
# and the cap radius lands on 0.74 of the tick ring for all ten small knobs.
KNOBS = [
    ('hf_gain',   617.3,  295.4, 60.1, 'kHFGainParam',  GAIN_STEPS),
    ('hf_freq',   868.2,  296.8, 58.0, 'kHFFreqParam',  FREQ_STEPS),
    ('hmf_gain',  617.1,  648.3, 60.4, 'kHMFGainParam', GAIN_STEPS),
    ('hmf_freq',  866.7,  646.4, 61.3, 'kHMFFreqParam', FREQ_STEPS),
    ('gain',      249.8,  893.5, 75.4, 'kGainParam',    GAIN_STEPS),
    ('lmf_gain',  618.3,  999.6, 59.1, 'kLMFGainParam', GAIN_STEPS),
    ('lmf_freq',  867.9,  999.8, 58.5, 'kLMFFreqParam', FREQ_STEPS),
    ('hpf',       137.8, 1243.8, 59.3, 'kHPFParam',     FREQ_STEPS),
    ('lpf',       358.5, 1243.7, 59.0, 'kLPFParam',     FREQ_STEPS),
    ('lf_gain',   619.9, 1333.7, 58.1, 'kLFGainParam',  GAIN_STEPS),
    ('lf_freq',   868.5, 1333.7, 58.3, 'kLFFreqParam',  FREQ_STEPS),
]

# Meter geometry, in original-art coordinates.
#
# METER_PAINTED is the extent of the LEDs painted into the render. Those get
# blacked out (below) so nothing shows through the drawn ones - but ONLY those.
# Blacking the whole recess instead destroys the painted bevel around it, and
# the meter then reads as a black box stuck on the panel rather than a window
# sunk into it. The bevel is backplate art and must survive.
#
# The drawn stack is inset inside that by METER_MARGIN. Keep that inset SMALL:
# the breathing room Ronald wants is BETWEEN the segments, not around the block.
# A previous pass put 11 px round the outside and the whole meter shrank -
# "more padding around the meter itself, which makes it look tiny". The gap is
# what carries the look; the outer margin only needs to clear the bevel. The previous values did the opposite - they were the painted
# extent "padded outwards", so the drawn rectangles were LARGER than the thing
# they replace and sat hard against the bevel. Measured on a live capture that
# left 3 px of margin on the left and 5 on the right, reading as both cramped
# and off-centre.
METER_PAINTED = {
    'kInputMeter':  (153, 198, 211, 652),
    'kOutputMeter': (316, 198, 374, 652),
}
METER_MARGIN_X = 2      # art px inset from the painted LEDs, each side
METER_MARGIN_Y = 4

METERS = {tag: (x0 + METER_MARGIN_X, y0 + METER_MARGIN_Y,
                x1 - METER_MARGIN_X, y1 - METER_MARGIN_Y)
          for tag, (x0, y0, x1, y1) in METER_PAINTED.items()}

METER_SEGMENTS = 12

# The drawn LED stack is deliberately NARROWER than the blacked-out window and
# has a wide gap. At 30 px wide with a 1 px gap the segments were 30 x 17.8
# slabs separated by a single pixel - nearly 9:1 - and the meter read as one bar
# changing colour rather than as twelve LEDs. WetReverb gets away with a small
# gap because its meters are long and thin (265 x 16), so the gap is
# proportionally far more visible. Inset and gap bring WetEQ into the same
# proportion; the art behind is already black, so the inset just shows more of it.
METER_INSET = 0         # px off each side of the painted window
METER_GAP = 8           # px of black between segments


def lighting_field(knob, blur_frac=LIGHT_BLUR):
    """The lamp, not the detail: low-frequency luminance of the knob."""
    radius = max(2.0, knob.size[0] * blur_frac)
    return knob.convert('L').filter(ImageFilter.GaussianBlur(radius))


def relit_frame(knob, light, angle):
    """Rotate the knob by angle, then put the static lighting back."""
    centre = ((knob.size[0] - 1) / 2.0,) * 2
    rot = knob.rotate(-angle, resample=Image.BICUBIC, center=centre)
    lrot = light.rotate(-angle, resample=Image.BICUBIC, center=centre)

    static = np.asarray(light).astype(float)
    carried = np.asarray(lrot).astype(float)
    # +10 keeps the ratio finite where the knob is nearly black (the LF pair)
    ratio = np.clip((static + 10.0) / (carried + 10.0), *LIGHT_CLIP)[..., None]

    px = np.asarray(rot).astype(float)
    px[..., :3] = np.clip(px[..., :3] * ratio, 0, 255)
    return Image.fromarray(px.astype('uint8'), 'RGBA')


def _parse_args():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--tilt', type=float, default=0.0, metavar='DEG',
                    help='raise the knob camera off the axis. 0 is what ships; '
                         'anything else is for looking, since a tilted knob on '
                         'a panel drawn face-on is a lie about where the viewer '
                         'is standing')
    ap.add_argument('--out', default=OUT, help='where to write the assets')
    return ap.parse_args()


def main():
    os.makedirs(OUT, exist_ok=True)

    src = Image.open(SRC).convert('RGB')
    sw, sh = src.size
    tw = round(sw * TARGET_H / sh)
    scale = TARGET_H / sh

    panel = src.resize((tw, TARGET_H), Image.LANCZOS)

    # Black the LED windows out in the art itself. LEDMeterView also fills its
    # own rect before drawing, but that only helps where the two agree to the
    # pixel; one row of painted LED peeking past the view's top edge reads as a
    # meter stuck at full scale. Belt and braces: nothing colourful is left
    # underneath. The 2 px inset keeps the painted recess bevel intact.
    # Pad OUTWARDS, never inwards. The drawn LED stack is inset from this
    # window (METER_INSET), so anything the blackening fails to reach shows as a
    # coloured fringe down both sides of the meter - which is exactly what an
    # inset here produced.
    art = ImageDraw.Draw(panel)
    for (x0, y0, x1, y1) in METER_PAINTED.values():
        art.rectangle([round(x0 * scale) - 1, round(y0 * scale) - 1,
                       round(x1 * scale) + 1, round(y1 * scale) + 1],
                      fill=(0, 0, 0))

    panel.save(os.path.join(OUT, 'backplate.png'))
    print(f'backplate.png  {sw}x{sh} -> {tw}x{TARGET_H}  (scale {scale:.5f})')

    layout = {'panel': {'size': [tw, TARGET_H]}, 'knobs': {}, 'meters': {}}

    # Sample every painted cap first, so CAP_ALIAS can point one knob at
    # another's colour without depending on the order they are rendered in.
    _painted = {}
    for name, cx, cy, cap_r, tag, steps in KNOBS:
        s = int(cap_r * 0.45)
        patch = np.asarray(src.crop((int(cx - s), int(cy - s),
                                     int(cx + s), int(cy + s))).convert('RGB'))
        _painted[name] = tuple(int(v) for v in np.median(patch.reshape(-1, 3), axis=0))

    for name, cx, cy, cap_r, tag, steps in KNOBS:
        painted = _painted[CAP_ALIAS.get(name, name)]
        # Solve for the albedo that RENDERS as the painted colour, then push the
        # saturation past it a little: the panel art's caps are already muted by
        # its own lighting, and reproducing that exactly reads as washed out.
        if name in CAP_OVERRIDE:
            painted = CAP_OVERRIDE[name]
            cap = knob3d.albedo_for(painted, saturation=1.0)
        else:
            cap = knob3d.albedo_for(painted, saturation=CAP_SATURATION)

        # The measurement is of the CAP; the 3D knob's silhouette is its SHAFT,
        # and the cap covers R_CAP of that. Divide back out rather than carry a
        # separate fudge factor - this is why BODY_OVER_RING is gone.
        r_px = (cap_r * scale) / knob3d.R_CAP
        probe, off = knob3d.render_at(r_px, 0.0, cap, ss=SUPERSAMPLE)
        fd = probe.size[0]

        strip = Image.new('RGBA', (fd, fd * steps), (0, 0, 0, 0))
        step = SWEEP / (steps - 1)
        for i in range(steps):
            angle = -SWEEP / 2 + i * step        # frame 0 = min = -135 deg
            frame = probe if i == 0 and False else knob3d.render_at(r_px, angle, cap, ss=SUPERSAMPLE)[0]
            strip.paste(frame, (0, i * fd), frame)
        strip.save(os.path.join(OUT, f'knob_{name}.png'))

        ox = int(round(cx * scale)) - off
        oy = int(round(cy * scale)) - off
        layout['knobs'][name] = dict(origin=[ox, oy], size=[fd, fd],
                                     frames=steps, tag=tag)
        print(f'knob_{name}.png  {fd}x{fd} x{steps} frames  '
              f'origin=({ox},{oy})  painted{painted} -> albedo{cap}  {tag}')

    for tag, (x0, y0, x1, y1) in METERS.items():
        ox, oy = int(round(x0 * scale)) + METER_INSET, int(round(y0 * scale))
        w = int(round((x1 - x0) * scale)) - 2 * METER_INSET
        h = int(round((y1 - y0) * scale))
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


def _tilted(deg):
    """Raise knob3d's camera off the axis for the duration of a run."""
    import contextlib

    @contextlib.contextmanager
    def ctx():
        if not deg:
            yield
            return
        saved = knob3d.CAM.copy()
        t = math.radians(deg)
        dist = float(np.linalg.norm(knob3d.CAM - knob3d.TARGET))
        knob3d.CAM = knob3d.TARGET + np.array([0.0, -math.sin(t), math.cos(t)]) * dist
        try:
            yield
        finally:
            knob3d.CAM = saved
    return ctx()


if __name__ == '__main__':
    _args = _parse_args()
    OUT = _args.out
    with _tilted(_args.tilt):
        if _args.tilt:
            print(f'camera raised {_args.tilt:g} deg off the axis '
                  f'-- FOR LOOKING ONLY, do not ship these')
        main()
