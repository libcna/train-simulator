#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/render/PlatformLayout.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace TrainSim::Render {

enum class StationFacadeFamily { Rural, Urban, Waterfront, Suburban };

struct StationMaterialPalette
{
    Microsoft::Xna::Framework::Color wall,plinth,trim;
    bool brickUpper;
};

inline StationMaterialPalette stationMaterialPalette(StationFacadeFamily family,bool terminal)
{
    using Microsoft::Xna::Framework::Color;
    if (terminal) return {Color(201,192,169,255),Color(123,126,119,255),Color(183,180,163,255),false};
    switch (family)
    {
    case StationFacadeFamily::Urban:
        return {Color(184,143,110,255),Color(114,119,115,255),Color(184,179,159,255),true};
    case StationFacadeFamily::Waterfront:
        return {Color(174,192,187,255),Color(111,133,135,255),Color(190,200,188,255),false};
    case StationFacadeFamily::Rural:
        return {Color(195,189,166,255),Color(126,128,111,255),Color(178,180,153,255),false};
    case StationFacadeFamily::Suburban:
        return {Color(205,201,188,255),Color(128,113,103,255),Color(171,176,166,255),false};
    }
    return {};
}

// Closed perimeter cap/cornice and restrained corner masonry. The lower front
// cap is split around the public door instead of running a stone bar across it.
inline void addStationWallFinish(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Matrix& placement,float width,float length,
    float height,float plinth,float doorAlong,float platformSide,
    const Microsoft::Xna::Framework::Color& trim,bool quoins)
{
    using namespace Microsoft::Xna::Framework;
    if (width<2 || length<4 || height<3 || plinth<=0 || std::abs(platformSide)!=1) return;
    const float hw=width*0.5f,hl=length*0.5f,front=-platformSide*hw;
    const auto box=[&](Vector3 a,Vector3 b) { mesh.addBox(a,b,trim,placement); };
    for (float y : {plinth,height-0.20f})
    {
        const float outer=hw+0.075f;
        for (float end : {-hl,hl})
            box({-outer,y,end-0.065f},{outer,y+0.075f,end+0.065f});
        box({-front-0.065f,y,-hl},{-front+0.065f,y+0.075f,hl});
        if (y<height-0.3f)
        {
            box({front-0.075f,y,-hl},{front+0.075f,y+0.075f,doorAlong-0.88f});
            box({front-0.075f,y,doorAlong+0.88f},{front+0.075f,y+0.075f,hl});
        }
        else box({front-0.075f,y,-hl},{front+0.075f,y+0.075f,hl});
    }
    if (!quoins) return;
    int course=0;
    for (float y=plinth+0.10f;y<height-0.35f;y+=0.34f,++course)
        for (float side : {-1.0f,1.0f})
            for (float end : {-1.0f,1.0f})
            {
                const float reach=course%2==0 ? 0.48f : 0.30f;
                const float x=side*hw,z=end*hl;
                box({x-0.045f,y,std::min(z,z-end*reach)},
                    {x+0.045f,std::min(y+0.31f,height-0.24f),std::max(z,z-end*reach)});
                box({std::min(x,x-side*reach),y,z-0.045f},
                    {std::max(x,x-side*reach),std::min(y+0.31f,height-0.24f),z+0.045f});
            }
}

// Opaque shallow-room approximation, held behind the frame and mullions. Two
// faces support either platform bank without sorting individual glass panes.
inline void addStationGlazing(MeshBuilder& mesh,float width,float height,
    const Microsoft::Xna::Framework::Matrix& placement,unsigned seed)
{
    using namespace Microsoft::Xna::Framework;
    if (width<=0 || height<=0) return;
    const unsigned variant=(seed^(seed>>5U))%4U;
    for (float face : {-1.0f,1.0f})
    {
        const auto panel=[&](float x0,float y0,float x1,float y1,float depth,Color low,Color high) {
            const auto p=[&](float x,float y) {
                return Vector3::Transform({x*width,y*height,face*depth},placement);
            };
            if (face>0) mesh.addQuad(p(x0,y0),p(x1,y0),p(x1,y1),p(x0,y1),low,low,high,high);
            else mesh.addQuad(p(x1,y0),p(x0,y0),p(x0,y1),p(x1,y1),low,low,high,high);
        };
        // Muted lower room tones and upper sky/canopy reflection, not luminous blue paint.
        panel(-0.5f,-0.5f,0.5f,0.5f,0.0675f,Color(67,77,75,255),Color(95,117,122,255));
        panel(-0.47f,-0.25f,0.47f,0.24f,0.0678f,Color(62,75,76,255),Color(84,98,94,255));
        const float partition=variant%2U==0 ? -0.28f : 0.27f;
        panel(partition-0.014f,-0.46f,partition+0.014f,0.40f,0.0680f,
              Color(99,108,102,255),Color(119,126,115,255));
        panel(-0.46f,0.36f,0.46f,0.40f,0.0682f,Color(113,128,127,255),Color(114,130,131,255));
        if (variant==1)
        {
            // Staff-office roller shade, drawn down only part way.
            panel(-0.47f,0.12f,0.47f,0.48f,0.0684f,Color(120,123,111,255),Color(145,148,132,255));
            panel(-0.47f,0.105f,0.47f,0.125f,0.0686f,Color(78,89,85,255),Color(78,89,85,255));
        }
        else
        {
            // Shallow silhouettes of waiting-room seating and a small wall notice.
            panel(-0.39f,-0.34f,0.37f,-0.19f,0.0684f,Color(39,61,63,255),Color(49,74,75,255));
            panel(-0.40f,-0.36f,0.38f,-0.335f,0.0686f,Color(93,109,105,255),Color(93,109,105,255));
            for (float x : {-0.29f,0.27f})
                panel(x-0.014f,-0.47f,x+0.014f,-0.36f,0.0686f,Color(57,70,69,255),Color(63,76,75,255));
            panel(0.31f,-0.04f,0.43f,0.18f,0.0683f,Color(118,120,103,255),Color(132,134,117,255));
        }
    }
}

// Bounded facade kits supplement the existing station mass and roof. Local +Z
// faces the platform; cladding stays behind window beads and clear of the door.
inline void addStationFacadeIdentity(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Matrix& placement,float length,float height,
    float doorX,StationFacadeFamily family)
{
    using namespace Microsoft::Xna::Framework;
    if (length<6 || height<3.5f) return;
    const bool boarded=family==StationFacadeFamily::Rural || family==StationFacadeFamily::Waterfront;
    const Color trim=family==StationFacadeFamily::Waterfront ? Color(194,204,194,255)
        : boarded ? Color(180,181,155,255) : Color(114,129,132,255);
    if (boarded)
    {
        int row=0;
        for (float y=1.02f;y<height-0.16f;y+=0.18f,++row)
        {
            const int variation=(row*7)%5-2;
            const Color paint=family==StationFacadeFamily::Waterfront
                ? Color(111+variation,144+variation,150+variation,255)
                : Color(174+variation,181+variation,158+variation,255);
            const auto board=[&](float x0,float x1) {
                if (x1<=x0) return;
                const auto p=[&](float x,float py,float z) { return Vector3::Transform({x,py,z},placement); };
                mesh.addQuad(p(x0,y,0.045f),p(x1,y,0.045f),
                             p(x1,y+0.165f,0.020f),p(x0,y+0.165f,0.020f),paint);
            };
            if (y<3.30f)
            {
                board(-length*0.5f,doorX-0.85f);
                board(doorX+0.85f,length*0.5f);
            }
            else board(-length*0.5f,length*0.5f);
        }
        // A modest timber entrance hood and two supports distinguish the small waiting hall.
        mesh.addBox({doorX-1.15f,3.43f,-0.01f},{doorX+1.15f,3.55f,0.76f},trim,placement);
        for (float side : {-1.0f,1.0f})
        {
            const float x=doorX+side*0.97f;
            mesh.addBox({x-0.05f,0.78f,0.59f},{x+0.05f,3.43f,0.69f},trim,placement);
            mesh.addBox({side*length*0.5f-0.06f,0.82f,0.02f},
                        {side*length*0.5f+0.06f,height-0.10f,0.07f},trim,placement);
        }
    }
    else
    {
        const int bays=family==StationFacadeFamily::Urban ? 6 : 4;
        for (int bay=0;bay<=bays;++bay)
        {
            const float x=(static_cast<float>(bay)/static_cast<float>(bays)-0.5f)*length*0.82f;
            if (std::abs(x-doorX)<1.05f) continue;
            mesh.addBox({x-0.12f,0.83f,0.01f},{x+0.12f,height-0.12f,0.13f},trim,placement);
        }
        mesh.addBox({-length*0.5f,3.43f,0.01f},{length*0.5f,3.59f,0.16f},trim,placement);
        for (float side : {-1.0f,1.0f})
        {
            const float x=doorX+side*0.96f;
            mesh.addBox({x-0.09f,0.78f,0.02f},{x+0.09f,3.43f,0.17f},trim,placement);
        }
    }
}

inline void addStationNameBoard(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Matrix& placement,float platformHeight)
{
    using namespace Microsoft::Xna::Framework;
    const Color metal(104,116,117,255);
    for (float post : {-1.12f,1.12f})
    {
        mesh.addBox({post-0.045f,platformHeight,-0.045f},
                    {post+0.045f,2.48f,0.045f},metal,placement);
        mesh.addBox({post-0.12f,platformHeight,-0.12f},
                    {post+0.12f,platformHeight+0.045f,0.12f},metal,placement);
    }
    mesh.addBox({-1.40f,2.21f,-0.06f},{1.40f,2.69f,0.06f},Color(25,71,101,255),placement);
    mesh.addBox({-1.22f,2.255f,0.0605f},{1.22f,2.285f,0.064f},Color(224,185,58,255),placement);
}

struct StationStoreys
{
    int count;
    float base, pitch;
    static constexpr float WindowHeight=1.45f;
    [[nodiscard]] float windowCentre(int floor) const noexcept
    {
        return base+1.65f+static_cast<float>(floor)*pitch;
    }
};

inline StationStoreys stationStoreys(float wallHeight,float platformHeight)
{
    // Rural waiting halls have one tall public floor, not two compressed storeys.
    const int count=wallHeight-platformHeight>=8.0f ? 3 : 1;
    return {count,platformHeight,(wallHeight-platformHeight)/static_cast<float>(count)};
}

inline bool stationWindowClearsEntrance(double windowOffset,double doorOffset,float windowWidth=1.45f)
{
    // Half window including frame + half entrance surround + a visible pier.
    return std::abs(windowOffset-doorOffset)>static_cast<double>(windowWidth)*0.5+0.10+0.81+0.25;
}

// Local +Z faces the platform. The public entrance is scenery, not a player exit.
inline void addStationEntrance(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Matrix& transform)
{
    using namespace Microsoft::Xna::Framework;
    const Color frame(133,143,139,255), leaf(64,76,77,255), glass(48,88,102,255);
    const Color metal(182,187,178,255), recess(34,43,46,255);
    const auto box=[&](Vector3 a,Vector3 b,const Color& colour) {
        mesh.addBox(a,b,colour,transform);
    };
    box({-0.725f,0,-0.035f},{0.725f,2.40f,0},recess);
    for (float side : {-1.0f,1.0f})
    {
        const float centre=side*0.36f;
        box({centre-0.345f,0.035f,0.002f},{centre+0.345f,2.31f,0.025f},leaf);
        box({centre-0.265f,0.91f,0.027f},{centre+0.265f,2.20f,0.037f},glass);
        const auto pane=[&](float x,float y) { return Vector3::Transform({x,y,0.038f},transform); };
        mesh.addQuad(pane(centre-0.265f,0.91f),pane(centre+0.265f,0.91f),
            pane(centre+0.265f,2.20f),pane(centre-0.265f,2.20f),
            Color(53,68,70,255),Color(53,68,70,255),Color(92,112,118,255),Color(92,112,118,255));
        box({centre-0.24f,1.98f,0.0382f},{centre+0.24f,2.005f,0.039f},Color(101,120,119,255));
        box({centre-0.265f,0.20f,0.027f},{centre+0.265f,0.75f,0.040f},frame);
        // Glazing bead and a restrained safety manifestation, not an opaque fake reflection.
        box({centre-0.28f,0.88f,0.039f},{centre+0.28f,0.915f,0.055f},metal);
        box({centre-0.265f,1.43f,0.038f},{centre+0.265f,1.45f,0.045f},frame);
        const float jamb=side*0.76f;
        box({jamb-0.035f,0,0},{jamb+0.035f,2.48f,0.105f},frame);
        const float handle=side*0.115f;
        for (float y : {1.02f,1.38f})
            box({handle-0.023f,y-0.022f,0.025f},{handle+0.023f,y+0.022f,0.105f},metal);
        box({handle-0.018f,1.0f,0.09f},{handle+0.018f,1.40f,0.125f},metal);
    }
    box({-0.795f,2.40f,0},{0.795f,2.48f,0.105f},frame);
    box({-0.81f,0,0},{0.81f,0.035f,0.15f},metal);
}

template<class GroundPoint>
void addTerminalStreet(MeshBuilder& mesh,GroundPoint ground)
{
    using namespace Microsoft::Xna::Framework;
    // Ten-metre patches follow the terrain; two pedestrian strips border a seven-metre road.
    const auto strip=[&](float begin,float end,float x0,float x1,float lift,const Color& colour) {
        mesh.addQuad(ground(begin,x0)+Vector3{0,lift,0},ground(end,x0)+Vector3{0,lift,0},
                     ground(end,x1)+Vector3{0,lift,0},ground(begin,x1)+Vector3{0,lift,0},colour);
    };
    for (int span=0;span<65;++span)
    {
        const float z=-325.0f+static_cast<float>(span)*10.0f;
        strip(z,z+10,-3.5f,3.5f,0.05f,Color(78,83,83,255));
        strip(z,z+10,-6.1f,-3.5f,0.16f,Color(164,166,155,255));
        strip(z,z+10,3.5f,6.1f,0.16f,Color(164,166,155,255));
        strip(z+2,z+5,-0.045f,0.045f,0.064f,Color(214,211,191,255));
        for (float side : {-1.0f,1.0f})
        {
            const float x=side*3.5f;
            const auto a=ground(z,x),b=ground(z+10,x);
            if (side>0)
                mesh.addQuad(a+Vector3{0,0.05f,0},b+Vector3{0,0.05f,0},
                             b+Vector3{0,0.16f,0},a+Vector3{0,0.16f,0},Color(135,141,135,255));
            else
                mesh.addQuad(b+Vector3{0,0.05f,0},a+Vector3{0,0.05f,0},
                             a+Vector3{0,0.16f,0},b+Vector3{0,0.16f,0},Color(135,141,135,255));
        }
    }
}

struct CanopyShadowBounds
{
    float minX, maxX, minZ, maxZ;
};

// Horizontal roof at 3.69 m above rail; input rays and receiver coordinates are canopy-local.
inline CanopyShadowBounds canopyShadowBounds(float length, float receiverHeight,
    const Microsoft::Xna::Framework::Vector3& rays)
{
    if (length <= 0 || receiverHeight >= 3.69f || rays.Y >= -0.001f) return {};
    const float travel=(3.69f-receiverHeight)/-rays.Y;
    const float dx=rays.X*travel, dz=rays.Z*travel;
    return {-2.30f+dx,2.30f+dx,-length*0.5f+dz,length*0.5f+dz};
}

inline float canopyShadeAt(const CanopyShadowBounds& bounds,float x,float z)
{
    const float inside=std::min({x-bounds.minX,bounds.maxX-x,z-bounds.minZ,bounds.maxZ-z});
    // Small receiver-space blend avoids abrupt whole-avatar light changes at a roof edge.
    return std::clamp(inside/0.25f,0.0f,1.0f);
}

// Canopy identity per station family (ENV-131): roof deck, fascia bands, steelwork and soffit
// panels. Suburban keeps the original teal-grey scheme; the terminus has its own train shed.
struct CanopyPalette
{
    Microsoft::Xna::Framework::Color deck{67,91,102,255},fascia{48,66,74,255},fasciaLip{54,65,67,255};
    Microsoft::Xna::Framework::Color steel{113,128,130,255},foot{96,108,109,255};
    Microsoft::Xna::Framework::Color soffit{208,211,205,255},soffitAccent{201,205,198,255};
    Microsoft::Xna::Framework::Color rail{116,129,131,255};
    bool valance=false;   // Timber dagger-board valance along both eaves.
};

inline CanopyPalette canopyPalette(StationFacadeFamily family,bool terminal)
{
    using Microsoft::Xna::Framework::Color;
    CanopyPalette palette;
    if (terminal) return palette;
    switch (family)
    {
    case StationFacadeFamily::Rural:
        palette.deck=Color(52,82,62,255); palette.fascia=Color(214,206,178,255);
        palette.fasciaLip=Color(46,74,56,255); palette.steel=Color(58,96,72,255);
        palette.foot=Color(48,80,60,255); palette.soffit=Color(222,216,196,255);
        palette.soffitAccent=Color(214,206,186,255); palette.rail=Color(58,96,72,255);
        palette.valance=true;
        break;
    case StationFacadeFamily::Waterfront:
        palette.deck=Color(70,104,132,255); palette.fascia=Color(58,98,132,255);
        palette.fasciaLip=Color(214,220,222,255); palette.steel=Color(206,212,212,255);
        palette.foot=Color(170,178,180,255); palette.soffit=Color(226,230,230,255);
        palette.soffitAccent=Color(214,222,224,255); palette.rail=Color(196,204,206,255);
        break;
    case StationFacadeFamily::Urban:
        palette.deck=Color(60,62,66,255); palette.fascia=Color(118,54,48,255);
        palette.fasciaLip=Color(62,64,68,255); palette.steel=Color(74,78,84,255);
        palette.foot=Color(62,66,70,255); palette.soffit=Color(196,198,196,255);
        palette.soffitAccent=Color(184,188,188,255); palette.rail=Color(74,78,84,255);
        break;
    case StationFacadeFamily::Suburban:
        break;
    }
    return palette;
}

// Timber valance boards with pointed ends hang below one eave in canopy-local X/Z (the soffit
// frame). Double-sided flat boards keep it to four triangles each.
inline int addCanopyValance(MeshBuilder& mesh,const Microsoft::Xna::Framework::Matrix& transform,
    float length,float eaveX,const Microsoft::Xna::Framework::Color& colour)
{
    using namespace Microsoft::Xna::Framework;
    const int boards=std::clamp(static_cast<int>(std::floor(length/0.19f)),1,1024);
    const float pitch=length/static_cast<float>(boards);
    const Color shade(static_cast<int>(colour.getRProperty())*86/100,
                      static_cast<int>(colour.getGProperty())*86/100,
                      static_cast<int>(colour.getBProperty())*86/100,255);
    for (int board=0;board<boards;++board)
    {
        const float z=-length*0.5f+(static_cast<float>(board)+0.5f)*pitch;
        const auto p=[&](float y,float dz) { return Vector3::Transform(Vector3{eaveX,y,z+dz},transform); };
        const Vector3 a=p(3.31f,-0.075f),b=p(3.31f,0.075f),c=p(3.10f,0.075f),d=p(3.10f,-0.075f),tip=p(3.0f,0);
        mesh.addQuad(a,b,c,d,colour);
        mesh.addQuad(d,c,b,a,shade);
        mesh.addTriangle(d,c,tip,colour);
        mesh.addTriangle(tip,c,d,shade);
    }
    return boards;
}

inline void addCanopySoffit(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Vector3& base,float length,float yaw,MeshBuilder* finish=nullptr,
    const CanopyPalette& palette={})
{
    using namespace Microsoft::Xna::Framework;
    if (length <= 0) return;
    const Matrix transform=Matrix::CreateRotationY(yaw)*Matrix::CreateTranslation(base);
    const int panels=std::clamp(static_cast<int>(std::ceil(length/1.5f)),1,128);
    const float step=length/static_cast<float>(panels);
    for (int panel=0;panel<panels;++panel)
    {
        const float z0=-length*0.5f+static_cast<float>(panel)*step+0.015f;
        const float z1=z0+step-0.030f;
        for (int strip=0;strip<6;++strip)
        {
            const float x0=-2.19f+static_cast<float>(strip)*0.73f+0.007f;
            const float x1=x0+0.716f;
            const Color colour=(panel+strip)%5==0 ? palette.soffitAccent : palette.soffit;
            const auto a=Vector3::Transform({x0,3.487f,z0},transform);
            const auto b=Vector3::Transform({x1,3.487f,z0},transform);
            const auto c=Vector3::Transform({x1,3.487f,z1},transform);
            const auto d=Vector3::Transform({x0,3.487f,z1},transform);
            if (finish)
                finish->addTexturedQuad(a,b,c,d,colour,{x0/0.5f,z0/0.5f},
                    {x1/0.5f,z0/0.5f},{x1/0.5f,z1/0.5f},{x0/0.5f,z1/0.5f});
            else mesh.addQuad(a,b,c,d,colour);
        }
    }
    // Slim longitudinal support rails leave the central luminaire strip unobstructed.
    for (float x : {-1.65f,1.65f})
        mesh.addBox({x-0.045f,3.37f,-length*0.5f},{x+0.045f,3.48f,length*0.5f},
                    palette.rail,transform);
}

// 600 mm staggered paving with 6 mm joints and small, closed bevels. The existing
// platform surface remains below it as the joint bed; no extra material is needed.
inline int addPlatformPaving(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Matrix& placement,float length,float side,
    const Microsoft::Xna::Framework::Color& base)
{
    using namespace Microsoft::Xna::Framework;
    if (length<3 || length>300 || std::abs(side)!=1) return 0;
    const float start=-length*0.5f+1,end=length*0.5f-1;
    int slabs=0;
    for (int row=0;row<7;++row)
    {
        const float a=side*(-2.5f+0.70f+static_cast<float>(row)*0.60f);
        const float b=a+side*0.60f;
        const float x0=std::min(a,b)+0.003f,x1=std::max(a,b)-0.003f;
        const float shift=row%2==0 ? 0 : -0.30f;
        for (int column=0;column<502;++column)
        {
            const float nominal=start+shift+static_cast<float>(column)*0.60f;
            if (nominal>=end) break;
            const float z0=std::max(start,nominal)+0.003f;
            const float z1=std::min(end,nominal+0.60f)-0.003f;
            if (z1-z0<0.02f) continue;
            const int variation=(row*37+column*13+column/7)%9-4;
            const Color colour(std::clamp(base.getRProperty()+6+variation,0,255),
                               std::clamp(base.getGProperty()+6+variation,0,255),
                               std::clamp(base.getBProperty()+6+variation,0,255),255);
            const std::array<Vector3,4> outer{{{x0,0.003f,z0},{x0,0.003f,z1},
                                              {x1,0.003f,z1},{x1,0.003f,z0}}};
            const std::array<Vector3,4> inner{{{x0+0.008f,0.008f,z0+0.008f},
                {x0+0.008f,0.008f,z1-0.008f},{x1-0.008f,0.008f,z1-0.008f},
                {x1-0.008f,0.008f,z0+0.008f}}};
            const auto quad=[&](const std::array<Vector3,4>& p) {
                mesh.addTexturedQuad(Vector3::Transform(p[0],placement),Vector3::Transform(p[1],placement),
                    Vector3::Transform(p[2],placement),Vector3::Transform(p[3],placement),colour,
                    {p[0].X/0.65f,p[0].Z/0.65f},{p[1].X/0.65f,p[1].Z/0.65f},
                    {p[2].X/0.65f,p[2].Z/0.65f},{p[3].X/0.65f,p[3].Z/0.65f});
            };
            quad(inner);
            for (std::size_t edge=0;edge<4;++edge)
                quad({outer[edge],outer[(edge+1)%4],inner[(edge+1)%4],inner[edge]});
            ++slabs;
        }
    }
    return slabs;
}

// A 400 mm tactile module with six rows of small raised blisters. Relief is
// bounded to the full passenger consist's close viewing range, not every distant tile.
inline int addPlatformTactile(MeshBuilder& structure,MeshBuilder& material,
    const Microsoft::Xna::Framework::Matrix& placement,float length)
{
    using namespace Microsoft::Xna::Framework;
    if (length<3 || length>300) return 0;
    constexpr float pitch=0.4f/6;
    const int tiles=static_cast<int>((length-2)/0.4f);
    const float begin=-static_cast<float>(tiles)*0.20f;
    int blisters=0;
    const auto p=[&](float x,float y,float z) { return Vector3::Transform({x,y,z},placement); };
    for (int tile=0;tile<tiles;++tile)
    {
        const float z=begin+static_cast<float>(tile)*0.4f;
        const int tint=(tile*13)%5-2;
        const Color colour(207+tint,190+tint,121+tint,255);
        material.addTexturedQuad(p(-0.20f,0.010f,z+0.003f),p(-0.20f,0.010f,z+0.397f),
            p(0.20f,0.010f,z+0.397f),p(0.20f,0.010f,z+0.003f),colour,
            {-0.4f,z/0.5f},{-0.4f,(z+0.4f)/0.5f},{0.4f,(z+0.4f)/0.5f},{0.4f,z/0.5f});
        if (std::abs(z+0.2f)>78) continue;
        for (int row=0;row<6;++row)
            for (int column=0;column<6;++column)
            {
                const float x=-0.20f+(static_cast<float>(row)+0.5f)*pitch;
                const float centre=z+(static_cast<float>(column)+0.5f)*pitch;
                const auto top=p(x,0.015f,centre);
                for (int edge=0;edge<6;++edge)
                {
                    const float a=static_cast<float>(edge)*1.047197551f;
                    const float b=static_cast<float>(edge+1)*1.047197551f;
                    structure.addTriangle(top,p(x+0.0135f*std::cos(b),0.010f,centre+0.0135f*std::sin(b)),
                        p(x+0.0135f*std::cos(a),0.010f,centre+0.0135f*std::sin(a)),colour);
                }
                ++blisters;
            }
    }
    return blisters;
}

// Track-facing face of the authored market-town clock turret. The clock surface
// is beyond the 2.2 m wall, not hidden at the centre of the solid tower.
inline void addMarketTurretFace(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Matrix& placement)
{
    using namespace Microsoft::Xna::Framework;
    constexpr float pi=3.141592654f;
    const Color stone(178,173,150,255),metal(69,80,78,255),dark(31,45,47,255);
    const auto box=[&](Vector3 a,Vector3 b,Color colour) { mesh.addBox(a,b,colour,placement); };
    const auto p=[&](float x,float y,float z) { return Vector3::Transform({x,y,z},placement); };
    for (float y : {3.35f,5.55f})
        for (float x : {-0.98f,0.98f})
        {
            box({x-0.43f,y-0.72f,2.205f},{x+0.43f,y+0.72f,2.245f},stone);
            box({x-0.34f,y-0.61f,2.247f},{x+0.34f,y+0.61f,2.253f},dark);
            for (int slat=0;slat<8;++slat)
            {
                const float sy=y-0.52f+static_cast<float>(slat)*0.145f;
                box({x-0.32f,sy,2.253f},{x+0.32f,sy+0.04f,2.29f},metal);
            }
            box({x-0.48f,y-0.78f,2.20f},{x+0.48f,y-0.70f,2.34f},stone);
        }
    for (int segment=0;segment<48;++segment)
    {
        const float a=static_cast<float>(segment)*2*pi/48,b=static_cast<float>(segment+1)*2*pi/48;
        const auto ring=[&](float angle,float radius,float depth) {
            return p(std::cos(angle)*radius,marketTowerClockHeight+std::sin(angle)*radius,depth);
        };
        mesh.addTriangle(p(0,marketTowerClockHeight,marketTowerClockFaceDepth),
            ring(a,0.84f,marketTowerClockFaceDepth),ring(b,0.84f,marketTowerClockFaceDepth),Color(226,223,199,255));
        mesh.addQuad(ring(a,0.84f,marketTowerClockFaceDepth),ring(a,0.96f,2.26f),
            ring(b,0.96f,2.26f),ring(b,0.84f,marketTowerClockFaceDepth),stone);
    }
    for (int tick=0;tick<12;++tick)
    {
        const float angle=static_cast<float>(tick)*2*pi/12;
        const Matrix tickPlacement=Matrix::CreateRotationZ(-angle)
            *Matrix::CreateTranslation(0,marketTowerClockHeight,marketTowerClockFaceDepth+0.009f)*placement;
        mesh.addBox({-0.026f,0.64f,-0.004f},{0.026f,0.77f,0.004f},dark,tickPlacement);
    }
    box({-0.38f,2.36f,2.20f},{0.38f,2.46f,2.45f},metal);
}

inline void addPlatformDepartureBoard(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Matrix& placement,float platformHeight)
{
    using namespace Microsoft::Xna::Framework;
    const Color metal(110,124,123,255),side(59,73,75,255),screen(13,27,32,255);
    const auto outline=[](float x,float y,float corner) {
        return std::array<Vector2,8>{{{-x+corner,-y},{x-corner,-y},{x,-y+corner},{x,y-corner},
            {x-corner,y},{-x+corner,y},{-x,y-corner},{-x,-y+corner}}};
    };
    const auto outer=outline(1.585f,0.58f,0.045f),inner=outline(1.495f,0.515f,0.025f);
    const auto p=[&](Vector2 q,float z) { return Vector3::Transform(
        {q.X,q.Y+platformDisplayCentreHeight,z},placement); };
    for (std::size_t i=0;i<outer.size();++i)
    {
        const std::size_t next=(i+1)%outer.size();
        mesh.addQuad(p(outer[i],0.10f),p(outer[next],0.10f),p(inner[next],0.10f),p(inner[i],0.10f),metal);
        mesh.addQuad(p(outer[i],-0.09f),p(outer[next],-0.09f),p(outer[next],0.10f),p(outer[i],0.10f),side);
        mesh.addTriangle(p({0,0},-0.09f),p(outer[next],-0.09f),p(outer[i],-0.09f),side);
        mesh.addTriangle(p({0,0},0.095f),p(inner[i],0.095f),p(inner[next],0.095f),screen);
    }
    for (float x : {-1.18f,1.18f})
    {
        mesh.addBox({x-0.05f,platformHeight,-0.065f},{x+0.05f,platformDisplayPostTop,0.035f},side,placement);
        mesh.addBox({x-0.13f,platformHeight,-0.14f},{x+0.13f,platformHeight+0.04f,0.12f},metal,placement);
        mesh.addBox({x-0.095f,1.985f,-0.13f},{x+0.095f,2.12f,-0.075f},metal,placement);
        for (float dx : {-0.09f,0.09f})
            for (float dz : {-0.10f,0.08f})
                mesh.addBox({x+dx-0.012f,platformHeight+0.04f,dz-0.012f},
                    {x+dx+0.012f,platformHeight+0.056f,dz+0.012f},side,placement);
    }
    // Flush frame fasteners and a lower service seam remain outside the text rectangle.
    for (float x : {-1.55f,1.55f})
        for (float y : {platformDisplayCentreHeight-0.45f,platformDisplayCentreHeight+0.45f})
            mesh.addBox({x-0.007f,y-0.008f,0.101f},{x+0.007f,y+0.008f,0.105f},side,placement);
}

inline void addPlatformFencePost(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Matrix& placement)
{
    using namespace Microsoft::Xna::Framework;
    mesh.addBox({-0.09f,0.008f,-0.09f},{0.09f,0.042f,0.09f},Color(102,112,108,255),placement);
    mesh.addBox({-0.05f,0.04f,-0.05f},{0.05f,1.30f,0.05f},Color(71,79,78,255),placement);
    mesh.addBox({-0.057f,1.30f,-0.057f},{0.057f,1.325f,0.057f},Color(111,121,116,255),placement);
}

// Local +Z is the passenger-facing opening. The aperture has actual depth, not
// a dark rectangle laid on a solid front panel.
inline void addPlatformBin(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Matrix& placement)
{
    using namespace Microsoft::Xna::Framework;
    const Color paint(77,91,87,255),edge(113,127,121,255),metal(154,163,157,255);
    const Color recess(20,27,27,255),kick(96,109,105,255);
    const auto box=[&](Vector3 a,Vector3 b,const Color& colour) { mesh.addBox(a,b,colour,placement); };
    const auto p=[&](float x,float y,float z) { return Vector3::Transform({x,y,z},placement); };
    box({-0.23f,0.008f,-0.23f},{0.23f,0.065f,0.23f},recess);
    box({-0.26f,0.055f,-0.26f},{0.26f,0.635f,0.26f},paint);
    box({-0.265f,0.07f,0.258f},{0.265f,0.16f,0.278f},kick);
    box({-0.218f,0.18f,0.261f},{0.218f,0.605f,0.271f},recess);
    box({-0.207f,0.19f,0.271f},{0.207f,0.596f,0.279f},paint);
    // Folded side cheeks and the recessed liner enclose the upper cavity.
    for (float side : {-1.0f,1.0f})
    {
        const float x=side*0.235f;
        box({x-0.025f,0.63f,-0.26f},{x+0.025f,0.88f,0.275f},paint);
        box({x-0.008f,0.065f,0.275f},{x+0.008f,0.88f,0.286f},edge);
        box({x-0.014f,0.30f,0.279f},{x+0.014f,0.37f,0.293f},metal);
    }
    box({-0.21f,0.635f,0.025f},{0.21f,0.86f,0.04f},recess);
    box({-0.21f,0.632f,0.04f},{0.21f,0.648f,0.278f},metal);
    box({-0.26f,0.815f,-0.26f},{0.26f,0.875f,0.28f},paint);
    // Sloped lid with a projecting drip edge and closed end faces.
    mesh.addQuad(p(-0.29f,0.96f,-0.29f),p(-0.29f,0.90f,0.30f),
                 p(0.29f,0.90f,0.30f),p(0.29f,0.96f,-0.29f),edge);
    mesh.addQuad(p(-0.29f,0.90f,0.30f),p(-0.29f,0.868f,0.30f),
                 p(0.29f,0.868f,0.30f),p(0.29f,0.90f,0.30f),kick);
    mesh.addQuad(p(0.29f,0.96f,-0.29f),p(0.29f,0.868f,-0.29f),
                 p(-0.29f,0.868f,-0.29f),p(-0.29f,0.96f,-0.29f),kick);
    for (float side : {-1.0f,1.0f})
    {
        const float x=side*0.29f;
        if (side>0) mesh.addQuad(p(x,0.90f,0.30f),p(x,0.868f,0.30f),
            p(x,0.868f,-0.29f),p(x,0.96f,-0.29f),paint);
        else mesh.addQuad(p(x,0.96f,-0.29f),p(x,0.868f,-0.29f),
            p(x,0.868f,0.30f),p(x,0.90f,0.30f),paint);
    }
    box({-0.29f,0.862f,-0.29f},{0.29f,0.868f,0.30f},kick);
    for (float x : {-0.15f,-0.075f,0.0f,0.075f,0.15f})
        box({x-0.003f,0.22f,0.279f},{x+0.003f,0.43f,0.283f},edge);
    box({0.15f,0.52f,0.28f},{0.184f,0.554f,0.287f},metal);
    box({0.165f,0.527f,0.287f},{0.169f,0.547f,0.288f},recess);
}

// Only the diffuser is unlit; housing, gasket and fasteners retain daylight shading.
inline void addCanopyLuminaire(MeshBuilder& housing,MeshBuilder& emitter,
    const Microsoft::Xna::Framework::Matrix& placement)
{
    using namespace Microsoft::Xna::Framework;
    const auto prism=[&](MeshBuilder& target,float halfX,float halfZ,float corner,
                         float bottom,float top,const Color& colour) {
        const std::array<Vector2,8> outline{{
            {-halfX+corner,-halfZ},{halfX-corner,-halfZ},
            {halfX,-halfZ+corner},{halfX,halfZ-corner},
            {halfX-corner,halfZ},{-halfX+corner,halfZ},
            {-halfX,halfZ-corner},{-halfX,-halfZ+corner}}};
        const auto point=[&](Vector2 p,float y) {
            return Vector3::Transform({p.X,y,p.Y},placement);
        };
        for (std::size_t i=0;i<outline.size();++i)
        {
            const auto a=outline[i],b=outline[(i+1)%outline.size()];
            target.addTriangle(point({0,0},bottom),point(a,bottom),point(b,bottom),colour);
            target.addTriangle(point({0,0},top),point(b,top),point(a,top),colour);
            target.addQuad(point(a,bottom),point(a,top),point(b,top),point(b,bottom),colour);
        }
    };
    housing.addBox({-0.60f,3.455f,-0.10f},{0.60f,3.489f,0.10f},Color(126,136,133,255),placement);
    prism(housing,0.775f,0.17f,0.035f,3.37f,3.46f,Color(178,185,177,255));
    prism(housing,0.72f,0.125f,0.025f,3.35f,3.375f,Color(49,59,60,255));
    prism(emitter,0.69f,0.10f,0.022f,3.325f,3.355f,Color(244,239,215,255));
    for (float x : {-0.745f,0.745f})
        for (float z : {-0.105f,0.105f})
            housing.addBox({x-0.011f,3.360f,z-0.012f},{x+0.011f,3.374f,z+0.012f},
                           Color(102,115,115,255),placement);
}

inline void addCanopyFrame(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Matrix& placement,float postX,float platformHeight,
    const CanopyPalette& palette={})
{
    using namespace Microsoft::Xna::Framework;
    const Color steel=palette.steel,foot=palette.foot,bolt(159,164,158,255);
    // I-section beam across the roof, seated on a short head plate and hollow steel column.
    mesh.addBox({-2.19f,3.23f,-0.022f},{2.19f,3.42f,0.022f},steel,placement);
    mesh.addBox({-2.21f,3.20f,-0.11f},{2.21f,3.235f,0.11f},steel,placement);
    mesh.addBox({-2.21f,3.415f,-0.11f},{2.21f,3.45f,0.11f},steel,placement);
    mesh.addBox({postX-0.17f,3.17f,-0.17f},{postX+0.17f,3.20f,0.17f},foot,placement);
    mesh.addBox({postX-0.06f,platformHeight,-0.06f},{postX+0.06f,3.17f,0.06f},steel,placement);
    mesh.addBox({postX-0.16f,platformHeight,-0.16f},
                {postX+0.16f,platformHeight+0.045f,0.16f},foot,placement);
    for (float x : {-0.11f,0.11f})
        for (float z : {-0.11f,0.11f})
            mesh.addBox({postX+x-0.018f,platformHeight+0.045f,z-0.018f},
                        {postX+x+0.018f,platformHeight+0.066f,z+0.018f},bolt,placement);
}

} // namespace TrainSim::Render
