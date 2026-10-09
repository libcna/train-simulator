#include "TestHarness.hpp"

#include "trainsim/sim/Route.hpp"
#include "trainsim/sim/RouteSimulation.hpp"
#include "trainsim/world/StreamingWorld.hpp"
#include "trainsim/world/StreetLayout.hpp"
#include "trainsim/world/StationSite.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>

using namespace TrainSim::Sim;
using namespace TrainSim::World;

TEST_CASE("terminal forecourts reserve full decorative footprints without removing railway equipment")
{
    const Route route=Route::NorthernValleyExpress();
    REQUIRE(overlapsTerminalForecourt({FeatureKind::Tree,-20,-22,1,0},route));
    REQUIRE(overlapsTerminalForecourt({FeatureKind::Tree,route.lengthMetres()+20,-22,1,0},route));
    REQUIRE(!overlapsTerminalForecourt({FeatureKind::Tree,-20,22,1,0},route));
    REQUIRE(!overlapsTerminalForecourt({FeatureKind::Signal,-20,-22,1,0},route));
    REQUIRE(!overlapsTerminalForecourt({FeatureKind::Tree,1000,-22,1,0},route));
    StreamingWorld world(route);
    for (double distance : {0.0,route.lengthMetres()})
    {
        world.update(distance);
        for (const auto& chunk : world.chunks())
            for (const auto& feature : chunk.features)
                REQUIRE(!overlapsTerminalForecourt(feature,route));
    }
}

TEST_CASE("residential access reservations share actual door offsets and bounded pavement joins")
{
    for (double side : {-1.0,1.0})
    {
        const auto access=residentialAccess({FeatureKind::Apartment,100,side*50,1,1});
        REQUIRE(access.has_value());
        REQUIRE(std::abs(access->distance-100.62)<1e-8);
        REQUIRE(access->side==side);
        REQUIRE(std::abs(access->facade-44.95)<1e-8);
        REQUIRE(std::abs(access->apronEdge-41.85)<1e-8);
        REQUIRE(access->width==1.62);
        REQUIRE(!residentialAccess({FeatureKind::House,100,side*25,1,0}));
        REQUIRE(!residentialAccess({FeatureKind::House,100,side*110,1,0}));
        REQUIRE(!residentialAccess({FeatureKind::Tree,100,side*50,1,0}));
    }
}

TEST_CASE("paired frontage passages connect only separate same-bank parcels")
{
    std::vector<WorldFeature> features={{FeatureKind::Townhouse,30,30,1,0},
        {FeatureKind::Townhouse,44.4,30.2,1,1},
        {FeatureKind::Townhouse,70,-30,1,0}};
    const auto link=frontageLink(features,features.front());
    REQUIRE(link.has_value());
    REQUIRE(std::abs(link->begin-35.7)<1e-8);
    REQUIRE(std::abs(link->end-38.7)<1e-8);
    REQUIRE(std::abs(link->inner-25.9)<1e-8);
    REQUIRE(std::abs(link->outer-34.3)<1e-8);
    REQUIRE(!frontageLink(features,features[1]));
    features[1].lateralMetres=-30;
    REQUIRE(!frontageLink(features,features.front()));
    features[1].lateralMetres=30;
    features[1].routeDistance=39;
    REQUIRE(!frontageLink(features,features.front()));
}

TEST_CASE("shared street boundaries leave household entrances and crossings unobstructed")
{
    std::vector<WorldFeature> features={{FeatureKind::Townhouse,30,30,1,0},
        {FeatureKind::Townhouse,44.4,30,1,1},
        {FeatureKind::Townhouse,80,30,1,0},
        {FeatureKind::House,60,55,1,0}};
    const auto spans=frontageBoundarySpans(features,1);
    REQUIRE(!spans.empty());
    for (const auto& [a,b] : spans)
    {
        REQUIRE(a>=24.3-1e-8);
        REQUIRE(b<=85.7+1e-8);
        REQUIRE(b>a);
        for (double door : {28.45,45.95,78.45,60.0})
            REQUIRE(door<a || door>b);
    }
    REQUIRE(frontageBoundarySpans(features,-1).empty());
    features.push_back({FeatureKind::LevelCrossing,55,0,1,0});
    for (const auto& [a,b] : frontageBoundarySpans(features,1))
        REQUIRE(b<=39 || a>=71);
}

TEST_CASE("street pockets require an unobstructed gap and deterministic residential neighbours")
{
    std::vector<WorldFeature> features={{FeatureKind::Townhouse,30,30,1,0},
        {FeatureKind::Townhouse,70,30,1,1}};
    const auto first=streetPocket(features,1);
    REQUIRE(first.has_value());
    REQUIRE(first->distance==50);
    REQUIRE(first->lateral==30);
    REQUIRE(!streetPocket(features,-1));
    std::reverse(features.begin(),features.end());
    REQUIRE(streetPocket(features,1)->distance==first->distance);
    features.push_back({FeatureKind::Tree,50,34,1,0});
    REQUIRE(!streetPocket(features,1));
    features.pop_back();
    features.push_back({FeatureKind::House,50,70,1,0});
    REQUIRE(!streetPocket(features,1)); // The distant house itself is clear; its path is not.
    features.back().lateralMetres=-70;
    REQUIRE(streetPocket(features,1).has_value());
    features.pop_back();
    features[0].routeDistance=48;
    REQUIRE(!streetPocket(features,1));
}

TEST_CASE("court entrance reservations account for side offsets and the rendered path range")
{
    for (double side : {-1.0,1.0})
    {
        const StreetPocket pocket{50,side*30};
        REQUIRE(streetPocketEntranceConflict(pocket,{FeatureKind::Apartment,50,side*70,1,0}));
        REQUIRE(streetPocketEntranceConflict(pocket,{FeatureKind::Tower,50,side*70,1,0}));
        REQUIRE(!streetPocketEntranceConflict(pocket,{FeatureKind::House,70,side*70,1,0}));
        REQUIRE(!streetPocketEntranceConflict(pocket,{FeatureKind::House,50,-side*70,1,0}));
        REQUIRE(!streetPocketEntranceConflict(pocket,{FeatureKind::House,50,side*110,1,0}));
    }
}

TEST_CASE("streamed feature records retain a compact cache-friendly layout")
{
    REQUIRE(sizeof(WorldFeature) <= 32);
    constexpr WorldFeature signal{FeatureKind::Signal, 230.0, -3.4, 1.0, 7U};
    REQUIRE(signal.kind == FeatureKind::Signal);
    REQUIRE(signal.routeDistance == 230.0);
    REQUIRE(signal.lateralMetres == -3.4);
    REQUIRE(signal.variant == 7U);
}

TEST_CASE("streamer keeps a bounded corridor around the train")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route, 250.0, 3);
    constexpr std::size_t activeWindow = 7;
    const auto maximumChunk = static_cast<std::int64_t>(
        std::floor(route.lengthMetres()/world.chunkLength()));
    for (double distance = 0.0; distance < route.lengthMetres(); distance += 193.0)
    {
        world.update(distance);
        REQUIRE(world.chunks().size() == activeWindow);
        REQUIRE(world.chunks().front().index >= 0);
        REQUIRE(world.chunks().back().index <= maximumChunk);
        REQUIRE(world.chunks().front().index <= world.currentChunk());
        REQUIRE(world.chunks().back().index >= world.currentChunk());
        for (std::size_t index = 1; index < world.chunks().size(); ++index)
            REQUIRE(world.chunks()[index].index == world.chunks()[index-1].index+1);
    }

    world.update(route.lengthMetres());
    REQUIRE(world.chunks().size() == activeWindow);
    REQUIRE(world.chunks().back().index == maximumChunk);
}

TEST_CASE("procedural chunks are deterministic across streamers")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld a(route, 250.0, 3);
    StreamingWorld b(route, 250.0, 3);
    a.update(2600.0);
    b.update(3300.0);
    b.update(2600.0);
    REQUIRE(a.chunks().size() == b.chunks().size());
    for (std::size_t i = 0; i < a.chunks().size(); ++i)
    {
        REQUIRE(a.chunks()[i].index == b.chunks()[i].index);
        REQUIRE(a.chunks()[i].features.size() == b.chunks()[i].features.size());
        for (std::size_t j = 0; j < a.chunks()[i].features.size(); ++j)
        {
            REQUIRE(a.chunks()[i].features[j].kind == b.chunks()[i].features[j].kind);
            REQUIRE(a.chunks()[i].features[j].routeDistance
                    == b.chunks()[i].features[j].routeDistance);
            REQUIRE(a.chunks()[i].features[j].lateralMetres
                    == b.chunks()[i].features[j].lateralMetres);
            REQUIRE(a.chunks()[i].features[j].scale == b.chunks()[i].features[j].scale);
            REQUIRE(a.chunks()[i].features[j].variant == b.chunks()[i].features[j].variant);
        }
    }
}

TEST_CASE("full route generation contains the intended scenery families")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route, 250.0, 1);
    std::array<bool, static_cast<std::size_t>(FeatureKind::Count)> seen{};
    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 250.0)
    {
        world.update(distance);
        for (const Chunk& chunk : world.chunks())
            for (const WorldFeature& feature : chunk.features)
                seen[static_cast<std::size_t>(feature.kind)] = true;
    }
    for (bool present : seen) REQUIRE(present);
}

TEST_CASE("catenary masts remain outside both running lines")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route, 250.0, 3);
    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 250.0)
    {
        world.update(distance);
        for (const Chunk& chunk : world.chunks())
            for (const WorldFeature& feature : chunk.features)
            {
                if (feature.kind != FeatureKind::CatenaryPole) continue;
                const double parallel = route.parallelTrackLateralMetres(
                    feature.routeDistance);
                REQUIRE(feature.lateralMetres >= 4.29
                    || feature.lateralMetres <= parallel - 4.29);
            }
    }
}

TEST_CASE("rural corridor includes near vegetation and distant woodland bands")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route, 250.0, 3);
    world.update(52'000.0);

    int nearTrees = 0;
    int distantTrees = 0;
    int boundedNearTrees = 0;
    for (const Chunk& chunk : world.chunks())
        for (const WorldFeature& feature : chunk.features)
        {
            if (feature.kind != FeatureKind::Tree) continue;
            const double lateral = std::abs(feature.lateralMetres);
            if (lateral < 80.0)
            {
                ++nearTrees;
                if (!route.sample(feature.routeDistance).waterCrossing)
                {
                    const double closeScaleLimit = 1.34
                        + std::clamp((lateral-11.0)/34.0,0.0,1.0)*0.78;
                    REQUIRE(feature.scale <= closeScaleLimit+1.0e-9);
                    REQUIRE(lateral-2.30*feature.scale >= 8.8-1.0e-9);
                    ++boundedNearTrees;
                }
            }
            if (std::abs(feature.lateralMetres) >= 85.0) ++distantTrees;
        }

    REQUIRE(nearTrees >= 100);
    REQUIRE(boundedNearTrees >= 100);
    REQUIRE(distantTrees >= 50);
}

TEST_CASE("rural woodland forms bounded repeatable copses without adding trees")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route,250.0,3);
    StreamingWorld reloaded(route,250.0,3);
    for (double distance : {52'000.0,74'500.0,177'900.0})
    {
        world.update(distance);
        reloaded.update(distance+4'000.0);
        reloaded.update(distance);
        int trees=0, grouped=0;
        for (std::size_t c=0;c<world.chunks().size();++c)
        {
            const auto& chunk=world.chunks()[c];
            const auto& restored=reloaded.chunks()[c];
            REQUIRE(chunk.features.size()==restored.features.size());
            int chunkTrees=0;
            for (std::size_t i=0;i<chunk.features.size();++i)
            {
                const auto& tree=chunk.features[i];
                REQUIRE(tree.routeDistance==restored.features[i].routeDistance);
                REQUIRE(tree.lateralMetres==restored.features[i].lateralMetres);
                if (tree.kind!=FeatureKind::Tree) continue;
                ++trees; ++chunkTrees;
                REQUIRE(tree.routeDistance>=chunk.beginDistance);
                REQUIRE(tree.routeDistance<=chunk.endDistance);
                REQUIRE(terrainFeatureAllowed(route.sample(tree.routeDistance),tree.lateralMetres));
                if (std::any_of(chunk.features.begin(),chunk.features.end(),[&](const auto& other) {
                    return &other!=&tree && other.kind==FeatureKind::Tree
                        && std::hypot(tree.routeDistance-other.routeDistance,
                                      tree.lateralMetres-other.lateralMetres)<22.0;
                })) ++grouped;
            }
            REQUIRE(chunkTrees<=39); // Existing near + distant candidate budget.
        }
        REQUIRE(trees>0);
        REQUIRE(grouped>trees/2);
    }
}

TEST_CASE("rural field parcels remain visible on both sides of the passenger view")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route,250.0,3);
    world.update(52'000.0);

    int leftFields = 0;
    int rightFields = 0;
    for (const Chunk& chunk : world.chunks())
        for (const WorldFeature& feature : chunk.features)
        {
            if (feature.kind != FeatureKind::FieldCrop) continue;
            REQUIRE(terrainFeatureAllowed(
                route.sample(feature.routeDistance),feature.lateralMetres));
            if (feature.lateralMetres < 0.0) ++leftFields;
            else ++rightFields;
        }

    REQUIRE(leftFields >= 5);
    REQUIRE(rightFields >= 5);
    REQUIRE(leftFields+rightFields >= 16);
}

TEST_CASE("terrain anchored scenery respects tunnel channel and viaduct clearances")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route, 250.0, 1);
    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 250.0)
    {
        world.update(distance);
        for (const Chunk& chunk : world.chunks())
            for (const WorldFeature& feature : chunk.features)
            {
                const bool terrainAnchored = feature.kind == FeatureKind::GrassPatch
                    || feature.kind == FeatureKind::FieldCrop
                    || feature.kind == FeatureKind::HayBale
                    || feature.kind == FeatureKind::Tree
                    || feature.kind == FeatureKind::Bush
                    || feature.kind == FeatureKind::House
                    || feature.kind == FeatureKind::Townhouse
                    || feature.kind == FeatureKind::Apartment
                    || feature.kind == FeatureKind::Tower
                    || feature.kind == FeatureKind::Warehouse
                    || feature.kind == FeatureKind::Farmstead
                    || feature.kind == FeatureKind::PowerPylon
                    || feature.kind == FeatureKind::Substation
                    || feature.kind == FeatureKind::RoadVehicle;
                if (!terrainAnchored) continue;
                const RoutePose pose = route.sample(feature.routeDistance);
                if (feature.kind == FeatureKind::RoadVehicle)
                {
                    REQUIRE(!pose.tunnel);
                    REQUIRE(!pose.bridge);
                }
                else
                {
                    REQUIRE(terrainFeatureAllowed(pose,feature.lateralMetres));
                }
            }
    }
}

TEST_CASE("rural straw stacks are sparse deterministic and terrain anchored")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route,250.0,1);
    std::set<double> bales;
    bool sawPositive = false;
    bool sawNegative = false;
    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 250.0)
    {
        world.update(distance);
        for (const Chunk& chunk : world.chunks())
            for (const WorldFeature& feature : chunk.features)
            {
                if (feature.kind != FeatureKind::HayBale) continue;
                const RoutePose pose = route.sample(feature.routeDistance);
                REQUIRE(terrainFeatureAllowed(pose,feature.lateralMetres));
                REQUIRE(!pose.tunnel);
                REQUIRE(!pose.bridge);
                REQUIRE(std::abs(feature.lateralMetres) >= 31.0);
                REQUIRE(std::abs(feature.lateralMetres) <= 56.0);
                REQUIRE(feature.scale >= 0.82);
                REQUIRE(feature.scale <= 1.12);
                for (const Station& station : route.stations())
                    REQUIRE(std::abs(station.distanceMetres-feature.routeDistance) >= 500.0);
                sawPositive = sawPositive || feature.lateralMetres > 0.0;
                sawNegative = sawNegative || feature.lateralMetres < 0.0;
                bales.insert(feature.routeDistance);
            }
    }
    REQUIRE(bales.size() >= 30);
    REQUIRE(bales.size() < 160);
    REQUIRE(sawPositive);
    REQUIRE(sawNegative);
}

TEST_CASE("Lakehurst procedural scenery leaves the adjacent water basin clear")
{
    const Route route = Route::NorthernValleyExpress();
    const RoutePose lakeside = route.sample(84'000.0);
    REQUIRE(lakeside.adjacentWaterSide == 1);
    REQUIRE(terrainFeatureAllowed(lakeside,-120.0));
    REQUIRE(!terrainFeatureAllowed(lakeside,120.0));
    REQUIRE(!terrainFeatureAllowed(lakeside,420.0));
    REQUIRE(terrainFeatureAllowed(lakeside,600.0));
}

TEST_CASE("urban traffic keeps a coherent road and building setback")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route, 250.0, 1);
    bool sawTraffic = false;
    bool sawTrafficSide[2] = {false,false};
    bool sawPositiveBuilding = false;
    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 250.0)
    {
        world.update(distance);
        for (const Chunk& chunk : world.chunks())
        {
            const bool urbanRoadChunk = route.sample(
                (chunk.beginDistance+chunk.endDistance)*0.5).urbanity > 0.28;
            for (const WorldFeature& feature : chunk.features)
            {
                if (feature.kind == FeatureKind::RoadVehicle)
                {
                    const double roadSide = feature.variant < 3U ? 1.0 : -1.0;
                    REQUIRE(feature.lateralMetres*roadSide > 0.0);
                    const double laneMagnitude = std::abs(feature.lateralMetres);
                    if (feature.variant % 2U == 0U)
                    {
                        REQUIRE(laneMagnitude >= 17.125);
                        REQUIRE(laneMagnitude <= 17.575);
                    }
                    else
                    {
                        REQUIRE(laneMagnitude >= 19.025);
                        REQUIRE(laneMagnitude <= 19.475);
                    }
                    sawTrafficSide[feature.lateralMetres > 0.0 ? 1 : 0] = true;
                    sawTraffic = true;
                }
                if (urbanRoadChunk
                    && (feature.kind == FeatureKind::Tree
                        || feature.kind == FeatureKind::Bush))
                    REQUIRE(std::abs(feature.lateralMetres) >= 26.2);
                const bool building = feature.kind == FeatureKind::House
                    || feature.kind == FeatureKind::Townhouse
                    || feature.kind == FeatureKind::Apartment
                    || feature.kind == FeatureKind::Tower
                    || feature.kind == FeatureKind::Warehouse
                    || feature.kind == FeatureKind::Farmstead;
                if (building && feature.lateralMetres > 0.0)
                {
                    REQUIRE(feature.lateralMetres
                            >= (feature.kind == FeatureKind::Townhouse ? 27.0 : 32.0));
                    sawPositiveBuilding = true;
                }
                if (building)
                {
                    const double halfWidth = feature.kind == FeatureKind::Warehouse
                        ? 8.5 * feature.scale
                        : feature.kind == FeatureKind::Farmstead ? 14.0 * feature.scale
                        : feature.kind == FeatureKind::Tower ? 7.25 * feature.scale
                        : feature.kind == FeatureKind::Apartment ? 5.0 * feature.scale
                        : feature.kind == FeatureKind::Townhouse ? 3.0 * feature.scale
                                                                 : 3.5 * feature.scale;
                    REQUIRE(feature.scale >= 0.75);
                    REQUIRE(feature.scale <= 1.65);
                    REQUIRE(std::abs(feature.lateralMetres) - halfWidth >= 15.0);
                }
            }
        }
    }
    REQUIRE(sawTraffic);
    REQUIRE(sawTrafficSide[0]);
    REQUIRE(sawTrafficSide[1]);
    REQUIRE(sawPositiveBuilding);
}

TEST_CASE("urban warehouses form bounded deterministic industrial compounds")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route,250.0,1);
    std::set<double> compoundLeaders;
    bool sawTwoBuildingCompound = false;
    bool sawThreeBuildingCompound = false;

    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 250.0)
    {
        world.update(distance);
        for (const Chunk& chunk : world.chunks())
            for (const WorldFeature& leader : chunk.features)
            {
                if (leader.kind != FeatureKind::Warehouse
                    || (leader.variant&kWarehouseCompoundLeader) == 0U
                    || !compoundLeaders.insert(leader.routeDistance).second) continue;

                const int expectedCount =
                    (leader.variant&kWarehouseThreeBuildingCompound) != 0U ? 3 : 2;
                sawTwoBuildingCompound = sawTwoBuildingCompound || expectedCount == 2;
                sawThreeBuildingCompound = sawThreeBuildingCompound || expectedCount == 3;
                const double spacing = 30.0*leader.scale;
                const double compoundCentre = leader.routeDistance
                    + 0.5*static_cast<double>(expectedCount-1)*spacing;
                for (const auto& frontage : chunk.features)
                {
                    if (frontage.kind != FeatureKind::Townhouse
                        || frontage.lateralMetres*leader.lateralMetres <= 0.0) continue;
                    REQUIRE(std::abs(frontage.routeDistance-compoundCentre) >= 58.0);
                }
                for (int slot = 0; slot < expectedCount; ++slot)
                {
                    const double expectedDistance = leader.routeDistance
                        + static_cast<double>(slot)*spacing;
                    const auto warehouse = std::find_if(
                        chunk.features.begin(),chunk.features.end(),
                        [&](const WorldFeature& candidate) {
                            return candidate.kind == FeatureKind::Warehouse
                                && std::abs(candidate.routeDistance-expectedDistance) < 1.0e-6
                                && std::abs(candidate.lateralMetres-leader.lateralMetres) < 1.0e-6
                                && candidate.scale >= leader.scale*0.93
                                && candidate.scale <= leader.scale*1.07
                                && (candidate.variant&3U) == (leader.variant&3U);
                        });
                    REQUIRE(warehouse != chunk.features.end());
                }
                REQUIRE(std::abs(leader.lateralMetres)-8.5*leader.scale >= 32.0);
            }
    }

    REQUIRE(compoundLeaders.size() >= 20);
    REQUIRE(sawTwoBuildingCompound);
    REQUIRE(sawThreeBuildingCompound);
}

TEST_CASE("major city approaches contain a bounded two-sided streetfront")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route,250.0,3);
    world.update(191'000.0);

    int townhouses = 0;
    int pairedFrontages = 0;
    bool sawPositive = false;
    bool sawNegative = false;
    for (const Chunk& chunk : world.chunks())
    {
        int perChunk = 0;
        for (const WorldFeature& feature : chunk.features)
        {
            if (feature.kind != FeatureKind::Townhouse) continue;
            ++townhouses;
            ++perChunk;
            sawPositive = sawPositive || feature.lateralMetres > 0.0;
            sawNegative = sawNegative || feature.lateralMetres < 0.0;
            const double innerEdge = std::abs(feature.lateralMetres) - 4.3 * feature.scale;
            REQUIRE(innerEdge >= 25.5);
            REQUIRE(innerEdge <= 26.0);
            bool hasPair=false;
            for (const WorldFeature& neighbour : chunk.features)
            {
                if (&neighbour == &feature || neighbour.kind != FeatureKind::Townhouse
                    || neighbour.lateralMetres*feature.lateralMetres <= 0.0) continue;
                REQUIRE(std::abs(neighbour.routeDistance-feature.routeDistance)
                        > 5.7*(neighbour.scale+feature.scale));
                hasPair=hasPair || std::abs(neighbour.routeDistance-feature.routeDistance)<16.0;
            }
            pairedFrontages+=hasPair;
        }
        REQUIRE(perChunk <= 15);
    }
    REQUIRE(townhouses >= 60);
    REQUIRE(pairedFrontages>townhouses/2);
    REQUIRE(sawPositive);
    REQUIRE(sawNegative);
}

TEST_CASE("generated tree crowns stay outside building parcels across chunk seams")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route,250.0,1);
    int checked = 0;
    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 500.0)
    {
        world.update(distance);
        for (const auto& chunk : world.chunks())
            for (const auto& tree : chunk.features)
            {
                if (tree.kind != FeatureKind::Tree) continue;
                ++checked;
                for (const auto& neighbour : world.chunks())
                for (const auto& house : neighbour.features)
                {
                    double halfWidth = 0.0, halfLength = 0.0;
                    switch (house.kind)
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
                    const double crown = 3.1*tree.scale;
                    REQUIRE(std::abs(tree.lateralMetres-house.lateralMetres) >= halfWidth*house.scale+crown
                         || std::abs(tree.routeDistance-house.routeDistance) >= halfLength*house.scale+crown);
                    const auto accessPose=route.sample(house.routeDistance);
                    if (accessPose.urbanity>0.45 && !accessPose.bridge && !accessPose.tunnel)
                        if (const auto access=residentialAccess(house))
                            REQUIRE(std::abs(tree.lateralMetres-access->side*(24.6+access->facade)*0.5)
                                        >=(access->facade-24.6)*0.5+crown
                                || std::abs(tree.routeDistance-access->distance)>=access->width*0.5+1.1+crown);
                }
            }
    }
    REQUIRE(checked > 10);
}

TEST_CASE("major city approaches contain a sparse distant tower layer")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route,250.0,3);
    world.update(191'000.0);

    int towers = 0;
    bool sawPositive = false;
    bool sawNegative = false;
    for (const Chunk& chunk : world.chunks())
        for (const WorldFeature& feature : chunk.features)
        {
            if (feature.kind != FeatureKind::Tower) continue;
            ++towers;
            REQUIRE(std::abs(feature.lateralMetres) >= 92.0);
            REQUIRE(std::abs(feature.lateralMetres) <= 188.0);
            REQUIRE(feature.scale >= 0.82);
            REQUIRE(feature.scale <= 1.25);
            sawPositive = sawPositive || feature.lateralMetres > 0.0;
            sawNegative = sawNegative || feature.lateralMetres < 0.0;
        }
    REQUIRE(towers >= 7);
    REQUIRE(towers <= 21);
    REQUIRE(sawPositive);
    REQUIRE(sawNegative);
}

TEST_CASE("rural farmsteads are sparse deterministic and clear of the railway")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route,250.0,1);
    std::set<double> farms;
    bool sawPositive = false;
    bool sawNegative = false;
    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 250.0)
    {
        world.update(distance);
        for (const Chunk& chunk : world.chunks())
            for (const WorldFeature& feature : chunk.features)
            {
                if (feature.kind != FeatureKind::Farmstead) continue;
                const RoutePose pose = route.sample(feature.routeDistance);
                REQUIRE(terrainFeatureAllowed(pose,feature.lateralMetres));
                REQUIRE(std::abs(feature.lateralMetres)-14.0*feature.scale >= 30.0);
                REQUIRE(feature.scale >= 0.90);
                REQUIRE(feature.scale <= 1.18);
                sawPositive = sawPositive || feature.lateralMetres > 0.0;
                sawNegative = sawNegative || feature.lateralMetres < 0.0;
                farms.insert(feature.routeDistance);
            }
    }
    REQUIRE(farms.size() >= 8);
    REQUIRE(farms.size() < 24);
    REQUIRE(sawPositive);
    REQUIRE(sawNegative);
}

TEST_CASE("road overpasses remain sparse and clear of stations and authored crossings")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route,250.0,1);
    std::set<double> overpassDistances;
    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 250.0)
    {
        world.update(distance);
        for (const Chunk& chunk : world.chunks())
            for (const WorldFeature& feature : chunk.features)
            {
                if (feature.kind != FeatureKind::RoadOverpass) continue;
                const RoutePose pose = route.sample(feature.routeDistance);
                REQUIRE(!pose.tunnel);
                REQUIRE(!pose.bridge);
                REQUIRE(feature.routeDistance >= chunk.beginDistance);
                REQUIRE(feature.routeDistance <= chunk.endDistance);
                for (const Station& station : route.stations())
                    REQUIRE(std::abs(station.distanceMetres-feature.routeDistance) >= 900.0);
                overpassDistances.insert(feature.routeDistance);
            }
    }
    REQUIRE(overpassDistances.size() >= 4);
    REQUIRE(overpassDistances.size() < 12);
}

TEST_CASE("level crossings are deterministic rural features clear of stations")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route,250.0,1);
    std::set<double> crossingDistances;
    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 250.0)
    {
        world.update(distance);
        for (const Chunk& chunk : world.chunks())
            for (const WorldFeature& feature : chunk.features)
            {
                if (feature.kind != FeatureKind::LevelCrossing) continue;
                const RoutePose pose = route.sample(feature.routeDistance);
                REQUIRE(!pose.tunnel);
                REQUIRE(!pose.bridge);
                REQUIRE(feature.lateralMetres == 0.0);
                REQUIRE(feature.routeDistance >= chunk.beginDistance);
                REQUIRE(feature.routeDistance <= chunk.endDistance);
                for (const Station& station : route.stations())
                    REQUIRE(std::abs(station.distanceMetres-feature.routeDistance) >= 650.0);
                crossingDistances.insert(feature.routeDistance);
            }
    }
    REQUIRE(crossingDistances.size() >= 25);
    REQUIRE(crossingDistances.size() < 40);

    REQUIRE(levelCrossingClosureAmount(7'375.0,7'375.0) == 1.0);
    REQUIRE(levelCrossingClosureAmount(7'375.0,7'045.0) == 1.0);
    const double transition = levelCrossingClosureAmount(7'375.0,6'950.0);
    REQUIRE(transition > 0.0);
    REQUIRE(transition < 1.0);
    REQUIRE(levelCrossingClosureAmount(7'375.0,6'855.0) == 0.0);
    REQUIRE(levelCrossingClosureAmount(7'375.0,7'800.0) == transition);
    REQUIRE(levelCrossingClosureAmount(
        std::numeric_limits<double>::quiet_NaN(),7'375.0) == 0.0);

    const WorldFeature aiTrain{FeatureKind::OtherTrain,268'330.0,-5.0,1.0,2U};
    const double aiDistance = movingOtherTrainDistance(aiTrain,0.0,route.lengthMetres());
    REQUIRE(levelCrossingClosureAmount(268'375.0,267'575.0) == 0.0);
    REQUIRE(levelCrossingClosureAmount(268'375.0,aiDistance) > 0.0);

    StreamingWorld trafficWorld(route,250.0,3);
    trafficWorld.update(267'575.0);
    REQUIRE(trafficWorld.requiredLevelCrossingClosure(
        268'375.0,267'575.0,0.0) > 0.0);
    REQUIRE(trafficWorld.requiredLevelCrossingClosure(7'375.0,7'375.0,0.0) == 1.0);
    REQUIRE(trafficWorld.requiredLevelCrossingClosure(
        std::numeric_limits<double>::quiet_NaN(),7'375.0,0.0) == 0.0);
}

TEST_CASE("block signals alternate direction and use their documented track side")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route, 250.0, 1);
    std::array<bool, 2> sawDirection{};
    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 250.0)
    {
        world.update(distance);
        for (const Chunk& chunk : world.chunks())
            for (const WorldFeature& feature : chunk.features)
            {
                if (feature.kind != FeatureKind::Signal) continue;
                REQUIRE(feature.variant <= 1U);
                const int facingDirection = feature.variant == 0 ? 1 : -1;
                REQUIRE(feature.lateralMetres * static_cast<double>(facingDirection) > 0.0);
                REQUIRE(feature.routeDistance >= chunk.beginDistance);
                REQUIRE(feature.routeDistance <= chunk.endDistance);
                sawDirection[feature.variant] = true;
            }
    }
    REQUIRE(sawDirection[0]);
    REQUIRE(sawDirection[1]);
}

TEST_CASE("power corridors and substations remain sparse and clear of the railway")
{
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route, 250.0, 1);
    std::set<double> pylonDistances;
    std::set<double> substationDistances;
    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 250.0)
    {
        world.update(distance);
        for (const Chunk& chunk : world.chunks())
            for (const WorldFeature& feature : chunk.features)
            {
                if (feature.kind == FeatureKind::PowerPylon)
                {
                    REQUIRE(std::abs(feature.lateralMetres) >= 75.0);
                    pylonDistances.insert(feature.routeDistance);
                }
                else if (feature.kind == FeatureKind::Substation)
                {
                    REQUIRE(std::abs(feature.lateralMetres) >= 35.0);
                    for (const WorldFeature& neighbour : chunk.features)
                    {
                        const bool building = neighbour.kind == FeatureKind::House
                            || neighbour.kind == FeatureKind::Townhouse
                            || neighbour.kind == FeatureKind::Apartment
                            || neighbour.kind == FeatureKind::Tower
                            || neighbour.kind == FeatureKind::Warehouse
                            || neighbour.kind == FeatureKind::Farmstead;
                        if (!building
                            || std::abs(neighbour.routeDistance - feature.routeDistance) >= 45.0)
                            continue;
                        REQUIRE(neighbour.lateralMetres * feature.lateralMetres < 0.0);
                    }
                    substationDistances.insert(feature.routeDistance);
                }
            }
    }
    REQUIRE(pylonDistances.size() >= 20);
    REQUIRE(substationDistances.size() >= 4);
    REQUIRE(pylonDistances.size() < 100);
    REQUIRE(substationDistances.size() < 20);
}

TEST_CASE("block signal aspects follow occupancy departure and station approach state")
{
    const Route route = Route::NorthernValleyExpress();
    ServiceState service;
    const WorldFeature eastbound{FeatureKind::Signal, 230.0, 3.4, 1.0, 0U};

    service.direction = 1;
    service.routeDistanceMetres = 0.0;
    service.nextStation = 1;
    service.phase = ServicePhase::Dwelling;
    REQUIRE(signalAspect(eastbound, route, service) == SignalAspect::Red);
    const WorldFeature nextBlock{FeatureKind::Signal, 730.0, 3.4, 1.0, 0U};
    REQUIRE(signalAspect(nextBlock, route, service) == SignalAspect::Green);

    service.phase = ServicePhase::Accelerating;
    REQUIRE(signalAspect(eastbound, route, service) == SignalAspect::Green);

    service.routeDistanceMetres = 300.0;
    REQUIRE(signalAspect(eastbound, route, service) == SignalAspect::Red);
    service.routeDistanceMetres = 800.0;
    REQUIRE(signalAspect(eastbound, route, service) == SignalAspect::Green);

    const WorldFeature stationApproach{FeatureKind::Signal, 18'230.0, 3.4, 1.0, 0U};
    service.routeDistanceMetres = 17'400.0;
    service.phase = ServicePhase::Braking;
    REQUIRE(signalAspect(stationApproach, route, service) == SignalAspect::Yellow);

    const WorldFeature opposite{FeatureKind::Signal, 230.0, -3.4, 1.0, 1U};
    REQUIRE(signalAspect(opposite, route, service) == SignalAspect::Green);
}

TEST_CASE("moving other train position is deterministic finite and route bounded")
{
    const WorldFeature train{FeatureKind::OtherTrain, 91'000.0, -5.0, 1.0, 2U};
    const double first = movingOtherTrainDistance(train,12.5,286'400.0);
    const double repeated = movingOtherTrainDistance(train,12.5,286'400.0);
    const double later = movingOtherTrainDistance(train,13.5,286'400.0);
    REQUIRE(first == repeated);
    REQUIRE(first != later);
    REQUIRE(first >= 0.0 && first <= 286'400.0);
    REQUIRE(later >= 0.0 && later <= 286'400.0);
    REQUIRE(std::isfinite(movingOtherTrainDistance(
        train,std::numeric_limits<double>::quiet_NaN(),286'400.0)));

    const WorldFeature lifecycleTrain{FeatureKind::OtherTrain,91'000.0,-5.0,1.0,0U};
    REQUIRE(movingOtherTrainVisible(lifecycleTrain,0.0,286'400.0));
    REQUIRE(movingOtherTrainVisible(lifecycleTrain,61.0,286'400.0));
    REQUIRE(!movingOtherTrainVisible(lifecycleTrain,62.0,286'400.0));
    REQUIRE(!movingOtherTrainVisible(lifecycleTrain,70.0,286'400.0));
    REQUIRE(movingOtherTrainVisible(lifecycleTrain,72.0,286'400.0));
    REQUIRE(std::abs(movingOtherTrainDistance(lifecycleTrain,1.0,286'400.0)
                     - movingOtherTrainDistance(lifecycleTrain,0.0,286'400.0) + 31.0) < 1e-9);

    // The only discontinuity is the reset from the far trailing boundary back to the leading
    // boundary, and both samples around it belong to the explicit hidden interval.
    constexpr double resetBeforeSeconds = 61.5;
    constexpr double resetAfterSeconds = 70.5;
    REQUIRE(!movingOtherTrainVisible(lifecycleTrain,resetBeforeSeconds,286'400.0));
    REQUIRE(!movingOtherTrainVisible(lifecycleTrain,resetAfterSeconds,286'400.0));

    const WorldFeature endpointTrain{FeatureKind::OtherTrain,100.0,-5.0,1.0,0U};
    REQUIRE(!movingOtherTrainVisible(endpointTrain,61.0,286'400.0));

    // Train motion/audio identity must not depend on how many decorative RNG draws preceded it.
    // Chunk 37 is also the deterministic pass used by the real OpenGL/audio smoke test. That
    // runtime starts the train centre 48 m ahead so this feature is abreast of the player in the
    // rear passenger car on the first audio update, independent of rendering throughput.
    const Route route = Route::NorthernValleyExpress();
    StreamingWorld world(route,250.0,1);
    world.update(9'250.0);
    bool foundClosePass = false;
    for (const Chunk& chunk : world.chunks())
        for (const WorldFeature& feature : chunk.features)
            if (feature.kind == FeatureKind::OtherTrain)
            {
                REQUIRE(feature.variant == static_cast<std::uint32_t>(chunk.index % 3));
                if (chunk.index == 37)
                {
                    REQUIRE(feature.variant == 1U);
                    foundClosePass = true;
                }
            }
    REQUIRE(foundClosePass);
}
