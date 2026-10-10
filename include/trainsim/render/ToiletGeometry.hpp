#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include <algorithm>
#include <cmath>

namespace TrainSim::Render {

// Accessible toilet compartment fittings in train-local metres (X across, Y up, Z along).
// The compartment occupies x 0.44..1.35 and z 4.59..8.00 in the toilet car; the bowl end is
// at +Z and the sink stands beside the entrance at the -Z end. Everything joins the shared
// interior batch; no new material or draw call is introduced.
inline constexpr float kToiletCompartmentInnerX=0.445f;
inline constexpr float kToiletCompartmentOuterX=1.345f;
inline constexpr float kToiletCompartmentBeginZ=4.60f;
inline constexpr float kToiletCompartmentEndZ=7.99f;
inline constexpr float kToiletFloorY=0.099f;

// Smooth oval basin: rim ring, inner slope and a bottom disc, shaded like a glazed ceramic.
inline void addSanitaryBowl(MeshBuilder& mesh,const Microsoft::Xna::Framework::Vector3& centre,
                            float rx,float rz,float depth)
{
    using namespace Microsoft::Xna::Framework;
    constexpr int sides=24;
    constexpr float pi=3.14159265358979f;
    const auto point=[&](int index,float radius,float y) {
        const float angle=static_cast<float>(index)*2.0f*pi/sides;
        return Vector3{centre.X+rx*radius*std::cos(angle),centre.Y+y,
                       centre.Z+rz*radius*std::sin(angle)};
    };
    for (int i=0;i<sides;++i)
    {
        const auto normal=[&](int index,bool inside) {
            const float angle=static_cast<float>(index)*2.0f*pi/sides;
            const float slope=inside ? 0.42f/(depth*0.73f-0.012f) : 0.48f/depth;
            const float sign=inside ? -1.0f : 1.0f;
            return Vector3{sign*std::cos(angle)/rx,-sign*slope,sign*std::sin(angle)/rz};
        };
        const Vector3 outer=point(i,1.0f,0.0f),nextOuter=point(i+1,1.0f,0.0f);
        const Vector3 inner=point(i,0.76f,-0.012f),nextInner=point(i+1,0.76f,-0.012f);
        const Vector3 bottom=point(i,0.34f,-depth*0.73f),nextBottom=point(i+1,0.34f,-depth*0.73f);
        mesh.addQuad(point(i,0.52f,-depth),outer,nextOuter,point(i+1,0.52f,-depth),
                     Color(221,225,221,255),normal(i,false),normal(i,false),
                     normal(i+1,false),normal(i+1,false));
        mesh.addQuad(outer,inner,nextInner,nextOuter,Color(242,243,236,255));
        mesh.addQuad(inner,bottom,nextBottom,nextInner,Color(190,204,203,255),
                     normal(i,true),normal(i,true),normal(i+1,true),normal(i+1,true));
        mesh.addTriangle(Vector3{centre.X,centre.Y-depth*0.73f,centre.Z},nextBottom,bottom,
                         Color(120,148,151,255));
    }
}

// Compact wall-hung train toilet: a shrouded pedestal, an oval bowl with a distinct seat ring
// and raised lid, a stainless cistern cover against the end wall and a labelled flush button.
inline void addTrainToilet(MeshBuilder& mesh)
{
    using namespace Microsoft::Xna::Framework;
    const Color shroud(214,220,218,255),ceramic(242,243,236,255),seat(236,238,232,255);
    const Color steel(170,178,177,255),steelDark(118,127,128,255);
    constexpr float cx=1.04f,cz=7.50f;
    // Shroud from the floor to the rim, slightly narrower than the rim so the bowl overhangs.
    mesh.addRoundedBoxXZ(Vector3{cx-0.21f,kToiletFloorY,cz-0.25f},Vector3{cx+0.21f,0.40f,cz+0.36f},
                         0.10f,3,shroud);
    mesh.addBox(Vector3{cx-0.19f,kToiletFloorY,cz-0.22f},Vector3{cx+0.19f,0.14f,cz+0.36f},
                Color(96,108,110,255));
    addSanitaryBowl(mesh,Vector3{cx,0.45f,cz},0.24f,0.30f,0.22f);
    // Seat ring: a flat oval ring above the rim, with a hinge block at the back.
    {
        constexpr int sides=24;
        constexpr float pi=3.14159265358979f;
        const auto ring=[&](int index,float rx,float rz,float y) {
            const float angle=static_cast<float>(index)*2.0f*pi/sides;
            return Vector3{cx+rx*std::cos(angle),y,cz+rz*std::sin(angle)};
        };
        for (int i=0;i<sides;++i)
        {
            mesh.addQuad(ring(i,0.26f,0.32f,0.49f),ring(i,0.17f,0.22f,0.49f),
                         ring(i+1,0.17f,0.22f,0.49f),ring(i+1,0.26f,0.32f,0.49f),seat);
            mesh.addQuad(ring(i,0.26f,0.32f,0.455f),ring(i,0.26f,0.32f,0.49f),
                         ring(i+1,0.26f,0.32f,0.49f),ring(i+1,0.26f,0.32f,0.455f),
                         Color(214,218,212,255));
            mesh.addQuad(ring(i,0.17f,0.22f,0.49f),ring(i,0.17f,0.22f,0.46f),
                         ring(i+1,0.17f,0.22f,0.46f),ring(i+1,0.17f,0.22f,0.49f),
                         Color(200,206,202,255));
        }
    }
    mesh.addBox(Vector3{cx-0.11f,0.455f,cz+0.30f},Vector3{cx+0.11f,0.50f,cz+0.37f},steelDark);
    // Raised lid leaning on the cistern cover.
    mesh.addRoundedBoxXY(Vector3{cx-0.25f,0.50f,cz+0.33f},Vector3{cx+0.25f,1.02f,cz+0.36f},
                         0.06f,3,seat);
    // Stainless cistern cover panel with a flush button and label plate.
    mesh.addRoundedBoxXY(Vector3{cx-0.26f,0.40f,cz+0.37f},Vector3{cx+0.26f,1.12f,cz+0.48f},
                         0.03f,2,steel);
    mesh.addRoundedBoxXY(Vector3{cx-0.065f,1.14f,cz+0.405f},Vector3{cx+0.065f,1.27f,cz+0.48f},
                         0.05f,4,steelDark);
    mesh.addRoundedBoxXY(Vector3{cx-0.045f,1.16f,cz+0.385f},Vector3{cx+0.045f,1.25f,cz+0.405f},
                         0.04f,4,Color(61,105,115,255));
    mesh.addBox(Vector3{cx-0.11f,1.29f,cz+0.47f},Vector3{cx+0.11f,1.34f,cz+0.485f},
                Color(236,238,232,255));
}

// Fold-down support rail: rounded tube with wall brackets instead of two yellow slabs.
inline void addToiletSupportRail(MeshBuilder& mesh)
{
    using namespace Microsoft::Xna::Framework;
    const Color rail(231,187,48,255),bracket(150,156,152,255);
    mesh.addRoundedBoxYZ(Vector3{0.448f,1.00f,6.20f},Vector3{0.486f,1.038f,7.30f},0.017f,3,rail);
    mesh.addRoundedBoxXY(Vector3{0.448f,0.62f,7.262f},Vector3{0.486f,1.038f,7.30f},0.017f,3,rail);
    mesh.addRoundedBoxYZ(Vector3{0.448f,0.60f,6.62f},Vector3{0.486f,0.64f,7.30f},0.017f,3,rail);
    for (float z : {6.24f,6.66f,7.26f})
        mesh.addBox(Vector3{0.445f,0.985f,z-0.03f},Vector3{0.470f,1.053f,z+0.03f},bracket);
    mesh.addBox(Vector3{0.445f,0.585f,6.63f},Vector3{0.470f,0.655f,6.69f},bracket);
}

// Paper dispenser beside the bowl: rounded housing, visible roll end and a hanging tongue.
inline void addPaperDispenser(MeshBuilder& mesh)
{
    using namespace Microsoft::Xna::Framework;
    mesh.addRoundedBoxYZ(Vector3{0.447f,0.80f,7.42f},Vector3{0.545f,1.08f,7.80f},0.04f,3,
                         Color(224,229,226,255));
    mesh.addBox(Vector3{0.545f,0.86f,7.47f},Vector3{0.553f,1.02f,7.75f},Color(58,78,83,255));
    mesh.addRoundedBoxYZ(Vector3{0.553f,0.885f,7.50f},Vector3{0.556f,0.995f,7.72f},0.05f,4,
                         Color(246,246,240,255));
    mesh.addBox(Vector3{0.50f,0.72f,7.56f},Vector3{0.545f,0.80f,7.66f},Color(246,246,240,255));
}

// Sanitary cove at every wall/floor junction: a 45-degree fillet between skirting and floor.
inline void addToiletFloorCove(MeshBuilder& mesh)
{
    using namespace Microsoft::Xna::Framework;
    const Color cove(39,77,86,255);
    constexpr float rise=0.045f;
    const float y0=kToiletFloorY,y1=kToiletFloorY+rise;
    // Long walls.
    mesh.addQuad(Vector3{kToiletCompartmentInnerX,y1,kToiletCompartmentBeginZ},
                 Vector3{kToiletCompartmentInnerX,y1,kToiletCompartmentEndZ},
                 Vector3{kToiletCompartmentInnerX+rise,y0,kToiletCompartmentEndZ},
                 Vector3{kToiletCompartmentInnerX+rise,y0,kToiletCompartmentBeginZ},cove);
    mesh.addQuad(Vector3{kToiletCompartmentOuterX-rise,y0,kToiletCompartmentBeginZ},
                 Vector3{kToiletCompartmentOuterX-rise,y0,kToiletCompartmentEndZ},
                 Vector3{kToiletCompartmentOuterX,y1,kToiletCompartmentEndZ},
                 Vector3{kToiletCompartmentOuterX,y1,kToiletCompartmentBeginZ},cove);
    // End wall and entrance wall.
    mesh.addQuad(Vector3{kToiletCompartmentInnerX,y1,kToiletCompartmentEndZ},
                 Vector3{kToiletCompartmentOuterX,y1,kToiletCompartmentEndZ},
                 Vector3{kToiletCompartmentOuterX,y0,kToiletCompartmentEndZ-rise},
                 Vector3{kToiletCompartmentInnerX,y0,kToiletCompartmentEndZ-rise},cove);
    mesh.addQuad(Vector3{kToiletCompartmentInnerX,y0,kToiletCompartmentBeginZ+rise},
                 Vector3{kToiletCompartmentOuterX,y0,kToiletCompartmentBeginZ+rise},
                 Vector3{kToiletCompartmentOuterX,y1,kToiletCompartmentBeginZ},
                 Vector3{kToiletCompartmentInnerX,y1,kToiletCompartmentBeginZ},cove);
}

// Vanity: counter with rear upstand, enclosed cupboard with kick recess and waste flap, so the
// basin no longer floats; a sensor spout on a post with an aerator replaces the pale shape.
inline void addSinkVanity(MeshBuilder& mesh)
{
    using namespace Microsoft::Xna::Framework;
    const Color cabinet(214,220,218,255),counter(224,230,230,255),chrome(192,198,197,255);
    const Color dark(58,66,68,255);
    mesh.addRoundedBoxXZ(Vector3{0.84f,0.20f,3.98f},Vector3{1.32f,0.80f,4.50f},0.03f,2,cabinet);
    mesh.addBox(Vector3{0.86f,kToiletFloorY,4.02f},Vector3{1.32f,0.20f,4.46f},Color(96,108,110,255));
    mesh.addBox(Vector3{0.835f,0.52f,4.12f},Vector3{0.845f,0.70f,4.36f},Color(150,160,160,255));
    mesh.addBox(Vector3{0.83f,0.60f,4.20f},Vector3{0.84f,0.62f,4.28f},dark);
    mesh.addRoundedBoxXZ(Vector3{0.80f,0.80f,3.96f},Vector3{1.33f,0.835f,4.52f},0.025f,3,counter);
    mesh.addBox(Vector3{1.30f,0.835f,3.96f},Vector3{1.33f,0.93f,4.52f},counter);
    // Spout post, horizontal spout with sensor window and aerator; chrome ring at the base.
    mesh.addCylinder(Vector3{1.05f,0.835f,4.06f},0.030f,0.008f,12,chrome);
    mesh.addCylinder(Vector3{1.05f,0.843f,4.06f},0.018f,0.20f,12,chrome);
    mesh.addRoundedBoxXZ(Vector3{1.026f,1.02f,4.035f},Vector3{1.074f,1.062f,4.25f},0.02f,3,chrome);
    mesh.addBox(Vector3{1.038f,1.005f,4.205f},Vector3{1.062f,1.02f,4.235f},dark);
    mesh.addBox(Vector3{1.036f,1.03f,4.252f},Vector3{1.064f,1.05f,4.258f},Color(28,34,36,255));
    // Drain strainer: chrome ring and dark outlet.
    mesh.addDiscXZ(Vector3{1.04f,0.7835f,4.25f},0.034f,16,chrome);
    mesh.addDiscXZ(Vector3{1.04f,0.784f,4.25f},0.016f,12,Color(40,48,50,255));
}

// Mirror: bevelled frame and a vertical gradient with a slanted highlight approximating what the
// compartment would reflect, without a render target.
inline void addToiletMirror(MeshBuilder& mesh)
{
    using namespace Microsoft::Xna::Framework;
    const Color frame(164,176,176,255),bevel(196,206,206,255);
    constexpr float x=1.322f;
    mesh.addBox(Vector3{1.305f,1.00f,3.96f},Vector3{1.350f,1.06f,4.96f},frame);
    mesh.addBox(Vector3{1.305f,2.09f,3.96f},Vector3{1.350f,2.15f,4.96f},frame);
    mesh.addBox(Vector3{1.305f,1.00f,3.96f},Vector3{1.350f,2.15f,4.02f},frame);
    mesh.addBox(Vector3{1.305f,1.00f,4.90f},Vector3{1.350f,2.15f,4.96f},frame);
    for (float z : {4.02f,4.88f})
        mesh.addBox(Vector3{1.318f,1.06f,z},Vector3{1.326f,2.09f,z+0.02f},bevel);
    const Color top(206,222,226,255),middle(176,192,196,255),bottom(72,126,141,255);
    const auto band=[&](float y0,float y1,const Color& c0,const Color& c1) {
        mesh.addQuad(Vector3{x,y0,4.04f},Vector3{x,y0,4.88f},Vector3{x,y1,4.88f},Vector3{x,y1,4.04f},
                     c0,c0,c1,c1);
    };
    band(1.06f,1.42f,bottom,middle);
    band(1.42f,2.09f,middle,top);
    mesh.addQuad(Vector3{x-0.001f,1.15f,4.60f},Vector3{x-0.001f,1.15f,4.70f},
                 Vector3{x-0.001f,2.05f,4.30f},Vector3{x-0.001f,2.05f,4.20f},
                 Color(222,232,234,255));
}

// Soap dispenser with sight window, push plate and nozzle, plus a towel dispenser with a slot
// and a hanging towel, on the sink-side wall.
inline void addWashroomDispensers(MeshBuilder& mesh)
{
    using namespace Microsoft::Xna::Framework;
    const Color housing(224,229,226,255),dark(58,78,83,255);
    mesh.addRoundedBoxYZ(Vector3{1.285f,1.12f,4.92f},Vector3{1.345f,1.46f,5.18f},0.03f,3,housing);
    mesh.addBox(Vector3{1.277f,1.26f,5.00f},Vector3{1.286f,1.40f,5.10f},Color(61,132,151,255));
    mesh.addRoundedBoxYZ(Vector3{1.272f,1.15f,4.97f},Vector3{1.286f,1.24f,5.13f},0.02f,3,
                         Color(196,204,203,255));
    mesh.addBox(Vector3{1.262f,1.105f,5.03f},Vector3{1.300f,1.135f,5.07f},dark);
    mesh.addRoundedBoxYZ(Vector3{1.285f,1.10f,5.26f},Vector3{1.345f,1.50f,5.62f},0.03f,3,housing);
    mesh.addBox(Vector3{1.278f,1.13f,5.32f},Vector3{1.286f,1.15f,5.56f},dark);
    mesh.addBox(Vector3{1.279f,0.98f,5.35f},Vector3{1.284f,1.13f,5.53f},Color(246,246,240,255));
}

// Doorway threshold: a brushed plate with short ramps and a dark seal joins the compartment
// floor to the vestibule floor instead of an abrupt material patch.
inline void addToiletThreshold(MeshBuilder& mesh)
{
    using namespace Microsoft::Xna::Framework;
    const Color plate(178,184,182,255),seal(34,44,47,255);
    constexpr float z0=3.56f,z1=4.58f;
    mesh.addBox(Vector3{0.34f,kToiletFloorY,z0},Vector3{0.46f,kToiletFloorY+0.012f,z1},plate);
    mesh.addQuad(Vector3{0.30f,kToiletFloorY,z0},Vector3{0.30f,kToiletFloorY,z1},
                 Vector3{0.34f,kToiletFloorY+0.012f,z1},Vector3{0.34f,kToiletFloorY+0.012f,z0},plate);
    mesh.addQuad(Vector3{0.46f,kToiletFloorY+0.012f,z0},Vector3{0.46f,kToiletFloorY+0.012f,z1},
                 Vector3{0.50f,kToiletFloorY,z1},Vector3{0.50f,kToiletFloorY,z0},plate);
    mesh.addBox(Vector3{0.395f,kToiletFloorY+0.012f,z0},Vector3{0.405f,kToiletFloorY+0.014f,z1},seal);
}

} // namespace TrainSim::Render
