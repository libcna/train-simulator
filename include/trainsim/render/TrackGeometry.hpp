#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include <algorithm>
#include <cmath>

namespace TrainSim::Render {

// Permanent-way cross-sections shared by the 2 km track batch. All coordinates are in the
// segment's local frame: X across the track, Y above rail-foot level, Z along the route.
inline constexpr float kBallastCrownHalfWidth=2.66f;
inline constexpr float kBallastCrownHeight=-0.084f;
inline constexpr float kBallastToeHeight=-0.40f;
inline constexpr float kSleeperHalfLength=1.20f;
inline constexpr float kSleeperHalfWidth=0.13f;
inline constexpr float kSleeperTopHalfWidth=0.105f;
inline constexpr float kRailFootHalfWidth=0.075f;
inline constexpr float kRailWebHalfWidth=0.009f;
inline constexpr float kRailHeadHalfWidth=0.036f;
inline constexpr float kRailFootTop=-0.016f;
inline constexpr float kRailHeadBottom=0.105f;
inline constexpr float kRailHeadTop=0.148f;

// Lateral distance of the ballast toe from the track centre. Bounded, deterministic wobble
// keeps the granular edge from reading as a ruled line while consecutive segments still meet.
inline float ballastShoulderToe(double distance)
{
    return 3.36f+0.16f*static_cast<float>(std::sin(distance*0.37))
               +0.09f*static_cast<float>(std::sin(distance*1.31+0.8));
}

// Monoblock concrete sleeper: two chamfered rail-seat blocks joined by a lower, narrower waist.
// The simple form is a single box for sleepers beyond the fastener detail radius.
inline void addConcreteSleeper(MeshBuilder& track,const Microsoft::Xna::Framework::Matrix& placement,
                               float centreX,const Microsoft::Xna::Framework::Color& tone,
                               bool detailed)
{
    using namespace Microsoft::Xna::Framework;
    if (!detailed)
    {
        track.addBox(Vector3{centreX-kSleeperHalfLength,-0.10f,-kSleeperHalfWidth},
                     Vector3{centreX+kSleeperHalfLength,0.0f,kSleeperHalfWidth},tone,placement);
        return;
    }
    const auto shade=[&](float factor) {
        return Color(static_cast<int>(tone.getRProperty()*factor),
                     static_cast<int>(tone.getGProperty()*factor),
                     static_cast<int>(tone.getBProperty()*factor),255);
    };
    const auto at=[&](float x,float y,float z) {
        return Vector3::Transform(Vector3{x,y,z},placement);
    };
    // A tapered block: wider at the base than at the top, so each long side is a visible chamfer.
    const auto block=[&](float x0,float x1,float top,float baseHalf,float topHalf) {
        constexpr float base=-0.10f;
        track.addQuad(at(x0,top,-topHalf),at(x0,top,topHalf),at(x1,top,topHalf),
                      at(x1,top,-topHalf),shade(1.08f));
        // Same winding as MeshBuilder::addBox so every chamfered side faces outward.
        track.addQuad(at(x1,base,-baseHalf),at(x0,base,-baseHalf),at(x0,top,-topHalf),
                      at(x1,top,-topHalf),shade(0.80f));
        track.addQuad(at(x0,base,baseHalf),at(x1,base,baseHalf),at(x1,top,topHalf),
                      at(x0,top,topHalf),shade(0.88f));
        track.addQuad(at(x0,base,-baseHalf),at(x0,base,baseHalf),at(x0,top,topHalf),
                      at(x0,top,-topHalf),shade(0.84f));
        track.addQuad(at(x1,base,baseHalf),at(x1,base,-baseHalf),at(x1,top,-topHalf),
                      at(x1,top,topHalf),shade(0.84f));
    };
    block(centreX-kSleeperHalfLength,centreX-0.34f,0.0f,kSleeperHalfWidth,kSleeperTopHalfWidth);
    block(centreX+0.34f,centreX+kSleeperHalfLength,0.0f,kSleeperHalfWidth,kSleeperTopHalfWidth);
    block(centreX-0.34f,centreX+0.34f,-0.035f,0.115f,0.095f);
}

// Flat-bottom rail: oxidised foot and slender web with a broader head whose running surface is
// supplied separately by the caller as a specular textured quad.
inline void addRailProfile(MeshBuilder& track,const Microsoft::Xna::Framework::Matrix& placement,
                           float centreX,float halfLength)
{
    using namespace Microsoft::Xna::Framework;
    track.addBox(Vector3{centreX-kRailFootHalfWidth,-0.03f,-halfLength},
                 Vector3{centreX+kRailFootHalfWidth,kRailFootTop,halfLength},
                 Color(86,64,52,255),placement);
    track.addBox(Vector3{centreX-kRailWebHalfWidth,kRailFootTop,-halfLength},
                 Vector3{centreX+kRailWebHalfWidth,kRailHeadBottom,halfLength},
                 Color(99,72,57,255),placement);
    track.addBox(Vector3{centreX-kRailHeadHalfWidth,kRailHeadBottom,-halfLength},
                 Vector3{centreX+kRailHeadHalfWidth,kRailHeadTop,halfLength},
                 Color(121,114,106,255),placement);
}

} // namespace TrainSim::Render
