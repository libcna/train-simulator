#include "SceneShared.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::GamerServices;

namespace TrainSim::Render {

void TrackGeometry::build(const Sim::Route& route, double anchorDistance, int graphicsQuality)
{
    const auto generationStart = std::chrono::steady_clock::now();
    anchor = anchorDistance;
    quality = graphicsQuality;
    constexpr double kSegmentLength = 6.0;
    constexpr double kTrackRadius = 1'000.0;
    track.clear();
    trackBedMaterial.clear();
    railHeadMaterial.clear();
    // Stable capacities avoid allocator growth/fragmentation every time the 2 km sleeper and rail
    // batch recentres. Excess capacity is modest compared with the resulting GPU buffers.
    track.reserve(420'000, 650'000);
    trackBedMaterial.reserve(4'000,6'000);
    railHeadMaterial.reserve(6'000,9'000);
    // Rails, sleepers and ballast overrun both route endpoints to the terminus buffer stops.
    const double begin = std::max(-kTerminalTrackOverrunMetres, anchorDistance - kTrackRadius);
    const double end = std::min(route.lengthMetres()+kTerminalTrackOverrunMetres,
                                anchorDistance + kTrackRadius);
    const double fastenerRadius = graphicsQuality == 0 ? 360.0
                                : graphicsQuality == 1 ? 460.0 : 560.0;
    kilometrePosts = 0;
    fastenerAssemblies = 0;
    turnoutPointMotors = 0;
    turnoutBladeAssemblies = 0;
    const auto addTrackTopQuad = [this](const Matrix& placement, float x0, float x1,
                                          float z0, float z1, float y,
                                          const Color& color) {
        track.addQuad(
            Vector3::Transform(Vector3{x0,y,z0},placement),
            Vector3::Transform(Vector3{x0,y,z1},placement),
            Vector3::Transform(Vector3{x1,y,z1},placement),
            Vector3::Transform(Vector3{x1,y,z0},placement),color);
    };
    const auto addRouteRailSegment = [&](double distance0, double lateral0,
                                         double distance1, double lateral1,
                                         float width, float height,
                                         const Color& color) {
        const LocalRoutePoint a = localRoutePoint(
            route,anchorDistance,distance0,lateral0);
        const LocalRoutePoint b = localRoutePoint(
            route,anchorDistance,distance1,lateral1);
        const float dx = b.x-a.x;
        const float dz = b.z-a.z;
        const float length = std::sqrt(dx*dx+dz*dz);
        if (length <= 0.01f) return;
        const float yaw = std::atan2(dx,dz);
        addOrientedBox(track,(a.x+b.x)*0.5f,(a.y+b.y)*0.5f+0.115f,
                       (a.z+b.z)*0.5f,width,height,length,yaw,color);
    };
    for (double segmentBegin = begin; segmentBegin < end; segmentBegin += kSegmentLength)
    {
        const double segmentEnd = std::min(end, segmentBegin + kSegmentLength);
        const double midpoint = (segmentBegin + segmentEnd) * 0.5;
        const float halfLength = static_cast<float>((segmentEnd - segmentBegin) * 0.5 + 0.08);
        const LocalRoutePoint point = localRoutePoint(route, anchorDistance, midpoint);
        const Matrix placement = Matrix::CreateRotationY(point.yaw)
                               * Matrix::CreateTranslation(point.x, point.y, point.z);
        std::array<float, 3> trackCentres = {
            0.0f, static_cast<float>(route.parallelTrackLateralMetres(midpoint)), 0.0f};
        std::size_t trackCentreCount = 2;
        if (const auto siding = stationSidingLateral(route, midpoint);
            siding.has_value() && std::abs(*siding) > 0.30)
            trackCentres[trackCentreCount++] = static_cast<float>(*siding);
        for (std::size_t trackIndex = 0; trackIndex < trackCentreCount; ++trackIndex)
        {
            const float trackCentre = trackCentres[trackIndex];
            // The formation box stays hidden under the crown; both ballast shoulders are sloped
            // textured quads down to a wobbling toe instead of a vertical dark curb.
            track.addBox(Vector3{trackCentre-2.2f,-0.39f,-halfLength},
                         Vector3{trackCentre+2.2f,-0.09f,halfLength},
                         Color(102,96,88,255), placement);
            constexpr float kBallastTileMetres = 0.88f;
            const Color ballastTint(132,124,113,255);
            const auto bedUV = [&](float x,double distance) {
                return Vector2{x/kBallastTileMetres,
                               static_cast<float>(distance)/kBallastTileMetres};
            };
            const auto bedPoint = [&](float x,float y,float z) {
                return Vector3::Transform(Vector3{x,y,z},placement);
            };
            const float crownLeft = trackCentre-kBallastCrownHalfWidth;
            const float crownRight = trackCentre+kBallastCrownHalfWidth;
            trackBedMaterial.addTexturedQuad(
                bedPoint(crownLeft,kBallastCrownHeight,-halfLength),
                bedPoint(crownLeft,kBallastCrownHeight,halfLength),
                bedPoint(crownRight,kBallastCrownHeight,halfLength),
                bedPoint(crownRight,kBallastCrownHeight,-halfLength),ballastTint,
                bedUV(crownLeft,segmentBegin),bedUV(crownLeft,segmentEnd),
                bedUV(crownRight,segmentEnd),bedUV(crownRight,segmentBegin));
            const float toeBegin = ballastShoulderToe(segmentBegin);
            const float toeEnd = ballastShoulderToe(segmentEnd);
            const Color shoulderTint(124,117,107,255);
            trackBedMaterial.addTexturedQuad(
                bedPoint(trackCentre-toeBegin,kBallastToeHeight,-halfLength),
                bedPoint(trackCentre-toeEnd,kBallastToeHeight,halfLength),
                bedPoint(crownLeft,kBallastCrownHeight,halfLength),
                bedPoint(crownLeft,kBallastCrownHeight,-halfLength),shoulderTint,
                bedUV(-toeBegin,segmentBegin),bedUV(-toeEnd,segmentEnd),
                bedUV(crownLeft,segmentEnd),bedUV(crownLeft,segmentBegin));
            trackBedMaterial.addTexturedQuad(
                bedPoint(crownRight,kBallastCrownHeight,-halfLength),
                bedPoint(crownRight,kBallastCrownHeight,halfLength),
                bedPoint(trackCentre+toeEnd,kBallastToeHeight,halfLength),
                bedPoint(trackCentre+toeBegin,kBallastToeHeight,-halfLength),shoulderTint,
                bedUV(crownRight,segmentBegin),bedUV(crownRight,segmentEnd),
                bedUV(toeEnd,segmentEnd),bedUV(toeBegin,segmentBegin));
            for (float railOffset : {-0.81f, 0.71f})
            {
                const float x = trackCentre + railOffset + 0.05f;
                // A rail is not one silver bar: the oxidised foot and slender web carry a broader
                // head, and only the wheel-polished running surface keeps the metallic highlight.
                addRailProfile(track,placement,x,halfLength);
                const Vector3 railA = Vector3::Transform(
                    Vector3{x-kRailHeadHalfWidth,kRailHeadTop+0.001f,-halfLength},placement);
                const Vector3 railB = Vector3::Transform(
                    Vector3{x-kRailHeadHalfWidth,kRailHeadTop+0.001f,halfLength},placement);
                const Vector3 railC = Vector3::Transform(
                    Vector3{x+kRailHeadHalfWidth,kRailHeadTop+0.001f,halfLength},placement);
                const Vector3 railD = Vector3::Transform(
                    Vector3{x+kRailHeadHalfWidth,kRailHeadTop+0.001f,-halfLength},placement);
                railHeadMaterial.addTexturedQuad(
                    railA,railB,railC,railD,Color(166,174,173,255),
                    Vector2{0.0f,0.0f},Vector2{0.0f,1.0f},
                    Vector2{1.0f,1.0f},Vector2{1.0f,0.0f});
            }
            for (float sleeper = -halfLength + 0.32f; sleeper < halfLength; sleeper += 0.72f)
            {
                const Matrix sleeperPlacement = Matrix::CreateTranslation(0.0f, 0.0f, sleeper)
                                                * placement;
                const std::int64_t sleeperIndex = static_cast<std::int64_t>(std::floor(
                    (midpoint+static_cast<double>(sleeper))/0.72));
                const std::array<Color,3> sleeperTones = {
                    Color(127,124,116,255),Color(117,116,111,255),
                    Color(137,132,121,255)};
                const std::size_t tone = static_cast<std::size_t>(std::abs(
                    sleeperIndex+static_cast<std::int64_t>(trackIndex)*17))
                    % sleeperTones.size();
                // Broad dark baseplates and paired rust-toned elastic clips make the rail/sleeper
                // connection legible from a nearby passenger window. Top faces are sufficient at
                // this scale and avoid turning thousands of fasteners into box-sized geometry.
                // Quality-scaled distance limits keep sub-pixel fittings out of the far corridor;
                // at the largest possible 250 m anchor offset Low still retains 110 m ahead.
                // The chamfered monoblock profile shares that radius; farther sleepers stay boxes.
                const double sleeperDistance = midpoint+static_cast<double>(sleeper);
                const bool closeSleeper = std::abs(sleeperDistance-anchorDistance) <= fastenerRadius;
                addConcreteSleeper(track,sleeperPlacement,trackCentre,sleeperTones[tone],
                                   closeSleeper);
                if (!closeSleeper) continue;
                for (float railOffset : {-0.81f,0.71f})
                {
                    const float centre = trackCentre+railOffset+0.05f;
                    addTrackTopQuad(sleeperPlacement,centre-0.15f,centre+0.15f,
                                    -0.115f,0.115f,0.006f,Color(62,66,65,255));
                    addTrackTopQuad(sleeperPlacement,centre-0.145f,centre-0.088f,
                                    -0.064f,0.064f,0.010f,Color(126,87,58,255));
                    addTrackTopQuad(sleeperPlacement,centre+0.088f,centre+0.145f,
                                    -0.064f,0.064f,0.010f,Color(126,87,58,255));
                    ++fastenerAssemblies;
                }
            }
        }
    }

    // Major-station sidings already diverge smoothly from the main line. Explicit point motors,
    // drive rods and a low route indicator make both ends read as operable turnouts rather than
    // as decorative rails that silently merge. Absolute station-relative placement remains stable
    // across 250 m recenter boundaries and everything stays in the shared track submission.
    for (const Sim::Station& station : route.stations())
    {
        if (!station.major) continue;
        const double sidingSide = station.platformOnLeft ? 1.0 : -1.0;
        for (double approach : {-1.0,1.0})
        {
            const double motorDistance = station.distanceMetres+approach*600.0;
            if (motorDistance < begin || motorDistance >= end
                || motorDistance < 0.0 || motorDistance > route.lengthMetres())
                continue;
            const Sim::RoutePose motorPose = route.sample(motorDistance);
            if (motorPose.tunnel || motorPose.bridge) continue;
            const double motorLateral = sidingSide*1.62;
            const LocalRoutePoint pad = localRoutePoint(
                route,anchorDistance,motorDistance,motorLateral);
            addOrientedBox(track,pad.x,pad.y+0.035f,pad.z,
                           0.78f,0.11f,1.08f,pad.yaw,Color(118,117,109,255));
            addOrientedBox(track,pad.x,pad.y+0.19f,pad.z,
                           0.52f,0.24f,0.72f,pad.yaw,Color(69,76,74,255));
            const LocalRoutePoint rod = localRoutePoint(
                route,anchorDistance,motorDistance,sidingSide*0.82);
            addOrientedBox(track,rod.x,rod.y+0.11f,rod.z,
                           1.55f,0.055f,0.10f,rod.yaw,Color(96,82,67,255));
            const LocalRoutePoint indicator = localRoutePoint(
                route,anchorDistance,motorDistance+approach*0.34,motorLateral);
            addOrientedBox(track,indicator.x,indicator.y+0.49f,indicator.z,
                           0.11f,0.62f,0.11f,indicator.yaw,Color(54,60,59,255));
            addOrientedBox(track,indicator.x,indicator.y+0.82f,indicator.z,
                           0.20f,0.16f,0.30f,indicator.yaw,Color(229,190,49,255));

            // Two polished switch blades follow the authored siding blend from the stock rails
            // toward the diverging alignment. The frog is found from that same function at the
            // first gauge-width separation, so opposite platform sides need no guessed offset.
            const double bladeOuterDistance = motorDistance+approach*4.0;
            const double bladeInnerDistance = motorDistance-approach*18.0;
            const double bladeOuterLateral = stationSidingLateral(
                route,bladeOuterDistance).value_or(0.0);
            const double bladeInnerLateral = stationSidingLateral(
                route,bladeInnerDistance).value_or(0.0);
            for (double railOffset : {-0.81,0.71})
                addRouteRailSegment(
                    bladeOuterDistance,bladeOuterLateral+railOffset,
                    bladeInnerDistance,bladeInnerLateral+railOffset,
                    0.064f,0.070f,Color(150,157,156,255));

            double frogDistance = motorDistance;
            for (int metre = 0; metre <= 100; ++metre)
            {
                const double candidate = motorDistance
                    - approach*static_cast<double>(metre);
                const double sidingLateral = stationSidingLateral(
                    route,candidate).value_or(0.0);
                frogDistance = candidate;
                if (std::abs(sidingLateral) >= 1.52) break;
            }
            const double mainDivergingRail = sidingSide > 0.0 ? 0.71 : -0.81;
            const double branchInnerRailOffset = sidingSide > 0.0 ? -0.81 : 0.71;
            const LocalRoutePoint frogBase = localRoutePoint(
                route,anchorDistance,frogDistance,mainDivergingRail);
            addOrientedBox(track,frogBase.x,frogBase.y+0.035f,frogBase.z,
                           0.52f,0.045f,2.7f,frogBase.yaw,Color(55,60,59,255));
            const double frogOuterDistance = frogDistance+approach*3.2;
            const double frogInnerDistance = frogDistance-approach*3.2;
            addRouteRailSegment(
                frogOuterDistance,mainDivergingRail,
                frogInnerDistance,mainDivergingRail,
                0.072f,0.074f,Color(158,164,163,255));
            addRouteRailSegment(
                frogOuterDistance,
                stationSidingLateral(route,frogOuterDistance).value_or(0.0)
                    + branchInnerRailOffset,
                frogInnerDistance,
                stationSidingLateral(route,frogInnerDistance).value_or(0.0)
                    + branchInnerRailOffset,
                0.072f,0.074f,Color(151,158,157,255));
            ++turnoutPointMotors;
            ++turnoutBladeAssemblies;
        }
    }

    // Numbered kilometre posts turn the otherwise continuous procedural corridor into a railway
    // with measurable geography. They use absolute route alignment, stay on the main-line verge,
    // and join the existing 2 km track batch. Their inward face remains readable from passenger
    // windows in either travel direction without adding another draw call.
    const auto firstKilometre = static_cast<std::int64_t>(std::ceil(begin/1'000.0));
    const auto endKilometre = static_cast<std::int64_t>(std::ceil(end/1'000.0));
    for (std::int64_t kilometre = firstKilometre; kilometre < endKilometre; ++kilometre)
    {
        const double markerDistance = static_cast<double>(kilometre)*1'000.0;
        const Sim::RoutePose pose = route.sample(markerDistance);
        bool nearStation = false;
        for (const Sim::Station& station : route.stations())
            nearStation = nearStation
                || std::abs(station.distanceMetres-markerDistance) < 700.0;
        if (nearStation || pose.tunnel || pose.bridge) continue;

        constexpr double kMarkerLateral = 3.65;
        const LocalRoutePoint post = localRoutePoint(
            route,anchorDistance,markerDistance,kMarkerLateral);
        addOrientedBox(track,post.x,post.y+0.79f,post.z,
                       0.13f,1.58f,0.13f,post.yaw,Color(172,174,166,255));
        addOrientedBox(track,post.x,post.y+1.69f,post.z,
                       0.16f,0.97f,1.23f,post.yaw,Color(62,67,65,255));
        addOrientedBox(track,post.x,post.y+1.69f,post.z,
                       0.19f,0.84f,1.10f,post.yaw,Color(226,226,215,255));

        const std::string kilometres = std::to_string(kilometre);
        constexpr double kInwardFaceLateral = kMarkerLateral-0.105;
        addRouteSignText(track,route,anchorDistance,"KM",markerDistance,
                         kInwardFaceLateral,2.00f,Color(45,49,48,255));
        addRouteSignText(track,route,anchorDistance,kilometres,markerDistance,
                         kInwardFaceLateral,1.66f,Color(45,49,48,255));
        ++kilometrePosts;
    }
    for (std::size_t i=0;i<kTrackMaterialCount;++i)
        segments[i]=meshSegments(materials()[i]->data());
    generationMs = std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-generationStart).count();
}


} // namespace TrainSim::Render
