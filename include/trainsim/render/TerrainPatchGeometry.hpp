#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include <algorithm>
#include <array>

namespace TrainSim::Render {

// An open height-field mound, not a flattened sphere: every perimeter sample is
// buried in the actual terrain, so neither an underside nor a floating rim exists.
template<class GroundAt>
void addGroundedHill(MeshBuilder& mesh,GroundAt ground,float radius,float height,
                     unsigned seed,int subdivisions,
                     Microsoft::Xna::Framework::Color colour)
{
    using namespace Microsoft::Xna::Framework;
    if (radius<=0 || height<=0) return;
    const int n=std::clamp(subdivisions,4,16),stride=n+1;
    std::array<Vector3,17*17> points{},normals{};
    const float phase=static_cast<float>(seed%97U)*0.73f;
    for (int i=0;i<=n;++i)
        for (int j=0;j<=n;++j)
        {
            const float u=2.0f*static_cast<float>(i)/static_cast<float>(n)-1;
            const float v=2.0f*static_cast<float>(j)/static_cast<float>(n)-1;
            const float envelope=(1-u*u)*(1-u*u)*(1-v*v)*(1-v*v);
            const float shape=0.78f+0.16f*std::sin(3*u+phase)
                +0.10f*std::cos(4*v-phase)+0.12f*std::sin(6*u+2*v+phase);
            auto p=ground(u*radius*1.12f,v*radius);
            p.Y+=height*envelope*shape-0.30f;
            points[i*stride+j]=p;
        }
    for (int i=0;i<=n;++i)
        for (int j=0;j<=n;++j)
        {
            auto normal=Vector3::Cross(
                points[std::min(i+1,n)*stride+j]-points[std::max(i-1,0)*stride+j],
                points[i*stride+std::min(j+1,n)]-points[i*stride+std::max(j-1,0)]);
            if (normal.Y<0) normal=-normal;
            normal.Normalize();
            normals[i*stride+j]=normal;
        }
    for (int i=0;i<n;++i)
        for (int j=0;j<n;++j)
        {
            const int a=i*stride+j,b=(i+1)*stride+j,c=b+1,d=a+1;
            if (Vector3::Cross(points[b]-points[a],points[c]-points[a]).Y>=0)
                mesh.addQuad(points[a],points[b],points[c],points[d],colour,
                             normals[a],normals[b],normals[c],normals[d]);
            else
                mesh.addQuad(points[a],points[d],points[c],points[b],colour,
                             normals[a],normals[d],normals[c],normals[b]);
        }
}

// Equal U/V split positions keep the source a-c diagonal on sub-cell diagonals, so
// vertex-colour detail never changes terrain height or floats above its collision surface.
template<class ColourAt,class TextureAt>
void addSubdividedTerrainCell(MeshBuilder& mesh,
    const std::array<Microsoft::Xna::Framework::Vector3,4>& corners,
    ColourAt colour,TextureAt uv)
{
    constexpr std::array<float,4> cuts={0,0.10f,0.90f,1};
    const auto point=[&](float u,float v) {
        return u>=v ? corners[0]*(1-u)+corners[1]*(u-v)+corners[2]*v
                    : corners[0]*(1-v)+corners[2]*u+corners[3]*(v-u);
    };
    for (int along=0;along<3;++along)
        for (int across=0;across<3;++across)
        {
            const float u=cuts[along],un=cuts[along+1],v=cuts[across],vn=cuts[across+1];
            mesh.addTexturedQuad(point(u,v),point(un,v),point(un,vn),point(u,vn),
                colour(u,v),colour(un,v),colour(un,vn),colour(u,vn),
                uv(u,v),uv(un,v),uv(un,vn),uv(u,vn));
        }
}

// Clip a rectangular overlay to both triangles of one terrain cell. Subdividing only
// into quads is insufficient: a sub-quad can cross the source cell's non-planar diagonal.
inline void addTerrainCellPatch(MeshBuilder& mesh,
    const std::array<Microsoft::Xna::Framework::Vector3,4>& corners,
    float minU,float maxU,float minV,float maxV,
    const Microsoft::Xna::Framework::Color& colour,float lift=0.018f)
{
    using namespace Microsoft::Xna::Framework;
    minU=std::clamp(minU,0.0f,1.0f); maxU=std::clamp(maxU,0.0f,1.0f);
    minV=std::clamp(minV,0.0f,1.0f); maxV=std::clamp(maxV,0.0f,1.0f);
    if (minU>=maxU || minV>=maxV) return;
    const std::array<Vector2,4> rectangle={Vector2{minU,minV},{maxU,minV},{maxU,maxV},{minU,maxV}};
    const auto point=[&](Vector2 uv) {
        const auto p=uv.X>=uv.Y
            ? corners[0]*(1-uv.X)+corners[1]*(uv.X-uv.Y)+corners[2]*uv.Y
            : corners[0]*(1-uv.Y)+corners[2]*uv.X+corners[3]*(uv.Y-uv.X);
        return p+Vector3{0,lift,0};
    };
    for (float side : {-1.0f,1.0f})
    {
        std::array<Vector2,6> clipped;
        std::size_t count=0;
        auto previous=rectangle.back();
        float before=(previous.X-previous.Y)*side;
        for (const auto current : rectangle)
        {
            const float after=(current.X-current.Y)*side;
            if ((before>=0)!=(after>=0))
                clipped[count++]=previous+(current-previous)*(before/(before-after));
            if (after>=0) clipped[count++]=current;
            previous=current; before=after;
        }
        for (std::size_t i=1;i+1<count;++i)
        {
            const auto a=point(clipped[0]),b=point(clipped[i]),c=point(clipped[i+1]);
            if (Vector3::Cross(b-a,c-a).LengthSquared()>1e-10f)
                mesh.addTriangle(a,b,c,colour);
        }
    }
}

} // namespace TrainSim::Render
