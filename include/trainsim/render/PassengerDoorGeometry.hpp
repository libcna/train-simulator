#pragma once
#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/render/PixelGlyphs.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <utility>

namespace TrainSim::Render {
inline void addPassengerDoorButtonFaces(MeshBuilder& mesh)
{
    using namespace Microsoft::Xna::Framework;
    constexpr float tau=6.283185307f;
    for (float face : {-1.0f,1.0f})
        for (int i=0;i<24;++i)
        {
            const float a=tau*static_cast<float>(i)/24,b=tau*static_cast<float>(i+1)/24;
            const Vector3 centre{face*0.069f,0,0};
            const Vector3 p{centre.X,0.032f*std::sin(a),0.032f*std::cos(a)};
            const Vector3 q{centre.X,0.032f*std::sin(b),0.032f*std::cos(b)};
            if (face<0) mesh.addTriangle(centre,p,q,Color::White);
            else mesh.addTriangle(centre,q,p,Color::White);
        }
}

inline void addPassengerDoorControlHousing(MeshBuilder& mesh,float side,float z)
{
    using namespace Microsoft::Xna::Framework;
    for (float face : {-1.0f,1.0f})
    {
        const float x=side*1.45f+face*0.060f;
        mesh.addRoundedBoxYZ({std::min(x,x-face*0.030f),1.235f,z-0.055f},
                             {std::max(x,x-face*0.030f),1.475f,z+0.055f},
                             0.018f,3,Color(137,153,157,255));
        mesh.addEllipsoid({x+face*0.002f,1.36f,z},{0.005f,0.045f,0.045f},
                           16,6,Color(29,41,45,255),true);
        const auto rect=[&](float depth,float bottom,float top,float lo,float hi) {
            const Vector3 a{depth,bottom,lo},b{depth,bottom,hi},c{depth,top,hi},d{depth,top,lo};
            if (face<0) mesh.addQuad(a,b,c,d,Color(233,238,235,255));
            else mesh.addQuad(b,a,d,c,Color(233,238,235,255));
        };
        for (const auto& label : {std::pair{"DOOR",1.458f},std::pair{"OPEN",1.294f}})
        {
            constexpr float pixel=0.0035f;
            for (int character=0;character<4;++character)
            {
                const auto rows=pixelGlyphRows(label.first[character]);
                for (int row=0;row<7;++row)
                    for (int col=0;col<5;++col)
                    {
                        if ((rows[row]&(0x10U>>col))==0) continue;
                        const float offset=(static_cast<float>(character*6+col)-11.5f)*pixel;
                        const float a=z-face*offset,b=a-face*pixel*0.88f;
                        const float top=label.second-static_cast<float>(row)*pixel;
                        rect(x+face*0.001f,top-pixel*0.88f,top,std::min(a,b),std::max(a,b));
                    }
            }
        }
        // Raised two-panel/opening-arrow symbol stays pale while the button illumination changes.
        for (float direction : {-1.0f,1.0f})
        {
            rect(x+face*0.010f,1.35f,1.37f,z+direction*0.005f-0.0015f,
                 z+direction*0.005f+0.0015f);
            const Vector3 tip{x+face*0.010f,1.36f,z+direction*0.026f};
            const Vector3 upper{tip.X,1.368f,z+direction*0.014f};
            const Vector3 lower{tip.X,1.352f,upper.Z};
            if (face*direction<0) mesh.addTriangle(tip,upper,lower,Color(233,238,235,255));
            else mesh.addTriangle(tip,lower,upper,Color(233,238,235,255));
        }
        // Three tactile locating dots, not a claim to encode a Braille legend.
        for (float offset : {-0.012f,0.0f,0.012f})
            mesh.addEllipsoid({x+face*0.001f,1.248f,z+offset},{0.002f,0.002f,0.002f},
                               8,4,Color(215,221,221,255),true);
    }
}

inline std::array<Microsoft::Xna::Framework::Vector3,20> passengerDoorWindowContour(
    float x,float bottom,float top,float halfWidth,float radius)
{
    using namespace Microsoft::Xna::Framework;
    std::array<Vector3,20> result{};
    constexpr float halfPi=1.570796327f;
    for (int corner=0;corner<4;++corner)
        for (int step=0;step<=4;++step)
        {
            const float cz=(corner==0 || corner==3) ? halfWidth-radius : -halfWidth+radius;
            const float cy=corner<2 ? top-radius : bottom+radius;
            const float angle=halfPi*(static_cast<float>(corner)+static_cast<float>(step)/4);
            result[corner*5+step]={x,cy+radius*std::sin(angle),cz+radius*std::cos(angle)};
        }
    return result;
}

inline void addPassengerDoorLeaf(MeshBuilder& mesh)
{
    using namespace Microsoft::Xna::Framework;
    const Color paint(39,105,124,255);
    // Four structural panels leave a real opening, rather than painting a glass rectangle
    // on a solid leaf. The larger rounded flange overlaps every corner of this opening.
    mesh.addBox({-0.042f,-1.11f,-0.34f},{0.042f,-0.26f,0.34f},paint);
    mesh.addBox({-0.042f,0.64f,-0.34f},{0.042f,1.11f,0.34f},paint);
    mesh.addBox({-0.042f,-0.26f,-0.34f},{0.042f,0.64f,-0.23f},paint);
    mesh.addBox({-0.042f,-0.26f,0.23f},{0.042f,0.64f,0.34f},paint);
    for (float face : {-1.0f,1.0f})
    {
        const auto flange=passengerDoorWindowContour(face*0.043f,-0.285f,0.665f,0.255f,0.015f);
        const auto lip=passengerDoorWindowContour(face*0.055f,-0.266f,0.646f,0.236f,0.063f);
        const auto seal=passengerDoorWindowContour(face*0.048f,-0.235f,0.615f,0.205f,0.05f);
        const auto inner=passengerDoorWindowContour(0,-0.235f,0.615f,0.205f,0.05f);
        const auto band=[&](const auto& outer,const auto& inside,const Color& color) {
            for (int i=0;i<20;++i)
            {
                const int next=(i+1)%20;
                if (face>0) mesh.addQuad(outer[next],outer[i],inside[i],inside[next],color);
                else mesh.addQuad(outer[i],outer[next],inside[next],inside[i],color);
            }
        };
        band(flange,lip,Color(115,141,148,255));
        band(lip,seal,Color(24,35,39,255));
        band(seal,inner,Color(18,28,31,255));
        mesh.addBox({face<0 ? -0.048f : 0.043f,-0.73f,-0.27f},
                    {face<0 ? -0.043f : 0.048f,-0.61f,-0.20f},Color(225,184,47,255));
    }
}

inline void addPassengerDoorPane(MeshBuilder& mesh,float x,float baseY,float centreZ)
{
    using namespace Microsoft::Xna::Framework;
    const auto contour=passengerDoorWindowContour(x,-0.235f,0.615f,0.205f,0.05f);
    const Vector3 centre{x,baseY+0.19f,centreZ};
    const Color glass(155,200,211,22);
    for (int i=0;i<20;++i)
    {
        auto a=contour[i],b=contour[(i+1)%20];
        a.Y+=baseY; b.Y+=baseY;
        a.Z+=centreZ; b.Z+=centreZ;
        mesh.addTriangle(centre,a,b,glass);
        mesh.addTriangle(centre,b,a,glass);
    }
}
} // namespace TrainSim::Render
