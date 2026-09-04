//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/events.h"

namespace Yonie {

//------------------------------------------------------------------------
// SteppedKnob - a filmstrip knob that only rests on discrete positions.
//
// Every WetEQ control is a stepped encoder: coarse on purpose, so it forces a
// decision instead of inviting a 0.5 dB fiddle. That means:
//
//   * the value always snaps to a detent, however it was changed
//   * one mouse-wheel notch moves exactly one detent, which is what every
//     console-style plugin does. VSTGUI's default wheel increment is 0.1 of the
//     full range, which on an 11-position knob would jump an inconsistent
//     number of steps.
//   * dragging moves through the detents rather than sliding between them
//
// TWO GRIDS, ONE FILMSTRIP. stepCount is the fine grid and equals the number of
// frames, so the pointer is truthful at every position it can hold. coarseStep
// is how many fine steps make one ordinary detent: without a modifier the knob
// only rests on multiples of it, which is the coarse feel the plugin is built
// around. Hold SHIFT while dragging, scrolling or arrowing and every fine step
// is reachable. Shift is VSTGUI's kZoomModifier, so in linear knob mode it also
// slows the drag - the two work together rather than fighting.
//
// coarseStep must divide (stepCount - 1) exactly, or the coarse detents drift
// off the fine grid and the top of the knob stops being reachable coarsely.
//------------------------------------------------------------------------
class SteppedKnob : public VSTGUI::CAnimKnob
{
public:
    SteppedKnob(const VSTGUI::CRect& size);

    void setStepCount(int count);
    int getStepCount() const { return stepCount; }

    // Fine steps per ordinary detent. 1 means there is no coarse grid and
    // every step is reachable without a modifier.
    void setCoarseStep(int count);
    int getCoarseStep() const { return coarseStep; }

    // overrides
    void onMouseWheelEvent(VSTGUI::MouseWheelEvent& event) override;
    void onKeyboardEvent(VSTGUI::KeyboardEvent& event) override;
    void setValue(float val) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where,
                                           const VSTGUI::CButtonState& buttons) override;

    CLASS_METHODS(SteppedKnob, VSTGUI::CAnimKnob)

private:
    // Move by whole detents and report the change to the host.
    void nudge(int detents, bool fine);

    // Nearest detent to a normalised value, as a normalised value.
    float snap(float normalized, bool fine) const;

    int stepCount = 11;   // number of positions, not the number of gaps
    int coarseStep = 1;   // fine steps per detent when no modifier is held
};

//------------------------------------------------------------------------
} // namespace Yonie
