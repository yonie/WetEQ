//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/cview.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/cdrawcontext.h"

namespace Yonie {

//------------------------------------------------------------------------
// LEDMeterView - Custom 80's style LED segment meter
// Displays 12 discrete LED segments: 8 green, 2 yellow, 2 red
//------------------------------------------------------------------------
class LEDMeterView : public VSTGUI::CControl
{
public:
    LEDMeterView(const VSTGUI::CRect& size);
    ~LEDMeterView() override = default;

    // CView overrides
    void draw(VSTGUI::CDrawContext* context) override;
    
    // Configuration
    void setNumSegments(int num) { numSegments = num; }
    void setSegmentGap(VSTGUI::CCoord gap) { segmentGap = gap; }
    void setHorizontal(bool horiz) { isHorizontal = horiz; }

    // Map the incoming value as dB rather than linearly across the segments.
    //
    // Linear is what this did, and it makes the meter read far lower than the
    // signal actually is: half the strip covers -6 dBFS to 0, so -20 dBFS lights
    // ONE segment of twelve and anything below -24 lights none at all. A player
    // reads that as "no signal" when the track is perfectly healthy.
    //
    // Off by default, so a meter that has not been given a range keeps the old
    // behaviour.
    void setDbRange(double lo, double hi) { dbLo = lo; dbHi = hi; dbScale = hi > lo; }
    bool hasDbRange() const { return dbScale; }
    double dbMin() const { return dbLo; }
    double dbMax() const { return dbHi; }

    // Number of segments lit for the current value.
    int litSegments() const;
    
    // Custom view class name for VSTGUI factory
    CLASS_METHODS(LEDMeterView, CControl)

protected:
    VSTGUI::CRect calculateSegmentRect(int segmentIndex) const;
    VSTGUI::CColor getLitColor(int segmentIndex) const;
    VSTGUI::CColor getDarkColor(int segmentIndex) const;
    
    int numSegments = 12;
    VSTGUI::CCoord segmentGap = 2;
    bool isHorizontal = true;
    bool dbScale = false;
    double dbLo = -48.0, dbHi = 18.0;
    
    // Color definitions for 80's LED look
    // Green zone (segments 0-7)
    // The panel art paints every LED lit, so the meter must black its own
    // window out before drawing. Without this the painted LEDs show through
    // the gaps between segments as bright slivers and the meter reads as
    // permanently pinned.
    VSTGUI::CColor windowBlack{0, 0, 0, 255};

    // A lit LED spills light onto the black around it. Drawn as a few
    // translucent rings stepping outwards from each lit segment, brightest
    // against the segment. Only lit ones glow - an unlit LED emits nothing.
    int glowSpread = 3;         // px the spill reaches
    uint8_t glowAlpha = 42;     // alpha of the innermost ring

    VSTGUI::CColor greenLit{0, 255, 0, 255};
    VSTGUI::CColor greenDark{10, 42, 10, 255};
    
    // Yellow zone (segments 8-9)
    VSTGUI::CColor yellowLit{255, 255, 0, 255};
    VSTGUI::CColor yellowDark{42, 42, 10, 255};
    
    // Red zone (segments 10-11)
    VSTGUI::CColor redLit{255, 0, 0, 255};
    VSTGUI::CColor redDark{42, 10, 10, 255};
};

//------------------------------------------------------------------------
} // namespace Yonie