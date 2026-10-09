#pragma once
#include "trainsim/render/MeshBuilder.hpp"
#include <array>
#include <algorithm>
#include <cmath>

namespace TrainSim::Render {

// Two low edging ribbons and six sparse planted pockets define a long access
// path without turning every lawn into paving. The central walking width stays clear.
template<class Surface>
void addAccessGarden(MeshBuilder& mesh,Surface&& surface,float length,float width)
{
    using namespace Microsoft::Xna::Framework;
    if (length<3 || width<0.6f) return;
    const auto quad=[&](Vector3 a,Vector3 b,Vector3 c,Vector3 d,Color colour) {
        if (Vector3::Cross(b-a,c-a).Y>=0) mesh.addQuad(a,b,c,d,colour);
        else mesh.addQuad(d,c,b,a,colour);
    };
    const int panels=static_cast<int>(std::ceil(length/2));
    for (float side : {-1.0f,1.0f})
    {
        const float edge=side*(width*0.5f+0.04f);
        for (int panel=0;panel<panels;++panel)
        {
            const float f0=static_cast<float>(panel)/static_cast<float>(panels);
            const float f1=static_cast<float>(panel+1)/static_cast<float>(panels);
            auto a=surface(f0,edge-0.04f),b=surface(f1,edge-0.04f);
            auto c=surface(f1,edge+0.04f),d=surface(f0,edge+0.04f);
            const Vector3 up{0,0.07f,0};
            const Color edging(132,132,119,255);
            quad(a+up,b+up,c+up,d+up,edging);
            // Both exposed ribbon sides; no full cuboids hidden below the ground.
            if (Vector3::Cross(b-a,c-a).Y>=0)
            {
                mesh.addQuad(a,b,b+up,a+up,edging);
                mesh.addQuad(c,d,d+up,c+up,edging);
            }
            else
            {
                mesh.addQuad(b,a,a+up,b+up,edging);
                mesh.addQuad(d,c,c+up,d+up,edging);
            }
        }
        for (float fraction : {0.20f,0.50f,0.80f})
        {
            const float half=std::min(0.7f/length,0.12f);
            const float inner=side*(width*0.5f+0.12f),outer=side*(width*0.5f+1.02f);
            auto a=surface(fraction-half,inner),b=surface(fraction+half,inner);
            auto c=surface(fraction+half,outer),d=surface(fraction-half,outer);
            quad(a,b,c,d,Color(83,76,60,255));
            const auto centre=surface(fraction,(inner+outer)*0.5f);
            mesh.addFoliageLobe(centre+Vector3::Up*0.35f,{0.42f,0.45f,0.42f},6,3,
                Color(67,91,50,255),true);
        }
    }
}

// A closed 240 mm precast kerb with chamfered top arrises. The callback samples
// the same curved street datum as the road and pavement, including grade changes.
template<class Surface>
void addStreetKerb(MeshBuilder& mesh,Surface&& surface,double begin,double end)
{
    using namespace Microsoft::Xna::Framework;
    if (end<=begin) return;
    const std::array<Vector2,6> profile{{
        {-0.12f,-0.24f},{-0.12f,-0.105f},{-0.08f,-0.07f},
        {0.08f,-0.07f},{0.12f,-0.105f},{0.12f,-0.24f}}};
    const auto p=[&](std::size_t i,double along) {
        auto point=surface(profile[i].X,along);
        point.Y+=profile[i].Y;
        return point;
    };
    const Color concrete(151,151,143,255);
    for (std::size_t i=0;i+1<profile.size();++i)
        mesh.addTexturedQuad(p(i,begin),p(i,end),p(i+1,end),p(i+1,begin),concrete,
            {0,profile[i].X},{static_cast<float>(end-begin)/0.65f,profile[i].X},
            {static_cast<float>(end-begin)/0.65f,profile[i+1].X},{0,profile[i+1].X});
    for (std::size_t i=1;i+1<profile.size();++i)
    {
        mesh.addTriangle(p(0,begin),p(i,begin),p(i+1,begin),concrete);
        mesh.addTriangle(p(0,end),p(i+1,end),p(i,end),concrete);
    }
}

template<class Surface>
void addStreetPocket(MeshBuilder& mesh,Surface&& surface,float yaw,int side)
{
    using namespace Microsoft::Xna::Framework;
    const auto tile=[&](float x0,float x1,float z0,float z1,const Color& colour) {
        auto a=surface(x0,z0),b=surface(x0,z1),c=surface(x1,z1),d=surface(x1,z0);
        a.Y+=0.045f; b.Y+=0.045f; c.Y+=0.045f; d.Y+=0.045f;
        if (side>0) mesh.addQuad(a,b,c,d,colour);
        else mesh.addQuad(d,c,b,a,colour);
    };
    for (int x=0;x<4;++x)
        for (int z=0;z<6;++z)
            tile(-4+2.0f*static_cast<float>(x),-2+2.0f*static_cast<float>(x),
                -6+2.0f*static_cast<float>(z),-4+2.0f*static_cast<float>(z),
                (x+z)%3==0 ? Color(147,146,133,255) : Color(158,157,146,255));
    tile(-5.4f,-4,-1,1,Color(158,157,146,255));
    // Two planted beds occupy the rear edge, leaving the centre and the bench approaches clear.
    for (float end : {-1.0f,1.0f})
    {
        const auto base=surface(3.0f,end*2.0f);
        const Matrix placement=Matrix::CreateRotationY(yaw)*Matrix::CreateTranslation(base);
        // A shallow buried footing accommodates the accepted nearly-flat terrain receiver.
        mesh.addBox({-0.65f,-0.10f,-1.1f},{0.65f,0.40f,1.1f},Color(119,122,108,255),placement);
        mesh.addBox({-0.55f,0.401f,-1.0f},{0.55f,0.415f,1.0f},Color(75,66,48,255),placement);
        for (float along : {-0.55f,0.0f,0.55f})
            mesh.addFoliageLobe(Vector3::Transform({0,0.72f,along},placement),
                {0.48f,0.40f,0.48f},6,3,Color(61,91,49,255),true);
    }
    for (float end : {-1.0f,1.0f})
    {
        const auto base=surface(0,end*4.4f)+Vector3::Up*0.05f;
        const Matrix placement=Matrix::CreateRotationY(yaw)*Matrix::CreateTranslation(base);
        for (float leg : {-0.85f,0.85f})
            mesh.addBox({leg-0.05f,0,-0.24f},{leg+0.05f,0.43f,0.24f},Color(53,61,57,255),placement);
        for (int slat=0;slat<3;++slat)
        {
            const float z=-0.23f+0.16f*static_cast<float>(slat);
            mesh.addBox({-1.1f,0.43f,z},{1.1f,0.49f,z+0.13f},Color(128,104,73,255),placement);
            const float y=0.57f+0.14f*static_cast<float>(slat);
            mesh.addBox({-1.1f,y,end*0.27f-0.03f},{1.1f,y+0.11f,end*0.27f+0.03f},
                Color(128,104,73,255),placement);
        }
    }
}
} // namespace TrainSim::Render
