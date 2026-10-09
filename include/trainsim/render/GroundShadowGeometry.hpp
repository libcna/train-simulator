#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include <algorithm>
#include <array>

namespace TrainSim::Render {

// Convex ground projection of an upright box, valid in every sunlight quadrant. A bounded
// subdivision samples the receiving surface rather than bridging it with six giant triangles.
template<class SurfacePoint>
void addProjectedBoxShadow(MeshBuilder& mesh,float width,float length,float shiftX,float shiftZ,
                           SurfacePoint surface,const Microsoft::Xna::Framework::Color& colour)
{
    using namespace Microsoft::Xna::Framework;
    if (width<=0 || length<=0) return;
    std::array<Vector2,8> points;
    int index=0;
    for (float shift : {0.0f,1.0f})
        for (float x : {-width*0.5f,width*0.5f})
            for (float z : {-length*0.5f,length*0.5f})
                points[index++]={x+shift*shiftX,z+shift*shiftZ};
    std::sort(points.begin(),points.end(),[](const Vector2& a,const Vector2& b) {
        return a.X<b.X || (a.X==b.X && a.Y<b.Y);
    });
    const auto cross=[](const Vector2& a,const Vector2& b,const Vector2& c) {
        return (b.X-a.X)*(c.Y-a.Y)-(b.Y-a.Y)*(c.X-a.X);
    };
    std::array<Vector2,16> hull;
    int count=0;
    for (const auto& point : points)
    {
        while (count>=2 && cross(hull[count-2],hull[count-1],point)<=0) --count;
        hull[count++]=point;
    }
    const int upper=count+1;
    for (int i=6;i>=0;--i)
    {
        while (count>=upper && cross(hull[count-2],hull[count-1],points[i])<=0) --count;
        hull[count++]=points[i];
    }
    --count;
    Vector2 centre=Vector2::Zero;
    for (int i=0;i<count;++i) centre+=hull[i];
    centre/=static_cast<float>(count);
    constexpr int divisions=4;
    // Neighbouring fan sectors share their radial samples exactly. Apart from eliminating
    // redundant terrain lookups, this prevents tiny floating-point cracks along sector edges.
    const Vector3 surfaceCentre=surface(centre.X,centre.Y);
    std::array<std::array<Vector3,divisions+1>,8> spokes;
    for (int edge=0;edge<count;++edge)
    {
        spokes[edge][0]=surfaceCentre;
        for (int i=1;i<=divisions;++i)
        {
            const float t=static_cast<float>(i)/divisions;
            const Vector2 p=centre*(1-t)+hull[edge]*t;
            spokes[edge][i]=surface(p.X,p.Y);
        }
    }
    for (int edge=0;edge<count;++edge)
    {
        // Reverse the XZ hull winding so the rendered surface faces up.
        const Vector2 b=hull[(edge+1)%count], c=hull[edge];
        std::array<std::array<Vector3,divisions+1>,divisions+1> grid;
        for (int i=0;i<=divisions;++i)
            for (int j=0;j<=divisions-i;++j)
            {
                if (i==0) { grid[i][j]=spokes[edge][j]; continue; }
                if (j==0) { grid[i][j]=spokes[(edge+1)%count][i]; continue; }
                const float u=static_cast<float>(i)/divisions, v=static_cast<float>(j)/divisions;
                const Vector2 point=centre*(1-u-v)+b*u+c*v;
                grid[i][j]=surface(point.X,point.Y);
            }
        for (int i=0;i<divisions;++i)
            for (int j=0;j<divisions-i;++j)
            {
                mesh.addTriangle(grid[i][j],grid[i+1][j],grid[i][j+1],colour);
                if (i+j<divisions-1)
                    mesh.addTriangle(grid[i+1][j],grid[i+1][j+1],grid[i][j+1],colour);
            }
    }
}

} // namespace TrainSim::Render
