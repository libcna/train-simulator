#include "TestHarness.hpp"

#include "trainsim/sim/Route.hpp"
#include "trainsim/sim/RouteSimulation.hpp"

#include <cmath>

using namespace TrainSim::Sim;

TEST_CASE("geometry-only relative sampling matches full route metadata sampling")
{
    const Route route = Route::NorthernValleyExpress();
    const auto extended = [&](double distance) {
        auto pose = route.sample(distance);
        const double extra = distance-pose.distance;
        if (extra != 0)
        {
            const auto probe = route.sample(pose.distance <= 0 ? 1 : route.lengthMetres()-1);
            pose.elevation += (probe.elevation-pose.elevation)/(probe.distance-pose.distance)*extra;
            pose.x += std::tan(pose.headingRadians)*extra;
            pose.distance += extra;
        }
        return pose;
    };
    for (double reference : {-120.0,0.0,52'000.0,88'700.0,124'000.0,286'400.0,286'520.0})
        for (double offset : {-500.0,-20.0,0.0,20.0,500.0})
        {
            const auto a = extended(reference), b = extended(reference+offset);
            const auto actual = route.relativePose(reference,reference+offset);
            const double dx=b.x-a.x, dz=b.distance-a.distance;
            REQUIRE(std::abs(actual.x-(dx*std::cos(a.headingRadians)-dz*std::sin(a.headingRadians))) < 1e-9);
            REQUIRE(std::abs(actual.z-(dx*std::sin(a.headingRadians)+dz*std::cos(a.headingRadians))) < 1e-9);
            REQUIRE(std::abs(actual.elevation-(b.elevation-a.elevation)) < 1e-9);
            const double angle=b.headingRadians-a.headingRadians;
            REQUIRE(std::abs(actual.headingRadians-std::atan2(std::sin(angle),std::cos(angle))) < 1e-9);
        }
}

TEST_CASE("fictional route is ordered and has intercity scale")
{
    const Route route = Route::NorthernValleyExpress();
    REQUIRE(route.stations().size() == 12);
    REQUIRE(route.lengthMetres() > 250'000.0);
    REQUIRE(route.lengthMetres() < 350'000.0);
    REQUIRE(route.stations().front().name == "Alderwick Central");
    REQUIRE(route.stations().back().name == "Westhaven Central");
    REQUIRE(route.scheduledOneWaySeconds() > 134.0 * 60.0);
    REQUIRE(route.scheduledOneWaySeconds() < 136.0 * 60.0);
    bool saw120 = false;
    bool saw140 = false;
    bool saw160 = false;
    for (std::size_t index = 1; index < route.stations().size(); ++index)
    {
        const int kilometresPerHour = static_cast<int>(
            std::round(route.stations()[index].speedLimitMetresPerSecondFromPrevious * 3.6));
        saw120 = saw120 || kilometresPerHour == 120;
        saw140 = saw140 || kilometresPerHour == 140;
        saw160 = saw160 || kilometresPerHour == 160;
    }
    REQUIRE(saw120 && saw140 && saw160);
}

TEST_CASE("route sampler remains finite for the complete corridor")
{
    const Route route = Route::NorthernValleyExpress();
    for (double distance = 0.0; distance <= route.lengthMetres(); distance += 137.0)
    {
        const RoutePose pose = route.sample(distance);
        REQUIRE(std::isfinite(pose.x));
        REQUIRE(std::isfinite(pose.elevation));
        REQUIRE(std::isfinite(pose.headingRadians));
        REQUIRE(pose.urbanity >= 0.0 && pose.urbanity <= 1.0);
    }
}

TEST_CASE("authored city stations create broad urban approaches")
{
    const Route route = Route::NorthernValleyExpress();
    REQUIRE(route.stations()[7].name == "Meadow Junction");
    REQUIRE(route.stations()[7].major);
    REQUIRE(route.stations()[8].name == "Northbridge");
    REQUIRE(route.stations()[8].major);
    REQUIRE(route.sample(190'000.0).urbanity > 0.65);
    REQUIRE(route.sample(192'400.0).urbanity > 0.99);
}

TEST_CASE("parallel railway alignment widens around left-side island platforms")
{
    const Route route = Route::NorthernValleyExpress();
    REQUIRE(std::abs(route.parallelTrackLateralMetres(30'000.0) + 5.0) < 1.0e-9);
    REQUIRE(route.stations()[2].platformOnLeft);
    REQUIRE(route.parallelTrackLateralMetres(route.stations()[2].distanceMetres) < -8.9);
    REQUIRE(std::abs(route.parallelTrackLateralMetres(
        route.stations()[2].distanceMetres + 700.0) + 5.0) < 1.0e-9);
}

TEST_CASE("authored crossings distinguish water bridge and dry valley viaduct")
{
    const Route route = Route::NorthernValleyExpress();
    const RoutePose river = route.sample(74'500.0);
    REQUIRE(river.bridge);
    REQUIRE(river.waterCrossing);
    REQUIRE(river.waterCrossingSurfaceElevation < river.elevation);
    REQUIRE(std::abs(route.sample(74'000.0).waterCrossingSurfaceElevation
                     - route.sample(75'000.0).waterCrossingSurfaceElevation) < 1.0e-9);

    const RoutePose valley = route.sample(178'500.0);
    REQUIRE(valley.bridge);
    REQUIRE(!valley.waterCrossing);
    REQUIRE(valley.waterCrossingSurfaceElevation == 0.0);

    const RoutePose ordinary = route.sample(100'000.0);
    REQUIRE(!ordinary.bridge);
    REQUIRE(!ordinary.waterCrossing);
    REQUIRE(ordinary.waterCrossingSurfaceElevation == 0.0);
}

TEST_CASE("Lakehurst approach exposes a bounded level lakeside landmark")
{
    const Route route = Route::NorthernValleyExpress();
    const RoutePose before = route.sample(80'000.0);
    const RoutePose shore = route.sample(84'000.0);
    const RoutePose after = route.sample(88'000.0);

    REQUIRE(before.adjacentWaterSide == 0);
    REQUIRE(shore.adjacentWaterSide == 1);
    REQUIRE(std::abs(shore.adjacentWaterSurfaceElevation - 8.0) < 1.0e-9);
    REQUIRE(after.adjacentWaterSide == 0);
}

TEST_CASE("route relative frame keeps the train at a precise local origin")
{
    const Route route = Route::NorthernValleyExpress();
    for (double reference = 0.0; reference <= route.lengthMetres(); reference += 9'137.0)
    {
        const RelativeRoutePose origin = route.relativePose(reference, reference);
        REQUIRE(std::abs(origin.x) < 1e-9);
        REQUIRE(std::abs(origin.elevation) < 1e-9);
        REQUIRE(std::abs(origin.z) < 1e-9);
        REQUIRE(std::abs(origin.headingRadians) < 1e-9);

        const double aheadDistance = std::min(route.lengthMetres(), reference + 100.0);
        const RelativeRoutePose ahead = route.relativePose(reference, aheadDistance);
        REQUIRE(std::isfinite(ahead.x));
        REQUIRE(std::isfinite(ahead.z));
        REQUIRE(ahead.z >= -1e-6);
        REQUIRE(std::abs(ahead.x) < 10.0);
    }
}

TEST_CASE("route relative frame extrapolates terminal furniture along endpoint tangents")
{
    const Route route = Route::NorthernValleyExpress();
    const RelativeRoutePose beforeAlderwick = route.relativePose(0.0,-120.0);
    const RelativeRoutePose beyondWesthaven = route.relativePose(
        route.lengthMetres(),route.lengthMetres()+120.0);

    REQUIRE(beforeAlderwick.z < -119.0);
    REQUIRE(beyondWesthaven.z > 119.0);
    REQUIRE(std::abs(beforeAlderwick.x) < 1e-6);
    REQUIRE(std::abs(beyondWesthaven.x) < 1e-6);
    REQUIRE(std::isfinite(beforeAlderwick.elevation));
    REQUIRE(std::isfinite(beyondWesthaven.elevation));
}

TEST_CASE("train-local consist remains within the gangway curve tolerance")
{
    const Route route = Route::NorthernValleyExpress();
    constexpr double carLengthMetres = 21.0;
    double maximumHeadingDelta = 0.0;
    for (double distance = 0.0; distance + carLengthMetres <= route.lengthMetres();
         distance += 10.0)
    {
        const RelativeRoutePose adjacent = route.relativePose(
            distance, distance + carLengthMetres);
        maximumHeadingDelta = std::max(maximumHeadingDelta,
                                       std::abs(adjacent.headingRadians));
    }

    // The fixed train-local passenger space is intentional: even at the sharpest authored curve,
    // adjacent 21 m cars differ by less than 0.1 degree. That is below the visual clearance in the
    // flexible gangways while preserving perfectly stable walking, seating, and NPC coordinates.
    REQUIRE(maximumHeadingDelta > 0.001);
    REQUIRE(maximumHeadingDelta < 0.1 * 3.14159265358979323846 / 180.0);
}

TEST_CASE("service departs stops and reverses indefinitely")
{
    const Route route = Route::NorthernValleyExpress();
    RouteSimulation simulation(route);
    simulation.setDevelopmentDwellScale(0.02);

    int directionChanges = 0;
    int previousDirection = simulation.state().direction;
    double previousScheduledArrival = simulation.state().scheduledNextArrivalSeconds;
    bool observedCoasting = false;
    std::uint64_t observedLegs = 0;
    for (int step = 0; step < 120'000; ++step)
    {
        simulation.update(2.0);
        REQUIRE(simulation.state().routeDistanceMetres >= 0.0);
        REQUIRE(simulation.state().routeDistanceMetres <= route.lengthMetres());
        REQUIRE(simulation.state().speedMetresPerSecond >= 0.0);
        REQUIRE(simulation.state().speedMetresPerSecond <= 44.45);
        REQUIRE(!(simulation.state().doorsOpen && simulation.state().speedMetresPerSecond > 0.001));
        REQUIRE(simulation.state().scheduledNextArrivalSeconds >= previousScheduledArrival);
        REQUIRE(std::isfinite(simulation.predictedScheduleDeviationSeconds()));
        observedCoasting = observedCoasting || simulation.state().phase == ServicePhase::Coasting;
        previousScheduledArrival = simulation.state().scheduledNextArrivalSeconds;
        if (simulation.state().completedLegs != observedLegs)
        {
            observedLegs = simulation.state().completedLegs;
            if (std::abs(simulation.state().lastArrivalDeviationSeconds) >= 60.0)
                throw std::runtime_error(
                    "Timetable drift at "
                    + route.stations()[simulation.state().currentStation].name
                    + ": " + std::to_string(simulation.state().lastArrivalDeviationSeconds)
                    + " seconds");
        }
        if (simulation.state().direction != previousDirection)
        {
            ++directionChanges;
            previousDirection = simulation.state().direction;
        }
        if (directionChanges >= 3) break;
    }
    REQUIRE(directionChanges >= 3);
    REQUIRE(simulation.state().completedLegs > route.stations().size() * 2);
    REQUIRE(observedCoasting);
}

TEST_CASE("service publishes signed physical longitudinal acceleration")
{
    const Route route = Route::NorthernValleyExpress();
    RouteSimulation simulation(route);

    simulation.teleportToRouteDistance(50'000.0,1,20.0);
    simulation.update(0.10);
    REQUIRE(simulation.state().phase == ServicePhase::Accelerating);
    REQUIRE(simulation.state().longitudinalAccelerationMetresPerSecondSquared > 0.61);
    REQUIRE(simulation.state().longitudinalAccelerationMetresPerSecondSquared < 0.63);

    simulation.teleportToRouteDistance(17'800.0,1,33.33);
    simulation.update(0.10);
    REQUIRE(simulation.state().phase == ServicePhase::Coasting);
    REQUIRE(simulation.state().longitudinalAccelerationMetresPerSecondSquared < -0.034);
    REQUIRE(simulation.state().longitudinalAccelerationMetresPerSecondSquared > -0.036);

    simulation.teleportToRouteDistance(18'200.0,1,33.33);
    simulation.update(0.10);
    REQUIRE(simulation.state().phase == ServicePhase::Braking);
    REQUIRE(simulation.state().longitudinalAccelerationMetresPerSecondSquared < -0.77);
    REQUIRE(simulation.state().longitudinalAccelerationMetresPerSecondSquared > -0.79);

    simulation.freeze(true);
    REQUIRE(simulation.state().longitudinalAccelerationMetresPerSecondSquared == 0.0);
}

TEST_CASE("doors open on each correct platform side")
{
    const Route route = Route::NorthernValleyExpress();
    RouteSimulation simulation(route);
    simulation.setDevelopmentDwellScale(0.02);
    std::uint64_t observedLegs = 0;
    for (int step = 0; step < 500'000 && observedLegs < 8; ++step)
    {
        simulation.update(0.1);
        if (simulation.state().completedLegs == observedLegs) continue;
        observedLegs = simulation.state().completedLegs;
        REQUIRE(simulation.state().doorsOpen);
        const bool eastboundLeft = route.stations()[simulation.state().currentStation].platformOnLeft;
        const bool expectedLeft = simulation.state().direction > 0 ? eastboundLeft : !eastboundLeft;
        REQUIRE(simulation.state().platformOnLeft == expectedLeft);
    }
    REQUIRE(observedLegs >= 8);
}

TEST_CASE("station teleport chooses an in-bounds onward direction")
{
    const Route route = Route::NorthernValleyExpress();
    RouteSimulation simulation(route);

    simulation.teleportToStation(route.stations().size() - 1, 1);
    REQUIRE(simulation.state().direction == -1);
    REQUIRE(simulation.state().currentStation == route.stations().size() - 1);
    REQUIRE(simulation.state().nextStation == route.stations().size() - 2);
    REQUIRE(simulation.state().doorsOpen);

    simulation.teleportToStation(5, -1);
    REQUIRE(simulation.state().direction == -1);
    REQUIRE(simulation.state().nextStation == 4);
    REQUIRE(simulation.state().routeDistanceMetres == route.stations()[5].distanceMetres);
}

TEST_CASE("route-distance teleport creates a coherent moving service in either direction")
{
    const Route route = Route::NorthernValleyExpress();
    RouteSimulation simulation(route);

    simulation.teleportToRouteDistance(123'000.0, 1, 60.0);
    REQUIRE(simulation.state().routeDistanceMetres == 123'000.0);
    REQUIRE(simulation.state().direction == 1);
    REQUIRE(simulation.state().currentStation == 5);
    REQUIRE(simulation.state().nextStation == 6);
    REQUIRE(!simulation.state().doorsOpen);
    REQUIRE(simulation.state().speedMetresPerSecond
            <= simulation.currentSpeedLimitMetresPerSecond());
    const double before = simulation.state().routeDistanceMetres;
    simulation.update(0.5);
    REQUIRE(simulation.state().routeDistanceMetres > before);

    simulation.teleportToRouteDistance(123'000.0, -1, 20.0);
    REQUIRE(simulation.state().direction == -1);
    REQUIRE(simulation.state().currentStation == 6);
    REQUIRE(simulation.state().nextStation == 5);
    REQUIRE(simulation.distanceToNextStation() > 11'000.0);

    bool rejected = false;
    try { simulation.teleportToRouteDistance(-1.0); }
    catch (const std::out_of_range&) { rejected = true; }
    REQUIRE(rejected);
}

TEST_CASE("segment speed limits apply in both travel directions")
{
    const Route route = Route::NorthernValleyExpress();
    RouteSimulation simulation(route);
    REQUIRE(std::round(simulation.currentSpeedLimitMetresPerSecond() * 3.6) == 120.0);

    simulation.teleportToStation(1, 1);
    REQUIRE(std::round(simulation.currentSpeedLimitMetresPerSecond() * 3.6) == 140.0);
    simulation.teleportToStation(2, -1);
    REQUIRE(std::round(simulation.currentSpeedLimitMetresPerSecond() * 3.6) == 140.0);
    simulation.teleportToStation(2, 1);
    REQUIRE(std::round(simulation.currentSpeedLimitMetresPerSecond() * 3.6) == 160.0);
}

TEST_CASE("arrival estimate remains useful while stopped and moving")
{
    const Route route = Route::NorthernValleyExpress();
    RouteSimulation simulation(route);
    const double stoppedEstimate = simulation.estimatedSecondsToNextStation();
    REQUIRE(stoppedEstimate > 8.0 * 60.0);
    REQUIRE(stoppedEstimate < 12.0 * 60.0);

    simulation.setDevelopmentDwellScale(0.02);
    for (int step = 0; step < 80; ++step) simulation.update(0.1);
    REQUIRE(simulation.state().speedMetresPerSecond > 0.0);
    REQUIRE(simulation.estimatedSecondsToNextStation() > 0.0);
    REQUIRE(simulation.estimatedSecondsToNextStation() < stoppedEstimate);
}

TEST_CASE("timetable state advances at arrivals and reports deviation")
{
    const Route route = Route::NorthernValleyExpress();
    RouteSimulation simulation(route);
    REQUIRE(std::abs(simulation.state().scheduledNextArrivalSeconds - 699.0) < 0.001);

    simulation.setDevelopmentDwellScale(0.02);
    const double firstScheduledArrival = simulation.state().scheduledNextArrivalSeconds;
    REQUIRE(std::abs(firstScheduledArrival - 610.8) < 0.001);
    while (simulation.state().completedLegs == 0) simulation.update(0.1);
    REQUIRE(std::abs(simulation.state().lastArrivalDeviationSeconds) < 10.0);
    REQUIRE(std::abs(simulation.predictedScheduleDeviationSeconds()
                     - simulation.state().lastArrivalDeviationSeconds) < 10.0);
    REQUIRE(simulation.state().nextStation != simulation.state().currentStation);

    while (simulation.state().phase == ServicePhase::Dwelling) simulation.update(0.1);
    REQUIRE(simulation.state().scheduledNextArrivalSeconds > firstScheduledArrival);
    REQUIRE(simulation.state().serviceElapsedSeconds > 0.0);
}

TEST_CASE("train-local door side aligns with every authored platform in both directions")
{
    const Route route = Route::NorthernValleyExpress();
    RouteSimulation simulation(route);
    for (std::size_t station = 0; station < route.stations().size(); ++station)
    {
        for (int requestedDirection : {-1, 1})
        {
            simulation.teleportToStation(station, requestedDirection);
            REQUIRE(platformOnNegativeTrainX(simulation.state())
                    == route.stations()[station].platformOnLeft);
        }
    }
}

TEST_CASE("upcoming platform side remains train-local while travelling in either direction")
{
    const Route route = Route::NorthernValleyExpress();
    RouteSimulation simulation(route);
    for (std::size_t segment = 0; segment + 1 < route.stations().size(); ++segment)
    {
        const double midpoint = (route.stations()[segment].distanceMetres
                                 + route.stations()[segment+1].distanceMetres) * 0.5;
        for (int direction : {-1,1})
        {
            simulation.teleportToRouteDistance(midpoint,direction);
            REQUIRE(simulation.state().direction == direction);
            REQUIRE(simulation.nextPlatformOnNegativeTrainX()
                    == route.stations()[simulation.state().nextStation].platformOnLeft);
        }
    }
}

TEST_CASE("terminal reversal keeps the closing doors on the platform side")
{
    const Route route = Route::NorthernValleyExpress();
    RouteSimulation simulation(route);
    simulation.setDevelopmentDwellScale(0.02);
    simulation.teleportToStation(route.stations().size() - 2, 1);

    while (simulation.state().completedLegs == 0) simulation.update(2.0);
    REQUIRE(simulation.state().currentStation + 1 == route.stations().size());
    REQUIRE(simulation.state().doorsOpen);
    REQUIRE(simulation.state().direction < 0);
    REQUIRE(simulation.state().nextStation + 1 == simulation.state().currentStation);
    REQUIRE(simulation.destinationName() == "Alderwick Central");
    const bool openSide = platformOnNegativeTrainX(simulation.state());

    while (simulation.state().phase == ServicePhase::Dwelling) simulation.update(0.1);
    REQUIRE(simulation.state().phase == ServicePhase::ClosingDoors);
    REQUIRE(!simulation.state().doorsOpen);
    REQUIRE(platformOnNegativeTrainX(simulation.state()) == openSide);
}
