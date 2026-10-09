#include "TestHarness.hpp"

#include "trainsim/app/MouseLook.hpp"

using TrainSim::App::MouseLookDelta;
using TrainSim::App::MouseCaptureAction;
using TrainSim::App::decodeMouseLookDelta;
using TrainSim::App::mouseCaptureAction;

TEST_CASE("relative mouse samples are already deltas and remain continuous")
{
    MouseLookDelta previous{6,-2};
    int accumulatedX = 0;
    int accumulatedY = 0;
    for (int frame = 0; frame < 240; ++frame)
    {
        const MouseLookDelta decoded = decodeMouseLookDelta(
            6,-2,previous.x,previous.y,true);
        REQUIRE((decoded == MouseLookDelta{6,-2}));
        accumulatedX += decoded.x;
        accumulatedY += decoded.y;
        previous = {6,-2};
    }
    REQUIRE(accumulatedX == 1'440);
    REQUIRE(accumulatedY == -480);
}

TEST_CASE("absolute mouse samples still use position differences")
{
    REQUIRE((decodeMouseLookDelta(107,92,100,100,false) == MouseLookDelta{7,-8}));
    REQUIRE((decodeMouseLookDelta(107,92,107,92,false) == MouseLookDelta{0,0}));
}

TEST_CASE("mouse capture policy tears down stale relative streams on focus loss")
{
    REQUIRE(mouseCaptureAction(true,true,false,true,false)
            == MouseCaptureAction::ReleaseUnexpected);
    REQUIRE(mouseCaptureAction(false,true,true,true,false)
            == MouseCaptureAction::ReleaseUnexpected);
    REQUIRE(mouseCaptureAction(true,false,true,true,false)
            == MouseCaptureAction::ReleaseUnexpected);
    REQUIRE(mouseCaptureAction(false,false,false,true,true)
            == MouseCaptureAction::None);
}

TEST_CASE("mouse capture policy distinguishes overlays from real mode loss")
{
    REQUIRE(mouseCaptureAction(true,true,true,false,false)
            == MouseCaptureAction::ReleaseRequested);
    REQUIRE(mouseCaptureAction(false,false,true,true,true)
            == MouseCaptureAction::Acquire);
    REQUIRE(mouseCaptureAction(true,true,true,true,false)
            == MouseCaptureAction::None);
}
