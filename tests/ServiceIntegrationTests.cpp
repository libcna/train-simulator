#include "TestHarness.hpp"

#include "trainsim/sim/PassengerSystem.hpp"
#include "trainsim/sim/Route.hpp"
#include "trainsim/sim/RouteSimulation.hpp"
#include "trainsim/sim/TrainInterior.hpp"
#include "trainsim/world/StreamingWorld.hpp"

#include <cmath>
#include <cstdint>

using namespace TrainSim::Sim;
using namespace TrainSim::World;

TEST_CASE("complete passenger service remains coherent through repeated terminal reversals")
{
    const Route route = Route::NorthernValleyExpress();
    RouteSimulation service(route);
    PassengerSystem passengers(route.seed() ^ 0x50415353454e4745ULL, 6, 34);
    StreamingWorld world(route, 250.0, 3);

    constexpr double stepSeconds = 0.25;
    int directionChanges = 0;
    int previousDirection = service.state().direction;
    bool previousDoorsOpen = service.state().doorsOpen;
    std::uint64_t previousCompletedLegs = 0;
    int doorOpenTransitions = 0;
    int doorCloseTransitions = 0;
    int exchangeFrames = 0;
    int fullExchangeStops = 0;
    std::size_t previousExchangeParticipants = 0;

    // Three reversals cover A -> B -> A -> B and every station in both directions. This is a
    // renderer-independent soak of the same integration path used by the live CNA application.
    for (int step = 0; step < 140'000 && directionChanges < 3; ++step)
    {
        service.update(stepSeconds);
        const ServiceState& state = service.state();
        const bool atStation = state.phase == ServicePhase::Dwelling;
        const bool approaching = state.phase == ServicePhase::Coasting
                              || state.phase == ServicePhase::Braking;
        const bool platformOnNegativeX = atStation
            ? platformOnNegativeTrainX(state) : service.nextPlatformOnNegativeTrainX();
        passengers.update(stepSeconds, atStation, state.doorsOpen,
                          platformOnNegativeX ? -1.0 : 1.0, state.direction,
                          true, 100.0, 100.0, approaching);
        world.update(state.routeDistanceMetres);

        if (state.direction != previousDirection)
        {
            ++directionChanges;
            previousDirection = state.direction;
        }
        if (state.doorsOpen != previousDoorsOpen)
        {
            if (state.doorsOpen) ++doorOpenTransitions;
            else ++doorCloseTransitions;
            previousDoorsOpen = state.doorsOpen;
        }
        if (state.completedLegs != previousCompletedLegs)
        {
            previousCompletedLegs = state.completedLegs;
            REQUIRE(state.doorsOpen);
            REQUIRE(state.speedMetresPerSecond < 0.001);
            REQUIRE(platformOnNegativeTrainX(state)
                    == route.stations()[state.currentStation].platformOnLeft);
        }
        if (passengers.stationExchangeActorCount() > 0) ++exchangeFrames;
        const std::size_t exchangeParticipants =
            passengers.lastStationExchangeParticipantCount();
        if (exchangeParticipants == 4 && previousExchangeParticipants != 4)
            ++fullExchangeStops;
        previousExchangeParticipants = exchangeParticipants;

        REQUIRE(world.chunks().size() <= 7);
        REQUIRE(!(state.doorsOpen && state.speedMetresPerSecond > 0.001));
        if (step % 250 == 0)
        {
            REQUIRE(passengers.invariantsHold());
            const Passenger& driver = passengers.passengers().back();
            REQUIRE(driver.role == PassengerRole::Driver);
            REQUIRE(driver.car == (state.direction > 0
                ? TrainInterior::PassengerCars : -1));
            REQUIRE(std::isfinite(service.predictedScheduleDeviationSeconds()));
        }
    }

    REQUIRE(directionChanges == 3);
    REQUIRE(service.state().completedLegs >= 3U * (route.stations().size() - 1U));
    REQUIRE(doorOpenTransitions >= 3 * static_cast<int>(route.stations().size() - 1U));
    REQUIRE(doorCloseTransitions >= doorOpenTransitions);
    REQUIRE(exchangeFrames > 100);
    // A few stops may have a naturally occupied doorway, but the great majority must still
    // complete the full two-alight/two-board exchange over three complete traversals.
    REQUIRE(fullExchangeStops >= 28);
    REQUIRE(passengers.offboardPassengerCount() == 2);
    REQUIRE(passengers.invariantsHold());
}
