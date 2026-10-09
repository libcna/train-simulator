#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace TrainSim::Render {

// Height is authored at scale one, just like crown and branch attachment coordinates.
// A shared middle ring forms a continuous taper, without overlapping cylinder end caps.
inline void addTaperedTreeTrunk(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Vector3& ground,float scale,float height,int sides,
    const Microsoft::Xna::Framework::Color& color)
{
    using namespace Microsoft::Xna::Framework;
    if (scale<=0 || height<=0) return;
    sides=std::clamp(sides,3,32);
    constexpr float tau=6.283185307f;
    constexpr std::array<float,3> levels={0,0.54f,1};
    constexpr std::array<float,3> radii={0.20f,0.16f,0.10f};
    const auto point=[&](int ring,int side) {
        const float angle=tau*static_cast<float>(side%sides)/static_cast<float>(sides);
        return ground+Vector3{radii[ring]*scale*std::cos(angle),levels[ring]*height*scale,
                              radii[ring]*scale*std::sin(angle)};
    };
    for (int side=0;side<sides;++side)
    {
        const float shade=0.84f+0.16f*std::cos(tau*static_cast<float>(side)/static_cast<float>(sides));
        const Color bark(static_cast<int>(color.getRProperty()*shade),
                         static_cast<int>(color.getGProperty()*shade),
                         static_cast<int>(color.getBProperty()*shade),255);
        for (int ring=0;ring<2;++ring)
            mesh.addQuad(point(ring+1,side),point(ring+1,side+1),
                         point(ring,side+1),point(ring,side),bark);
        mesh.addTriangle(ground+Vector3{0,height*scale,0},point(2,side+1),point(2,side),color);
        mesh.addTriangle(ground,point(0,side),point(0,side+1),color);
    }
}

// Stable authored-route identity, never camera/chunk-local coordinates. Quarter-metre
// bins determine shape identity; geometry streaming never contributes to the seed.
inline std::uint32_t treeShapeSeed(double distance,double lateral,std::uint32_t species)
{
    const auto mix=[](std::uint32_t value) {
        value^=value>>16; value*=0x7feb352dU;
        value^=value>>15; value*=0x846ca68bU;
        return value^(value>>16);
    };
    const auto along=static_cast<std::uint32_t>(static_cast<std::int64_t>(std::llround(distance*4)));
    const auto across=static_cast<std::uint32_t>(static_cast<std::int64_t>(std::llround(lateral*4)));
    return mix(along^mix(across)^mix(species+1U));
}

// Close spruce crowns use separate drooping branch fans, not continuous cone skirts. Far trees
// retain the small cone representation. All coordinates and variation are tree-local.
inline int addConiferCrown(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Vector3& ground, float scale, int sides,
    std::uint32_t variant, const Microsoft::Xna::Framework::Color& foliage,
    const Microsoft::Xna::Framework::Color& light, bool closeDetail,MeshBuilder* sprays=nullptr)
{
    using namespace Microsoft::Xna::Framework;
    if (scale <= 0) return 0;
    const auto firstVertex=mesh.data().vertices.size();
    constexpr float tau=6.283185307f;
    const int tiers=closeDetail ? 7 : 3;
    for (int tier=0;tier<tiers;++tier)
    {
        const float level=static_cast<float>(tier);
        const float phase=static_cast<float>(variant%97U)*1.37f+level*2.11f;
        const float spacingVariation=0.08f*std::sin(phase)*std::sin(level*tau/12.0f);
        const float y=closeDetail ? 1.58f+level*0.68f+spacingVariation : 1.58f+level*0.99f;
        const float breadth=0.88f+0.12f*static_cast<float>((variant>>8)%101U)/100.0f;
        const float radius=(closeDetail ? (2.05f-level*0.255f)*breadth : 2.05f-level*0.345f)*scale;
        const float height=(closeDetail ? 1.50f-level*0.035f : 2.32f-level*0.07f)*scale;
        const Vector3 base=ground+Vector3{0.06f*scale*std::sin(phase),y*scale,
                                         0.06f*scale*std::cos(phase)};
        const Color crown=tier%2 == 0 ? foliage : light;
        if (!closeDetail)
        {
            mesh.addCone(base,radius,height,sides,crown);
            continue;
        }
        const int branches=std::clamp(sides,6,12);
        // A narrow central needle mass joins the whorls without sealing every branch gap.
        mesh.addCone(base,radius*(sprays ? 0.29f : 0.38f),height,branches,foliage);
        for (int i=0;i<branches;++i)
        {
            const float ordinal=static_cast<float>(i);
            const float angle=tau*ordinal/static_cast<float>(branches)+phase
                +0.08f*std::sin(phase+ordinal*1.73f);
            const float reach=0.87f+0.11f*std::sin(phase+ordinal*2.37f);
            const Vector3 radial{std::cos(angle),0,std::sin(angle)};
            const Vector3 tangent{-radial.Z,0,radial.X};
            const float fanWidth=radius*(0.19f+0.035f*std::sin(phase+ordinal));
            const float droop=scale*(-0.10f+0.065f*std::sin(phase+ordinal*1.31f));
            const Vector3 root=base+radial*(radius*0.10f)+Vector3{0,height*0.55f,0};
            const Vector3 left=base+radial*(radius*0.53f)+tangent*fanWidth+Vector3{0,droop+height*0.16f,0};
            const Vector3 tip=base+radial*(radius*reach)+Vector3{0,droop,0};
            const Vector3 right=base+radial*(radius*0.53f)-tangent*fanWidth+Vector3{0,droop+height*0.16f,0};
            const Vector3 ridge=base+radial*(radius*0.48f)+Vector3{0,height*0.36f,0};
            const float blend=0.18f+0.20f*(0.5f+0.5f*std::sin(phase+ordinal*2.19f));
            const auto channel=[&](int a,int b) {
                return static_cast<int>(static_cast<float>(a)+static_cast<float>(b-a)*blend);
            };
            const Color needles(channel(foliage.getRProperty(),light.getRProperty()),
                channel(foliage.getGProperty(),light.getGProperty()),
                channel(foliage.getBProperty(),light.getBProperty()),255);
            if (sprays)
            {
                // A narrow opaque branch supports three crossed needle fronds, not a solid skirt.
                const Vector3 half=tangent*(0.018f*scale);
                mesh.addQuad(root-half,tip-half,tip+half,root+half,foliage);
                mesh.addQuad(root+half,tip+half,tip-half,root-half,foliage);
                const Color tint(static_cast<int>(needles.getRProperty()*0.60f),
                                 static_cast<int>(needles.getGProperty()*0.60f),
                                 static_cast<int>(needles.getBProperty()*0.60f),255);
                for (int plane=0;plane<3;++plane)
                {
                    const Vector3 width=plane==0 ? tangent*fanWidth*1.25f
                        : (tangent*0.50f+Vector3::Up*(plane==1 ? 0.75f : -0.75f))*fanWidth;
                    const auto a=root-width,b=root+width,c=tip+width,d=tip-width;
                    sprays->addTexturedQuad(a,b,c,d,tint,{0,1},{1,1},{1,0},{0,0});
                    sprays->addTexturedQuad(d,c,b,a,tint,{0,0},{1,0},{1,1},{0,1});
                }
                continue;
            }
            // Four top facets and two underside triangles per fan, with outward winding.
            mesh.addTriangle(root,left,ridge,needles);
            mesh.addTriangle(left,tip,ridge,needles);
            mesh.addTriangle(tip,right,ridge,needles);
            mesh.addTriangle(right,root,ridge,needles);
            mesh.addQuad(root,right,tip,left,foliage);
        }
    }
    if (closeDetail) mesh.projectDetailUVs(firstVertex,ground,0.75f);
    return tiers;
}

// Match MeshBuilder's a-c diagonal, not bilinear interpolation across a non-planar quad.
inline Microsoft::Xna::Framework::Vector3 terrainQuadPoint(
    const std::array<Microsoft::Xna::Framework::Vector3,4>& corners, float along, float across)
{
    if (along >= across)
        return corners[0]*(1-along)+corners[1]*(along-across)+corners[2]*across;
    return corners[0]*(1-across)+corners[2]*along+corners[3]*(across-along);
}

template<class GroundPoint>
void addSoftVegetationShadow(MeshBuilder& mesh, GroundPoint point, float radiusX, float radiusZ,
    const Microsoft::Xna::Framework::Color& color, std::uint32_t seed)
{
    using namespace Microsoft::Xna::Framework;
    constexpr int sides = 12;
    constexpr float tau = 6.283185307f;
    std::array<Vector3,sides> inner, outer;
    const float phase=static_cast<float>(seed%31U)*0.2f;
    for (int i=0;i<sides;++i)
    {
        const float angle=tau*static_cast<float>(i)/sides;
        const float lobe=0.94f+0.06f*std::sin(3*angle+phase);
        const float x=radiusX*lobe*std::cos(angle), z=radiusZ*lobe*std::sin(angle);
        inner[i]=point(x*0.65f,z*0.65f);
        outer[i]=point(x,z);
    }
    const auto centre=point(0.0f,0.0f);
    Color clear=color;
    clear.setAProperty(0);
    for (int i=0;i<sides;++i)
    {
        const int next=(i+1)%sides;
        mesh.addTriangle(centre,inner[next],inner[i],color);
        mesh.addQuad(inner[i],inner[next],outer[next],outer[i],color,color,clear,clear);
    }
}

// A continuous opaque hedge; the callback places each profile vertex on the ground.
template<class GroundPoint>
void addTerrainHedge(MeshBuilder& mesh, GroundPoint groundPoint,
    float length, float width, float height,
    const Microsoft::Xna::Framework::Color& color, std::uint32_t seed,MeshBuilder* leaves=nullptr)
{
    using namespace Microsoft::Xna::Framework;
    if (length <= 0 || width <= 0 || height <= 0) return;
    const auto firstVertex=mesh.data().vertices.size();
    const int spans = std::clamp(static_cast<int>(std::ceil(length/1.5f)),1,128);
    constexpr std::array<Vector2,6> section{{{-0.5f,0},{-0.5f,0.55f},{-0.25f,0.92f},
                                           {0.25f,0.92f},{0.5f,0.55f},{0.5f,0}}};
    const float phase=static_cast<float>(seed%97U)*0.17f;
    std::array<std::array<Vector3,6>,129> profile;
    std::array<std::array<Color,6>,129> colours;
    for (int span=0;span<=spans;++span)
    {
        const float x=-length*0.5f+length*static_cast<float>(span)/static_cast<float>(spans);
        const float station=static_cast<float>(span);
        const float endTaper=leaves && (span==0 || span==spans) ? 0.35f : 1.0f;
        const float variation=(0.86f+0.14f*std::sin(phase+station*2.3f))*endTaper;
        // Slightly wandering sides and differently sized clumps break the straight wall edge.
        // The sum of half-width and centre displacement stays inside the authored footprint.
        const float breadth=(0.82f+0.12f*std::sin(phase+station*1.7f))*endTaper;
        const float wander=0.03f*width*std::sin(phase+station*0.83f);
        for (int edge=0;edge<6;++edge)
        {
            constexpr std::array<Vector2,6> rounded{{{-0.48f,0},{-0.38f,0.60f},{-0.14f,0.96f},
                                                    {0.14f,0.96f},{0.38f,0.60f},{0.48f,0}}};
            const auto shape=leaves ? rounded[edge] : section[edge];
            const float z=shape.X*width*breadth+wander;
            profile[span][edge]=groundPoint(x,z)+Vector3{0,shape.Y*height*variation,0};
            const float shade=0.76f+0.25f*section[edge].Y
                +0.045f*std::sin(phase+station*1.31f+static_cast<float>(edge)*0.7f);
            const auto channel=[&](int value) {
                return std::clamp(static_cast<int>(static_cast<float>(value)*shade),0,255);
            };
            colours[span][edge]=Color(channel(color.getRProperty()),channel(color.getGProperty()),
                                     channel(color.getBProperty()),static_cast<int>(color.getAProperty()));
        }
    }
    const auto point = [&](int span,int edge) {
        return profile[span][edge];
    };
    for (int span=0;span<spans;++span)
        for (int edge=0;edge<5;++edge)
        {
            if (leaves)
            {
                auto across=point(span,5)-point(span,0);
                across.Y=0;
                across.Normalize();
                const auto normal=[&](int e) {
                    constexpr std::array<Vector2,6> directions{{{-1,0},{-0.8f,0.6f},
                        {-0.28f,0.96f},{0.28f,0.96f},{0.8f,0.6f},{1,0}}};
                    return across*directions[e].X+Vector3::Up*directions[e].Y;
                };
                mesh.addSmoothTexturedQuad({point(span,edge),point(span,edge+1),
                    point(span+1,edge+1),point(span+1,edge)},
                    {normal(edge),normal(edge+1),normal(edge+1),normal(edge)},
                    {Vector2{0,0},Vector2{0,1},Vector2{1,1},Vector2{1,0}},color);
                continue;
            }
            mesh.addQuad(point(span,edge),point(span,edge+1),
                         point(span+1,edge+1),point(span+1,edge),
                         colours[span][edge],colours[span][edge+1],
                         colours[span+1][edge+1],colours[span+1][edge]);
        }
    for (int edge=1;edge<5;++edge)
    {
        mesh.addTriangle(point(0,0),point(0,edge+1),point(0,edge),color);
        mesh.addTriangle(point(spans,0),point(spans,edge),point(spans,edge+1),color);
    }
    if (leaves)
    {
        // Overlapping irregular leaf masses interrupt the long extruded body, including its
        // ends. One small lobe per span keeps the shared geometry budget bounded.
        for (int span=0;span<spans;++span)
        {
            const auto ground=(point(span,0)+point(span,5)+point(span+1,0)+point(span+1,5))*0.25f;
            auto along=point(span+1,0)-point(span,0);
            along.Y=0;
            const float halfLength=along.Length()*0.5f;
            along.Normalize();
            const Vector3 cross{-along.Z,0,along.X};
            const float variation=0.90f+0.10f*std::sin(phase+static_cast<float>(span)*2.17f);
            const Vector3 radii{std::abs(along.X)*halfLength+std::abs(cross.X)*width*0.48f,
                height*0.53f*variation,
                std::abs(along.Z)*halfLength+std::abs(cross.Z)*width*0.48f};
            mesh.addFoliageLobe(ground+Vector3::Up*(height*0.48f),radii,7,4,color,true);
        }
        mesh.projectDetailUVs(firstVertex,point(0,0),0.55f);
        const Color tint(static_cast<int>(color.getRProperty()*0.40f),
                         static_cast<int>(color.getGProperty()*0.40f),
                         static_cast<int>(color.getBProperty()*0.40f),255);
        const auto card=[&](const Vector3& centre,const Vector3& across,const Vector3& up) {
            const auto a=centre-across-up,b=centre+across-up,c=centre+across+up,d=centre-across+up;
            leaves->addTexturedQuad(a,b,c,d,tint,{0,1},{1,1},{1,0},{0,0});
            leaves->addTexturedQuad(d,c,b,a,tint,{0,0},{1,0},{1,1},{0,1});
        };
        for (int span=0;span<spans;++span)
        {
            const auto along=(point(span+1,0)-point(span,0))*0.5f;
            auto across=point(span,4)-point(span,1);
            across.Y=0;
            across.Normalize();
            for (int edge : {1,4})
            {
                const auto centre=(point(span,edge)+point(span+1,edge))*0.5f
                    +across*(edge==1 ? -0.035f*width : 0.035f*width);
                card(centre,along,Vector3::Up*(height*0.32f));
            }
            const auto crown=(point(span,2)+point(span,3)+point(span+1,2)+point(span+1,3))*0.25f;
            card(crown,along,Vector3::Up*(height*0.19f));
        }
        // Leafy end caps extend across the tapered core, not beyond the reserved hedge length.
        for (int end : {0,spans})
        {
            const auto centre=(point(end,1)+point(end,4))*0.5f;
            const auto across=(point(end,4)-point(end,1))*0.60f;
            card(centre,across,Vector3::Up*(height*0.16f));
        }
    }
}

// A young-crop canopy has sloping shoulders, not the dark vertical walls of long boxes.
// Ground callback follows the same handed local X/Z convention as addTerrainHedge.
template<class GroundPoint>
void addTerrainCropRow(MeshBuilder& mesh,GroundPoint ground,float length,float width,float height,
    const Microsoft::Xna::Framework::Color& colour,std::uint32_t seed)
{
    using namespace Microsoft::Xna::Framework;
    if (length<=0 || width<=0 || height<=0) return;
    const int spans=std::clamp(static_cast<int>(std::ceil(length/2.0f)),1,128);
    std::array<std::array<Vector3,3>,129> sections;
    const float phase=static_cast<float>(seed%97U)*0.31f;
    for (int i=0;i<=spans;++i)
    {
        const float x=-length*0.5f+length*static_cast<float>(i)/static_cast<float>(spans);
        const float crown=height*(0.86f+0.14f*std::sin(phase+static_cast<float>(i)*1.71f));
        sections[i]={ground(x,-width*0.5f)-Vector3{0,0.03f,0},
                     ground(x,0)+Vector3{0,crown,0},
                     ground(x,width*0.5f)-Vector3{0,0.03f,0}};
    }
    for (int i=0;i<spans;++i)
        for (int edge=0;edge<2;++edge)
            mesh.addQuad(sections[i][edge],sections[i][edge+1],
                sections[i+1][edge+1],sections[i+1][edge],colour);
    mesh.addTriangle(sections[0][0],sections[0][2],sections[0][1],colour);
    mesh.addTriangle(sections[spans][0],sections[spans][1],sections[spans][2],colour);
}

// Ground callback accepts absolute (route distance, lateral). Reverse the longitudinal
// axis to preserve handedness when mapping hedge-local X to route Z.
template<class GroundPoint>
void addRouteAlignedHedge(MeshBuilder& mesh,GroundPoint groundPoint,double distance,double lateral,
    float length,float width,float height,bool longitudinal,
    const Microsoft::Xna::Framework::Color& colour,std::uint32_t seed,MeshBuilder* leaves=nullptr)
{
    addTerrainHedge(mesh,[&](float x,float z) {
        auto point=longitudinal ? groundPoint(distance-x,lateral+z)
                                : groundPoint(distance+z,lateral+x);
        point.Y-=0.08f;
        return point;
    },length,width,height,colour,seed,leaves);
}

inline void addHedgeSegment(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Vector3& root, float length, float width, float height,
    float yaw, const Microsoft::Xna::Framework::Color& color, std::uint32_t seed)
{
    using namespace Microsoft::Xna::Framework;
    const float cosine=std::cos(yaw), sine=std::sin(yaw);
    addTerrainHedge(mesh,[&](float x,float z) {
        return root+Vector3{x*cosine+z*sine,0,-x*sine+z*cosine};
    },length,width,height,color,seed);
}

template<class Visitor>
void forEachBroadleafLobe(
    const Microsoft::Xna::Framework::Vector3& root, float scale, std::uint32_t seed,
    bool reducedDetail,Visitor visit)
{
    using namespace Microsoft::Xna::Framework;
    if (scale <= 0) return;
    constexpr float tau = 6.283185307f;
    const float phase = static_cast<float>(seed%97U)*0.0648f;
    const float crownWidth=0.84f+0.16f*static_cast<float>((seed>>8)%101U)/100.0f;
    const float crownDepth=0.84f+0.16f*static_cast<float>((seed>>16)%101U)/100.0f;
    for (int tier = 0; tier < 3; ++tier)
    {
        const int count = reducedDetail ? 3 : tier == 0 ? 5 : tier == 1 ? 6 : 4;
        const float ring = tier == 0 ? 1.02f : tier == 1 ? 1.23f : 0.68f;
        const float height = tier == 0 ? 3.72f : tier == 1 ? 4.55f : 5.35f;
        for (int lobe = 0; lobe < count; ++lobe)
        {
            const float angle = phase+static_cast<float>(lobe)*tau/static_cast<float>(count)
                +static_cast<float>(tier)*0.71f
                +0.19f*std::sin(phase+static_cast<float>(lobe)*2.71f+static_cast<float>(tier));
            const float variation = std::sin(angle*2.0f+phase+static_cast<float>(tier));
            const float reach=ring*(0.91f+0.09f*std::sin(angle*3.0f+phase));
            // The reduced crown retains three interleaved tiers, with wider lobes to avoid
            // opening a hollow ring when reducing the number of leaf masses.
            const float radius = (reducedDetail ? 0.86f : 0.71f)+0.065f*variation;
            const Vector3 centre = root+Vector3{
                std::cos(angle)*reach*crownWidth*scale,
                (height+0.15f*variation)*scale,
                std::sin(angle)*reach*0.87f*crownDepth*scale};
            // Unequal branch reach and lobe aspect ratios break the evenly spaced ball rings.
            // All variation contracts the previous horizontal envelope and retains its budget.
            visit(centre,Vector3{radius*crownWidth*scale,(0.72f+0.10f*variation)*scale,
                                 radius*0.93f*crownDepth*scale},angle,(lobe+tier)%3 == 0);
        }
    }
}

inline void addClusteredBroadleafCrown(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Vector3& root,float scale,std::uint32_t seed,
    const Microsoft::Xna::Framework::Color& dark,
    const Microsoft::Xna::Framework::Color& light,bool reducedDetail=false)
{
    forEachBroadleafLobe(root,scale,seed,reducedDetail,[&](const auto& centre,const auto& radii,
                                                        float,bool highlighted) {
        // Small opaque inner leaf masses preserve depth, while alpha-tested sprays below
        // define the porous silhouette. Full-radius solids concealed every opening before.
        mesh.addFoliageLobe(centre,radii*0.60f,reducedDetail ? 6 : 7,4,highlighted ? light : dark,true);
    });
}

// Depth-writing alpha-test cards, paired faces rather than a global culling-state override.
inline void addBroadleafEdgeSprays(MeshBuilder& mesh,
    const Microsoft::Xna::Framework::Vector3& root,float scale,std::uint32_t seed,
    const Microsoft::Xna::Framework::Color& color,bool reducedDetail=false)
{
    using namespace Microsoft::Xna::Framework;
    if (scale<=0) return;
    forEachBroadleafLobe(root,scale,seed,reducedDetail,[&](const auto& lobe,const auto& radii,
                                                        float angle,bool) {
        // Three non-camera-facing planes keep volume through curves and from both windows.
        // One atlas and depth-writing alpha test avoid transparency sorting or new draws.
        for (int plane=0;plane<3;++plane)
        {
            const float yaw=angle+static_cast<float>(plane)*1.04719755f;
            const Vector3 across{std::cos(yaw)*radii.X,0,std::sin(yaw)*radii.Z};
            const Vector3 up{0,radii.Y,0};
            const float shade=0.48f+0.05f*std::sin(yaw+0.3f);
            const Color shaded(static_cast<int>(color.getRProperty()*shade),
                               static_cast<int>(color.getGProperty()*shade),
                               static_cast<int>(color.getBProperty()*shade),255);
            const Vector3 a=lobe-across-up,b=lobe+across-up,c=lobe+across+up,d=lobe-across+up;
            mesh.addTexturedQuad(a,b,c,d,shaded,{0,1},{1,1},{1,0},{0,0});
            mesh.addTexturedQuad(d,c,b,a,shaded,{0,0},{1,0},{1,1},{0,1});
        }
    });
}

// Compact ground-level foliage, sharing the tree material and leaf cutout batches.
inline void addLeafyShrub(MeshBuilder& solid,MeshBuilder& sprays,
    const Microsoft::Xna::Framework::Vector3& ground,float scale,std::uint32_t seed,
    const Microsoft::Xna::Framework::Color& color)
{
    using namespace Microsoft::Xna::Framework;
    if (scale<=0) return;
    const float phase=static_cast<float>(seed%97U)*0.0648f;
    const std::array<Vector3,3> offsets={Vector3{0,0.55f,0},Vector3{0.44f,0.40f,0.22f},
                                        Vector3{-0.39f,0.36f,-0.16f}};
    for (std::size_t i=0;i<offsets.size();++i)
    {
        const float angle=phase+static_cast<float>(i)*1.71f;
        const float variation=0.94f+0.06f*std::sin(angle);
        const Vector3 centre=ground+offsets[i]*scale;
        const Vector3 radii=Vector3{0.60f,0.60f,0.55f}*(scale*variation);
        solid.addFoliageLobe(centre,radii*0.80f,7,4,color,true);
        const float shade=0.47f+0.045f*std::sin(angle);
        const Color tint(static_cast<int>(color.getRProperty()*shade),
                         static_cast<int>(color.getGProperty()*shade),
                         static_cast<int>(color.getBProperty()*shade),255);
        for (int plane=0;plane<3;++plane)
        {
            const float yaw=angle+static_cast<float>(plane)*1.04719755f;
            const Vector3 across{radii.X*std::cos(yaw),0,radii.Z*std::sin(yaw)};
            const Vector3 up{0,radii.Y,0};
            auto a=centre-across-up,b=centre+across-up;
            const auto c=centre+across+up,d=centre-across+up;
            a.Y=std::max(a.Y,ground.Y+0.006f*scale);
            b.Y=std::max(b.Y,ground.Y+0.006f*scale);
            sprays.addTexturedQuad(a,b,c,d,tint,{0,1},{1,1},{1,0},{0,0});
            sprays.addTexturedQuad(d,c,b,a,tint,{0,0},{1,0},{1,1},{0,1});
        }
    }
}

struct VergeBladePlacement
{
    double distance=0,lateral=0;
    float height=0,width=0,yaw=0;
};

inline int vergeBladeCount(int quality) { return quality<=0 ? 3 : quality==1 ? 9 : 15; }
inline double vergeTuftStep(int quality) { return quality<=0 ? 50.0 : quality==1 ? 6.0 : 3.0; }

inline VergeBladePlacement vergeBladePlacement(double distance,int sideIndex,int quality,int blade)
{
    const double step=vergeTuftStep(quality);
    const auto index=static_cast<std::int64_t>(std::llround(distance/step));
    const int perCluster=quality<=0 ? 1 : quality==1 ? 3 : 5;
    const int cluster=blade/perCluster;
    const double side=sideIndex==0 ? -1.0 : 1.0;
    const double clusterPhase=static_cast<double>(index)*2.399963+sideIndex*3.71+cluster*1.618;
    const double phase=clusterPhase+static_cast<double>(blade%perCluster)*2.399963;
    const float height=quality<=0 ? 0.34f+0.06f*static_cast<float>((index+sideIndex+4)%4)
                                 : 0.22f+0.035f*static_cast<float>((index+sideIndex+4)%4);
    return {
        distance+(static_cast<double>(cluster)-1.0)*1.05+0.12*std::sin(phase*2.3),
        side*(11.30+0.18*std::sin(distance*0.071+side)+0.30*std::sin(clusterPhase)
                    +(quality<=0 ? 0 : 0.12*std::cos(phase))),
        height*(0.78f+0.22f*static_cast<float>(std::sin(phase*1.7))),
        quality<=0 ? 0.065f : 0.024f+0.012f*static_cast<float>(0.5+0.5*std::sin(phase)),
        static_cast<float>(std::remainder(phase,6.283185307179586))};
}

// Opaque, double-sided bent ribbon: no alpha sorting and half the triangles of a box.
inline void addGrassBlade(MeshBuilder& mesh, const Microsoft::Xna::Framework::Vector3& root,
                         float height, float width, float yaw,
                         const Microsoft::Xna::Framework::Color& color)
{
    using namespace Microsoft::Xna::Framework;
    if (height <= 0.0f || width <= 0.0f) return;
    const Vector3 across{std::cos(yaw)*width*0.5f,0.0f,-std::sin(yaw)*width*0.5f};
    const Vector3 lean{std::sin(yaw)*height*0.22f,0.0f,std::cos(yaw)*height*0.22f};
    const Vector3 knee = root+Vector3::Up*(height*0.62f)+lean*0.45f;
    const Vector3 tip = root+Vector3::Up*height+lean;
    const Vector3 a = root-across;
    const Vector3 b = root+across;
    const Vector3 c = knee+across*0.56f;
    const Vector3 d = knee-across*0.56f;
    const auto shade=[&](float factor) {
        return Color(std::min(255,static_cast<int>(color.getRProperty()*factor)),
                     std::min(255,static_cast<int>(color.getGProperty()*factor)),
                     std::min(255,static_cast<int>(color.getBProperty()*factor)),255);
    };
    const auto baseColor=shade(0.85f),tipColor=shade(1.20f);
    mesh.addQuad(a,b,c,d,baseColor,baseColor,tipColor,tipColor);
    mesh.addQuad(d,c,b,a,tipColor,tipColor,baseColor,baseColor);
    mesh.addTriangle(d,c,tip,tipColor);
    mesh.addTriangle(tip,c,d,tipColor);
}

} // namespace TrainSim::Render
