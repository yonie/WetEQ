//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "steppedknob.h"

#include <cmath>

using namespace VSTGUI;

namespace Yonie {

namespace {

// Shift. VSTGUI already calls this the zoom modifier and uses it to slow a
// linear drag, so reusing it means one key does one thing: finer.
inline bool isFine(const CButtonState& buttons)
{
    return (buttons & CControl::kZoomModifier) != 0;
}

inline bool isFine(const Modifiers& modifiers)
{
    return modifiers.has(ModifierKey::Shift);
}

} // namespace

//------------------------------------------------------------------------
SteppedKnob::SteppedKnob(const CRect& size)
: CAnimKnob(size, nullptr, -1, nullptr)
{
    setStepCount(stepCount);
}

//------------------------------------------------------------------------
void SteppedKnob::setStepCount(int count)
{
    stepCount = count > 1 ? count : 2;

    // One wheel notch = one detent. VSTGUI multiplies deltaY by this.
    setWheelInc(1.0f / static_cast<float>(stepCount - 1));

    // CAnimKnob picks its frame from the normalised value across the strip.
    // The strip has one frame per FINE step, so frame == step and a Shift move
    // turns the pointer by exactly what it changed.
    setInverseBitmap(false);
}

//------------------------------------------------------------------------
void SteppedKnob::setCoarseStep(int count)
{
    coarseStep = count > 1 ? count : 1;
}

//------------------------------------------------------------------------
float SteppedKnob::snap(float normalized, bool fine) const
{
    const int gaps = stepCount - 1;
    const int unit = fine ? 1 : coarseStep;
    if (normalized < 0.f) normalized = 0.f;
    if (normalized > 1.f) normalized = 1.f;

    // Round to the nearest multiple of the active grid. coarseStep divides
    // gaps exactly, so the top of the travel is on the coarse grid too.
    int index = static_cast<int>(std::floor(normalized * gaps / unit + 0.5f)) * unit;
    if (index < 0) index = 0;
    if (index > gaps) index = gaps;
    return static_cast<float>(index) / static_cast<float>(gaps);
}

//------------------------------------------------------------------------
void SteppedKnob::setValue(float val)
{
    const float lo = getMin();
    const float hi = getMax();
    const float range = hi - lo;
    if (range <= 0.f)
    {
        CAnimKnob::setValue(val);
        return;
    }
    // Always snap to the FINE grid here, never the coarse one. This is the
    // path the host and the controller write through - automation, a preset,
    // a typed value - and quantising someone else's value to our coarse grid
    // would throw away a fine setting the moment anything touched the knob.
    // The coarse feel belongs to the input handlers below, not to the value.
    const float norm = snap((val - lo) / range, true);
    CAnimKnob::setValue(lo + norm * range);
}

//------------------------------------------------------------------------
void SteppedKnob::nudge(int detents, bool fine)
{
    if (detents == 0)
        return;

    const int unit = fine ? 1 : coarseStep;
    const float stepSize = static_cast<float>(unit) / static_cast<float>(stepCount - 1);

    // Snap to the active grid FIRST, so a knob sitting between coarse detents
    // (put there by a Shift move) steps onto the grid rather than carrying the
    // offset along with it for ever.
    const float from = snap(getValueNormalized(), fine);
    const float target = snap(from + detents * stepSize, fine);

    beginEdit();
    setValueNormalized(target);
    if (isDirty())
    {
        invalid();
        valueChanged();
    }
    endEdit();
}

//------------------------------------------------------------------------
void SteppedKnob::onMouseWheelEvent(MouseWheelEvent& event)
{
    // One notch, one detent - a coarse one, or a fine one with Shift held.
    //
    // WHEEL UP = CLOCKWISE. Scrolling up raises the value, and the filmstrip
    // runs from minimum at -135 deg to maximum at +135 deg, so a rising value
    // turns the pointer clockwise, as every console-style plugin does.
    const double dy = event.deltaY;
    int detents = 0;
    if (dy > 0.0)      detents =  1;
    else if (dy < 0.0) detents = -1;

    if (detents != 0)
        nudge(detents, isFine(event.modifiers));

    event.consumed = true;
}

//------------------------------------------------------------------------
void SteppedKnob::onKeyboardEvent(KeyboardEvent& event)
{
    if (event.type != EventType::KeyDown)
        return;

    const bool fine = isFine(event.modifiers);

    switch (event.virt)
    {
        case VirtualKey::Up:
        case VirtualKey::Right:
            nudge(1, fine);
            event.consumed = true;
            break;
        case VirtualKey::Down:
        case VirtualKey::Left:
            nudge(-1, fine);
            event.consumed = true;
            break;
        default:
            break;
    }
}

//------------------------------------------------------------------------
CMouseEventResult SteppedKnob::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
    // CKnobBase::onMouseMoved writes the member `value` DIRECTLY rather than
    // going through setValue, so the snapping above never sees a drag. Let the
    // base class do its maths, then snap what it left behind.
    const CMouseEventResult result = CAnimKnob::onMouseMoved(where, buttons);
    if (result != kMouseEventHandled)
        return result;

    const float lo = getMin();
    const float hi = getMax();
    const float range = hi - lo;
    if (range <= 0.f)
        return result;

    const float snapped = lo + snap((getValue() - lo) / range, isFine(buttons)) * range;
    if (snapped != getValue())
    {
        // The base class already reported the unsnapped value, so report the
        // snapped one too: the host must not be left holding a value the knob
        // is not showing.
        CAnimKnob::setValue(snapped);
        if (isDirty())
        {
            invalid();
            valueChanged();
        }
    }
    return result;
}

//------------------------------------------------------------------------
} // namespace Yonie
