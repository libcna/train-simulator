#pragma once
#include <array>
#include <cmath>
#include <numbers>

namespace TrainSim::Render {
// Reduce in double precision before conversion: indefinitely running services
// must not lose sub-second wave motion as elapsed time grows.
inline std::array<float,3> waterWavePhases(double seconds)
{
    if (!std::isfinite(seconds)) seconds=0.0;
    constexpr double turn=2.0*std::numbers::pi;
    constexpr std::array rates{0.70,1.13,0.43};
    std::array<float,3> result{};
    for (std::size_t i=0;i<result.size();++i)
    {
        double phase=std::fmod(seconds,turn/rates[i])*rates[i];
        if (phase<0.0) phase+=turn;
        result[i]=static_cast<float>(phase);
    }
    return result;
}
}
