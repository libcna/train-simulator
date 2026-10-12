#include "SceneShared.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::GamerServices;

namespace TrainSim::Render {

void SceneRenderer::buildInterior()
{
    MeshBuilder mesh;
    MeshBuilder glass;
    MeshBuilder rainDroplets;
    std::array<MeshBuilder,2> glassReflections;
    MeshBuilder sunlight;
    MeshBuilder interiorMaterial;
    MeshBuilder upholsteryMaterial;
    MeshBuilder interiorPanelMaterial;
    MeshBuilder interiorResilientMaterial;
    MeshBuilder interiorShadows;
    MeshBuilder interiorLighting;
    MeshBuilder interiorEmitters;
    // Fixed signage shares the antialiased passenger-display atlas instead of dot-matrix quads.
    MeshBuilder interiorLettering;
    paSpeakerGrilleCount_ = 0;
    hvacVentCount_ = 0;
    carIdentitySignCount_ = 0;
    carFunctionFixtureGroupCount_ = 0;
    cabControlGroupCount_ = 0;
    vestibulePortalDetailGroupCount_ = 0;
    overheadLuggageItemCount_ = 0;
    floorLuggageCaseCount_ = 0;
    floorLuggageWheelPairCount_ = 0;
    roundedSeatCushionCount_ = 0;
    roundedSeatBackCount_ = 0;
    seatPedestalCount_ = 0;
    seatMountFastenerCount_ = 0;
    roundedAisleArmrestCount_ = 0;
    roundedTableTopCount_ = 0;
    tableSupportPlateCount_ = 0;
    windowReflectionAccentCount_ = 0;
    interiorLightGradientQuadCount_ = 0;
    sunlightPatchCount_ = 0;
    aisleRunnerInlayCount_ = 0;
    nonCarpetedFloorZoneCount_ = 0;
    interiorCovingSectionCount_ = 0;
    ceilingPanelSeamCount_ = 0;
    vestibuleWearMarkCount_ = 0;
    const Color shell{211,218,218,255};
    const Color trim{32,102,121,255};
    const Color floor{68,72,76,255};
    const std::array<Color, 6> carSeatColors = {
        Color(31,65,94,255), Color(37,76,111,255), Color(39,101,101,255),
        Color(42,82,105,255), Color(43,70,91,255), Color(37,76,111,255)};
    const std::array<Color, 6> carSeatSeams = {
        Color(44,79,107,255), Color(50,90,124,255), Color(52,115,114,255),
        Color(55,96,118,255), Color(56,84,104,255), Color(50,90,124,255)};
    const std::array<Color, 6> carHeadrests = {
        Color(164,180,185,255), Color(151,184,190,255), Color(146,190,184,255),
        Color(190,181,130,255), Color(151,174,163,255), Color(151,184,190,255)};
    const std::array<Color, 6> carAccents = {
        Color(75,99,119,255), Color(61,101,132,255), Color(41,128,124,255),
        Color(169,137,55,255), Color(75,112,91,255), Color(52,102,124,255)};
    const Color seatBackShell{62,77,85,255};
    const Color seatFrame{95,99,99,255};
    const auto addSeatPanel = [&mesh](float xmin, float xmax, float ymin, float ymax,
                                      float z, bool normalTowardPositiveZ,
                                      const Color& color) {
        const Vector3 lowerLeft{xmin,ymin,z};
        const Vector3 lowerRight{xmax,ymin,z};
        const Vector3 upperRight{xmax,ymax,z};
        const Vector3 upperLeft{xmin,ymax,z};
        if (normalTowardPositiveZ)
            mesh.addQuad(lowerLeft,lowerRight,upperRight,upperLeft,color);
        else
            mesh.addQuad(lowerRight,lowerLeft,upperLeft,upperRight,color);
    };
    const auto addWallPanel = [&interiorPanelMaterial](
        float x, float y0, float y1, float z0, float z1,
        bool inwardNormalPositiveX, const Color& color) {
        constexpr float tileMetres = 0.90f;
        const Vector3 lowerBegin{x,y0,z0};
        const Vector3 lowerEnd{x,y0,z1};
        const Vector3 upperEnd{x,y1,z1};
        const Vector3 upperBegin{x,y1,z0};
        const Vector2 uvLowerBegin{z0/tileMetres,y0/tileMetres};
        const Vector2 uvLowerEnd{z1/tileMetres,y0/tileMetres};
        const Vector2 uvUpperEnd{z1/tileMetres,y1/tileMetres};
        const Vector2 uvUpperBegin{z0/tileMetres,y1/tileMetres};
        if (inwardNormalPositiveX)
            interiorPanelMaterial.addTexturedQuad(
                lowerEnd,lowerBegin,upperBegin,upperEnd,color,
                uvLowerEnd,uvLowerBegin,uvUpperBegin,uvUpperEnd);
        else
            interiorPanelMaterial.addTexturedQuad(
                lowerBegin,lowerEnd,upperEnd,upperBegin,color,
                uvLowerBegin,uvLowerEnd,uvUpperEnd,uvUpperBegin);
    };
    const auto addCeilingPanel = [&interiorPanelMaterial](
        float x0, float x1, float y, float z0, float z1, const Color& color) {
        constexpr float tileMetres = 0.90f;
        interiorPanelMaterial.addTexturedQuad(
            Vector3{x1,y,z0},Vector3{x1,y,z1},
            Vector3{x0,y,z1},Vector3{x0,y,z0},color,
            Vector2{x1/tileMetres,z0/tileMetres},
            Vector2{x1/tileMetres,z1/tileMetres},
            Vector2{x0/tileMetres,z1/tileMetres},
            Vector2{x0/tileMetres,z0/tileMetres});
    };
    const auto addResilientFloor = [&interiorResilientMaterial](
        float x0, float x1, float y, float z0, float z1, const Color& color) {
        constexpr float tileMetres = 0.48f;
        interiorResilientMaterial.addTexturedQuad(
            Vector3{x0,y,z0},Vector3{x0,y,z1},
            Vector3{x1,y,z1},Vector3{x1,y,z0},color,
            Vector2{x0/tileMetres,z0/tileMetres},
            Vector2{x0/tileMetres,z1/tileMetres},
            Vector2{x1/tileMetres,z1/tileMetres},
            Vector2{x1/tileMetres,z0/tileMetres});
    };

    for (int car = 0; car < 6; ++car)
    {
        const float centre = kFirstCarCentre + static_cast<float>(car) * kCarLength;
        const float start = centre - 10.25f;
        const float end = centre + 10.25f;
        const float passengerDoorZ = static_cast<float>(
            Sim::TrainInterior::passengerDoorZ(car));
        const float passengerDoorHalfOpening =
            static_cast<float>(Sim::TrainInterior::PassengerDoorHalfOpening);
        const float passengerDoorBegin = passengerDoorZ - passengerDoorHalfOpening;
        const float passengerDoorEnd = passengerDoorZ + passengerDoorHalfOpening;
        mesh.addBox(Vector3{-1.48f,-0.08f,start}, Vector3{1.48f,0.08f,end}, floor);
        // A restrained hard-wearing aisle runner breaks up the otherwise uniform floor and
        // provides a continuous visual path through the complete passenger consist.
        mesh.addBox(Vector3{-0.39f,0.081f,start+0.12f},
                    Vector3{0.39f,0.094f,end-0.12f}, Color(42,54,61,255));

        // Resilient vestibule flooring gives the surface-aware footstep change a matching visual
        // cue. The authored extent comes from TrainInterior, and shallow transverse joints keep
        // the panel from reading as another flat carpet rectangle. It remains in the interior
        // batch and therefore adds no draw call.
        const float vestibuleBegin = std::max(
            start+0.02f,passengerDoorZ-static_cast<float>(
                Sim::TrainInterior::VestibuleFloorHalfLength));
        const float vestibuleEnd = std::min(
            end-0.02f,passengerDoorZ+static_cast<float>(
                Sim::TrainInterior::VestibuleFloorHalfLength));

        const auto addCarpetSpan = [&interiorMaterial](float spanBegin, float spanEnd) {
            if (spanEnd-spanBegin < 0.12f) return;
            constexpr float tileMetres = 0.85f;
            const auto addSurface = [&](float x0, float x1, float y, const Color& color) {
                interiorMaterial.addTexturedQuad(
                    Vector3{x0,y,spanBegin},Vector3{x0,y,spanEnd},
                    Vector3{x1,y,spanEnd},Vector3{x1,y,spanBegin},color,
                    Vector2{x0/tileMetres,spanBegin/tileMetres},
                    Vector2{x0/tileMetres,spanEnd/tileMetres},
                    Vector2{x1/tileMetres,spanEnd/tileMetres},
                    Vector2{x1/tileMetres,spanBegin/tileMetres});
            };
            addSurface(-1.38f,1.38f,0.083f,Color(82,84,84,255));
            addWornCarpetRunner(interiorMaterial,spanBegin,spanEnd);
        };
        addCarpetSpan(start+0.12f,vestibuleBegin-0.05f);
        addCarpetSpan(vestibuleEnd+0.05f,end-0.12f);

        // Flush woven perimeter tape and narrow installation seams reuse the carpet batch.
        aisleRunnerInlayCount_ += addCarpetBindings(
            interiorMaterial,start+0.12f,vestibuleBegin-0.05f);
        aisleRunnerInlayCount_ += addCarpetBindings(
            interiorMaterial,vestibuleEnd+0.05f,end-0.12f);
        mesh.addBox(Vector3{-1.39f,0.095f,vestibuleBegin},
                    Vector3{1.39f,0.106f,vestibuleEnd},Color(55,72,77,255));
        addResilientFloor(-1.385f,1.385f,0.107f,vestibuleBegin+0.015f,
                          vestibuleEnd-0.015f,Color(74,91,95,255));
        const int floorJointCount = static_cast<int>(
            std::floor((vestibuleEnd-vestibuleBegin)/0.58f));
        for (int joint = 1; joint <= floorJointCount; ++joint)
        {
            const float z = vestibuleBegin+static_cast<float>(joint)*0.58f;
            mesh.addBox(Vector3{-1.34f,0.106f,z-0.010f},
                        Vector3{1.34f,0.110f,z+0.010f},Color(36,49,53,255));
        }
        // Paired low-contrast wheel/scuff tracks run inward from both doorway leaves. They add
        // believable traffic wear to the wipe-clean vestibule without raising a collision edge
        // or muddying the central wayfinding strip.
        for (float side : {-1.0f,1.0f})
        {
            const float wearBeginX = side < 0.0f ? -1.30f : 0.43f;
            const float wearEndX = side < 0.0f ? -0.43f : 1.30f;
            for (float offsetZ : {-0.22f,0.22f})
            {
                const float z = passengerDoorZ+offsetZ;
                mesh.addBox(Vector3{wearBeginX,0.109f,z-0.014f},
                            Vector3{wearEndX,0.112f,z+0.014f},Color(61,77,81,255));
                ++vestibuleWearMarkCount_;
            }
        }
        ++nonCarpetedFloorZoneCount_;
        mesh.addBox(Vector3{-1.48f,2.92f,start}, Vector3{1.48f,3.08f,end}, shell);
        // Exterior roof crown, chamfers, HVAC housing and pantographs on the powered cars.
        addRoofEquipment(mesh,start,end,centre,car == 1 || car == 4);

        // Close passenger-eye surfaces receive a restrained polymer-grain material. These thin
        // inset skins retain the structural shell and trim in the legacy batch while replacing
        // its largest perfectly flat regions. The ceiling is split around the continuous light,
        // and lower wall spans stop at the passenger doorway rather than covering its opening.
        const Color wallPanel(211,218,218,255);
        const Color ceilingPanel(222,227,226,255);
        addCeilingPanel(-1.40f,-0.115f,2.914f,start+0.03f,end-0.03f,ceilingPanel);
        addCeilingPanel(0.115f,1.40f,2.914f,start+0.03f,end-0.03f,ceilingPanel);

        for (float side : {-1.0f, 1.0f})
        {
            const float x0 = side < 0 ? -1.50f : 1.42f;
            const float x1 = side < 0 ? -1.42f : 1.50f;
            // Lower bodyside with recessed skirt, bogies, underframe equipment and livery.
            addLowerBodyside(mesh,side,start,end,centre,passengerDoorBegin,passengerDoorEnd,shell);
            addLiveryBands(mesh,side,start,end,passengerDoorBegin,passengerDoorEnd,trim);
            mesh.addBox(Vector3{x0,2.34f,start}, Vector3{x1,3.00f,end}, shell);
            const float panelX = side*1.416f;
            addWallPanel(panelX,0.10f,0.80f,start+0.04f,passengerDoorBegin-0.07f,
                         side < 0.0f,wallPanel);
            addWallPanel(panelX,0.10f,0.80f,passengerDoorEnd+0.07f,end-0.04f,
                         side < 0.0f,wallPanel);
            addWallPanel(panelX,2.40f,2.90f,start+0.04f,end-0.04f,
                         side < 0.0f,ceilingPanel);

            // Continuous pale ceiling coving and a darker wipe-clean floor skirting make the
            // wall junctions read as assembled transport panels rather than intersecting boxes.
            addCeilingCove(interiorPanelMaterial,side,start+0.04f,end-0.04f,ceilingPanel);
            ++interiorCovingSectionCount_;
            const float skirtX0 = std::min(side*1.416f,side*1.37f);
            const float skirtX1 = std::max(side*1.416f,side*1.37f);
            mesh.addBox(Vector3{skirtX0,0.095f,start+0.05f},
                        Vector3{skirtX1,0.205f,passengerDoorBegin-0.08f},
                        Color(48,79,86,255));
            mesh.addBox(Vector3{skirtX0,0.095f,passengerDoorEnd+0.08f},
                        Vector3{skirtX1,0.205f,end-0.05f},Color(48,79,86,255));
            interiorCovingSectionCount_ += 2;
            for (int pillar = 0; pillar <= 10; ++pillar)
            {
                const float z = static_cast<float>(Sim::TrainInterior::windowMullionZ(car,pillar));
                if (std::abs(z - passengerDoorZ) < 0.82f) continue;
                const float halfWidth=static_cast<float>(Sim::TrainInterior::WindowMullionWidth)*0.5f;
                mesh.addBox(Vector3{x0,0.82f,z-halfWidth},
                            Vector3{x1,2.38f,z+halfWidth}, trim);
                addExteriorMullionFace(mesh,side,z,halfWidth);
                addPassengerMullionCap(interiorPanelMaterial,side,z,halfWidth);
                const float seamX0 = side < 0.0f ? -1.38f : 0.13f;
                const float seamX1 = side < 0.0f ? -0.13f : 1.38f;
                mesh.addBox(Vector3{seamX0,2.900f,z-0.012f},
                            Vector3{seamX1,2.913f,z+0.012f},Color(174,184,184,255));
                ++ceilingPanelSeamCount_;
                // Lower-wall panel joints continue the window-bay rhythm below the glazing.
                const float innerX0 = side < 0.0f ? -1.414f : 1.408f;
                const float innerX1 = side < 0.0f ? -1.404f : 1.418f;
                mesh.addBox(Vector3{innerX0,0.18f,z-0.020f},
                            Vector3{innerX1,0.78f,z+0.020f},Color(170,181,181,255));
            }
            // The last bay ends before the car-end partition. Close that remaining pier
            // rather than leaving an unglazed strip beyond the final mullion.
            const float lastWindowEnd=static_cast<float>(Sim::TrainInterior::windowClearBeginZ(
                car,Sim::TrainInterior::WindowBaysPerCar-1)+Sim::TrainInterior::WindowClearLength);
            if (lastWindowEnd<end)
                mesh.addBox({x0,0.82f,lastWindowEnd},{x1,2.38f,end},trim);
            for (float jambZ : {passengerDoorBegin, passengerDoorEnd})
                mesh.addBox(Vector3{x0,0.04f,jambZ-0.065f},
                            Vector3{x1,2.40f,jambZ+0.065f}, trim);
            addExteriorDoorReveal(mesh,side,passengerDoorBegin,passengerDoorEnd);
            addPassengerDoorControlHousing(mesh,side,passengerDoorBegin);
            const float thresholdInner = side * 1.18f;
            const float thresholdOuter = side * 1.50f;
            mesh.addBox(Vector3{std::min(thresholdInner,thresholdOuter),0.081f,
                                passengerDoorBegin+0.07f},
                        Vector3{std::max(thresholdInner,thresholdOuter),0.098f,
                                passengerDoorEnd-0.07f}, Color(225,184,47,255));
            if (car == Sim::TrainInterior::ToiletCar && side > 0.0f)
            {
                // The accessible toilet replaces this side of the saloon. Split the continuous
                // shelf at its partition instead of letting it protrude through the compartment.
                addLuggageRackSpan(mesh,side,start+0.7f,static_cast<float>(
                    Sim::TrainInterior::ToiletRackClearanceBeginZ));
                addLuggageRackSpan(mesh,side,static_cast<float>(
                    Sim::TrainInterior::ToiletRackClearanceEndZ),end-0.7f);
            }
            else
                addLuggageRackSpan(mesh,side,start+0.7f,end-0.7f);

            const auto addWindowSpan = [&](float windowBegin, float windowEnd,
                                           int reflectionVariant) {
                if (windowEnd - windowBegin < 0.08f) return;
                addPassengerWindowFrame(mesh,side,windowBegin,windowEnd);
                addExteriorWindowGasket(mesh,side,windowBegin,windowEnd);
                const float innerX = side < 0.0f ? -1.455f : 1.445f;
                const float outerX = side < 0.0f ? -1.445f : 1.455f;
                glass.addBox(Vector3{innerX,0.88f,windowBegin},
                             Vector3{outerX,2.32f,windowEnd}, Color(110,177,201,28));
                // Rain on the outer face, shown only in wet weather (ENV-139).
                addWindowDroplets(rainDroplets,side*1.4575f,0.88f,2.32f,windowBegin,windowEnd,
                    static_cast<std::uint32_t>(std::lround((windowBegin+200.0f)*97.0f))
                        ^ (side > 0.0f ? 0x9E3779B9U : 0U));

                // Reflections live in a separate side-specific batch so draw-time view angle can
                // attenuate them. Soft edges and varied position avoid a repeated decal pattern while
                // keeping the landscape readable through the base tint.
                const float span = windowEnd - windowBegin;
                if (span > 1.20f)
                {
                    MeshBuilder& reflection = glassReflections[
                        static_cast<std::size_t>(side > 0.0f ? 1 : 0)];
                    const float glintZ = windowBegin + span
                        * ((reflectionVariant & 1) == 0 ? 0.42f : 0.58f);
                    const float reflectionX=side*1.439f;
                    addSoftWindowReflection(reflection,reflectionX,1.91f,glintZ,
                                            0.18f,span*0.28f,16);
                    addSoftWindowReflection(reflection,reflectionX,2.18f,
                                            windowBegin+span*0.53f,0.09f,span*0.32f,8);
                    windowReflectionAccentCount_ += 2;
                }
            };
            for (int window = 0; window < Sim::TrainInterior::WindowBaysPerCar; ++window)
            {
                const float windowBegin = static_cast<float>(
                    Sim::TrainInterior::windowClearBeginZ(car, window));
                const float windowEnd = windowBegin
                    + static_cast<float>(Sim::TrainInterior::WindowClearLength);
                const int reflectionVariant = car * Sim::TrainInterior::WindowBaysPerCar
                    + window + (side > 0.0f ? 1 : 0);
                if (windowEnd <= passengerDoorBegin || windowBegin >= passengerDoorEnd)
                    addWindowSpan(windowBegin, windowEnd, reflectionVariant);
                else
                {
                    addWindowSpan(windowBegin, std::min(windowEnd, passengerDoorBegin),
                                  reflectionVariant);
                    addWindowSpan(std::max(windowBegin, passengerDoorEnd), windowEnd,
                                  reflectionVariant);
                }
            }
        }

        // Soft window-shaped floor highlights make the sunny exterior affect the carriage
        // without a shadow map. The low-alpha gradient is one transparent batch for all cars;
        // opaque seats and fixtures already in the depth buffer naturally mask it. Avoid the
        // enclosed toilet floor, where the saloon partition blocks this side's direct sun.
        for (int window : {1,3,5,7})
        {
            const float centreZ = static_cast<float>(
                Sim::TrainInterior::windowClearCentreZ(car,window));
            if (car == Sim::TrainInterior::ToiletCar
                && centreZ > static_cast<float>(Sim::TrainInterior::ToiletRackClearanceBeginZ)
                && centreZ < static_cast<float>(Sim::TrainInterior::ToiletRackClearanceEndZ))
                continue;
            const float variation = static_cast<float>((car+window) % 3) * 0.09f;
            const float nearBegin = centreZ-0.54f;
            const float nearEnd = centreZ+0.54f;
            const float farShift = 0.70f+variation;
            // Match the visible top-face winding used by MeshBuilder::addBox. The warm edge is
            // beside the window and the two faint vertices carry the gradient into the aisle.
            sunlight.addQuad(
                Vector3{-1.37f,0.101f,nearBegin},
                Vector3{-1.37f,0.101f,nearEnd},
                Vector3{0.72f,0.101f,nearEnd+farShift},
                Vector3{0.72f,0.101f,nearBegin+farShift},
                Color(255,235,174,64),Color(255,235,174,64),
                Color(255,229,158,8),Color(255,229,158,8));
            ++sunlightPatchCount_;
        }

        // A broad, low-alpha reflected-light gradient makes the continuous luminaire affect its
        // surroundings instead of reading as an isolated glowing box. Four quads per car cover
        // both ceiling slopes and the upper walls in one transparent batch. The alpha remains
        // deliberately restrained so daylight and the polymer material texture still dominate.
        const Color ceilingLightWarm{255,231,166,44};
        const Color wallLightWarm{255,228,158,24};
        const Color noLight{255,231,166,0};
        interiorLighting.addQuad(
            Vector3{-0.108f,2.905f,start+0.08f},
            Vector3{-0.108f,2.905f,end-0.08f},
            Vector3{-1.39f,2.905f,end-0.08f},
            Vector3{-1.39f,2.905f,start+0.08f},
            ceilingLightWarm,ceilingLightWarm,noLight,noLight);
        interiorLighting.addQuad(
            Vector3{1.39f,2.905f,start+0.08f},
            Vector3{1.39f,2.905f,end-0.08f},
            Vector3{0.108f,2.905f,end-0.08f},
            Vector3{0.108f,2.905f,start+0.08f},
            noLight,noLight,ceilingLightWarm,ceilingLightWarm);
        interiorLighting.addQuad(
            Vector3{-1.403f,2.36f,end-0.08f},
            Vector3{-1.403f,2.36f,start+0.08f},
            Vector3{-1.403f,2.89f,start+0.08f},
            Vector3{-1.403f,2.89f,end-0.08f},
            noLight,noLight,wallLightWarm,wallLightWarm);
        interiorLighting.addQuad(
            Vector3{1.403f,2.36f,start+0.08f},
            Vector3{1.403f,2.36f,end-0.08f},
            Vector3{1.403f,2.89f,end-0.08f},
            Vector3{1.403f,2.89f,start+0.08f},
            noLight,noLight,wallLightWarm,wallLightWarm);
        interiorLightGradientQuadCount_ += 4;

        // Serviceable ceiling-light modules and modern passenger-information screens.
        // Recessed aluminium channel with replaceable opal diffuser sections. The emitting
        // face is unlit: sunlight direction must not turn a powered ceiling lamp dark brown.
        for (float segment = start+0.72f; segment < end-0.72f; segment += 1.50f)
        {
            const float finish = std::min(segment+1.38f,end-0.72f);
            addCeilingLightModule(mesh,interiorEmitters,segment,finish);
        }
        addPassengerDisplayHousing(mesh,centre);

        // Two ceiling PA grilles give the onboard chime and announcements a visible source.
        // The shallow panels and perforations remain in the immutable interior batch, so all
        // twelve speakers add detail without adding a material switch or draw call.
        for (const std::pair<float,float>& speaker : {
                 std::pair{-0.54f,-5.35f}, std::pair{0.54f,5.35f}})
        {
            const float speakerX = speaker.first;
            const float speakerZ = centre + speaker.second;
            mesh.addBox(Vector3{speakerX-0.19f,2.805f,speakerZ-0.19f},
                        Vector3{speakerX+0.19f,2.835f,speakerZ+0.19f},
                        Color(149,157,157,255));
            for (int row = -1; row <= 1; ++row)
                for (int column = -1; column <= 1; ++column)
                {
                    const float x = speakerX + static_cast<float>(column)*0.075f;
                    const float z = speakerZ + static_cast<float>(row)*0.075f;
                    mesh.addBox(Vector3{x-0.018f,2.796f,z-0.018f},
                                Vector3{x+0.018f,2.807f,z+0.018f},
                                Color(48,60,63,255));
                }
            ++paSpeakerGrilleCount_;
        }

        // Paired ceiling diffusers make the otherwise quiet climate system legible even while
        // stationary. Their shallow louvres share the same immutable batch as the speaker panels.
        for (const std::pair<float,float>& vent : {
                 std::pair{-0.56f,2.70f}, std::pair{0.56f,-2.70f}})
        {
            addCeilingDiffuser(mesh,vent.first,centre+vent.second);
            ++hvacVentCount_;
        }

        // Restrained pairs of suspended grab handles add useful scale and vestibule detail while
        // leaving the central standing envelope clear below 2.3 m.
        for (float offset : {-8.0f, -4.0f, 4.0f, 8.0f})
            for (float x : {-0.38f, 0.38f})
            {
                const float z = centre + offset;
                addSuspendedPassengerHandle(mesh,interiorResilientMaterial,x,z);
            }

        // Visible rack ribs and a few reusable cases turn the continuous shelf rail into a
        // believable luggage system. Everything remains in the one interior mesh: this detail
        // adds no material switch or draw call, and the sparse placement keeps the saloon airy.
        const std::array<Color, 6> rackLuggageColors = {
            Color(125,67,52,255), Color(48,89,122,255), Color(121,103,52,255),
            Color(64,101,78,255), Color(99,63,111,255), Color(137,80,45,255)};
        constexpr std::array<float, 4> luggageOffsets = {-6.1f,5.15f,-1.8f,1.85f};
        for (float side : {-1.0f, 1.0f})
        {
            for (int pillar : {1,3,5,7,9})
            {
                const float z = static_cast<float>(Sim::TrainInterior::windowMullionZ(car,pillar));
                if (std::abs(z-passengerDoorZ) < 0.82f) continue;
                if (!Sim::TrainInterior::overheadRackPositionAllowed(
                        car,side > 0.0f ? 1 : -1,z)) continue;
                // Bolted to finished structural mullions, never unsupported glazing.
                addLuggageRackBracket(mesh,side,z);
            }
            const int bagCount = car == 4 ? 4 : 2;
            for (int bag = 0; bag < bagCount; ++bag)
            {
                const float z = centre + luggageOffsets[static_cast<std::size_t>(bag)]
                              + side * static_cast<float>((car + bag) % 3) * 0.22f;
                if (!Sim::TrainInterior::overheadRackPositionAllowed(
                        car,side > 0.0f ? 1 : -1,z)) continue;
                const float inner = side * 0.93f;
                const float outer = side * (1.19f + 0.03f*static_cast<float>(bag));
                const float xmin = std::min(inner,outer);
                const float xmax = std::max(inner,outer);
                const float halfLength = bag % 2 == 0 ? 0.43f : 0.33f;
                const Color& caseColor = rackLuggageColors[
                    (static_cast<std::size_t>(car)*2U + static_cast<std::size_t>(bag)
                     + (side > 0.0f ? 1U : 0U)) % rackLuggageColors.size()];
                addRackSuitcase(mesh,xmin,xmax,z,halfLength,
                                2.46f+0.05f*static_cast<float>(bag),caseColor,bag%2==0);
                ++overheadLuggageItemCount_;
            }
        }

        // Fixed function signs make every saloon legible as a deliberate part of one consistent
        // train rather than six repeated rooms. A narrow family-colour band carries that identity
        // without turning the interior into a collection of unrelated themes.
        mesh.addBox(Vector3{-0.94f,2.10f,centre-0.055f},
                    Vector3{0.94f,2.32f,centre+0.055f}, Color(31,51,59,255));
        mesh.addBox(Vector3{-0.94f,2.10f,centre-0.061f},
                    Vector3{0.94f,2.145f,centre+0.061f},
                    carAccents[static_cast<std::size_t>(car)]);
        for (float face : {-1.0f, 1.0f})
            addCarRoleIcon(mesh,car,Matrix::CreateRotationY(face<0 ? kPi : 0)
                *Matrix::CreateTranslation(face*0.76f,2.205f,centre+face*0.064f),
                Color(223,232,230,255));
        ++carIdentitySignCount_;

        // Role names now share the antialiased display atlas. Remove the redundant facility
        // plaque formerly hidden behind the main screen; physical cues below remain visible.
        if (car == 2)
        {
            // Passenger-assistance call point at the accessible bay entrance.
            mesh.addBox(Vector3{-0.94f,1.52f,centre-0.065f},
                        Vector3{-0.62f,2.02f,centre+0.065f},Color(47,75,82,255));
            mesh.addBox(Vector3{-0.85f,1.66f,centre-0.076f},
                        Vector3{-0.71f,1.82f,centre+0.076f},Color(224,184,47,255));
            mesh.addBox(Vector3{-0.82f,1.88f,centre-0.077f},
                        Vector3{-0.74f,1.94f,centre+0.077f},Color(91,205,119,255));
        }
        else if (car == 5)
        {
            // A slim timetable/route-leaflet pocket gives the general saloon a plausible service
            // fixture without occupying the passenger aisle.
            mesh.addBox(Vector3{0.61f,1.55f,centre-0.060f},
                        Vector3{0.92f,2.01f,centre+0.060f},Color(71,82,84,255));
            for (int leaflet = 0; leaflet < 3; ++leaflet)
            {
                const float x = 0.65f+static_cast<float>(leaflet)*0.09f;
                mesh.addBox(Vector3{x,1.68f,centre-0.071f},
                            Vector3{x+0.07f,1.94f,centre+0.071f},
                            leaflet == 1 ? Color(224,184,47,255)
                                         : Color(208,222,221,255));
            }
        }
        ++carFunctionFixtureGroupCount_;

        // Every vestibule carries the safety equipment a passenger expects to find: an
        // end-wall extinguisher, a clearly distinct emergency intercom, and illuminated exit
        // pictograms above both door leaves. The end wall avoids sacrificing glazing or placing
        // equipment behind the final seat row. All pieces remain outside the walkable gangway.
        const float safetyWallZ = car + 1 == Sim::TrainInterior::PassengerCars
            ? 63.08f : centre + kCarLength*0.5f - 0.245f;
        const float fixtureZ = safetyWallZ - 0.09f;
        // Layer the broad vestibule end wall behind the safety fixtures. A shallow inset field,
        // perimeter trim, visible panel fasteners and a dark doorway seal replace the previous
        // uninterrupted slab while remaining outside the authoritative gangway clearance.
        const Color vestibuleSeal(22,31,34,255);
        for (float side : {-1.0f,1.0f})
        {
            const float x0 = side < 0.0f ? -1.40f : 0.67f;
            const float x1 = side < 0.0f ? -0.67f : 1.40f;
            addVestibuleServicePanels(interiorPanelMaterial,side,fixtureZ+0.020f,-1);
            for (float x : {x0+0.075f,x1-0.075f})
                for (float y : {0.29f,2.17f})
                    addSeatedPanelFastener(mesh,x,y,fixtureZ-0.002f,1.0f);
        }
        mesh.addBox(Vector3{-0.650f,0.10f,fixtureZ-0.004f},
                    Vector3{-0.608f,2.38f,fixtureZ+0.014f},vestibuleSeal);
        mesh.addBox(Vector3{0.608f,0.10f,fixtureZ-0.004f},
                    Vector3{0.650f,2.38f,fixtureZ+0.014f},vestibuleSeal);
        mesh.addBox(Vector3{-0.650f,2.34f,fixtureZ-0.004f},
                    Vector3{0.650f,2.39f,fixtureZ+0.014f},vestibuleSeal);
        ++vestibulePortalDetailGroupCount_;
        mesh.addBox(Vector3{0.78f,0.13f,safetyWallZ-0.035f},
                    Vector3{1.22f,0.94f,safetyWallZ},Color(72,79,80,255));
        addExtinguisherFittings(mesh,1.0f,fixtureZ,safetyWallZ);
        mesh.addCylinder(Vector3{1.0f,0.25f,fixtureZ},0.082f,0.46f,12,
                         Color(202,48,40,255));
        mesh.addCone(Vector3{1.0f,0.71f,fixtureZ},0.070f,0.10f,12,
                     Color(202,48,40,255));
        mesh.addCylinder(Vector3{1.0f,0.81f,fixtureZ},0.034f,0.085f,8,
                         Color(45,51,52,255));
        mesh.addBox(Vector3{0.95f,0.86f,fixtureZ-0.065f},
                    Vector3{1.05f,0.91f,fixtureZ+0.065f},Color(50,54,54,255));
        mesh.addBox(Vector3{0.96f,0.44f,fixtureZ-0.086f},
                    Vector3{1.04f,0.55f,fixtureZ-0.078f},Color(245,221,184,255));

        addEmergencyIntercom(mesh,-1.0f,safetyWallZ);
        addPrintedPlaqueText(interiorLettering,"EMERGENCY",1.452f,safetyWallZ-0.0925f,false,
                             Color(214,224,222,255),-1.0f,0.028f,0.22f);
        addPrintedPlaqueText(interiorLettering,"PRESS AND SPEAK",1.204f,safetyWallZ-0.0925f,false,
                             Color(214,224,222,255),-1.0f,0.024f,0.22f);

        for (float side : {-1.0f,1.0f})
        {
            const float x0 = side < 0.0f ? -1.435f : 1.405f;
            const float x1 = side < 0.0f ? -1.405f : 1.435f;
            mesh.addBox(Vector3{x0,2.48f,passengerDoorZ-0.33f},
                        Vector3{x1,2.76f,passengerDoorZ+0.33f},Color(38,139,78,255));
            mesh.addBox(Vector3{x0-side*0.010f,2.595f,passengerDoorZ-0.18f},
                        Vector3{x1-side*0.010f,2.635f,passengerDoorZ+0.17f},
                        Color(231,241,226,255));
            mesh.addBox(Vector3{x0-side*0.011f,2.635f,passengerDoorZ+0.10f},
                        Vector3{x1-side*0.011f,2.690f,passengerDoorZ+0.18f},
                        Color(231,241,226,255));
            mesh.addBox(Vector3{x0-side*0.011f,2.540f,passengerDoorZ+0.10f},
                        Vector3{x1-side*0.011f,2.595f,passengerDoorZ+0.18f},
                        Color(231,241,226,255));
        }

        for (int row = 0; row < Sim::TrainInterior::SeatRowsPerCar; ++row)
        {
            const float z = static_cast<float>(Sim::TrainInterior::seatRowZ(car, row));
            const bool forward = row % 2 == 0;
            for (int side : {-1, 1})
            {
                if (!Sim::TrainInterior::seatSidePresent(car, row, side)) continue;
                const bool hasInnerSeat = Sim::TrainInterior::innerSeatPresent(car, row);
                const Color& upholstery = carSeatColors[static_cast<std::size_t>(car)];
                const Color& upholsterySeam = carSeatSeams[static_cast<std::size_t>(car)];
                const float backZ = z + (forward ? 0.28f : -0.38f);
                const std::array<float, 2> seatCentres = {
                    1.04f * static_cast<float>(side),
                    0.58f * static_cast<float>(side)};
                const int seatCount = hasInnerSeat ? 2 : 1;
                const float shadowInner = (hasInnerSeat ? 0.33f : 0.70f)
                                        * static_cast<float>(side);
                const float shadowOuter = 1.31f*static_cast<float>(side);
                addSoftContactShadow(interiorShadows,(shadowInner+shadowOuter)*0.5f,0.104f,z,
                                     (std::abs(shadowOuter-shadowInner)+0.12f)*0.5f,0.43f,48);
                for (int seatIndex = 0; seatIndex < seatCount; ++seatIndex)
                {
                    const float seatX = seatCentres[static_cast<std::size_t>(seatIndex)];
                    const float xmin = seatX - 0.225f;
                    const float xmax = seatX + 0.225f;
                    addSeatUpholsteryBody(upholsteryMaterial,seatX,z,backZ,upholstery);
                    ++roundedSeatCushionCount_;
                    ++roundedSeatBackCount_;
                    // The replaceable antimacassar belongs only on the passenger-facing side.
                    // A separate rear shell, grab recess and literature pocket stop every row
                    // from looking like the same pale slab when viewed while walking the aisle.
                    const float frontSurface = forward ? backZ-0.014f : backZ+0.114f;
                    const float rearSurface = forward ? backZ+0.114f : backZ-0.014f;
                    const float frontDepth = forward ? -0.012f : 0.012f;
                    // The shaped inset uses the same physical weave coordinates as the body.
                    const float fabricSurface = frontSurface+frontDepth*1.25f;
                    const auto fabricBegin=upholsteryMaterial.data().vertices.size();
                    addShapedSeatFabric(upholsteryMaterial,xmin+0.040f,xmax-0.040f,0.69f,1.29f,
                                       fabricSurface,!forward,upholstery);
                    upholsteryMaterial.projectDetailUVs(fabricBegin,{xmin,0.42f,z-0.35f},0.16f);
                    addSeatHeadCover(mesh,seatX,backZ,forward,
                                     carHeadrests[static_cast<std::size_t>(car)]);
                    const float seamSurface = frontSurface+frontDepth*1.7f;
                    // Restrained 4 mm piping, not the previous 18–30 mm contrasting bands.
                    // Keep the same batches and silhouettes: this is a textile-scale correction.
                    addSeatPanel(xmin+0.036f,xmin+0.040f,0.68f,1.28f,
                                 seamSurface,!forward,upholsterySeam);
                    addSeatPanel(xmax-0.040f,xmax-0.036f,0.68f,1.28f,
                                 seamSurface,!forward,upholsterySeam);
                    addSeatPanel(xmin+0.040f,xmax-0.040f,1.258f,1.262f,
                                 seamSurface,!forward,upholsterySeam);
                    addSeatCushionWelt(mesh,seatX,z,upholsterySeam);
                    addSeatRearShell(interiorPanelMaterial,seatX,rearSurface,forward ? 1.0f : -1.0f,
                                     seatBackShell);
                    // A slim offset pedestal replaces the pair of broad rectangular side plates.
                    // The support remains inside the old footprint and below the authoritative
                    // cushion collision, but reads as modern railway furniture from the aisle.
                    const float pedestalZ = z+(forward ? 0.10f : -0.10f);
                    addSoftContactShadow(interiorShadows,seatX,0.1048f,pedestalZ,0.16f,0.16f,105);
                    addSeatPedestal(mesh,seatX,pedestalZ,seatFrame);
                    seatMountFastenerCount_ += 2;
                    mesh.addRoundedBoxXZ(Vector3{seatX-0.18f,0.39f,z-0.21f},
                                         Vector3{seatX+0.18f,0.44f,z+0.21f},
                                         0.035f,2,seatFrame);
                    ++seatPedestalCount_;
                }

                // Low aisle armrest, mirrored per side. It stays outside the collision-tested
                // 0.84 m clear aisle and gives the seat rows a more plausible silhouette.
                const float aisleEdge = (hasInnerSeat ? 0.34f : 0.72f)
                                      * static_cast<float>(side);
                mesh.addCylinder(Vector3{aisleEdge,0.59f,z},0.027f,0.31f,8,seatFrame);
                addPassengerArmrest(mesh,interiorResilientMaterial,aisleEdge,z);
                ++roundedAisleArmrestCount_;
            }
        }

        // Facing pairs receive real window tables rather than another decorative decal. The
        // shared TrainInterior layout also supplies their collision footprint, so neither the
        // player nor visual-QA camera can pass through a tabletop. The accessible companion bay
        // and the toilet-replaced side deliberately remain clear.
        const Color tableTop = car == 1 ? Color(181,190,190,255)
                             : car == 3 ? Color(199,193,169,255)
                                        : Color(190,194,187,255);
        for (int table = 0; table < Sim::TrainInterior::TableBaysPerCar; ++table)
        {
            const float z = static_cast<float>(Sim::TrainInterior::tableBayZ(car,table));
            for (int side : {-1,1})
            {
                if (!Sim::TrainInterior::tableSidePresent(car,table,side)) continue;
                const float direction = static_cast<float>(side);
                const float inner = direction*0.48f;
                const float outer = direction*1.36f;
                const float xmin = std::min(inner,outer);
                const float xmax = std::max(inner,outer);
                addPassengerTableTop(interiorPanelMaterial,(xmin+xmax)*0.5f,z,tableTop);
                ++roundedTableTopCount_;
                mesh.addCylinder(Vector3{direction*0.66f,0.08f,z},0.13f,0.025f,12,
                                 Color(103,110,108,255));
                // The narrow pedestal previously met the resilient floor without any local
                // occlusion, which made the table appear to hover in bright daylight. Keep this
                // footprint tight around the base plate: the broad seat shadows already model
                // the softer ambient occlusion beneath the surrounding facing bays.
                addSoftContactShadow(interiorShadows,direction*0.66f,0.1045f,z,0.19f,0.19f,105);
                for (float boltZ : {-0.070f,0.070f})
                    mesh.addCylinder(Vector3{direction*0.66f,0.105f,z+boltZ},
                                     0.013f,0.009f,6,Color(43,48,48,255));
                ++tableSupportPlateCount_;
                mesh.addCylinder(Vector3{direction*0.66f,0.08f,z},0.045f,0.71f,10,
                                 Color(119,125,123,255));
                mesh.addBox(Vector3{std::min(direction*1.41f,direction*1.31f),0.67f,z-0.16f},
                            Vector3{std::max(direction*1.41f,direction*1.31f),0.78f,z+0.16f},
                            Color(116,123,122,255));
                // A small twin power outlet is visible beneath the window-side edge.
                mesh.addBox(Vector3{std::min(direction*1.435f,direction*1.405f),0.58f,z-0.11f},
                            Vector3{std::max(direction*1.435f,direction*1.405f),0.72f,z+0.11f},
                            Color(45,61,66,255));
                for (float socketZ : {-0.055f,0.055f})
                    mesh.addBox(Vector3{std::min(direction*1.442f,direction*1.434f),0.62f,
                                        z+socketZ-0.022f},
                                Vector3{std::max(direction*1.442f,direction*1.434f),0.68f,
                                        z+socketZ+0.022f},Color(218,190,78,255));

                if (car==1)
                {
                    const float powerX=direction*0.56f;
                    mesh.addRoundedBoxXZ({powerX-0.027f,0.861f,z-0.073f},
                        {powerX+0.027f,0.865f,z+0.073f},0.009f,3,Color(48,62,68,255));
                    for (float portZ : {z-0.035f,z+0.035f})
                    {
                        mesh.addRoundedBoxXZ({powerX-0.008f,0.865f,portZ-0.006f},
                            {powerX+0.008f,0.867f,portZ+0.006f},0.003f,3,Color(157,170,173,255));
                        mesh.addBox({powerX-0.006f,0.867f,portZ-0.0025f},
                            {powerX+0.006f,0.8675f,portZ+0.0025f},Color(19,29,34,255));
                    }
                }

                if (car == 0 && table == 0 && side < 0)
                {
                    // A deliberately analogue quiet-zone cue: a slim open book lies flat without
                    // blocking either cup recess or any of the four facing seats.
                    mesh.addBox(Vector3{-1.14f,0.862f,z-0.20f},
                                Vector3{-0.90f,0.875f,z+0.18f},Color(43,69,83,255));
                    mesh.addBox(Vector3{-1.12f,0.876f,z-0.18f},
                                Vector3{-0.91f,0.883f,z-0.01f},Color(226,222,204,255));
                    mesh.addBox(Vector3{-1.12f,0.876f,z+0.01f},
                                Vector3{-0.91f,0.883f,z+0.16f},Color(226,222,204,255));
                }
                else if (car == 1 && table == 1)
                {
                    // The work car gets two open laptops at its central table. Their compact
                    // silhouette, dark screen and keyboard make the power provision meaningful.
                    const float laptopX = direction*0.94f;
                    mesh.addBox(Vector3{laptopX-0.21f,0.862f,z-0.22f},
                                Vector3{laptopX+0.21f,0.878f,z+0.08f},Color(57,63,65,255));
                    mesh.addBox(Vector3{laptopX-0.22f,0.88f,z+0.07f},
                                Vector3{laptopX+0.22f,1.16f,z+0.095f},Color(50,57,60,255));
                    mesh.addBox(Vector3{laptopX-0.19f,0.91f,z+0.064f},
                                Vector3{laptopX+0.19f,1.13f,z+0.068f},Color(62,111,128,255));
                    for (int key = 0; key < 5; ++key)
                    {
                        const float keyX = laptopX-0.16f+static_cast<float>(key)*0.08f;
                        mesh.addBox(Vector3{keyX-0.025f,0.879f,z-0.13f},
                                    Vector3{keyX+0.025f,0.884f,z-0.08f},Color(112,120,121,255));
                    }
                }
                else if (car == 3 && table == 1 && side > 0)
                {
                    addFamilyActivityBook(mesh,z);
                }
            }
        }
    }

    // High-contrast floor zoning makes the widened accessible seating bay readable at a glance.
    mesh.addBox(Vector3{-0.76f,0.081f,-18.25f}, Vector3{0.76f,0.095f,-14.70f},
                Color(44,112,139,255));
    for (float x : {-0.575f,0.575f})
        addCarRoleIcon(mesh,2,Matrix::CreateScale(2.35f)*Matrix::CreateRotationX(-kPi*0.5f)
            *Matrix::CreateTranslation(x,0.104f,-16.90f),Color(204,220,218,255));

    // Double-ended cab shells continue the body beyond the locked passenger bulkheads. They are
    // deliberately generic rather than reproducing a trademarked train, and remain inaccessible.
    for (float direction : {-1.0f, 1.0f})
    {
        const float inner = direction * 63.3f;
        const float nose = direction * 72.8f;
        const float start = std::min(inner, nose);
        const float end = std::max(inner, nose);
        const float windscreenZ0 = nose - direction * 0.08f;
        const float windscreenZ1 = nose + direction * 0.08f;
        mesh.addBox(Vector3{-1.48f,-0.08f,start}, Vector3{1.48f,0.08f,end}, floor);
        mesh.addBox(Vector3{-1.42f,2.92f,start}, Vector3{1.42f,3.08f,end}, shell);
        for (float side : {-1.0f, 1.0f})
        {
            const float x0 = side < 0.0f ? -1.50f : 1.42f;
            const float x1 = side < 0.0f ? -1.42f : 1.50f;
            mesh.addBox(Vector3{x0,2.55f,start}, Vector3{x1,3.0f,end}, shell);
            addCabBodyside(mesh,glass,side,direction,inner,nose,shell,trim);
        }
        const float noseMin = std::min(windscreenZ0, windscreenZ1);
        const float noseMax = std::max(windscreenZ0, windscreenZ1);
        mesh.addBox(Vector3{-1.46f,0.0f,noseMin}, Vector3{1.46f,1.35f,noseMax}, trim);
        mesh.addBox(Vector3{-1.46f,2.72f,noseMin}, Vector3{1.46f,3.0f,noseMax}, shell);
        mesh.addBox(Vector3{-1.46f,1.32f,noseMin}, Vector3{-1.08f,2.75f,noseMax}, shell);
        mesh.addBox(Vector3{1.08f,1.32f,noseMin}, Vector3{1.46f,2.75f,noseMax}, shell);
        glass.addBox(Vector3{-1.06f,1.42f,noseMin}, Vector3{1.06f,2.68f,noseMax},
                     Color(50,103,126,64));
        addCabExterior(mesh,interiorEmitters,direction,nose,trim);
        const float deskZ0 = nose - direction * 1.2f;
        const float deskZ1 = nose - direction * 0.45f;
        mesh.addBox(Vector3{-1.08f,1.02f,std::min(deskZ0,deskZ1)},
                    Vector3{1.08f,1.28f,std::max(deskZ0,deskZ1)}, Color(50,57,59,255));

        // A low-profile crashworthy desk, two physical display bezels, controls and a power/brake
        // handle replace the old empty shelf. Luminous contents are drawn only in the active cab,
        // so direction reversal remains visible without rebuilding this immutable mesh.
        const float screenZ = nose-direction*1.21f;
        const float screenZ0 = std::min(screenZ-direction*0.040f,
                                        screenZ+direction*0.040f);
        const float screenZ1 = std::max(screenZ-direction*0.040f,
                                        screenZ+direction*0.040f);
        mesh.addRoundedBoxXY(Vector3{-0.63f,1.30f,screenZ0},
                             Vector3{0.59f,1.72f,screenZ1},0.055f,3,
                             Color(27,34,37,255));
        mesh.addBox(Vector3{-1.03f,1.255f,std::min(screenZ-direction*0.42f,screenZ)},
                    Vector3{1.03f,1.34f,std::max(screenZ-direction*0.42f,screenZ)},
                    Color(70,78,78,255));
        const float buttonZ = screenZ-direction*0.25f;
        for (int button = 0; button < 6; ++button)
        {
            const float x = -0.62f+static_cast<float>(button)*0.245f;
            const float z0 = std::min(buttonZ-direction*0.035f,
                                      buttonZ+direction*0.035f);
            const float z1 = std::max(buttonZ-direction*0.035f,
                                      buttonZ+direction*0.035f);
            mesh.addBox(Vector3{x-0.040f,1.34f,z0},Vector3{x+0.040f,1.365f,z1},
                        button == 0 ? Color(205,57,45,255)
                        : button == 5 ? Color(231,183,47,255)
                                      : Color(126,141,141,255));
        }
        const float leverZ = screenZ-direction*0.34f;
        mesh.addCylinder(Vector3{0.78f,1.30f,leverZ},0.035f,0.19f,8,
                         Color(126,132,130,255));
        mesh.addEllipsoid(Vector3{0.78f,1.52f,leverZ},Vector3{0.070f,0.055f,0.070f},
                          10,5,Color(40,47,49,255));

        // Two restrained interior wipers break up the broad windscreen and remain visible from
        // the staff-door observation pane. Their placement follows each cab's operating end.
        for (float x : {-0.42f,0.42f})
        {
            const Matrix wiper = Matrix::CreateRotationZ(x < 0.0f ? -0.49f : 0.49f)
                * Matrix::CreateTranslation(x,2.05f,nose-direction*0.095f);
            mesh.addBox(Vector3{-0.31f,-0.018f,-0.012f},
                        Vector3{0.31f,0.018f,0.012f},Color(20,25,26,255),wiper);
            mesh.addBox(Vector3{-0.025f,-0.10f,-0.018f},
                        Vector3{0.025f,0.10f,0.018f},Color(25,30,31,255),
                        Matrix::CreateTranslation(x,1.79f,nose-direction*0.096f));
        }
        ++cabControlGroupCount_;
        const float seatZ0 = nose - direction * 2.4f;
        const float seatZ1 = nose - direction * 1.75f;
        mesh.addBox(Vector3{-0.42f,0.18f,std::min(seatZ0,seatZ1)},
                    Vector3{0.42f,0.86f,std::max(seatZ0,seatZ1)}, Color(42,58,68,255));
        mesh.addBox(Vector3{-1.48f,0.43f,start}, Vector3{1.48f,0.60f,end},
                    Color(230,181,40,255));
    }

    // Gangways: a short ribbed bellows tunnel, bridge plate and passenger-scale fittings make the
    // connection read as flexible railway hardware rather than a flat black hole. All five join
    // the immutable interior batch, so this detail adds no draw call.
    for (int boundary = 1; boundary < 6; ++boundary)
    {
        const float z = -63.0f + static_cast<float>(boundary) * kCarLength;
        const Color endWall(48,57,61,255);
        const Color brushedMetal(164,168,163,255);
        mesh.addBox(Vector3{-1.47f,0.0f,z-0.24f},
                    Vector3{-0.62f,2.92f,z+0.24f},endWall);
        mesh.addBox(Vector3{0.62f,0.0f,z-0.24f},
                    Vector3{1.47f,2.92f,z+0.24f},endWall);
        addExteriorBellows(mesh,z);
        // The structural header must clear the complete corrugated skin (top 2.416 m).
        mesh.addBox(Vector3{-0.62f,2.43f,z-0.24f},
                    Vector3{0.62f,2.92f,z+0.24f},endWall);
        // Finish the previously bare reverse face too; the opposite side already carries
        // the car's safety-equipment service panels. Header skins stay behind the sign.
        for (float side : {-1.0f,1.0f})
            addVestibuleServicePanels(interiorPanelMaterial,side,z+0.315f,1);
        for (float face : {-1.0f,1.0f})
        {
            const auto first=interiorPanelMaterial.data().vertices.size();
            const float front=z+face*0.250f,back=z+face*0.240f;
            interiorPanelMaterial.addRoundedBoxXY(
                {-1.415f,2.44f,std::min(front,back)},
                {1.415f,2.895f,std::max(front,back)},0.025f,3,Color(143,156,157,255));
            interiorPanelMaterial.projectXYDetailUVs(first,{0,2.44f,z},0.23f);
        }

        // Continuous rounded rubber folds, not overlapping rigid rectangular frames.
        addFlexibleGangwayBellows(interiorResilientMaterial,z);

        // A slightly crowned metal bridge closes the half-metre structural gap between the two
        // floor slabs. Its inset dark strips stay below the collision plane and provide speed-
        // independent visual footing through the doorway.
        addGangwayBridgePlate(mesh,z);
        ++nonCarpetedFloorZoneCount_;

        // Full-height grab rails have visible wall mounts. A warm header lamp and an illuminated
        // label keep the threshold readable even while the scenery is inside a tunnel.
        for (float x : {-0.46f,0.46f})
        {
            mesh.addCylinder(Vector3{x,0.12f,z},0.025f,2.13f,8,brushedMetal);
            addGrabRailEnds(mesh,x,z,0.12f,2.25f,0.025f,brushedMetal);
            for (float y : {0.31f,2.03f})
            {
                const float x0 = x < 0.0f ? x-0.12f : x;
                const float x1 = x < 0.0f ? x : x+0.12f;
                mesh.addBox(Vector3{x0,y-0.025f,z-0.035f},
                            Vector3{x1,y+0.025f,z+0.035f},brushedMetal);
            }
        }
        mesh.addBox(Vector3{-0.27f,2.305f,z-0.16f},
                    Vector3{0.27f,2.345f,z+0.16f},Color(117,129,132,255));
        interiorEmitters.addQuad(Vector3{-0.245f,2.302f,z-0.135f},
            Vector3{0.245f,2.302f,z-0.135f},Vector3{0.245f,2.302f,z+0.135f},
            Vector3{-0.245f,2.302f,z+0.135f},Color(246,241,218,255));
        mesh.addBox(Vector3{-0.42f,2.47f,z-0.255f},
                    Vector3{0.42f,2.73f,z+0.255f},Color(24,39,45,255));
        for (float face : {-1.0f,1.0f})
        {
            const float surface = z + face*0.262f;
            addPrintedPlaqueText(interiorLettering,"GANGWAY",2.67f,surface,face < 0.0f,
                                 Color(196,226,224,255),0.0f,0.105f,0.70f);
        }
    }

    // Staff-only cab bulkheads make the passenger boundary physical and legible.
    for (float z : {-63.2f, 63.2f})
    {
        // Side panels and the locked centre door leave a glazed observation window. Collision is
        // still governed by the passenger-space boundary, so the opening never grants cab access.
        mesh.addBox(Vector3{-1.48f,0.0f,z-0.10f},
                    Vector3{-0.54f,3.0f,z+0.10f}, Color(56,69,76,255));
        mesh.addBox(Vector3{0.54f,0.0f,z-0.10f},
                    Vector3{1.48f,3.0f,z+0.10f}, Color(56,69,76,255));
        // The leaf sits 15 mm behind the bulkhead face inside a proud frame; hinges, kick
        // plate and lever hardware come from InteriorFittingsGeometry.hpp.
        mesh.addBox(Vector3{-0.54f,0.0f,z-0.085f},
                    Vector3{0.54f,1.22f,z+0.085f}, Color(62,77,84,255));
        mesh.addBox(Vector3{-0.54f,2.38f,z-0.085f},
                    Vector3{0.54f,2.42f,z+0.085f}, Color(62,77,84,255));
        mesh.addBox(Vector3{-0.60f,2.42f,z-0.10f},
                    Vector3{0.60f,3.0f,z+0.10f}, Color(56,69,76,255));
        glass.addBox(Vector3{-0.48f,1.26f,z-0.10f},
                     Vector3{0.48f,2.38f,z+0.10f}, Color(86,143,161,42));
        addCabDoorHardware(mesh,z,z < 0.0f ? 1.0f : -1.0f);
        mesh.addBox(Vector3{-0.62f,0.72f,z-0.12f},
                    Vector3{0.62f,1.12f,z+0.12f}, Color(205,59,45,255));
        const bool mirrored = z > 0.0f;
        const float surface = z + (z < 0.0f ? 0.125f : -0.125f);
        mesh.addBox(Vector3{-0.56f,0.77f,surface-0.012f},
                    Vector3{0.56f,1.08f,surface+0.012f}, Color(18,24,27,255));
        addPrintedPlaqueText(interiorLettering,"STAFF ONLY",1.02f,
                             surface + (z < 0.0f ? 0.014f : -0.014f), mirrored,
                             Color(248,202,48,255),0.0f,0.115f,0.98f);
    }

    // Accessible toilet in the fourth car: three walls, sanitary fixtures and a hinged aisle door.
    mesh.addBox(Vector3{0.34f,0.0f,3.45f}, Vector3{1.46f,2.70f,3.55f}, Color(204,211,208,255));
    mesh.addBox(Vector3{0.34f,0.0f,4.59f}, Vector3{0.44f,2.70f,8.10f}, Color(204,211,208,255));
    mesh.addBox(Vector3{1.35f,0.0f,3.45f}, Vector3{1.46f,2.70f,8.10f}, Color(204,211,208,255));
    mesh.addBox(Vector3{0.34f,0.0f,8.00f}, Vector3{1.46f,2.70f,8.10f}, Color(204,211,208,255));
    mesh.addBox(Vector3{0.45f,0.081f,4.60f}, Vector3{1.34f,0.098f,7.98f},
                Color(57,92,102,255));
    addResilientFloor(0.455f,1.335f,0.099f,4.605f,7.975f,
                      Color(64,101,110,255));
    ++nonCarpetedFloorZoneCount_;

    // A wipe-clean lower wall band, coved skirting and panel joints give the compartment its own
    // material language instead of inheriting the saloon's flat grey shell. These pieces remain in
    // the shared interior batch and sit just inside the collision walls.
    const Color toiletWallAccent(55,118,132,255);
    const Color toiletWallJoint(177,188,187,255);
    mesh.addBox(Vector3{0.445f,0.10f,4.62f},Vector3{0.457f,0.72f,7.96f},
                toiletWallAccent);
    mesh.addBox(Vector3{1.333f,0.10f,4.62f},Vector3{1.345f,0.72f,7.96f},
                toiletWallAccent);
    mesh.addBox(Vector3{0.46f,0.10f,7.975f},Vector3{1.33f,0.72f,7.992f},
                toiletWallAccent);
    mesh.addBox(Vector3{0.445f,0.10f,4.62f},Vector3{0.475f,0.20f,7.96f},
                Color(39,77,86,255));
    mesh.addBox(Vector3{1.315f,0.10f,4.62f},Vector3{1.345f,0.20f,7.96f},
                Color(39,77,86,255));
    mesh.addBox(Vector3{0.46f,0.10f,7.955f},Vector3{1.33f,0.20f,7.992f},
                Color(39,77,86,255));
    for (float panelZ : {5.55f,6.68f})
    {
        mesh.addBox(Vector3{0.445f,0.72f,panelZ-0.014f},
                    Vector3{0.459f,2.56f,panelZ+0.014f},toiletWallJoint);
        mesh.addBox(Vector3{1.331f,0.72f,panelZ-0.014f},
                    Vector3{1.345f,2.56f,panelZ+0.014f},toiletWallJoint);
    }
    mesh.addBox(Vector3{0.445f,1.42f,4.62f},Vector3{0.460f,1.57f,7.96f},
                Color(63,137,151,255));
    mesh.addBox(Vector3{1.330f,1.42f,4.62f},Vector3{1.345f,1.57f,7.96f},
                Color(63,137,151,255));
    mesh.addBox(Vector3{0.46f,1.42f,7.964f},Vector3{1.33f,1.57f,7.992f},
                Color(63,137,151,255));
    addWallPanel(0.444f,0.74f,2.55f,4.62f,7.96f,true,
                 Color(207,216,214,255));
    addWallPanel(1.347f,0.74f,2.55f,4.62f,7.96f,false,
                 Color(207,216,214,255));
    // Fine anti-slip lanes break up the blue floor without creating a raised trip edge.
    for (float z = 4.88f; z < 7.82f; z += 0.42f)
        mesh.addBox(Vector3{0.56f,0.098f,z},Vector3{1.23f,0.102f,z+0.035f},
                    Color(94,131,139,255));

    // Sanitary fittings come from ToiletGeometry.hpp: compact wall-hung toilet with seat, lid,
    // cistern cover and flush button, tubular support rail, paper dispenser, floor coves, vanity
    // with sensor spout and strainer, gradient mirror, soap/towel dispensers and the threshold.
    addTrainToilet(mesh);
    addToiletSupportRail(mesh);
    addPaperDispenser(mesh);
    addToiletFloorCove(mesh);
    addSinkVanity(mesh);
    addSanitaryBowl(mesh,Vector3{1.04f,0.89f,4.25f},0.25f,0.21f,0.15f);
    addToiletMirror(mesh);
    addWashroomDispensers(mesh);
    addToiletThreshold(mesh);
    // Alarm call point and waste bin remain as before.
    mesh.addBox(Vector3{0.410f,1.17f,5.43f}, Vector3{0.470f,1.43f,5.67f},
                Color(205,51,45,255));
    mesh.addBox(Vector3{0.471f,1.245f,5.495f}, Vector3{0.481f,1.355f,5.605f},
                Color(248,213,63,255));
    mesh.addBox(Vector3{0.48f,0.09f,5.02f}, Vector3{0.73f,0.58f,5.48f},
                Color(118,130,132,255));
    mesh.addBox(Vector3{0.50f,0.56f,5.06f}, Vector3{0.71f,0.59f,5.44f},
                Color(47,57,60,255));

    // A red emergency pull cord and triangular handle are deliberately prominent against the
    // pale wall. A small high-level ventilation grille adds another familiar railway detail.
    mesh.addBox(Vector3{0.486f,1.13f,5.93f},Vector3{0.504f,2.52f,5.95f},
                Color(190,47,42,255));
    mesh.addBox(Vector3{0.465f,1.08f,5.89f},Vector3{0.525f,1.14f,5.99f},
                Color(235,75,55,255));
    mesh.addBox(Vector3{0.55f,2.15f,7.965f},Vector3{0.94f,2.43f,7.985f},
                Color(80,99,103,255));
    for (float x = 0.59f; x <= 0.90f; x += 0.075f)
        mesh.addBox(Vector3{x,2.18f,7.957f},Vector3{x+0.025f,2.40f,7.991f},
                    Color(178,188,187,255));

    // The WC diffuser shares the unlit saloon batch; its metal frame remains independently lit.
    mesh.addBox(Vector3{0.58f,2.61f,5.20f},Vector3{1.22f,2.68f,6.30f},
                Color(157,169,171,255));
    interiorEmitters.addQuad(Vector3{0.605f,2.607f,5.225f},
        Vector3{1.195f,2.607f,5.225f},Vector3{1.195f,2.607f,6.275f},
        Vector3{0.605f,2.607f,6.275f},Color(246,241,218,255));
    mesh.addBox(Vector3{0.42f,1.72f,3.425f}, Vector3{1.38f,2.22f,3.445f},
                Color(25,74,88,255));
    addPrintedPlaqueText(interiorLettering,"ACCESSIBLE",2.15f,3.416f,true,
                         Color(229,240,237,255),0.90f,0.085f,0.84f);
    addPrintedPlaqueText(interiorLettering,"WC",1.98f,3.416f,true,
                         Color(247,194,48,255),0.90f,0.15f,0.84f);
    mesh.addBox(Vector3{0.42f,1.72f,3.555f}, Vector3{1.38f,2.22f,3.575f},
                Color(25,74,88,255));
    addPrintedPlaqueText(interiorLettering,"ACCESSIBLE",2.15f,3.584f,false,
                         Color(229,240,237,255),0.90f,0.085f,0.84f);
    addPrintedPlaqueText(interiorLettering,"WC",1.98f,3.584f,false,
                         Color(247,194,48,255),0.90f,0.15f,0.84f);
    mesh.addBox(Vector3{0.42f,1.72f,8.105f}, Vector3{1.38f,2.22f,8.125f},
                Color(25,74,88,255));
    addPrintedPlaqueText(interiorLettering,"ACCESSIBLE",2.15f,8.134f,false,
                         Color(229,240,237,255),0.90f,0.085f,0.84f);
    addPrintedPlaqueText(interiorLettering,"WC",1.98f,8.134f,false,
                         Color(247,194,48,255),0.90f,0.15f,0.84f);

    // Vestibule luggage stacks and grab poles.
    const std::array<float, 6> luggagePositions = {-61.0f, -40.0f, -19.0f, 23.0f, 44.0f, 61.0f};
    const std::array<Color, 6> luggageColors = {
        Color(137,72,54,255), Color(53,92,125,255), Color(132,111,55,255),
        Color(72,105,82,255), Color(108,69,118,255), Color(143,84,48,255)};
    for (std::size_t index = 0; index < luggagePositions.size(); ++index)
    {
        const float z = luggagePositions[index];
        // The rack and lower suitcase share one restrained floor contact patch. It is long
        // enough to anchor the full rack footprint but stays behind the clear aisle edge.
        addOrientedGroundQuad(
            interiorShadows,-1.105f,0.1045f,z,
            0.62f,1.48f,0.0f,Color(7,11,12,30));
        if (index == 3U)
            mesh.addBox(Vector3{-1.39f,0.08f,z-0.75f},Vector3{-1.36f,2.10f,z+0.75f},Color(87,94,97,255));
        else
            mesh.addBox(Vector3{-1.32f,0.08f,z-0.75f}, Vector3{-0.87f,1.55f,z+0.75f}, Color(87,94,97,255));
        const Color& caseColor = luggageColors[index];
        mesh.addRoundedBoxYZ(Vector3{-1.35f,0.12f,z-0.62f},
                             Vector3{-0.82f,0.72f,z-0.05f},0.060f,3,caseColor);
        mesh.addRoundedBoxYZ(Vector3{-1.31f,0.77f,z+0.02f},
                             Vector3{-0.84f,1.29f,z+0.57f},0.055f,3,
                             luggageColors[(index+2)%luggageColors.size()]);
        floorLuggageCaseCount_ += 2;

        // Aisle-facing zipper bands, grounded caster wheels and real U-shaped pull handles break
        // up the case silhouettes. They stay behind x=-0.77 m, safely outside the 0.84 m aisle.
        mesh.addBox(Vector3{-0.819f,0.39f,z-0.55f},
                    Vector3{-0.803f,0.43f,z-0.12f},Color(55,59,59,255));
        mesh.addBox(Vector3{-0.839f,1.00f,z+0.08f},
                    Vector3{-0.823f,1.04f,z+0.51f},Color(55,59,59,255));
        for (float wheelZ : {z-0.52f,z-0.15f})
            mesh.addEllipsoid(Vector3{-0.79f,0.115f,wheelZ},Vector3{0.026f,0.045f,0.041f},
                              8,4,Color(38,43,44,255));
        ++floorLuggageWheelPairCount_;
        for (float handleZ : {z-0.47f,z-0.20f})
            mesh.addBox(Vector3{-0.817f,0.68f,handleZ-0.012f},
                        Vector3{-0.795f,0.88f,handleZ+0.012f},Color(64,68,68,255));
        mesh.addBox(Vector3{-0.817f,0.86f,z-0.47f},
                    Vector3{-0.795f,0.90f,z-0.20f},Color(55,59,59,255));
        for (float handleZ : {z+0.15f,z+0.44f})
            mesh.addBox(Vector3{-0.837f,1.25f,handleZ-0.012f},
                        Vector3{-0.815f,1.43f,handleZ+0.012f},Color(64,68,68,255));
        mesh.addBox(Vector3{-0.837f,1.41f,z+0.15f},
                    Vector3{-0.815f,1.45f,z+0.44f},Color(55,59,59,255));
        if (index == 3U) // z=23 is Car 5; the luggage array omits the toilet vestibule.
        {
            for (float strapZ : {z-0.30f,z+0.30f})
            {
                mesh.addBox({-0.785f,0.18f,strapZ-0.014f},{-0.777f,2.0f,strapZ+0.014f},
                            Color(33,48,45,255));
                mesh.addBox({-0.790f,1.08f,strapZ-0.022f},{-0.770f,1.14f,strapZ+0.022f},
                            Color(124,141,140,255));
            }
            // Car 5 owns a full-height vestibule rack rather than the generic two-case stack.
            // Shelves and lips hold two additional cabin bags entirely outside the clear aisle.
            for (float shelfY : {0.76f,1.40f,2.05f})
                mesh.addBox(Vector3{-1.39f,shelfY,z-0.72f},
                            Vector3{-0.79f,shelfY+0.045f,z+0.72f},Color(96,103,104,255));
            for (float railZ : {z-0.72f,z+0.72f})
                mesh.addBox(Vector3{-1.39f,0.08f,railZ-0.025f},
                            Vector3{-1.32f,2.10f,railZ+0.025f},Color(96,103,104,255));
            mesh.addRoundedBoxYZ(Vector3{-1.34f,1.445f,z-0.61f},
                                 Vector3{-0.85f,1.98f,z-0.08f},0.052f,3,
                                 Color(55,96,124,255));
            mesh.addRoundedBoxYZ(Vector3{-1.31f,1.445f,z+0.06f},
                                 Vector3{-0.87f,1.94f,z+0.58f},0.050f,3,
                                 Color(132,105,54,255));
            floorLuggageCaseCount_ += 2;
            mesh.addBox(Vector3{-1.18f,1.92f,z-0.43f},
                        Vector3{-1.01f,2.04f,z-0.26f},Color(55,59,59,255));
            mesh.addBox(Vector3{-1.17f,1.88f,z+0.22f},
                        Vector3{-1.00f,2.00f,z+0.39f},Color(55,59,59,255));
        }
        mesh.addCylinder(Vector3{0.78f,0.05f,z}, 0.028f, 2.35f, 8, Color(226,185,48,255));
    }

    interior_ = std::make_unique<GpuMesh>(device_, mesh.take());
    interiorMaterial_ = std::make_unique<GpuMesh>(device_,interiorMaterial.take());
    upholsteryMaterial_ = std::make_unique<GpuMesh>(device_,upholsteryMaterial.take());
    interiorPanelMaterial_ = std::make_unique<GpuMesh>(
        device_,interiorPanelMaterial.take());
    interiorResilientMaterial_ = std::make_unique<GpuMesh>(
        device_,interiorResilientMaterial.take());
    interiorShadows_ = std::make_unique<GpuMesh>(device_,interiorShadows.take());
    interiorLightGradients_ = std::make_unique<GpuMesh>(device_,interiorLighting.take());
    interiorEmitters_ = std::make_unique<GpuMesh>(device_,interiorEmitters.take());
    interiorLettering_ = std::make_unique<GpuMesh>(device_,interiorLettering.take());
    windowGlass_ = std::make_unique<GpuMesh>(device_, glass.take());
    if (!rainDroplets.data().empty())
        rainDroplets_ = std::make_unique<GpuMesh>(device_, rainDroplets.take());
    for (std::size_t side = 0; side < windowReflections_.size(); ++side)
        windowReflections_[side] = std::make_unique<GpuMesh>(
            device_,glassReflections[side].take());
    sunlightPatches_ = std::make_unique<GpuMesh>(device_, sunlight.take());
}


} // namespace TrainSim::Render
