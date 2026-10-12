#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace TrainSim::Render {

inline constexpr float marketTowerClockHeight=8.25f;
inline constexpr float marketTowerClockFaceDepth=2.28f;
[[nodiscard]] constexpr bool hasMarketTownTurret(std::size_t stationIndex) noexcept
{
    return stationIndex==1 || stationIndex==6;
}

[[nodiscard]] constexpr double stationNameBoardLateral(double offset) noexcept
{
    // The centre board stays in the seated window's viewing cone, ahead of the
    // station facade. End boards can use the unobstructed rear platform edge.
    return std::abs(offset)<=10.0 ? 4.85 : 6.15;
}
inline constexpr double stationNameBoardHalfWidth=1.40;
[[nodiscard]] constexpr std::array<double,3> stationNameBoardOffsets(bool major)
{
    const double reach=(major ? 240.0 : 180.0)*0.36;
    return {-reach,-8.5,reach};
}

[[nodiscard]] constexpr float platformCanopyLength(bool terminal, bool major) noexcept
{
    return terminal ? 132.0f : major ? 86.0f : 44.0f;
}

inline constexpr std::array<double,6> platformWaitingLaterals{4.75,4.97,4.59,4.93,4.65,4.99};

// Platform amenities along the rear zone (ENV-136), station-local offsets and lateral.
inline constexpr double kPlatformAmenityLateral=5.55;
inline constexpr double kPlatformHelpPointOffset=3.2;
inline constexpr std::array<double,2> kPlatformValidatorOffsets{-5.2,10.6};
// Clear of the departure board (about +14 m), which it hid from the train at 12.6 m.
inline constexpr double kPlatformVendingOffset=25.4;
inline constexpr double kPlatformCycleStandOffset=-30.5;
inline constexpr double kPlatformTrolleyOffset=-20.8;
inline constexpr double kPlatformBenchLateral=4.15;

inline constexpr double canopySupportSpacing=6.0;
[[nodiscard]] inline std::array<std::array<double,2>,2> platformFenceRuns(double length,double access)
{
    const double begin=-length*0.5+3.0,end=length*0.5-3.0;
    return {{{begin,std::clamp(access-5.2,begin,end)},
             {std::clamp(access+5.2,begin,end),end}}};
}
inline constexpr double platformIdentifierOffset=20.8;
// Keep the entire live board below the canopy beam, with grounded supports.
inline constexpr float platformDisplayCentreHeight=2.50f;
inline constexpr float platformDisplayHalfHeight=0.54f;
inline constexpr float platformDisplayPostTop=2.10f;
inline constexpr float platformDisplayFirstRow=2.94f;
[[nodiscard]] inline double platformDisplayOffset(bool terminal,bool major)
{
    const double begin=-static_cast<double>(platformCanopyLength(terminal,major))*0.42;
    return begin+(std::floor((12.8-begin)/canopySupportSpacing)+0.5)*canopySupportSpacing;
}
inline double canopySupportOffset(double nominal,bool terminal,bool major)
{
    const double buildingLength=terminal ? 58.0 : major ? 42.0 : 16.0;
    const double buildingCentre=terminal ? 42.0 : major ? 28.0 : 12.0;
    for (double orientation : {-1.0,1.0})
    {
        if (!terminal && orientation>0) continue;
        for (double variant : {-1.0,1.0})
        {
            const double door=orientation*buildingCentre+variant*buildingLength*0.07;
            if (std::abs(nominal-door)<1.10)
                nominal=door+std::copysign(1.10,nominal-door);
        }
    }
    // The central name board shares this lateral line: do not grow a new column
    // through its readable face when adding the denser structural bay grid.
    constexpr double centreName=-8.5;
    if (std::abs(nominal-centreName)<1.70)
        nominal=centreName+std::copysign(1.70,nominal-centreName);
    return nominal;
}

inline double canopyLightOffset(float length,int light,int count)
{
    const double nominal=((static_cast<double>(light)+0.5)/count-0.5)*length*0.80;
    const double begin=-static_cast<double>(length)*0.42;
    return begin+(std::floor((nominal-begin)/canopySupportSpacing)+0.5)*canopySupportSpacing;
}

// One footprint catalogue for body clearance and the space between conversation partners.
template<class Overlap>
[[nodiscard]] inline bool platformFixturesClear(bool terminal, bool major, Overlap overlaps)
{
    const double length = platformCanopyLength(terminal,major);
    for (double board : stationNameBoardOffsets(major))
        for (double post : {-1.12,1.12})
            if (overlaps(board+post,stationNameBoardLateral(board),0.045,0.045)) return false;
    for (double post = -length*0.42; post <= length*0.42; post += canopySupportSpacing)
        if (overlaps(canopySupportOffset(post,terminal,major),4.8,0.06,0.06)) return false;
    for (double post : {-1.18,1.18})
        if (overlaps(platformDisplayOffset(terminal,major)+post,6.20,0.06,0.06)) return false;
    if (overlaps(-13.0,4.72,0.29,0.30)) return false;
    for (double lamp : {-60.0,-30.0,0.0,30.0,60.0})
        if ((major || std::abs(lamp) <= 31.0) && overlaps(lamp,4.9,0.05,0.05))
            return false;
    const std::array<double,3> benches = major
        ? std::array<double,3>{-38.0,0.0,38.0} : std::array<double,3>{-24.0,24.0,0.0};
    for (int i = 0; i < (major ? 3 : 2); ++i)
        if (overlaps(benches[i],4.15,1.2,0.35)) return false;
    // ENV-136 amenities: help point, validators, vending machine, cycle stands, trolley.
    if (overlaps(kPlatformHelpPointOffset,kPlatformAmenityLateral,0.15,0.10)) return false;
    for (double validator : kPlatformValidatorOffsets)
        if (overlaps(validator,kPlatformAmenityLateral,0.12,0.08)) return false;
    if (major && overlaps(kPlatformVendingOffset,kPlatformAmenityLateral+0.15,0.45,0.38)) return false;
    if (!major && overlaps(kPlatformCycleStandOffset,kPlatformAmenityLateral+0.15,1.05,0.35)) return false;
    if ((major || terminal) && overlaps(kPlatformTrolleyOffset,kPlatformAmenityLateral,0.70,0.31))
        return false;
    return !overlaps(7.0,4.5,0.31,0.325);
}

// Station-local coordinates, with positive lateral measured away from the track on either side.
// Reserve a 0.38 m body envelope plus a small personal-space margin around solid fixtures.
[[nodiscard]] inline bool platformWaitingClear(double z, double x, bool terminal, bool major)
{
    return platformFixturesClear(terminal,major,[=](double fz, double fx, double hz, double hx) {
        return std::abs(z-fz) < hz+0.48 && std::abs(x-fx) < hx+0.48;
    });
}

[[nodiscard]] inline bool platformConversationClear(
    double az, double ax, double bz, double bx, bool terminal, bool major)
{
    // Exact segment/expanded-rectangle clipping, not samples which can skip a narrow post.
    return platformFixturesClear(terminal,major,[=](double z, double x, double hz, double hx) {
        double enter = 0.0;
        double leave = 1.0;
        const auto clip = [&](double start, double delta, double centre, double half) {
            half += 0.15;
            if (std::abs(delta) < 1e-10) return std::abs(start-centre) <= half;
            double low = (centre-half-start)/delta;
            double high = (centre+half-start)/delta;
            if (low > high) std::swap(low,high);
            enter = std::max(enter,low);
            leave = std::min(leave,high);
            return enter <= leave;
        };
        return clip(az,bz-az,z,hz) && clip(ax,bx-ax,x,hx);
    });
}

[[nodiscard]] inline std::array<double,6> platformWaitingOffsets(bool terminal, bool major)
{
    std::array<double,6> result{-47.0,-42.5,8.5,14.0,33.0,34.6};
    // Resolve full authored groups before quality selection: switching quality cannot move people.
    // The last two slots move together, preserving their conversation distance and heading.
    for (std::size_t slot = 0; slot < result.size(); ++slot)
    {
        const std::size_t end = slot == 4 ? 6 : slot+1;
        for (int attempt = 0; attempt <= 24; ++attempt)
        {
            const double shift = attempt == 0 ? 0.0
                : (attempt%2 == 1 ? 1.0 : -1.0)*((attempt+1)/2)*0.5;
            bool clear = true;
            for (std::size_t member = slot; member < end; ++member)
                clear = clear && platformWaitingClear(
                    result[member]+shift,platformWaitingLaterals[member],terminal,major);
            if (slot == 4)
                clear = clear && platformConversationClear(
                    result[4]+shift,platformWaitingLaterals[4],
                    result[5]+shift,platformWaitingLaterals[5],terminal,major);
            if (!clear) continue;
            for (std::size_t member = slot; member < end; ++member) result[member] += shift;
            break;
        }
        slot = end-1;
    }
    return result;
}

// Extra waiting figures (ENV-136), in drawing priority order: Medium shows the first two.
enum class PlatformExtraPose : unsigned char { BenchSeated, WithSuitcase, ReadingTimetable };
struct PlatformExtraFigure
{
    double offset=0, lateral=0;
    PlatformExtraPose pose=PlatformExtraPose::BenchSeated;
};
inline constexpr double kPlatformSeatedRootLateral=kPlatformBenchLateral-0.10;

[[nodiscard]] inline std::array<PlatformExtraFigure,6> platformExtraFigures(bool terminal,bool major)
{
    const double benchA=major ? -38.0 : -24.0, benchB=major ? 0.0 : 24.0;
    std::array<PlatformExtraFigure,6> figures{{
        {benchA-0.55,kPlatformSeatedRootLateral,PlatformExtraPose::BenchSeated},
        {-15.6,4.45,PlatformExtraPose::WithSuitcase},
        {benchB+0.50,kPlatformSeatedRootLateral,PlatformExtraPose::BenchSeated},
        {19.8,4.55,PlatformExtraPose::WithSuitcase},
        {benchA+0.60,kPlatformSeatedRootLateral,PlatformExtraPose::BenchSeated},
        {18.3,5.02,PlatformExtraPose::ReadingTimetable}}};
    // Standing figures keep personal space around fixtures, as the original waiting slots do.
    for (auto& figure : figures)
    {
        if (figure.pose==PlatformExtraPose::BenchSeated) continue;
        for (int attempt=0; attempt<=24; ++attempt)
        {
            const double shift=attempt==0 ? 0.0
                : (attempt%2==1 ? 1.0 : -1.0)*((attempt+1)/2)*0.5;
            if (!platformWaitingClear(figure.offset+shift,figure.lateral,terminal,major)) continue;
            figure.offset+=shift;
            break;
        }
    }
    return figures;
}

} // namespace TrainSim::Render

