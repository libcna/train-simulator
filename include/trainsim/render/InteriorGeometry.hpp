#pragma once
#include "trainsim/render/MeshBuilder.hpp"
#include <cmath>
#include <algorithm>

namespace TrainSim::Render {
inline void addVestibuleServicePanels(MeshBuilder& mesh,float side,float frontZ,float face)
{
    using namespace Microsoft::Xna::Framework;
    const float left=side<0 ? -1.40f : 0.67f,right=side<0 ? -0.67f : 1.40f;
    const auto first=mesh.data().vertices.size();
    const auto panel=[&](float x0,float x1,float bottom,float top,float front,float depth,
                          float radius,const Color& color) {
        const float back=front-face*depth;
        mesh.addRoundedBoxXY({x0,bottom,std::min(front,back)},
                             {x1,top,std::max(front,back)},radius,3,color);
    };
    panel(left-0.012f,right+0.012f,0.168f,2.292f,frontZ-face*0.003f,0.018f,
          0.028f,Color(53,71,75,255));
    panel(left,right,0.18f,1.09f,frontZ,0.016f,0.021f,Color(118,134,137,255));
    panel(left,right,1.102f,2.28f,frontZ,0.016f,0.021f,Color(143,156,157,255));
    mesh.projectXYDetailUVs(first,{(left+right)*0.5f,0,frontZ},0.23f);
}

inline void addFlexibleGangwayBellows(MeshBuilder& mesh,float centreZ)
{
    using namespace Microsoft::Xna::Framework;
    constexpr int rows=56,points=12;
    constexpr float pi=3.141592654f,tau=2*pi;
    const auto point=[&](int row,int index,bool outer) {
        const float t=static_cast<float>(row)/rows;
        const float corrugation=std::cos(tau*7*t);
        const float skin=outer ? 0.012f : 0;
        const float width=0.590f+0.025f*corrugation+skin;
        const float top=2.377f+0.027f*corrugation+skin,radius=0.105f+skin;
        const float z=centreZ-0.31f+0.62f*t;
        if (index==0) return Vector3{-width,0.105f,z};
        if (index==11) return Vector3{width,0.105f,z};
        const bool left=index<=5;
        const float angle=left ? pi-(pi*0.5f)*static_cast<float>(index-1)/4
                               : pi*0.5f-(pi*0.5f)*static_cast<float>(index-6)/4;
        return Vector3{(left ? -width+radius : width-radius)+radius*std::cos(angle),
                       top-radius+radius*std::sin(angle),z};
    };
    const auto normal=[&](int row,int index,bool outer) {
        const float t=static_cast<float>(row)/rows;
        const bool left=index<=5;
        const float angle=index==0 ? pi : index==11 ? 0
            : left ? pi-(pi*0.5f)*static_cast<float>(index-1)/4
                   : pi*0.5f-(pi*0.5f)*static_cast<float>(index-6)/4;
        const float nx=-std::cos(angle),ny=-std::sin(angle);
        const float derivative=-(tau*7/0.62f)*std::sin(tau*7*t);
        const float dx=(left ? -1.0f : 1.0f)*0.025f*derivative;
        const float dy=(index==0 || index==11) ? 0 : 0.027f*derivative;
        const float nz=-(nx*dx+ny*dy);
        const float scale=(outer ? -1.0f : 1.0f)/std::sqrt(1+nz*nz);
        return Vector3{nx*scale,ny*scale,nz*scale};
    };
    const auto uv=[](int row,int index) {
        constexpr float sideLength=2.167f,arcLength=0.105f*pi*0.5f,roofLength=0.970f;
        const float perimeter=index==0 ? 0 : index<=5
            ? sideLength+arcLength*static_cast<float>(index-1)/4
            : index<=10 ? sideLength+arcLength+roofLength+arcLength*static_cast<float>(index-6)/4
            : 2*sideLength+2*arcLength+roofLength;
        return Vector2{perimeter/0.12f,0.62f*static_cast<float>(row)/rows/0.12f};
    };
    for (bool outer : {false,true})
        for (int row=0;row<rows;++row)
            for (int i=0;i<points-1;++i)
            {
                std::array<Vector3,4> p={point(row,i,outer),point(row,i+1,outer),
                                         point(row+1,i+1,outer),point(row+1,i,outer)};
                std::array<Vector3,4> n={normal(row,i,outer),normal(row,i+1,outer),
                                         normal(row+1,i+1,outer),normal(row+1,i,outer)};
                std::array<Vector2,4> tex={uv(row,i),uv(row,i+1),uv(row+1,i+1),uv(row+1,i)};
                if (outer) { std::reverse(p.begin(),p.end()); std::reverse(n.begin(),n.end());
                             std::reverse(tex.begin(),tex.end()); }
                mesh.addSmoothTexturedQuad(p,n,tex,outer ? Color(34,43,46,255) : Color(45,55,58,255));
            }
    // Close the 12 mm skin at its mounting flanges and lower sill edges.
    const Color edge(49,59,62,255);
    for (int row : {0,rows})
        for (int i=0;i<points-1;++i)
            if (row==0) mesh.addQuad(point(row,i,false),point(row,i,true),
                                     point(row,i+1,true),point(row,i+1,false),edge);
            else mesh.addQuad(point(row,i+1,false),point(row,i+1,true),
                               point(row,i,true),point(row,i,false),edge);
    for (int row=0;row<rows;++row)
        for (int i : {0,11})
            if (i==0) mesh.addQuad(point(row,i,false),point(row+1,i,false),
                                   point(row+1,i,true),point(row,i,true),edge);
            else mesh.addQuad(point(row,i,true),point(row+1,i,true),
                               point(row+1,i,false),point(row,i,false),edge);
}

inline void addPassengerDisplayHousing(MeshBuilder& mesh,float z)
{
    using namespace Microsoft::Xna::Framework;
    for (float x : {-0.63f,0.63f})
        mesh.addRoundedBoxXZ({x-0.017f,2.795f,z-0.020f},
                              {x+0.017f,2.915f,z+0.020f},0.010f,3,Color(135,150,153,255));
    mesh.addRoundedBoxXY({-0.985f,2.320f,z-0.090f},{0.985f,2.815f,z+0.090f},
                         0.035f,4,Color(57,71,77,255));
    for (float face : {-1.0f,1.0f})
    {
        const float surface=z+face*0.094f;
        mesh.addRoundedBoxXY({-0.948f,2.340f,surface-0.003f},
                             {0.948f,2.795f,surface+0.003f},0.018f,3,Color(23,34,40,255));
        const float glass=z+face*0.099f;
        mesh.addRoundedBoxXY({-0.917f,2.357f,glass-0.001f},
                             {0.917f,2.778f,glass+0.001f},0.009f,3,Color(9,20,26,255));
        for (float x : {-0.964f,0.964f})
            for (float y : {2.372f,2.763f})
                mesh.addEllipsoid({x,y,z+face*0.091f},{0.003f,0.003f,0.0015f},
                                   8,4,Color(119,132,134,255),true);
    }
}

inline void addSuspendedPassengerHandle(MeshBuilder& metal,MeshBuilder& grip,float x,float z)
{
    using namespace Microsoft::Xna::Framework;
    metal.addRoundedBoxXZ({x-0.029f,2.893f,z-0.035f},
                          {x+0.029f,2.915f,z+0.035f},0.009f,3,Color(143,155,157,255));
    metal.addRoundedBoxXY({x-0.012f,2.55f,z-0.008f},
                          {x+0.012f,2.904f,z+0.008f},0.010f,3,Color(114,128,131,255));
    metal.addRoundedBoxXY({x-0.021f,2.542f,z-0.019f},
                          {x+0.021f,2.584f,z+0.019f},0.012f,3,Color(70,83,87,255));
    constexpr int rings=24,sides=8;
    constexpr float tau=6.283185307f;
    const auto normal=[](int ring,int side) {
        const float theta=tau*static_cast<float>(ring%rings)/rings;
        const float phi=tau*static_cast<float>(side%sides)/sides;
        float nx=std::cos(theta)/0.071f,ny=std::sin(theta)/0.100f;
        const float length=std::sqrt(nx*nx+ny*ny);
        return Vector3{nx/length*std::cos(phi),ny/length*std::cos(phi),std::sin(phi)};
    };
    const auto point=[&](int ring,int side) {
        const float theta=tau*static_cast<float>(ring%rings)/rings;
        const auto n=normal(ring,side);
        return Vector3{x+0.071f*std::cos(theta)+0.014f*n.X,
                       2.45f+0.100f*std::sin(theta)+0.014f*n.Y,z+0.014f*n.Z};
    };
    const auto first=grip.data().vertices.size();
    for (int ring=0;ring<rings;++ring)
        for (int side=0;side<sides;++side)
            grip.addSmoothTexturedQuad(
                {point(ring,side),point(ring+1,side),point(ring+1,side+1),point(ring,side+1)},
                {normal(ring,side),normal(ring+1,side),normal(ring+1,side+1),normal(ring,side+1)},
                {Vector2{},Vector2{},Vector2{},Vector2{}},Color(220,178,48,255));
    grip.projectXYDetailUVs(first,{x,2.45f,z},0.060f);
}

inline int addCarpetBindings(MeshBuilder& mesh,float begin,float end)
{
    using namespace Microsoft::Xna::Framework;
    if (end-begin<0.12f) return 0;
    const auto strip=[&](float x0,float x1,float z0,float z1) {
        mesh.addTexturedQuad({x0,0.099f,z0},{x0,0.099f,z1},
                             {x1,0.099f,z1},{x1,0.099f,z0},Color(49,62,68,255),
                             {x0/0.85f,z0/0.85f},{x0/0.85f,z1/0.85f},
                             {x1/0.85f,z1/0.85f},{x1/0.85f,z0/0.85f});
    };
    // Eight-millimetre woven edge tape follows the actual runner boundary.
    // Butt-jointed end tapes close the perimeter without overlapping faces.
    strip(-0.39f,-0.382f,begin,end);
    strip(0.382f,0.39f,begin,end);
    strip(-0.382f,0.382f,begin,begin+0.008f);
    strip(-0.382f,0.382f,end-0.008f,end);
    const int seams=std::max(0,static_cast<int>(std::floor((end-begin)/1.38f))-1);
    for (int i=1;i<=seams;++i)
    {
        const float z=begin+static_cast<float>(i)*1.38f;
        strip(-0.382f,0.382f,z-0.0015f,z+0.0015f);
    }
    return 4+seams;
}

inline void addWornCarpetRunner(MeshBuilder& mesh,float begin,float end)
{
    using namespace Microsoft::Xna::Framework;
    if (end-begin<0.12f) return;
    constexpr int columns=8;
    const int rows=std::clamp(static_cast<int>(std::ceil((end-begin)/1.25f)),1,32);
    const auto point=[&](int column,int row) {
        return Vector3{-0.39f+0.78f*static_cast<float>(column)/columns,0.098f,
                       begin+(end-begin)*static_cast<float>(row)/static_cast<float>(rows)};
    };
    const auto color=[&](const Vector3& p) {
        // Pile is slightly flattened and desaturated along the walked centre line;
        // protected edges retain their original blue-grey. No random dirt decals.
        const float centre=std::max(0.0f,1-p.X*p.X/(0.34f*0.34f));
        const float endDistance=std::min(p.Z-begin,end-p.Z);
        const float traffic=centre*centre*(0.78f+0.22f*std::exp(-endDistance/1.1f));
        return Color(50+static_cast<int>(14*traffic),64+static_cast<int>(11*traffic),
                     71+static_cast<int>(7*traffic),255);
    };
    const auto uv=[](const Vector3& p) { return Vector2{p.X/0.85f,p.Z/0.85f}; };
    for (int row=0;row<rows;++row)
        for (int column=0;column<columns;++column)
        {
            const auto a=point(column,row),b=point(column,row+1);
            const auto c=point(column+1,row+1),d=point(column+1,row);
            mesh.addTexturedQuad(a,b,c,d,color(a),color(b),color(c),color(d),
                                 uv(a),uv(b),uv(c),uv(d));
        }
}

inline void addSoftContactShadow(MeshBuilder& mesh,float x,float y,float z,
                                 float halfWidth,float halfDepth,int opacity)
{
    using namespace Microsoft::Xna::Framework;
    if (halfWidth<=0 || halfDepth<=0) return;
    constexpr int segments=16;
    constexpr float tau=6.283185307f;
    const Color dark(7,11,14,std::clamp(opacity,0,255)),clear(7,11,14,0);
    const auto point=[&](int i,float scale) {
        const float angle=tau*static_cast<float>(i%segments)/segments;
        return Vector3{x+halfWidth*scale*std::cos(angle),y,z+halfDepth*scale*std::sin(angle)};
    };
    for (int i=0;i<segments;++i)
    {
        const auto a=point(i,1),b=point(i+1,1),c=point(i+1,0.45f),d=point(i,0.45f);
        mesh.addQuad(b,a,d,c,clear,clear,dark,dark);
        mesh.addTriangle({x,y,z},c,d,dark);
    }
}

inline void addCeilingLightModule(MeshBuilder& housing,MeshBuilder& emitters,float begin,float end)
{
    using namespace Microsoft::Xna::Framework;
    if (end-begin<0.25f) return;
    const Color aluminium(151,162,164,255);
    housing.addBox({-0.115f,2.835f,begin},{0.115f,2.913f,end},Color(92,106,111,255));
    for (float side : {-1.0f,1.0f})
    {
        const float a=side*0.115f,b=side*0.094f;
        housing.addBox({std::min(a,b),2.815f,begin},{std::max(a,b),2.839f,end},aluminium);
    }
    for (float z : {begin,end-0.025f})
        housing.addBox({-0.094f,2.815f,z},{0.094f,2.839f,z+0.025f},aluminium);
    const std::array<float,4> xs{-0.093f,-0.075f,0.075f,0.093f};
    const std::array<float,4> zs{begin+0.026f,begin+0.10f,end-0.10f,end-0.026f};
    const auto point=[&](int column,int row) {
        return Vector3{xs[column],column==0 || column==3 ? 2.829f : 2.822f,zs[row]};
    };
    const auto color=[](int column,int row) {
        const bool edge=column==0 || column==3 || row==0 || row==3;
        return edge ? Color(219,219,206,255) : Color(246,241,222,255);
    };
    for (int column=0;column<3;++column)
        for (int row=0;row<3;++row)
            emitters.addQuad(point(column,row),point(column+1,row),
                             point(column+1,row+1),point(column,row+1),
                             color(column,row),color(column+1,row),
                             color(column+1,row+1),color(column,row+1));
}

// Soft, transparent-edged cabin-light approximation on a side pane. Unlike a
// translucent cuboid this has no bright cap faces or hard rectangular perimeter.
inline void addSoftWindowReflection(MeshBuilder& mesh,float x,float centreY,float centreZ,
                                    float halfHeight,float halfLength,int peakAlpha)
{
    using namespace Microsoft::Xna::Framework;
    constexpr int cells=4;
    const auto point=[&](int u,int v) {
        return Vector3{x,centreY+halfHeight*(2.0f*static_cast<float>(u)/cells-1),
                         centreZ+halfLength*(2.0f*static_cast<float>(v)/cells-1)};
    };
    const auto color=[&](int u,int v) {
        constexpr std::array<float,5> weight{0,0.5f,1,0.5f,0};
        return Color(224,237,242,static_cast<int>(static_cast<float>(peakAlpha)*weight[u]*weight[v]));
    };
    for (int u=0;u<cells;++u)
        for (int v=0;v<cells;++v)
        {
            const auto a=point(u,v),b=point(u+1,v),c=point(u+1,v+1),d=point(u,v+1);
            const auto ca=color(u,v),cb=color(u+1,v),cc=color(u+1,v+1),cd=color(u,v+1);
            // Wind toward the cabin; the opposite train side mirrors the order.
            if (x<0) mesh.addQuad(a,b,c,d,ca,cb,cc,cd);
            else mesh.addQuad(d,c,b,a,cd,cc,cb,ca);
        }
}

inline void addPassengerMullionCap(MeshBuilder& mesh,float side,float z,float halfWidth)
{
    using namespace Microsoft::Xna::Framework;
    if (halfWidth<0.025f) return;
    const auto first=mesh.data().vertices.size();
    const float a=side*1.394f,b=side*1.419f;
    mesh.addRoundedBoxXZ({std::min(a,b),0.845f,z-halfWidth+0.004f},
                         {std::max(a,b),2.355f,z+halfWidth-0.004f},
                         0.010f,3,Color(187,200,201,255));
    // Flush end-joint grooves stop short of the curved returns and the clear aperture.
    for (float y : {0.867f,2.330f})
    {
        const float front=side*1.393f,back=side*1.395f;
        mesh.addBox({std::min(front,back),y,z-halfWidth+0.023f},
                    {std::max(front,back),y+0.002f,z+halfWidth-0.023f},
                    Color(114,130,133,255));
    }
    mesh.projectDetailUVs(first,{side*1.394f,0.845f,z-halfWidth},0.30f);
}

// Closed, layered aperture around the existing pane. The outer flange overlaps the
// shell by 15 mm vertically and the mullions by 40 mm longitudinally. Geometry is
// train-local and joins the existing opaque batch; the transparent pane stays behind
// the gasket. Narrow spans next to passenger doors retain a positive clear opening.
inline void addPassengerWindowFrame(MeshBuilder& mesh, float side, float begin, float end)
{
    using namespace Microsoft::Xna::Framework;
    if (end-begin < 0.08f) return;
    constexpr int cornerSegments = 4;
    constexpr int points = 4*(cornerSegments+1);
    constexpr float halfPi = 1.570796327f;
    const float inset = std::min(0.06f,(end-begin)*0.20f);
    const float radius = std::min(0.10f,(end-begin-2*inset)*0.25f);
    const auto contour = [&](float x,float bottom,float top,float lo,float hi,float r) {
        r=std::min({r,(hi-lo)*0.49f,(top-bottom)*0.49f});
        std::array<Vector3,points> result{};
        for (int corner=0;corner<4;++corner)
        {
            const float cz = (corner==0 || corner==3) ? hi-r : lo+r;
            const float cy = corner<2 ? top-r : bottom+r;
            for (int step=0;step<=cornerSegments;++step)
            {
                const float angle=halfPi*(static_cast<float>(corner)
                    +static_cast<float>(step)/cornerSegments);
                result[corner*(cornerSegments+1)+step] =
                    {side*x,cy+r*std::sin(angle),cz+r*std::cos(angle)};
            }
        }
        return result;
    };
    const auto band = [&](const auto& a,const auto& b,const Color& color,bool outside) {
        for (int i=0;i<points;++i)
        {
            const int next=(i+1)%points;
            if (outside != (side>0)) mesh.addQuad(a[i],a[next],b[next],b[i],color);
            else mesh.addQuad(a[next],a[i],b[i],b[next],color);
        }
    };
    const auto flange=contour(1.414f,0.845f,2.355f,begin-0.04f,end+0.04f,0.01f);
    const auto lip=contour(1.396f,0.89f,2.31f,begin,end,radius+0.03f);
    const auto sealOuter=contour(1.434f,0.92f,2.28f,
                                  begin+inset*0.66f,end-inset*0.66f,radius+0.02f);
    const auto sealInner=contour(1.440f,0.94f,2.26f,begin+inset,end-inset,radius);
    const auto exteriorSeal=contour(1.461f,0.94f,2.26f,begin+inset,end-inset,radius);
    const auto exteriorFlange=contour(1.505f,0.845f,2.355f,
                                       begin-0.04f,end+0.04f,0.01f);
    band(flange,lip,Color(192,203,203,255),false);
    band(lip,sealOuter,Color(137,153,155,255),false);
    band(sealOuter,sealInner,Color(29,39,41,255),false);
    band(sealInner,exteriorSeal,Color(21,30,32,255),false);
    band(exteriorFlange,exteriorSeal,Color(54,65,68,255),true);
}

inline void addShapedSeatFabric(MeshBuilder& mesh, float xmin,float xmax,float ymin,float ymax,
                               float z, bool positiveZ,
                               const Microsoft::Xna::Framework::Color& color)
{
    using namespace Microsoft::Xna::Framework;
    constexpr float pi = 3.141592654f;
    constexpr float depth = 0.018f;
    constexpr int columns = 4, rows = 6;
    if (xmax <= xmin || ymax <= ymin) return;
    const float direction = positiveZ ? 1.0f : -1.0f;
    for (int row = 0; row < rows; ++row)
        for (int column = 0; column < columns; ++column)
        {
            std::array<Vector3,4> positions, normals;
            std::array<Vector2,4> uvs;
            constexpr std::array<int,4> dx{0,1,1,0}, dy{0,0,1,1};
            for (int corner = 0; corner < 4; ++corner)
            {
                const float u = static_cast<float>(column+dx[corner])/columns;
                const float v = static_cast<float>(row+dy[corner])/rows;
                const float su = std::sin(pi*u), sv = std::sin(pi*v);
                const float h = depth*su*su*sv*sv;
                const float hx = depth*pi*std::sin(2*pi*u)*sv*sv/(xmax-xmin);
                const float hy = depth*pi*std::sin(2*pi*v)*su*su/(ymax-ymin);
                positions[corner] = {xmin+(xmax-xmin)*u,ymin+(ymax-ymin)*v,z+direction*h};
                normals[corner] = {-hx,-hy,direction};
                uvs[corner] = {(positions[corner].X-xmin)/0.16f,
                               (positions[corner].Y-ymin)/0.16f};
            }
            if (!positiveZ)
            {
                std::swap(positions[0],positions[1]); std::swap(positions[2],positions[3]);
                std::swap(normals[0],normals[1]); std::swap(normals[2],normals[3]);
                std::swap(uvs[0],uvs[1]); std::swap(uvs[2],uvs[3]);
            }
            mesh.addSmoothTexturedQuad(positions,normals,uvs,color);
        }
}

// Rounded footprint shared by the padded cushion and its side welt.
inline constexpr int CushionContourPoints=16;
inline std::array<Microsoft::Xna::Framework::Vector3,CushionContourPoints>
cushionContour(float halfWidth,float halfDepth,float radius,float y)
{
    using namespace Microsoft::Xna::Framework;
    std::array<Vector3,CushionContourPoints> points{};
    constexpr float halfPi=1.570796327f;
    for (int corner=0;corner<4;++corner)
        for (int step=0;step<=3;++step)
        {
            const float angle=halfPi*(static_cast<float>(corner)+static_cast<float>(step)/3);
            points[corner*4+step]={
                (corner==0 || corner==3 ? halfWidth-radius : -halfWidth+radius)+radius*std::cos(angle),
                y,(corner<2 ? halfDepth-radius : -halfDepth+radius)+radius*std::sin(angle)};
        }
    return points;
}

inline void addShapedSeatCushion(MeshBuilder& mesh,float x,float z,
                               const Microsoft::Xna::Framework::Color& color)
{
    using namespace Microsoft::Xna::Framework;
    struct Ring { float width,depth,radius,y,uvExtra,normalY,normalRadial; };
    // A 24 mm sitting dish, a 50 mm waterfall edge, a straight side and a small
    // underside tuck. All remain inside the previous collision/seat envelope.
    const std::array<Ring,6> rings{{
        {0.0975f,0.15f,0.02f,0.6065f,0,1,-0.12f},
        {0.195f,0.30f,0.04f,0.62f,0,1,0},
        {0.2162132f,0.3353553f,0.0506066f,0.6053553f,0.004f,0.7071068f,0.7071068f},
        {0.225f,0.35f,0.055f,0.57f,0.027f,0,1},
        {0.225f,0.35f,0.055f,0.455f,0.142f,0,1},
        {0.205f,0.33f,0.045f,0.42f,0.202f,-1,0}
    }};
    struct Point { Vector3 position,normal; Vector2 uv; };
    std::array<std::array<Point,CushionContourPoints>,6> surface{};
    for (std::size_t ring=0;ring<rings.size();++ring)
    {
        const auto& r=rings[ring];
        const auto positions=cushionContour(r.width,r.depth,r.radius,r.y);
        // Unfold the fabric outwards around the roll and down the side. Unlike
        // dominant-face projection this does not switch UV axes halfway round it.
        const auto cloth=cushionContour(r.width+r.uvExtra,r.depth+r.uvExtra,
                                        r.radius+r.uvExtra,0);
        for (int i=0;i<CushionContourPoints;++i)
        {
            const float angle=1.570796327f*(static_cast<float>(i/4)+static_cast<float>(i%4)/3);
            surface[ring][i]={positions[i]+Vector3{x,0,z},
                {std::cos(angle)*r.normalRadial,r.normalY,std::sin(angle)*r.normalRadial},
                {(cloth[i].X+0.225f)/0.16f,(cloth[i].Z+0.35f)/0.16f}};
        }
    }
    const auto quad=[&](const Point& a,const Point& b,const Point& c,const Point& d) {
        mesh.addSmoothTexturedQuad({a.position,b.position,c.position,d.position},
            {a.normal,b.normal,c.normal,d.normal},{a.uv,b.uv,c.uv,d.uv},color);
    };
    for (std::size_t ring=0;ring+1<surface.size();++ring)
        for (int i=0;i<CushionContourPoints;++i)
        {
            const int next=(i+1)%CushionContourPoints;
            quad(surface[ring][i],surface[ring][next],surface[ring+1][next],surface[ring+1][i]);
        }
    const Point top{{x,0.596f,z},Vector3::Up,{0.225f/0.16f,0.35f/0.16f}};
    const Point bottom{{x,0.42f,z},{0,-1,0},top.uv};
    for (int i=0;i<CushionContourPoints;i+=2)
    {
        const int next=(i+1)%CushionContourPoints, end=(i+2)%CushionContourPoints;
        quad(top,surface.front()[end],surface.front()[next],surface.front()[i]);
        quad(bottom,surface.back()[i],surface.back()[next],surface.back()[end]);
    }
}

inline void addSeatCushionWelt(MeshBuilder& mesh,float x,float z,
                              const Microsoft::Xna::Framework::Color& color)
{
    using namespace Microsoft::Xna::Framework;
    const auto upper=cushionContour(0.2255f,0.3505f,0.0555f,0.565f);
    const auto lower=cushionContour(0.2255f,0.3505f,0.0555f,0.562f);
    const Vector3 offset{x,0,z};
    for (int i=0;i<CushionContourPoints;++i)
    {
        const int next=(i+1)%CushionContourPoints;
        mesh.addQuad(upper[i]+offset,upper[next]+offset,lower[next]+offset,lower[i]+offset,color);
    }
}

// Complete upholstered volume, not just a decal on its passenger-facing planes.
// Project at a fixed physical scale from the seat's own origin so different cars,
// directions and floating-origin positions never stretch or slide the weave.
inline void addSeatUpholsteryBody(MeshBuilder& mesh, float x, float z, float backZ,
                                 const Microsoft::Xna::Framework::Color& color)
{
    addShapedSeatCushion(mesh,x,z,color);
    const auto first=mesh.data().vertices.size();
    mesh.addRoundedBoxXY({x-0.225f,0.58f,backZ},{x+0.225f,1.56f,backZ+0.10f},
                         0.055f,2,color);
    mesh.addEllipsoid({x,1.505f,backZ+0.05f},{0.222f,0.105f,0.082f},24,10,color,true);
    mesh.projectDetailUVs(first,{x-0.225f,0.42f,z-0.35f},0.16f);
}

inline void addTableSurfaceWithCupRecesses(MeshBuilder& mesh,float x,float z,
                                          const Microsoft::Xna::Framework::Color& laminate)
{
    using namespace Microsoft::Xna::Framework;
    constexpr float pi=3.141592654f;
    const auto point=[&](float px,float pz,float y=0.860f) { return Vector3{x+px,y,z+pz}; };
    const auto rectangle=[&](float x0,float x1,float z0,float z1) {
        mesh.addQuad(point(x0,z0),point(x0,z1),point(x1,z1),point(x1,z0),laminate);
    };
    // Partition the top around two square patches, then join each patch to its
    // circular opening. No full top face remains underneath the recessed inserts.
    const float cupX=x>0 ? 0.06f : -0.06f;
    constexpr float patch=0.068f;
    rectangle(-0.432f,cupX-patch,-0.305f,0.305f);
    rectangle(cupX+patch,0.432f,-0.305f,0.305f);
    rectangle(cupX-patch,cupX+patch,-0.305f,-0.19f-patch);
    rectangle(cupX-patch,cupX+patch,-0.19f+patch,0.19f-patch);
    rectangle(cupX-patch,cupX+patch,0.19f+patch,0.305f);
    rectangle(-0.365f,0.365f,-0.372f,-0.305f);
    rectangle(-0.365f,0.365f,0.305f,0.372f);
    for (int corner=0;corner<4;++corner)
    {
        const float cx=(corner==0 || corner==3) ? 0.365f : -0.365f;
        const float cz=corner<2 ? 0.305f : -0.305f;
        for (int step=0;step<3;++step)
        {
            const float a=pi*0.5f*(static_cast<float>(corner)+static_cast<float>(step)/3);
            const float b=a+pi/6;
            mesh.addTriangle(point(cx,cz),point(cx+0.067f*std::cos(b),cz+0.067f*std::sin(b)),
                             point(cx+0.067f*std::cos(a),cz+0.067f*std::sin(a)),laminate);
        }
    }
    for (float cupZ : {-0.19f,0.19f})
    {
        for (int segment=0;segment<16;++segment)
        {
            const float a=2*pi*static_cast<float>(segment)/16;
            const float b=2*pi*static_cast<float>(segment+1)/16;
            const auto radial=[&](float angle,float radius,float height) {
                return point(cupX+radius*std::cos(angle),cupZ+radius*std::sin(angle),height);
            };
            const auto square=[&](float angle) {
                const float r=patch/std::max(std::abs(std::cos(angle)),std::abs(std::sin(angle)));
                return radial(angle,r,0.860f);
            };
            mesh.addQuad(square(b),square(a),radial(a,0.058f,0.860f),
                         radial(b,0.058f,0.860f),laminate);
            mesh.addQuad(radial(b,0.058f,0.860f),radial(a,0.058f,0.860f),
                         radial(a,0.050f,0.861f),radial(b,0.050f,0.861f),Color(191,203,204,255));
            mesh.addQuad(radial(b,0.050f,0.861f),radial(a,0.050f,0.861f),
                         radial(a,0.045f,0.848f),radial(b,0.045f,0.848f),Color(108,126,129,255));
        }
        mesh.addDiscXZ(point(cupX,cupZ,0.848f),0.045f,16,Color(66,82,85,255));
    }
}

inline void addPassengerTableTop(MeshBuilder& mesh,float x,float z,
                                 const Microsoft::Xna::Framework::Color& laminate)
{
    using namespace Microsoft::Xna::Framework;
    const auto first=mesh.data().vertices.size();
    // Set the tray inward so it cannot protrude past the rounded laminate corners.
    mesh.addRoundedBoxXZ({x-0.40f,0.775f,z-0.34f},{x+0.40f,0.803f,z+0.34f},
                         0.065f,3,Color(70,83,87,255));
    mesh.addRoundedBoxXZ({x-0.44f,0.797f,z-0.38f},{x+0.44f,0.845f,z+0.38f},
                         0.075f,3,Color(136,146,145,255));
    const std::array rings{
        cushionContour(0.440f,0.380f,0.075f,0.845f),
        cushionContour(0.438f,0.378f,0.073f,0.855f),
        cushionContour(0.432f,0.372f,0.067f,0.860f)};
    for (int ring=0;ring<2;++ring)
        for (int i=0;i<CushionContourPoints;++i)
        {
            const int next=(i+1)%CushionContourPoints;
            const Vector3 offset{x,0,z};
            mesh.addQuad(rings[ring][next]+offset,rings[ring][i]+offset,
                         rings[ring+1][i]+offset,rings[ring+1][next]+offset,laminate);
        }
    addTableSurfaceWithCupRecesses(mesh,x,z,laminate);
    mesh.projectDetailUVs(first,{x-0.44f,0.775f,z-0.38f},0.18f);
}

inline void addPassengerArmrest(MeshBuilder& frame,MeshBuilder& padding,float x,float z)
{
    using namespace Microsoft::Xna::Framework;
    frame.addRoundedBoxXZ({x-0.039f,0.88f,z-0.252f},{x+0.039f,0.901f,z+0.252f},
                          0.035f,3,Color(104,115,116,255));
    const Color vinyl(68,82,87,255);
    const auto first=padding.data().vertices.size();
    padding.addRoundedBoxXZ({x-0.045f,0.898f,z-0.27f},{x+0.045f,0.910f,z+0.27f},
                            0.042f,3,vinyl);
    const std::array rings{
        cushionContour(0.045f,0.270f,0.042f,0.910f),
        cushionContour(0.039f,0.264f,0.036f,0.931f),
        cushionContour(0.021f,0.246f,0.018f,0.940f)};
    const auto normal=[](int ring,int i) {
        const float angle=1.570796327f*(static_cast<float>(i/4)+static_cast<float>(i%4)/3);
        const float radial=ring==0 ? 1.0f : ring==1 ? 0.7071068f : 0.0f;
        const float up=ring==0 ? 0.0f : ring==1 ? 0.7071068f : 1.0f;
        return Vector3{std::cos(angle)*radial,up,std::sin(angle)*radial};
    };
    for (int ring=0;ring<2;++ring)
        for (int i=0;i<CushionContourPoints;++i)
        {
            const int next=(i+1)%CushionContourPoints;
            const Vector3 offset{x,0,z};
            padding.addQuad(rings[ring][next]+offset,rings[ring][i]+offset,
                            rings[ring+1][i]+offset,rings[ring+1][next]+offset,vinyl,
                            normal(ring,next),normal(ring,i),
                            normal(ring+1,i),normal(ring+1,next));
        }
    padding.addRoundedBoxXZ({x-0.021f,0.939f,z-0.246f},{x+0.021f,0.940f,z+0.246f},
                            0.018f,3,vinyl);
    padding.projectDetailUVs(first,{x-0.045f,0.898f,z-0.27f},0.06f);
}

inline void addSeatHeadCover(MeshBuilder& mesh, float x, float backZ, bool forward,
                             const Microsoft::Xna::Framework::Color& color)
{
    using namespace Microsoft::Xna::Framework;
    // A washable cloth hangs down the front and wraps over the padded crown.
    // Its elliptical top clears the cushion; shallow flutes relax toward the top.
    constexpr int columns=12, rows=9;
    constexpr float pi=3.141592654f;
    const float direction=forward ? -1.0f : 1.0f;
    std::array<std::array<Vector3,columns+1>,rows> cloth{};
    for (int row=0;row<rows;++row)
        for (int col=0;col<=columns;++col)
        {
            const float u=static_cast<float>(col)/columns;
            const float dx=(u-0.5f)*0.38f;
            const float fold=0.0025f*(1-std::cos(u*6*pi));
            float y,depth;
            if (row<3)
            {
                y=1.31f+static_cast<float>(row)*0.0975f;
                depth=0.088f+fold*(1-static_cast<float>(row)*0.35f);
            }
            else
            {
                const float angle=static_cast<float>(row-2)*pi/10;
                y=1.505f+0.110f*std::sqrt(1-dx*dx/(0.226f*0.226f))*std::sin(angle);
                depth=0.088f*std::cos(angle);
            }
            cloth[row][col]={x+dx,y,backZ+0.05f+direction*depth};
        }
    const auto normal=[&](int row,int col) {
        const auto across=cloth[row][std::min(col+1,columns)]-cloth[row][std::max(col-1,0)];
        const auto along=cloth[std::min(row+1,rows-1)][col]-cloth[std::max(row-1,0)][col];
        return Vector3::Cross(across,along);
    };
    for (int row=0;row<rows-1;++row)
        for (int col=0;col<columns;++col)
        {
            const auto a=cloth[row][col], b=cloth[row][col+1];
            const auto c=cloth[row+1][col+1], d=cloth[row+1][col];
            // Two-sided thin fabric, not a solid plastic plaque. No new material batch.
            const auto na=normal(row,col),nb=normal(row,col+1);
            const auto nc=normal(row+1,col+1),nd=normal(row+1,col);
            mesh.addQuad(a,b,c,d,color,na,nb,nc,nd);
            mesh.addQuad(d,c,b,a,color,-nd,-nc,-nb,-na);
        }
    // Turned lower hem follows the same flutes rather than floating over the cloth.
    for (int col=0;col<columns;++col)
    {
        auto a=cloth[0][col],b=cloth[0][col+1];
        a.Z+=direction*0.001f; b.Z+=direction*0.001f;
        const Vector3 height{0,0.004f,0};
        mesh.addQuad(a,b,b+height,a+height,color);
        mesh.addQuad(a+height,b+height,b,a,color);
    }
}

inline void addSeatRearShell(MeshBuilder& mesh, float x, float rearZ, float direction,
                             const Microsoft::Xna::Framework::Color& shell)
{
    using namespace Microsoft::Xna::Framework;
    const auto firstVertex=mesh.data().vertices.size();
    // The rear reference is 64 mm from the backrest centre, but the rounded crown reaches
    // 82 mm. Clear its complete envelope, just as the passenger-facing head cover does.
    rearZ += direction*0.020f;
    const auto panel = [&](float halfWidth,float bottom,float top,float near,float far,
                           float radius,const Color& color) {
        const float a = rearZ+direction*near;
        const float b = rearZ+direction*far;
        mesh.addRoundedBoxXY({x-halfWidth,bottom,std::min(a,b)},
                             {x+halfWidth,top,std::max(a,b)},radius,3,color);
    };
    // A narrow return and inclined perimeter give the injection-moulded shell depth.
    // Both contours share corner samples, so there are no cracks at the bevel ends.
    panel(0.207f,0.68f,1.52f,0.004f,0.005f,0.038f,shell);
    panel(0.193f,0.694f,1.506f,0.011f,0.012f,0.024f,shell);
    const auto outer=cushionContour(0.207f,0.42f,0.038f,0);
    const auto inner=cushionContour(0.193f,0.406f,0.024f,0);
    for (int i=0;i<CushionContourPoints;++i)
    {
        const int next=(i+1)%CushionContourPoints;
        const auto point=[&](const Vector3& p,float depth) {
            return Vector3{x+p.X,1.10f+p.Z,rearZ+direction*depth};
        };
        const auto a=point(outer[i],0.005f),b=point(outer[next],0.005f);
        const auto c=point(inner[next],0.012f),d=point(inner[i],0.012f);
        if (direction>0) mesh.addQuad(a,b,c,d,shell);
        else mesh.addQuad(d,c,b,a,shell);
    }
    // Layered recess and pocket mouth remain inside the existing seat footprint.
    panel(0.117f,1.328f,1.442f,0.0125f,0.018f,0.031f,Color(86,101,107,255));
    panel(0.105f,1.34f,1.43f,0.0185f,0.019f,0.025f,Color(16,27,34,255));
    panel(0.081f,1.343f,1.352f,0.0195f,0.025f,0.004f,Color(105,116,118,255));
    panel(0.160f,0.80f,1.08f,0.013f,0.030f,0.022f,Color(67,84,91,255));
    panel(0.142f,1.035f,1.066f,0.0305f,0.032f,0.010f,Color(23,34,39,255));
    panel(0.158f,1.021f,1.038f,0.030f,0.035f,0.007f,Color(132,143,143,255));
    // Object-local millimetre-scale polymer grain, independent of route position.
    mesh.projectXYDetailUVs(firstVertex,{x-0.207f,0.68f,rearZ},0.12f);
}

// Open aluminium shelf with a rounded aisle lip. Gaps expose the cabin above rather than
// making the entire rack a dark solid slab at passenger eye level.
inline void addLuggageRackSpan(MeshBuilder& mesh, float side, float z0, float z1)
{
    using namespace Microsoft::Xna::Framework;
    if (z1 <= z0) return;
    for (int slat = 0; slat < 6; ++slat)
    {
        const float inner = 0.884f+static_cast<float>(slat)*0.060f;
        const float outer = inner+0.032f;
        mesh.addRoundedBoxXY({side > 0 ? inner : -outer,2.214f,z0},
                             {side > 0 ? outer : -inner,2.230f,z1},
                             0.008f,3,Color(179,191,192,255));
    }
    mesh.addRoundedBoxXY({side > 0 ? 0.85f : -0.884f,2.210f,z0},
                         {side > 0 ? 0.884f : -0.85f,2.248f,z1},
                         0.015f,3,Color(163,178,180,255));
    mesh.addRoundedBoxXY({side > 0 ? 1.217f : -1.235f,2.214f,z0},
                         {side > 0 ? 1.235f : -1.217f,2.230f,z1},
                         0.008f,3,Color(163,178,180,255));
    // Short end rails close the cut extrusion ends, including the toilet cut-out.
    const float cap=std::min(0.010f,(z1-z0)*0.25f);
    for (const float begin : {z0,z1-cap})
        mesh.addBox({side > 0 ? 0.85f : -1.235f,2.214f,begin},
                    {side > 0 ? 1.235f : -0.85f,2.230f,begin+cap},Color(163,178,180,255));
}

inline void addLuggageRackBracket(MeshBuilder& mesh,float side,float z)
{
    using namespace Microsoft::Xna::Framework;
    const Color aluminium(153,170,172,255);
    // Tapered support blade, deep at its bolted wall root and thin at the aisle lip.
    std::array<Vector2,4> section={Vector2{0.86f,2.194f},{1.40f,2.123f},
                                   {1.40f,2.214f},{0.86f,2.214f}};
    if (side<0) std::reverse(section.begin(),section.end());
    const auto point=[&](int i,float depth) {
        return Vector3{side*section[i%4].X,section[i%4].Y,z+depth};
    };
    mesh.addQuad(point(0,0.011f),point(1,0.011f),point(2,0.011f),point(3,0.011f),aluminium);
    mesh.addQuad(point(3,-0.011f),point(2,-0.011f),point(1,-0.011f),point(0,-0.011f),aluminium);
    for (int i=0;i<4;++i)
        mesh.addQuad(point(i,0.011f),point(i,-0.011f),point(i+1,-0.011f),
                     point(i+1,0.011f),aluminium);
    mesh.addRoundedBoxYZ({side>0 ? 1.375f : -1.402f,2.08f,z-0.029f},
                         {side>0 ? 1.402f : -1.375f,2.26f,z+0.029f},
                         0.012f,3,Color(170,182,183,255));
    for (float y : {2.102f,2.240f})
        mesh.addEllipsoid({side*1.374f,y,z},{0.003f,0.006f,0.006f},8,4,
                           Color(91,108,113,255),true);
}

// Quarter-round interior skin, joining the wall tangentially to the ceiling.
inline void addCeilingCove(MeshBuilder& mesh, float side, float z0, float z1,
                           const Microsoft::Xna::Framework::Color& color)
{
    using namespace Microsoft::Xna::Framework;
    constexpr int segments = 8;
    constexpr float radius = 0.25f;
    constexpr float halfPi = 1.570796327f;
    constexpr float tile = 0.90f;
    for (int i = 0; i < segments; ++i)
    {
        const float a = halfPi*static_cast<float>(i)/segments;
        const float b = halfPi*static_cast<float>(i+1)/segments;
        const Vector3 p{side*(1.166f+radius*std::cos(a)),2.664f+radius*std::sin(a),z0};
        const Vector3 q{side*(1.166f+radius*std::cos(b)),2.664f+radius*std::sin(b),z0};
        const Vector3 length{0,0,z1-z0};
        const Vector2 pa{z0/tile,a*radius/tile}, pb{z1/tile,a*radius/tile};
        const Vector2 qa{z0/tile,b*radius/tile}, qb{z1/tile,b*radius/tile};
        if (side > 0)
            mesh.addTexturedQuad(p,p+length,q+length,q,color,pa,pb,qb,qa);
        else
            mesh.addTexturedQuad(p+length,p,q,q+length,color,pb,pa,qa,qb);
    }
}
} // namespace TrainSim::Render
