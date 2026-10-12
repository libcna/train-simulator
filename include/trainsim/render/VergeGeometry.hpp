#pragma once

#include "trainsim/render/LinesideGeometry.hpp"
#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/render/ProceduralMaterials.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace TrainSim::Render {

// Rural lineside verge (ENV-124): absolute-route vertex colour zones from the ballast toe to the
// field headland, a concrete cable trough on the open side and wildflower clusters along the
// fence. Everything is anchored to route metres so chunk boundaries never show.

namespace verge_detail {
inline std::uint64_t mix(std::uint64_t value)
{
    value=(value^(value>>30))*0xbf58476d1ce4e5b9ULL;
    value=(value^(value>>27))*0x94d049bb133111ebULL;
    return value^(value>>31);
}
inline float lattice(std::int64_t i,std::int64_t j,std::uint64_t seed)
{
    // One finalising mix of a multiplicatively combined key is ample for visual noise.
    const auto h=mix(static_cast<std::uint64_t>(i)*0x9e3779b97f4a7c15ULL
                     +static_cast<std::uint64_t>(j)*0xc2b2ae3d27d4eb4fULL+seed*0x165667b19e3779f9ULL);
    return static_cast<float>(h&0xffffU)/65535.0f;
}
inline float smooth(float t) { return t*t*(3-2*t); }
inline float band(double value,double from,double to)
{
    return smooth(static_cast<float>(std::clamp((value-from)/(to-from),0.0,1.0)));
}
inline Microsoft::Xna::Framework::Color blend(const Microsoft::Xna::Framework::Color& a,
                                              const Microsoft::Xna::Framework::Color& b,float t)
{
    const auto channel=[t](int x,int y) {
        return std::clamp(static_cast<int>(std::lround(static_cast<float>(x)
                                                      +static_cast<float>(y-x)*t)),0,255);
    };
    return {channel(a.getRProperty(),b.getRProperty()),channel(a.getGProperty(),b.getGProperty()),
            channel(a.getBProperty(),b.getBProperty()),255};
}
} // namespace verge_detail

// Smooth deterministic value noise in [0,1]; x and y are already divided by the feature scale.
inline float vergeValueNoise(double x,double y,std::uint64_t seed)
{
    const double fx=std::floor(x),fy=std::floor(y);
    const auto i=static_cast<std::int64_t>(fx),j=static_cast<std::int64_t>(fy);
    const float tx=verge_detail::smooth(static_cast<float>(x-fx));
    const float ty=verge_detail::smooth(static_cast<float>(y-fy));
    const float a=verge_detail::lattice(i,j,seed),b=verge_detail::lattice(i+1,j,seed);
    const float c=verge_detail::lattice(i,j+1,seed),d=verge_detail::lattice(i+1,j+1,seed);
    return (a+(b-a)*tx)+((c+(d-c)*tx)-(a+(b-a)*tx))*ty;
}

// Lateral zones, metres from the running-line centre on either side.
inline constexpr double kVergeCessEnd=4.75;
inline constexpr double kVergeMownEnd=7.45;
inline constexpr double kVergeFenceLineEnd=8.8;
inline constexpr double kVergeDrainEnd=12.0;
inline constexpr double kVergeHeadlandEnd=26.0;
// Behind the 4.3 m catenary masts and in front of the 6.75 m lineside cabinets.
inline constexpr float kCableTroughLateral=5.05f;

// Colour of the verge surface for |lateral| < 26 m. It returns exactly the headland margin
// colour (91,133,72) at 26 m so the field surface beyond continues without a seam.
inline Microsoft::Xna::Framework::Color ruralVergeTint(double distance,double lateral)
{
    using Microsoft::Xna::Framework::Color;
    using verge_detail::band;
    using verge_detail::blend;
    const double across=std::min(std::abs(lateral),kVergeHeadlandEnd);
    const std::uint64_t side=lateral<0 ? 0x51ULL : 0xa7ULL;
    // Broad growth patches, finer clumps and long dry stretches, each at a different scale.
    const float broad=vergeValueNoise(distance/23.0,across/3.7,side*3+1);
    const float fine=vergeValueNoise(distance/4.3,across/1.15,side*3+2);
    const float dry=vergeValueNoise(distance/71.0,across/9.0,side*3+3);
    const float clump=vergeValueNoise(distance/9.0,across/2.1,side*3+4);

    // Cess: compacted ballast dust and sparse grass beside the toe.
    const Color cess=blend(Color(121,115,97,255),Color(103,112,77,255),
                           std::clamp(fine*1.2f-0.15f,0.0f,1.0f));
    // Mown strip: lush, clover-dark and sun-dried patches.
    Color mown=blend(Color(84,124,62,255),Color(62,100,52,255),std::clamp(broad*1.7f-0.45f,0.0f,1.0f));
    mown=blend(mown,Color(132,132,78,255),std::clamp((dry-0.42f)*1.9f,0.0f,0.72f));
    mown=blend(mown,Color(100,140,70,255),std::clamp((fine-0.55f)*2.2f,0.0f,0.6f));
    mown=blend(mown,Color(58,92,48,255),std::clamp((clump-0.6f)*2.4f,0.0f,0.55f));
    // Unmown fence line: darker, coarser growth with straw-coloured seed heads.
    Color rough=blend(Color(60,90,48,255),Color(124,120,78,255),std::clamp((fine-0.5f)*2.0f,0.0f,0.7f));
    rough=blend(rough,Color(50,78,44,255),std::clamp(broad-0.35f,0.0f,0.6f));
    // Drainage strip outside the fence: damp, darker sward.
    const Color drain=blend(Color(64,96,56,255),Color(55,85,52,255),broad);
    // Field headland: rough grass that settles to the margin colour at the parcel edge.
    Color headland=blend(Color(86,126,66,255),Color(112,122,72,255),
                         std::clamp((dry-0.45f)*1.4f,0.0f,0.6f));
    headland=blend(headland,Color(76,114,60,255),std::clamp((broad-0.5f)*1.2f,0.0f,0.5f));
    headland=blend(headland,Color(91,133,72,255),band(across,21.0,kVergeHeadlandEnd));

    Color colour=blend(cess,mown,band(across,kVergeCessEnd-0.45,kVergeCessEnd+0.25));
    colour=blend(colour,rough,band(across,kVergeMownEnd-0.35,kVergeMownEnd+0.2));
    colour=blend(colour,drain,band(across,kVergeFenceLineEnd,kVergeFenceLineEnd+0.6));
    colour=blend(colour,headland,band(across,kVergeDrainEnd-0.4,kVergeDrainEnd+1.4));
    return colour;
}

// Urban lineside (ENV-125): gravel cess, weed-grown ballast dust and a scrubby strip along the
// palisade boundary, joining the street-side ground colour at 12 m.
inline constexpr double kUrbanVergeEnd=12.0;
inline constexpr double kPalisadeLateral=11.55;
inline Microsoft::Xna::Framework::Color urbanVergeTint(double distance,double lateral,
                                                       double routeLength)
{
    using Microsoft::Xna::Framework::Color;
    using verge_detail::band;
    using verge_detail::blend;
    const double across=std::min(std::abs(lateral),kUrbanVergeEnd);
    const std::uint64_t side=lateral<0 ? 0x33ULL : 0xc5ULL;
    const float broad=vergeValueNoise(distance/19.0,across/3.1,side*5+1);
    const float fine=vergeValueNoise(distance/3.9,across/1.0,side*5+2);
    const float weeds=vergeValueNoise(distance/11.0,across/1.7,side*5+3);
    const Color gravel=blend(Color(116,111,100,255),Color(98,94,86,255),broad);
    Color mid=blend(gravel,Color(84,100,66,255),std::clamp((weeds-0.45f)*2.2f,0.0f,0.85f));
    mid=blend(mid,Color(128,120,104,255),std::clamp((fine-0.7f)*2.5f,0.0f,0.5f));
    Color scrub=blend(Color(70,90,56,255),Color(104,106,74,255),std::clamp((fine-0.5f)*1.8f,0.0f,0.6f));
    scrub=blend(scrub,Color(58,78,48,255),std::clamp(broad-0.4f,0.0f,0.5f));
    const Color street=terminalGroundTint(distance,std::copysign(kUrbanVergeEnd,lateral),routeLength);
    Color colour=blend(gravel,mid,band(across,5.0,6.2));
    colour=blend(colour,scrub,band(across,9.6,10.6));
    return blend(colour,street,band(across,11.4,kUrbanVergeEnd));
}

// Across-track terrain cuts for the near cells. High quality samples the verge roughly every
// 0.7 m and the headland every 1.6-2.5 m; Medium uses about half the cuts.
inline std::span<const double> vergeLateralCuts(int quality)
{
    static constexpr std::array<double,15> high={3.1,3.75,4.35,4.75,5.3,5.9,6.5,7.0,7.45,8.1,
                                                 9.6,11.2,14.0,18.0,22.0};
    static constexpr std::array<double,7> medium={3.1,4.75,6.0,7.45,10.4,15.0,21.0};
    return quality>=2 ? std::span<const double>{high} : std::span<const double>{medium};
}
inline std::span<const double> urbanVergeLateralCuts(int quality)
{
    static constexpr std::array<double,9> high={3.1,4.0,5.0,6.0,7.2,8.1,9.6,10.6,11.4};
    static constexpr std::array<double,5> medium={3.1,5.5,8.0,10.4,11.4};
    return quality>=2 ? std::span<const double>{high} : std::span<const double>{medium};
}
inline double vergeAlongStep(int quality) { return quality>=2 ? 5.0 : 10.0; }

// Steel palisade boundary between an urban railway and its street: posts on an absolute 2.75 m
// cadence, two rails and pointed pales every 0.25 m. Pales are double-sided flat blades, cheap
// enough for the see-through urban boundary. Spans and posts are emitted separately so a chunk
// draws exactly the spans that start inside it. Point(distance,lateral) yields {x,y,z,yaw}.
inline constexpr double kPalisadePostSpacing=2.75;
template<class Point>
void addPalisadePost(MeshBuilder& mesh,Point point,double distance,double lateral)
{
    using namespace Microsoft::Xna::Framework;
    const auto p=point(distance,lateral);
    mesh.addBox(Vector3{-0.045f,-0.25f,-0.045f},Vector3{0.045f,2.05f,0.045f},Color(66,84,74,255),
                Matrix::CreateRotationY(p.yaw)*Matrix::CreateTranslation(p.x,p.y,p.z));
}
template<class Point>
void addPalisadeSpan(MeshBuilder& mesh,Point point,double begin,double end,double lateral,
                     bool detailed)
{
    using namespace Microsoft::Xna::Framework;
    const Color rail(58,74,66,255),pale(74,94,82,255),paleTop(92,112,98,255);
    const auto at=[&](double distance,float height) {
        const auto p=point(distance,lateral);
        return Vector3{p.x,p.y+height,p.z};
    };
    for (float height : {0.42f,1.62f})
        addBarBetween(mesh,at(begin,height),at(end,height),0.05f,rail);
    if (!detailed) return;
    for (double d=begin+0.14;d<end-0.08;d+=0.25)
    {
        const Vector3 bottom0=at(d-0.03,0.05f),bottom1=at(d+0.03,0.05f);
        const Vector3 top0=at(d-0.03,1.90f),top1=at(d+0.03,1.90f),tip=at(d,2.04f);
        mesh.addQuad(bottom0,bottom1,top1,top0,pale,pale,paleTop,paleTop);
        mesh.addQuad(top0,top1,bottom1,bottom0,paleTop,paleTop,pale,pale);
        mesh.addTriangle(top0,top1,tip,paleTop);
        mesh.addTriangle(tip,top1,top0,paleTop);
    }
}

// One precast trough unit with two lids; the gaps between boxes read as lid joints.
inline constexpr double kCableTroughUnitLength=2.0;
inline void addCableTroughUnit(MeshBuilder& mesh,const Microsoft::Xna::Framework::Matrix& placement,
                               unsigned variant)
{
    using namespace Microsoft::Xna::Framework;
    const int tone=128+static_cast<int>(variant%5U)*3;
    mesh.addBox(Vector3{-0.16f,-0.06f,-0.99f},Vector3{0.16f,0.09f,0.99f},
                Color(tone-12,tone-14,tone-20,255),placement);
    for (float centre : {-0.5f,0.5f})
        mesh.addBox(Vector3{-0.15f,0.09f,centre-0.485f},Vector3{0.15f,0.115f,centre+0.485f},
                    Color(tone,tone-2,tone-9,255),placement);
}

struct VergeFlowerPlacement
{
    double distance=0,lateral=0;
    float height=0,yaw=0;
    unsigned palette=0;
    bool flowering=false;
};

inline double vergeFlowerStep(int quality) { return quality>=2 ? 2.0 : 4.0; }

// Deterministic clump along the inner fence line, side -1 or +1.
inline VergeFlowerPlacement vergeFlowerPlacement(double distance,int side)
{
    const auto slot=static_cast<std::int64_t>(std::llround(distance/2.0));
    const auto h=verge_detail::mix(static_cast<std::uint64_t>(slot)*2+(side<0 ? 0U : 1U)+0x7f4aULL);
    const auto unit=[&](int shift) { return static_cast<double>((h>>shift)&1023U)/1023.0; };
    // Flowers grow in drifts: a slow noise along the route decides whether this stretch blooms.
    const float drift=vergeValueNoise(distance/37.0,side,0x6a09ULL);
    return {distance+(unit(0)-0.5)*1.5,
            side*(7.6+unit(10)*0.9),
            static_cast<float>(0.36+unit(20)*0.32),
            static_cast<float>(unit(30)*6.283185307179586),
            static_cast<unsigned>((h>>40)%4U),
            drift>0.42f && unit(44)<0.75};
}

// A clump of five leaning grass stems; in a flowering drift three of them carry heads.
// Opaque and double-sided, so no alpha sorting or extra draw call is needed.
inline void addVergeFlowerClump(MeshBuilder& mesh,const Microsoft::Xna::Framework::Vector3& root,
                                const VergeFlowerPlacement& placement)
{
    using namespace Microsoft::Xna::Framework;
    static const std::array<Color,4> heads={Color(236,202,52,255),Color(238,236,224,255),
                                            Color(156,106,176,255),Color(222,152,48,255)};
    const Color stem(70,100,50,255),stemTip(112,128,70,255);
    for (int i=0;i<5;++i)
    {
        const float yaw=placement.yaw+static_cast<float>(i)*1.2566371f;
        const float height=placement.height*(0.62f+0.11f*static_cast<float>(i));
        const float reach=0.07f+0.03f*static_cast<float>(i%3);
        const Vector3 lean{std::sin(yaw)*reach,0.0f,std::cos(yaw)*reach};
        const Vector3 across{std::cos(yaw)*0.03f,0.0f,-std::sin(yaw)*0.03f};
        const Vector3 base=root+lean*0.2f;
        const Vector3 tip=root+lean+Vector3{0,height,0};
        mesh.addQuad(base-across,base+across,tip+across*0.3f,tip-across*0.3f,stem,stem,stemTip,stemTip);
        mesh.addQuad(tip-across*0.3f,tip+across*0.3f,base+across,base-across,stemTip,stemTip,stem,stem);
        if (!placement.flowering || i%2==1) continue;
        const Color head=heads[(placement.palette+static_cast<unsigned>(i/2))%heads.size()];
        // Three crossed diamonds give a rounded head from every view instead of a square flag.
        const float r=placement.palette==1 ? 0.055f : 0.045f;
        const Vector3 x{r,0,0},z{0,0,r},y{0,r*0.8f,0};
        mesh.addQuad(tip-x,tip-z,tip+x,tip+z,head);
        mesh.addQuad(tip+z,tip+x,tip-z,tip-x,head);
        mesh.addQuad(tip-x,tip-y,tip+x,tip+y,head);
        mesh.addQuad(tip+y,tip+x,tip-y,tip-x,head);
        mesh.addQuad(tip-z,tip-y,tip+z,tip+y,head);
        mesh.addQuad(tip+y,tip+z,tip-y,tip-z,head);
    }
}

} // namespace TrainSim::Render
