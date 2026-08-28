//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "ledmeterview.h"

using namespace VSTGUI;

namespace Yonie {

//------------------------------------------------------------------------
// LEDMeterView Implementation
//------------------------------------------------------------------------
LEDMeterView::LEDMeterView(const CRect& size)
    : CControl(size, nullptr, -1)
{
    setWantsFocus(false);
}

//------------------------------------------------------------------------
void LEDMeterView::draw(CDrawContext* context)
{
    // Get normalized value (0.0 to 1.0)
    float level = getValueNormalized();
    
    // Calculate how many segments should be lit
    int litCount = static_cast<int>(level * numSegments + 0.5f);
    if (litCount > numSegments) litCount = numSegments;
    if (litCount < 0) litCount = 0;
    
    // Black the whole window out first. The backplate has all 12 LEDs painted
    // lit, and the drawn segments do not line up with the painted ones, so
    // anything left uncovered shows through as a bright sliver.
    context->setFillColor(windowBlack);
    context->drawRect(getViewSize(), kDrawFilled);

    // Glow first, so the segments themselves land on top of it and stay crisp.
    // Rings step outwards from each lit segment with the alpha falling off, and
    // they are drawn for every lit LED before any segment is filled - otherwise
    // a neighbour's glow would paint over the LED below it.
    for (int i = 0; i < litCount; i++)
    {
        CRect seg = calculateSegmentRect(i);
        CColor glow = getLitColor(i);
        for (int s = glowSpread; s >= 1; s--)
        {
            CRect halo = seg;
            halo.extend(static_cast<CCoord>(s), static_cast<CCoord>(s));
            glow.alpha = static_cast<uint8_t>(glowAlpha * (glowSpread - s + 1)
                                              / (glowSpread + 1));
            context->setFillColor(glow);
            context->drawRect(halo, kDrawFilled);
        }
    }

    // Draw each segment
    for (int i = 0; i < numSegments; i++)
    {
        CRect segRect = calculateSegmentRect(i);
        CColor color = (i < litCount) ? getLitColor(i) : getDarkColor(i);
        
        context->setFillColor(color);
        context->drawRect(segRect, kDrawFilled);
    }
    
    setDirty(false);
}

//------------------------------------------------------------------------
CRect LEDMeterView::calculateSegmentRect(int segmentIndex) const
{
    CRect viewSize = getViewSize();
    CCoord totalWidth = viewSize.getWidth();
    CCoord totalHeight = viewSize.getHeight();
    
    if (isHorizontal)
    {
        // Horizontal meter: segments arranged left to right
        CCoord segmentWidth = (totalWidth - (numSegments - 1) * segmentGap) / numSegments;
        CCoord x = viewSize.left + segmentIndex * (segmentWidth + segmentGap);
        
        return CRect(x, viewSize.top, x + segmentWidth, viewSize.bottom);
    }
    else
    {
        // Vertical meter: segments arranged bottom to top
        CCoord segmentHeight = (totalHeight - (numSegments - 1) * segmentGap) / numSegments;
        CCoord y = viewSize.bottom - (segmentIndex + 1) * segmentHeight - segmentIndex * segmentGap;
        
        return CRect(viewSize.left, y, viewSize.right, y + segmentHeight);
    }
}

//------------------------------------------------------------------------
CColor LEDMeterView::getLitColor(int segmentIndex) const
{
    // 12 segments: 0-7 green, 8-9 yellow, 10-11 red
    // Scale for different segment counts
    float position = static_cast<float>(segmentIndex) / (numSegments - 1);
    
    if (position < 0.67f)  // First ~67% (green zone)
    {
        return greenLit;
    }
    else if (position < 0.83f)  // Next ~16% (yellow zone)
    {
        return yellowLit;
    }
    else  // Last ~17% (red zone)
    {
        return redLit;
    }
}

//------------------------------------------------------------------------
CColor LEDMeterView::getDarkColor(int segmentIndex) const
{
    // Same zones as lit colors, but darker
    float position = static_cast<float>(segmentIndex) / (numSegments - 1);
    
    if (position < 0.67f)
    {
        return greenDark;
    }
    else if (position < 0.83f)
    {
        return yellowDark;
    }
    else
    {
        return redDark;
    }
}

//------------------------------------------------------------------------
} // namespace Yonie