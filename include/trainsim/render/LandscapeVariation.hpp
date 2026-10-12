#pragma once

#include <array>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <utility>

namespace TrainSim::Render {

struct TerminalTreePlacement
{
    float outward, lateral, scale;
    unsigned variant;
};

inline TerminalTreePlacement terminalVergeTree(int index,unsigned seed)
{
    const float phase=static_cast<float>(seed%97U)*0.71f+static_cast<float>(index)*2.37f;
    return {24.0f+static_cast<float>(index/2)*55.0f+static_cast<float>(index%2)*8.0f
                +3.0f*std::sin(phase),
            46.0f+0.8f*std::sin(phase*1.31f),1.05f+0.15f*std::sin(phase*0.83f),
            seed+static_cast<unsigned>(index)*17U};
}

struct LakeBankTreePlacement
{
    double distance;
    double lateral;
    float scale;
    unsigned variant;
};

inline LakeBankTreePlacement lakeBankTreePlacement(std::int64_t stripe,int slot)
{
    const auto hash=[](std::uint64_t value) {
        value=(value^(value>>30))*0xbf58476d1ce4e5b9ULL;
        value=(value^(value>>27))*0x94d049bb133111ebULL;
        return value^(value>>31);
    };
    const auto group=static_cast<std::uint64_t>(stripe/2);
    const auto sample=hash(group*31U+static_cast<unsigned>((stripe%2)*4+slot)+107U);
    const auto unit=[](std::uint64_t value) { return static_cast<double>(value&65535U)/65535.0; };
    const double centre=static_cast<double>(stripe/2)*100+30+unit(hash(group+93U))*40;
    const double ordinal=static_cast<double>((stripe%2)*4+slot)-3.5;
    return {centre+ordinal*5+(unit(sample)-0.5)*3,
            550+unit(hash(group+713U))*75+static_cast<double>(slot%3)*13
                +(unit(sample>>16)-0.5)*10,
            1.45f+static_cast<float>(unit(sample>>32))*0.95f,
            static_cast<unsigned>(sample%3U)};
}

inline double lakeFarBankRelief(double distance,double waterward)
{
    const double t=std::clamp((waterward-455)/180,0.0,1.0);
    const double endFade=std::clamp(std::min(distance-80500,87200-distance)/400,0.0,1.0);
    const double profile=20+12*std::sin(distance/240)+6*std::sin(distance/91+0.6);
    return t*t*(3-2*t)*endFade*endFade*(3-2*endFade)*profile;
}

inline double lakeShoreTerrainOffset(double natural,double waterward,double waterHeight)
{
    if (waterward<=12) return natural;
    const auto smooth=[](double value) {
        const double t=std::clamp(value,0.0,1.0);
        return t*t*(3-2*t);
    };
    const double inner=smooth((waterward-26)/84);
    const double far=smooth((waterward-455)/135);
    const double basin=waterHeight-2.4;
    const double bank=basin+(natural-basin)*far;
    // Keep the railway-side verge below passenger eye height; an incidental terrain ridge
    // here hides the entire authored lake even though the water mesh itself is present.
    const double approach=waterward<110 ? std::min(natural,-0.34) : natural;
    return approach+(bank-approach)*inner;
}

[[nodiscard]] constexpr std::pair<double,double> terminalTerrainRange(
    double begin,double end,double routeLength) noexcept
{
    return {begin <= 0.0 ? -1200.0 : begin,
            end >= routeLength ? routeLength+1200.0 : end};
}

[[nodiscard]] constexpr std::pair<double,double> streamedBackdropRange(
    double begin,double end,double routeLength,bool first,bool last) noexcept
{
    return {first ? std::max(0.0,begin-500.0) : begin,
            last ? std::min(routeLength,end+500.0) : end};
}

[[nodiscard]] constexpr int ridgeVariant(std::int64_t chunk, int side, int layer) noexcept
{
    std::uint64_t value = static_cast<std::uint64_t>(chunk)
        ^ (static_cast<std::uint64_t>(side+1)*0x9e3779b97f4a7c15ULL)
        ^ (static_cast<std::uint64_t>(layer+1)*0xbf58476d1ce4e5b9ULL);
    value = (value^(value>>30))*0xbf58476d1ce4e5b9ULL;
    value = (value^(value>>27))*0x94d049bb133111ebULL;
    return static_cast<int>((value^(value>>31))%5U);
}

// Tramlines (ENV-130): paired tractor wheelings every 24 m across an arable parcel, parallel to
// the railway. Lateral centres of the pairs on one side, inside both parcel bands.
inline constexpr std::array<double,9> kTramlineLaterals={38,62,86,114,138,162,186,210,234};
inline constexpr double kTramlineHalfGauge=0.9;   // Wheel-track centres either side of a line.
inline constexpr double kTramlineWheelWidth=0.45;
inline constexpr double kTramlineHeadland=6.0;    // Kept clear of each parcel boundary.

// Field boundary at one parcel edge (ENV-128). Hedges, gated hedges, tree lines and open
// grass margins alternate per side, and each boundary reaches its own outer extent.
enum class FieldBoundaryKind { Hedge, GatedHedge, TreeLine, Open };

struct FieldBoundaryLayout
{
    FieldBoundaryKind kind=FieldBoundaryKind::Hedge;
    float outerLateral=96;   // Hedge runs from 12 m to this distance from the line.
    float gateLateral=0;     // Centre of a 5 m field gateway for GatedHedge.
    float height=0.95f;
    std::uint32_t seed=0;
};

[[nodiscard]] inline FieldBoundaryLayout fieldBoundaryLayout(std::int64_t parcel,int side) noexcept
{
    std::uint64_t value=static_cast<std::uint64_t>(parcel)*0x9e3779b97f4a7c15ULL
        ^(side<0 ? 0x3c6ef372fe94f82bULL : 0xa54ff53a5f1d36f1ULL);
    value=(value^(value>>30))*0xbf58476d1ce4e5b9ULL;
    value=(value^(value>>27))*0x94d049bb133111ebULL;
    value^=value>>31;
    constexpr FieldBoundaryKind kinds[10]={
        FieldBoundaryKind::Hedge,FieldBoundaryKind::Hedge,FieldBoundaryKind::Hedge,
        FieldBoundaryKind::Hedge,FieldBoundaryKind::GatedHedge,FieldBoundaryKind::GatedHedge,
        FieldBoundaryKind::TreeLine,FieldBoundaryKind::TreeLine,FieldBoundaryKind::Open,
        FieldBoundaryKind::Open};
    constexpr float extents[4]={96,120,150,180};
    FieldBoundaryLayout layout;
    layout.kind=kinds[value%10U];
    layout.outerLateral=extents[(value>>8)%4U];
    layout.gateLateral=24.0f+static_cast<float>((value>>16)%1024U)/1023.0f
        *(layout.outerLateral-48.0f);
    layout.height=0.80f+static_cast<float>((value>>28)%7U)*0.09f;
    layout.seed=static_cast<std::uint32_t>(value>>32);
    return layout;
}

} // namespace TrainSim::Render
