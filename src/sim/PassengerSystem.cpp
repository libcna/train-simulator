#include "trainsim/sim/PassengerSystem.hpp"
#include "trainsim/sim/TrainInterior.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace TrainSim::Sim {

namespace {
constexpr double kCarLength = TrainInterior::CarLength;
constexpr double kFirstCarCentre = TrainInterior::FirstCarCentre;
constexpr double kAisleLaneX = 0.28;
constexpr double kActorSeparation = 0.62;
constexpr double kPassingActorSeparation = 0.52;
constexpr double kPlayerYieldEngageDistance = 2.50;
constexpr double kPlayerYieldReleaseDistance = 3.00;
constexpr std::size_t kOffboardPassengerPool = 2;
constexpr double kPassengerDoorPassageSeconds =
    TrainInterior::PassengerDoorPassageSeconds;

bool isToiletState(PassengerState state) noexcept
{
    return state == PassengerState::VisitingToilet
        || state == PassengerState::UsingToilet
        || state == PassengerState::LeavingToilet;
}
}

PassengerSystem::PassengerSystem(std::uint64_t seed, int passengerCars, std::size_t passengerCount)
    : randomState_(seed == 0 ? 1 : seed)
{
    if (passengerCars != TrainInterior::PassengerCars)
        throw std::invalid_argument("PassengerSystem must match the six-car train interior");
    std::size_t seatId = 0;
    for (int car = 0; car < passengerCars; ++car)
    {
        for (int row = 0; row < TrainInterior::SeatRowsPerCar; ++row)
        {
            const double z = TrainInterior::seatRowZ(car, row);
            for (int side : {-1, 1})
            {
                if (!TrainInterior::seatSidePresent(car, row, side)) continue;
                seats_.push_back(Seat{seatId++, car, 1.04 * side, z, true,
                                      car == 2 && (row == 0 || row == 1), std::nullopt});
                if (TrainInterior::innerSeatPresent(car, row))
                    seats_.push_back(Seat{seatId++, car, 0.58 * side, z, false, false, std::nullopt});
            }
        }
    }

    freeSeatScratch_.reserve(seats_.size());
    socialSeatScratch_.reserve(seats_.size());
    passengerCount = std::clamp<std::size_t>(passengerCount, 1, seats_.size());
    passengers_.reserve(passengerCount + kOffboardPassengerPool + 2);
    for (std::size_t i = 0; i < passengerCount; ++i)
    {
        Passenger passenger;
        passenger.id = i;
        passenger.appearanceSeed = static_cast<std::uint32_t>(nextRandom());
        passenger.car = static_cast<int>(nextRandom() % static_cast<std::uint64_t>(passengerCars));
        passenger.z = kFirstCarCentre + static_cast<double>(passenger.car) * kCarLength;
        if (const auto seat = chooseFreeSeat(passenger.car); seat.has_value())
        {
            sit(passenger,*seat);
            // The journey starts in progress, not at the instant every passenger sat down.
            // Deterministic residual times prevent an artificial twenty-second motionless wave
            // without consuming more RNG state or changing the generated population identities.
            passenger.activityRemaining = 3.0
                + static_cast<double>(passenger.appearanceSeed%7'500U)/100.0;
        }
        passengers_.push_back(passenger);
    }

    // A tiny persistent offboard pool lets genuinely new identities board without stealing an
    // actor from a visible onboard seat. Passengers who finish alighting replenish this pool, so
    // total storage and avatar demand remain bounded over an indefinite A-B-A service.
    for (std::size_t index = 0; index < kOffboardPassengerPool; ++index)
    {
        Passenger passenger;
        passenger.id = passengers_.size();
        passenger.state = PassengerState::Offboard;
        passenger.car = -1;
        passenger.x = 1'000.0 + static_cast<double>(index);
        passenger.z = 1'000.0;
        passenger.targetZ = passenger.z;
        passenger.activityRemaining = std::numeric_limits<double>::infinity();
        passenger.appearanceSeed = static_cast<std::uint32_t>(nextRandom());
        passengers_.push_back(passenger);
    }

    conductorIndex_ = passengers_.size();
    passengers_.push_back(Passenger{conductorIndex_, PassengerRole::Conductor,
                                    PassengerState::Walking, 0, -kAisleLaneX, -61.0, 61.0,
                                    0.0, std::nullopt, 0xC0AD0C70u, 0.0,
                                    std::nullopt, std::nullopt});
    passengers_.push_back(Passenger{passengers_.size(), PassengerRole::Driver,
                                    PassengerState::Seated, TrainInterior::PassengerCars,
                                    0.0, 71.0, 71.0,
                                    std::numeric_limits<double>::infinity(), std::nullopt, 0xD12E3u,
                                    0.0, std::nullopt, std::nullopt});
    conductorInspectionCooldown_ = 5.0 + randomUnit() * 3.0;
    synchronizeConversations();
}

std::uint64_t PassengerSystem::nextRandom()
{
    randomState_ ^= randomState_ >> 12;
    randomState_ ^= randomState_ << 25;
    randomState_ ^= randomState_ >> 27;
    return randomState_ * 2685821657736338717ULL;
}

double PassengerSystem::randomUnit()
{
    return static_cast<double>(nextRandom() >> 11) * (1.0 / 9007199254740992.0);
}

std::optional<std::size_t> PassengerSystem::chooseFreeSeat(int preferredCar,
                                                           bool requireClearApproach)
{
    auto& choices = freeSeatScratch_;
    auto& socialChoices = socialSeatScratch_;
    choices.clear();
    socialChoices.clear();
    for (std::size_t i = 0; i < seats_.size(); ++i)
        if (seatAvailable(i) && seats_[i].car == preferredCar
            && (!requireClearApproach || seatApproachClear(i))) choices.push_back(i);
    if (choices.empty())
        for (std::size_t i = 0; i < seats_.size(); ++i)
            if (seatAvailable(i) && (!requireClearApproach || seatApproachClear(i)))
                choices.push_back(i);
    if (choices.empty()) return std::nullopt;

    for (std::size_t choice : choices)
        if (seatHasOccupiedNeighbor(choice)) socialChoices.push_back(choice);
    if (!socialChoices.empty() && randomUnit() < 0.42)
        return socialChoices[static_cast<std::size_t>(nextRandom() % socialChoices.size())];
    return choices[static_cast<std::size_t>(nextRandom() % choices.size())];
}

bool PassengerSystem::seatHasOccupiedNeighbor(std::size_t seatIndex) const noexcept
{
    if (seatIndex >= seats_.size()) return false;
    const Seat& source = seats_[seatIndex];
    return std::any_of(seats_.begin(), seats_.end(), [&source](const Seat& candidate) {
        return candidate.occupant.has_value() && candidate.id != source.id
            && candidate.car == source.car && std::abs(candidate.z - source.z) <= 0.01
            && candidate.x * source.x > 0.0 && std::abs(candidate.x - source.x) <= 0.55;
    });
}

void PassengerSystem::sit(Passenger& passenger, std::size_t seatIndex)
{
    stand(passenger);
    Seat& seat = seats_[seatIndex];
    seat.occupant = passenger.id;
    passenger.seat = seatIndex;
    passenger.car = seat.car;
    passenger.x = seat.x;
    passenger.z = seat.z;
    passenger.targetZ = seat.z;
    passenger.yieldingToPlayer = false;
    const double idle = randomUnit();
    passenger.state = idle < 0.20 ? PassengerState::LookingOutside
                    : idle < (seatHasOccupiedNeighbor(seatIndex) ? 0.52 : 0.34)
                                  ? PassengerState::Talking
                                  : PassengerState::Seated;
    passenger.activityRemaining = 20.0 + randomUnit() * 75.0;
}

void PassengerSystem::stand(Passenger& passenger)
{
    if (passenger.seat.has_value() && *passenger.seat < seats_.size())
        seats_[*passenger.seat].occupant.reset();
    passenger.seat.reset();
    passenger.targetSeat.reset();
    passenger.exitingSeat.reset();
}

void PassengerSystem::moveOffboard(Passenger& passenger)
{
    stand(passenger);
    passenger.state = PassengerState::Offboard;
    passenger.car = -1;
    passenger.x = 1'000.0 + static_cast<double>(passenger.id % kOffboardPassengerPool);
    passenger.z = 1'000.0;
    passenger.targetX = passenger.x;
    passenger.targetZ = passenger.z;
    passenger.activityRemaining = std::numeric_limits<double>::infinity();
    passenger.yieldingToPlayer = false;
    passenger.appearanceSeed = static_cast<std::uint32_t>(nextRandom());
}

void PassengerSystem::chooseActivity(Passenger& passenger)
{
    const double roll = randomUnit();
    const bool toiletReserved = std::any_of(passengers_.begin(), passengers_.end(),
        [&passenger](const Passenger& other) {
            return &other != &passenger && isToiletState(other.state);
        });
    if (passenger.seat.has_value() && !seatApproachClear(*passenger.seat))
    {
        const std::size_t currentSeat = *passenger.seat;
        sit(passenger,currentSeat);
        return;
    }
    if (roll < 0.50 && passenger.seat.has_value())
    {
        const std::size_t currentSeat = *passenger.seat;
        sit(passenger, currentSeat);
        return;
    }
    if (roll < 0.62)
    {
        if (const auto seatIndex = chooseFreeSeat(passenger.car,true); seatIndex.has_value())
        {
            const Seat& seat = seats_[*seatIndex];
            const double walkingDistance = std::abs(seat.z - passenger.z);
            const std::optional<std::size_t> sourceSeat = passenger.seat;
            stand(passenger);
            passenger.exitingSeat = sourceSeat;
            passenger.targetSeat = seatIndex;
            passenger.state = PassengerState::WalkingToSeat;
            if (!sourceSeat.has_value())
                passenger.x = std::abs(passenger.x) > 0.05
                    ? std::copysign(kAisleLaneX,passenger.x)
                    : (seat.x < 0.0 ? -kAisleLaneX : kAisleLaneX);
            passenger.targetX = seat.x;
            passenger.targetZ = seat.z;
            // Allow a complete-consist walk at crowd speed, but never reserve a seat forever if
            // two dense aisle flows deadlock temporarily.
            passenger.activityRemaining = 35.0 + walkingDistance / 0.40;
        }
        else
        {
            passenger.activityRemaining = 5.0 + randomUnit() * 8.0;
        }
        return;
    }

    const std::optional<std::size_t> sourceSeat = passenger.seat;
    stand(passenger);
    passenger.exitingSeat = sourceSeat;
    // Two subtle aisle lanes let walkers pass without visibly marching through one another.
    if (!sourceSeat.has_value())
        passenger.x = std::abs(passenger.x) > 0.05
            ? std::copysign(kAisleLaneX,passenger.x)
            : (randomUnit() < 0.5 ? -kAisleLaneX : kAisleLaneX);
    if (roll < 0.76)
    {
        passenger.state = PassengerState::Walking;
        const int destinationCar = static_cast<int>(
            nextRandom() % static_cast<std::uint64_t>(TrainInterior::PassengerCars));
        passenger.targetZ = kFirstCarCentre + static_cast<double>(destinationCar) * kCarLength;
        passenger.activityRemaining = 12.0 + randomUnit() * 18.0;
    }
    else if (roll < 0.87 && toiletAvailable_ && !toiletReserved)
    {
        passenger.state = PassengerState::VisitingToilet;
        passenger.targetZ = 4.07;
        passenger.targetX = 0.66;
        passenger.activityRemaining = 0.0;
    }
    else if (roll < 0.96)
    {
        // A small number of passengers remain beside their row for a short conversation or to
        // stretch their legs. Retaining the source-seat exit path moves them into the aisle
        // before this idle begins, so they never stand inside the cushion geometry.
        passenger.state = PassengerState::Standing;
        passenger.targetX = passenger.x < 0.0 ? -kAisleLaneX : kAisleLaneX;
        passenger.targetZ = passenger.z;
        passenger.activityRemaining = 7.0 + randomUnit() * 14.0;
    }
    else
    {
        passenger.state = PassengerState::WaitingAtVestibule;
        passenger.targetZ = std::round(passenger.z / kCarLength) * kCarLength;
        passenger.activityRemaining = 8.0 + randomUnit() * 20.0;
    }
}

std::optional<std::size_t>
PassengerSystem::adjacentSeatedPassenger(const Passenger& passenger) const noexcept
{
    if (!passenger.seat.has_value() || *passenger.seat >= seats_.size()) return std::nullopt;
    const Seat& source = seats_[*passenger.seat];
    for (const Seat& candidate : seats_)
    {
        if (!candidate.occupant.has_value() || candidate.id == source.id) continue;
        if (candidate.car != source.car || std::abs(candidate.z - source.z) > 0.01) continue;
        if (candidate.x * source.x <= 0.0 || std::abs(candidate.x - source.x) > 0.55) continue;
        const std::size_t id = *candidate.occupant;
        if (id >= passengers_.size()) continue;
        const Passenger& partner = passengers_[id];
        if (partner.role == PassengerRole::Passenger && partner.seat.has_value()) return id;
    }
    return std::nullopt;
}

void PassengerSystem::synchronizeConversations()
{
    for (Passenger& passenger : passengers_)
    {
        if (passenger.state != PassengerState::Talking) continue;
        const auto partnerId = adjacentSeatedPassenger(passenger);
        if (!partnerId.has_value())
        {
            passenger.state = PassengerState::Seated;
            continue;
        }
        Passenger& partner = passengers_[*partnerId];
        if (partner.state != PassengerState::Seated
            && partner.state != PassengerState::LookingOutside
            && partner.state != PassengerState::Talking)
        {
            passenger.state = PassengerState::Seated;
            continue;
        }
        partner.state = PassengerState::Talking;
        const double sharedDuration = std::max(passenger.activityRemaining,
                                               partner.activityRemaining);
        passenger.activityRemaining = sharedDuration;
        partner.activityRemaining = sharedDuration;
    }
}

void PassengerSystem::update(double seconds, bool atStation, bool doorsOpen, double platformSide,
                             int travelDirection, bool toiletAvailable,
                             double playerX, double playerZ, bool approachingStation)
{
    const double dt = std::clamp(seconds, 0.0, 0.25);
    toiletAvailable_ = toiletAvailable;
    if (approachingStation && !atStation && !doorsOpen && !stationPreparationPerformed_)
    {
        stationPreparationPerformed_ = true;
        prepareForStation(platformSide);
    }
    else if (!approachingStation && !atStation && !doorsOpen)
    {
        stationPreparationPerformed_ = false;
        stationPreparedPassengerCount_ = 0;
    }
    if (!atStation || !doorsOpen)
    {
        doorsOpenElapsed_ = 0.0;
        stationExchangePerformed_ = false;
    }
    else
    {
        doorsOpenElapsed_ += dt;
        if (!stationExchangePerformed_
            && doorsOpenElapsed_ >= kPassengerDoorPassageSeconds)
        {
            simulateStationExchange(platformSide);
            stationExchangePerformed_ = true;
        }
    }

    const auto movesIntoPlayer = [playerX, playerZ](const Passenger& passenger,
                                                    double candidateX, double candidateZ) {
        const double candidateDistance = std::hypot(candidateX - playerX, candidateZ - playerZ);
        const double currentDistance = std::hypot(passenger.x - playerX, passenger.z - playerZ);
        return candidateDistance < 1.15 && candidateDistance < currentDistance;
    };
    const auto requiredActorSeparation = [](const Passenger&, double candidateX,
                                            const Passenger& other) {
        const bool oppositeAisleLanes = candidateX*other.x < 0.0
            && std::abs(candidateX-other.x) > 0.50
            && std::abs(candidateX) < 0.50 && std::abs(other.x) < 0.50;
        return oppositeAisleLanes ? kPassingActorSeparation : kActorSeparation;
    };
    const auto hasActorClearance = [this,&movesIntoPlayer,&requiredActorSeparation](
        const Passenger& passenger, double candidateX, double candidateZ) {
        if (movesIntoPlayer(passenger,candidateX,candidateZ)) return false;
        for (const Passenger& other : passengers_)
        {
            if (&other == &passenger || other.role == PassengerRole::Driver
                || other.state == PassengerState::Offboard || other.seat.has_value()) continue;
            const double candidateDistance = std::hypot(other.x-candidateX,other.z-candidateZ);
            if (candidateDistance < requiredActorSeparation(passenger,candidateX,other))
                return false;
        }
        return true;
    };
    const auto canAdvance = [this,&movesIntoPlayer,&requiredActorSeparation](
        const Passenger& passenger, double candidateX, double candidateZ) {
        if (movesIntoPlayer(passenger, candidateX, candidateZ)) return false;
        for (const Passenger& other : passengers_)
        {
            if (&other == &passenger || other.role == PassengerRole::Driver
                || other.state == PassengerState::Offboard || other.seat.has_value()) continue;
            const double candidateDistance = std::hypot(other.x-candidateX,other.z-candidateZ);
            const double currentDistance = std::hypot(other.x-passenger.x,other.z-passenger.z);
            const double requiredSeparation = requiredActorSeparation(
                passenger,candidateX,other);
            if (candidateDistance < requiredSeparation && candidateDistance < currentDistance)
                return false;
        }
        return true;
    };
    const auto safeAisleLane = [&hasActorClearance](const Passenger& passenger,
                                                    double preferred) {
        if (hasActorClearance(passenger,preferred,passenger.z))
            return std::optional<double>(preferred);
        const double alternate = -preferred;
        if (hasActorClearance(passenger,alternate,passenger.z))
            return std::optional<double>(alternate);
        return std::optional<double>{};
    };
    const auto safeAislePlacement = [&hasActorClearance](const Passenger& passenger,
                                                         double preferred) {
        constexpr std::array<double,9> offsets = {0.0,0.70,-0.70,1.40,-1.40,
                                                   2.10,-2.10,2.80,-2.80};
        for (double offset : offsets)
            for (double lane : {preferred,-preferred})
                if (hasActorClearance(passenger,lane,passenger.z+offset))
                    return std::optional<std::pair<double,double>>(
                        std::in_place,lane,passenger.z+offset);
        return std::optional<std::pair<double,double>>{};
    };
    const bool playerAboard = std::isfinite(playerX) && std::isfinite(playerZ)
        && std::abs(playerX) <= 1.27
        && playerZ >= TrainInterior::PassengerMinimumZ
        && playerZ <= TrainInterior::PassengerMaximumZ;
    const auto updatePlayerYield = [this,dt,playerAboard,playerX,playerZ,
                                    &hasActorClearance,&canAdvance](Passenger& passenger) {
        const bool aisleActor = passenger.role != PassengerRole::Driver
            && !passenger.seat.has_value()
            && passenger.state != PassengerState::Offboard
            && passenger.state != PassengerState::Boarding
            && passenger.state != PassengerState::Alighting
            && !isToiletState(passenger.state)
            && std::abs(passenger.x) < 0.50;
        const double playerDistance = playerAboard
            ? std::hypot(passenger.x-playerX,passenger.z-playerZ)
            : std::numeric_limits<double>::infinity();
        // Keep a yielding actor engaged long enough to finish a short retreat
        // around another body, instead of immediately returning to the blocked lane.
        // Notice the player across a whole seating bay, so the back of a small
        // queue makes room before its front actor needs to retreat into it.
        if (aisleActor && playerDistance < (passenger.yieldingToPlayer
                ? kPlayerYieldReleaseDistance : kPlayerYieldEngageDistance))
        {
            // A centred player must not be boxed between opposing pairs. Give
            // every actor the same stable preference until the player takes a
            // side; then yield away from that side.
            double desired = std::abs(playerX) > 0.08
                ? (playerX < 0.0 ? 0.39 : -0.39)
                : -0.39;
            const bool constrainedByWall = !TrainInterior::canOccupy(desired,passenger.z,false);
            if (constrainedByWall) desired = -desired;
            const bool retreatAlongWall = constrainedByWall && desired*playerX > 0.0;
            const double candidateX = passenger.x
                + std::clamp(desired-passenger.x,-0.72*dt,0.72*dt);
            if (!retreatAlongWall && TrainInterior::canOccupy(candidateX,passenger.z,false)
                && hasActorClearance(passenger,candidateX,passenger.z))
                passenger.x = candidateX;
            else if (retreatAlongWall || std::abs(desired-passenger.x) > 0.02)
            {
                // Two actors abreast cannot both slide through each other. The
                // blocked one steps away from the player before trying sideways
                // again. Every step retains body clearance and interior bounds.
                const double candidateZ = passenger.z
                    + (passenger.z < playerZ ? -1.0 : 1.0)*0.65*dt;
                if (TrainInterior::canOccupy(passenger.x,candidateZ,false)
                    && canAdvance(passenger,passenger.x,candidateZ))
                {
                    passenger.z = candidateZ;
                    passenger.car = std::clamp(
                        static_cast<int>((passenger.z+63.0)/kCarLength),
                        0,TrainInterior::PassengerCars-1);
                }
            }
            if (!passenger.yieldingToPlayer)
            {
                passenger.yieldingToPlayer = true;
                ++playerYieldCount_;
            }
            return;
        }

        if (!aisleActor)
        {
            passenger.yieldingToPlayer = false;
            return;
        }
        if (!passenger.yieldingToPlayer) return;
        // Returning is gradual and clearance-checked, so a passing player or second actor cannot
        // be clipped by the transition back to the ordinary two-lane topology.
        const double ordinaryLane = passenger.x < 0.0 ? -kAisleLaneX : kAisleLaneX;
        if (std::abs(passenger.x-ordinaryLane) <= 0.01)
        {
            passenger.x = ordinaryLane;
            passenger.yieldingToPlayer = false;
            return;
        }
        const double candidateX = passenger.x
            + std::clamp(ordinaryLane-passenger.x,-0.72*dt,0.72*dt);
        if (hasActorClearance(passenger,candidateX,passenger.z))
        {
            passenger.x = candidateX;
            if (std::abs(passenger.x-ordinaryLane) <= 0.01)
            {
                passenger.x = ordinaryLane;
                passenger.yieldingToPlayer = false;
            }
        }
    };

    for (Passenger& passenger : passengers_)
    {
        if (passenger.role == PassengerRole::Driver)
        {
            passenger.z = travelDirection >= 0 ? 71.0 : -71.0;
            passenger.targetZ = passenger.z;
            passenger.car = travelDirection >= 0 ? TrainInterior::PassengerCars : -1;
            continue;
        }
        updatePlayerYield(passenger);
        if (passenger.role == PassengerRole::Conductor)
        {
            if (atStation && doorsOpen)
            {
                if (!conductorStationDuty_)
                {
                    conductorStationDuty_ = true;
                    conductorRoundTargetZ_ = passenger.targetZ;
                    passenger.activityRemaining = 0.0;
                    constexpr double firstDoorZ = TrainInterior::passengerDoorZ(0);
                    const int preferredDoorCar = std::clamp(
                        static_cast<int>(std::lround((passenger.z - firstDoorZ) / kCarLength)),
                        0, TrainInterior::PassengerCars - 1);
                    std::array<bool,TrainInterior::PassengerCars> exchangeDoor{};
                    for (const Passenger& other : passengers_)
                        if ((other.state == PassengerState::Boarding
                             || other.state == PassengerState::Alighting)
                            && other.car >= 0 && other.car < TrainInterior::PassengerCars)
                            exchangeDoor[static_cast<std::size_t>(other.car)] = true;
                    int doorCar = preferredDoorCar;
                    double bestDistance = std::numeric_limits<double>::infinity();
                    for (int candidate = 0; candidate < TrainInterior::PassengerCars; ++candidate)
                    {
                        if (exchangeDoor[static_cast<std::size_t>(candidate)]) continue;
                        const double distance = std::abs(
                            firstDoorZ + static_cast<double>(candidate)*kCarLength - passenger.z);
                        if (distance >= bestDistance) continue;
                        bestDistance = distance;
                        doorCar = candidate;
                    }
                    passenger.targetZ = firstDoorZ + static_cast<double>(doorCar) * kCarLength;
                }

                const double candidateZ = passenger.z
                    + std::clamp(passenger.targetZ - passenger.z, -1.0 * dt, 1.0 * dt);
                if (std::abs(passenger.targetZ - passenger.z) >= 0.2)
                {
                    passenger.state = PassengerState::Walking;
                    if (canAdvance(passenger, passenger.x, candidateZ))
                    {
                        passenger.z = candidateZ;
                    }
                    else
                    {
                        const double otherLane = passenger.x < 0.0
                            ? kAisleLaneX : -kAisleLaneX;
                        if (canAdvance(passenger, otherLane, candidateZ))
                        {
                            passenger.x = otherLane;
                            passenger.z = candidateZ;
                        }
                    }
                }
                else
                {
                    passenger.state = PassengerState::WaitingAtVestibule;
                    const double doorObservationX = platformSide < 0.0 ? -0.88 : 0.88;
                    const double candidateX = passenger.x
                        + std::clamp(doorObservationX - passenger.x, -0.65 * dt, 0.65 * dt);
                    if (!movesIntoPlayer(passenger, candidateX, passenger.z))
                        passenger.x = candidateX;
                }
                passenger.car = std::clamp(
                    static_cast<int>((passenger.z + 63.0) / kCarLength),
                    0, TrainInterior::PassengerCars - 1);
                continue;
            }
            if (conductorStationDuty_)
            {
                const double preferred = passenger.x < 0.0 ? -kAisleLaneX : kAisleLaneX;
                const auto lane = safeAisleLane(passenger,preferred);
                if (!lane.has_value())
                {
                    passenger.state = PassengerState::WaitingAtVestibule;
                    continue;
                }
                conductorStationDuty_ = false;
                passenger.targetZ = conductorRoundTargetZ_;
                passenger.x = *lane;
                passenger.state = PassengerState::Walking;
            }
            if (passenger.activityRemaining > 0.0)
            {
                passenger.activityRemaining -= dt;
                continue;
            }
            const double delta = passenger.targetZ - passenger.z;
            if (std::abs(delta) < 0.2)
            {
                passenger.targetZ = passenger.targetZ > 0.0 ? -61.0 : 61.0;
                passenger.activityRemaining = 4.0 + randomUnit() * 8.0;
                passenger.state = PassengerState::WaitingAtVestibule;
                continue;
            }
            passenger.state = PassengerState::Walking;
            const double candidateZ = passenger.z
                + std::clamp(passenger.targetZ - passenger.z, -1.0 * dt, 1.0 * dt);
            if (canAdvance(passenger, passenger.x, candidateZ))
            {
                passenger.z = candidateZ;
            }
            else
            {
                // A conductor should yield to passengers but must still be able to complete a
                // round. Switch to the opposite aisle lane only when that lane is clear.
                const double otherLane = passenger.x < 0.0
                    ? kAisleLaneX : -kAisleLaneX;
                if (canAdvance(passenger, otherLane, candidateZ))
                {
                    passenger.x = otherLane;
                    passenger.z = candidateZ;
                }
            }
            const bool besideOccupiedRow = std::any_of(
                passengers_.begin(), passengers_.end(), [&passenger](const Passenger& other) {
                    return other.role == PassengerRole::Passenger && other.seat.has_value()
                        && std::abs(other.z - passenger.z) < 0.85;
                });
            conductorInspectionCooldown_ -= dt;
            if (besideOccupiedRow && std::abs(passenger.z) < 55.0
                && conductorInspectionCooldown_ <= 0.0)
            {
                passenger.state = PassengerState::Standing;
                passenger.activityRemaining = 3.5 + randomUnit() * 4.5;
                conductorInspectionCooldown_ = 38.0 + randomUnit() * 32.0;
                ++conductorInspectionCount_;
            }
            passenger.car = std::clamp(
                static_cast<int>((passenger.z + 63.0) / kCarLength),
                0, TrainInterior::PassengerCars - 1);
            continue;
        }

        if (passenger.state == PassengerState::Offboard) continue;

        if (passenger.state == PassengerState::Boarding
            || passenger.state == PassengerState::Alighting)
        {
            if (!doorsOpen)
            {
                // Roll an interrupted exchange back according to the actor's original side:
                // arrivals remain in the offboard pool and departures remain aboard. Production
                // dwells provide ample crossing time; this deterministic recovery protects the
                // population balance when development time scaling closes doors prematurely.
                if (passenger.state == PassengerState::Boarding)
                {
                    moveOffboard(passenger);
                    continue;
                }
                const std::optional<std::size_t> returnSeat = passenger.exitingSeat;
                passenger.exitingSeat.reset();
                const double preferred = passenger.id % 2 == 0
                    ? -kAisleLaneX : kAisleLaneX;
                if (const auto placement = safeAislePlacement(passenger,preferred);
                    placement.has_value())
                {
                    passenger.x = placement->first;
                    passenger.z = placement->second;
                }
                else
                {
                    passenger.x = preferred;
                }
                if (returnSeat.has_value() && seatAvailable(*returnSeat))
                {
                    const Seat& seat = seats_[*returnSeat];
                    passenger.targetSeat = returnSeat;
                    passenger.state = PassengerState::WalkingToSeat;
                    passenger.targetX = seat.x;
                    passenger.targetZ = seat.z;
                    passenger.activityRemaining = 35.0
                        + std::abs(seat.z-passenger.z)/0.40;
                }
                else
                {
                    passenger.state = PassengerState::Walking;
                    passenger.targetZ = kFirstCarCentre
                        + static_cast<double>(passenger.car)*kCarLength;
                    passenger.activityRemaining = 4.0 + randomUnit()*5.0;
                }
                continue;
            }
            const double deltaX = passenger.targetX - passenger.x;
            const double candidateX = passenger.x + std::clamp(deltaX, -0.78 * dt, 0.78 * dt);
            if (canAdvance(passenger,candidateX,passenger.z)) passenger.x = candidateX;
            if (std::abs(passenger.targetX - passenger.x) < 0.04)
            {
                if (passenger.state == PassengerState::Alighting)
                {
                    // The completed alighter becomes the bounded offboard identity used at a
                    // later station. Current boarders came from that pool independently.
                    moveOffboard(passenger);
                }
                else
                {
                    const double preferred = passenger.id % 2 == 0
                        ? -kAisleLaneX : kAisleLaneX;
                    const auto lane = safeAisleLane(passenger,preferred);
                    if (!lane.has_value()) continue;
                    passenger.x = *lane;
                    passenger.state = PassengerState::Walking;
                    passenger.targetZ = kFirstCarCentre + static_cast<double>(passenger.car) * kCarLength;
                    passenger.activityRemaining = 3.0 + randomUnit() * 5.0;
                }
            }
            continue;
        }

        if (passenger.state == PassengerState::PreparingToLeave
            && !approachingStation && !atStation && !doorsOpen)
        {
            // An extreme time scale can advance through the complete stop before this actor has
            // even cleared the seat row. Reverse that reservation immediately instead of leaving
            // a stale station-exchange actor walking toward an interlocked vestibule in cruise.
            const std::optional<std::size_t> returnSeat = passenger.exitingSeat;
            passenger.exitingSeat.reset();
            if (returnSeat.has_value() && seatAvailable(*returnSeat))
            {
                const Seat& seat = seats_[*returnSeat];
                passenger.targetSeat = returnSeat;
                passenger.state = PassengerState::WalkingToSeat;
                passenger.targetX = seat.x;
                passenger.targetZ = seat.z;
                passenger.activityRemaining = 35.0 + std::abs(seat.z-passenger.z)/0.40;
            }
            else
            {
                passenger.state = PassengerState::Walking;
                passenger.targetX = passenger.x;
                passenger.targetZ = kFirstCarCentre
                    + static_cast<double>(passenger.car)*kCarLength;
                passenger.activityRemaining = 4.0 + randomUnit()*5.0;
            }
            continue;
        }

        if (passenger.exitingSeat.has_value())
        {
            if (*passenger.exitingSeat >= seats_.size())
            {
                passenger.exitingSeat.reset();
                continue;
            }
            const Seat& source = seats_[*passenger.exitingSeat];
            const double aisleX = source.x < 0.0 ? -kAisleLaneX : kAisleLaneX;
            const double candidateX = passenger.x
                + std::clamp(aisleX-passenger.x,-0.55*dt,0.55*dt);
            if (canAdvance(passenger,candidateX,passenger.z)) passenger.x = candidateX;
            if (std::abs(passenger.x-aisleX) < 0.04)
            {
                passenger.x = aisleX;
                passenger.exitingSeat.reset();
            }
            passenger.car = source.car;
            continue;
        }

        if (passenger.state == PassengerState::PreparingToLeave)
        {
            const double candidateZ = passenger.z
                + std::clamp(passenger.targetZ-passenger.z,-0.82*dt,0.82*dt);
            if (std::abs(passenger.targetZ-passenger.z) >= 0.10)
            {
                if (canAdvance(passenger,passenger.x,candidateZ)) passenger.z = candidateZ;
                else
                {
                    const double otherLane = passenger.x < 0.0
                        ? kAisleLaneX : -kAisleLaneX;
                    if (canAdvance(passenger,otherLane,candidateZ))
                    {
                        passenger.x = otherLane;
                        passenger.z = candidateZ;
                        ++passengerLaneChangeCount_;
                    }
                }
            }
            else if (atStation && doorsOpen
                     && doorsOpenElapsed_ >= kPassengerDoorPassageSeconds)
            {
                const double candidateX = passenger.x
                    + std::clamp(passenger.targetX-passenger.x,-0.62*dt,0.62*dt);
                if (canAdvance(passenger,candidateX,passenger.z)) passenger.x = candidateX;
                if (std::abs(passenger.targetX-passenger.x) < 0.04)
                {
                    passenger.x = passenger.targetX;
                    passenger.targetX = platformSide * 4.2;
                    passenger.activityRemaining = 12.0;
                    passenger.state = PassengerState::Alighting;
                }
            }
            continue;
        }

        if (passenger.state == PassengerState::VisitingToilet)
        {
            if (passenger.x < 0.55)
            {
                const double candidateZ = passenger.z
                    + std::clamp(4.07 - passenger.z, -0.72 * dt, 0.72 * dt);
                if (canAdvance(passenger, passenger.x, candidateZ)) passenger.z = candidateZ;
                if (std::abs(passenger.z - 4.07) < 0.09)
                {
                    const double candidateX = passenger.x
                        + std::clamp(0.66 - passenger.x, -0.55 * dt, 0.55 * dt);
                    if (!movesIntoPlayer(passenger, candidateX, passenger.z))
                        passenger.x = candidateX;
                }
            }
            else
            {
                passenger.z += std::clamp(5.70 - passenger.z, -0.62 * dt, 0.62 * dt);
                if (std::abs(passenger.z - 5.70) < 0.09)
                {
                    passenger.x += std::clamp(0.95 - passenger.x, -0.45 * dt, 0.45 * dt);
                    if (std::abs(passenger.x - 0.95) < 0.04)
                    {
                        passenger.state = PassengerState::UsingToilet;
                        passenger.activityRemaining = 8.0 + randomUnit() * 12.0;
                    }
                }
            }
            continue;
        }
        if (passenger.state == PassengerState::UsingToilet)
        {
            passenger.activityRemaining -= dt;
            if (passenger.activityRemaining <= 0.0)
            {
                passenger.state = PassengerState::LeavingToilet;
                passenger.targetZ = 4.07;
                passenger.targetX = kAisleLaneX;
            }
            continue;
        }
        if (passenger.state == PassengerState::LeavingToilet)
        {
            if (std::abs(passenger.z - 4.07) >= 0.09)
                passenger.z += std::clamp(4.07 - passenger.z, -0.62 * dt, 0.62 * dt);
            else
            {
                const double candidateX = passenger.x
                    + std::clamp(kAisleLaneX - passenger.x, -0.55 * dt, 0.55 * dt);
                if (!movesIntoPlayer(passenger, candidateX, passenger.z))
                    passenger.x = candidateX;
                if (std::abs(passenger.x - kAisleLaneX) < 0.04) chooseActivity(passenger);
            }
            continue;
        }

        if (passenger.state == PassengerState::WalkingToSeat)
        {
            passenger.activityRemaining -= dt;
            const bool targetValid = passenger.targetSeat.has_value()
                && *passenger.targetSeat < seats_.size()
                && !seats_[*passenger.targetSeat].occupant.has_value()
                && playerSeat_ != passenger.targetSeat
                && passenger.activityRemaining > 0.0;
            if (!targetValid)
            {
                passenger.targetSeat.reset();
                passenger.state = PassengerState::Walking;
                passenger.targetZ = kFirstCarCentre
                    + static_cast<double>(passenger.car) * kCarLength;
                passenger.activityRemaining = 4.0 + randomUnit() * 5.0;
                continue;
            }

            const Seat& target = seats_[*passenger.targetSeat];
            if (std::abs(target.z - passenger.z) >= 0.09)
            {
                const double candidateZ = passenger.z
                    + std::clamp(target.z - passenger.z, -0.82 * dt, 0.82 * dt);
                if (canAdvance(passenger, passenger.x, candidateZ))
                {
                    passenger.z = candidateZ;
                }
                else
                {
                    const double otherLane = passenger.x < 0.0
                        ? kAisleLaneX : -kAisleLaneX;
                    if (canAdvance(passenger, otherLane, candidateZ))
                    {
                        passenger.x = otherLane;
                        passenger.z = candidateZ;
                        ++passengerLaneChangeCount_;
                    }
                }
            }
            else
            {
                passenger.z += std::clamp(target.z - passenger.z, -0.55 * dt, 0.55 * dt);
                const double candidateX = passenger.x
                    + std::clamp(target.x - passenger.x, -0.55 * dt, 0.55 * dt);
                if (canAdvance(passenger,candidateX,passenger.z))
                    passenger.x = candidateX;
                if (std::abs(target.z - passenger.z) < 0.04
                    && std::abs(target.x - passenger.x) < 0.04)
                    sit(passenger, target.id);
            }
            passenger.car = std::clamp(
                static_cast<int>((passenger.z + 63.0) / kCarLength),
                0, TrainInterior::PassengerCars - 1);
            continue;
        }

        passenger.activityRemaining -= dt;
        if (passenger.state == PassengerState::Walking
            || passenger.state == PassengerState::WaitingAtVestibule)
        {
            const double delta = passenger.targetZ - passenger.z;
            const double candidateZ = passenger.z + std::clamp(delta, -0.82 * dt, 0.82 * dt);
            if (canAdvance(passenger, passenger.x, candidateZ))
            {
                passenger.z = candidateZ;
            }
            else
            {
                // Regular passengers use the same two-lane topology as the conductor. A blocked
                // walker changes lane only when the complete next step is clear, avoiding both
                // body overlap and indefinite face-to-face queues in gangways.
                const double otherLane = passenger.x < 0.0
                    ? kAisleLaneX : -kAisleLaneX;
                if (canAdvance(passenger, otherLane, candidateZ))
                {
                    passenger.x = otherLane;
                    passenger.z = candidateZ;
                    ++passengerLaneChangeCount_;
                }
            }
            passenger.car = std::clamp(
                static_cast<int>((passenger.z + 63.0) / kCarLength),
                0, TrainInterior::PassengerCars - 1);
        }
        if (passenger.activityRemaining <= 0.0)
        {
            if (std::abs(passenger.x) > 0.40)
            {
                const double preferred = std::copysign(kAisleLaneX,passenger.x);
                const auto lane = safeAisleLane(passenger,preferred);
                if (!lane.has_value())
                {
                    passenger.activityRemaining = 0.5;
                    continue;
                }
                passenger.x = *lane;
            }
            chooseActivity(passenger);
        }
    }
    synchronizeConversations();
}

void PassengerSystem::prepareForStation(double platformSide)
{
    platformSide = platformSide < 0.0 ? -1.0 : 1.0;
    stationPreparedPassengerCount_ = 0;
    lastStationExchangeParticipantCount_ = 0;
    std::array<bool,TrainInterior::PassengerCars> usedDoors{};
    const std::size_t regularPassengerCount = conductorIndex_;
    if (regularPassengerCount == 0) return;
    const std::size_t first = static_cast<std::size_t>(
        nextRandom() % static_cast<std::uint64_t>(regularPassengerCount));

    for (std::size_t offset = 0;
         offset < regularPassengerCount && stationPreparedPassengerCount_ < 2; ++offset)
    {
        Passenger& passenger = passengers_[(first+offset)%regularPassengerCount];
        if (passenger.role != PassengerRole::Passenger || !passenger.seat.has_value()
            || !seatApproachClear(*passenger.seat)) continue;
        const int doorCar = passenger.car;
        if (doorCar < 0 || doorCar >= TrainInterior::PassengerCars
            || usedDoors[static_cast<std::size_t>(doorCar)]) continue;
        const double doorZ = TrainInterior::passengerDoorZ(doorCar);
        const bool actorAtDoor = std::any_of(
            passengers_.begin(),passengers_.end(),[&passenger,doorZ](const Passenger& other) {
                return &other != &passenger && other.role != PassengerRole::Driver
                    && !other.seat.has_value()
                    && std::hypot(other.x,other.z-doorZ) < kActorSeparation;
            });
        if (actorAtDoor) continue;

        const std::size_t sourceSeat = *passenger.seat;
        stand(passenger);
        passenger.exitingSeat = sourceSeat;
        passenger.state = PassengerState::PreparingToLeave;
        passenger.targetZ = doorZ;
        passenger.targetX = platformSide * 0.18;
        passenger.activityRemaining = 90.0;
        usedDoors[static_cast<std::size_t>(doorCar)] = true;
        ++stationPreparedPassengerCount_;
    }
}

void PassengerSystem::simulateStationExchange(double platformSide)
{
    platformSide = platformSide < 0.0 ? -1.0 : 1.0;
    lastStationExchangeParticipantCount_ = stationPreparationPerformed_
        ? stationPreparedPassengerCount_ : 0;
    std::array<bool,TrainInterior::PassengerCars> usedDoors{};
    std::array<bool,TrainInterior::PassengerCars> staffDoors{};
    std::array<bool,TrainInterior::PassengerCars> secondaryDoorSlots{};
    std::array<double,TrainInterior::PassengerCars> reservedBoardingZ{};
    reservedBoardingZ.fill(std::numeric_limits<double>::quiet_NaN());
    if (conductorStationDuty_ && conductorIndex_ < passengers_.size())
    {
        constexpr double firstDoorZ = TrainInterior::passengerDoorZ(0);
        const int conductorDoor = std::clamp(static_cast<int>(std::lround(
            (passengers_[conductorIndex_].targetZ-firstDoorZ)/kCarLength)),
            0,TrainInterior::PassengerCars-1);
        usedDoors[static_cast<std::size_t>(conductorDoor)] = true;
        staffDoors[static_cast<std::size_t>(conductorDoor)] = true;
    }
    for (const Passenger& passenger : passengers_)
        if ((passenger.state == PassengerState::PreparingToLeave
             || passenger.state == PassengerState::Boarding
             || passenger.state == PassengerState::Alighting)
            && passenger.car >= 0 && passenger.car < TrainInterior::PassengerCars)
            usedDoors[static_cast<std::size_t>(passenger.car)] = true;

    // Preserve one clear half of each already assigned outbound doorway for an arrival. This is
    // evaluated before adding new actors, so a dense saloon cannot consume every boarding slot
    // between selecting the alighter and selecting the matching platform passenger.
    for (const Passenger& outbound : passengers_)
    {
        if ((outbound.state != PassengerState::PreparingToLeave
             && outbound.state != PassengerState::Alighting)
            || outbound.car < 0 || outbound.car >= TrainInterior::PassengerCars) continue;
        const int car = outbound.car;
        const double centreZ = TrainInterior::passengerDoorZ(car);
        for (double slotOffset : {-0.66,0.66})
        {
            const double slotZ = centreZ+slotOffset;
            const bool occupied = std::any_of(
                passengers_.begin(),passengers_.end(),
                [&outbound,platformSide,slotZ](const Passenger& other) {
                    return &other != &outbound && other.role != PassengerRole::Driver
                        && other.state != PassengerState::Offboard
                        && !other.seat.has_value()
                        && std::hypot(other.x-platformSide*0.18,other.z-slotZ)
                           < kActorSeparation;
                });
            if (occupied) continue;
            reservedBoardingZ[static_cast<std::size_t>(car)] = slotZ;
            break;
        }
    }
    int alighting = static_cast<int>(std::count_if(
        passengers_.begin(),passengers_.end(),[](const Passenger& passenger) {
            return passenger.state == PassengerState::PreparingToLeave
                || passenger.state == PassengerState::Alighting;
        }));
    alighting = std::min(alighting,2);
    const int eligibleOnboard = static_cast<int>(std::count_if(
        passengers_.begin(),passengers_.end(),[](const Passenger& passenger) {
            return passenger.role == PassengerRole::Passenger
                && passenger.state != PassengerState::Offboard
                && passenger.state != PassengerState::PreparingToLeave
                && passenger.state != PassengerState::Boarding
                && passenger.state != PassengerState::Alighting
                && !isToiletState(passenger.state);
        }));
    const int desiredAlighting = std::min(2,alighting+eligibleOnboard);
    int boarding = static_cast<int>(std::count_if(
        passengers_.begin(),passengers_.end(),[](const Passenger& passenger) {
            return passenger.state == PassengerState::Boarding;
        }));
    const int offboard = static_cast<int>(offboardPassengerCount());
    // End each successful call with the two-identity reserve restored. A third doorway slot can
    // gradually recover an exchange shortened by an earlier accelerated or obstructed dwell.
    const int desiredBoarding = std::min(offboard,std::clamp(
        offboard+desiredAlighting-static_cast<int>(kOffboardPassengerPool),0,3));
    boarding = std::min(boarding,desiredBoarding);
    for (int flow = 0; flow < 2; ++flow)
    {
        const bool assignAlighter = flow == 0;
        for (Passenger& passenger : passengers_)
        {
            if ((assignAlighter && alighting >= desiredAlighting)
                || (!assignAlighter && boarding >= desiredBoarding)) break;
            if (passenger.role != PassengerRole::Passenger
                || passenger.state == PassengerState::PreparingToLeave
                || passenger.state == PassengerState::Boarding
                || passenger.state == PassengerState::Alighting
                || isToiletState(passenger.state)) continue;
            if (assignAlighter && passenger.state == PassengerState::Offboard) continue;
            if (!assignAlighter && passenger.state != PassengerState::Offboard) continue;
            const int firstCar = static_cast<int>(
                nextRandom() % static_cast<std::uint64_t>(TrainInterior::PassengerCars));
            int doorCar = -1;
            double actorDoorZ = 0.0;
            for (int offset = 0; offset < TrainInterior::PassengerCars; ++offset)
            {
                const int candidate = (firstCar+offset) % TrainInterior::PassengerCars;
                if (usedDoors[static_cast<std::size_t>(candidate)]) continue;
                const double candidateZ = TrainInterior::passengerDoorZ(candidate);
                const bool actorAtDoor = std::any_of(
                    passengers_.begin(),passengers_.end(),
                    [&passenger,platformSide,candidateZ](const Passenger& other) {
                        return &other != &passenger && other.role != PassengerRole::Driver
                            && !other.seat.has_value()
                            && std::hypot(other.x-platformSide*0.18,other.z-candidateZ)
                               < kActorSeparation;
                    });
                if (actorAtDoor) continue;
                double pairedBoardingZ = std::numeric_limits<double>::quiet_NaN();
                if (assignAlighter && desiredBoarding > boarding)
                {
                    for (double slotOffset : {-0.66,0.66})
                    {
                        const double slotZ = candidateZ+slotOffset;
                        const bool slotOccupied = std::any_of(
                            passengers_.begin(),passengers_.end(),
                            [&passenger,platformSide,slotZ](const Passenger& other) {
                                return &other != &passenger
                                    && other.role != PassengerRole::Driver
                                    && other.state != PassengerState::Offboard
                                    && !other.seat.has_value()
                                    && std::hypot(other.x-platformSide*0.18,other.z-slotZ)
                                       < kActorSeparation;
                            });
                        if (slotOccupied) continue;
                        pairedBoardingZ = slotZ;
                        break;
                    }
                    if (!std::isfinite(pairedBoardingZ)) continue;
                }
                doorCar = candidate;
                actorDoorZ = candidateZ;
                if (std::isfinite(pairedBoardingZ))
                    reservedBoardingZ[static_cast<std::size_t>(candidate)] = pairedBoardingZ;
                break;
            }
            // Real double doors carry opposing streams. If every unused aperture is obstructed,
            // let a boarding passenger use one side of an alighter's 1.56 m opening.
            if (doorCar < 0 && !assignAlighter)
            {
                for (int candidate = 0; candidate < TrainInterior::PassengerCars
                                      && doorCar < 0; ++candidate)
                {
                    if (staffDoors[static_cast<std::size_t>(candidate)]
                        || secondaryDoorSlots[static_cast<std::size_t>(candidate)]) continue;
                    const double slotZ = reservedBoardingZ[static_cast<std::size_t>(candidate)];
                    if (!std::isfinite(slotZ)) continue;
                    doorCar = candidate;
                    actorDoorZ = slotZ;
                    secondaryDoorSlots[static_cast<std::size_t>(candidate)] = true;
                }
            }
            if (doorCar < 0) break;
            usedDoors[static_cast<std::size_t>(doorCar)] = true;
            const std::optional<std::size_t> returnSeat = passenger.seat;
            stand(passenger);
            passenger.car = doorCar;
            passenger.z = actorDoorZ;
            passenger.activityRemaining = 12.0;
            ++lastStationExchangeParticipantCount_;
            if (assignAlighter)
            {
                passenger.exitingSeat = returnSeat;
                passenger.x = platformSide * 0.18;
                passenger.targetX = platformSide * 4.2;
                passenger.state = PassengerState::Alighting;
                ++alighting;
            }
            else
            {
                passenger.x = platformSide * 4.2;
                passenger.targetX = 0.0;
                passenger.state = PassengerState::Boarding;
                ++boarding;
            }
        }
    }
}

void PassengerSystem::resetStationCycleForTeleport()
{
    doorsOpenElapsed_ = 0.0;
    stationExchangePerformed_ = false;
    stationPreparationPerformed_ = false;
    stationPreparedPassengerCount_ = 0;
    lastStationExchangeParticipantCount_ = 0;

    for (Passenger& passenger : passengers_)
    {
        if (passenger.role == PassengerRole::Conductor)
        {
            if (conductorStationDuty_)
            {
                passenger.targetZ = conductorRoundTargetZ_;
                passenger.state = PassengerState::Walking;
                passenger.x = passenger.id % 2 == 0 ? -kAisleLaneX : kAisleLaneX;
            }
            continue;
        }
        if (passenger.role != PassengerRole::Passenger) continue;
        if (passenger.state == PassengerState::Boarding)
        {
            // This identity had not completed boarding, so it remains available at the newly
            // selected station instead of being silently inserted into the train.
            moveOffboard(passenger);
            continue;
        }
        if (passenger.state != PassengerState::Alighting
            && passenger.state != PassengerState::PreparingToLeave) continue;

        const std::optional<std::size_t> returnSeat = passenger.exitingSeat;
        passenger.exitingSeat.reset();
        passenger.x = passenger.id % 2 == 0 ? -kAisleLaneX : kAisleLaneX;
        if (returnSeat.has_value() && seatAvailable(*returnSeat))
        {
            const Seat& seat = seats_[*returnSeat];
            passenger.targetSeat = returnSeat;
            passenger.state = PassengerState::WalkingToSeat;
            passenger.targetX = seat.x;
            passenger.targetZ = seat.z;
            passenger.activityRemaining = 35.0 + std::abs(seat.z-passenger.z)/0.40;
        }
        else
        {
            passenger.state = PassengerState::Walking;
            passenger.targetX = passenger.x;
            passenger.targetZ = kFirstCarCentre
                + static_cast<double>(std::clamp(passenger.car,0,TrainInterior::PassengerCars-1))
                    * kCarLength;
            passenger.activityRemaining = 4.0 + randomUnit()*5.0;
        }
    }
    conductorStationDuty_ = false;
}

std::size_t PassengerSystem::occupiedSeatCount() const noexcept
{
    const auto npcSeats = static_cast<std::size_t>(std::count_if(seats_.begin(), seats_.end(),
        [](const Seat& seat) { return seat.occupant.has_value(); }));
    return npcSeats + static_cast<std::size_t>(playerSeat_.has_value());
}

std::size_t PassengerSystem::conversationPairCount() const noexcept
{
    std::size_t pairs = 0;
    for (const Passenger& passenger : passengers_)
    {
        if (passenger.state != PassengerState::Talking) continue;
        const auto partner = adjacentSeatedPassenger(passenger);
        if (partner.has_value() && passengers_[*partner].state == PassengerState::Talking
            && passenger.id < *partner)
            ++pairs;
    }
    return pairs;
}

std::size_t PassengerSystem::stationExchangeActorCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        passengers_.begin(), passengers_.end(), [](const Passenger& passenger) {
            return passenger.state == PassengerState::PreparingToLeave
                || passenger.state == PassengerState::Boarding
                || passenger.state == PassengerState::Alighting;
        }));
}

std::size_t PassengerSystem::offboardPassengerCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        passengers_.begin(),passengers_.end(),[](const Passenger& passenger) {
            return passenger.state == PassengerState::Offboard;
        }));
}

std::size_t PassengerSystem::walkingToSeatCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        passengers_.begin(), passengers_.end(), [](const Passenger& passenger) {
            return passenger.state == PassengerState::WalkingToSeat;
        }));
}

bool PassengerSystem::seatAvailable(std::size_t seatIndex) const noexcept
{
    if (seatIndex >= seats_.size() || seats_[seatIndex].occupant.has_value()
        || playerSeat_ == seatIndex) return false;
    const Seat& candidate = seats_[seatIndex];
    return std::none_of(passengers_.begin(), passengers_.end(),
        [this,seatIndex,&candidate](const Passenger& passenger) {
            const auto blocksCandidate = [this,seatIndex,&candidate](
                                             std::optional<std::size_t> reservation) {
                if (!reservation.has_value() || *reservation >= seats_.size()) return false;
                if (*reservation == seatIndex) return true;
                const Seat& reserved = seats_[*reservation];
                return reserved.window && !candidate.window
                    && reserved.car == candidate.car
                    && reserved.x * candidate.x > 0.0
                    && std::abs(reserved.z-candidate.z) < 0.01
                    && std::abs(candidate.x) < std::abs(reserved.x);
            };
            // A passenger crossing to a reserved window seat temporarily owns the inner access
            // position as well. The same rule holds while leaving an old window seat.
            return blocksCandidate(passenger.targetSeat)
                || blocksCandidate(passenger.exitingSeat);
        });
}

bool PassengerSystem::seatApproachClear(std::size_t seatIndex) const noexcept
{
    if (seatIndex >= seats_.size()) return false;
    const Seat& target = seats_[seatIndex];
    if (!target.window) return true;
    for (const Seat& candidate : seats_)
    {
        if (candidate.car != target.car || candidate.window
            || candidate.x * target.x <= 0.0
            || std::abs(candidate.z-target.z) >= 0.01
            || std::abs(candidate.x) >= std::abs(target.x)) continue;
        return seatAvailable(candidate.id);
    }
    return true;
}

bool PassengerSystem::occupySeatForPlayer(std::size_t seatIndex) noexcept
{
    if (!seatAvailable(seatIndex) || !seatApproachClear(seatIndex)) return false;
    playerSeat_ = seatIndex;
    return true;
}

void PassengerSystem::releasePlayerSeat() noexcept
{
    playerSeat_.reset();
}

bool PassengerSystem::toiletDoorRequestedOpen() const noexcept
{
    return std::any_of(passengers_.begin(), passengers_.end(), [](const Passenger& passenger) {
        return (passenger.state == PassengerState::VisitingToilet
                && (std::abs(passenger.z - 4.07) < 1.2 || passenger.x > 0.30))
            || passenger.state == PassengerState::LeavingToilet;
    });
}

bool PassengerSystem::toiletOccupied() const noexcept
{
    return std::any_of(passengers_.begin(), passengers_.end(), [](const Passenger& passenger) {
        return passenger.state == PassengerState::UsingToilet;
    });
}

bool PassengerSystem::toiletReserved() const noexcept
{
    return std::any_of(passengers_.begin(), passengers_.end(), [](const Passenger& passenger) {
        return isToiletState(passenger.state);
    });
}

bool PassengerSystem::invariantsHold() const noexcept
{
    if (playerSeat_.has_value())
    {
        if (*playerSeat_ >= seats_.size()) return false;
        if (seats_[*playerSeat_].occupant.has_value()) return false;
    }
    std::array<bool, 256> passengerHasSeat{};
    std::array<bool, 256> passengerTargetsSeat{};
    if (passengers_.size() > passengerHasSeat.size()) return false;
    if (seats_.size() > passengerTargetsSeat.size()) return false;
    for (std::size_t index = 0; index < seats_.size(); ++index)
    {
        const Seat& seat = seats_[index];
        if (!seat.occupant.has_value()) continue;
        if (*seat.occupant >= passengers_.size() || passengerHasSeat[*seat.occupant]) return false;
        const Passenger& passenger = passengers_[*seat.occupant];
        if (!passenger.seat.has_value() || *passenger.seat != index) return false;
        passengerHasSeat[*seat.occupant] = true;
    }
    for (const Passenger& passenger : passengers_)
    {
        if (passenger.state == PassengerState::Offboard)
        {
            if (passenger.role != PassengerRole::Passenger || passenger.car != -1
                || passenger.seat.has_value() || passenger.targetSeat.has_value()
                || passenger.exitingSeat.has_value()) return false;
            continue;
        }
        if (passenger.seat.has_value() && !passengerHasSeat[passenger.id]) return false;
        if (passenger.targetSeat.has_value())
        {
            const std::size_t target = *passenger.targetSeat;
            if (passenger.state != PassengerState::WalkingToSeat || target >= seats_.size()
                || seats_[target].occupant.has_value() || playerSeat_ == target
                || passengerTargetsSeat[target]) return false;
            passengerTargetsSeat[target] = true;
        }
        else if (passenger.state == PassengerState::WalkingToSeat)
        {
            return false;
        }
        if (passenger.exitingSeat.has_value())
        {
            const std::size_t source = *passenger.exitingSeat;
            if (source >= seats_.size() || passenger.seat.has_value()
                || passenger.role != PassengerRole::Passenger
                || passengerTargetsSeat[source]) return false;
            passengerTargetsSeat[source] = true;
        }
        if (passenger.state == PassengerState::Talking)
        {
            const auto partner = adjacentSeatedPassenger(passenger);
            if (!partner.has_value() || passengers_[*partner].state != PassengerState::Talking)
                return false;
        }
    }
    return true;
}

} // namespace TrainSim::Sim
