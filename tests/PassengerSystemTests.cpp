#include "TestHarness.hpp"

#include "trainsim/sim/PassengerSystem.hpp"
#include "trainsim/sim/TrainInterior.hpp"

#include <algorithm>
#include <set>

using namespace TrainSim::Sim;

TEST_CASE("passengers occupy unique seats")
{
    PassengerSystem passengers(42, 6, 48);
    REQUIRE(passengers.passengers().size() == 52);
    REQUIRE(passengers.occupiedSeatCount() == 48);
    REQUIRE(std::count_if(passengers.passengers().begin(), passengers.passengers().end(),
        [](const Passenger& passenger) {
            return passenger.state == PassengerState::Offboard;
        }) == 2);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("passengers enter a seated conversational idle")
{
    PassengerSystem passengers(0x7A1CULL, 6, 34);
    bool observedTalking = false;
    for (int step = 0; step < 20'000 && !observedTalking; ++step)
    {
        passengers.update(0.1, false, false);
        for (const Passenger& passenger : passengers.passengers())
        {
            if (passenger.state != PassengerState::Talking) continue;
            REQUIRE(passenger.role == PassengerRole::Passenger);
            REQUIRE(passenger.seat.has_value());
            observedTalking = true;
        }
        if (observedTalking)
        {
            const auto talking = static_cast<std::size_t>(std::count_if(
                passengers.passengers().begin(), passengers.passengers().end(),
                [](const Passenger& passenger) {
                    return passenger.state == PassengerState::Talking;
                }));
            REQUIRE(passengers.conversationPairCount() > 0);
            REQUIRE(talking == passengers.conversationPairCount() * 2);
        }
    }
    REQUIRE(observedTalking);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("passengers occasionally stand beside a row without occupying seat geometry")
{
    PassengerSystem passengers(0x57A4D1A6ULL,6,42);
    bool observedSettledStandingPassenger = false;
    for (int step = 0; step < 20'000 && !observedSettledStandingPassenger; ++step)
    {
        passengers.update(0.1,false,false);
        for (const Passenger& passenger : passengers.passengers())
        {
            if (passenger.role != PassengerRole::Passenger
                || passenger.state != PassengerState::Standing
                || passenger.exitingSeat.has_value())
                continue;
            REQUIRE(!passenger.seat.has_value());
            REQUIRE(std::abs(std::abs(passenger.x)-0.28) < 0.02);
            observedSettledStandingPassenger = true;
            break;
        }
    }
    REQUIRE(observedSettledStandingPassenger);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("passenger population rejects a mismatched train layout")
{
    bool rejected = false;
    try
    {
        static_cast<void>(PassengerSystem(42, 0, 12));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    REQUIRE(rejected);
}

TEST_CASE("passenger activity remains consistent during a long journey")
{
    PassengerSystem passengers(0x12345678ULL, 6, 56);
    for (int step = 0; step < 200'000; ++step)
    {
        const bool station = step % 2'000 < 100;
        passengers.update(0.1, station, station);
        if (step % 1'000 == 0) REQUIRE(passengers.invariantsHold());
    }
    REQUIRE(passengers.invariantsHold());
    REQUIRE(passengers.offboardPassengerCount() == 2);
    REQUIRE(passengers.occupiedSeatCount() > 20);
}

TEST_CASE("walking passengers change to a clear aisle lane instead of clustering")
{
    PassengerSystem passengers(0x1A1ECAFEULL, 6, 80);
    for (int step = 0; step < 120'000 && passengers.passengerLaneChangeCount() == 0; ++step)
        passengers.update(0.1, false, false);

    REQUIRE(passengers.passengerLaneChangeCount() > 0);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("an aisle actor yields sideways to a nearby player and restores its lane")
{
    PassengerSystem passengers(0x71E1D5ULL, 6, 24);
    const std::size_t conductorIndex = passengers.conductorIndex();
    const double originalX = passengers.passengers()[conductorIndex].x;
    const double conductorZ = passengers.passengers()[conductorIndex].z;

    for (int step = 0; step < 12; ++step)
        passengers.update(0.1,false,false,1.0,1,true,0.0,conductorZ+0.85);

    const Passenger& yielding = passengers.passengers()[conductorIndex];
    REQUIRE(yielding.yieldingToPlayer);
    REQUIRE(std::abs(yielding.x) > std::abs(originalX)+0.08);
    REQUIRE(passengers.playerYieldCount() == 1);
    // A player taking the other usable edge of the aisle now has genuine body clearance.
    REQUIRE(std::abs(yielding.x-0.39) > 0.70);

    for (int step = 0; step < 8; ++step)
        passengers.update(0.1,false,false,1.0,1,true,1'000.0,1'000.0);

    const Passenger& restored = passengers.passengers()[conductorIndex];
    REQUIRE(!restored.yieldingToPlayer);
    REQUIRE(std::abs(restored.x) < 0.31);
    REQUIRE(passengers.playerYieldCount() == 1);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("opposing aisle actors clear a player's passing lane without overlapping")
{
    for (std::size_t actorCount : {2U,6U})
    {
        for (double playerX : {-0.32,0.0,0.32})
        {
            PassengerSystem passengers(42,6,actorCount);
            // Reproduce the native-walk blocker: two stationary bodies abreast at the
            // display bay. Mutate only this non-const fixture, preserving seat ownership.
            auto& actors = const_cast<std::vector<Passenger>&>(passengers.passengers());
            auto& seats = const_cast<std::vector<Seat>&>(passengers.seats());
            for (std::size_t i = 0; i < actorCount; ++i)
            {
                seats[*actors[i].seat].occupant.reset();
                actors[i].seat.reset();
                actors[i].state = PassengerState::Standing;
                actors[i].activityRemaining = 1'000.0;
                actors[i].x = i % 2 == 0 ? -0.39 : 0.39;
                actors[i].z = 10.50-0.625*static_cast<double>(i/2)-(i%2 == 0 ? 0.0 : 0.033);
                actors[i].car = 3;
            }
            for (int step = 0; step < 200; ++step)
            {
                passengers.update(0.05,false,false,1.0,1,true,playerX,11.3);
                for (std::size_t i = 0; i < actorCount; ++i)
                    for (std::size_t j = i+1; j < actorCount; ++j)
                        REQUIRE(std::hypot(actors[i].x-actors[j].x,actors[i].z-actors[j].z) >= 0.51);
                for (std::size_t i = 0; i < actorCount; ++i)
                {
                    REQUIRE(TrainInterior::canOccupy(actors[i].x,actors[i].z,false));
                    REQUIRE(std::hypot(actors[i].x-playerX,actors[i].z-11.3) >= 0.48);
                }
            }
            // A centred player also needs a clear side to take before passing.
            const double passingX = playerX == 0.0 ? 0.32 : playerX;
            double playerZ = 11.3;
            for (int step = 0; step < 400 && playerZ > 9.1; ++step)
            {
                passengers.update(0.05,false,false,1.0,1,true,passingX,playerZ);
                const double candidateZ = std::max(9.1,playerZ-1.75*0.05);
                bool clear = TrainInterior::canOccupy(passingX,candidateZ,false);
                for (const auto& actor : actors)
                {
                    if (actor.seat.has_value() || actor.state == PassengerState::Offboard
                        || actor.role == PassengerRole::Driver) continue;
                    if (std::hypot(actor.x-passingX,actor.z-candidateZ) < 0.48) clear = false;
                }
                if (clear) playerZ = candidateZ;
            }
            if (playerZ > 9.1)
                std::cerr << "Blocked yield fixture: count=" << actorCount
                          << " player_x=" << playerX << " player_z=" << playerZ << '\n';
            REQUIRE(playerZ <= 9.1);
            REQUIRE(passengers.invariantsHold());
        }
    }
}

TEST_CASE("an actor yielding beside the toilet remains outside its side wall")
{
    PassengerSystem passengers(42,6,0);
    auto& actors = const_cast<std::vector<Passenger>&>(passengers.passengers());
    auto& conductor = actors[passengers.conductorIndex()];
    conductor.x = -0.28;
    conductor.z = 6.0;
    conductor.car = 3;
    conductor.activityRemaining = 1'000.0;
    for (int step = 0; step < 30; ++step)
    {
        passengers.update(0.05,false,false,1.0,1,true,-0.32,7.0);
        REQUIRE(TrainInterior::canOccupy(conductor.x,conductor.z,false));
    }
}

TEST_CASE("unseated onboard actors retain visible body separation")
{
    PassengerSystem passengers(0x5E9A2A7EULL, 6, 48);
    double minimumSeparation = 10.0;
    for (int step = 0; step < 40'000; ++step)
    {
        const bool station = step % 4'000 < 160;
        passengers.update(0.1,station,station,-1.0);
        if (step % 10 != 0) continue;

        const auto& actors = passengers.passengers();
        for (std::size_t first = 0; first < actors.size(); ++first)
        {
            if (actors[first].role == PassengerRole::Driver || actors[first].seat.has_value()
                || std::abs(actors[first].x) > 1.20) continue;
            for (std::size_t second = first+1; second < actors.size(); ++second)
            {
                if (actors[second].role == PassengerRole::Driver || actors[second].seat.has_value()
                    || std::abs(actors[second].x) > 1.20) continue;
                minimumSeparation = std::min(minimumSeparation,std::hypot(
                    actors[first].x-actors[second].x,actors[first].z-actors[second].z));
            }
        }
    }

    REQUIRE(minimumSeparation >= 0.50);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("passengers walk continuously to a reserved seat before sitting")
{
    PassengerSystem passengers(0x5EA7C0DEULL, 6, 34);
    std::optional<std::size_t> walkerId;
    std::optional<std::size_t> targetSeat;
    for (int step = 0; step < 20'000 && !walkerId.has_value(); ++step)
    {
        passengers.update(0.1, false, false);
        for (const Passenger& passenger : passengers.passengers())
        {
            if (passenger.state != PassengerState::WalkingToSeat) continue;
            REQUIRE(passenger.targetSeat.has_value());
            walkerId = passenger.id;
            targetSeat = passenger.targetSeat;
            REQUIRE(!passengers.seatAvailable(*targetSeat));
            REQUIRE(passenger.exitingSeat.has_value());
            REQUIRE(!passengers.seatAvailable(*passenger.exitingSeat));
            break;
        }
    }
    REQUIRE(walkerId.has_value());
    REQUIRE(targetSeat.has_value());

    double previousX = passengers.passengers()[*walkerId].x;
    double previousZ = passengers.passengers()[*walkerId].z;
    bool satDown = false;
    for (int step = 0; step < 5'000 && !satDown; ++step)
    {
        passengers.update(0.1, false, false);
        const Passenger& walker = passengers.passengers()[*walkerId];
        REQUIRE(std::hypot(walker.x - previousX, walker.z - previousZ) < 0.14);
        previousX = walker.x;
        previousZ = walker.z;
        satDown = walker.seat == targetSeat;
    }
    REQUIRE(satDown);
    REQUIRE(!passengers.passengers()[*walkerId].targetSeat.has_value());
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("the conductor yields to the player before continuing the onboard round")
{
    PassengerSystem passengers(7, 6, 24);
    for (int step = 0; step < 1'500; ++step)
        passengers.update(0.1, false, false, 1.0, 1, true, 0.0, 0.0);

    const Passenger& yielding = passengers.passengers()[passengers.conductorIndex()];
    REQUIRE(std::hypot(yielding.x, yielding.z) >= 1.11);
    REQUIRE(yielding.z < 0.0);

    double maximum = yielding.z;
    for (int step = 0; step < 1'500; ++step)
    {
        passengers.update(0.1, false, false);
        maximum = std::max(maximum,
                           passengers.passengers()[passengers.conductorIndex()].z);
    }
    REQUIRE(maximum > 55.0);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("player seat reservation excludes passengers and remains stable")
{
    PassengerSystem passengers(0x51EA7ULL, 6, 24);
    const auto available = std::find_if(passengers.seats().begin(), passengers.seats().end(),
        [&passengers](const Seat& seat) {
            return passengers.seatAvailable(seat.id)
                && passengers.seatApproachClear(seat.id);
        });
    REQUIRE(available != passengers.seats().end());

    const std::size_t seatIndex = available->id;
    const std::size_t before = passengers.occupiedSeatCount();
    REQUIRE(passengers.occupySeatForPlayer(seatIndex));
    REQUIRE(!passengers.seatAvailable(seatIndex));
    REQUIRE(passengers.playerSeat() == seatIndex);
    REQUIRE(passengers.occupiedSeatCount() == before + 1);

    for (int step = 0; step < 50'000; ++step)
        passengers.update(0.1, step % 2'000 < 100, step % 2'000 < 100);
    REQUIRE(passengers.invariantsHold());
    REQUIRE(passengers.playerSeat() == seatIndex);
    REQUIRE(!passengers.seats()[seatIndex].occupant.has_value());

    passengers.releasePlayerSeat();
    REQUIRE(passengers.seatAvailable(seatIndex));
    REQUIRE(!passengers.playerSeat().has_value());
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("an occupied aisle seat blocks access to its window seat")
{
    PassengerSystem passengers(0xA15EULL, 6, 1);
    std::optional<std::size_t> innerSeat;
    std::optional<std::size_t> windowSeat;
    for (const Seat& candidate : passengers.seats())
    {
        if (candidate.window || !passengers.seatAvailable(candidate.id)) continue;
        const auto window = std::find_if(
            passengers.seats().begin(),passengers.seats().end(),[&candidate,&passengers](const Seat& seat) {
                return seat.window && seat.car == candidate.car
                    && seat.x * candidate.x > 0.0
                    && std::abs(seat.z-candidate.z) < 0.01
                    && passengers.seatAvailable(seat.id);
            });
        if (window == passengers.seats().end()) continue;
        innerSeat = candidate.id;
        windowSeat = window->id;
        break;
    }
    REQUIRE(innerSeat.has_value());
    REQUIRE(windowSeat.has_value());
    REQUIRE(passengers.seatApproachClear(*windowSeat));
    REQUIRE(passengers.occupySeatForPlayer(*innerSeat));
    REQUIRE(!passengers.seatApproachClear(*windowSeat));
    passengers.releasePlayerSeat();
    REQUIRE(passengers.seatApproachClear(*windowSeat));
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("conductor walks through the complete accessible consist")
{
    PassengerSystem passengers(7, 6, 24);
    double minimum = 1'000.0;
    double maximum = -1'000.0;
    for (int step = 0; step < 2'000; ++step)
    {
        passengers.update(0.1, false, false);
        const auto& conductor = passengers.passengers()[passengers.conductorIndex()];
        minimum = std::min(minimum, conductor.z);
        maximum = std::max(maximum, conductor.z);
    }
    REQUIRE(minimum < -55.0);
    REQUIRE(maximum > 55.0);
}

TEST_CASE("conductor pauses for passenger-area inspections during a round")
{
    PassengerSystem passengers(0xC0AD0C70ULL, 6, 34);
    bool observedInspection = false;
    for (int step = 0; step < 4'000 && !observedInspection; ++step)
    {
        const std::size_t before = passengers.conductorInspectionCount();
        passengers.update(0.1, false, false);
        if (passengers.conductorInspectionCount() == before) continue;
        const Passenger& conductor = passengers.passengers()[passengers.conductorIndex()];
        REQUIRE(conductor.state == PassengerState::Standing);
        REQUIRE(std::abs(conductor.z) < 55.0);
        observedInspection = std::any_of(
            passengers.passengers().begin(), passengers.passengers().end(),
            [&conductor](const Passenger& passenger) {
                return passenger.role == PassengerRole::Passenger && passenger.seat.has_value()
                    && std::abs(passenger.z - conductor.z) < 0.85;
            });
    }
    REQUIRE(observedInspection);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("driver occupies the active cab when service direction reverses")
{
    PassengerSystem passengers(0xD12E3ULL, 6, 24);
    const Passenger& initialDriver = passengers.passengers().back();
    REQUIRE(initialDriver.role == PassengerRole::Driver);
    REQUIRE(initialDriver.z == 71.0);

    passengers.update(0.1, false, false, 1.0, -1);
    const Passenger& reverseDriver = passengers.passengers().back();
    REQUIRE(reverseDriver.z == -71.0);
    REQUIRE(reverseDriver.car == -1);

    passengers.update(0.1, false, false, 1.0, 1);
    const Passenger& forwardDriver = passengers.passengers().back();
    REQUIRE(forwardDriver.z == 71.0);
    REQUIRE(forwardDriver.car == TrainInterior::PassengerCars);
}

TEST_CASE("conductor attends the platform door and resumes the onboard round")
{
    PassengerSystem passengers(0xC0AD0C70ULL, 6, 24);
    const std::size_t conductorIndex = passengers.conductorIndex();
    for (int step = 0; step < 300; ++step)
        passengers.update(0.1, true, true, -1.0);

    const Passenger& atDoor = passengers.passengers()[conductorIndex];
    REQUIRE(atDoor.state == PassengerState::WaitingAtVestibule);
    REQUIRE(std::abs(atDoor.z - (-44.5)) < 0.25);
    REQUIRE(atDoor.x < -0.75);

    passengers.update(0.1, false, false, -1.0);
    const Passenger& resumed = passengers.passengers()[conductorIndex];
    REQUIRE(resumed.state == PassengerState::Walking);
    REQUIRE(resumed.targetZ > 55.0);
    REQUIRE(std::abs(resumed.x) < 0.30);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("station exchange visibly uses the platform-side door")
{
    PassengerSystem passengers(0xB04DULL, 6, 32);
    const auto offboardCount = [&passengers] {
        return std::count_if(passengers.passengers().begin(), passengers.passengers().end(),
            [](const Passenger& passenger) {
                return passenger.state == PassengerState::Offboard;
            });
    };
    REQUIRE(offboardCount() == 2);
    for (int step = 0; step < 14; ++step)
        passengers.update(0.1, true, true, -1.0);
    REQUIRE(std::none_of(passengers.passengers().begin(), passengers.passengers().end(),
        [](const Passenger& passenger) {
            return passenger.state == PassengerState::Boarding
                || passenger.state == PassengerState::Alighting;
        }));

    for (int step = 0; step < 2; ++step)
        passengers.update(0.1, true, true, -1.0);
    REQUIRE(passengers.lastStationExchangeParticipantCount() == 4);
    REQUIRE(offboardCount() == 0);
    const auto exchanging = std::count_if(passengers.passengers().begin(), passengers.passengers().end(),
        [](const Passenger& passenger) {
            return passenger.state == PassengerState::Boarding
                || passenger.state == PassengerState::Alighting;
        });
    REQUIRE(exchanging >= 2);
    std::set<int> exchangeCars;
    for (const Passenger& passenger : passengers.passengers())
    {
        if (passenger.state != PassengerState::Boarding
            && passenger.state != PassengerState::Alighting) continue;
        const double doorZ = TrainInterior::passengerDoorZ(passenger.car);
        REQUIRE(std::abs(passenger.z - doorZ) < 1e-9);
        REQUIRE(passenger.x <= 0.0);
        REQUIRE(passenger.targetX <= 0.0);
        REQUIRE(exchangeCars.insert(passenger.car).second);
    }
    const Passenger& conductor = passengers.passengers()[passengers.conductorIndex()];
    const double firstDoorZ = TrainInterior::passengerDoorZ(0);
    const int conductorDoorCar = std::clamp(static_cast<int>(std::lround(
        (conductor.targetZ-firstDoorZ)/TrainInterior::CarLength)),
        0,TrainInterior::PassengerCars-1);
    REQUIRE(!exchangeCars.contains(conductorDoorCar));

    bool reachedPlatform = false;
    for (int step = 0; step < 80; ++step)
    {
        passengers.update(0.1, true, true, -1.0);
        reachedPlatform = reachedPlatform || std::any_of(
            passengers.passengers().begin(), passengers.passengers().end(),
            [](const Passenger& passenger) { return passenger.x < -2.5; });
    }
    REQUIRE(reachedPlatform);
    REQUIRE(offboardCount() == 2);
    REQUIRE(passengers.invariantsHold());

    passengers.update(0.1, false, false, -1.0);
    REQUIRE(std::none_of(passengers.passengers().begin(), passengers.passengers().end(),
        [](const Passenger& passenger) {
            return passenger.state == PassengerState::Boarding
                || passenger.state == PassengerState::Alighting;
        }));
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("alighting passengers walk to vestibules during the station approach")
{
    PassengerSystem passengers(0xA221A1ULL, 6, 34);
    passengers.update(0.1, false, false, -1.0, 1, true, 1'000.0, 1'000.0, true);

    std::vector<std::size_t> preparing;
    for (const Passenger& passenger : passengers.passengers())
    {
        if (passenger.state != PassengerState::PreparingToLeave) continue;
        preparing.push_back(passenger.id);
        REQUIRE(passenger.exitingSeat.has_value());
        const double doorZ = TrainInterior::passengerDoorZ(passenger.car);
        REQUIRE(std::abs(passenger.z-doorZ) > 1.0);
        REQUIRE(std::abs(passenger.x) < 1.20);
    }
    REQUIRE(preparing.size() == 2);

    bool reachedVestibule = false;
    for (int step = 0; step < 600; ++step)
    {
        passengers.update(0.1, false, false, -1.0, 1, true,
                          1'000.0, 1'000.0, true);
        for (std::size_t id : preparing)
        {
            const Passenger& passenger = passengers.passengers()[id];
            REQUIRE(passenger.state == PassengerState::PreparingToLeave);
            REQUIRE(std::abs(passenger.x) < 1.20);
            reachedVestibule = reachedVestibule
                || (!passenger.exitingSeat.has_value()
                    && std::abs(passenger.z-passenger.targetZ) < 0.15);
        }
        REQUIRE(passengers.invariantsHold());
    }
    REQUIRE(reachedVestibule);

    // The visual leaf animation takes 1.35 s. Prepared alighters remain inside until the shared
    // 1.45 s passage release, rather than walking through a partially open door.
    for (int step = 0; step < 14; ++step)
        passengers.update(0.1, true, true, -1.0, 1, true,
                          1'000.0, 1'000.0, false);
    for (std::size_t id : preparing)
    {
        const Passenger& passenger = passengers.passengers()[id];
        REQUIRE(passenger.state == PassengerState::PreparingToLeave);
        REQUIRE(std::abs(passenger.x) < 0.40);
    }
    for (int step = 0; step < 2; ++step)
        passengers.update(0.1, true, true, -1.0, 1, true,
                          1'000.0, 1'000.0, false);
    REQUIRE(passengers.lastStationExchangeParticipantCount() == 4);
    REQUIRE(passengers.stationExchangeActorCount() == 4);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("missed accelerated stop cancels preparation before leaving the seat row")
{
    PassengerSystem passengers(0xFA57F02ULL,6,34);
    passengers.update(0.1,false,false,-1.0,1,true,1'000.0,1'000.0,true);
    REQUIRE(passengers.stationExchangeActorCount() == 2);
    REQUIRE(std::count_if(passengers.passengers().begin(),passengers.passengers().end(),
        [](const Passenger& passenger) {
            return passenger.state == PassengerState::PreparingToLeave
                && passenger.exitingSeat.has_value();
        }) == 2);

    passengers.update(0.1,false,false,-1.0,1,true,1'000.0,1'000.0,false);
    REQUIRE(passengers.stationExchangeActorCount() == 0);
    REQUIRE(passengers.walkingToSeatCount() == 2);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("reverse-direction approach uses the upcoming platform side")
{
    PassengerSystem passengers(0x5E7E25EULL,6,34);
    passengers.update(0.1,false,false,1.0,-1,true,1'000.0,1'000.0,true);

    std::size_t preparing = 0;
    for (const Passenger& passenger : passengers.passengers())
    {
        if (passenger.state != PassengerState::PreparingToLeave) continue;
        ++preparing;
        REQUIRE(passenger.targetX > 0.0);
    }
    REQUIRE(preparing == 2);

    for (int step = 0; step < 650; ++step)
        passengers.update(0.1,false,false,1.0,-1,true,1'000.0,1'000.0,true);
    for (int step = 0; step < 16; ++step)
        passengers.update(0.1,true,true,1.0,-1,true,1'000.0,1'000.0,false);

    REQUIRE(passengers.lastStationExchangeParticipantCount() == 4);
    REQUIRE(passengers.stationExchangeActorCount() == 4);
    for (const Passenger& passenger : passengers.passengers())
    {
        if (passenger.state != PassengerState::Boarding
            && passenger.state != PassengerState::Alighting) continue;
        REQUIRE(passenger.x >= 0.0);
        REQUIRE(passenger.targetX >= 0.0);
    }
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("interrupted station exchange preserves the offboard identity reserve")
{
    PassengerSystem passengers(0xC105EDULL,6,34);
    for (int step = 0; step < 16; ++step)
        passengers.update(0.1,true,true,-1.0);

    REQUIRE(passengers.lastStationExchangeParticipantCount() == 4);
    REQUIRE(passengers.stationExchangeActorCount() == 4);
    REQUIRE(passengers.offboardPassengerCount() == 0);

    passengers.update(0.1,false,false,-1.0);
    REQUIRE(passengers.stationExchangeActorCount() == 0);
    REQUIRE(passengers.offboardPassengerCount() == 2);
    REQUIRE(passengers.walkingToSeatCount() == 2);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("station teleport settles and rearms an active passenger exchange")
{
    PassengerSystem passengers(0x7E1E907ULL,6,34);
    for (int step = 0; step < 16; ++step)
        passengers.update(0.1,true,true,-1.0);
    REQUIRE(passengers.stationExchangeActorCount() == 4);
    REQUIRE(passengers.offboardPassengerCount() == 0);

    passengers.resetStationCycleForTeleport();
    REQUIRE(passengers.stationExchangeActorCount() == 0);
    REQUIRE(passengers.lastStationExchangeParticipantCount() == 0);
    REQUIRE(passengers.offboardPassengerCount() == 2);
    REQUIRE(passengers.invariantsHold());

    for (int step = 0; step < 16; ++step)
        passengers.update(0.1,true,true,1.0);
    REQUIRE(passengers.stationExchangeActorCount() == 4);
    REQUIRE(passengers.lastStationExchangeParticipantCount() == 4);
    for (const Passenger& passenger : passengers.passengers())
    {
        if (passenger.state != PassengerState::Boarding
            && passenger.state != PassengerState::Alighting) continue;
        REQUIRE(passenger.x >= 0.0);
        REQUIRE(passenger.targetX >= 0.0);
    }
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("driver occupies the active cab after reversal")
{
    PassengerSystem passengers(92, 6, 20);
    passengers.update(0.1, false, false, 1.0, 1);
    const auto driver = std::find_if(passengers.passengers().begin(), passengers.passengers().end(),
        [](const Passenger& passenger) { return passenger.role == PassengerRole::Driver; });
    REQUIRE(driver != passengers.passengers().end());
    REQUIRE(driver->z == 71.0);
    passengers.update(0.1, true, true, -1.0, -1);
    REQUIRE(driver->z == -71.0);
}

TEST_CASE("passenger completes an exclusive toilet visit")
{
    PassengerSystem passengers(0x7011E7ULL, 6, 34);
    bool sawDoorRequest = false;
    bool sawOccupied = false;
    bool sawVisitFinish = false;
    for (int step = 0; step < 120'000; ++step)
    {
        const bool wasOccupied = passengers.toiletOccupied();
        passengers.update(0.1, false, false);
        sawDoorRequest = sawDoorRequest || passengers.toiletDoorRequestedOpen();
        sawOccupied = sawOccupied || passengers.toiletOccupied();
        sawVisitFinish = sawVisitFinish || (wasOccupied && !passengers.toiletOccupied());
        const auto toiletUsers = std::count_if(
            passengers.passengers().begin(), passengers.passengers().end(),
            [](const Passenger& passenger) {
                return passenger.state == PassengerState::VisitingToilet
                    || passenger.state == PassengerState::UsingToilet
                    || passenger.state == PassengerState::LeavingToilet;
            });
        REQUIRE(toiletUsers <= 1);
        if (sawDoorRequest && sawOccupied && sawVisitFinish) break;
    }
    REQUIRE(sawDoorRequest);
    REQUIRE(sawOccupied);
    REQUIRE(sawVisitFinish);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("NPCs cannot reserve an opened toilet during player entry but can after departure")
{
    PassengerSystem passengers(0xA11CEULL,6,34);
    for (int step = 0; step < 20'000; ++step)
    {
        const double x = step % 2 == 0 ? -0.95 : 0.64;
        const double z = step % 2 == 0 ? 4.1 : 4.537;
        passengers.update(0.1,false,false,1.0,1,
            TrainInterior::toiletAvailableToPassengers(x,z,true),x,z);
        REQUIRE(!passengers.toiletReserved());
    }
    bool reservedAfterDeparture = false;
    for (int step = 0; step < 20'000 && !reservedAfterDeparture; ++step)
    {
        passengers.update(0.1,false,false,1.0,1,
            TrainInterior::toiletAvailableToPassengers(0.0,1.5,true),0.0,1.5);
        reservedAfterDeparture = passengers.toiletReserved();
    }
    REQUIRE(reservedAfterDeparture);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("player occupancy prevents an NPC toilet reservation")
{
    PassengerSystem passengers(0xA11CEULL, 6, 34);
    for (int step = 0; step < 120'000; ++step)
    {
        passengers.update(0.1, false, false, 1.0, 1, false);
        REQUIRE(!passengers.toiletReserved());
    }

    bool eventuallyReserved = false;
    for (int step = 0; step < 120'000; ++step)
    {
        passengers.update(0.1, false, false, 1.0, 1, true);
        eventuallyReserved = eventuallyReserved || passengers.toiletReserved();
        if (eventuallyReserved) break;
    }
    REQUIRE(eventuallyReserved);
    REQUIRE(passengers.invariantsHold());
}

TEST_CASE("station exchange does not interrupt a reserved toilet visit")
{
    PassengerSystem passengers(0x570FULL, 6, 34);
    for (int step = 0; step < 120'000 && !passengers.toiletReserved(); ++step)
        passengers.update(0.1, false, false);
    REQUIRE(passengers.toiletReserved());

    const auto isToiletActor = [](const Passenger& passenger) {
        return passenger.state == PassengerState::VisitingToilet
            || passenger.state == PassengerState::UsingToilet
            || passenger.state == PassengerState::LeavingToilet;
    };
    const auto user = std::find_if(passengers.passengers().begin(),
                                   passengers.passengers().end(), isToiletActor);
    REQUIRE(user != passengers.passengers().end());
    const std::size_t userId = user->id;

    for (int exchange = 0; exchange < 64; ++exchange)
        passengers.simulateStationExchange(-1.0);

    REQUIRE(isToiletActor(passengers.passengers()[userId]));
    REQUIRE(passengers.toiletReserved());
    REQUIRE(passengers.invariantsHold());
}
