#pragma once

#include "trainsim/render/LinesideGeometry.hpp"
#include "trainsim/render/MeshBuilder.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace TrainSim::Render {

// Terminus composition shared by both route endpoints. Point(distance,lateral) returns a local
// route point {x,y,z,yaw} at rail level; `inward` is +1 at the first station (the route
// continues toward increasing distance) and -1 at the last. Distances are measured from the
// station datum; the platform ends 120 m beyond it, the buffer stops sit just before that end
// and the head house closes the tracks behind them.
inline constexpr double kTerminalTrackOverrunMetres=125.0;
inline constexpr double kTerminalBufferOffset=117.0;
inline constexpr double kTerminalHeadHouseNear=124.0;
inline constexpr double kTerminalHeadHouseFar=146.0;
inline constexpr double kTerminalShedBegin=30.0;
inline constexpr double kTerminalShedEnd=118.0;
inline constexpr float kTerminalShedRoofHeight=8.6f;

namespace detail {
template<class Point>
void routeBox(MeshBuilder& mesh,Point& point,double distance,double lateral,float y,
              float sx,float sy,float sz,const Microsoft::Xna::Framework::Color& colour)
{
    using namespace Microsoft::Xna::Framework;
    const auto p=point(distance,lateral);
    mesh.addBox(Vector3{-0.5f,-0.5f,-0.5f},Vector3{0.5f,0.5f,0.5f},colour,
                Matrix::CreateScale(sx,sy,sz)*Matrix::CreateRotationY(p.yaw)
                *Matrix::CreateTranslation(p.x,p.y+y,p.z));
}
template<class Point>
Microsoft::Xna::Framework::Vector3 routeAt(Point& point,double distance,double lateral,float y)
{
    const auto p=point(distance,lateral);
    return {p.x,p.y+y,p.z};
}
} // namespace detail

// Friction buffer stop: sloped rail struts from the running rails to a red/white beam head.
template<class Point>
void addBufferStop(MeshBuilder& mesh,Point point,double distance,double lateral,double inward)
{
    using namespace Microsoft::Xna::Framework;
    const Color steel(70,74,76,255),beam(196,44,40,255),stripe(236,234,222,255);
    const double behind=distance-inward*2.6;
    for (double rail : {-0.76,0.76})
    {
        addBarBetween(mesh,detail::routeAt(point,behind,lateral+rail,0.16f),
                      detail::routeAt(point,distance,lateral+rail,0.95f),0.11f,steel);
        detail::routeBox(mesh,point,behind,lateral+rail,0.22f,0.26f,0.12f,0.60f,steel);
        addBarBetween(mesh,detail::routeAt(point,distance,lateral+rail,0.16f),
                      detail::routeAt(point,distance,lateral+rail,0.95f),0.09f,steel);
    }
    detail::routeBox(mesh,point,distance,lateral,0.98f,2.10f,0.34f,0.30f,beam);
    for (double across : {-0.70,0.0,0.70})
        detail::routeBox(mesh,point,distance+inward*0.01,lateral+across,0.98f,0.28f,0.36f,0.31f,
                         stripe);
    detail::routeBox(mesh,point,distance,lateral,1.30f,0.10f,0.30f,0.10f,steel);
    detail::routeBox(mesh,point,distance,lateral,1.50f,0.24f,0.12f,0.12f,Color(214,46,40,255));
}

// Head house closing both tracks: a tall masonry mass with a glazed concourse front facing the
// platforms, a canopy over the entrances, a clock and a parapet. The caller prints the name.
template<class Point>
void addTerminusHeadHouse(MeshBuilder& mesh,Point point,double datum,double inward,double lateral)
{
    using namespace Microsoft::Xna::Framework;
    const Color masonry(151,145,126,255),masonryDark(126,120,104,255),glass(70,125,145,255);
    const Color frame(53,62,66,255),parapet(66,68,67,255),canopy(96,104,106,255);
    const double near=datum-inward*kTerminalHeadHouseNear,far=datum-inward*kTerminalHeadHouseFar;
    const double middle=(near+far)*0.5;
    const float depth=static_cast<float>(kTerminalHeadHouseFar-kTerminalHeadHouseNear);
    constexpr float width=66.0f,height=15.0f;
    // Main mass with a buried foundation, parapet and two corner pavilions.
    detail::routeBox(mesh,point,middle,lateral,height*0.5f-1.0f,width,height+2.0f,depth,masonry);
    detail::routeBox(mesh,point,middle,lateral,height+0.3f,width+0.8f,0.6f,depth+0.8f,parapet);
    for (double across : {-width*0.5+5.0,width*0.5-5.0})
    {
        detail::routeBox(mesh,point,middle,lateral+across,height*0.5f+1.4f,10.6f,height+2.8f,
                         depth+1.2f,masonryDark);
        detail::routeBox(mesh,point,middle,lateral+across,height+3.3f,11.4f,0.6f,depth+2.0f,parapet);
    }
    // Glazed concourse front between the pavilions, divided by mullions and a transom.
    const double face=near+inward*0.15;
    detail::routeBox(mesh,point,face,lateral,7.0f,width-21.2f,12.0f,0.30f,glass);
    for (double across=-20.0;across<=20.01;across+=4.0)
        detail::routeBox(mesh,point,face-inward*0.1,lateral+across,7.0f,0.28f,12.2f,0.5f,frame);
    for (float y : {1.0f,4.6f,9.8f,13.0f})
        detail::routeBox(mesh,point,face-inward*0.1,lateral,y,width-21.0f,0.30f,0.5f,frame);
    // Entrance canopy and doors.
    detail::routeBox(mesh,point,face-inward*2.2,lateral,4.5f,width-20.0f,0.35f,4.6f,canopy);
    for (double across=-14.0;across<=14.01;across+=7.0)
        detail::routeBox(mesh,point,face-inward*4.3,lateral+across,2.2f,0.18f,4.5f,0.18f,frame);
    for (double across : {-9.0,-3.0,3.0,9.0})
        detail::routeBox(mesh,point,face-inward*0.2,lateral+across,1.6f,2.2f,3.2f,0.3f,
                         Color(40,52,58,255));
    // Clock on the attic between the pavilions.
    detail::routeBox(mesh,point,face-inward*0.05,lateral,13.0f,2.4f,2.4f,0.25f,
                     Color(238,239,224,255));
    detail::routeBox(mesh,point,face-inward*0.25,lateral,13.0f,2.7f,2.7f,0.12f,frame);
    detail::routeBox(mesh,point,face-inward*0.28,lateral,13.55f,0.09f,1.0f,0.05f,frame);
    detail::routeBox(mesh,point,face-inward*0.28,lateral+0.35,13.0f,0.75f,0.09f,0.05f,frame);
}

// Train shed: a high gabled roof with a glazed clerestory on paired columns, spanning both
// platform tracks for the last stretch before the buffer stops.
template<class Point>
int addTrainShed(MeshBuilder& mesh,Point point,double datum,double inward,double lateral)
{
    using namespace Microsoft::Xna::Framework;
    const Color steel(88,96,99,255),roof(150,156,156,255),eave(118,124,125,255);
    const Color clerestory(168,198,208,255),ridge(96,104,106,255);
    const double begin=datum-inward*kTerminalShedBegin,end=datum-inward*kTerminalShedEnd;
    const double middle=(begin+end)*0.5;
    const float length=static_cast<float>(kTerminalShedEnd-kTerminalShedBegin);
    constexpr float halfSpan=15.5f,eaveHeight=7.4f;
    int columns=0;
    for (double along=kTerminalShedBegin;along<=kTerminalShedEnd+0.01;along+=11.0)
        for (double across : {-halfSpan,halfSpan})
        {
            detail::routeBox(mesh,point,datum-inward*along,lateral+across,eaveHeight*0.5f,
                             0.55f,eaveHeight,0.55f,steel);
            detail::routeBox(mesh,point,datum-inward*along,lateral+across,0.12f,1.1f,0.24f,1.1f,
                             steel);
            ++columns;
        }
    for (double across : {-halfSpan,halfSpan})
        detail::routeBox(mesh,point,middle,lateral+across,eaveHeight+0.3f,0.7f,0.6f,length,steel);
    // Two roof slopes from the eave beams to the ridge, each as a sloped slab.
    for (float side : {-1.0f,1.0f})
    {
        const Vector3 a=detail::routeAt(point,begin,lateral+side*halfSpan,eaveHeight+0.6f);
        const Vector3 b=detail::routeAt(point,end,lateral+side*halfSpan,eaveHeight+0.6f);
        const Vector3 c=detail::routeAt(point,end,lateral+side*3.8,kTerminalShedRoofHeight+1.6f);
        const Vector3 d=detail::routeAt(point,begin,lateral+side*3.8,kTerminalShedRoofHeight+1.6f);
        const Vector3 outward{0.0f,1.0f,0.0f};
        if (Vector3::Dot(Vector3::Cross(b-a,c-a),outward)>=0.0f) mesh.addQuad(a,b,c,d,roof);
        else mesh.addQuad(d,c,b,a,roof);
        const Vector3 down{0.0f,-1.0f,0.0f};
        const Vector3 a2=a+Vector3{0,-0.25f,0},b2=b+Vector3{0,-0.25f,0};
        const Vector3 c2=c+Vector3{0,-0.25f,0},d2=d+Vector3{0,-0.25f,0};
        if (Vector3::Dot(Vector3::Cross(b2-a2,c2-a2),down)>=0.0f) mesh.addQuad(a2,b2,c2,d2,eave);
        else mesh.addQuad(d2,c2,b2,a2,eave);
    }
    // Clerestory ridge box with pale glazing strips and a dark capping.
    detail::routeBox(mesh,point,middle,lateral,kTerminalShedRoofHeight+2.0f,7.6f,1.2f,length,ridge);
    for (double across : {-3.82,3.82})
        detail::routeBox(mesh,point,middle,lateral+across,kTerminalShedRoofHeight+2.0f,0.1f,0.9f,
                         length-1.0f,clerestory);
    detail::routeBox(mesh,point,middle,lateral,kTerminalShedRoofHeight+2.7f,8.2f,0.25f,length+0.6f,
                     ridge);
    return columns;
}

// Taxi rank on the forecourt apron: a row of parked cars with roof signs and a rank post.
// Ground(along,across) returns the apron surface; `heading` is the yaw of the platform edge.
template<class Ground>
int addTaxiRank(MeshBuilder& mesh,Ground ground,float heading,unsigned seed)
{
    using namespace Microsoft::Xna::Framework;
    const std::array<Color,4> paint={Color(222,212,96,255),Color(57,61,63,255),
                                     Color(225,220,204,255),Color(44,92,142,255)};
    int cars=0;
    for (int car=0;car<4;++car)
    {
        const float along=-72.0f+static_cast<float>(car)*7.2f;
        const float across=23.5f;
        const Vector3 root=ground(along,across);
        const Matrix placement=Matrix::CreateRotationY(heading)*Matrix::CreateTranslation(root);
        const Color body=paint[(seed+static_cast<unsigned>(car))%paint.size()];
        mesh.addBox(Vector3{-0.86f,0.42f,-2.15f},Vector3{0.86f,1.02f,2.15f},body,placement);
        mesh.addBox(Vector3{-0.78f,1.02f,-1.05f},Vector3{0.78f,1.50f,0.95f},Color(42,72,84,255),
                    placement);
        mesh.addBox(Vector3{-0.70f,1.50f,-0.95f},Vector3{0.70f,1.55f,0.85f},body,placement);
        for (float x : {-0.84f,0.84f})
            for (float z : {-1.35f,1.35f})
                mesh.addBox(Vector3{x-0.11f,0.02f,z-0.30f},Vector3{x+0.11f,0.62f,z+0.30f},
                            Color(29,31,31,255),placement);
        mesh.addBox(Vector3{-0.30f,1.55f,-0.12f},Vector3{0.30f,1.72f,0.12f},Color(236,194,44,255),
                    placement);
        ++cars;
    }
    const Vector3 post=ground(-48.0f,20.4f);
    mesh.addBox(Vector3{post.X-0.05f,post.Y,post.Z-0.05f},Vector3{post.X+0.05f,post.Y+2.9f,post.Z+0.05f},
                Color(67,72,72,255));
    mesh.addBox(Vector3{post.X-0.45f,post.Y+2.5f,post.Z-0.05f},Vector3{post.X+0.45f,post.Y+2.9f,post.Z+0.05f},
                Color(236,194,44,255));
    return cars;
}

} // namespace TrainSim::Render
