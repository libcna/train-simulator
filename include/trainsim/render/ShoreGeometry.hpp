#pragma once
#include "trainsim/render/MeshBuilder.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace TrainSim::Render {
template<class HeightAt>
std::optional<double> shoreCoordinateAtHeight(HeightAt height,double lo,double hi,float target)
{
    const float a=height(lo),b=height(hi);
    if (target<std::min(a,b) || target>std::max(a,b) || lo>=hi || a==b) return std::nullopt;
    const bool ascending=b>a;
    for (int iteration=0;iteration<14;++iteration)
    {
        const double mid=(lo+hi)*0.5;
        if ((height(mid)<target)==ascending) lo=mid;
        else hi=mid;
    }
    return (lo+hi)*0.5;
}

inline double lakeShoreCoordinate(double distance,double waterward)
{
    const double blend=std::clamp((waterward-26.0)/54.0,0.0,1.0);
    const double irregularity=6.0*std::sin(distance/73.0)
        +3.0*std::sin(distance/31.0+0.7);
    return waterward+blend*irregularity;
}

inline double riverShoreCoordinate(double distance,double lateral)
{
    // Independent bank phases avoid a symmetrical engineered channel.
    return lakeShoreCoordinate(distance+(lateral<0.0 ? 43.0 : 0.0),std::abs(lateral));
}

inline Microsoft::Xna::Framework::Color shoreSurfaceTint(float aboveWater,
    const Microsoft::Xna::Framework::Color& vegetation)
{
    using Microsoft::Xna::Framework::Color;
    const Color wet(84,87,72,255),gravel(139,134,107,255);
    if (aboveWater<0.30f)
        return Color::Lerp(wet,gravel,std::clamp((aboveWater+0.20f)/0.50f,0.0f,1.0f));
    return Color::Lerp(gravel,vegetation,std::clamp((aboveWater-0.30f)/2.5f,0.0f,1.0f));
}

// Split the actual terrain triangles at water-relative contour heights. Every
// output point remains on its source plane: no overlay, holes or depth fighting.
inline void addShoreTerrainCell(MeshBuilder& mesh,
    const std::array<Microsoft::Xna::Framework::Vector3,4>& positions,
    const std::array<Microsoft::Xna::Framework::Vector2,4>& uvs,
    float waterHeight,const Microsoft::Xna::Framework::Color& vegetation)
{
    using namespace Microsoft::Xna::Framework;
    struct Point { Vector3 p; Vector2 uv; };
    const std::array<float,6> levels={-std::numeric_limits<float>::max(),
        waterHeight-0.20f,waterHeight+0.30f,waterHeight+1.20f,waterHeight+2.80f,
        std::numeric_limits<float>::max()};
    for (const std::array<int,3> triangle : {std::array{0,1,2},std::array{0,2,3}})
        for (std::size_t band=0;band+1<levels.size();++band)
        {
            std::array<Point,8> polygon{},scratch{};
            std::size_t count=3;
            for (std::size_t i=0;i<3;++i)
                polygon[i]={positions[triangle[i]],uvs[triangle[i]]};
            // A horizontal triangle exactly on a contour belongs only to its lower band.
            if (polygon[0].p.Y==levels[band] && polygon[1].p.Y==levels[band]
                && polygon[2].p.Y==levels[band]) continue;
            for (int plane=0;plane<2 && count>=3;++plane)
            {
                const float height=levels[band+static_cast<std::size_t>(plane)];
                const auto inside=[&](const Point& p) {
                    return plane==0 ? p.p.Y>=height : p.p.Y<=height;
                };
                std::size_t output=0;
                Point previous=polygon[count-1];
                for (std::size_t i=0;i<count;++i)
                {
                    const Point current=polygon[i];
                    if (inside(previous)!=inside(current))
                    {
                        const float t=(height-previous.p.Y)/(current.p.Y-previous.p.Y);
                        scratch[output++]={previous.p+(current.p-previous.p)*t,
                                           previous.uv+(current.uv-previous.uv)*t};
                    }
                    if (inside(current)) scratch[output++]=current;
                    previous=current;
                }
                polygon=scratch; count=output;
            }
            for (std::size_t i=1;i+1<count;++i)
            {
                const std::array<Point,3> t={polygon[0],polygon[i],polygon[i+1]};
                if (Vector3::Cross(t[1].p-t[0].p,t[2].p-t[0].p).LengthSquared()<1e-10f) continue;
                mesh.addTexturedTriangle({t[0].p,t[1].p,t[2].p},
                    {shoreSurfaceTint(t[0].p.Y-waterHeight,vegetation),
                     shoreSurfaceTint(t[1].p.Y-waterHeight,vegetation),
                     shoreSurfaceTint(t[2].p.Y-waterHeight,vegetation)},
                    {t[0].uv,t[1].uv,t[2].uv});
            }
        }
}
}
