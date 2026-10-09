#pragma once

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

} // namespace TrainSim::Render
