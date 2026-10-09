#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/world/StationSite.hpp"
#include <algorithm>

namespace TrainSim::Render {

template<class SurfacePoint>
int addTerminalForecourt(MeshBuilder& structure,MeshBuilder& paving,SurfacePoint surface)
{
    using namespace Microsoft::Xna::Framework;
    using namespace World;
    const auto quad=[&](MeshBuilder& target,float a0,float a1,float x0,float x1,float lift,Color colour) {
        const auto a=surface(a0,x0)+Vector3{0,lift,0},b=surface(a1,x0)+Vector3{0,lift,0};
        const auto c=surface(a1,x1)+Vector3{0,lift,0},d=surface(a0,x1)+Vector3{0,lift,0};
        if (Vector3::Cross(b-a,c-a).Y>0)
            target.addTexturedQuad(a,b,c,d,colour,{x0,a0},{x0,a1},{x1,a1},{x1,a0});
        else target.addTexturedQuad(d,c,b,a,colour,{x1,a0},{x1,a1},{x0,a1},{x0,a0});
    };
    int slabs=0;
    const auto slab=[&](float a0,float a1,float x0,float x1) {
        if (a1-a0<0.012f || x1-x0<0.012f) return;
        const int tint=static_cast<int>(a0*11+x0*7)%5;
        quad(paving,a0+0.003f,a1-0.003f,x0+0.003f,x1-0.003f,
            0.004f,Color(174+tint,174+tint,159+tint,255));
        // Joint strips do not overlap the slab: no nearly coplanar underlay that
        // can intersect a different terrain-cell diagonal in a distant view.
        const Color joint(117,123,116,255);
        quad(structure,a0,a1,x0,x0+0.003f,0.004f,joint);
        quad(structure,a0,a1,x1-0.003f,x1,0.004f,joint);
        quad(structure,a0,a0+0.003f,x0+0.003f,x1-0.003f,0.004f,joint);
        quad(structure,a1-0.003f,a1,x0+0.003f,x1-0.003f,0.004f,joint);
        ++slabs;
    };
    for (float along=ForecourtBegin;along<ForecourtEnd;along+=1.2f)
        for (float across=ForecourtInner;across<ForecourtOuter;across+=1.2f)
        {
            const float end=std::min(along+1.2f,ForecourtEnd),outer=std::min(across+1.2f,ForecourtOuter);
            bool clipped=false;
            for (const auto& bed : ForecourtBeds)
            {
                if (end<=bed[0]-5 || along>=bed[0]+5 || outer<=bed[1]-2.4f || across>=bed[1]+2.4f) continue;
                const float left=std::max(across,bed[1]-2.4f),right=std::min(outer,bed[1]+2.4f);
                slab(along,end,across,left);
                slab(along,end,right,outer);
                slab(along,std::max(along,bed[0]-5),left,right);
                slab(std::min(end,bed[0]+5),end,left,right);
                clipped=true;
                break;
            }
            if (!clipped) slab(along,end,across,outer);
        }
    for (const auto& bed : ForecourtBeds)
    {
        quad(paving,bed[0]-4.9f,bed[0]+4.9f,bed[1]-2.3f,bed[1]+2.3f,0.20f,Color(84,79,60,255));
        for (float sign : {-1.0f,1.0f})
        {
            const float a=bed[0]+sign*4.95f,x=bed[1]+sign*2.35f;
            quad(structure,a-0.05f,a+0.05f,bed[1]-2.4f,bed[1]+2.4f,0.26f,Color(149,155,140,255));
            quad(structure,bed[0]-4.9f,bed[0]+4.9f,x-0.05f,x+0.05f,0.26f,Color(149,155,140,255));
        }
        const std::array<Vector2,4> corners{{{bed[0]-5,bed[1]-2.4f},{bed[0]+5,bed[1]-2.4f},
            {bed[0]+5,bed[1]+2.4f},{bed[0]-5,bed[1]+2.4f}}};
        const auto centre=surface(bed[0],bed[1]);
        for (std::size_t i=0;i<4;++i)
        {
            const auto u=corners[i],v=corners[(i+1)%4];
            const auto a=surface(u.X,u.Y),b=surface(v.X,v.Y);
            const auto c=b+Vector3{0,0.26f,0},d=a+Vector3{0,0.26f,0};
            if (Vector3::Dot(Vector3::Cross(b-a,c-a),(a+b)*0.5f-centre)>0)
                structure.addQuad(a,b,c,d,Color(126,136,126,255));
            else structure.addQuad(d,c,b,a,Color(126,136,126,255));
        }
    }
    // Close both exposed apron ends down to the existing terrain. The street
    // side joins its sidewalk at 160 mm, while the platform edge meets 780 mm.
    for (float along : {ForecourtBegin,ForecourtEnd})
        for (float x=ForecourtInner;x<ForecourtOuter;x+=2.0f)
        {
            const float end=std::min(x+2.0f,ForecourtOuter);
            const auto a=surface(along,x),b=surface(along,end);
            const auto c=b-Vector3{0,forecourtHeightAboveGround(end),0};
            const auto d=a-Vector3{0,forecourtHeightAboveGround(x),0};
            structure.addQuad(a,b,c,d,Color(117,125,117,255));
            structure.addQuad(d,c,b,a,Color(117,125,117,255));
        }
    // Narrow public approach continues to the hall entrance beside the platform.
    for (float along=ForecourtEnd;along<47;along+=1.0f)
        quad(paving,along+0.003f,along+0.997f,ForecourtInner,8.45f,0.004f,Color(175,177,162,255));
    return slabs;
}

inline void addForecourtBench(MeshBuilder& mesh,const Microsoft::Xna::Framework::Matrix& placement)
{
    using namespace Microsoft::Xna::Framework;
    for (float x : {-0.62f,0.62f})
    {
        mesh.addBox({x-0.045f,0,-0.22f},{x+0.045f,0.46f,0.22f},Color(78,91,86,255),placement);
        mesh.addBox({x-0.12f,0,-0.27f},{x+0.12f,0.035f,0.27f},Color(94,106,99,255),placement);
    }
    for (int slat=0;slat<5;++slat)
    {
        const float z=-0.25f+static_cast<float>(slat)*0.10f;
        mesh.addBox({-0.90f,0.45f,z},{0.90f,0.49f,z+0.082f},Color(141+slat*2,112+slat,77,255),placement);
    }
    for (int slat=0;slat<3;++slat)
    {
        const float y=0.57f+static_cast<float>(slat)*0.105f;
        mesh.addBox({-0.90f,y,0.22f},{0.90f,y+0.085f,0.265f},Color(143,115,79,255),placement);
    }
    for (float x : {-0.62f,0.62f})
        mesh.addBox({x-0.03f,0.45f,0.265f},{x+0.03f,0.88f,0.31f},Color(78,91,86,255),placement);
}
} // namespace TrainSim::Render
