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
// The filmstrip is authored with exactly one frame per position, so the frame
// index equals the step index and every pointer angle is exactly right.
//------------------------------------------------------------------------
class SteppedKnob : public VSTGUI::CAnimKnob
{
public:
    SteppedKnob(const VSTGUI::CRect& size);

    void setStepCount(int count);
    int getStepCount() const { return stepCount; }

    // overrides
    void onMouseWheelEvent(VSTGUI::MouseWheelEvent& event) override;
    void onKeyboardEvent(VSTGUI::KeyboardEvent& event) override;
    void setValue(float val) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where,
                                           const VSTGUI::CButtonState& buttons) override;

    CLASS_METHODS(SteppedKnob, VSTGUI::CAnimKnob)

private:
    // Move by whole detents and report the change to the host.
    void nudge(int detents);

    // Nearest detent to a normalised value, as a normalised value.
    float snap(float normalized) const;

    int stepCount = 11;   // number of positions, not the number of gaps
};

//------------------------------------------------------------------------
} // namespace Yonie
