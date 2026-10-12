#include "trainsim/world/StreamingWorld.hpp"
#include "trainsim/world/StreetLayout.hpp"
#include "trainsim/world/StationSite.hpp"

#include "trainsim/sim/RouteSimulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace TrainSim::World {

namespace {
constexpr double kSignalBlockMetres = 500.0;
constexpr double kOtherTrainSpeedMetresPerSecond = 31.0;
constexpr double kOtherTrainHalfTraversalMetres = 950.0;
constexpr double kOtherTrainTraversalMetres = kOtherTrainHalfTraversalMetres * 2.0;
constexpr double kOtherTrainHiddenResetMetres = 310.0;
constexpr double kOtherTrainCycleMetres =
    kOtherTrainTraversalMetres + kOtherTrainHiddenResetMetres;

// Compact, irregular copses with clearings between them. Consume no generator
// state: changing vegetation layout must not relocate stations or buildings.
void placeCopse(std::int64_t chunkIndex, int slot, double begin, double length,
                bool distant, double& distance, double& side, double& lateralRoll)
{
    const int group = slot / 5;
    const int member = slot % 5;
    const double phase = static_cast<double>(chunkIndex)*1.61803398875
        + group*2.39996322973 + (distant ? 5.7 : 0.0);
    const double centre = 0.18 + 0.64*(0.5+0.5*std::sin(phase*1.73));
    const double angle = member*2.39996322973+phase;
    const double radius = 4.0+member*2.3;
    distance = begin + std::clamp(centre*length + std::cos(angle)*radius,
                                 2.0, std::max(2.0,length-2.0));
    side = std::sin(phase*2.17) < 0.0 ? -1.0 : 1.0;
    lateralRoll = std::clamp(0.48+0.23*std::sin(phase*0.83)
        + std::sin(angle)*radius/(distant ? 145.0 : 65.0),0.0,1.0);
}

double otherTrainPhase(const WorldFeature& feature, double seconds) noexcept
{
    const double safeSeconds = std::isfinite(seconds) ? std::max(0.0,seconds) : 0.0;
    // Variant one puts chunk 37's service beside the default rear-car player position in the
    // established pass-by smoke. Variant two makes the far-route integration fixture protect a
    // nearby level crossing even while the player's train is outside its closure envelope.
    constexpr std::array<double,3> phaseOffsets = {0.0,738.0,1'350.0};
    return std::fmod(safeSeconds * kOtherTrainSpeedMetresPerSecond
                     + phaseOffsets[feature.variant%phaseOffsets.size()],
                     kOtherTrainCycleMetres);
}
}

int signalFacingDirection(const WorldFeature& feature) noexcept
{
    return feature.variant == 0 ? 1 : -1;
}

bool terrainFeatureAllowed(const Sim::RoutePose& pose, double lateralMetres) noexcept
{
    if (pose.tunnel) return false;
    const double waterwardLateral = lateralMetres
        * static_cast<double>(pose.adjacentWaterSide);
    if (pose.adjacentWaterSide != 0
        && waterwardLateral > 60.0 && waterwardLateral < 560.0)
        return false;
    if (!pose.bridge) return true;
    // A dry viaduct may cross wooded slopes, but its deck and pier corridor stays clear. The
    // estuary crossing has a much wider navigable channel, with scenery only on raised banks.
    const double clearance = pose.waterCrossing ? 125.0 : 18.0;
    return std::abs(lateralMetres) >= clearance;
}

double movingOtherTrainDistance(const WorldFeature& feature, double seconds,
                                double routeLengthMetres) noexcept
{
    const double safeLength = std::isfinite(routeLengthMetres)
        ? std::max(0.0,routeLengthMetres) : 0.0;
    const double phase = std::min(otherTrainPhase(feature,seconds),
                                  kOtherTrainTraversalMetres);
    const double offset = kOtherTrainHalfTraversalMetres - phase;
    return std::clamp(feature.routeDistance + offset,0.0,safeLength);
}

bool movingOtherTrainVisible(const WorldFeature& feature, double seconds,
                             double routeLengthMetres) noexcept
{
    if (!std::isfinite(routeLengthMetres) || routeLengthMetres < 0.0) return false;
    const double phase = otherTrainPhase(feature,seconds);
    if (phase >= kOtherTrainTraversalMetres) return false;
    const double rawDistance = feature.routeDistance
        + kOtherTrainHalfTraversalMetres-phase;
    return rawDistance >= 0.0 && rawDistance <= routeLengthMetres;
}

double levelCrossingClosureAmount(double crossingDistanceMetres,
                                  double trainDistanceMetres) noexcept
{
    if (!std::isfinite(crossingDistanceMetres) || !std::isfinite(trainDistanceMetres))
        return 0.0;
    const double separation = std::abs(crossingDistanceMetres-trainDistanceMetres);
    const double raising = std::clamp((separation-330.0)/(520.0-330.0),0.0,1.0);
    const double easedRaising = raising*raising*(3.0-2.0*raising);
    return 1.0-easedRaising;
}

double StreamingWorld::requiredLevelCrossingClosure(
    double crossingDistanceMetres, double trainDistanceMetres,
    double seconds) const noexcept
{
    double requiredClosure = levelCrossingClosureAmount(
        crossingDistanceMetres,trainDistanceMetres);
    // The same bounded seven-chunk traffic window drives rendering and audio. An opposing train
    // can therefore protect the road even while the passenger service is outside the envelope.
    for (const Chunk& chunk : chunks_)
        for (const WorldFeature& feature : chunk.features)
        {
            if (feature.kind != FeatureKind::OtherTrain
                || !movingOtherTrainVisible(feature,seconds,route_.lengthMetres())) continue;
            requiredClosure = std::max(requiredClosure,levelCrossingClosureAmount(
                crossingDistanceMetres,
                movingOtherTrainDistance(feature,seconds,route_.lengthMetres())));
        }
    return requiredClosure;
}

SignalAspect signalAspect(const WorldFeature& feature, const Sim::Route& route,
                          const Sim::ServiceState& service) noexcept
{
    const int facingDirection = signalFacingDirection(feature);
    if (facingDirection != service.direction) return SignalAspect::Green;

    const double signedDistance =
        (feature.routeDistance - service.routeDistanceMetres) * facingDirection;
    const bool occupiedBlockBehind = signedDistance < 0.0
        && signedDistance > -kSignalBlockMetres;
    const bool heldForStationDeparture = signedDistance >= 0.0
        && signedDistance < kSignalBlockMetres
        && (service.phase == Sim::ServicePhase::Dwelling
            || service.phase == Sim::ServicePhase::ClosingDoors);
    if (occupiedBlockBehind || heldForStationDeparture) return SignalAspect::Red;

    const double stationBeyondSignal =
        (route.stations()[service.nextStation].distanceMetres - feature.routeDistance)
        * facingDirection;
    if (signedDistance >= 0.0 && stationBeyondSignal >= 0.0
        && stationBeyondSignal < kSignalBlockMetres
        && (service.phase == Sim::ServicePhase::Coasting
            || service.phase == Sim::ServicePhase::Braking))
        return SignalAspect::Yellow;
    return SignalAspect::Green;
}

StreamingWorld::StreamingWorld(const Sim::Route& route, double chunkLengthMetres, int radius)
    : route_(route), chunkLength_(chunkLengthMetres), radius_(radius)
{
    if (chunkLengthMetres < 50.0 || radius < 1)
        throw std::invalid_argument("Streaming chunks require at least 50 m length and radius one");
    const std::size_t maximumActive = static_cast<std::size_t>(radius_) * 2U + 1U;
    chunks_.reserve(maximumActive);
    nextChunksScratch_.reserve(maximumActive);
}

std::uint64_t StreamingWorld::hash(std::uint64_t value) const noexcept
{
    value ^= route_.seed() + 0x9e3779b97f4a7c15ULL + (value << 6) + (value >> 2);
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

Chunk StreamingWorld::generate(std::int64_t index, bool resolveVegetation) const
{
    Chunk chunk;
    chunk.index = index;
    chunk.beginDistance = static_cast<double>(index) * chunkLength_;
    chunk.endDistance = std::min(route_.lengthMetres(), chunk.beginDistance + chunkLength_);
    const double midpoint = (chunk.beginDistance + chunk.endDistance) * 0.5;
    const Sim::RoutePose pose = route_.sample(midpoint);

    std::uint64_t state = hash(static_cast<std::uint64_t>(index));
    auto random = [&state]() {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 2685821657736338717ULL;
    };
    auto unit = [&random]() {
        return static_cast<double>(random() >> 11) * (1.0 / 9007199254740992.0);
    };

    const int poles = std::max(1, static_cast<int>(chunkLength_ / 50.0));
    for (int i = 0; i < poles; ++i)
    {
        const double distance = chunk.beginDistance
            + (static_cast<double>(i) + 0.5) * chunkLength_ / poles;
        const double lateral = i % 2 == 0 ? 4.3
            : route_.parallelTrackLateralMetres(distance) - 4.3;
        chunk.features.push_back({FeatureKind::CatenaryPole, distance, lateral, 1.0,
                                  static_cast<std::uint32_t>(i & 1)});
    }

    // Four adjacent chunks form a short high-voltage corridor. Keeping the group deterministic
    // makes the same pylons and wire spans return after streaming without route-wide density.
    const std::int64_t powerPhase = index % 113;
    if (powerPhase >= 24 && powerPhase <= 27 && !pose.tunnel && !pose.bridge)
    {
        const double side = ((index / 113) & 1) == 0 ? 1.0 : -1.0;
        chunk.features.push_back({FeatureKind::PowerPylon,
            (chunk.beginDistance + chunk.endDistance) * 0.5, side * 82.0,
            0.88 + unit() * 0.16, static_cast<std::uint32_t>(powerPhase - 24)});
    }
    const double substationSide = ((index / 173) & 1) == 0 ? 1.0 : -1.0;
    const double substationDistance = chunk.beginDistance + chunkLength_ * 0.58;
    const Sim::RoutePose substationPose = route_.sample(substationDistance);
    const bool hasSubstation = index > 0 && index % 173 == 8
        && !substationPose.tunnel && !substationPose.bridge;
    if (hasSubstation)
    {
        chunk.features.push_back({FeatureKind::Substation,
            substationDistance, substationSide * 38.0,
            0.92 + unit() * 0.16, static_cast<std::uint32_t>(random() % 3ULL)});
    }

    const int vegetation = 7 + static_cast<int>((1.0 - pose.urbanity) * 22.0);
    for (int i = 0; i < vegetation; ++i)
    {
        double side = unit() < 0.5 ? -1.0 : 1.0;
        double distance = chunk.beginDistance + unit() * chunkLength_;
        double lateralRoll = unit();
        const double rawScale = 0.72 + unit() * 1.4;
        const std::uint32_t variant = static_cast<std::uint32_t>(random() % 5ULL);
        // Retain occasional isolated edge trees, and preserve urban street planting.
        if (pose.urbanity < 0.35 && i % 6 != 5)
            placeCopse(index,i-i/6,chunk.beginDistance,chunkLength_,false,
                       distance,side,lateralRoll);
        const Sim::RoutePose featurePose = route_.sample(distance);
        double lateralMagnitude = featurePose.waterCrossing
            ? 145.0 + lateralRoll * 100.0 : 11.0 + lateralRoll * 65.0;
        // Urban trees belong behind the outer pavement, never in either mirrored carriageway.
        // Keep random consumption unchanged so this clearance does not reshuffle later features.
        if (!featurePose.waterCrossing && pose.urbanity > 0.28)
            lateralMagnitude = std::max(lateralMagnitude,26.5);
        // Very large crowns immediately outside the fence filled an entire passenger window.
        // Preserve mature specimens farther out, but bound close-tree height and keep the crown
        // beyond the maintained railway verge. Random consumption and route determinism stay
        // unchanged.
        const double closeScaleLimit = 1.34
            + std::clamp((lateralMagnitude-11.0)/34.0,0.0,1.0)*0.78;
        const double scale = featurePose.waterCrossing
            ? rawScale : std::min(rawScale,closeScaleLimit);
        if (!featurePose.waterCrossing)
            lateralMagnitude = std::max(lateralMagnitude,8.8+2.30*scale);
        const double lateral = side*lateralMagnitude;
        if (terrainFeatureAllowed(featurePose,lateral))
            chunk.features.push_back({FeatureKind::Tree,distance,lateral,scale,variant});
    }

    // A second, smaller-scale band prevents open countryside from ending in a bare horizon.
    // These trees sit well beyond the lineside vegetation and form intermittent woodland edges;
    // they remain ordinary deterministic features, so streaming and quality-distance culling need
    // no special case.
    const int distantWoodland = pose.urbanity < 0.35 ? 10 : pose.urbanity < 0.62 ? 4 : 0;
    for (int i = 0; i < distantWoodland; ++i)
    {
        double side = unit() < 0.5 ? -1.0 : 1.0;
        double distance = chunk.beginDistance + unit() * chunkLength_;
        const double scale = 0.55 + unit() * 0.78;
        const std::uint32_t variant = static_cast<std::uint32_t>(random() % 5ULL);
        double lateralRoll = unit();
        if (pose.urbanity < 0.35)
            placeCopse(index,i,chunk.beginDistance,chunkLength_,true,
                       distance,side,lateralRoll);
        const Sim::RoutePose featurePose = route_.sample(distance);
        const double lateral = side * (featurePose.waterCrossing
            ? 170.0 + lateralRoll * 130.0 : 88.0 + lateralRoll * 145.0);
        if (terrainFeatureAllowed(featurePose,lateral))
            chunk.features.push_back({FeatureKind::Tree,distance,lateral,scale,variant});
    }

    const int grassPatches = 4 + static_cast<int>((1.0 - pose.urbanity) * 10.0);
    for (int i = 0; i < grassPatches; ++i)
    {
        const double side = unit() < 0.5 ? -1.0 : 1.0;
        const double distance = chunk.beginDistance + unit() * chunkLength_;
        const double lateralRoll = unit();
        const double scale = 0.62 + unit() * 0.82;
        const std::uint32_t variant = static_cast<std::uint32_t>(random() % 6ULL);
        const Sim::RoutePose featurePose = route_.sample(distance);
        const double lateral = side * (featurePose.waterCrossing
            ? 130.0 + lateralRoll * 120.0 : 8.5 + lateralRoll * 78.0);
        if (terrainFeatureAllowed(featurePose,lateral))
            chunk.features.push_back({FeatureKind::GrassPatch,distance,lateral,scale,variant});
    }

    if (pose.urbanity < 0.42)
        for (int i = 0; i < 2; ++i)
        {
            const double side = unit() < 0.5 ? -1.0 : 1.0;
            const double distance = chunk.beginDistance + 18.0
                + unit() * std::max(1.0, chunkLength_ - 36.0);
            const double lateralRoll = unit();
            const double scale = 0.78 + unit() * 0.65;
            const std::uint32_t variant = static_cast<std::uint32_t>(random() % 4ULL);
            const Sim::RoutePose featurePose = route_.sample(distance);
            const double lateral = side * (featurePose.waterCrossing
                ? 160.0 + lateralRoll * 100.0 : 34.0 + lateralRoll * 72.0);
            if (terrainFeatureAllowed(featurePose,lateral))
                chunk.features.push_back({FeatureKind::FieldCrop,distance,lateral,scale,variant});
        }

    // Preserve the two established random parcels above, then add one independently hashed
    // alternating-side field. This prevents a long passenger-side view from becoming empty when
    // both random parcels happen to choose the opposite side, without shifting the random stream
    // (and therefore every established building/landmark) later in the chunk.
    if (pose.urbanity < 0.42)
    {
        const std::uint64_t fieldSeed = hash(
            static_cast<std::uint64_t>(index)^0x4649454c44504152ULL);
        const auto fraction = [fieldSeed](unsigned shift) {
            return static_cast<double>((fieldSeed>>shift)&0xffffULL)/65'535.0;
        };
        const double distance = chunk.beginDistance+18.0
            + fraction(0U)*std::max(1.0,chunk.endDistance-chunk.beginDistance-36.0);
        const double side = (index&1) == 0 ? 1.0 : -1.0;
        const double lateral = side*(38.0+fraction(16U)*52.0);
        const double scale = 0.84+fraction(32U)*0.56;
        const std::uint32_t variant = static_cast<std::uint32_t>((fieldSeed>>48U)&3ULL);
        const Sim::RoutePose fieldPose = route_.sample(distance);
        if (terrainFeatureAllowed(fieldPose,lateral))
            chunk.features.push_back({FeatureKind::FieldCrop,distance,lateral,scale,variant});
    }

    // Occasional compact straw stacks give the fields a worked agricultural character. They are
    // sparse enough to remain landmarks at speed and are grouped within one deterministic host
    // chunk instead of becoming a uniform route-wide scatter.
    bool hayNearStation = false;
    for (const Sim::Station& station : route_.stations())
        hayNearStation = hayNearStation || std::abs(station.distanceMetres-midpoint) < 650.0;
    if (index > 0 && index % 23 == 11 && pose.urbanity < 0.42
        && !pose.tunnel && !pose.bridge && !hayNearStation)
    {
        const double side = ((index/23)&1) == 0 ? 1.0 : -1.0;
        const int baleCount = 2+static_cast<int>(random()%3ULL);
        for (int bale = 0; bale < baleCount; ++bale)
        {
            const double distance = chunk.beginDistance+chunkLength_*(0.30+0.16*bale)
                +(unit()-0.5)*9.0;
            const double lateral = side*(31.0+unit()*25.0);
            const double scale = 0.82+unit()*0.30;
            const Sim::RoutePose balePose = route_.sample(distance);
            if (terrainFeatureAllowed(balePose,lateral))
                chunk.features.push_back({FeatureKind::HayBale,distance,lateral,scale,
                    static_cast<std::uint32_t>(random()%4ULL)});
        }
    }

    const int shrubs = 3 + static_cast<int>((1.0 - pose.urbanity) * 9.0);
    for (int i = 0; i < shrubs; ++i)
    {
        const double side = unit() < 0.5 ? -1.0 : 1.0;
        const double distance = chunk.beginDistance + unit() * chunkLength_;
        const double lateralRoll = unit();
        const double scale = 0.55 + unit() * 0.75;
        const std::uint32_t variant = static_cast<std::uint32_t>(random() % 4ULL);
        const Sim::RoutePose featurePose = route_.sample(distance);
        double lateralMagnitude = featurePose.waterCrossing
            ? 140.0 + lateralRoll * 110.0 : 10.0 + lateralRoll * 72.0;
        if (!featurePose.waterCrossing && pose.urbanity > 0.28)
            lateralMagnitude = std::max(lateralMagnitude,26.2);
        const double lateral = side*lateralMagnitude;
        if (terrainFeatureAllowed(featurePose,lateral))
            chunk.features.push_back({FeatureKind::Bush,distance,lateral,scale,variant});
    }

    // Industrial buildings read more convincingly as deliberately planned compounds than as a
    // uniform random dusting of sheds. Every third eligible urban chunk receives one compact
    // two/three-building group with a shared palette and enough setback for a service yard.
    // Standalone warehouses remain possible below, but at a much lower frequency.
    const bool hasIndustrialCompound = pose.urbanity > 0.55
        && index % 3 == 1 && !pose.tunnel && !pose.bridge;
    const int compoundWarehouseCount = 2+static_cast<int>((index/3)&1);
    const double compoundScale = 0.88+0.05*static_cast<double>((index/3)%4);
    const double compoundSpacing = 30.0*compoundScale;
    const double compoundDistance = midpoint
        + (static_cast<double>((index/3)%5)-2.0)*5.5;
    double compoundSide = ((index/3)&1) == 0 ? 1.0 : -1.0;
    const Sim::RoutePose compoundPose = route_.sample(compoundDistance);
    if (compoundPose.adjacentWaterSide != 0
        && compoundSide == static_cast<double>(compoundPose.adjacentWaterSide))
        compoundSide = -compoundSide;
    if (hasSubstation && compoundSide == substationSide
        && std::abs(compoundDistance-substationDistance) < 70.0)
        compoundSide = -compoundSide;
    const double compoundLateral = compoundSide
        * (49.0+3.0*static_cast<double>((index/3)%3));
    const std::uint32_t compoundPalette = static_cast<std::uint32_t>((index/3)%4);

    const int buildings = static_cast<int>(pose.urbanity * 18.0);
    for (int i = 0; i < buildings; ++i)
    {
        double side = unit() < 0.5 ? -1.0 : 1.0;
        FeatureKind kind = FeatureKind::House;
        const double buildingRoll = unit();
        if (pose.urbanity > 0.68 && buildingRoll < 0.30) kind = FeatureKind::Apartment;
        else if (pose.urbanity > 0.55 && buildingRoll < 0.36) kind = FeatureKind::Warehouse;
        const double lateralRoll = unit();
        const double buildingDistance = chunk.beginDistance + unit() * chunkLength_;
        // Keep modular buildings large enough to read at speed without allowing a random tower
        // to engulf the railway corridor. The lateral setback below accounts for the actual
        // footprint, rather than only the building centre.
        const double buildingScale = 0.75 + unit() * 0.90;
        const double halfWidth = kind == FeatureKind::Warehouse ? 8.5 * buildingScale
                               : kind == FeatureKind::Apartment ? 5.0 * buildingScale
                                                                : 3.5 * buildingScale;
        if (hasSubstation && side == substationSide
            && std::abs(buildingDistance - substationDistance) < 45.0)
            side = -side;
        if (hasIndustrialCompound && side == compoundSide
            && std::abs(buildingDistance-compoundDistance) < 58.0)
            side = -side;
        if (hasSubstation && side == substationSide
            && std::abs(buildingDistance-substationDistance) < 45.0)
            continue;
        if (hasIndustrialCompound && side == compoundSide
            && std::abs(buildingDistance-compoundDistance) < 58.0)
            continue;
        // Both sides contain a parallel urban road. Keep modular buildings behind a deliberate
        // pavement/setback band. Host-chunk buildings also yield to a substation.
        // Reserve the first parcel row for coherent townhouse frontage. Larger random
        // buildings form a second depth layer instead of intersecting those front gardens.
        const double frontageSetback = pose.urbanity > 0.45 ? 44.0 : 32.0;
        const double lateral = side*(frontageSetback+halfWidth+lateralRoll*28.0);
        const std::uint32_t variant = static_cast<std::uint32_t>(random() % 8ULL);
        const Sim::RoutePose featurePose = route_.sample(buildingDistance);
        if (terrainFeatureAllowed(featurePose,lateral))
            chunk.features.push_back({kind,buildingDistance,lateral,buildingScale,variant});
    }

    if (hasIndustrialCompound)
    {
        bool compoundAllowed = true;
        for (int building = 0; building < compoundWarehouseCount; ++building)
        {
            const double centredSlot = static_cast<double>(building)
                - 0.5*static_cast<double>(compoundWarehouseCount-1);
            const double distance = compoundDistance+centredSlot*compoundSpacing;
            compoundAllowed = compoundAllowed
                && terrainFeatureAllowed(route_.sample(distance),compoundLateral);
        }
        if (compoundAllowed)
        {
            for (int building = 0; building < compoundWarehouseCount; ++building)
            {
                const double centredSlot = static_cast<double>(building)
                    - 0.5*static_cast<double>(compoundWarehouseCount-1);
                const double distance = compoundDistance+centredSlot*compoundSpacing;
                std::uint32_t variant = compoundPalette;
                if (building == 0) variant |= kWarehouseCompoundLeader;
                if (compoundWarehouseCount == 3)
                    variant |= kWarehouseThreeBuildingCompound;
                const double buildingScale = compoundScale
                    * (building == 1 ? 0.94 : building == 2 ? 1.06 : 1.0);
                chunk.features.push_back({FeatureKind::Warehouse,distance,compoundLateral,
                                          buildingScale,variant});
            }
        }
    }

    // Dense city envelopes receive a second, distant depth layer. Taller silhouettes sit well
    // behind the ordinary road frontage, so a lateral passenger view reads as a city rather than
    // a row of mutually occluding houses while retaining a tiny deterministic feature count.
    if (pose.urbanity > 0.72 && !pose.tunnel && !pose.bridge)
    {
        const int towers = 1 + static_cast<int>((pose.urbanity-0.72)*6.0);
        for (int i = 0; i < towers; ++i)
        {
            const double side = ((i+static_cast<int>(index&1))%2 == 0) ? -1.0 : 1.0;
            const double towerDistance = chunk.beginDistance
                + (static_cast<double>(i)+0.35+unit()*0.30)
                    * chunkLength_/static_cast<double>(towers);
            const double scale = 0.82+unit()*0.43;
            const double lateral = side*(92.0+unit()*96.0);
            const Sim::RoutePose towerPose = route_.sample(towerDistance);
            if (terrainFeatureAllowed(towerPose,lateral))
                chunk.features.push_back({FeatureKind::Tower,towerDistance,lateral,scale,
                    static_cast<std::uint32_t>(random()%8ULL)});
        }
    }

    // Sparse farm compounds break up the long agricultural sections without turning every field
    // into a village. Their broad footprint stays well beyond the railway/road corridor and the
    // same deterministic feature returns after unloading and re-streaming its host chunk.
    if (index > 0 && index % 53 == 31 && pose.urbanity < 0.35
        && !pose.tunnel && !pose.bridge)
    {
        double side = ((index / 53) & 1) == 0 ? 1.0 : -1.0;
        const double farmDistance = chunk.beginDistance + chunkLength_ * 0.35
            + (unit()-0.5)*36.0;
        const double scale = 0.90 + unit()*0.28;
        if (hasSubstation && side == substationSide
            && std::abs(farmDistance-substationDistance) < 55.0)
            side = -side;
        const double lateral = side * (52.0 + unit()*23.0);
        const Sim::RoutePose farmPose = route_.sample(farmDistance);
        if (terrainFeatureAllowed(farmPose,lateral))
            chunk.features.push_back({FeatureKind::Farmstead,farmDistance,lateral,scale,
                static_cast<std::uint32_t>(random()%6ULL)});
    }

    // Dense approaches need a continuous street edge as well as randomly scattered larger
    // buildings. A bounded row of narrow townhouses creates that readable urban rhythm without
    // requiring authored geometry or a separate render batch. Spacing is deterministic and keeps
    // every footprint clear of both the railway and the parallel road on either side.
    if (pose.urbanity > 0.45)
    {
        const int streetfrontBuildings = 4 + static_cast<int>(pose.urbanity * 11.0);
        for (int i = 0; i < streetfrontBuildings; ++i)
        {
            const int blocks=(streetfrontBuildings+3)/4;
            // Keep end blocks close to the seam: centred bins left a conspicuous empty
            // street at every 250 m boundary even though each individual block looked sound.
            const double centre=chunk.beginDistance+15.0
                +static_cast<double>(i/4)*(chunkLength_-30.0)/static_cast<double>(blocks-1);
            const double buildingDistance=centre+(i%2==0 ? -7.2 : 7.2)
                +(unit()-0.5)*0.6;
            double side=((i/2+static_cast<int>(index&1))%2==0) ? -1.0 : 1.0;
            const double scale = 0.76 + unit() * 0.42;
            // Reserve the entire planted parcel, not only the narrower building shell.
            const double halfWidth = 4.3 * scale;
            if (hasSubstation && side == substationSide
                && std::abs(buildingDistance - substationDistance) < 45.0)
                side = -side;
            if (hasIndustrialCompound && side == compoundSide
                && std::abs(buildingDistance-compoundDistance) < 58.0)
                side = -side;
            const double lateral=side*(25.5+halfWidth+unit()*0.5);
            const Sim::RoutePose featurePose = route_.sample(buildingDistance);
            // A switch away from the yard can land on an opposite-side substation. Recheck
            // both reservations; omit the parcel rather than toggling it back into a conflict.
            if ((hasSubstation && side == substationSide
                    && std::abs(buildingDistance-substationDistance) < 45.0)
                || (hasIndustrialCompound && side == compoundSide
                    && std::abs(buildingDistance-compoundDistance) < 58.0))
                continue;
            const bool overlapsFrontage=std::any_of(chunk.features.begin(),chunk.features.end(),
                [&](const WorldFeature& other) {
                    return other.kind==FeatureKind::Townhouse && other.lateralMetres*lateral>0
                        && std::abs(other.routeDistance-buildingDistance)
                            <=5.7*(other.scale+scale)+0.15;
                });
            if (!overlapsFrontage && terrainFeatureAllowed(featurePose,lateral))
                chunk.features.push_back({FeatureKind::Townhouse,buildingDistance,lateral,scale,
                    static_cast<std::uint32_t>(random() % 8ULL)});
        }
    }

    if (pose.urbanity > 0.28)
    {
        const int traffic = 1 + static_cast<int>(pose.urbanity * 3.0);
        for (int i = 0; i < traffic; ++i)
        {
            const double routeDistance = chunk.beginDistance + unit() * chunkLength_;
            const double laneJitter = (unit() - 0.5) * 0.45;
            const double scale = 0.82 + unit() * 0.22;
            const std::uint32_t variant = static_cast<std::uint32_t>(random() % 6ULL);
            // Even variants move with increasing route distance in the inner lane; odd variants
            // return in the outer lane. Half of the palette uses the mirrored street, making both
            // passenger-window sides active without increasing the bounded traffic population.
            const double roadSide = variant < 3U ? 1.0 : -1.0;
            const double laneCentre = roadSide
                * (variant % 2U == 0U ? 17.35 : 19.25);
            const Sim::RoutePose featurePose = route_.sample(routeDistance);
            if (!featurePose.tunnel && !featurePose.bridge)
                chunk.features.push_back({FeatureKind::RoadVehicle, routeDistance,
                                          laneCentre + laneJitter, scale, variant});
        }
    }

    // Ordinary rural cross-roads use protected level crossings. Making them explicit streamed
    // features lets the renderer animate their barriers and warning lamps from the live service
    // distance without inventing a second placement system.
    bool crossingNearStation = false;
    for (const Sim::Station& station : route_.stations())
        crossingNearStation = crossingNearStation
            || std::abs(station.distanceMetres-midpoint) < 650.0;
    if (index > 0 && index % 29 == 0 && !crossingNearStation
        && !pose.bridge && !pose.tunnel)
        chunk.features.push_back({FeatureKind::LevelCrossing,midpoint,0.0,1.0,
                                  static_cast<std::uint32_t>((index/29)&1)});

    // A few cross-route roads use grade separation rather than another level crossing. The
    // bridge is deliberately offset from the regular 50 m catenary-pole cadence, and important
    // station/tunnel/bridge sections remain authored and unobstructed.
    const double overpassDistance = chunk.beginDistance + chunkLength_ * 0.572;
    const Sim::RoutePose overpassPose = route_.sample(overpassDistance);
    bool nearStation = false;
    for (const Sim::Station& station : route_.stations())
        nearStation = nearStation
            || std::abs(station.distanceMetres - overpassDistance) < 900.0;
    if (index > 0 && index % 149 == 57 && !overpassPose.tunnel && !overpassPose.bridge
        && !nearStation)
    {
        chunk.features.push_back({FeatureKind::RoadOverpass,overpassDistance,2.5,
            0.94 + unit() * 0.12,static_cast<std::uint32_t>(random() % 4ULL)});
    }

    // Alternate direction-facing block signals. Variant zero faces increasing route distance and
    // variant one faces decreasing distance; the renderer derives live aspects from that contract.
    const bool facesIncreasingDistance = index % 2 == 0;
    chunk.features.push_back({FeatureKind::Signal, chunk.endDistance - 20.0,
                              facesIncreasingDistance ? 3.4 : -3.4, 1.0,
                              facesIncreasingDistance ? 0U : 1U});
    if (index > 0 && index % 37 == 0)
        chunk.features.push_back({FeatureKind::OtherTrain, chunk.beginDistance + 80.0,
                                  -5.0, 1.0, static_cast<std::uint32_t>(index % 3)});
    // Last endpoint chunks may contain samples beyond the nominal route end.
    // Keep their decorative parcels out of the authored terminal public apron.
    std::erase_if(chunk.features,[&](const WorldFeature& feature) {
        return overlapsTerminalForecourt(feature,route_);
    });
    if (!resolveVegetation) return chunk;
    // Vegetation is sampled before buildings. Resolve it against the finished parcel layout,
    // including crown spread, rather than only keeping trunks off the road. Store independent
    // footprints so erase_if never observes records being moved within its source vector.
    struct Parcel { double distance, lateral, halfWidth, halfLength; };
    std::vector<Parcel> parcels;
    const auto appendParcels = [&](const Chunk& source) {
    for (const auto& feature : source.features)
    {
        double halfWidth = 0.0;
        double halfLength = 0.0;
        switch (feature.kind)
        {
            case FeatureKind::House: halfWidth=5.0; halfLength=6.2; break;
            case FeatureKind::Townhouse: halfWidth=4.3; halfLength=5.7; break;
            case FeatureKind::Apartment: halfWidth=6.4; halfLength=7.8; break;
            case FeatureKind::Tower: halfWidth=8.8; halfLength=10.5; break;
            case FeatureKind::Warehouse: halfWidth=10.0; halfLength=13.0; break;
            case FeatureKind::Farmstead: halfWidth=14.0; halfLength=16.0; break;
            case FeatureKind::Substation: halfWidth=12.0; halfLength=16.0; break;
            default: continue;
        }
        parcels.push_back({feature.routeDistance,feature.lateralMetres,
                           halfWidth*feature.scale,halfLength*feature.scale});
        if (feature.kind==FeatureKind::House
            && houseHasOutbuilding(feature.variant,route_.sample(feature.routeDistance).urbanity))
            parcels.push_back({feature.routeDistance+houseOutbuildingDirection(feature.variant)
                                   *kHouseOutbuildingOffset*feature.scale,
                               feature.lateralMetres,(kHouseOutbuildingHalfWidth+0.6)*feature.scale,
                               (kHouseOutbuildingHalfLength+0.6)*feature.scale});
        const auto accessPose=route_.sample(feature.routeDistance);
        if (accessPose.urbanity>0.45 && !accessPose.bridge && !accessPose.tunnel)
            if (const auto access=residentialAccess(feature))
                parcels.push_back({access->distance,access->side*(24.6+access->facade)*0.5,
                    (access->facade-24.6)*0.5,access->width*0.5+1.1});
        if (const auto link=frontageLink(source.features,feature))
        {
            const double side=feature.lateralMetres<0 ? -1.0 : 1.0;
            parcels.push_back({(link->begin+link->end)*0.5,side*(link->inner+link->outer)*0.5,
                               (link->outer-link->inner)*0.5,(link->end-link->begin)*0.5});
        }
    }
    };
    appendParcels(chunk);
    // Inspect deterministic raw neighbours, not whichever chunks happen to be loaded. Raw
    // generation exits before this stage, so it neither recurses nor relocates their plants.
    // Only parcel footprints survive these short-lived snapshots.
    const auto lastIndex = static_cast<std::int64_t>(std::floor(route_.lengthMetres()/chunkLength_));
    if (index > 0) appendParcels(generate(index-1,false));
    if (index < lastIndex) appendParcels(generate(index+1,false));
    const auto overlapsParcel = [&](const WorldFeature& feature) {
        if (feature.kind != FeatureKind::Tree && feature.kind != FeatureKind::Bush) return false;
        const double radius = (feature.kind == FeatureKind::Tree ? 3.1 : 1.2)*feature.scale;
        return std::any_of(parcels.begin(),parcels.end(),[&](const Parcel& parcel) {
            return std::abs(feature.routeDistance-parcel.distance) < parcel.halfLength+radius
                && std::abs(feature.lateralMetres-parcel.lateral) < parcel.halfWidth+radius;
        });
    };
    for (auto& feature : chunk.features)
    {
        if (!overlapsParcel(feature)) continue;
        const WorldFeature original = feature;
        const double radius = (feature.kind == FeatureKind::Tree ? 3.1 : 1.2)*feature.scale;
        bool placed = false;
        for (double outward : {0.0,8.0,16.0})
        {
            for (double along : {-12.0,12.0,-24.0,24.0,-36.0,36.0})
            {
                WorldFeature candidate = original;
                candidate.routeDistance += along;
                candidate.lateralMetres += original.lateralMetres < 0.0 ? -outward : outward;
                if (candidate.routeDistance < chunk.beginDistance+radius
                    || candidate.routeDistance >= chunk.endDistance-radius
                    || overlapsParcel(candidate) || overlapsTerminalForecourt(candidate,route_)) continue;
                const auto candidatePose = route_.sample(candidate.routeDistance);
                if (!terrainFeatureAllowed(candidatePose,candidate.lateralMetres)
                    || (candidatePose.urbanity > 0.28
                        && std::abs(candidate.lateralMetres) < 24.7+radius)) continue;
                const bool crowded = std::any_of(chunk.features.begin(),chunk.features.end(),
                    [&](const WorldFeature& other) {
                        if (&other == &feature || (other.kind != FeatureKind::Tree
                            && other.kind != FeatureKind::Bush)) return false;
                        const double otherRadius = (other.kind == FeatureKind::Tree ? 3.1 : 1.2)*other.scale;
                        return std::hypot(candidate.routeDistance-other.routeDistance,
                                          candidate.lateralMetres-other.lateralMetres)
                            < radius+otherRadius;
                    });
                if (crowded) continue;
                feature = candidate;
                placed = true;
                break;
            }
            if (placed) break;
        }
    }
    std::erase_if(chunk.features,overlapsParcel);
    return chunk;
}

void StreamingWorld::update(double trainDistanceMetres)
{
    const auto maxChunk = static_cast<std::int64_t>(std::floor(route_.lengthMetres() / chunkLength_));
    const auto requested = std::clamp(static_cast<std::int64_t>(std::floor(trainDistanceMetres / chunkLength_)),
                                      std::int64_t{0}, maxChunk);
    if (requested == currentChunk_ && !chunks_.empty()) return;
    currentChunk_ = requested;

    // Preserve the complete active window at route boundaries by shifting it toward the available
    // corridor. A merely clamped centred interval loses chunks near either terminal and can expose
    // a visibly thinner horizon precisely while the train is dwelling and reversing.
    const std::int64_t desiredCount = std::min<std::int64_t>(
        static_cast<std::int64_t>(radius_) * 2 + 1, maxChunk + 1);
    const std::int64_t lastPossibleFirst = maxChunk - desiredCount + 1;
    const std::int64_t first = std::clamp(
        currentChunk_ - static_cast<std::int64_t>(radius_),
        std::int64_t{0}, lastPossibleFirst);
    const std::int64_t last = first + desiredCount - 1;
    nextChunksScratch_.clear();
    for (std::int64_t index = first; index <= last; ++index)
    {
        const auto found = std::find_if(chunks_.begin(), chunks_.end(),
                                       [index](const Chunk& chunk) { return chunk.index == index; });
        if (found != chunks_.end()) nextChunksScratch_.push_back(std::move(*found));
        else
        {
            nextChunksScratch_.push_back(generate(index));
            ++generatedChunkCount_;
        }
    }
    chunks_.swap(nextChunksScratch_);
}

} // namespace TrainSim::World
