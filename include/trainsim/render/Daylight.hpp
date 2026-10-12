#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"
#include <cmath>
#include <algorithm>

namespace TrainSim::Render {

// Fixed sunny daylight in absolute route coordinates, expressed in a local route frame.
// This is the direction rays travel, so the visible sun lies in the opposite direction.
inline Microsoft::Xna::Framework::Vector3 daylightDirection(double heading)
{
    const double c = std::cos(heading), s = std::sin(heading);
    return Microsoft::Xna::Framework::Vector3::Normalize({
        static_cast<float>(0.42*c-0.34*s),-0.84f,
        static_cast<float>(0.42*s+0.34*c)});
}

struct AvatarCabinLighting
{
    Microsoft::Xna::Framework::Vector3 direction;
    Microsoft::Xna::Framework::Vector3 diffuse;
};

struct ExteriorLighting
{
    Microsoft::Xna::Framework::Vector3 ambient;
    Microsoft::Xna::Framework::Vector3 diffuse;
    // Warm light reflected from sunlit ground onto faces turned away from the sun.
    Microsoft::Xna::Framework::Vector3 bounce;
    // Light reflected straight up from paving and grass onto soffits and other undersides.
    Microsoft::Xna::Framework::Vector3 uplight;
};

// Rays of the reflected uplight travel vertically upward, so only down-facing surfaces see it.
inline Microsoft::Xna::Framework::Vector3 groundUplightDirection()
{
    return {0.0f,1.0f,0.0f};
}

// Ground-bounce fill travels back against the sun's horizontal heading and slightly upward, so
// shaded walls and soffits receive it while terrain, roofs and every up-facing surface do not.
inline Microsoft::Xna::Framework::Vector3 groundBounceDirection(
    const Microsoft::Xna::Framework::Vector3& daylight)
{
    return Microsoft::Xna::Framework::Vector3::Normalize({-daylight.X,0.30f,-daylight.Z});
}

inline ExteriorLighting exteriorLighting(float tunnelMix)
{
    using namespace Microsoft::Xna::Framework;
    const float t=std::clamp(tunnelMix,0.0f,1.0f);
    // Cool skylight and warmer direct sun retain the old total lit-surface energy, but no
    // longer fill every shaded face with almost the same intensity as a sun-facing surface.
    return {Vector3{0.35f,0.42f,0.52f}*(1-t)+Vector3{0.40f,0.41f,0.43f}*t,
            Vector3{0.67f,0.59f,0.47f}*(1-t)+Vector3{0.12f,0.12f,0.12f}*t,
            Vector3{0.19f,0.17f,0.13f}*(1-t),
            Vector3{0.17f,0.16f,0.13f}*(1-t)};
}

// Open-air lighting from the atmosphere (ENV-138/139) faded into the tunnel's fixed fill. With
// the reference daylight it equals exteriorLighting(tunnelMix).
inline ExteriorLighting exteriorLighting(float tunnelMix, const ExteriorLighting& open)
{
    using namespace Microsoft::Xna::Framework;
    const float t=std::clamp(tunnelMix,0.0f,1.0f);
    return {open.ambient*(1-t)+Vector3{0.40f,0.41f,0.43f}*t,
            open.diffuse*(1-t)+Vector3{0.12f,0.12f,0.12f}*t,
            open.bounce*(1-t),
            open.uplight*(1-t)};
}

inline AvatarCabinLighting avatarCabinLighting(
    const Microsoft::Xna::Framework::Vector3& daylight, float tunnelMix)
{
    using Microsoft::Xna::Framework::Vector3;
    const float t = std::clamp(tunnelMix,0.0f,1.0f);
    return {Vector3::Normalize(daylight*(1.0f-t)+Vector3{0,-1,0}*t),
            Vector3{0.46f-0.28f*t,0.43f-0.24f*t,0.37f-0.17f*t}};
}

} // namespace TrainSim::Render
