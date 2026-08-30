//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "steppedknob.h"

#include <cmath>

using namespace VSTGUI;

namespace Yonie {

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
    // The strip has one frame per detent, so this keeps frame == step.
    setInverseBitmap(false);
}

//------------------------------------------------------------------------
float SteppedKnob::snap(float normalized) const
{
    const float steps = static_cast<float>(stepCount - 1);
    if (normalized < 0.f) normalized = 0.f;
    if (normalized > 1.f) normalized = 1.f;
    return std::floor(normalized * steps + 0.5f) / steps;
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
    // Snap in normalised space, then convert back: getMin/getMax are not
    // guaranteed to be 0..1.
    const float norm = snap((val - lo) / range);
    CAnimKnob::setValue(lo + norm * range);
}

//------------------------------------------------------------------------
void SteppedKnob::nudge(int detents)
{
    if (detents == 0)
        return;

    const float stepSize = 1.0f / static_cast<float>(stepCount - 1);
    const float target = snap(getValueNormalized() + detents * stepSize);

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
    // One notch, one detent. Deliberately does NOT honour the zoom modifier:
    // there is nothing finer than a detent to zoom into.
    //
    // WHEEL UP = CLOCKWISE. Scrolling up raises the value, and the filmstrip
    // runs from minimum at -135 deg to maximum at +135 deg, so a rising value
    // turns the pointer clockwise, as every console-style plugin does.
    const double dy = event.deltaY;
    int detents = 0;
    if (dy > 0.0)      detents =  1;
    else if (dy < 0.0) detents = -1;

    if (detents != 0)
        nudge(detents);

    event.consumed = true;
}

//------------------------------------------------------------------------
void SteppedKnob::onKeyboardEvent(KeyboardEvent& event)
{
    if (event.type != EventType::KeyDown)
        return;

    switch (event.virt)
    {
        case VirtualKey::Up:
        case VirtualKey::Right:
            nudge(1);
            event.consumed = true;
            break;
        case VirtualKey::Down:
        case VirtualKey::Left:
            nudge(-1);
            event.consumed = true;
            break;
        default:
            break;
    }
}

//------------------------------------------------------------------------
CMouseEventResult SteppedKnob::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
    // Let the base class do the drag maths, then snap. setValue() above rounds
    // to a detent, so the knob steps through positions while dragging instead
    // of sliding smoothly and landing between them.
    return CAnimKnob::onMouseMoved(where, buttons);
}

//------------------------------------------------------------------------
} // namespace Yonie
