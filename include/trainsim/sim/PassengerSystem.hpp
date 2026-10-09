#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace TrainSim::Sim {

enum class PassengerRole : std::uint8_t { Passenger, Conductor, Driver };
enum class PassengerState : std::uint8_t {
    Offboard, Seated, Standing, Walking, WalkingToSeat, LookingOutside, Talking, VisitingToilet,
    WaitingAtVestibule, PreparingToLeave, UsingToilet, LeavingToilet, Boarding, Alighting
};

struct Seat
{
    std::size_t id = 0;
    int car = 0;
    double x = 0.0;
    double z = 0.0;
    bool window = false;
    bool accessible = false;
    std::optional<std::size_t> occupant;
};

struct Passenger
{
    std::size_t id = 0;
    PassengerRole role = PassengerRole::Passenger;
    PassengerState state = PassengerState::Standing;
    int car = 0;
    double x = 0.0;
    double z = 0.0;
    double targetZ = 0.0;
    double activityRemaining = 0.0;
    std::optional<std::size_t> seat;
    std::uint32_t appearanceSeed = 0;
    double targetX = 0.0;
    std::optional<std::size_t> targetSeat;
    std::optional<std::size_t> exitingSeat;
    bool yieldingToPlayer = false;
};

class PassengerSystem
{
public:
    explicit PassengerSystem(std::uint64_t seed, int passengerCars = 6, std::size_t passengerCount = 32);

    void update(double seconds, bool atStation, bool doorsOpen, double platformSide = 1.0,
                int travelDirection = 1, bool toiletAvailable = true,
                double playerX = 1'000.0, double playerZ = 1'000.0,
                bool approachingStation = false);
    void simulateStationExchange(double platformSide = 1.0);
    // Developer station teleports are allowed at any point in a dwell. Settle doorway actors and
    // arm a fresh exchange without disturbing ordinary onboard activities or toilet ownership.
    void resetStationCycleForTeleport();

    [[nodiscard]] const std::vector<Seat>& seats() const noexcept { return seats_; }
    [[nodiscard]] const std::vector<Passenger>& passengers() const noexcept { return passengers_; }
    [[nodiscard]] std::size_t occupiedSeatCount() const noexcept;
    [[nodiscard]] std::size_t conversationPairCount() const noexcept;
    [[nodiscard]] std::size_t conductorIndex() const noexcept { return conductorIndex_; }
    [[nodiscard]] std::size_t conductorInspectionCount() const noexcept
    {
        return conductorInspectionCount_;
    }
    [[nodiscard]] std::size_t passengerLaneChangeCount() const noexcept
    {
        return passengerLaneChangeCount_;
    }
    [[nodiscard]] std::size_t playerYieldCount() const noexcept
    {
        return playerYieldCount_;
    }
    [[nodiscard]] std::size_t walkingToSeatCount() const noexcept;
    [[nodiscard]] std::size_t stationExchangeActorCount() const noexcept;
    [[nodiscard]] std::size_t offboardPassengerCount() const noexcept;
    [[nodiscard]] std::size_t lastStationExchangeParticipantCount() const noexcept
    {
        return lastStationExchangeParticipantCount_;
    }
    [[nodiscard]] bool invariantsHold() const noexcept;
    [[nodiscard]] bool seatAvailable(std::size_t seatIndex) const noexcept;
    [[nodiscard]] bool seatApproachClear(std::size_t seatIndex) const noexcept;
    [[nodiscard]] bool occupySeatForPlayer(std::size_t seatIndex) noexcept;
    void releasePlayerSeat() noexcept;
    [[nodiscard]] std::optional<std::size_t> playerSeat() const noexcept { return playerSeat_; }
    [[nodiscard]] bool toiletDoorRequestedOpen() const noexcept;
    [[nodiscard]] bool toiletReserved() const noexcept;
    [[nodiscard]] bool toiletOccupied() const noexcept;

private:
    std::uint64_t nextRandom();
    double randomUnit();
    [[nodiscard]] bool seatHasOccupiedNeighbor(std::size_t seatIndex) const noexcept;
    std::optional<std::size_t> chooseFreeSeat(int preferredCar, bool requireClearApproach = false);
    [[nodiscard]] std::optional<std::size_t>
    adjacentSeatedPassenger(const Passenger& passenger) const noexcept;
    void synchronizeConversations();
    void prepareForStation(double platformSide);
    void moveOffboard(Passenger& passenger);
    void sit(Passenger& passenger, std::size_t seatIndex);
    void stand(Passenger& passenger);
    void chooseActivity(Passenger& passenger);

    std::vector<Seat> seats_;
    std::vector<Passenger> passengers_;
    std::vector<std::size_t> freeSeatScratch_;
    std::vector<std::size_t> socialSeatScratch_;
    std::uint64_t randomState_ = 0;
    std::size_t conductorIndex_ = 0;
    std::optional<std::size_t> playerSeat_;
    double doorsOpenElapsed_ = 0.0;
    bool stationExchangePerformed_ = false;
    bool stationPreparationPerformed_ = false;
    std::size_t stationPreparedPassengerCount_ = 0;
    std::size_t lastStationExchangeParticipantCount_ = 0;
    bool toiletAvailable_ = true;
    bool conductorStationDuty_ = false;
    double conductorRoundTargetZ_ = 61.0;
    std::size_t conductorInspectionCount_ = 0;
    double conductorInspectionCooldown_ = 6.0;
    std::size_t passengerLaneChangeCount_ = 0;
    std::size_t playerYieldCount_ = 0;
};

} // namespace TrainSim::Sim
