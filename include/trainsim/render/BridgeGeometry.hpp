#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/sim/Route.hpp"
#include <algorithm>
#include <cmath>

namespace TrainSim::Render {

// The dry viaduct crosses a valley that runs across the railway. Terrain falls from the
// natural ground at each abutment to a broad floor; it is not a trench dug along the track.
// Zero weight at both abutments keeps the approach formation and its neighbours seamless.
inline double viaductValleyWeight(double distance,double lateral)
{
    const double fromBegin=distance-Sim::kValleyViaductBeginMetres;
    const double fromEnd=Sim::kValleyViaductEndMetres-distance;
    const double inside=std::min(fromBegin,fromEnd);
    if (!(inside>0.0)) return 0.0;
    // Each valley side has its own lateral phase, so the slopes are not parallel lines.
    const double reach=430.0+110.0*std::sin(lateral/260.0+(fromBegin<fromEnd ? 0.0 : 1.9));
    const double t=std::min(inside/reach,1.0);
    return 1.0-(1.0-t)*(1.0-t);
}

inline double viaductValleyFloor(double distance,double lateral)
{
    // A broad bowl deepest near mid-span, gentle undulation and a slight cross-fall toward
    // the drainage side. A flat floor under the whole 2.8 km span read as an open plain.
    constexpr double pi=3.14159265358979323846;
    const double span=Sim::kValleyViaductEndMetres-Sim::kValleyViaductBeginMetres;
    const double along=std::clamp((distance-Sim::kValleyViaductBeginMetres)/span,0.0,1.0);
    const double bowl=std::pow(std::sin(along*pi),0.7);
    // Valley-side spurs close in with lateral distance away from mid-span, so a passenger
    // looking along the valley sees it narrow toward rising ground rather than a level plain.
    const double outward=std::clamp((std::abs(lateral)-350.0)/750.0,0.0,1.0);
    const double narrowing=1.0-std::pow(std::sin(along*pi),3.0);
    const double spur=outward*outward*(3.0-2.0*outward)*narrowing
                     *(42.0+8.0*std::sin(lateral/170.0+(lateral<0.0 ? 0.0 : 2.3)));
    return -9.0-27.0*bowl+spur+1.8*std::sin(distance/520.0+lateral/310.0)
               +1.1*std::sin(lateral/120.0-distance/880.0)+0.0015*lateral;
}

inline double viaductValleyTerrain(double distance,double lateral,double natural)
{
    const double weight=viaductValleyWeight(distance,lateral);
    return natural+(viaductValleyFloor(distance,lateral)-natural)*weight;
}

struct BridgeParapetStyle
{
    Microsoft::Xna::Framework::Color upstand,coping,cornice,steel,trough;
};

inline BridgeParapetStyle bridgeParapetStyle(bool waterCrossing)
{
    using Microsoft::Xna::Framework::Color;
    return waterCrossing
        ? BridgeParapetStyle{Color(108,116,116,255),Color(150,154,149,255),
                             Color(53,72,80,255),Color(164,171,170,255),Color(104,108,104,255)}
        : BridgeParapetStyle{Color(128,127,119,255),Color(163,161,151,255),
                             Color(112,111,104,255),Color(152,157,154,255),Color(110,111,105,255)};
}

inline constexpr float kBridgeParapetTop=0.45f;
inline constexpr float kBridgeHandrailHeight=1.22f;
inline constexpr float kBridgeKneeRailHeight=0.86f;
inline constexpr float kBridgeHandrailRadius=0.030f;
inline constexpr float kBridgeKneeRailRadius=0.021f;
inline constexpr float kBridgePostWidth=0.055f;
inline constexpr double kBridgePostSpacing=2.5;

// A tube along the local route axis. Faces are outward and lit from above.
inline void addBridgeRail(MeshBuilder& mesh,float x,float y,float z,float yaw,
                          float radius,float length,int sides,bool capBegin,bool capEnd,
                          const Microsoft::Xna::Framework::Color& color)
{
    using namespace Microsoft::Xna::Framework;
    const Matrix transform=Matrix::CreateRotationY(yaw)*Matrix::CreateTranslation(x,y,z);
    constexpr float tau=6.2831853071795864769f;
    const float half=length*0.5f;
    const auto at=[&](float angle,float along) {
        return Vector3::Transform(Vector3{std::cos(angle)*radius,std::sin(angle)*radius,along},
                                  transform);
    };
    for (int side=0;side<sides;++side)
    {
        const float a0=tau*static_cast<float>(side)/static_cast<float>(sides);
        const float a1=tau*static_cast<float>(side+1)/static_cast<float>(sides);
        const float facing=std::sin((a0+a1)*0.5f);
        const float factor=0.78f+0.24f*facing;
        const Color shade(static_cast<int>(std::min(255.0f,color.getRProperty()*factor)),
                          static_cast<int>(std::min(255.0f,color.getGProperty()*factor)),
                          static_cast<int>(std::min(255.0f,color.getBProperty()*factor)),255);
        mesh.addQuad(at(a0,half),at(a0,-half),at(a1,-half),at(a1,half),shade);
        if (capBegin) mesh.addTriangle(Vector3::Transform(Vector3{0,0,-half},transform),
                                       at(a1,-half),at(a0,-half),color);
        if (capEnd) mesh.addTriangle(Vector3::Transform(Vector3{0,0,half},transform),
                                     at(a0,half),at(a1,half),color);
    }
}

// One straight bridge section edge: concrete upstand with an overhanging coping and drip
// line, an inboard cable trough, and a galvanised two-rail handrail on slender posts with
// base plates. Point(distance,lateral) returns a local route point {x,y,z,yaw} at rail level.
// Outward is +1 when increasing lateral leaves the deck. Low quality omits the post base
// plates and trough lid joints, which only read at close range. Returns the number of posts.
template<class Point>
int addBridgeParapet(MeshBuilder& mesh,Point point,double begin,double end,double lateral,
                     float outward,bool capBegin,bool capEnd,const BridgeParapetStyle& style,
                     bool closeDetail=true)
{
    using namespace Microsoft::Xna::Framework;
    const auto box=[&](double distance,double across,float y,float sx,float sy,float sz,
                       const Color& color) {
        const auto p=point(distance,lateral+across*outward);
        mesh.addBox(Vector3{-0.5f,-0.5f,-0.5f},Vector3{0.5f,0.5f,0.5f},color,
                    Matrix::CreateScale(sx,sy,sz)*Matrix::CreateRotationY(p.yaw)
                    *Matrix::CreateTranslation(p.x,p.y+y,p.z));
    };
    const double middle=(begin+end)*0.5;
    const float length=static_cast<float>(end-begin)+0.02f;
    // Upstand from deck level, coping projecting outward and a recessed drip shadow.
    box(middle,0.0,0.16f,0.28f,0.58f,length,style.upstand);
    box(middle,0.05,kBridgeParapetTop-0.035f,0.40f,0.07f,length,style.coping);
    box(middle,0.146,kBridgeParapetTop-0.105f,0.012f,0.03f,length,Color(66,69,67,255));
    box(middle,0.21,-0.30f,0.10f,0.66f,length,style.cornice);
    // Cable trough lids on the walkway with joints at the post cadence.
    box(middle,-0.62,-0.065f,0.42f,0.10f,length,style.trough);

    const double firstPost=std::ceil(begin/kBridgePostSpacing)*kBridgePostSpacing;
    int posts=0;
    for (double distance=firstPost;distance<end;distance+=kBridgePostSpacing)
    {
        const float postHeight=kBridgeHandrailHeight-kBridgeParapetTop;
        if (closeDetail)
        {
            const auto joint0=point(distance,lateral-0.83*outward);
            const auto joint1=point(distance,lateral-0.41*outward);
            const Vector3 a{joint0.x,joint0.y-0.013f,joint0.z};
            const Vector3 b{joint1.x,joint1.y-0.013f,joint1.z};
            const Vector3 along{std::sin(joint0.yaw)*0.012f,0.0f,std::cos(joint0.yaw)*0.012f};
            const Color joint(70,72,68,255);
            if (Vector3::Cross(b-a,along).Y>0.0f)
                mesh.addQuad(a-along,b-along,b+along,a+along,joint);
            else mesh.addQuad(a+along,b+along,b-along,a-along,joint);
            box(distance,-0.02,kBridgeParapetTop+0.012f,0.15f,0.024f,0.12f,style.steel);
        }
        box(distance,-0.02,kBridgeParapetTop+postHeight*0.5f,
            kBridgePostWidth,postHeight,kBridgePostWidth,style.steel);
        ++posts;
    }
    const auto rail=point(middle,lateral-0.02*outward);
    addBridgeRail(mesh,rail.x,rail.y+kBridgeHandrailHeight,rail.z,rail.yaw,
                  kBridgeHandrailRadius,length,8,capBegin,capEnd,style.steel);
    addBridgeRail(mesh,rail.x,rail.y+kBridgeKneeRailHeight,rail.z,rail.yaw,
                  kBridgeKneeRailRadius,length,6,capBegin,capEnd,style.steel);
    return posts;
}

} // namespace TrainSim::Render
