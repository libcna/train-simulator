#pragma once

namespace TrainSim::App {

struct MouseLookDelta
{
    int x = 0;
    int y = 0;
    bool operator==(const MouseLookDelta&) const = default;
};

enum class MouseCaptureAction
{
    None,
    Acquire,
    ReleaseRequested,
    ReleaseUnexpected
};

// Keep project capture synchronized with CNA's native relative mode and desktop focus. Some
// window managers leave the native flag enabled after motion delivery has stopped, so an inactive
// window must always tear the stream down explicitly before a later click reacquires it.
[[nodiscard]] constexpr MouseCaptureAction mouseCaptureAction(
    bool captured, bool relativeMode, bool windowActive,
    bool captureAllowed, bool primaryButtonPressed) noexcept
{
    if (!windowActive)
        return captured || relativeMode
            ? MouseCaptureAction::ReleaseUnexpected : MouseCaptureAction::None;
    if (!captureAllowed)
        return captured || relativeMode
            ? MouseCaptureAction::ReleaseRequested : MouseCaptureAction::None;
    if (captured != relativeMode) return MouseCaptureAction::ReleaseUnexpected;
    if (!captured && primaryButtonPressed) return MouseCaptureAction::Acquire;
    return MouseCaptureAction::None;
}

// CNA reports accumulated motion directly in MouseState.X/Y while relative mode is active. In
// absolute mode those fields are positions and need the traditional previous-sample subtraction.
// Keeping the distinction explicit prevents a steady stream of equal relative deltas from being
// mistaken for zero motion.
[[nodiscard]] constexpr MouseLookDelta decodeMouseLookDelta(
    int reportedX, int reportedY, int previousX, int previousY, bool relativeMode) noexcept
{
    if (relativeMode) return {reportedX,reportedY};
    return {reportedX-previousX,reportedY-previousY};
}

} // namespace TrainSim::App
