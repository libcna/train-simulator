#pragma once
#include "trainsim/render/MeshBuilder.hpp"
#include <cmath>
#include <array>
#include <string_view>

namespace TrainSim::Render {
inline constexpr std::array<std::string_view,6> PassengerCarRoleLabels={
    "Car 1 | Quiet", "Car 2 | Work tables", "Car 3 | Accessible",
    "Car 4 | Family", "Car 5 | Luggage", "Car 6 | Standard"};

inline void addFamilyActivityBook(MeshBuilder& mesh,float z)
{
    using namespace Microsoft::Xna::Framework;
    mesh.addRoundedBoxXZ({0.740f,0.861f,z-0.115f},{1.100f,0.868f,z+0.115f},
                          0.012f,3,Color(134,111,62,255));
    for (float left : {0.748f,0.924f})
        mesh.addBox({left,0.868f,z-0.108f},{left+0.168f,0.870f,z+0.108f},Color(226,220,200,255));
    // A small train drawing and a printed route puzzle, not loose blocks obstructing cup wells.
    for (int car=0;car<3;++car)
    {
        const float x=0.763f+static_cast<float>(car)*0.047f;
        mesh.addBox({x,0.8705f,z-0.026f},{x+0.040f,0.871f,z+0.018f},Color(73,116,137,255));
        mesh.addBox({x+0.006f,0.871f,z-0.020f},{x+0.034f,0.8715f,z-0.003f},Color(193,211,208,255));
        for (float wheel : {x+0.010f,x+0.031f})
            mesh.addDiscXZ({wheel,0.872f,z+0.018f},0.008f,10,Color(58,68,70,255));
    }
    for (int row=0;row<4;++row)
    {
        const float a=z-0.082f+static_cast<float>(row)*0.045f;
        const float left=row%2==0 ? 0.944f : 0.974f;
        mesh.addBox({left,0.8705f,a},{1.077f,0.8715f,a+0.004f},Color(103,131,100,255));
        const float turn=row%2==0 ? 1.073f : 0.974f;
        mesh.addBox({turn,0.8705f,a},{turn+0.004f,0.8715f,a+0.041f},Color(103,131,100,255));
    }
    const std::array<Color,3> crayons={Color(156,79,62,255),Color(67,111,144,255),Color(165,137,61,255)};
    for (int i=0;i<3;++i)
        mesh.addEllipsoid({1.16f+static_cast<float>(i)*0.025f,0.865f,z},
                           {0.004f,0.004f,0.065f},10,6,crayons[i],true);
}

// Compact, project-created monochrome pictograms; transforms also place accessible floor marks.
inline void addCarRoleIcon(MeshBuilder& mesh,int car,
    const Microsoft::Xna::Framework::Matrix& transform,
    const Microsoft::Xna::Framework::Color& color)
{
    using namespace Microsoft::Xna::Framework;
    if (car<0 || car>=6) return;
    const auto point=[&](float x,float y) { return Vector3::Transform({x,y,0},transform); };
    const auto line=[&](float x0,float y0,float x1,float y1,float width=0.007f) {
        const float length=std::hypot(x1-x0,y1-y0);
        if (length<0.0001f) return;
        const float dx=-(y1-y0)/length*width*0.5f,dy=(x1-x0)/length*width*0.5f;
        mesh.addQuad(point(x0-dx,y0-dy),point(x1-dx,y1-dy),
                     point(x1+dx,y1+dy),point(x0+dx,y0+dy),color);
    };
    const auto rectangle=[&](float left,float bottom,float right,float top) {
        line(left,bottom,right,bottom); line(right,bottom,right,top);
        line(right,top,left,top); line(left,top,left,bottom);
    };
    const auto circle=[&](float x,float y,float radius,bool filled) {
        constexpr int segments=20;
        constexpr float tau=6.283185307f;
        for (int i=0;i<segments;++i)
        {
            const float a=tau*static_cast<float>(i)/segments,b=tau*static_cast<float>(i+1)/segments;
            const auto p=point(x+radius*std::cos(a),y+radius*std::sin(a));
            const auto q=point(x+radius*std::cos(b),y+radius*std::sin(b));
            if (filled) mesh.addTriangle(point(x,y),p,q,color);
            else mesh.addQuad(p,q,point(x+(radius-0.007f)*std::cos(b),y+(radius-0.007f)*std::sin(b)),
                               point(x+(radius-0.007f)*std::cos(a),y+(radius-0.007f)*std::sin(a)),color);
        }
    };
    switch (car)
    {
    case 0: // Quiet: silenced handset.
        rectangle(-0.027f,-0.043f,0.027f,0.045f);
        line(-0.014f,-0.032f,0.014f,-0.032f,0.004f);
        line(-0.055f,-0.054f,0.055f,0.055f,0.010f);
        break;
    case 1: // Work: laptop.
        rectangle(-0.046f,-0.020f,0.046f,0.045f);
        line(-0.046f,-0.020f,-0.060f,-0.043f);
        line(0.046f,-0.020f,0.060f,-0.043f);
        line(-0.060f,-0.043f,0.060f,-0.043f);
        break;
    case 2: // Accessible seating/bay.
        circle(-0.018f,-0.020f,0.037f,false);
        circle(0.007f,0.060f,0.012f,true);
        line(0.005f,0.039f,0.005f,-0.006f,0.009f);
        line(0.005f,-0.006f,0.039f,-0.006f,0.009f);
        line(0.039f,-0.006f,0.055f,-0.039f,0.009f);
        line(0.055f,-0.039f,0.065f,-0.039f,0.006f);
        break;
    case 3: // Family: adult and child.
        circle(-0.028f,0.046f,0.014f,true);
        circle(0.035f,0.018f,0.011f,true);
        line(-0.028f,0.022f,-0.028f,-0.026f,0.022f);
        line(0.035f,-0.001f,0.035f,-0.027f,0.016f);
        for (float offset : {-0.008f,0.008f})
        {
            line(-0.028f+offset,-0.024f,-0.028f+offset,-0.054f,0.007f);
            line(0.035f+offset*0.7f,-0.025f,0.035f+offset*0.7f,-0.048f,0.005f);
        }
        line(-0.014f,0.009f,0.025f,-0.012f,0.006f);
        break;
    case 4: // Luggage.
        rectangle(-0.043f,-0.044f,0.043f,0.032f);
        rectangle(-0.018f,0.032f,0.018f,0.054f);
        for (float x : {-0.027f,0.027f})
        {
            line(x,-0.035f,x,0.023f,0.004f);
            circle(x,-0.053f,0.007f,true);
        }
        break;
    case 5: // General seating.
        line(-0.035f,0.053f,-0.035f,-0.020f,0.012f);
        line(-0.035f,-0.020f,0.042f,-0.020f,0.012f);
        line(-0.029f,-0.025f,-0.029f,-0.055f);
        line(0.035f,-0.025f,0.035f,-0.055f);
        break;
    }
}
} // namespace TrainSim::Render
