#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/sim/Route.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

namespace TrainSim::Render {

// Tunnel lining construction shared by both authored tunnels. Point(distance,lateral) returns a
// local route point {x,y,z,yaw} at rail level; `face` is the lateral of the structural wall's
// inner face and `inward` is +1 or -1 toward the running lines. Wall-mounted quads are oriented
// so that their visible side faces the track, independent of the wall side or route heading.
inline constexpr double kTunnelRingSpacing=2.0;
inline constexpr double kTunnelBracketSpacing=4.0;
inline constexpr float kTunnelLiningBottom=0.20f;
inline constexpr float kTunnelLiningTop=6.72f;
inline constexpr float kTunnelHandrailHeight=1.28f;
inline constexpr float kTunnelCableTrayHeight=3.16f;
inline constexpr float kTunnelLuminaireHeight=2.92f;

struct TunnelLiningStyle
{
    Microsoft::Xna::Framework::Color lining,joint,pocket,haunch,steel,bracket,tray,cable;
};

inline TunnelLiningStyle tunnelLiningStyle()
{
    using Microsoft::Xna::Framework::Color;
    return {Color(146,144,136,255),Color(58,58,54,255),Color(48,48,45,255),
            Color(88,86,80,255),Color(150,156,150,255),Color(94,98,96,255),
            Color(104,108,100,255),Color(44,46,48,255)};
}

namespace detail {
template<class Point>
Microsoft::Xna::Framework::Vector3 tunnelInward(Point& point,double distance,double face,
                                                float inward)
{
    const auto a=point(distance,face),b=point(distance,face+inward);
    return {b.x-a.x,0.0f,b.z-a.z};
}

template<class Point>
Microsoft::Xna::Framework::Vector3 wallPosition(Point& point,double distance,double lateral,
                                                float height)
{
    const auto p=point(distance,lateral);
    return {p.x,p.y+height,p.z};
}

// Wall rectangle between two route distances and two heights at one lateral offset.
template<class Point>
void wallQuad(MeshBuilder& mesh,Point& point,double d0,double d1,float h0,float h1,
              double lateral,const Microsoft::Xna::Framework::Vector3& inwardVector,
              const Microsoft::Xna::Framework::Color& colour)
{
    using namespace Microsoft::Xna::Framework;
    const Vector3 a=wallPosition(point,d0,lateral,h0),b=wallPosition(point,d1,lateral,h0);
    const Vector3 c=wallPosition(point,d1,lateral,h1),d=wallPosition(point,d0,lateral,h1);
    if (Vector3::Dot(Vector3::Cross(b-a,c-a),inwardVector)>=0.0f) mesh.addQuad(a,b,c,d,colour);
    else mesh.addQuad(d,c,b,a,colour);
}

template<class Point>
void wallTexturedQuad(MeshBuilder& mesh,Point& point,double d0,double d1,float h0,float h1,
                      double lateral,const Microsoft::Xna::Framework::Vector3& inwardVector,
                      float tileMetres,const std::array<Microsoft::Xna::Framework::Color,4>& colours)
{
    using namespace Microsoft::Xna::Framework;
    const Vector3 a=wallPosition(point,d0,lateral,h0),b=wallPosition(point,d1,lateral,h0);
    const Vector3 c=wallPosition(point,d1,lateral,h1),d=wallPosition(point,d0,lateral,h1);
    const auto uv=[&](double distance,float height) {
        return Vector2{static_cast<float>(distance/tileMetres),height/tileMetres};
    };
    if (Vector3::Dot(Vector3::Cross(b-a,c-a),inwardVector)>=0.0f)
        mesh.addTexturedQuad(a,b,c,d,colours[0],colours[1],colours[2],colours[3],
                             uv(d0,h0),uv(d1,h0),uv(d1,h1),uv(d0,h1));
    else
        mesh.addTexturedQuad(d,c,b,a,colours[3],colours[2],colours[1],colours[0],
                             uv(d0,h1),uv(d1,h1),uv(d1,h0),uv(d0,h0));
}

template<class Point>
void wallBox(MeshBuilder& mesh,Point& point,double distance,double lateral,float y,
             float across,float tall,float along,const Microsoft::Xna::Framework::Color& colour)
{
    using namespace Microsoft::Xna::Framework;
    const auto p=point(distance,lateral);
    mesh.addBox(Vector3{-0.5f,-0.5f,-0.5f},Vector3{0.5f,0.5f,0.5f},colour,
                Matrix::CreateScale(across,tall,along)*Matrix::CreateRotationY(p.yaw)
                *Matrix::CreateTranslation(p.x,p.y+y,p.z));
}
} // namespace detail

// One structural segment of lining: a textured precast face, a wall/roof haunch, absolute-cadence
// ring joints with bolt pockets and two longitudinal joints. Returns the ring joints added.
template<class Point>
int addTunnelLiningSegment(MeshBuilder& structure,MeshBuilder& lining,Point point,
                           double begin,double end,double face,float inward,bool closeDetail,
                           const TunnelLiningStyle& style)
{
    using namespace Microsoft::Xna::Framework;
    const Vector3 inwardVector=detail::tunnelInward(point,(begin+end)*0.5,face,inward);
    const double offset=static_cast<double>(inward);
    detail::wallTexturedQuad(lining,point,begin,end,kTunnelLiningBottom,kTunnelLiningTop,
                             face+offset*0.012,inwardVector,1.6f,
                             {style.lining,style.lining,style.lining,style.lining});
    // Haunch: the lining leans inward below the roof slab instead of meeting it at a hard box edge.
    {
        const Vector3 a=detail::wallPosition(point,begin,face+offset*0.012,6.30f);
        const Vector3 b=detail::wallPosition(point,end,face+offset*0.012,6.30f);
        const Vector3 c=detail::wallPosition(point,end,face+offset*0.55,6.78f);
        const Vector3 d=detail::wallPosition(point,begin,face+offset*0.55,6.78f);
        if (Vector3::Dot(Vector3::Cross(b-a,c-a),inwardVector)>=0.0f)
            structure.addQuad(a,b,c,d,style.haunch);
        else structure.addQuad(d,c,b,a,style.haunch);
    }
    for (float height : {2.35f,4.55f})
        detail::wallQuad(structure,point,begin,end,height-0.022f,height+0.022f,
                         face+offset*0.02,inwardVector,style.joint);
    int rings=0;
    const double firstRing=std::ceil(begin/kTunnelRingSpacing)*kTunnelRingSpacing;
    for (double ring=firstRing;ring<end;ring+=kTunnelRingSpacing)
    {
        detail::wallQuad(structure,point,ring-0.03,ring+0.03,kTunnelLiningBottom+0.02f,6.28f,
                         face+offset*0.02,inwardVector,style.joint);
        if (closeDetail)
            for (float height : {1.45f,4.95f})
                detail::wallQuad(structure,point,ring-0.055,ring+0.055,height-0.055f,
                                 height+0.055f,face+offset*0.025,inwardVector,style.pocket);
        ++rings;
    }
    return rings;
}

// Handrail and cable tray with wall brackets. Returns the brackets added.
template<class Point>
int addTunnelServiceRuns(MeshBuilder& structure,Point point,double begin,double end,
                         double face,float inward,const TunnelLiningStyle& style)
{
    const double offset=static_cast<double>(inward);
    const float length=static_cast<float>(end-begin)+0.02f;
    const double middle=(begin+end)*0.5;
    detail::wallBox(structure,point,middle,face+offset*0.09,kTunnelHandrailHeight,
                    0.05f,0.05f,length,style.steel);
    detail::wallBox(structure,point,middle,face+offset*0.11,kTunnelCableTrayHeight,
                    0.16f,0.07f,length,style.tray);
    detail::wallBox(structure,point,middle,face+offset*0.07,kTunnelCableTrayHeight+0.052f,
                    0.035f,0.035f,length,style.cable);
    detail::wallBox(structure,point,middle,face+offset*0.14,kTunnelCableTrayHeight+0.052f,
                    0.035f,0.035f,length,style.cable);
    int brackets=0;
    const double first=std::ceil(begin/kTunnelBracketSpacing)*kTunnelBracketSpacing;
    for (double bracket=first;bracket<end;bracket+=kTunnelBracketSpacing)
    {
        detail::wallBox(structure,point,bracket,face+offset*0.045,kTunnelHandrailHeight-0.01f,
                        0.09f,0.07f,0.05f,style.bracket);
        detail::wallBox(structure,point,bracket,face+offset*0.11,kTunnelCableTrayHeight-0.05f,
                        0.20f,0.04f,0.06f,style.bracket);
        ++brackets;
    }
    return brackets;
}

// Wall luminaire: bracketed housing with end caps and a thin lens, plus a gradient wash on the
// textured lining whose edges equal the lining tint so the falloff is seamless.
template<class Point>
void addTunnelWallLuminaire(MeshBuilder& structure,MeshBuilder& lining,MeshBuilder& emitter,
                            Point point,double distance,double face,float inward,
                            const TunnelLiningStyle& style)
{
    using namespace Microsoft::Xna::Framework;
    const Vector3 inwardVector=detail::tunnelInward(point,distance,face,inward);
    const double offset=static_cast<double>(inward);
    const Color centre(214,206,178,255);
    const Color edge=style.lining;
    const double washLateral=face+offset*0.016;
    constexpr float washTop=4.25f,washBottom=1.85f;
    const float washMiddle=kTunnelLuminaireHeight;
    constexpr double washReach=1.9;
    // Four quads meet at the fixture so the bright corner of each one is the shared centre.
    detail::wallTexturedQuad(lining,point,distance-washReach,distance,washBottom,washMiddle,
                             washLateral,inwardVector,1.6f,{edge,edge,centre,edge});
    detail::wallTexturedQuad(lining,point,distance,distance+washReach,washBottom,washMiddle,
                             washLateral,inwardVector,1.6f,{edge,edge,edge,centre});
    detail::wallTexturedQuad(lining,point,distance-washReach,distance,washMiddle,washTop,
                             washLateral,inwardVector,1.6f,{edge,centre,edge,edge});
    detail::wallTexturedQuad(lining,point,distance,distance+washReach,washMiddle,washTop,
                             washLateral,inwardVector,1.6f,{centre,edge,edge,edge});

    const Color housing(62,64,60,255),cap(96,100,96,255);
    for (double along : {-0.42,0.42})
        detail::wallBox(structure,point,distance+along,face+offset*0.08,
                        kTunnelLuminaireHeight+0.03f,0.16f,0.05f,0.05f,style.bracket);
    detail::wallBox(structure,point,distance,face+offset*0.17,kTunnelLuminaireHeight,
                    0.14f,0.24f,1.10f,housing);
    for (double along : {-0.55,0.55})
        detail::wallBox(structure,point,distance+along,face+offset*0.17,kTunnelLuminaireHeight,
                        0.15f,0.27f,0.04f,cap);
    detail::wallBox(emitter,point,distance,face+offset*0.255,kTunnelLuminaireHeight-0.02f,
                    0.04f,0.12f,0.98f,Color(247,230,170,255));
    // Flush handrail guidance marker beneath the fixture.
    detail::wallBox(emitter,point,distance,face+offset*0.035,1.10f,0.03f,0.06f,0.24f,
                    Color(184,207,181,255));
}

// Small wall plate for printed text; the caller adds glyphs in front of it.
template<class Point>
void addTunnelWallPlate(MeshBuilder& structure,Point point,double distance,double face,
                        float inward,float centreHeight,float width,float height,
                        const Microsoft::Xna::Framework::Color& colour)
{
    detail::wallBox(structure,point,distance,face+static_cast<double>(inward)*0.04,centreHeight,
                    0.025f,height,width,colour);
}

// Authored tunnel extents: {begin,end} containing the distance, if any.
inline std::optional<std::pair<double,double>> tunnelExtentAt(double distance)
{
    for (const auto& extent : Sim::kTunnelExtentsMetres)
        if (distance>extent.first && distance<extent.second) return extent;
    return std::nullopt;
}

// Escape-route sign text: direction arrow and distance to the nearer portal, e.g. "< 850 M".
inline std::string tunnelExitLabel(double distance,double begin,double end)
{
    const double toBegin=std::max(0.0,distance-begin),toEnd=std::max(0.0,end-distance);
    const bool backward=toBegin<=toEnd;
    const double metres=backward ? toBegin : toEnd;
    char buffer[24];
    if (metres<995.0)
        std::snprintf(buffer,sizeof buffer,"%d M",static_cast<int>(std::lround(metres/10.0))*10);
    else std::snprintf(buffer,sizeof buffer,"%.1f KM",metres/1000.0);
    return backward ? std::string("< ")+buffer : std::string(buffer)+" >";
}

// Hectometre plate text, e.g. "123.4".
inline std::string tunnelChainageLabel(double distance)
{
    char buffer[16];
    std::snprintf(buffer,sizeof buffer,"%.1f",std::floor(distance/100.0+1e-6)/10.0);
    return buffer;
}

} // namespace TrainSim::Render
