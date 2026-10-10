#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace TrainSim::Render {

// Close-inspection passenger fittings. All coordinates are train-local metres: X across the
// car, Y above rail level, Z along the train. Everything stays inside the shared interior batch.

// Seat support: a bevelled two-step base plate (not one flat polygon), a collared twelve-sided
// column and the existing bolt pair, all within the earlier pedestal footprint.
inline void addSeatPedestal(MeshBuilder& mesh,float x,float z,
                            const Microsoft::Xna::Framework::Color& frame)
{
    using namespace Microsoft::Xna::Framework;
    const Color shadeFrame(static_cast<int>(frame.getRProperty()*0.86f),
                           static_cast<int>(frame.getGProperty()*0.86f),
                           static_cast<int>(frame.getBProperty()*0.86f),255);
    mesh.addCylinder(Vector3{x,0.080f,z},0.125f,0.014f,12,shadeFrame);
    mesh.addCylinder(Vector3{x,0.094f,z},0.104f,0.016f,12,frame);
    mesh.addCylinder(Vector3{x,0.110f,z},0.062f,0.022f,10,shadeFrame);
    mesh.addCylinder(Vector3{x,0.132f,z},0.044f,0.262f,10,frame);
    mesh.addCylinder(Vector3{x,0.392f,z},0.058f,0.020f,10,shadeFrame);
    for (float boltZ : {-0.063f,0.063f})
        mesh.addCylinder(Vector3{x,0.110f,z+boltZ},0.014f,0.010f,6,Color(44,49,49,255));
}

// Overhead suitcase: rounded shell, a seam band, corner guards, a loop handle on two posts
// and, for wheeled cases, two castors at one end.
inline void addRackSuitcase(MeshBuilder& mesh,float xmin,float xmax,float z,float halfLength,
                            float top,const Microsoft::Xna::Framework::Color& shell,bool wheeled)
{
    using namespace Microsoft::Xna::Framework;
    const Color trim(50,54,54,255),guard(40,43,44,255);
    const Color seam(static_cast<int>(shell.getRProperty()*0.72f),
                     static_cast<int>(shell.getGProperty()*0.72f),
                     static_cast<int>(shell.getBProperty()*0.72f),255);
    constexpr float bottom=2.23f;
    mesh.addRoundedBoxXZ(Vector3{xmin,bottom,z-halfLength},Vector3{xmax,top,z+halfLength},
                         0.035f,2,shell);
    const float midY=(bottom+top)*0.5f;
    mesh.addRoundedBoxXZ(Vector3{xmin-0.004f,midY-0.012f,z-halfLength-0.004f},
                         Vector3{xmax+0.004f,midY+0.012f,z+halfLength+0.004f},0.038f,2,seam);
    for (float cz : {z-halfLength+0.03f,z+halfLength-0.03f})
        for (float cx : {xmin+0.03f,xmax-0.03f})
            mesh.addBox(Vector3{cx-0.034f,bottom-0.003f,cz-0.034f},
                        Vector3{cx+0.034f,bottom+0.035f,cz+0.034f},guard);
    const float cx=(xmin+xmax)*0.5f;
    for (float post : {-0.075f,0.075f})
        mesh.addBox(Vector3{cx-0.012f,top-0.005f,z+post-0.012f},
                    Vector3{cx+0.012f,top+0.045f,z+post+0.012f},trim);
    mesh.addRoundedBoxXZ(Vector3{cx-0.016f,top+0.045f,z-0.095f},
                         Vector3{cx+0.016f,top+0.075f,z+0.095f},0.012f,2,trim);
    mesh.addBox(Vector3{cx-0.004f,top+0.001f,z-halfLength+0.05f},
                Vector3{cx+0.004f,top+0.006f,z+halfLength-0.05f},seam);
    if (wheeled)
        for (float wx : {xmin+0.05f,xmax-0.05f})
            mesh.addEllipsoid(Vector3{wx,bottom+0.01f,z+halfLength-0.036f},
                              Vector3{0.012f,0.028f,0.028f},8,4,guard,false);
}

// Cab door: recessed leaf in a proud frame, hinge knuckles, kick plate, escutcheon with a lever
// handle and lock cylinder. `face` is +1 when the saloon side of the bulkhead lies toward +Z.
inline void addCabDoorHardware(MeshBuilder& mesh,float z,float face)
{
    using namespace Microsoft::Xna::Framework;
    const Color frame(96,110,116,255),steel(150,158,160,255),dark(28,35,38,255);
    const Color handle(222,186,53,255);
    const float surface=z+face*0.10f;
    const auto proud=[&](float depth) { return surface+face*depth; };
    const auto slab=[&](float x0,float x1,float y0,float y1,float near,float far,const Color& c) {
        mesh.addBox(Vector3{x0,y0,std::min(proud(near),proud(far))},
                    Vector3{x1,y1,std::max(proud(near),proud(far))},c);
    };
    // Frame and leaf gap lines.
    slab(-0.60f,-0.54f,0.0f,2.48f,0.0f,0.022f,frame);
    slab(0.54f,0.60f,0.0f,2.48f,0.0f,0.022f,frame);
    slab(-0.60f,0.60f,2.42f,2.48f,0.0f,0.022f,frame);
    slab(-0.54f,-0.535f,0.02f,2.42f,0.0f,0.012f,dark);
    slab(0.535f,0.54f,0.02f,2.42f,0.0f,0.012f,dark);
    slab(-0.535f,0.535f,2.415f,2.42f,0.0f,0.012f,dark);
    // Hinge knuckles on the non-handle side and a brushed kick plate.
    for (float y : {0.42f,1.30f,2.18f})
        slab(-0.565f,-0.525f,y-0.06f,y+0.06f,0.016f,0.034f,steel);
    slab(-0.50f,0.50f,0.03f,0.28f,0.0f,0.008f,steel);
    // Escutcheon, lever handle on a stem and lock cylinder sit below the warning band so the
    // hardware never overlaps the STAFF ONLY frame.
    slab(0.36f,0.48f,0.36f,0.66f,0.0f,0.012f,dark);
    mesh.addRoundedBoxXY(Vector3{0.402f,0.557f,std::min(proud(0.012f),proud(0.024f))},
                         Vector3{0.438f,0.593f,std::max(proud(0.012f),proud(0.024f))},
                         0.017f,3,steel);
    slab(0.405f,0.435f,0.57f,0.60f,0.012f,0.060f,handle);
    mesh.addRoundedBoxXZ(Vector3{0.245f,0.565f,std::min(proud(0.045f),proud(0.075f))},
                         Vector3{0.435f,0.605f,std::max(proud(0.045f),proud(0.075f))},
                         0.014f,2,handle);
    mesh.addRoundedBoxXY(Vector3{0.409f,0.404f,std::min(proud(0.012f),proud(0.020f))},
                         Vector3{0.431f,0.426f,std::max(proud(0.012f),proud(0.020f))},
                         0.010f,3,Color(60,66,68,255));
}

// Extinguisher accessories: wall strap bracket with base cup, hose with nozzle, pressure gauge.
inline void addExtinguisherFittings(MeshBuilder& mesh,float x,float fixtureZ,float wallZ)
{
    using namespace Microsoft::Xna::Framework;
    const Color bracket(58,64,66,255),rubber(26,28,30,255),brass(176,142,74,255);
    const float wallFace=wallZ-0.035f;
    mesh.addBox(Vector3{x-0.03f,0.30f,wallFace-0.012f},Vector3{x+0.03f,0.66f,wallFace},bracket);
    mesh.addBox(Vector3{x-0.095f,0.50f,fixtureZ-0.095f},Vector3{x+0.095f,0.53f,wallFace},
                bracket);
    mesh.addCylinder(Vector3{x,0.225f,fixtureZ},0.090f,0.035f,12,bracket);
    // Hose clipped down the side of the body.
    const Vector3 valve{x+0.030f,0.80f,fixtureZ-0.055f};
    const Vector3 clip{x+0.090f,0.52f,fixtureZ-0.070f};
    const Vector3 nozzle{x+0.085f,0.30f,fixtureZ-0.075f};
    const auto tube=[&](const Vector3& a,const Vector3& b) {
        const Vector3 d=b-a;
        const float length=d.Length();
        const float yaw=std::atan2(d.X,d.Z);
        const float pitch=-std::atan2(d.Y,std::sqrt(d.X*d.X+d.Z*d.Z));
        mesh.addBox(Vector3{-0.5f,-0.5f,-0.5f},Vector3{0.5f,0.5f,0.5f},rubber,
                    Matrix::CreateScale(0.016f,0.016f,length)*Matrix::CreateRotationX(pitch)
                    *Matrix::CreateRotationY(yaw)*Matrix::CreateTranslation((a+b)*0.5f));
    };
    tube(valve,clip);
    tube(clip,nozzle);
    mesh.addCylinder(Vector3{x+0.085f,0.24f,fixtureZ-0.075f},0.014f,0.06f,8,rubber);
    mesh.addBox(Vector3{x+0.075f,0.505f,fixtureZ-0.082f},Vector3{x+0.105f,0.535f,fixtureZ-0.058f},
                bracket);
    // Gauge on the valve body facing the passenger.
    mesh.addBox(Vector3{x-0.062f,0.775f,fixtureZ-0.070f},Vector3{x-0.034f,0.803f,fixtureZ-0.040f},
                brass);
    mesh.addBox(Vector3{x-0.060f,0.777f,fixtureZ-0.073f},Vector3{x-0.036f,0.801f,fixtureZ-0.070f},
                Color(236,236,228,255));
    mesh.addBox(Vector3{x-0.049f,0.787f,fixtureZ-0.0745f},Vector3{x-0.047f,0.799f,fixtureZ-0.073f},
                Color(200,40,40,255));
}

// Emergency intercom: bezelled housing, speaker slots, a round red call button in a raised
// ring, a green answer lamp and legend plates whose text the caller prints.
inline void addEmergencyIntercom(MeshBuilder& mesh,float x,float wallZ)
{
    using namespace Microsoft::Xna::Framework;
    const Color housing(31,49,57,255),bezel(54,74,82,255),plate(204,213,210,255);
    const float front=wallZ-0.075f;
    mesh.addRoundedBoxXY(Vector3{x-0.23f,1.05f,front},Vector3{x+0.23f,1.78f,wallZ-0.020f},
                         0.02f,2,housing);
    mesh.addRoundedBoxXY(Vector3{x-0.215f,1.065f,front-0.006f},
                         Vector3{x+0.215f,1.765f,front},0.016f,2,bezel);
    mesh.addBox(Vector3{x-0.15f,1.14f,front-0.012f},Vector3{x+0.15f,1.65f,front-0.006f},plate);
    for (float speakerX : {-0.075f,-0.025f,0.025f,0.075f})
        mesh.addBox(Vector3{x+speakerX-0.014f,1.51f,front-0.020f},
                    Vector3{x+speakerX+0.014f,1.56f,front-0.012f},Color(55,67,70,255));
    // Call button: raised ring and domed red button.
    mesh.addRoundedBoxXY(Vector3{x-0.075f,1.215f,front-0.024f},Vector3{x+0.075f,1.365f,front-0.012f},
                         0.072f,4,Color(70,80,84,255));
    mesh.addRoundedBoxXY(Vector3{x-0.058f,1.232f,front-0.034f},Vector3{x+0.058f,1.348f,front-0.024f},
                         0.056f,4,Color(199,48,42,255));
    mesh.addEllipsoid(Vector3{x,1.29f,front-0.034f},Vector3{0.040f,0.040f,0.010f},10,3,
                      Color(222,72,64,255),true);
    // Answer lamp with bezel; legend strips above the button and under the speaker.
    mesh.addRoundedBoxXY(Vector3{x-0.055f,1.665f,front-0.016f},Vector3{x+0.055f,1.735f,front-0.012f},
                         0.02f,2,Color(70,80,84,255));
    mesh.addBox(Vector3{x-0.045f,1.675f,front-0.021f},Vector3{x+0.045f,1.725f,front-0.016f},
                Color(81,206,119,255));
    mesh.addBox(Vector3{x-0.13f,1.395f,front-0.016f},Vector3{x+0.13f,1.455f,front-0.012f},
                Color(46,62,69,255));
    mesh.addBox(Vector3{x-0.13f,1.17f,front-0.016f},Vector3{x+0.13f,1.205f,front-0.012f},
                Color(46,62,69,255));
}

// Panel fastener seated in a shallow countersink; close in tone to the panel.
inline void addSeatedPanelFastener(MeshBuilder& mesh,float x,float y,float z,float face)
{
    using namespace Microsoft::Xna::Framework;
    const float a=z,b=z+face*0.004f,c=z+face*0.007f;
    mesh.addBox(Vector3{x-0.012f,y-0.012f,std::min(a,b)},Vector3{x+0.012f,y+0.012f,std::max(a,b)},
                Color(96,104,103,255));
    mesh.addBox(Vector3{x-0.007f,y-0.007f,std::min(b,c)},Vector3{x+0.007f,y+0.007f,std::max(b,c)},
                Color(150,154,148,255));
}

// Gangway bridge plate: crowned plate with fine lengthwise anti-slip grooves, hinge lines at
// both ends and yellow edge nosings, instead of wide raised transverse bars.
inline void addGangwayBridgePlate(MeshBuilder& mesh,float z)
{
    using namespace Microsoft::Xna::Framework;
    mesh.addBox(Vector3{-0.55f,0.080f,z-0.34f},Vector3{0.55f,0.097f,z+0.34f},Color(112,117,115,255));
    for (float x=-0.45f;x<0.46f;x+=0.09f)
        mesh.addBox(Vector3{x-0.003f,0.097f,z-0.30f},Vector3{x+0.003f,0.0985f,z+0.30f},
                    Color(62,68,70,255));
    for (float hinge : {-0.325f,0.325f})
        mesh.addBox(Vector3{-0.53f,0.097f,z+hinge-0.012f},Vector3{0.53f,0.103f,z+hinge+0.012f},
                    Color(84,90,90,255));
    for (float x : {-0.53f,0.53f})
        mesh.addBox(Vector3{x-0.018f,0.097f,z-0.31f},Vector3{x+0.018f,0.104f,z+0.31f},
                    Color(225,181,45,255));
}

// Domed end caps and mount returns finish a vertical grab rail.
inline void addGrabRailEnds(MeshBuilder& mesh,float x,float z,float bottom,float top,
                            float radius,const Microsoft::Xna::Framework::Color& metal)
{
    using namespace Microsoft::Xna::Framework;
    const Color cap(static_cast<int>(metal.getRProperty()*0.9f),
                    static_cast<int>(metal.getGProperty()*0.9f),
                    static_cast<int>(metal.getBProperty()*0.9f),255);
    mesh.addEllipsoid(Vector3{x,top,z},Vector3{radius*1.08f,radius*0.9f,radius*1.08f},10,4,cap,true);
    mesh.addEllipsoid(Vector3{x,bottom,z},Vector3{radius*1.08f,radius*0.9f,radius*1.08f},10,4,cap,true);
}

// Ceiling HVAC diffuser: a proud trim frame around a recessed dark plenum, angled light-toned
// slats with visible gaps, a centre divider and corner fasteners, instead of a thick block
// with dark slots. Coordinates are train-local; the ceiling face is at y 2.806.
inline void addCeilingDiffuser(MeshBuilder& mesh,float x,float z)
{
    using namespace Microsoft::Xna::Framework;
    const Color trim(188,195,194,255),slat(160,168,167,255),plenum(44,52,54,255);
    const Color fastener(96,104,104,255);
    constexpr float halfX=0.20f,halfZ=0.34f,face=2.806f;
    // Dark recessed plenum behind the slats, then the trim frame standing 8 mm below the ceiling.
    mesh.addBox(Vector3{x-halfX+0.02f,face+0.014f,z-halfZ+0.02f},
                Vector3{x+halfX-0.02f,face+0.028f,z+halfZ-0.02f},plenum);
    for (const auto& [x0,x1,z0,z1] : {
             std::array{x-halfX,x-halfX+0.022f,z-halfZ,z+halfZ},
             std::array{x+halfX-0.022f,x+halfX,z-halfZ,z+halfZ},
             std::array{x-halfX,x+halfX,z-halfZ,z-halfZ+0.022f},
             std::array{x-halfX,x+halfX,z+halfZ-0.022f,z+halfZ}})
        mesh.addBox(Vector3{x0,face-0.008f,z0},Vector3{x1,face+0.016f,z1},trim);
    mesh.addBox(Vector3{x-0.009f,face-0.004f,z-halfZ+0.02f},Vector3{x+0.009f,face+0.016f,z+halfZ-0.02f},
                trim);
    // Angled slats: each strip leans from the plenum down toward the trim plane.
    for (int louvre=-3;louvre<=3;++louvre)
    {
        const float cz=z+static_cast<float>(louvre)*0.080f;
        for (float side : {-1.0f,1.0f})
        {
            const float xa=side<0 ? x-halfX+0.022f : x+0.009f;
            const float xb=side<0 ? x-0.009f : x+halfX-0.022f;
            // Wound so the visible face points down into the saloon.
            mesh.addQuad(Vector3{xb,face+0.014f,cz-0.028f},Vector3{xb,face-0.002f,cz+0.024f},
                         Vector3{xa,face-0.002f,cz+0.024f},Vector3{xa,face+0.014f,cz-0.028f},slat);
        }
    }
    for (float fx : {x-halfX+0.011f,x+halfX-0.011f})
        for (float fz : {z-halfZ+0.011f,z+halfZ-0.011f})
            mesh.addBox(Vector3{fx-0.006f,face-0.010f,fz-0.006f},Vector3{fx+0.006f,face-0.008f,fz+0.006f},
                        fastener);
}

} // namespace TrainSim::Render
