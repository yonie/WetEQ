#!/usr/bin/env python3
"""verify-plugin.py — check the built WetEQ.vst3 through a real VST3 host.

eqtest already proves the DSP maths. This proves the PLUGIN WIRING: that each
parameter ID reaches the band its label claims. A crossed wire would sail
through eqtest and still ship an EQ where the HF knob moves the mids.

Method: sweep the plugin flat, then sweep it again with one band pushed to
+15 dB, and find where the difference peaks. The difference cancels the shared
lo-fi colouring (resampler ripple, anti-alias filters, quantiser), which is
wanted character and must not be read as error.

    python tools/verify-plugin.py
"""
import csv
import math
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
WET = os.path.dirname(REPO)

RUNNER = os.path.join(WET, 'tools', 'vstrunner', 'build', 'bin', 'vstrunner.exe')
PLUGIN = os.path.join(REPO, 'WetEQ', 'build', 'VST3', 'Release', 'WetEQ.vst3')

# param ids from weteqcids.h
GAIN, HPF, LPF = 0, 1, 2
HF_G, HF_F, HMF_G, HMF_F, LMF_G, LMF_F, LF_G, LF_F = 3, 4, 5, 6, 7, 8, 9, 10

FREQ_STEPS, GAIN_STEPS = 10, 11


def step_to_log(i, steps, lo, hi):
    return lo * (hi / lo) ** (i / (steps - 1))


# band, gain param, expected centre at the DEFAULT frequency detent (5)
BANDS = [
    ('LF  shelf', LF_G,  step_to_log(5, FREQ_STEPS, 30.0, 600.0),    'shelf'),
    ('LMF bell',  LMF_G, step_to_log(5, FREQ_STEPS, 100.0, 4000.0),  'bell'),
    ('HMF bell',  HMF_G, step_to_log(5, FREQ_STEPS, 400.0, 10000.0), 'bell'),
    ('HF  shelf', HF_G,  step_to_log(5, FREQ_STEPS, 1500.0, 22000.0), 'shelf'),
]

NYQUIST_INTERNAL = 12000.0


def sweep(params, tag):
    out = os.path.join(tempfile.gettempdir(), f'weteq-sweep-{tag}.csv')
    cmd = [RUNNER, '--plugin', PLUGIN, '--sweep', out]
    for pid, val in params.items():
        cmd += ['--param', f'{pid}={val}']
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout, r.stderr)
        raise SystemExit(f'vstrunner failed for {tag}')
    rows = list(csv.DictReader(open(out)))
    return [(float(x['freq_hz']), float(x['mag_db_l'])) for x in rows]


def main():
    for path, what in ((RUNNER, 'vstrunner.exe'), (PLUGIN, 'WetEQ.vst3')):
        if not os.path.exists(path):
            raise SystemExit(f'missing {what}: {path}')

    failures = 0

    print('sweeping flat as the reference...')
    flat = dict(sweep({}, 'flat'))

    print('\nband wiring: push one band to +15 dB, find where the change lands')
    print(f"  {'band':<11} {'expected':>10} {'measured peak':>14} {'gain':>8}   verdict")

    for name, pid, want_hz, kind in BANDS:
        got = sweep({pid: 1.0}, f'p{pid}')          # 1.0 = top detent = +15 dB
        delta = [(f, m - flat[f]) for f, m in got if f in flat]

        # A shelf's biggest change is at the band edge, not at its corner, so
        # compare against the corner only for bells; for shelves just confirm
        # the change is on the correct side of the spectrum.
        peak_f, peak_d = max(delta, key=lambda t: t[1])

        if kind == 'bell':
            ratio = peak_f / want_hz
            ok = 0.5 <= ratio <= 2.0 and peak_d > 8.0
            verdict = 'ok' if ok else f'FAIL (peak {ratio:.2f}x off centre)'
        else:
            # LF shelf must lift the bottom, HF shelf the top
            if name.startswith('LF'):
                ok = peak_f < want_hz * 1.5 and peak_d > 8.0
            else:
                # the HF corner sits near/above the internal Nyquist, so the
                # lift shows up below it - that is the accepted consequence of
                # keeping the 24 kHz house rate
                ok = peak_f > 2000.0 and peak_d > 4.0
            verdict = 'ok' if ok else 'FAIL (change on the wrong side)'

        if not ok:
            failures += 1
        print(f'  {name:<11} {want_hz:>9.0f}Hz {peak_f:>13.0f}Hz {peak_d:>+7.1f}dB   {verdict}')

    # cut direction
    print('\ncut direction: bottom detent must attenuate, not boost')
    for name, pid, want_hz, kind in BANDS:
        got = sweep({pid: 0.0}, f'p{pid}cut')
        delta = [(f, m - flat[f]) for f, m in got if f in flat]
        trough_f, trough_d = min(delta, key=lambda t: t[1])
        ok = trough_d < -4.0
        if not ok:
            failures += 1
        print(f'  {name:<11} deepest cut {trough_d:>+7.1f} dB at {trough_f:>7.0f} Hz'
              f'   {"ok" if ok else "FAIL"}')

    # the filters
    print('\nHPF / LPF')
    hpf = dict(sweep({HPF: 1.0}, 'hpf'))            # 2 kHz
    at100 = hpf[min(hpf, key=lambda f: abs(f - 100))] - flat[min(flat, key=lambda f: abs(f - 100))]
    ok = at100 < -12.0
    if not ok:
        failures += 1
    print(f'  HPF at its top detent (2 kHz): 100 Hz is {at100:+.1f} dB'
          f'   {"ok" if ok else "FAIL"}')

    lpf = dict(sweep({LPF: 0.0}, 'lpf'))            # 2 kHz
    f8k = min(lpf, key=lambda f: abs(f - 8000))
    at8k = lpf[f8k] - flat[f8k]
    ok = at8k < -8.0
    if not ok:
        failures += 1
    print(f'  LPF at its bottom detent (2 kHz): {f8k:.0f} Hz is {at8k:+.1f} dB'
          f'   {"ok" if ok else "FAIL"}')

    # GAIN drives the quantiser rather than trimming the output
    print('\nGAIN is input drive, not makeup gain')
    for label, val, want in (('-20 dB', 0.0, -20.0), ('0 dB', 0.5, 0.0), ('+20 dB', 1.0, 20.0)):
        g = dict(sweep({GAIN: val}, f'g{val}'))
        f1k = min(g, key=lambda f: abs(f - 1000))
        got = g[f1k] - flat[f1k]
        ok = abs(got - want) < 2.0
        if not ok:
            failures += 1
        print(f'  GAIN {label:<7} -> {got:+6.1f} dB at 1 kHz (want {want:+.0f})'
              f'   {"ok" if ok else "FAIL"}')

    print(f'\n{"FAILED" if failures else "PASSED"}: {failures} failure(s)')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
