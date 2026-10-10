#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/sim/Route.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace TrainSim::Render {

// A square-section bar between two arbitrary world points: wires, strands and struts follow
// the route chord between their end points instead of one straight box per 250 m chunk.
inline void addBarBetween(MeshBuilder& mesh,const Microsoft::Xna::Framework::Vector3& a,
                          const Microsoft::Xna::Framework::Vector3& b,float thickness,
                          const Microsoft::Xna::Framework::Color& colour,float overlap=0.0f)
{
    using namespace Microsoft::Xna::Framework;
    const Vector3 delta=b-a;
    const float horizontal=std::sqrt(delta.X*delta.X+delta.Z*delta.Z);
    const float length=delta.Length();
    if (length<=0.001f) return;
    const float yaw=std::atan2(delta.X,delta.Z);
    const float pitch=-std::atan2(delta.Y,horizontal);
    const Vector3 middle=(a+b)*0.5f;
    mesh.addBox(Vector3{-0.5f,-0.5f,-0.5f},Vector3{0.5f,0.5f,0.5f},colour,
                Matrix::CreateScale(thickness,thickness,length+overlap)
                *Matrix::CreateRotationX(pitch)*Matrix::CreateRotationY(yaw)
                *Matrix::CreateTranslation(middle));
}

inline bool linesideFenceAllowed(const Sim::RoutePose& pose)
{
    return !pose.bridge && !pose.tunnel && pose.urbanity<0.62;
}

inline constexpr double kFencePostSpacing=3.0;
inline constexpr double kFenceStrandSpacing=12.0;
inline constexpr float kFenceLateral=8.7f;
inline constexpr float kFencePostHeight=1.35f;
inline constexpr std::array<float,4> kFenceStrandHeights={0.32f,0.62f,0.92f,1.22f};

// Boundary fence for one chunk and side: slim concrete posts on an absolute 3 m cadence, four
// wire strands following the route chord every 12 m, and a strained end post with a diagonal
// strut wherever the fence actually stops. Point(distance,lateral) yields {x,y,z,yaw}.
// Returns the number of posts.
template<class Point>
int addLinesideFence(MeshBuilder& mesh,Point point,double begin,double end,double lateral,
                     bool endsAtBegin,bool endsAtEnd)
{
    using namespace Microsoft::Xna::Framework;
    const Color post(118,121,114,255),strand(74,78,76,255),strainer(104,108,102,255);
    const auto ground=[&](double distance,float height) {
        const auto p=point(distance,lateral);
        return Vector3{p.x,p.y+height,p.z};
    };
    int posts=0;
    const double first=std::ceil(begin/kFencePostSpacing)*kFencePostSpacing;
    for (double distance=first;distance<end;distance+=kFencePostSpacing)
    {
        const auto p=point(distance,lateral);
        mesh.addBox(Vector3{-0.5f,-0.5f,-0.5f},Vector3{0.5f,0.5f,0.5f},post,
                    Matrix::CreateScale(0.075f,kFencePostHeight,0.075f)
                    *Matrix::CreateRotationY(p.yaw)
                    *Matrix::CreateTranslation(p.x,p.y+kFencePostHeight*0.5f,p.z));
        ++posts;
    }
    const double firstStrand=std::floor(begin/kFenceStrandSpacing)*kFenceStrandSpacing;
    for (double distance=std::max(begin,firstStrand);distance<end;)
    {
        const double next=std::min(end,std::floor(distance/kFenceStrandSpacing+1.0)
                                        *kFenceStrandSpacing);
        for (float height : kFenceStrandHeights)
            addBarBetween(mesh,ground(distance,height),ground(next,height),0.018f,strand,0.02f);
        distance=next;
    }
    for (const auto& [atBegin,active] : {std::pair{true,endsAtBegin},std::pair{false,endsAtEnd}})
    {
        if (!active) continue;
        const double distance=atBegin ? begin : end;
        const double inward=atBegin ? 1.0 : -1.0;
        const auto p=point(distance,lateral);
        mesh.addBox(Vector3{-0.5f,-0.5f,-0.5f},Vector3{0.5f,0.5f,0.5f},strainer,
                    Matrix::CreateScale(0.11f,kFencePostHeight+0.05f,0.11f)
                    *Matrix::CreateRotationY(p.yaw)
                    *Matrix::CreateTranslation(p.x,p.y+(kFencePostHeight+0.05f)*0.5f,p.z));
        addBarBetween(mesh,ground(distance,kFencePostHeight-0.12f),
                      ground(distance+inward*1.6,0.05f),0.05f,strainer);
        ++posts;
    }
    return posts;
}

} // namespace TrainSim::Render
