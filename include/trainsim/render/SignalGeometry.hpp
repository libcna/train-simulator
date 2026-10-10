#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include <array>
#include <cmath>

namespace TrainSim::Render {

// Three-aspect colour-light signal. Point(distance,lateral) yields {x,y,z,yaw}; `facing` is +1
// when the signal addresses trains travelling toward increasing route distance. Lens centres
// stay exactly where the live aspect pass overlays its lit face.
inline constexpr std::array<float,3> kSignalLensHeights={4.42f,4.10f,3.78f};
inline constexpr double kSignalLensDepth=0.275;
inline constexpr double kSignalLensInset=0.20;

template<class Point>
void addColourLightSignal(MeshBuilder& mesh,Point point,double distance,double lateral,
                          int facing,bool closeDetail=true)
{
    using namespace Microsoft::Xna::Framework;
    const double side=lateral<0.0 ? -1.0 : 1.0;
    const double faceDistance=distance-kSignalLensDepth*facing;
    const double lensLateral=lateral-side*kSignalLensInset;
    const auto box=[&](double d,double l,float y,float sx,float sy,float sz,const Color& c,
                       bool faceYaw) {
        const auto p=point(d,l);
        const float yaw=p.yaw+(faceYaw && facing<0 ? 3.14159265358979f : 0.0f);
        mesh.addBox(Vector3{-0.5f,-0.5f,-0.5f},Vector3{0.5f,0.5f,0.5f},c,
                    Matrix::CreateScale(sx,sy,sz)*Matrix::CreateRotationY(yaw)
                    *Matrix::CreateTranslation(p.x,p.y+y,p.z));
    };
    const Color steel(55,60,58,255),galvanised(119,123,119,255),black(28,31,30,255);

    // Foundation, flanged mast base with bolts, mast and an access panel at hand height.
    box(distance,lateral,0.12f,0.55f,0.24f,0.55f,Color(105,105,99,255),false);
    box(distance,lateral,0.255f,0.34f,0.03f,0.34f,Color(96,99,95,255),false);
    if (closeDetail)
        for (double dx : {-0.12,0.12})
            for (double dz : {-0.12,0.12})
                box(distance+dz,lateral+dx,0.285f,0.045f,0.03f,0.045f,Color(140,142,136,255),false);
    box(distance,lateral,2.1f,0.18f,4.2f,0.18f,steel,false);
    box(faceDistance+0.17*facing,lensLateral+side*0.20,1.35f,0.13f,0.56f,0.012f,
        Color(82,88,86,255),true);

    // Head: a white-edged black backboard in front of a slimmer housing, each lens recessed in
    // a lighter rim beneath its own hood.
    box(faceDistance+0.21*facing,lensLateral,4.10f,0.36f,1.22f,0.40f,black,true);
    box(faceDistance+0.045*facing,lensLateral,4.10f,0.76f,1.44f,0.03f,Color(228,228,222,255),true);
    box(faceDistance+0.02*facing,lensLateral,4.10f,0.66f,1.34f,0.035f,black,true);
    for (float height : kSignalLensHeights)
    {
        box(faceDistance+0.005*facing,lensLateral,height,0.29f,0.29f,0.02f,Color(92,96,94,255),true);
        box(faceDistance,lensLateral,height,0.23f,0.23f,0.075f,Color(16,19,18,255),true);
        box(faceDistance-0.10*facing,lensLateral,height+0.155f,0.30f,0.03f,0.24f,black,true);
    }

    // Rear ladder: slim stiles and round-ish rungs on stand-off brackets, with a small landing
    // and hoop guard at head level.
    const double ladderDistance=distance+0.16*facing;
    for (double offset : {-0.16,0.16})
        box(ladderDistance,lateral+offset,1.87f,0.028f,3.35f,0.028f,galvanised,false);
    for (float rung=0.38f;rung<3.48f;rung+=0.31f)
        box(ladderDistance,lateral,rung,0.33f,0.022f,0.022f,galvanised,false);
    for (float height : {0.55f,1.80f,3.05f})
        box(distance+0.125*facing,lateral,height,0.05f,0.03f,0.09f,Color(96,100,98,255),false);
    box(distance+0.22*facing,lateral+side*0.40,3.56f,0.62f,0.035f,0.42f,galvanised,false);
    box(distance+0.22*facing,lateral+side*0.69,4.05f,0.025f,0.95f,0.42f,galvanised,false);
    box(distance+0.22*facing,lateral+side*0.69,4.52f,0.025f,0.025f,0.42f,galvanised,false);

    // Ground cabinet with hinged door, louvres and a conduit up the mast.
    const double cabinetLateral=lateral+side*0.54;
    box(distance,cabinetLateral,0.47f,0.54f,0.86f,0.42f,Color(109,116,112,255),false);
    box(distance-0.23*facing,cabinetLateral,0.47f,0.44f,0.69f,0.035f,Color(148,153,146,255),true);
    if (closeDetail)
    {
        for (float hinge : {0.25f,0.69f})
            box(distance-0.235*facing,cabinetLateral+side*0.20,hinge,0.03f,0.07f,0.045f,
                Color(92,96,94,255),true);
        box(distance-0.24*facing,cabinetLateral-side*0.15,0.47f,0.025f,0.08f,0.02f,
            Color(60,64,62,255),true);
        for (float louvre : {0.64f,0.69f,0.74f})
            box(distance-0.236*facing,cabinetLateral,louvre,0.30f,0.012f,0.01f,
                Color(70,74,72,255),true);
    }
    box(distance,lateral+side*0.30,0.93f,0.33f,0.045f,0.045f,Color(80,84,82,255),false);
    box(distance,lateral+side*0.115,1.35f,0.045f,0.9f,0.045f,Color(80,84,82,255),false);

    // Identification plate below the head.
    box(faceDistance-0.01*facing,lensLateral,3.20f,0.50f,0.27f,0.055f,Color(230,227,205,255),true);
}

} // namespace TrainSim::Render
