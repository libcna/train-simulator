#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/render/TrainExteriorGeometry.hpp"
#include "trainsim/world/OtherTrainConsist.hpp"

#include <algorithm>
#include <cmath>

namespace TrainSim::Render {

// Oncoming and parked traffic vehicles (ENV-137) in vehicle-local metres: X across, Y above the
// rail head, Z along the vehicle with the leading end toward +Z. The renderer turns each mesh a
// half turn so +Z faces decreasing route distance, the direction these services travel. Every
// vehicle is one complete mesh, so a consist costs one draw per vehicle.
namespace other_train_detail {
using namespace Microsoft::Xna::Framework;
inline constexpr float kPi = 3.14159265358979323846f;

// Turn a mesh half round about the vertical axis, moving its +Z end to -Z.
inline void turnHalfRound(MeshData& data)
{
    for (LitColorVertex& vertex : data.vertices)
    {
        vertex.position.X = -vertex.position.X;
        vertex.position.Z = -vertex.position.Z;
        vertex.normal.X = -vertex.normal.X;
        vertex.normal.Z = -vertex.normal.Z;
    }
}

inline void box(MeshBuilder& mesh,float x0,float y0,float z0,float x1,float y1,float z1,
                const Color& colour)
{
    mesh.addBox(Vector3{std::min(x0,x1),std::min(y0,y1),std::min(z0,z1)},
                Vector3{std::max(x0,x1),std::max(y0,y1),std::max(z0,z1)},colour);
}

// Closed prism between two cap centres; u and v span the cross-section.
inline void addPrism(MeshBuilder& mesh,const Vector3& c0,const Vector3& c1,const Vector3& u,
                     const Vector3& v,float radius,int sides,const Color& colour,bool caps=true)
{
    const Vector3 axis = c1-c0;
    for (int i = 0; i < sides; ++i)
    {
        const float a0 = 2.0f*kPi*static_cast<float>(i)/static_cast<float>(sides);
        const float a1 = 2.0f*kPi*static_cast<float>(i+1)/static_cast<float>(sides);
        const Vector3 o0 = (u*std::cos(a0)+v*std::sin(a0))*radius;
        const Vector3 o1 = (u*std::cos(a1)+v*std::sin(a1))*radius;
        detail::orientedQuad(mesh,c0+o0,c1+o0,c1+o1,c0+o1,o0+o1,colour);
        if (!caps) continue;
        detail::orientedTriangle(mesh,c0,c0+o0,c0+o1,-axis,colour);
        detail::orientedTriangle(mesh,c1,c1+o0,c1+o1,axis,colour);
    }
}

// Elliptic roof extruded along Z from x = -halfWidth to +halfWidth, with flat end caps.
inline void addArcRoof(MeshBuilder& mesh,float halfWidth,float baseY,float rise,float z0,
                       float z1,int segments,const Color& colour)
{
    const auto point = [&](int i,float z) {
        const float t = kPi*static_cast<float>(i)/static_cast<float>(segments);
        return Vector3{-std::cos(t)*halfWidth,baseY+std::sin(t)*rise,z};
    };
    for (int i = 0; i < segments; ++i)
    {
        const Vector3 a = point(i,z0), b = point(i+1,z0);
        const Vector3 mid = (a+b)*0.5f;
        const Vector3 outward{mid.X/(halfWidth*halfWidth),(mid.Y-baseY)/(rise*rise),0.0f};
        detail::orientedQuad(mesh,a,point(i,z1),point(i+1,z1),b,outward,colour);
        detail::orientedTriangle(mesh,Vector3{0.0f,baseY,z0},a,b,Vector3{0,0,-1},colour);
        detail::orientedTriangle(mesh,Vector3{0.0f,baseY,z1},point(i,z1),point(i+1,z1),
                                 Vector3{0,0,1},colour);
    }
}

// Bogie with inside wheels on the 1.52 m rail centres and outside frames and axleboxes.
inline void addBogie(MeshBuilder& mesh,float z,float wheelbase,float wheelRadius,
                     const Color& frame)
{
    const Color wheel(46,48,49,255), tread(104,106,104,255), axlebox(58,61,62,255);
    const Color spring(72,60,48,255);
    const float frameTop = wheelRadius+0.30f;
    for (float side : {-1.0f,1.0f})
    {
        box(mesh,side*0.90f,wheelRadius-0.12f,z-wheelbase*0.5f-0.55f,
            side*1.04f,frameTop,z+wheelbase*0.5f+0.55f,frame);
        box(mesh,side*1.04f,wheelRadius+0.02f,z-0.38f,side*1.10f,frameTop-0.04f,z+0.38f,spring);
        for (float axle : {-0.5f,0.5f})
        {
            const float axleZ = z+axle*wheelbase;
            addPrism(mesh,Vector3{side*0.69f,wheelRadius,axleZ},
                     Vector3{side*0.83f,wheelRadius,axleZ},Vector3{0,1,0},Vector3{0,0,1},
                     wheelRadius,12,wheel);
            addPrism(mesh,Vector3{side*0.83f,wheelRadius,axleZ},
                     Vector3{side*0.845f,wheelRadius,axleZ},Vector3{0,1,0},Vector3{0,0,1},
                     wheelRadius*0.45f,8,tread);
            box(mesh,side*0.92f,wheelRadius-0.13f,axleZ-0.17f,side*1.12f,wheelRadius+0.13f,
                axleZ+0.17f,axlebox);
        }
    }
    box(mesh,-0.90f,frameTop-0.20f,z-0.30f,0.90f,frameTop,z+0.30f,frame);
}

// Side buffers and screw-coupling hook at one end. sign selects the end; endZ is the headstock.
inline void addBuffers(MeshBuilder& mesh,float sign,float endZ,float halfPitch,float height,
                       const Color& beam)
{
    const Color steel(52,55,56,255), head(132,134,130,255);
    box(mesh,-1.32f,height-0.24f,sign*(endZ-0.12f),1.32f,height+0.24f,sign*endZ,beam);
    const float headZ = halfPitch-0.06f;
    for (float x : {-0.875f,0.875f})
    {
        addPrism(mesh,Vector3{x,height,sign*endZ},Vector3{x,height,sign*(headZ-0.07f)},
                 Vector3{1,0,0},Vector3{0,1,0},0.10f,8,steel);
        addPrism(mesh,Vector3{x,height,sign*(headZ-0.07f)},Vector3{x,height,sign*headZ},
                 Vector3{1,0,0},Vector3{0,1,0},0.19f,10,head);
    }
    box(mesh,-0.07f,height-0.10f,sign*endZ,0.07f,height+0.08f,sign*(headZ-0.20f),steel);
}

// Raised (one strip touching the 5.85 m contact wire) or lowered single-arm pantograph.
inline void addPantograph(MeshBuilder& mesh,float baseY,float z,bool raised)
{
    const Color steel(67,72,71,255), strip(44,48,47,255), insulator(150,92,60,255);
    for (float x : {-0.55f,0.55f})
        addPrism(mesh,Vector3{x,baseY-0.20f,z},Vector3{x,baseY,z},Vector3{1,0,0},
                 Vector3{0,0,1},0.07f,8,insulator);
    box(mesh,-0.72f,baseY,z-0.68f,0.72f,baseY+0.10f,z-0.54f,steel);
    box(mesh,-0.72f,baseY,z+0.54f,0.72f,baseY+0.10f,z+0.68f,steel);
    if (!raised)
    {
        box(mesh,-0.60f,baseY+0.10f,z-0.62f,0.60f,baseY+0.16f,z+0.62f,steel);
        box(mesh,-1.10f,baseY+0.16f,z-0.08f,1.10f,baseY+0.23f,z+0.08f,strip);
        return;
    }
    const float base = baseY+0.10f;
    constexpr float top = 5.67f;
    const auto arm = [&](float baseX,float topX,float armZ) {
        const float deltaX = topX-baseX, deltaY = top-base;
        const float length = std::hypot(deltaX,deltaY);
        const Matrix placement = Matrix::CreateRotationZ(std::atan2(deltaY,deltaX))
            * Matrix::CreateTranslation((baseX+topX)*0.5f,(base+top)*0.5f,armZ);
        mesh.addBox(Vector3{-length*0.5f,-0.045f,-0.045f},Vector3{length*0.5f,0.045f,0.045f},
                    steel,placement);
    };
    for (float armZ : {z-0.58f,z+0.58f})
    {
        arm(-0.62f,0.0f,armZ);
        arm(0.62f,0.0f,armZ);
    }
    box(mesh,-0.08f,5.62f,z-0.66f,0.08f,5.73f,z+0.66f,steel);
    box(mesh,-1.12f,5.78f,z-0.09f,1.12f,5.86f,z+0.09f,strip);
}

// Sloped driving-cab nose at +Z: a vertical lower face to noseZ, a raked windscreen up to
// (topY, screenTopZ), and side cheeks. Headlights are lit white on a leading end, tail lamps red
// on a trailing end and both are dark on an inner (coupled) end.
enum class CabLamps : std::uint8_t { Leading, Trailing, Dark };
inline void addRakedCab(MeshBuilder& mesh,float bodyEndZ,float noseZ,float sillY,float waistY,
                        float topY,float halfWidth,const Color& body,const Color& nose,
                        CabLamps lamps)
{
    const Color glass(34,52,62,255), frame(24,28,30,255);
    const float w = halfWidth-0.04f;
    box(mesh,-w,sillY,bodyEndZ,w,waistY,noseZ,nose);
    const Vector3 bl{-w,waistY,noseZ}, br{w,waistY,noseZ};
    const Vector3 tl{-w,topY,bodyEndZ}, tr{w,topY,bodyEndZ};
    detail::orientedQuad(mesh,bl,br,tr,tl,Vector3{0,noseZ-bodyEndZ,topY-waistY},body);
    for (float side : {-1.0f,1.0f})
        detail::orientedTriangle(mesh,Vector3{side*w,waistY,bodyEndZ},
                                 Vector3{side*w,waistY,noseZ},Vector3{side*w,topY,bodyEndZ},
                                 Vector3{side,0,0},body);
    // Windscreen inset on the raked face, lifted along the face normal.
    const Vector3 rake = Vector3::Normalize(Vector3{0.0f,topY-waistY,bodyEndZ-noseZ});
    const Vector3 normal = Vector3::Normalize(Vector3{0.0f,noseZ-bodyEndZ,topY-waistY});
    const float rakeLength = std::hypot(topY-waistY,noseZ-bodyEndZ);
    const auto onFace = [&](float x,float along,float lift) {
        return Vector3{x,waistY,noseZ}+rake*along+normal*lift;
    };
    detail::orientedQuad(mesh,onFace(-w+0.12f,0.12f,0.012f),onFace(w-0.12f,0.12f,0.012f),
                         onFace(w-0.12f,rakeLength*0.78f,0.012f),
                         onFace(-w+0.12f,rakeLength*0.78f,0.012f),normal,glass);
    detail::orientedQuad(mesh,onFace(-0.03f,0.12f,0.02f),onFace(0.03f,0.12f,0.02f),
                         onFace(0.03f,rakeLength*0.78f,0.02f),
                         onFace(-0.03f,rakeLength*0.78f,0.02f),normal,frame);
    const Color lit = lamps==CabLamps::Leading ? Color(244,246,228,255)
                    : lamps==CabLamps::Trailing ? Color(214,38,34,255) : Color(64,66,66,255);
    const Color marker = lamps==CabLamps::Trailing ? Color(214,38,34,255)
                       : lamps==CabLamps::Leading ? Color(250,239,181,255) : Color(64,66,66,255);
    for (float x : {-0.82f,0.82f})
    {
        box(mesh,x-0.16f,sillY+0.32f,noseZ,x+0.16f,sillY+0.52f,noseZ+0.04f,frame);
        box(mesh,x-0.12f,sillY+0.35f,noseZ+0.02f,x+0.02f,sillY+0.49f,noseZ+0.05f,lit);
        box(mesh,x+0.04f,sillY+0.37f,noseZ+0.02f,x+0.13f,sillY+0.47f,noseZ+0.05f,marker);
    }
    if (lamps==CabLamps::Leading)
        box(mesh,-0.10f,topY-0.16f,bodyEndZ+0.02f,0.10f,topY-0.04f,bodyEndZ+0.06f,lit);
}

// Side window row inside [z0, z1]; each window is a dark pane with a lighter gasket.
inline void addWindowRow(MeshBuilder& mesh,float z0,float z1,int count,float y0,float y1,
                         float gap,const Color& glass)
{
    const Color gasket(30,32,34,255);
    const float pitch = (z1-z0)/static_cast<float>(count);
    for (int i = 0; i < count; ++i)
    {
        const float a = z0+static_cast<float>(i)*pitch+gap*0.5f;
        const float b = a+pitch-gap;
        for (float side : {-1.0f,1.0f})
        {
            detail::sideBox(mesh,side,1.395f,1.412f,y0-0.03f,y1+0.03f,a-0.03f,b+0.03f,gasket);
            detail::sideBox(mesh,side,1.40f,1.418f,y0,y1,a,b,glass);
        }
    }
}

// Pair of plug doors centred at z on both sides, with windows and a sill step.
inline void addSideDoors(MeshBuilder& mesh,float z,float halfWidth,float sillY,float topY,
                         const Color& leaf)
{
    const Color glass(40,58,70,255), seam(30,32,34,255), step(60,63,64,255);
    for (float side : {-1.0f,1.0f})
    {
        detail::sideBox(mesh,side,1.40f,1.416f,sillY,topY,z-halfWidth,z+halfWidth,leaf);
        detail::sideBox(mesh,side,1.416f,1.420f,sillY,topY,z-0.015f,z+0.015f,seam);
        for (float half : {-1.0f,1.0f})
        {
            const float c = z+half*halfWidth*0.5f;
            detail::sideBox(mesh,side,1.416f,1.424f,sillY+(topY-sillY)*0.45f,topY-0.18f,
                            c-halfWidth*0.32f,c+halfWidth*0.32f,glass);
        }
        detail::sideBox(mesh,side,1.30f,1.46f,sillY-0.22f,sillY-0.10f,z-halfWidth,
                        z+halfWidth,step);
    }
}
} // namespace other_train_detail

// Original three-car regional unit, now one mesh per car (formerly a shared shell plus pantograph
// and cab overlays). Body 17.6 m, coupler-to-coupler 19 m.
inline void addRegionalEmuCar(MeshBuilder& mesh,World::OtherVehicleKind kind)
{
    using namespace Microsoft::Xna::Framework;
    using World::OtherVehicleKind;
    const Color bodyRed(188,52,48,255), bodyCream(226,225,211,255);
    const Color windowBlue(43,79,96,255), underframe(47,49,49,255);
    mesh.addBox(Vector3{-1.38f,0.28f,-8.8f},Vector3{1.38f,3.45f,8.8f},bodyRed);
    mesh.addBox(Vector3{-1.40f,3.18f,-8.55f},Vector3{1.40f,3.62f,8.55f},bodyCream);
    for (float side : {-1.0f,1.0f})
    {
        const float inner = side < 0.0f ? -1.405f : 1.385f;
        const float outer = side < 0.0f ? -1.385f : 1.405f;
        mesh.addBox(Vector3{inner,1.10f,-8.25f},Vector3{outer,3.12f,8.25f},bodyCream);
        mesh.addBox(Vector3{inner,0.88f,-8.30f},Vector3{outer,1.25f,8.30f},Color(139,38,38,255));
        for (float windowZ : {-5.65f,-4.05f,-2.45f,-0.85f,0.85f,2.45f,4.05f,5.65f})
            mesh.addBox(Vector3{inner-0.004f,1.66f,windowZ-0.57f},
                        Vector3{outer+0.004f,2.70f,windowZ+0.57f},windowBlue);
        for (float doorZ : {-7.22f,7.22f})
        {
            mesh.addBox(Vector3{inner-0.006f,0.52f,doorZ-0.56f},
                        Vector3{outer+0.006f,2.96f,doorZ+0.56f},Color(197,69,58,255));
            mesh.addBox(Vector3{inner-0.010f,1.70f,doorZ-0.34f},
                        Vector3{outer+0.010f,2.60f,doorZ+0.34f},windowBlue);
        }
    }
    mesh.addBox(Vector3{-1.08f,0.08f,-6.9f},Vector3{1.08f,0.34f,6.9f},underframe);
    for (float bogieZ : {-5.65f,5.65f})
    {
        mesh.addBox(Vector3{-1.12f,0.02f,bogieZ-0.82f},Vector3{1.12f,0.31f,bogieZ+0.82f},
                    Color(39,42,42,255));
        for (float axleZ : {-0.48f,0.48f})
            for (float side : {-1.0f,1.0f})
            {
                const float wheelX = side*1.18f;
                mesh.addBox(Vector3{wheelX-0.12f,0.00f,bogieZ+axleZ-0.27f},
                            Vector3{wheelX+0.12f,0.48f,bogieZ+axleZ+0.27f},Color(30,32,32,255));
            }
    }
    for (float end : {-1.0f,1.0f})
    {
        const float endZ = end*8.82f;
        const float couplerEndZ = endZ+end*0.42f;
        mesh.addBox(Vector3{-0.72f,1.36f,endZ-0.035f},Vector3{0.72f,2.76f,endZ+0.035f},
                    Color(34,43,46,255));
        mesh.addBox(Vector3{-0.22f,0.38f,std::min(endZ,couplerEndZ)},
                    Vector3{0.22f,0.58f,std::max(endZ,couplerEndZ)},underframe);
    }
    mesh.addBox(Vector3{-0.74f,3.57f,-1.85f},Vector3{0.74f,3.82f,1.85f},Color(105,110,108,255));

    if (kind==OtherVehicleKind::EmuMotorCar)
    {
        other_train_detail::addPantograph(mesh,3.84f,0.0f,true);
        return;
    }
    // Driving cab authored at +Z; the trailing car receives it turned to -Z.
    const bool leading = kind==OtherVehicleKind::EmuLeadingCar;
    MeshBuilder cab;
    constexpr float end = 8.78f;
    cab.addBox(Vector3{-1.38f,0.30f,end},Vector3{1.38f,3.18f,end+0.28f},bodyRed);
    cab.addBox(Vector3{-1.37f,2.88f,end+0.01f},Vector3{1.37f,3.52f,end+0.23f},bodyCream);
    cab.addBox(Vector3{-1.02f,1.88f,end+0.285f},Vector3{1.02f,2.91f,end+0.335f},windowBlue);
    cab.addBox(Vector3{-0.035f,1.87f,end+0.337f},Vector3{0.035f,2.92f,end+0.347f},
               Color(31,38,41,255));
    cab.addBox(Vector3{-0.76f,3.02f,end+0.245f},Vector3{0.76f,3.29f,end+0.305f},
               Color(27,37,40,255));
    // Amber pixels suggest a destination display without adding a dynamic text draw.
    for (int pixel = 0; pixel < 7; ++pixel)
    {
        const float x = -0.53f+static_cast<float>(pixel)*0.175f;
        cab.addBox(Vector3{x,3.105f,end+0.307f},Vector3{x+0.095f,3.185f,end+0.318f},
                   Color(238,171,47,255));
    }
    cab.addBox(Vector3{-1.10f,0.31f,end+0.27f},Vector3{1.10f,0.77f,end+0.48f},
               Color(119,34,35,255));
    cab.addBox(Vector3{-1.15f,0.17f,end+0.22f},Vector3{1.15f,0.36f,end+0.53f},
               Color(42,46,46,255));
    const Color markerLight = leading ? Color(250,239,181,255) : Color(211,40,38,255);
    for (float x : {-0.78f,0.78f})
    {
        cab.addBox(Vector3{x-0.13f,0.66f,end+0.485f},Vector3{x+0.13f,0.88f,end+0.535f},
                   markerLight);
        if (leading)
            cab.addBox(Vector3{x-0.055f,1.02f,end+0.34f},Vector3{x+0.055f,1.13f,end+0.38f},
                       Color(240,244,226,255));
    }
    // Side cab glazing makes the vehicle type readable during a close pass.
    for (float side : {-1.0f,1.0f})
    {
        const float inner = side < 0.0f ? -1.405f : 1.385f;
        const float outer = side < 0.0f ? -1.385f : 1.405f;
        cab.addBox(Vector3{inner,1.91f,7.91f},Vector3{outer,2.82f,8.57f},windowBlue);
    }
    // Two restrained wipers break up the otherwise flat windscreen.
    for (float x : {-0.43f,0.43f})
    {
        const Matrix wiper = Matrix::CreateRotationZ(x < 0.0f ? -0.53f : 0.53f)
            * Matrix::CreateTranslation(x,2.16f,end+0.353f);
        cab.addBox(Vector3{-0.34f,-0.018f,-0.012f},Vector3{0.34f,0.018f,0.012f},
                   Color(21,25,26,255),wiper);
    }
    MeshData cabData = cab.take();
    if (!leading) other_train_detail::turnHalfRound(cabData);
    mesh.append(cabData);
}

// Double-ended Bo-Bo electric locomotive (19.4 m over buffers). The intercity livery is dark
// blue with a silver band; the freight engine is green with yellow warning ends.
inline void addElectricLocomotive(MeshBuilder& mesh,bool freightLivery)
{
    using namespace Microsoft::Xna::Framework;
    using namespace other_train_detail;
    const Color body = freightLivery ? Color(42,92,66,255) : Color(30,48,92,255);
    const Color band = freightLivery ? Color(214,178,52,255) : Color(196,202,206,255);
    const Color nose = freightLivery ? Color(232,190,44,255) : Color(30,48,92,255);
    const Color roof(92,96,96,255), frame(38,40,41,255), grille(48,54,56,255);
    const Color beam = freightLivery ? Color(150,40,34,255) : Color(40,42,44,255);
    constexpr float halfPitch = 9.7f, bodyEnd = 8.25f, nosePoint = 9.0f;
    constexpr float sill = 1.02f, waist = 2.12f, top = 3.52f;
    box(mesh,-1.36f,0.86f,-9.12f,1.36f,sill,9.12f,frame);
    box(mesh,-1.40f,sill,-bodyEnd,1.40f,top,bodyEnd,body);
    for (float side : {-1.0f,1.0f})
    {
        detail::sideBox(mesh,side,1.40f,1.414f,1.56f,1.78f,-bodyEnd,bodyEnd,band);
        // Machine-room louvres and the radiator grille.
        for (int louvre = 0; louvre < 8; ++louvre)
        {
            const float z = -5.6f+static_cast<float>(louvre)*1.6f;
            detail::sideBox(mesh,side,1.40f,1.418f,2.32f,3.18f,z-0.62f,z+0.62f,grille);
            for (int slat = 0; slat < 5; ++slat)
            {
                const float y = 2.40f+static_cast<float>(slat)*0.16f;
                detail::sideBox(mesh,side,1.418f,1.424f,y,y+0.05f,z-0.60f,z+0.60f,
                                Color(70,76,78,255));
            }
        }
        for (float end : {-1.0f,1.0f})
        {
            // Cab side window, cab door and grab rails at each end.
            const float window = end*7.55f;
            detail::sideBox(mesh,side,1.40f,1.418f,2.30f,3.14f,window-0.42f,window+0.42f,
                            Color(34,52,62,255));
            const float door = end*6.55f;
            detail::sideBox(mesh,side,1.40f,1.416f,1.10f,3.20f,door-0.40f,door+0.40f,
                            Color(26,30,32,255));
            detail::sideBox(mesh,side,1.40f,1.420f,1.14f,3.16f,door-0.37f,door+0.37f,body);
            detail::sideBox(mesh,side,1.40f,1.424f,2.42f,2.90f,door-0.22f,door+0.22f,
                            Color(34,52,62,255));
            for (float rail : {door-0.48f,door+0.48f})
                detail::sideBox(mesh,side,1.43f,1.46f,1.20f,2.85f,rail-0.02f,rail+0.02f,
                                Color(186,188,184,255));
        }
    }
    addArcRoof(mesh,1.40f,top,0.20f,-bodyEnd,bodyEnd,8,roof);
    for (float end : {-1.0f,1.0f})
    {
        MeshBuilder cab;
        addRakedCab(cab,bodyEnd,nosePoint,sill,waist,top,1.40f,body,nose,
                    end > 0.0f ? CabLamps::Leading : CabLamps::Dark);
        MeshData cabData = cab.take();
        if (end < 0.0f) turnHalfRound(cabData);
        mesh.append(cabData);
        addBuffers(mesh,end,9.12f,halfPitch,1.06f,beam);
    }
    // Roof equipment: main breaker, insulators, a raised trailing and a lowered leading pan.
    box(mesh,-0.45f,top+0.18f,-1.2f,0.45f,top+0.42f,1.2f,Color(110,114,112,255));
    addPantograph(mesh,top+0.36f,-5.2f,true);
    addPantograph(mesh,top+0.36f,5.2f,false);
    for (float z : {-5.2f,5.2f})
        box(mesh,-0.80f,top+0.12f,z-0.85f,0.80f,top+0.18f,z+0.85f,Color(70,74,74,255));
    for (float z : {-5.4f,5.4f}) addBogie(mesh,z,2.6f,0.56f,frame);
    // Fuel-free underframe: transformer tank between the bogies.
    box(mesh,-1.05f,0.48f,-2.6f,1.05f,0.86f,2.6f,Color(44,47,48,255));
}

// Intercity coach (26.4 m over buffers): light grey with a dark blue window band and red
// waist stripe, end doors, gangway bellows and an elliptic roof. The tail coach carries red
// lamps and an end window on its trailing (-Z) end.
inline void addIntercityCoach(MeshBuilder& mesh,bool tail)
{
    using namespace Microsoft::Xna::Framework;
    using namespace other_train_detail;
    const Color body(206,210,212,255), band(30,48,86,255), stripe(180,38,40,255);
    const Color glass(56,82,104,255), frame(40,42,43,255), roof(150,154,156,255);
    constexpr float halfPitch = 13.2f, bodyEnd = 12.75f, sill = 0.90f, top = 3.50f;
    box(mesh,-1.40f,sill,-bodyEnd,1.40f,top,bodyEnd,body);
    box(mesh,-1.30f,0.72f,-12.85f,1.30f,sill,12.85f,frame);
    addArcRoof(mesh,1.40f,top,0.34f,-bodyEnd,bodyEnd,10,roof);
    for (float side : {-1.0f,1.0f})
    {
        detail::sideBox(mesh,side,1.40f,1.410f,1.62f,2.80f,-10.6f,10.6f,band);
        detail::sideBox(mesh,side,1.40f,1.410f,1.36f,1.48f,-bodyEnd,bodyEnd,stripe);
    }
    addWindowRow(mesh,-10.4f,10.4f,10,1.72f,2.70f,0.36f,glass);
    for (float door : {-11.65f,11.65f})
        addSideDoors(mesh,door,0.55f,1.05f,3.02f,Color(178,184,188,255));
    for (float end : {-1.0f,1.0f})
    {
        box(mesh,-0.62f,1.00f,end*bodyEnd,0.62f,3.20f,end*(bodyEnd+0.24f),
            Color(32,34,36,255));
        addBuffers(mesh,end,12.85f,halfPitch,1.06f,frame);
    }
    for (float z : {-9.4f,9.4f}) addBogie(mesh,z,2.5f,0.46f,frame);
    for (float z : {-4.8f,0.0f,4.6f})
        box(mesh,-0.95f,0.42f,z-1.0f,0.95f,0.80f,z+1.0f,Color(52,56,57,255));
    box(mesh,-0.70f,top+0.30f,-2.0f,0.70f,top+0.46f,2.0f,Color(128,132,134,255));
    if (!tail) return;
    for (float x : {-0.86f,0.86f})
    {
        box(mesh,x-0.13f,1.18f,-bodyEnd-0.06f,x+0.13f,1.38f,-bodyEnd,Color(24,26,28,255));
        box(mesh,x-0.09f,1.21f,-bodyEnd-0.08f,x+0.09f,1.35f,-bodyEnd-0.04f,
            Color(214,38,34,255));
    }
    box(mesh,-0.22f,2.00f,-bodyEnd-0.25f,0.22f,2.75f,-bodyEnd-0.23f,glass);
}

namespace other_train_detail {
// Flat, low-sided wagon underframe: solebars, headstocks, buffers and two Y25 bogies.
inline void addWagonUnderframe(MeshBuilder& mesh,float halfPitch,float bogieOffset,
                               float deckY,const Color& frame)
{
    const float endZ = halfPitch-0.48f;
    for (float side : {-1.0f,1.0f})
        box(mesh,side*1.18f,deckY-0.30f,-endZ,side*1.34f,deckY,endZ,frame);
    box(mesh,-0.30f,deckY-0.36f,-endZ,0.30f,deckY,endZ,frame);
    for (float end : {-1.0f,1.0f})
        addBuffers(mesh,end,endZ,halfPitch,1.06f,frame);
    for (float z : {-bogieOffset,bogieOffset})
        addBogie(mesh,z,1.8f,0.42f,Color(40,40,38,255));
}

// Corrugated ISO container sitting on the deck, centred at z.
inline void addContainer(MeshBuilder& mesh,float deckY,float z,float length,const Color& colour)
{
    const float half = length*0.5f, top = deckY+2.59f;
    const auto shaded = [](auto channel) { return static_cast<int>(channel*0.82f); };
    const Color rib(shaded(colour.getRProperty()),shaded(colour.getGProperty()),
                    shaded(colour.getBProperty()),255);
    box(mesh,-1.20f,deckY,z-half,1.20f,top,z+half,colour);
    const int ribs = static_cast<int>(length/0.55f);
    for (int i = 1; i < ribs; ++i)
    {
        const float rz = z-half+length*static_cast<float>(i)/static_cast<float>(ribs);
        for (float side : {-1.0f,1.0f})
            detail::sideBox(mesh,side,1.20f,1.232f,deckY+0.14f,top-0.12f,rz-0.07f,rz+0.07f,rib);
    }
    for (float side : {-1.0f,1.0f})
        for (float end : {-1.0f,1.0f})
            box(mesh,side*1.10f,deckY,z+end*half-0.10f,side*1.235f,top+0.01f,z+end*half+0.10f,
                rib);
    for (float side : {-1.0f,1.0f})
        detail::sideBox(mesh,side,1.20f,1.236f,top-0.12f,top,z-half,z+half,rib);
    // Locking bars on both end doors.
    for (float end : {-1.0f,1.0f})
        for (float x : {-0.75f,-0.35f,0.35f,0.75f})
            box(mesh,x-0.025f,deckY+0.18f,z+end*half,x+0.025f,top-0.18f,z+end*(half+0.035f),
                Color(150,150,146,255));
}
} // namespace other_train_detail

// Freight wagons (ENV-137): container flats, a tank wagon, a covered hopper and an open wagon
// with an aggregate load.
inline void addFreightWagon(MeshBuilder& mesh,World::OtherVehicleKind kind)
{
    using namespace Microsoft::Xna::Framework;
    using namespace other_train_detail;
    using World::OtherVehicleKind;
    const Color frame(58,54,50,255);
    switch (kind)
    {
        case OtherVehicleKind::ContainerFlatPair:
        case OtherVehicleKind::ContainerFlatLong:
        {
            constexpr float deckY = 1.20f;
            addWagonUnderframe(mesh,9.8f,7.0f,deckY,frame);
            box(mesh,-1.30f,deckY-0.05f,-9.2f,1.30f,deckY,9.2f,Color(70,64,58,255));
            if (kind==OtherVehicleKind::ContainerFlatPair)
            {
                addContainer(mesh,deckY,-3.2f,6.06f,Color(132,40,36,255));
                addContainer(mesh,deckY,3.2f,6.06f,Color(38,82,132,255));
            }
            else addContainer(mesh,deckY,0.0f,12.19f,Color(40,112,92,255));
            return;
        }
        case OtherVehicleKind::TankWagon:
        {
            constexpr float deckY = 1.18f, centreY = 2.32f, radius = 1.25f;
            const Color tank(58,62,64,255), detailGrey(110,112,110,255);
            addWagonUnderframe(mesh,7.75f,5.4f,deckY,frame);
            addPrism(mesh,Vector3{0,centreY,-5.9f},Vector3{0,centreY,5.9f},Vector3{1,0,0},
                     Vector3{0,1,0},radius,18,tank);
            for (float end : {-1.0f,1.0f})
            {
                addPrism(mesh,Vector3{0,centreY,end*5.9f},Vector3{0,centreY,end*6.18f},
                         Vector3{1,0,0},Vector3{0,1,0},radius*0.86f,18,tank);
                addPrism(mesh,Vector3{0,centreY,end*6.18f},Vector3{0,centreY,end*6.32f},
                         Vector3{1,0,0},Vector3{0,1,0},radius*0.55f,14,tank);
                box(mesh,-1.0f,deckY,end*4.4f-0.25f,1.0f,centreY-radius*0.6f,end*4.4f+0.25f,
                    frame);
            }
            // Dome, walkway and side ladders.
            addPrism(mesh,Vector3{0,centreY+radius-0.05f,0},Vector3{0,centreY+radius+0.26f,0},
                     Vector3{1,0,0},Vector3{0,0,1},0.38f,12,tank);
            box(mesh,-0.80f,centreY+radius-0.04f,-1.2f,0.80f,centreY+radius+0.02f,1.2f,
                detailGrey);
            for (float side : {-1.0f,1.0f})
            {
                for (float z : {-0.30f,0.30f})
                    box(mesh,side*1.30f,deckY,z-0.02f,side*1.34f,centreY+0.4f,z+0.02f,detailGrey);
                for (int rung = 0; rung < 6; ++rung)
                {
                    const float y = deckY+0.20f+static_cast<float>(rung)*0.30f;
                    box(mesh,side*1.30f,y,-0.30f,side*1.34f,y+0.03f,0.30f,detailGrey);
                }
                // Hazard panel and a pale cleaning band near each end.
                box(mesh,side*1.10f,1.80f,2.4f,side*1.315f,2.22f,3.0f,Color(232,124,32,255));
                for (float end : {-1.0f,1.0f})
                    detail::sideBox(mesh,side,1.28f,1.312f,2.0f,2.9f,end*4.9f-0.10f,
                                    end*4.9f+0.10f,Color(170,172,168,255));
            }
            return;
        }
        case OtherVehicleKind::CoveredHopper:
        {
            constexpr float lowerY = 1.70f, top = 3.62f;
            const Color shell(196,190,172,255), shade(164,158,142,255);
            addWagonUnderframe(mesh,7.0f,4.9f,1.20f,frame);
            box(mesh,-1.45f,lowerY,-6.1f,1.45f,top,6.1f,shell);
            addArcRoof(mesh,1.45f,top,0.24f,-6.1f,6.1f,8,shell);
            for (int hopper = 0; hopper < 3; ++hopper)
            {
                const float z = -4.0f+static_cast<float>(hopper)*4.0f;
                const Vector3 a{-1.40f,lowerY,z-1.9f}, b{1.40f,lowerY,z-1.9f};
                const Vector3 c{1.40f,lowerY,z+1.9f}, d{-1.40f,lowerY,z+1.9f};
                const Vector3 e{-0.45f,0.82f,z-0.55f}, f{0.45f,0.82f,z-0.55f};
                const Vector3 g{0.45f,0.82f,z+0.55f}, h{-0.45f,0.82f,z+0.55f};
                detail::orientedQuad(mesh,a,b,f,e,Vector3{0,-0.3f,-1},shade);
                detail::orientedQuad(mesh,b,c,g,f,Vector3{1,-0.3f,0},shade);
                detail::orientedQuad(mesh,c,d,h,g,Vector3{0,-0.3f,1},shade);
                detail::orientedQuad(mesh,d,a,e,h,Vector3{-1,-0.3f,0},shade);
                box(mesh,-0.45f,0.68f,z-0.55f,0.45f,0.82f,z+0.55f,Color(70,70,66,255));
                // Roof loading hatch.
                box(mesh,-0.42f,top+0.20f,z-0.42f,0.42f,top+0.30f,z+0.42f,shade);
            }
            for (float side : {-1.0f,1.0f})
                for (int post = 0; post <= 6; ++post)
                {
                    const float z = -6.0f+static_cast<float>(post)*2.0f;
                    detail::sideBox(mesh,side,1.45f,1.49f,lowerY,top,z-0.06f,z+0.06f,shade);
                }
            for (float end : {-1.0f,1.0f})
                box(mesh,-1.0f,1.20f,end*6.1f,1.0f,1.26f,end*6.5f,Color(80,80,76,255));
            return;
        }
        default:
        {
            constexpr float floorY = 1.15f, rim = 2.95f;
            const Color side(122,64,44,255), stanchion(96,50,36,255);
            addWagonUnderframe(mesh,6.9f,4.6f,floorY,frame);
            box(mesh,-1.40f,floorY-0.06f,-6.3f,1.40f,floorY,6.3f,side);
            for (float s : {-1.0f,1.0f})
            {
                detail::sideBox(mesh,s,1.34f,1.40f,floorY,rim,-6.3f,6.3f,side);
                detail::sideBox(mesh,s,1.40f,1.46f,rim-0.10f,rim,-6.3f,6.3f,stanchion);
                for (int post = 0; post <= 8; ++post)
                {
                    const float z = -6.2f+static_cast<float>(post)*1.55f;
                    detail::sideBox(mesh,s,1.40f,1.45f,floorY,rim,z-0.05f,z+0.05f,stanchion);
                }
                box(mesh,-1.40f,floorY,s*6.24f,1.40f,rim,s*6.30f,side);
            }
            // Aggregate load heaped above the sides between the end walls.
            const Color load(124,120,112,255);
            box(mesh,-1.33f,floorY,-6.22f,1.33f,rim-0.18f,6.22f,load);
            for (float z : {-3.7f,0.0f,3.7f})
                mesh.addEllipsoid(Vector3{0.0f,rim-0.18f,z},Vector3{1.20f,0.42f,1.95f},10,4,
                                  load);
            return;
        }
    }
}

// Two-car diesel unit (23 m cars): white with a green skirt band, yellow doors, roof exhausts
// and an underfloor engine raft. The trailing car is the leading car turned half round, with
// red tail lamps instead of headlights.
inline void addDieselUnitCar(MeshBuilder& mesh,bool leading)
{
    using namespace Microsoft::Xna::Framework;
    using namespace other_train_detail;
    MeshBuilder car;
    const Color body(232,234,228,255), band(34,120,80,255), roof(128,132,130,255);
    const Color glass(46,64,74,255), frame(40,42,43,255);
    constexpr float sill = 0.70f, top = 3.46f, bodyEnd = 10.7f, nosePoint = 11.45f;
    box(car,-1.40f,sill,-11.2f,1.40f,top,bodyEnd,body);
    addArcRoof(car,1.40f,top,0.32f,-11.2f,bodyEnd,10,roof);
    for (float side : {-1.0f,1.0f})
    {
        detail::sideBox(car,side,1.40f,1.410f,sill,1.30f,-11.2f,bodyEnd,band);
        detail::sideBox(car,side,1.40f,1.410f,1.40f,1.47f,-11.2f,bodyEnd,band);
        detail::sideBox(car,side,1.40f,1.414f,2.30f,3.00f,9.30f,10.30f,glass);
    }
    addWindowRow(car,-10.4f,-6.9f,2,1.62f,2.62f,0.30f,glass);
    addWindowRow(car,-4.7f,4.9f,6,1.62f,2.62f,0.30f,glass);
    addWindowRow(car,7.1f,8.6f,1,1.62f,2.62f,0.30f,glass);
    for (float door : {-5.8f,6.0f})
        addSideDoors(car,door,0.72f,sill+0.12f,2.98f,Color(232,182,38,255));
    MeshBuilder cab;
    addRakedCab(cab,bodyEnd,nosePoint,sill,1.95f,top,1.40f,body,band,
                leading ? CabLamps::Leading : CabLamps::Trailing);
    car.append(cab.take());
    box(car,-0.24f,0.42f,nosePoint,0.24f,0.66f,nosePoint+0.12f,frame);
    box(car,-0.62f,1.00f,-11.2f-0.22f,0.62f,3.10f,-11.2f,Color(32,34,36,255));
    box(car,-0.22f,0.40f,-11.5f,0.22f,0.62f,-11.2f,frame);
    for (float z : {-8.2f,8.0f}) addBogie(car,z,2.4f,0.42f,frame);
    // Underfloor engine raft with a radiator grille, plus roof exhausts above it.
    box(car,-1.05f,0.38f,-3.4f,1.05f,0.70f,3.4f,Color(44,46,46,255));
    for (float side : {-1.0f,1.0f})
        for (int slat = 0; slat < 6; ++slat)
        {
            const float z = -1.2f+static_cast<float>(slat)*0.45f;
            detail::sideBox(car,side,1.05f,1.08f,0.42f,0.66f,z-0.12f,z+0.12f,Color(70,74,74,255));
        }
    for (float x : {-0.30f,0.30f})
        addPrism(car,Vector3{x,top+0.20f,-1.6f},Vector3{x,top+0.62f,-1.6f},Vector3{1,0,0},
                 Vector3{0,0,1},0.08f,8,Color(36,36,36,255));
    box(car,-0.80f,top+0.22f,2.0f,0.80f,top+0.48f,4.6f,Color(110,114,114,255));
    MeshData data = car.take();
    if (!leading) turnHalfRound(data);
    mesh.append(data);
}

inline void addOtherTrainVehicle(MeshBuilder& mesh,World::OtherVehicleKind kind)
{
    using World::OtherVehicleKind;
    switch (kind)
    {
        case OtherVehicleKind::EmuLeadingCar:
        case OtherVehicleKind::EmuMotorCar:
        case OtherVehicleKind::EmuTrailingCar: addRegionalEmuCar(mesh,kind); return;
        case OtherVehicleKind::IntercityLocomotive: addElectricLocomotive(mesh,false); return;
        case OtherVehicleKind::FreightLocomotive: addElectricLocomotive(mesh,true); return;
        case OtherVehicleKind::IntercityCoach: addIntercityCoach(mesh,false); return;
        case OtherVehicleKind::IntercityTailCoach: addIntercityCoach(mesh,true); return;
        case OtherVehicleKind::DieselLeadingCar: addDieselUnitCar(mesh,true); return;
        case OtherVehicleKind::DieselTrailingCar: addDieselUnitCar(mesh,false); return;
        default: addFreightWagon(mesh,kind); return;
    }
}

[[nodiscard]] constexpr bool otherVehicleIsFreightWagon(World::OtherVehicleKind kind) noexcept
{
    using World::OtherVehicleKind;
    return kind==OtherVehicleKind::ContainerFlatPair || kind==OtherVehicleKind::ContainerFlatLong
        || kind==OtherVehicleKind::TankWagon || kind==OtherVehicleKind::CoveredHopper
        || kind==OtherVehicleKind::OpenWagon;
}
[[nodiscard]] constexpr bool otherVehicleIsLocomotive(World::OtherVehicleKind kind) noexcept
{
    return kind==World::OtherVehicleKind::IntercityLocomotive
        || kind==World::OtherVehicleKind::FreightLocomotive;
}

} // namespace TrainSim::Render
