#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include <array>
#include <algorithm>
#include <cmath>

namespace TrainSim::Render {

inline float roadCarArchBottom(float z)
{
    for (float axle : {-1.30f,1.30f})
    {
        const float offset=z-axle;
        if (std::abs(offset)<=0.36f)
            return 0.30f+std::sqrt(std::max(0.0f,0.36f*0.36f-offset*offset));
    }
    return 0.28f;
}

// Open wheel arches are part of the body silhouette, not dark rectangles painted
// over an intact box. Shared upper points leave the existing tapered cabin intact.
inline void addRoadCarBody(MeshBuilder& mesh,const Microsoft::Xna::Framework::Color& colour)
{
    using namespace Microsoft::Xna::Framework;
    std::array<float,24> cuts{};
    std::size_t count=0;
    cuts[count++]=-2.05f;
    for (float axle : {-1.30f,1.30f})
        for (int i=0;i<=8;++i)
            cuts[count++]=axle-0.36f*std::cos(static_cast<float>(i)*3.141592654f/8);
    cuts[count++]=2.05f;
    const auto halfWidth=[](float z) { return 0.90f-0.12f*std::max(0.0f,(std::abs(z)-1.15f)/0.90f); };
    const auto top=[](float z) { return 1.02f-0.16f*std::max(0.0f,(std::abs(z)-1.15f)/0.90f); };
    const Color lower(colour.getRProperty()*4/5,colour.getGProperty()*4/5,
                      colour.getBProperty()*4/5,255);
    for (std::size_t i=0;i+1<count;++i)
    {
        const float a=cuts[i],b=cuts[i+1];
        for (float side : {-1.0f,1.0f})
        {
            const Vector3 p{side*halfWidth(a),roadCarArchBottom(a),a};
            const Vector3 q{side*halfWidth(b),roadCarArchBottom(b),b};
            const Vector3 r{side*halfWidth(b),top(b),b};
            const Vector3 s{side*halfWidth(a),top(a),a};
            if (side>0) mesh.addQuad(s,r,q,p,colour,colour,lower,lower);
            else mesh.addQuad(p,q,r,s,lower,lower,colour,colour);
        }
        mesh.addQuad({-halfWidth(a),top(a),a},{-halfWidth(b),top(b),b},
                     {halfWidth(b),top(b),b},{halfWidth(a),top(a),a},colour);
    }
    for (float end : {-1.0f,1.0f})
    {
        const float z=end*2.05f,w=halfWidth(z);
        const Vector3 a{-w,0.28f,z},b{w,0.28f,z},c{w,top(z),z},d{-w,top(z),z};
        if (end>0) mesh.addQuad(a,b,c,d,lower,lower,colour,colour);
        else mesh.addQuad(d,c,b,a,colour,colour,lower,lower);
    }
    mesh.addBox({-0.62f,0.25f,-1.95f},{0.62f,0.33f,1.95f},Color(37,40,39,255));
    // Inset wheel-well backing prevents bright pavement showing through above the tyre.
    for (float side : {-1.0f,1.0f})
        for (float axle : {-1.30f,1.30f})
            for (int i=0;i<8;++i)
            {
                const auto rim=[&](int step) {
                    const float angle=static_cast<float>(step)*3.141592654f/8;
                    return Vector3{side*0.74f,0.30f+0.36f*std::sin(angle),
                        axle+0.36f*std::cos(angle)};
                };
                const Vector3 centre{side*0.74f,0.30f,axle};
                if (side<0) mesh.addTriangle(centre,rim(i),rim(i+1),Color(24,28,27,255));
                else mesh.addTriangle(centre,rim(i+1),rim(i),Color(24,28,27,255));
                const auto a=rim(i),b=rim(i+1);
                auto c=b,d=a;
                c.X=side*halfWidth(c.Z);
                d.X=side*halfWidth(d.Z);
                const Vector3 radial{0,(a.Y+b.Y)*0.5f-0.30f,(a.Z+b.Z)*0.5f-axle};
                if (Vector3::Dot(Vector3::Cross(b-a,c-a),radial)<0)
                    mesh.addQuad(a,b,c,d,Color(28,32,31,255));
                else mesh.addQuad(d,c,b,a,Color(28,32,31,255));
            }
}

inline void addRoadWheelHub(MeshBuilder& mesh,float side,float axle)
{
    using namespace Microsoft::Xna::Framework;
    constexpr int segments=20;
    const auto p=[&](float radius,int step,float x) {
        const float angle=static_cast<float>(step)*6.283185307f/segments;
        return Vector3{side*x,0.30f+radius*std::sin(angle),axle+radius*std::cos(angle)};
    };
    for (int i=0;i<segments;++i)
    {
        auto a=p(0.18f,i,1.009f),b=p(0.18f,i+1,1.009f);
        auto c=p(0.145f,i+1,1.014f),d=p(0.145f,i,1.014f);
        if (side<0) mesh.addQuad(a,b,c,d,Color(176,180,177,255));
        else mesh.addQuad(d,c,b,a,Color(176,180,177,255));
        if (i%4==0)
        {
            const auto centre=p(0,0,1.016f);
            if (side<0) mesh.addTriangle(centre,d,c,Color(101,110,108,255));
            else mesh.addTriangle(centre,c,d,Color(101,110,108,255));
        }
    }
}

} // namespace TrainSim::Render
