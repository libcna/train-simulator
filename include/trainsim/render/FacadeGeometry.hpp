#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>

namespace TrainSim::Render {

struct ResidentialEnvelope
{
    float width, length, wallHeight, roofWidth, roofLength, roofHeight;
};

// Local X spans the entrance, Y starts at ground, Z faces out of either facade.
// The original slab remains behind this kit; both banks use the same bounded geometry.
inline void addResidentialDoorKit(MeshBuilder& mesh,float width,float height,
    const Microsoft::Xna::Framework::Matrix& placement,bool communal=false)
{
    using namespace Microsoft::Xna::Framework;
    if (width<0.6f || height<1.2f) return;
    const float hw=width*0.5f;
    const Color frame(155,151,137,255),panel(91,85,72,255),metal(151,157,153,255);
    for (float side : {-1.0f,1.0f})
        mesh.addBox({side*hw-0.055f,0,-0.11f},{side*hw+0.055f,height,0.11f},frame,placement);
    mesh.addBox({-hw-0.055f,height,-0.11f},{hw+0.055f,height+0.08f,0.11f},frame,placement);
    mesh.addBox({-hw-0.09f,0.01f,-0.24f},{hw+0.09f,0.06f,0.24f},
        Color(138,137,127,255),placement);
    for (float side : {-1.0f,1.0f})
    {
        const auto box=[&](float x0,float y0,float x1,float y1,float near,float far,Color c) {
            mesh.addBox({x0,y0,std::min(side*near,side*far)},
                        {x1,y1,std::max(side*near,side*far)},c,placement);
        };
        // Raised panel borders frame inset dark panels; communal doors have a glazed upper light.
        box(-hw+0.12f,height*0.12f,hw-0.12f,height*0.39f,0.066f,0.082f,panel);
        box(-hw+0.15f,height*0.14f,hw-0.15f,height*0.37f,0.082f,0.086f,
            Color(61,58,49,255));
        box(-hw+0.12f,height*0.58f,hw-0.12f,height*0.91f,0.066f,0.082f,panel);
        box(-hw+0.15f,height*0.60f,hw-0.15f,height*0.89f,0.082f,0.086f,
            communal ? Color(57,79,85,255) : Color(61,58,49,255));
        const float handleX=hw-0.13f,handleY=height*0.48f;
        box(handleX-0.025f,handleY-0.065f,handleX+0.025f,handleY+0.065f,0.068f,0.09f,metal);
        box(handleX-0.12f,handleY-0.012f,handleX+0.018f,handleY+0.018f,0.09f,0.14f,metal);
        if (!communal)
            box(-width*0.16f,height*0.52f,width*0.16f,height*0.55f,0.068f,0.089f,metal);
    }
}

// Extend the street-facing finish around both exposed ends. Physical UV height matches
// the longitudinal facade, so brick courses do not change scale at building corners.
inline void addFacadeEndMaterials(MeshBuilder& target,
    const Microsoft::Xna::Framework::Matrix& transform,float width,float length,
    float lowerHeight,float upperHeight,const Microsoft::Xna::Framework::Color& colour)
{
    using namespace Microsoft::Xna::Framework;
    if (width<=0 || length<=0 || upperHeight<=lowerHeight) return;
    constexpr float tileMetres=2.0f;
    for (float side : {-1.0f,1.0f})
    {
        const float z=side*length*0.5f;
        const float x=side*width*0.5f;
        const auto p=[&](float px,float py) { return Vector3::Transform({px,py,z},transform); };
        target.addTexturedQuad(p(-x,lowerHeight),p(x,lowerHeight),
            p(x,upperHeight),p(-x,upperHeight),colour,
            {-x/tileMetres,lowerHeight/tileMetres},{x/tileMetres,lowerHeight/tileMetres},
            {x/tileMetres,upperHeight/tileMetres},{-x/tileMetres,upperHeight/tileMetres});
    }
}

inline constexpr int apartmentFloorCount(unsigned variant) noexcept
{
    return 5+static_cast<int>(variant%4U);
}

// Urban block identity (ENV-129): render palette, balcony rhythm and plinth tone from a stable
// route-anchored seed instead of three near-identical grey-blue facades.
struct BlockStyle
{
    Microsoft::Xna::Framework::Color facade{164,154,141,255};
    Microsoft::Xna::Framework::Color plinth{112,108,100,255};
    Microsoft::Xna::Framework::Color railing{62,70,74,255};
    int balconyPattern=0;   // 0 none, 1 inner bays, 2 outer bays, 3 every bay.
};

inline BlockStyle blockStyle(std::uint32_t seed)
{
    using Microsoft::Xna::Framework::Color;
    static const std::array<Color,8> facades={Color(196,178,150,255),Color(214,206,188,255),
        Color(178,124,100,255),Color(162,172,152,255),Color(188,186,178,255),
        Color(156,104,86,255),Color(206,182,124,255),Color(151,164,166,255)};
    static const std::array<Color,3> plinths={Color(104,100,94,255),Color(122,112,98,255),
        Color(88,90,92,255)};
    static const std::array<Color,3> railings={Color(62,70,74,255),Color(150,154,150,255),
        Color(86,112,120,255)};
    BlockStyle style;
    style.facade=facades[seed%facades.size()];
    style.plinth=plinths[(seed>>5)%plinths.size()];
    style.railing=railings[(seed>>8)%railings.size()];
    style.balconyPattern=static_cast<int>((seed>>11)%4U);
    return style;
}

[[nodiscard]] inline bool blockBayHasBalcony(int pattern,double bay) noexcept
{
    const bool inner=std::abs(bay)<2.0;
    return pattern==3 || (pattern==1 && inner) || (pattern==2 && !inner);
}

inline ResidentialEnvelope apartmentEnvelope(float scale,unsigned variant)
{
    const float wall=(13.0f+static_cast<float>(variant%4U)*2.2f)*scale;
    // Existing slab thickness is a fixed half metre, independent of the building footprint scale.
    return {10.0f*scale,13.0f*scale,wall,10.5f*scale,13.5f*scale,wall+0.5f};
}

// A restrained gravel/membrane finish with coping, roll joints, drainage and one vent.
// Callers reuse an existing neutral detail batch instead of adding a rooftop material draw.
inline void addFlatRoofFinish(MeshBuilder& structure,MeshBuilder& surface,
    float x,float top,float z,float width,float length,float yaw,unsigned seed)
{
    using namespace Microsoft::Xna::Framework;
    if (width<2 || length<2) return;
    const Matrix placement=Matrix::CreateRotationY(yaw)*Matrix::CreateTranslation(x,top,z);
    const float hw=width*0.5f,hl=length*0.5f;
    const auto p=[&](float px,float py,float pz) { return Vector3::Transform({px,py,pz},placement); };
    const Color gravel(116+static_cast<int>(seed%3U)*5,119,111,255);
    surface.addTexturedQuad(p(-hw+0.17f,0.012f,-hl+0.17f),p(-hw+0.17f,0.012f,hl-0.17f),
        p(hw-0.17f,0.012f,hl-0.17f),p(hw-0.17f,0.012f,-hl+0.17f),gravel,
        {0,0},{0,length/0.65f},{width/0.65f,length/0.65f},{width/0.65f,0});
    const Color coping(137,140,134,255),joint(76,80,76,255),metal(106,113,112,255);
    for (float side : {-1.0f,1.0f})
    {
        structure.addBox({side*hw-0.09f,-0.04f,-hl-0.09f},
                         {side*hw+0.09f,0.16f,hl+0.09f},coping,placement);
        structure.addBox({-hw+0.09f,-0.04f,side*hl-0.09f},
                         {hw-0.09f,0.16f,side*hl+0.09f},coping,placement);
    }
    const int rolls=std::min(24,static_cast<int>(width/1.1f));
    for (int roll=1;roll<rolls;++roll)
    {
        const float across=-hw+width*static_cast<float>(roll)/static_cast<float>(rolls);
        structure.addQuad(p(across-0.007f,0.018f,-hl+0.19f),p(across-0.007f,0.018f,hl-0.19f),
                          p(across+0.007f,0.018f,hl-0.19f),p(across+0.007f,0.018f,-hl+0.19f),joint);
    }
    for (float side : {-1.0f,1.0f})
        structure.addBox({side*(hw-0.5f)-0.13f,0.02f,-hl+0.35f},
                         {side*(hw-0.5f)+0.13f,0.045f,-hl+0.61f},joint,placement);
    const float vx=width*0.22f,vz=-length*0.22f;
    structure.addBox({vx-0.4f,0.012f,vz-0.4f},{vx+0.4f,0.17f,vz+0.4f},joint,placement);
    structure.addBox({vx-0.28f,0.17f,vz-0.28f},{vx+0.28f,0.72f,vz+0.28f},metal,placement);
    structure.addBox({vx-0.36f,0.72f,vz-0.36f},{vx+0.36f,0.79f,vz+0.36f},coping,placement);
    for (float side : {-1.0f,1.0f})
        for (int slat=0;slat<3;++slat)
        {
            const float y=0.29f+static_cast<float>(slat)*0.13f;
            structure.addBox({vx-0.22f,y,vz+side*0.285f-0.01f},
                             {vx+0.22f,y+0.027f,vz+side*0.285f+0.01f},joint,placement);
        }
}

inline void addDomesticRainwaterGoods(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Matrix& placement,
    float roofWidth,float length,float wallWidth,float wallHeight)
{
    using namespace Microsoft::Xna::Framework;
    if (roofWidth<=wallWidth || wallWidth<=0 || length<1 || wallHeight<1) return;
    constexpr int sides=8;
    constexpr float pi=3.141592654f;
    const float hl=length*0.5f;
    const Color metal(89,96,94,255),inside(48,57,56,255),bracket(65,73,72,255);
    const auto p=[&](Vector3 v) { return Vector3::Transform(v,placement); };
    for (float side : {-1.0f,1.0f})
    {
        const float gx=side*(roofWidth*0.5f+0.025f);
        const auto trough=[&](int index,float along,bool inner) {
            const float angle=pi*static_cast<float>(index)/sides;
            return p({gx+(inner ? 0.079f : 0.085f)*std::cos(angle),
                      -0.03f-(inner ? 0.094f : 0.10f)*std::sin(angle),along});
        };
        for (int i=0;i<sides;++i)
        {
            mesh.addQuad(trough(i,-hl,false),trough(i,hl,false),
                         trough(i+1,hl,false),trough(i+1,-hl,false),metal);
            mesh.addQuad(trough(i+1,-hl,true),trough(i+1,hl,true),
                         trough(i,hl,true),trough(i,-hl,true),inside);
            for (float end : {-hl,hl})
                if (end<0)
                    mesh.addQuad(trough(i,end,false),trough(i+1,end,false),
                                 trough(i+1,end,true),trough(i,end,true),metal);
                else
                    mesh.addQuad(trough(i,end,true),trough(i+1,end,true),
                                 trough(i+1,end,false),trough(i,end,false),metal);
        }
        for (int edge : {0,sides})
            if (edge==0)
                mesh.addQuad(trough(edge,-hl,false),trough(edge,-hl,true),
                             trough(edge,hl,true),trough(edge,hl,false),metal);
            else
                mesh.addQuad(trough(edge,-hl,true),trough(edge,-hl,false),
                             trough(edge,hl,false),trough(edge,hl,true),metal);
        const int supports=std::clamp(static_cast<int>(length/1.5f),2,12);
        for (int i=0;i<supports;++i)
        {
            const float z=-hl+(static_cast<float>(i)+0.5f)*length/static_cast<float>(supports);
            mesh.addBox({gx-0.09f,-0.15f,z-0.022f},{gx+0.09f,-0.125f,z+0.022f},bracket,placement);
        }
        const float wallX=side*(wallWidth*0.5f+0.08f),z=hl-0.45f;
        const std::array<Vector3,5> path={Vector3{gx,-0.10f,z},{gx,-0.28f,z},
            {wallX,-0.58f,z},{wallX,-wallHeight+0.25f,z},
            {wallX+side*0.20f,-wallHeight+0.11f,z}};
        std::array<std::array<Vector3,sides>,5> rings;
        for (std::size_t ring=0;ring<path.size();++ring)
        {
            auto tangent=path[std::min(ring+1,path.size()-1)]-path[ring==0 ? 0 : ring-1];
            tangent.Normalize();
            auto across=Vector3::Cross(tangent,{0,0,1});
            across.Normalize();
            const auto depth=Vector3::Cross(tangent,across);
            for (int i=0;i<sides;++i)
            {
                const float angle=2*pi*static_cast<float>(i)/sides;
                rings[ring][i]=p(path[ring]+(across*std::cos(angle)+depth*std::sin(angle))*0.053f);
            }
        }
        for (std::size_t ring=0;ring+1<path.size();++ring)
            for (int i=0;i<sides;++i)
                mesh.addQuad(rings[ring][i],rings[ring][(i+1)%sides],
                             rings[ring+1][(i+1)%sides],rings[ring+1][i],metal);
        for (int i=0;i<sides;++i)
        {
            mesh.addTriangle(p(path.front()),rings.front()[(i+1)%sides],rings.front()[i],inside);
            mesh.addTriangle(p(path.back()),rings.back()[i],rings.back()[(i+1)%sides],inside);
        }
        for (float fraction : {0.25f,0.55f,0.82f})
        {
            const float y=-wallHeight*fraction;
            mesh.addBox({wallX-0.073f,y-0.035f,z-0.071f},
                         {wallX+0.073f,y+0.035f,z+0.071f},bracket,placement);
        }
    }
}

inline void addPitchedRoof(MeshBuilder& walls,MeshBuilder& roof,
    float x,float eaveY,float z,float width,float length,float rise,float yaw,
    const Microsoft::Xna::Framework::Color& colour,float wallWidth=0,float wallHeight=0)
{
    using namespace Microsoft::Xna::Framework;
    if (width<=0 || length<=0 || rise<0) return;
    const Matrix transform=Matrix::CreateRotationY(yaw)*Matrix::CreateTranslation(x,eaveY,z);
    const float hw=width*0.5f, hl=length*0.5f;
    const auto p=[&](float px,float py,float pz) { return Vector3::Transform({px,py,pz},transform); };
    const auto a=p(-hw,0,-hl), b=p(-hw,0,hl), c=p(0,rise,-hl), d=p(0,rise,hl);
    const auto e=p(hw,0,-hl), f=p(hw,0,hl);
    // Texture covers four 0.30 m columns and four 0.40 m courses. Use slope length, not
    // horizontal width, so tile size stays physical when roof pitch changes.
    const float u=length/1.2f, v=std::hypot(hw,rise)/1.6f;
    roof.addTexturedQuad(a,b,d,c,colour,{0,0},{u,0},{u,v},{0,v});
    roof.addTexturedQuad(c,d,f,e,colour,{0,v},{u,v},{u,0},{0,0});
    walls.addTriangle(a,c,e,Color(147,121,99,255));
    walls.addTriangle(b,f,d,Color(126,101,84,255));
    // Six narrow cap strips form a rounded ridge. Its shoulders penetrate the roof slightly,
    // while the crown sits 80 mm above it, avoiding coplanar flicker at every authored pitch.
    constexpr int capSegments=6;
    constexpr float radius=0.18f;
    const auto cap=[&](int step,float along) {
        const float angle=3.141592654f*static_cast<float>(step)/capSegments;
        const float across=-radius*std::cos(angle);
        return p(across,rise-std::abs(across)*rise/hw+0.10f*std::sin(angle)-0.02f,along);
    };
    for (int segment=0;segment<capSegments;++segment)
    {
        const float t0=static_cast<float>(segment)/capSegments*0.25f;
        const float t1=static_cast<float>(segment+1)/capSegments*0.25f;
        roof.addTexturedQuad(cap(segment,-hl),cap(segment,hl),
            cap(segment+1,hl),cap(segment+1,-hl),colour,{0,t0},{u,t0},{u,t1},{0,t1});
    }
    // Even Low needs a closed underside: this is a construction gap, not optional detail.
    walls.addBox({-hw,-0.055f,-hl},{hw,0.015f,hl},Color(151,153,145,255),transform);
    // Slim continuous metal gutters cover the eave edges, not the sloping tile surface.
    if (wallWidth>0 && wallHeight>0)
    {
        addDomesticRainwaterGoods(walls,transform,width,length,wallWidth,wallHeight);
    }
    else for (float side : {-1.0f,1.0f})
    {
        const float gx=side*(hw+0.025f);
        walls.addBox({gx-0.075f,-0.13f,-hl},{gx+0.075f,-0.01f,hl},
                     Color(76,80,78,255),transform);
    }
}

inline ResidentialEnvelope houseEnvelope(float scale)
{
    return {7*scale,10*scale,6*scale,7.7f*scale,10.8f*scale,8.15f*scale};
}

// Detached house identity (ENV-127): facade and roof palettes, storeys, door bay, chimneys,
// porch and roof pitch come from a stable route-anchored seed, so the one house model no
// longer repeats along the whole line. The outbuilding kind is decided by the world feature.
struct HouseStyle
{
    Microsoft::Xna::Framework::Color facade{188,166,135,255};
    Microsoft::Xna::Framework::Color roof{121,63,54,255};
    bool bungalow=false;
    int doorBay=0;        // -1, 0 or +1 of the three facade bays.
    int chimneys=1;       // 0 none, 1 right of ridge centre, 2 left, 3 both gable ends.
    bool porch=false;
    float pitch=1.0f;     // Roof rise multiplier.
    bool barn=false;      // Outbuilding kind when present: barn, otherwise a garage.
};

inline HouseStyle houseStyle(std::uint32_t seed)
{
    using Microsoft::Xna::Framework::Color;
    static const std::array<Color,7> facades={Color(188,166,135,255),Color(214,210,198,255),
        Color(172,174,168,255),Color(196,162,108,255),Color(190,144,120,255),
        Color(160,153,140,255),Color(150,92,72,255)};
    static const std::array<Color,5> roofs={Color(121,63,54,255),Color(96,66,52,255),
        Color(72,77,84,255),Color(56,58,62,255),Color(148,84,60,255)};
    HouseStyle style;
    style.facade=facades[seed%facades.size()];
    style.roof=roofs[(seed>>4)%roofs.size()];
    style.bungalow=(seed>>8)%5U==0U;
    style.doorBay=static_cast<int>((seed>>11)%3U)-1;
    style.chimneys=static_cast<int>((seed>>14)%4U);
    style.porch=(seed>>17)%3U==0U;
    style.pitch=0.82f+static_cast<float>((seed>>20)%5U)*0.11f;
    style.barn=(seed>>24)%2U==0U;
    return style;
}

inline ResidentialEnvelope houseEnvelope(float scale,const HouseStyle& style)
{
    const float wall=(style.bungalow ? 3.3f : 6.0f)*scale;
    const float rise=(style.bungalow ? 2.5f : 2.15f)*style.pitch*scale;
    return {7*scale,10*scale,wall,7.7f*scale,10.8f*scale,wall+rise};
}

// Detached barn (dark stained boarding, wide doors) or rendered garage (up-and-over door) in a
// local frame: X across the route with +X away from the railway, Y up, Z along the route.
inline void addRuralOutbuilding(MeshBuilder& walls,MeshBuilder& roof,
    const Microsoft::Xna::Framework::Matrix& placement,float scale,bool barn,
    const Microsoft::Xna::Framework::Color& render,const Microsoft::Xna::Framework::Color& roofColour)
{
    using namespace Microsoft::Xna::Framework;
    const float hw=(barn ? 3.2f : 2.6f)*scale,hl=(barn ? 3.7f : 3.0f)*scale;
    const float wall=(barn ? 3.4f : 2.5f)*scale;
    const Color body=barn ? Color(92,58,44,255) : render;
    walls.addBox({-hw,0,-hl},{hw,wall,hl},body,placement);
    const Color dark(46,40,34,255),trim(196,194,184,255);
    if (barn)
    {
        // Vertical batten shadows on the track-facing side and a pair of boarded doors.
        for (float z=-hl+0.45f*scale;z<hl-0.2f*scale;z+=0.9f*scale)
            walls.addBox({-hw-0.025f,0.1f,z-0.035f},{-hw,wall-0.1f,z+0.035f},Color(70,44,34,255),
                         placement);
        walls.addBox({-hw-0.04f,0,-1.3f*scale},{-hw,2.7f*scale,1.3f*scale},Color(78,52,40,255),placement);
        walls.addBox({-hw-0.06f,0,-0.03f*scale},{-hw-0.02f,2.7f*scale,0.03f*scale},dark,placement);
        walls.addBox({-hw-0.06f,1.2f*scale,-1.25f*scale},{-hw-0.02f,1.3f*scale,1.25f*scale},dark,placement);
    }
    else
    {
        walls.addBox({-hw-0.03f,0,-1.25f*scale},{-hw,2.1f*scale,1.25f*scale},Color(118,124,126,255),
                     placement);
        for (float y=0.35f;y<2.05f;y+=0.35f)
            walls.addBox({-hw-0.04f,y*scale-0.01f,-1.2f*scale},{-hw-0.03f,y*scale+0.01f,1.2f*scale},
                         Color(96,102,104,255),placement);
        walls.addBox({-hw-0.05f,2.1f*scale,-1.35f*scale},{-hw,2.2f*scale,1.35f*scale},trim,placement);
    }
    walls.addBox({-hw-0.02f,-0.15f,-hl-0.02f},{hw+0.02f,0.12f*scale,hl+0.02f},Color(98,96,90,255),placement);
    // Gable roof running along Z, with closed gable triangles.
    const float rise=(barn ? 2.4f : 1.3f)*scale,eave=wall,over=0.25f*scale;
    const auto p=[&](float x,float y,float z) { return Vector3::Transform(Vector3{x,y,z},placement); };
    const Vector3 a=p(-hw-over,eave-0.05f,-hl-over),b=p(-hw-over,eave-0.05f,hl+over);
    const Vector3 c=p(0,eave+rise,-hl-over),d=p(0,eave+rise,hl+over);
    const Vector3 e=p(hw+over,eave-0.05f,-hl-over),f=p(hw+over,eave-0.05f,hl+over);
    roof.addQuad(a,b,d,c,roofColour);
    roof.addQuad(f,e,c,d,roofColour);
    roof.addQuad(c,d,b,a,Color(60,58,54,255));
    roof.addQuad(d,c,e,f,Color(60,58,54,255));
    for (float z : {-hl,hl})
    {
        const Vector3 l=p(-hw,eave,z),r=p(hw,eave,z),top=p(0,eave+rise-0.05f,z);
        walls.addTriangle(l,r,top,body);
        walls.addTriangle(top,r,l,body);
    }
}

inline ResidentialEnvelope townhouseEnvelope(float scale,unsigned variant)
{
    const float wall=(9.2f+static_cast<float>(variant%3U)*1.35f)*scale;
    return {6*scale,9.2f*scale,wall,6.25f*scale,9.45f*scale,wall+0.44f*scale};
}

struct TownhouseStoreys
{
    int count;
    float pitch;
    float windowHeight;
    [[nodiscard]] float windowCentre(int floor) const noexcept
    {
        return (static_cast<float>(floor)+0.60f)*pitch;
    }
};

inline TownhouseStoreys townhouseStoreys(float scale,unsigned variant)
{
    const int count=variant%3U==0U ? 3 : 4;
    return {count,townhouseEnvelope(scale,variant).wallHeight/static_cast<float>(count),1.35f*scale};
}

// Opaque, depth-inset room cues: no per-window transparency sorting or interior meshes.
inline void addResidentialGlazing(MeshBuilder& mesh,float width,float height,
    const Microsoft::Xna::Framework::Matrix& transform,unsigned seed)
{
    using namespace Microsoft::Xna::Framework;
    if (width<=0 || height<=0) return;
    const unsigned variant=(seed^(seed>>7U))%4U;
    const Color lower(26+static_cast<int>(variant)*3,39+static_cast<int>(variant)*2,44,255);
    const Color upper(63+static_cast<int>(variant)*4,83+static_cast<int>(variant)*2,94,255);
    for (float side : {-1.0f,1.0f})
    {
        const auto rectangle=[&](float x0,float y0,float x1,float y1,float depth,
                                 Color bottom,Color top) {
            const auto p=[&](float x,float y) {
                return Vector3::Transform({x*width,y*height,side*depth},transform);
            };
            if (side>0) mesh.addQuad(p(x0,y0),p(x1,y0),p(x1,y1),p(x0,y1),bottom,bottom,top,top);
            else mesh.addQuad(p(x1,y0),p(x0,y0),p(x0,y1),p(x1,y1),bottom,bottom,top,top);
        };
        rectangle(-0.5f,-0.5f,0.5f,0.5f,0.0675f,lower,upper);
        const Color fabric(88+static_cast<int>(variant)*5,96+static_cast<int>(variant)*3,91,255);
        if (variant==1U)
        {
            rectangle(-0.47f,0.02f,0.47f,0.48f,0.068f,fabric,Color(119,124,115,255));
            for (float y : {0.15f,0.31f})
                rectangle(-0.47f,y,0.47f,y+0.014f,0.069f,Color(78,87,83,255),Color(78,87,83,255));
        }
        else if (variant>=2U)
        {
            rectangle(0.25f,-0.47f,0.47f,0.48f,0.068f,fabric,Color(126,128,114,255));
            if (variant==2U)
                rectangle(-0.47f,-0.47f,-0.28f,0.48f,0.068f,fabric,Color(126,128,114,255));
        }
        if (variant!=1U)
            rectangle(-0.43f,0.28f,0.43f,0.32f,0.069f,Color(83,103,112,255),Color(86,106,115,255));
    }
}

// Four solid rails leave the pane opening empty, with 42.5 mm of reveal in front of the
// existing 135 mm-deep opaque pane. The frame works from either side of a facade.
inline void addRaisedWindowFrame(MeshBuilder& mesh,float paneWidth,float paneHeight,
    const Microsoft::Xna::Framework::Matrix& transform,
    const Microsoft::Xna::Framework::Color& colour)
{
    using namespace Microsoft::Xna::Framework;
    if (paneWidth<=0.2f || paneHeight<=0.2f) return;
    const float x=paneWidth*0.5f+0.10f, y=paneHeight*0.5f+0.09f;
    constexpr float rail=0.10f, depth=0.11f;
    mesh.addBox({-x,-y,-depth},{-x+rail,y,depth},colour,transform);
    mesh.addBox({x-rail,-y,-depth},{x,y,depth},colour,transform);
    mesh.addBox({-x+rail,-y,-depth},{x-rail,-y+rail,depth},colour,transform);
    mesh.addBox({-x+rail,y-rail,-depth},{x-rail,y,depth},colour,transform);
}

// Bounded facade-mounted planter, mirrored toward the track on either side.
inline void addWindowPlanter(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Vector3& base,float sillBottom,float scale,float yaw,float side)
{
    using namespace Microsoft::Xna::Framework;
    if (scale<=0) return;
    const Matrix transform=Matrix::CreateScale(scale)
        *Matrix::CreateRotationY(yaw)*Matrix::CreateTranslation(base+Vector3::Up*sillBottom);
    const float back=side>0 ? -0.06f : -0.30f;
    const float front=side>0 ? 0.30f : 0.06f;
    // The back overlaps the facade; the top meets the underside of the actual window sill.
    mesh.addBox({back,-0.30f,-0.56f},{front,0,0.56f},Color(91,85,73,255),transform);
    mesh.addBox({back+0.035f,0.002f,-0.515f},{front-0.035f,0.018f,0.515f},
                Color(62,53,40,255),transform);
    mesh.addBox({back+0.06f,0.018f,-0.49f},{front-0.06f,0.10f,0.49f},
                Color(59,88,45,255),transform);
}

// Front garden stays inside the 8.6 x 11.4 m townhouse parcel. A clear opening follows
// the same alternating entrance offset as the facade and leaves the whole path unobstructed.
inline void addTownhouseFrontGarden(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Vector3& base,float scale,float yaw,float outerSide,unsigned variant)
{
    using namespace Microsoft::Xna::Framework;
    if (scale<=0) return;
    const Matrix transform=Matrix::CreateScale(scale)*Matrix::CreateRotationY(yaw)
        *Matrix::CreateTranslation(base+Vector3::Up*0.064f);
    const float door=variant%2U==0U ? -1.55f : 1.55f;
    const auto box=[&](float outward0,float outward1,float y0,float y1,float z0,float z1,const Color& colour) {
        const float a=-outerSide*outward0,b=-outerSide*outward1;
        mesh.addBox({std::min(a,b),y0-0.064f,z0},{std::max(a,b),y1-0.064f,z1},colour,transform);
    };
    const Color brick=variant%2U==0U ? Color(120,101,83,255) : Color(113,112,102,255);
    const Color coping(151,149,134,255), iron(47,55,51,255), soil(87,78,60,255);
    for (int section=0;section<2;++section)
    {
        const float z0=section==0 ? -5.22f : door+0.92f;
        const float z1=section==0 ? door-0.92f : 5.22f;
        box(4.02f,4.26f,0.064f,0.38f,z0,z1,brick);
        box(3.99f,4.29f,0.38f,0.44f,z0,z1,coping);
        box(3.40f,3.91f,0.065f,0.081f,z0+0.06f,z1-0.06f,soil);
        box(4.115f,4.155f,0.91f,0.96f,z0,z1,iron);
        const int bars=static_cast<int>(std::ceil((z1-z0)/0.65f));
        for (int i=0;i<=bars;++i)
        {
            const float z=z0+0.035f+(z1-z0-0.07f)*static_cast<float>(i)/static_cast<float>(bars);
            box(4.115f,4.155f,0.44f,1.00f,z-0.022f,z+0.022f,iron);
        }
        // Low clipped planting with gaps and alternating heights avoids one continuous green bar.
        const int plants=static_cast<int>((z1-z0)/1.2f);
        for (int i=0;i<plants;++i)
        {
            const float z=z0+(static_cast<float>(i)+0.5f)*(z1-z0)/static_cast<float>(plants);
            box(3.44f,3.88f,0.081f,0.31f+0.06f*static_cast<float>((i+variant)%2U),
                z-0.36f,z+0.36f,Color(64,86,48,255));
        }
    }
}

// Thin opaque facade layers for medium-distance buildings; no extra draw or glass sorting.
inline void addTerminalFrontage(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Vector3& base,float width,float length,float yaw,float side)
{
    using namespace Microsoft::Xna::Framework;
    if (width < 4 || length < 4) return;
    const Matrix transform=Matrix::CreateRotationY(yaw)*Matrix::CreateTranslation(base);
    const float face=-side*width*0.5f;
    const auto box=[&](float inward0,float inward1,float y0,float y1,float z0,float z1,
                       const Color& colour) {
        const float a=face-side*inward0,b=face-side*inward1;
        mesh.addBox({std::min(a,b),y0,z0},{std::max(a,b),y1,z1},colour,transform);
    };
    // A bounded raised entrance terrace shares the building datum. Its buried foundation
    // tolerates the coarse distant terrain without a floating thin pavement decal.
    box(-0.08f,6.0f,-1.5f,0.06f,-length*0.5f-2,length*0.5f+2,Color(151,155,148,255));
    box(5.82f,6.0f,0.06f,0.16f,-length*0.5f-2,length*0.5f+2,Color(191,194,184,255));
    for (float z=-length*0.5f;z<=length*0.5f;z+=3.0f)
        box(0.05f,5.82f,0.061f,0.065f,z-0.018f,z+0.018f,Color(115,123,118,255));
    // Raised planted ends leave the middle entrance and pedestrian strip unobstructed.
    for (float end : {-1.0f,1.0f})
    {
        const float z=end*(length*0.5f-2.0f);
        box(3.45f,5.3f,0.06f,0.48f,z-1.65f,z+1.65f,Color(113,123,116,255));
        box(3.56f,5.19f,0.48f,0.50f,z-1.54f,z+1.54f,Color(70,66,49,255));
        for (float along : {-0.95f,0.0f,0.95f})
            mesh.addFoliageLobe(Vector3::Transform({face-side*4.37f,0.75f,z+along},transform),
                               {0.68f,0.45f,0.73f},6,3,Color(57,91,52,255),true);
    }
}

inline void addTerminalSkylineFacade(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Vector3& base,float width,float height,float length,
    float yaw,float trackSide)
{
    using namespace Microsoft::Xna::Framework;
    if (width < 4 || length < 4 || height < 4) return;
    const Matrix transform=Matrix::CreateRotationY(yaw)*Matrix::CreateTranslation(base);
    const auto facade=[&](Vector3 origin,Vector3 tangent,Vector3 outward,float span,bool entrance) {
        const auto panel=[&](float u,float y,float w,float h,float offset,const Color& colour) {
            const Vector3 centre=origin+tangent*u+Vector3::Up*y+outward*offset;
            mesh.addQuad(Vector3::Transform(centre-tangent*(w*0.5f)-Vector3::Up*(h*0.5f),transform),
                         Vector3::Transform(centre+tangent*(w*0.5f)-Vector3::Up*(h*0.5f),transform),
                         Vector3::Transform(centre+tangent*(w*0.5f)+Vector3::Up*(h*0.5f),transform),
                         Vector3::Transform(centre-tangent*(w*0.5f)+Vector3::Up*(h*0.5f),transform),colour);
        };
        const int columns=std::clamp(static_cast<int>(span/3.8f),1,16);
        const int floors=std::clamp(static_cast<int>((height-1.0f)/3.1f),1,8);
        for (int floor=0;floor<floors;++floor)
            for (int column=0;column<columns;++column)
            {
                const float u=(static_cast<float>(column)+0.5f)*span/static_cast<float>(columns)-span*0.5f;
                const bool door=entrance && floor == 0 && column == columns/2;
                const float y=door ? 1.1f : 2.0f+static_cast<float>(floor)*3.1f;
                const float h=door ? 2.2f : 1.35f;
                panel(u,y,1.85f,h+0.14f,0.025f,Color(168,177,176,255));
                panel(u,y,1.61f,h,0.045f,(floor+column)%4 == 0
                    ? Color(92,115,124,255) : Color(49,72,84,255));
            }
    };
    facade({-trackSide*width*0.5f,0,0},{0,0,trackSide},{-trackSide,0,0},length,true);
    facade({0,0,length*0.5f},{1,0,0},{0,0,1},width,false);
    facade({0,0,-length*0.5f},{-1,0,0},{0,0,-1},width,false);
}

} // namespace TrainSim::Render
