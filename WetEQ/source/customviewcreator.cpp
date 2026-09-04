//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "customviewcreator.h"
#include "vstgui/uidescription/uiviewcreator.h"
#include "vstgui/lib/cbitmap.h"

using namespace VSTGUI;

namespace Yonie {

static const std::string kAttrNumSegments = "num-segments";
static const std::string kAttrSegmentGap  = "segment-gap";
static const std::string kAttrHorizontal  = "horizontal";
static const std::string kAttrDbMin       = "db-min";
static const std::string kAttrDbMax       = "db-max";
static const std::string kAttrStepCount   = "step-count";
static const std::string kAttrCoarseStep  = "coarse-step";

//------------------------------------------------------------------------
// LEDMeterViewCreator
//------------------------------------------------------------------------
LEDMeterViewCreator::LEDMeterViewCreator()
{
    UIViewFactory::registerViewCreator(*this);
}

CView* LEDMeterViewCreator::create(const UIAttributes&, const IUIDescription*) const
{
    return new LEDMeterView(CRect(0, 0, 20, 200));
}

bool LEDMeterViewCreator::apply(CView* view, const UIAttributes& attributes,
                                const IUIDescription*) const
{
    auto* meter = dynamic_cast<LEDMeterView*>(view);
    if (!meter)
        return false;

    int32_t intValue;
    if (attributes.getIntegerAttribute(kAttrNumSegments, intValue))
        meter->setNumSegments(intValue);

    double doubleValue;
    if (attributes.getDoubleAttribute(kAttrSegmentGap, doubleValue))
        meter->setSegmentGap(static_cast<CCoord>(doubleValue));

    bool boolValue;
    if (attributes.getBooleanAttribute(kAttrHorizontal, boolValue))
        meter->setHorizontal(boolValue);

    double lo = 0.0, hi = 0.0;
    if (attributes.getDoubleAttribute(kAttrDbMin, lo) &&
        attributes.getDoubleAttribute(kAttrDbMax, hi))
        meter->setDbRange(lo, hi);

    return true;
}

bool LEDMeterViewCreator::getAttributeNames(StringList& names) const
{
    names.emplace_back(kAttrNumSegments);
    names.emplace_back(kAttrSegmentGap);
    names.emplace_back(kAttrHorizontal);
    names.emplace_back(kAttrDbMin);
    names.emplace_back(kAttrDbMax);
    return true;
}

IViewCreator::AttrType LEDMeterViewCreator::getAttributeType(const string& name) const
{
    if (name == kAttrNumSegments) return kIntegerType;
    if (name == kAttrSegmentGap)  return kFloatType;
    if (name == kAttrHorizontal)  return kBooleanType;
    if (name == kAttrDbMin)       return kFloatType;
    if (name == kAttrDbMax)       return kFloatType;
    return kUnknownType;
}

bool LEDMeterViewCreator::getAttributeValue(CView* view, const string& name,
                                            string& value, const IUIDescription*) const
{
    auto* meter = dynamic_cast<LEDMeterView*>(view);
    if (!meter)
        return false;
    if (name == kAttrNumSegments) { value = "12";   return true; }
    if (name == kAttrSegmentGap)  { value = "2";    return true; }
    if (name == kAttrHorizontal)  { value = "false"; return true; }
    return false;
}

//------------------------------------------------------------------------
// SteppedKnobCreator
//------------------------------------------------------------------------
SteppedKnobCreator::SteppedKnobCreator()
{
    UIViewFactory::registerViewCreator(*this);
}

CView* SteppedKnobCreator::create(const UIAttributes&, const IUIDescription*) const
{
    return new SteppedKnob(CRect(0, 0, 60, 60));
}

bool SteppedKnobCreator::apply(CView* view, const UIAttributes& attributes,
                               const IUIDescription* /*description*/) const
{
    auto* knob = dynamic_cast<SteppedKnob*>(view);
    if (!knob)
        return false;

    // No need to touch the bitmap or the standard control attributes here:
    // UIViewFactory walks getBaseViewName() and applies the CAnimKnob creator
    // for us before this returns.
    int32_t steps = 0;
    if (attributes.getIntegerAttribute(kAttrStepCount, steps) && steps > 1)
        knob->setStepCount(steps);

    // How many of those steps make one ordinary detent. Absent means 1, so a
    // knob without the attribute behaves exactly as it did before Shift
    // existed: every step reachable, no modifier needed.
    int32_t coarse = 0;
    if (attributes.getIntegerAttribute(kAttrCoarseStep, coarse) && coarse > 1)
        knob->setCoarseStep(coarse);

    return true;
}

bool SteppedKnobCreator::getAttributeNames(StringList& names) const
{
    names.emplace_back(kAttrStepCount);
    names.emplace_back(kAttrCoarseStep);
    return true;
}

IViewCreator::AttrType SteppedKnobCreator::getAttributeType(const string& name) const
{
    if (name == kAttrStepCount)  return kIntegerType;
    if (name == kAttrCoarseStep) return kIntegerType;
    return kUnknownType;
}

bool SteppedKnobCreator::getAttributeValue(CView* view, const string& name,
                                           string& value, const IUIDescription*) const
{
    auto* knob = dynamic_cast<SteppedKnob*>(view);
    if (!knob)
        return false;
    if (name == kAttrStepCount)
    {
        value = std::to_string(knob->getStepCount());
        return true;
    }
    if (name == kAttrCoarseStep)
    {
        value = std::to_string(knob->getCoarseStep());
        return true;
    }
    return false;
}

//------------------------------------------------------------------------
void registerCustomViews()
{
    // Static so each creator registers exactly once, however many editor
    // windows the host opens.
    static LEDMeterViewCreator ledMeterCreator;
    static SteppedKnobCreator steppedKnobCreator;
}

//------------------------------------------------------------------------
} // namespace Yonie
