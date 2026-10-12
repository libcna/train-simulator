#pragma once

#include "trainsim/render/Atmosphere.hpp"
#include "trainsim/render/MeshBuilder.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace TrainSim::Render {

inline Microsoft::Xna::Framework::Color daylightSkyColor(float elevation)
{
    using Microsoft::Xna::Framework::Color;
    const float t=std::pow(std::clamp(elevation,0.0f,1.0f),0.55f);
    return Color(static_cast<int>(169-100*t),static_cast<int>(201-65*t),
                 static_cast<int>(222-24*t),255);
}

// Closed camera-centred sky for any time of day and weather (ENV-138/139). The palette's
// vertical gradient matches daylightSkyColor for the reference day; a twilight glow brightens
// the horizon toward the sun, and stars are small quads on the upper hemisphere at night.
// towardSun is the horizontal direction of the sun in the same frame as the mesh.
inline void addAtmosphereSky(MeshBuilder& mesh,float radius,const SkyPalette& palette,
                             const Microsoft::Xna::Framework::Vector3& towardSun)
{
    using namespace Microsoft::Xna::Framework;
    if (radius <= 0) return;
    constexpr int rings=16, segments=32;
    constexpr float pi=3.14159265358979323846f;
    const float sunLength=std::hypot(towardSun.X,towardSun.Z);
    const auto sunward=[&](const Vector3& p) {
        const float length=std::hypot(p.X,p.Z);
        if (length<1e-4f || sunLength<1e-4f) return 0.0f;
        return (p.X*towardSun.X+p.Z*towardSun.Z)/(length*sunLength);
    };
    const auto colour=[&](const Vector3& p) { return skyColour(palette,p.Y/radius,sunward(p)); };
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
            if (ring == 0) mesh.addTriangle(a,c,d,skyColour(palette,0.0f));
            else if (ring+1 == rings) mesh.addTriangle(a,b,c,skyColour(palette,1.0f));
            else mesh.addQuad(a,b,c,d,colour(a),colour(b),colour(c),colour(d));
        }
    if (palette.stars <= 0.01f) return;
    // Deterministic star field above 8 degrees; brightness varies per star.
    std::uint32_t state=0x5354415AU;
    const auto next=[&state] {
        state=state*1664525U+1013904223U;
        return static_cast<float>(state>>8U)/16777216.0f;
    };
    for (int star=0;star<320;++star)
    {
        const float azimuth=2*pi*next();
        const float height=0.14f+0.86f*std::sqrt(next());
        const float brightness=0.45f+0.55f*next();
        const float horizontal=std::sqrt(std::max(0.0f,1.0f-height*height));
        const Vector3 direction{horizontal*std::cos(azimuth),height,horizontal*std::sin(azimuth)};
        const Vector3 centre=direction*(radius*0.985f);
        const Vector3 right=Vector3::Normalize(Vector3::Cross(direction,Vector3::Up))
            *(radius*(0.0016f+0.0012f*brightness));
        const Vector3 up=Vector3::Normalize(Vector3::Cross(right,direction))
            *(radius*(0.0016f+0.0012f*brightness));
        const Color base=skyColour(palette,height);
        const float w=palette.stars*brightness;
        const auto lift=[w](std::uint8_t channel,float target) {
            return static_cast<int>(static_cast<float>(channel)+(target-static_cast<float>(channel))*w);
        };
        const Color light(lift(base.getRProperty(),236.0f),lift(base.getGProperty(),238.0f),
                          lift(base.getBProperty(),255.0f),255);
        // Wound to face the camera at the sky centre.
        mesh.addQuad(centre-right-up,centre-right+up,centre+right+up,centre+right-up,light);
    }
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
