#pragma once

#include "trainsim/world/StreamingWorld.hpp"
#include <array>
#include <algorithm>
#include <cmath>

namespace TrainSim::World {

// Signed along coordinates point into the route at either terminal. The public
// apron occupies the authored continuation beyond the endpoint, not active streets.
inline constexpr float ForecourtBegin=-90,ForecourtEnd=-2;
inline constexpr float ForecourtInner=6.70f,ForecourtOuter=51.90f;
inline constexpr std::array<std::array<float,2>,6> ForecourtBeds{{
    {-70,22},{-40,22},{-10,22},{-70,41},{-40,41},{-10,41}}};

inline float forecourtHeightAboveGround(float across)
{
    return 0.78f-0.62f*std::clamp((across-ForecourtInner)/(ForecourtOuter-ForecourtInner),0.0f,1.0f);
}

inline bool forecourtBedAt(float along,float across)
{
    for (const auto& bed : ForecourtBeds)
        if (std::abs(along-bed[0])<5.0f && std::abs(across-bed[1])<2.4f) return true;
    return false;
}

inline bool overlapsTerminalForecourt(const WorldFeature& feature,const Sim::Route& route)
{
    float radius=0;
    switch (feature.kind)
    {
    case FeatureKind::GrassPatch: case FeatureKind::FieldCrop: case FeatureKind::HayBale: radius=3; break;
    case FeatureKind::Tree: radius=3.1f; break;
    case FeatureKind::Bush: radius=1.2f; break;
    case FeatureKind::House: radius=6.2f; break;
    case FeatureKind::Townhouse: radius=5.7f; break;
    case FeatureKind::Apartment: radius=7.8f; break;
    case FeatureKind::Tower: radius=10.5f; break;
    case FeatureKind::Warehouse: case FeatureKind::Farmstead: radius=16; break;
    default: return false;
    }
    radius*=static_cast<float>(feature.scale);
    for (bool last : {false,true})
    {
        const auto& station=last ? route.stations().back() : route.stations().front();
        const double along=(feature.routeDistance-station.distanceMetres)*(last ? -1 : 1);
        const double across=feature.lateralMetres*(station.platformOnLeft ? -1 : 1);
        if (along>ForecourtBegin-radius && along<ForecourtEnd+radius
            && across>ForecourtInner-radius && across<ForecourtOuter+radius) return true;
    }
    return false;
}
} // namespace TrainSim::World
