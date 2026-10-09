#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include <algorithm>
#include <cmath>

namespace TrainSim::Render {

inline Microsoft::Xna::Framework::Color daylightSkyColor(float elevation)
{
    using Microsoft::Xna::Framework::Color;
    const float t=std::pow(std::clamp(elevation,0.0f,1.0f),0.55f);
    return Color(static_cast<int>(169-100*t),static_cast<int>(201-65*t),
                 static_cast<int>(222-24*t),255);
}

// Closed camera-centred sky. One unlit batch, without atmospheric shaders or textures.
inline void addDaylightSky(MeshBuilder& mesh,float radius)
{
    using namespace Microsoft::Xna::Framework;
    if (radius <= 0) return;
    constexpr int rings=16, segments=32;
    constexpr float pi=3.14159265358979323846f;
    const auto point=[&](int ring,int segment) {
        const float latitude=-pi*0.5f+pi*static_cast<float>(ring)/rings;
        const float longitude=2*pi*static_cast<float>(segment%segments)/segments;
        if (ring == 0) return Vector3{0,-radius,0};
        if (ring == rings) return Vector3{0,radius,0};
        return Vector3{radius*std::cos(latitude)*std::cos(longitude),
                       radius*std::sin(latitude),radius*std::cos(latitude)*std::sin(longitude)};
    };
    for (int ring=0;ring<rings;++ring)
        for (int segment=0;segment<segments;++segment)
        {
            const auto a=point(ring,segment),b=point(ring,segment+1);
            const auto c=point(ring+1,segment+1),d=point(ring+1,segment);
            if (ring == 0) mesh.addTriangle(a,c,d,daylightSkyColor(0));
            else if (ring+1 == rings) mesh.addTriangle(a,b,c,daylightSkyColor(1));
            else mesh.addQuad(a,b,c,d,daylightSkyColor(a.Y/radius),daylightSkyColor(b.Y/radius),
                              daylightSkyColor(c.Y/radius),daylightSkyColor(d.Y/radius));
        }
}

} // namespace TrainSim::Render
