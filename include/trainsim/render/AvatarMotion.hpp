#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace TrainSim::Render {

// Read-only compatibility filter for the inspected CNA catalog, not a description decoder.
// Unknown formats/catalogs are deliberately left alone. Stock skirts can stretch severely
// under the simulator's deeply bent seated pose; prefer trouser candidates until resolved.
[[nodiscard]] inline bool avatarHasSeatedSkirtRisk(std::span<const std::uint8_t> bytes) noexcept
{
    if (bytes.size() != 1021 || (bytes[0] != 1 && bytes[0] != 2)
        || bytes[1] != 'C' || bytes[2] != 'N' || bytes[3] != 'A'
        || bytes[8] != 3 || bytes[9] != 0) return false;
    const unsigned bottom = bytes[35] | (static_cast<unsigned>(bytes[36]) << 8U);
    return bottom == 42 || bottom == 45;
}

// CNA's public avatar forward vector is -Z. Share this conversion between locomotion
// and attention targets so a person does not walk or look away from the destination.
[[nodiscard]] inline float avatarFacingDirection(double deltaX, double deltaZ) noexcept
{
    if (std::hypot(deltaX,deltaZ) < 1e-8) return 0.0f;
    return static_cast<float>(std::atan2(-deltaX,-deltaZ));
}

[[nodiscard]] constexpr float avatarSeatFacing(int row) noexcept
{
    // CNA exposes XNA's -Z avatar front, matching the even-row seat cushion direction.
    return row%2 == 0 ? 0.0f : 3.1415927f;
}

// Authored platform slots 0/1 are staff/track watchers, 2/3 include a phone user.
// Only form the final conversation pair when both people are actually loaded.
[[nodiscard]] constexpr bool platformConversationSlot(
    std::size_t slot, std::size_t firstSlot, std::size_t count) noexcept
{
    const std::size_t partner = slot ^ 1U;
    return slot >= 4U && slot < 6U && slot >= firstSlot && slot-firstSlot < count
        && partner >= firstSlot && partner-firstSlot < count;
}

[[nodiscard]] inline float avatarWindowGaze(double lateral, float seatedFacing) noexcept
{
    const float windowFacing = lateral < 0.0 ? 1.5707963f : -1.5707963f;
    return std::clamp(std::atan2(std::sin(windowFacing-seatedFacing),
                                 std::cos(windowFacing-seatedFacing)),-0.85f,0.85f);
}

struct AvatarGaitStep
{
    float phaseRadians = 0.0f;
    float distanceMetres = 0.0f;
    bool moving = false;
};

struct AvatarSeatExitPlacement
{
    double x = 0.0;
    double z = 0.0;
    float weightTransfer = 0.0f;
};

[[nodiscard]] constexpr float avatarSeatTransferWeight(
    float seatedPoseBlend, bool seatTransition) noexcept
{
    if (!seatTransition) return 0.0f;
    const float seated = std::clamp(seatedPoseBlend,0.0f,1.0f);
    return 4.0f*seated*(1.0f-seated);
}

// Passenger navigation becomes authoritative as soon as an NPC decides to stand. Keep the
// rendered pelvis close to the vacated seat while the procedural skeleton is still mostly seated,
// then converge on navigation before the standing pose completes. This avoids an apparent seated
// sideways slide without delaying collision or seat-occupancy state.
[[nodiscard]] constexpr AvatarSeatExitPlacement stabilizeAvatarSeatExit(
    double navigationX, double navigationZ, double seatX, double seatZ,
    float seatedPoseBlend, bool exitingSeat) noexcept
{
    if (!exitingSeat)
        return {navigationX,navigationZ,0.0f};

    const float anchor = std::clamp(seatedPoseBlend,0.0f,1.0f);
    const double navigationWeight = static_cast<double>(1.0f-anchor);
    const double seatWeight = static_cast<double>(anchor);
    return {
        navigationX*navigationWeight + seatX*seatWeight,
        navigationZ*navigationWeight + seatZ*seatWeight,
        avatarSeatTransferWeight(anchor,true)
    };
}

[[nodiscard]] constexpr float seededAvatarGaitPhase(std::uint32_t appearanceSeed) noexcept
{
    constexpr float twoPi = 6.2831853071795864769f;
    return static_cast<float>(appearanceSeed%997U)*(twoPi/997.0f);
}

[[nodiscard]] inline AvatarGaitStep advanceAvatarGait(
    float phaseRadians, double previousX, double previousZ,
    double currentX, double currentZ, bool positionInitialized, bool walking) noexcept
{
    constexpr float twoPi = 6.2831853071795864769f;
    constexpr float radiansPerMetre = 7.35f;
    constexpr double minimumVisibleStep = 0.0005;
    constexpr double maximumContinuousStep = 0.40;

    if (!positionInitialized || !walking)
        return {phaseRadians,0.0f,false};

    const double distance = std::hypot(currentX-previousX,currentZ-previousZ);
    if (distance < minimumVisibleStep || distance > maximumContinuousStep)
        return {phaseRadians,static_cast<float>(distance),false};

    float advanced = std::fmod(
        phaseRadians+static_cast<float>(distance)*radiansPerMetre,twoPi);
    if (advanced < 0.0f) advanced += twoPi;
    return {advanced,static_cast<float>(distance),true};
}

} // namespace TrainSim::Render
