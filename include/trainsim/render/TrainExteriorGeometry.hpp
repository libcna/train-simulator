#pragma once

#include "trainsim/render/LinesideGeometry.hpp"
#include "trainsim/render/MeshBuilder.hpp"
#include <algorithm>
#include <cmath>

namespace TrainSim::Render {

// Exterior shell of the six-car passenger train in train-local metres (X across, Y above rail,
// Z along the consist). Everything joins the immutable interior batch, so the exterior gains no
// draw call; pieces sit outside the x = +-1.50 structural walls so they never show inside.
inline constexpr float kBodyHalfWidth=1.50f;
inline constexpr float kBogieOffset=6.8f;
inline constexpr float kBogieHalfLength=1.40f;
inline constexpr float kSkirtTop=0.40f;
inline constexpr float kRoofTop=3.08f;
inline constexpr float kRoofCrownTop=3.20f;
inline constexpr float kRoofCrownHalfWidth=1.10f;

namespace detail {
inline void orientedQuad(MeshBuilder& mesh,const Microsoft::Xna::Framework::Vector3& a,
                         const Microsoft::Xna::Framework::Vector3& b,
                         const Microsoft::Xna::Framework::Vector3& c,
                         const Microsoft::Xna::Framework::Vector3& d,
                         const Microsoft::Xna::Framework::Vector3& outward,
                         const Microsoft::Xna::Framework::Color& colour)
{
    using namespace Microsoft::Xna::Framework;
    if (Vector3::Dot(Vector3::Cross(b-a,c-a),outward)>=0.0f) mesh.addQuad(a,b,c,d,colour);
    else mesh.addQuad(d,c,b,a,colour);
}
inline void orientedTriangle(MeshBuilder& mesh,const Microsoft::Xna::Framework::Vector3& a,
                             const Microsoft::Xna::Framework::Vector3& b,
                             const Microsoft::Xna::Framework::Vector3& c,
                             const Microsoft::Xna::Framework::Vector3& outward,
                             const Microsoft::Xna::Framework::Color& colour)
{
    using namespace Microsoft::Xna::Framework;
    if (Vector3::Dot(Vector3::Cross(b-a,c-a),outward)>=0.0f) mesh.addTriangle(a,b,c,colour);
    else mesh.addTriangle(a,c,b,colour);
}
inline void sideBox(MeshBuilder& mesh,float side,float inner,float outer,float y0,float y1,
                    float z0,float z1,const Microsoft::Xna::Framework::Color& colour)
{
    using namespace Microsoft::Xna::Framework;
    mesh.addBox(Vector3{std::min(side*inner,side*outer),y0,z0},
                Vector3{std::max(side*inner,side*outer),y1,z1},colour);
}
} // namespace detail

// Lower bodyside: the band between skirt and sill, a recessed skirt with bogie cut-outs, a
// bogie frame with round wheels and axleboxes in each cut-out, and underframe equipment boxes
// between the bogies. Replaces the single 0.05..0.86 m wall box of the earlier shell.
inline void addLowerBodyside(MeshBuilder& mesh,float side,float start,float end,float centre,
                             float doorBegin,float doorEnd,
                             const Microsoft::Xna::Framework::Color& shell,
                             bool firstBogie=true,bool secondBogie=true)
{
    using namespace Microsoft::Xna::Framework;
    const Color skirt(74,82,86,255),frame(40,44,46,255),wheel(52,56,58,255);
    const Color wheelFace(84,88,90,255),equipment(58,63,66,255),lid(88,94,97,255);
    constexpr float inner=1.42f,outer=1.50f;
    // Upper part of the lower band keeps the shell colour on both sides of the doorway.
    if (doorBegin-start>0.01f) detail::sideBox(mesh,side,inner,outer,kSkirtTop,0.86f,start,doorBegin,shell);
    if (end-doorEnd>0.01f) detail::sideBox(mesh,side,inner,outer,kSkirtTop,0.86f,doorEnd,end,shell);
    // Recessed skirt segments outside the bogie cut-outs; the doorway keeps its own sill.
    const float bogieA=centre-kBogieOffset,bogieB=centre+kBogieOffset;
    const auto skirtSpan=[&](float z0,float z1) {
        z0=std::max(z0,start);
        z1=std::min(z1,end);
        if (z1-z0<0.05f) return;
        for (const auto& [s0,s1] : {std::pair{z0,std::min(z1,doorBegin)},
                                   std::pair{std::max(z0,doorEnd),z1}})
            if (s1-s0>0.05f) detail::sideBox(mesh,side,inner,inner+0.04f,0.05f,kSkirtTop,s0,s1,skirt);
    };
    float skirtCursor=start;
    for (const auto& [bogie,present] : {std::pair{bogieA,firstBogie},std::pair{bogieB,secondBogie}})
        if (present)
        {
            skirtSpan(skirtCursor,bogie-kBogieHalfLength);
            skirtCursor=bogie+kBogieHalfLength;
        }
    skirtSpan(skirtCursor,end);
    // Dark backing behind every cut-out so the interior never shows through.
    for (float bogie : {bogieA,bogieB})
    {
        if ((bogie==bogieA && !firstBogie) || (bogie==bogieB && !secondBogie)) continue;
        detail::sideBox(mesh,side,1.19f,1.23f,0.05f,kSkirtTop,bogie-kBogieHalfLength-0.02f,
                        bogie+kBogieHalfLength+0.02f,Color(26,29,30,255));
        // Bogie frame, bolster and two wheelsets with axleboxes.
        detail::sideBox(mesh,side,1.00f,1.26f,0.22f,0.38f,bogie-1.30f,bogie+1.30f,frame);
        detail::sideBox(mesh,side,1.18f,1.27f,0.26f,0.34f,bogie-0.35f,bogie+0.35f,lid);
        for (float axle : {-0.80f,0.80f})
        {
            const float x0=std::min(side*1.03f,side*1.15f),x1=std::max(side*1.03f,side*1.15f);
            mesh.addRoundedBoxYZ(Vector3{x0,0.0f,bogie+axle-0.19f},
                                 Vector3{x1,0.38f,bogie+axle+0.19f},0.19f,4,wheel);
            const float f0=std::min(side*1.15f,side*1.165f),f1=std::max(side*1.15f,side*1.165f);
            mesh.addRoundedBoxYZ(Vector3{f0,0.09f,bogie+axle-0.10f},
                                 Vector3{f1,0.29f,bogie+axle+0.10f},0.10f,4,wheelFace);
            detail::sideBox(mesh,side,1.14f,1.30f,0.16f,0.30f,bogie+axle-0.14f,bogie+axle+0.14f,
                            frame);
        }
    }
    if (!firstBogie || !secondBogie) return;
    // Underframe equipment between the bogies: battery box, converter and brake module.
    for (const auto& [z0,z1,top] : {std::array{bogieA+kBogieHalfLength+0.35f,
                                               bogieA+kBogieHalfLength+1.85f,0.36f},
                                   std::array{centre-0.55f,centre+0.55f,0.33f},
                                   std::array{bogieB-kBogieHalfLength-1.95f,
                                               bogieB-kBogieHalfLength-0.35f,0.37f}})
    {
        if (z1<=doorBegin-0.1f || z0>=doorEnd+0.1f)
        {
            detail::sideBox(mesh,side,1.30f,1.48f,0.06f,top,z0,z1,equipment);
            detail::sideBox(mesh,side,1.481f,1.49f,0.10f,top-0.06f,z0+0.08f,z1-0.08f,lid);
        }
    }
}

// Livery: a deep service-colour band below the sill with a yellow line, continuous across the
// mullions, giving the plain grey side a coherent identity.
inline void addLiveryBands(MeshBuilder& mesh,float side,float start,float end,float doorBegin,
                           float doorEnd,const Microsoft::Xna::Framework::Color& serviceColour)
{
    using namespace Microsoft::Xna::Framework;
    const Color line(226,186,42,255);
    for (const auto& [z0,z1] : {std::pair{start+0.02f,doorBegin-0.07f},
                               std::pair{doorEnd+0.07f,end-0.02f}})
    {
        if (z1-z0<0.1f) continue;
        detail::sideBox(mesh,side,1.501f,1.508f,0.44f,0.80f,z0,z1,serviceColour);
        detail::sideBox(mesh,side,1.501f,1.506f,0.80f,0.845f,z0,z1,line);
    }
}

// Exterior glazing gasket: a thin dark rubber frame proud of the body around each window.
inline void addExteriorWindowGasket(MeshBuilder& mesh,float side,float z0,float z1)
{
    using namespace Microsoft::Xna::Framework;
    const Color rubber(28,33,35,255);
    constexpr float y0=0.84f,y1=2.36f,w=0.03f;
    detail::sideBox(mesh,side,1.501f,1.506f,y0,y0+w,z0,z1,rubber);
    detail::sideBox(mesh,side,1.501f,1.506f,y1-w,y1,z0,z1,rubber);
    detail::sideBox(mesh,side,1.501f,1.506f,y0,y1,z0,z0+w,rubber);
    detail::sideBox(mesh,side,1.501f,1.506f,y0,y1,z1-w,z1,rubber);
}

// Exterior mullion face: a dark window-band skin that integrates the post with the glazing
// instead of a contrasting teal post against the pale shell.
inline void addExteriorMullionFace(MeshBuilder& mesh,float side,float z,float halfWidth)
{
    detail::sideBox(mesh,side,1.501f,1.505f,0.86f,2.34f,z-halfWidth,z+halfWidth,
                    Microsoft::Xna::Framework::Color(44,52,56,255));
}

// Exterior door reveal: dark perimeter seal and a recessed header so the closed leaves read as
// plug doors inside the body rather than flat panels on it.
inline void addExteriorDoorReveal(MeshBuilder& mesh,float side,float doorBegin,float doorEnd)
{
    using namespace Microsoft::Xna::Framework;
    const Color seal(22,27,29,255),reveal(96,104,108,255);
    detail::sideBox(mesh,side,1.501f,1.512f,0.04f,2.46f,doorBegin-0.105f,doorBegin-0.065f,seal);
    detail::sideBox(mesh,side,1.501f,1.512f,0.04f,2.46f,doorEnd+0.065f,doorEnd+0.105f,seal);
    detail::sideBox(mesh,side,1.501f,1.512f,2.40f,2.46f,doorBegin-0.105f,doorEnd+0.105f,seal);
    detail::sideBox(mesh,side,1.501f,1.509f,2.46f,2.52f,doorBegin-0.12f,doorEnd+0.12f,reveal);
}

// Roof: a raised crown with chamfered edges instead of one flat bright slab, panel divisions,
// an HVAC housing with side louvres and extractor discs, and a pantograph on the powered cars.
inline void addRoofEquipment(MeshBuilder& mesh,float start,float end,float centre,bool pantograph)
{
    using namespace Microsoft::Xna::Framework;
    const Color crown(196,202,202,255),chamfer(182,189,189,255),joint(150,157,158,255);
    const Color housing(172,178,178,255),louvre(74,82,84,255),steel(67,72,71,255);
    mesh.addBox(Vector3{-kRoofCrownHalfWidth,kRoofTop,start},
                Vector3{kRoofCrownHalfWidth,kRoofCrownTop,end},crown);
    for (float side : {-1.0f,1.0f})
    {
        const Vector3 a{side*1.48f,kRoofTop,start},b{side*1.48f,kRoofTop,end};
        const Vector3 c{side*kRoofCrownHalfWidth,kRoofCrownTop,end};
        const Vector3 d{side*kRoofCrownHalfWidth,kRoofCrownTop,start};
        detail::orientedQuad(mesh,a,b,c,d,Vector3{side,1.0f,0.0f},chamfer);
    }
    for (float z=start+3.5f;z<end-1.0f;z+=3.5f)
        mesh.addBox(Vector3{-kRoofCrownHalfWidth,kRoofCrownTop,z-0.008f},
                    Vector3{kRoofCrownHalfWidth,kRoofCrownTop+0.006f,z+0.008f},joint);
    mesh.addRoundedBoxXZ(Vector3{-0.80f,kRoofCrownTop,centre-1.7f},
                         Vector3{0.80f,kRoofCrownTop+0.26f,centre+1.7f},0.10f,2,housing);
    for (float side : {-1.0f,1.0f})
        for (float z=centre-1.3f;z<=centre+1.31f;z+=0.52f)
            mesh.addBox(Vector3{std::min(side*0.80f,side*0.806f),kRoofCrownTop+0.07f,z-0.16f},
                        Vector3{std::max(side*0.80f,side*0.806f),kRoofCrownTop+0.19f,z+0.16f},louvre);
    for (float z : {centre-0.85f,centre+0.85f})
        mesh.addDiscXZ(Vector3{0.0f,kRoofCrownTop+0.262f,z},0.28f,16,louvre);
    if (!pantograph) return;
    const float base=kRoofCrownTop+0.26f+0.0f;
    for (float x : {-0.62f,0.62f})
        for (float z : {centre-0.65f,centre+0.65f})
            mesh.addCylinder(Vector3{x,base,z},0.045f,0.16f,8,Color(186,176,150,255));
    mesh.addBox(Vector3{-0.70f,base+0.16f,centre-0.72f},Vector3{0.70f,base+0.24f,centre+0.72f},steel);
    const float frameTop=base+0.24f;
    for (float x : {-0.55f,0.55f})
    {
        addBarBetween(mesh,Vector3{x,frameTop,centre-0.62f},Vector3{x*0.25f,4.75f,centre+0.12f},
                      0.07f,steel);
        addBarBetween(mesh,Vector3{x*0.25f,4.75f,centre+0.12f},Vector3{x*0.12f,5.70f,centre-0.32f},
                      0.055f,steel);
    }
    addBarBetween(mesh,Vector3{-0.15f,4.75f,centre+0.12f},Vector3{0.15f,4.75f,centre+0.12f},0.07f,steel);
    mesh.addBox(Vector3{-0.95f,5.70f,centre-0.42f},Vector3{0.95f,5.76f,centre-0.22f},steel);
    for (float z : {centre-0.40f,centre-0.24f})
        mesh.addBox(Vector3{-0.90f,5.76f,z-0.03f},Vector3{0.90f,5.83f,z+0.03f},Color(44,48,47,255));
}

// Exterior inter-car connection: ribbed rubber bellows filling the gap between car ends.
inline void addExteriorBellows(MeshBuilder& mesh,float z)
{
    using namespace Microsoft::Xna::Framework;
    const Color rubber(34,38,40,255),fold(52,57,59,255);
    mesh.addRoundedBoxXZ(Vector3{-1.30f,0.30f,z-0.22f},Vector3{1.30f,2.98f,z+0.22f},0.12f,2,rubber);
    for (int fold_=-2;fold_<=2;++fold_)
    {
        const float fz=z+static_cast<float>(fold_)*0.085f;
        mesh.addRoundedBoxXZ(Vector3{-1.36f,0.36f,fz-0.022f},Vector3{1.36f,3.02f,fz+0.022f},
                             0.14f,2,fold_%2==0 ? fold : rubber);
    }
}

// Driving-cab bodyside (ENV-133) between the passenger bulkhead (`inner`) and the cab front
// (`nose`); `direction` is +1 for the cab at +Z. A solid equipment-room wall with a louvre
// grille, a hinged cab door with drop-light, grab handles and kick step, and a fixed side window
// replace the former single 7 m pane. The lower band, livery and a bogie under the cab match
// the passenger cars. Glazing goes to the transparent batch.
inline constexpr float kCabDoorFromNose=2.55f;
inline constexpr float kCabDoorWidth=0.86f;
inline constexpr float kCabBogieFromNose=3.4f;
inline void addCabBodyside(MeshBuilder& mesh,MeshBuilder& glass,float side,float direction,
                           float inner,float nose,const Microsoft::Xna::Framework::Color& shell,
                           const Microsoft::Xna::Framework::Color& serviceColour)
{
    using namespace Microsoft::Xna::Framework;
    const Color band(44,52,56,255),seal(22,27,29,255),louvre(70,78,82,255),steel(150,156,158,255);
    const float start=std::min(inner,nose),end=std::max(inner,nose);
    const auto z=[&](float fromNose) { return nose-direction*fromNose; };
    const auto span=[&](float a,float b) { return std::pair{std::min(z(a),z(b)),std::max(z(a),z(b))}; };
    // Lower body, skirt and one bogie close to the cab, as on a driving trailer.
    const float bogie=z(kCabBogieFromNose);
    const float centre=direction>0 ? bogie-kBogieOffset : bogie+kBogieOffset;
    // No passenger doorway here: a zero-width "door" at the section end keeps every band inside.
    addLowerBodyside(mesh,side,start,end,centre,end,end,shell,direction<0,direction>0);
    addLiveryBands(mesh,side,start,end,end,end,serviceColour);
    // Structural wall from sill to cantrail; openings are cut by splitting it into spans.
    const auto wall=[&](float y0,float y1,float a,float b,const Color& colour) {
        const auto [z0,z1]=span(a,b);
        if (z1-z0>0.01f) detail::sideBox(mesh,side,1.42f,1.50f,y0,y1,z0,z1,colour);
    };
    const float length=std::abs(nose-inner);
    const float doorBack=kCabDoorFromNose,doorFront=kCabDoorFromNose-kCabDoorWidth;
    const float windowBack=doorFront-0.22f,windowFront=0.30f;
    wall(0.86f,2.55f,doorBack,length,shell);                     // Equipment-room wall.
    wall(0.86f,1.24f,windowBack,windowFront,shell);              // Below the side window.
    wall(2.40f,2.55f,windowBack,windowFront,shell);              // Above the side window.
    wall(0.86f,2.55f,doorFront,windowBack,shell);                // Door-to-window post.
    wall(0.86f,2.55f,windowFront,0.0f,shell);                    // Front corner post.
    // Dark window-band skin continues the passenger-car identity across the equipment room.
    {
        const auto [z0,z1]=span(doorBack+0.25f,length-0.15f);
        detail::sideBox(mesh,side,1.501f,1.505f,1.18f,2.34f,z0,z1,band);
        // Louvred equipment-room grille in the band.
        const auto [g0,g1]=span(doorBack+0.9f,doorBack+2.6f);
        for (float y=1.36f;y<2.16f;y+=0.10f)
            detail::sideBox(mesh,side,1.505f,1.512f,y,y+0.045f,g0,g1,louvre);
        detail::sideBox(mesh,side,1.505f,1.514f,1.30f,2.22f,g0-0.03f,g0,seal);
        detail::sideBox(mesh,side,1.505f,1.514f,1.30f,2.22f,g1,g1+0.03f,seal);
    }
    // Hinged cab door: leaf with drop-light, perimeter seal, grab handles and a kick step.
    {
        const auto [d0,d1]=span(doorBack,doorFront);
        detail::sideBox(mesh,side,1.44f,1.497f,0.30f,2.42f,d0,d1,shell);
        detail::sideBox(mesh,side,1.497f,1.505f,0.44f,0.80f,d0,d1,serviceColour);
        detail::sideBox(mesh,side,1.497f,1.506f,0.80f,0.845f,d0,d1,Color(226,186,42,255));
        detail::sideBox(mesh,side,1.497f,1.508f,0.28f,2.44f,d0-0.03f,d0,seal);
        detail::sideBox(mesh,side,1.497f,1.508f,0.28f,2.44f,d1,d1+0.03f,seal);
        detail::sideBox(mesh,side,1.497f,1.508f,2.42f,2.45f,d0,d1,seal);
        detail::sideBox(mesh,side,1.497f,1.504f,1.52f,2.30f,d0+0.10f,d1-0.10f,seal);
        glass.addBox(Vector3{std::min(side*1.44f,side*1.503f),1.55f,d0+0.13f},
                     Vector3{std::max(side*1.44f,side*1.503f),2.27f,d1-0.13f},Color(52,96,116,70));
        for (float handle : {d0-0.09f,d1+0.09f})
            detail::sideBox(mesh,side,1.50f,1.545f,0.95f,2.05f,handle-0.017f,handle+0.017f,steel);
        detail::sideBox(mesh,side,1.30f,1.56f,0.16f,0.21f,d0+0.06f,d1-0.06f,Color(64,68,70,255));
        detail::sideBox(mesh,side,1.42f,1.56f,0.21f,0.40f,d0+0.10f,d0+0.14f,Color(64,68,70,255));
        detail::sideBox(mesh,side,1.42f,1.56f,0.21f,0.40f,d1-0.14f,d1-0.10f,Color(64,68,70,255));
    }
    // Fixed side window with a dark gasket.
    {
        const auto [w0,w1]=span(windowBack,windowFront);
        detail::sideBox(mesh,side,1.501f,1.508f,1.24f,1.28f,w0,w1,seal);
        detail::sideBox(mesh,side,1.501f,1.508f,2.36f,2.40f,w0,w1,seal);
        detail::sideBox(mesh,side,1.501f,1.508f,1.24f,2.40f,w0,w0+0.04f,seal);
        detail::sideBox(mesh,side,1.501f,1.508f,1.24f,2.40f,w1-0.04f,w1,seal);
        glass.addBox(Vector3{std::min(side*1.42f,side*1.50f),1.28f,w0+0.04f},
                     Vector3{std::max(side*1.42f,side*1.50f),2.36f,w1-0.04f},Color(52,96,116,64));
    }
}

// Cab exterior: a plan-view tapered lower nose with a front plate carrying coupler, anti-climber
// ribs and head/marker lamps, a roof visor over the windscreen, a windscreen gasket and
// external wipers. `direction` is +1 for the cab at +Z. Lamps go to the unlit emitter batch.
inline void addCabExterior(MeshBuilder& mesh,MeshBuilder& emitters,float direction,float nose,
                           const Microsoft::Xna::Framework::Color& trim)
{
    using namespace Microsoft::Xna::Framework;
    const Color dark(40,44,46,255),steel(118,126,128,255),gasket(26,31,33,255);
    const float forward=direction;
    const float plateZ=nose+forward*0.45f;
    const auto zmin=[&](float a,float b) { return std::min(a,b); };
    const auto zmax=[&](float a,float b) { return std::max(a,b); };
    // Lower nose block and its two tapered corner panels with top caps.
    mesh.addBox(Vector3{-0.80f,0.0f,zmin(nose,plateZ)},Vector3{0.80f,1.35f,zmax(nose,plateZ)},trim);
    for (float side : {-1.0f,1.0f})
    {
        const Vector3 from{side*1.46f,0.675f,nose},to{side*0.80f,0.675f,plateZ};
        const Vector3 d=to-from;
        const float length=d.Length();
        const float yaw=std::atan2(d.X,d.Z);
        mesh.addBox(Vector3{-0.5f,-0.5f,-0.5f},Vector3{0.5f,0.5f,0.5f},trim,
                    Matrix::CreateScale(0.10f,1.35f,length)*Matrix::CreateRotationY(yaw)
                    *Matrix::CreateTranslation((from+to)*0.5f));
        detail::orientedTriangle(mesh,Vector3{side*1.46f,1.35f,nose},Vector3{side*0.80f,1.35f,plateZ},
                                 Vector3{side*0.80f,1.35f,nose},Vector3{0,1,0},trim);
    }
    // Coupler shank and head, anti-climber ribs.
    mesh.addBox(Vector3{-0.12f,0.50f,zmin(plateZ,plateZ+forward*0.40f)},
                Vector3{0.12f,0.62f,zmax(plateZ,plateZ+forward*0.40f)},dark);
    mesh.addBox(Vector3{-0.21f,0.42f,zmin(plateZ+forward*0.40f,plateZ+forward*0.62f)},
                Vector3{0.21f,0.72f,zmax(plateZ+forward*0.40f,plateZ+forward*0.62f)},dark);
    for (float y : {0.98f,1.10f,1.22f})
        mesh.addBox(Vector3{-0.70f,y,zmin(plateZ,plateZ+forward*0.05f)},
                    Vector3{0.70f,y+0.045f,zmax(plateZ,plateZ+forward*0.05f)},steel);
    // Head and marker lamp clusters on the front plate and a high headlamp in the visor.
    for (float x : {-0.56f,0.56f})
    {
        mesh.addRoundedBoxXY(Vector3{x-0.17f,0.70f,zmin(plateZ,plateZ+forward*0.035f)},
                             Vector3{x+0.17f,0.92f,zmax(plateZ,plateZ+forward*0.035f)},0.04f,2,dark);
        emitters.addBox(Vector3{x-0.12f,0.75f,zmin(plateZ+forward*0.035f,plateZ+forward*0.045f)},
                        Vector3{x+0.02f,0.87f,zmax(plateZ+forward*0.035f,plateZ+forward*0.045f)},
                        Color(250,246,222,255));
        emitters.addBox(Vector3{x+0.06f,0.77f,zmin(plateZ+forward*0.035f,plateZ+forward*0.045f)},
                        Vector3{x+0.13f,0.85f,zmax(plateZ+forward*0.035f,plateZ+forward*0.045f)},
                        Color(222,46,42,255));
    }
    // Roof visor sloping from the roof line down toward the windscreen head.
    {
        const float tilt=forward>0 ? 0.52f : -0.52f;
        mesh.addBox(Vector3{-1.46f,-0.045f,-0.36f},Vector3{1.46f,0.045f,0.36f},
                    Color(196,202,202,255),
                    Matrix::CreateRotationX(tilt)*Matrix::CreateTranslation(0.0f,2.93f,nose-forward*0.17f));
    }
    mesh.addRoundedBoxXY(Vector3{-0.17f,2.74f,zmin(nose+forward*0.08f,nose+forward*0.13f)},
                         Vector3{0.17f,2.92f,zmax(nose+forward*0.08f,nose+forward*0.13f)},0.04f,2,dark);
    emitters.addBox(Vector3{-0.12f,2.78f,zmin(nose+forward*0.13f,nose+forward*0.14f)},
                    Vector3{0.12f,2.88f,zmax(nose+forward*0.13f,nose+forward*0.14f)},Color(250,246,222,255));
    // Windscreen gasket and external wipers.
    const float g0=zmin(nose+forward*0.08f,nose+forward*0.10f),g1=zmax(nose+forward*0.08f,nose+forward*0.10f);
    mesh.addBox(Vector3{-1.12f,1.36f,g0},Vector3{1.12f,1.42f,g1},gasket);
    mesh.addBox(Vector3{-1.12f,2.68f,g0},Vector3{1.12f,2.74f,g1},gasket);
    mesh.addBox(Vector3{-1.12f,1.36f,g0},Vector3{-1.06f,2.74f,g1},gasket);
    mesh.addBox(Vector3{1.06f,1.36f,g0},Vector3{1.12f,2.74f,g1},gasket);
    for (float x : {-0.42f,0.42f})
    {
        const Matrix wiper=Matrix::CreateRotationZ(x<0.0f ? -0.49f : 0.49f)
            *Matrix::CreateTranslation(x,1.98f,nose+forward*0.115f);
        mesh.addBox(Vector3{-0.34f,-0.016f,-0.012f},Vector3{0.34f,0.016f,0.012f},gasket,wiper);
        mesh.addBox(Vector3{-0.025f,-0.14f,-0.018f},Vector3{0.025f,0.14f,0.018f},gasket,
                    Matrix::CreateTranslation(x,1.62f,nose+forward*0.112f));
    }
}

} // namespace TrainSim::Render
