#pragma once

#include "trainsim/sim/Route.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace TrainSim::Sim {

enum class ServicePhase : std::uint8_t
{
    Dwelling,
    ClosingDoors,
    Accelerating,
    Cruising,
    Coasting,
    Braking
};

struct ServiceState
{
    double routeDistanceMetres = 0.0;
    double speedMetresPerSecond = 0.0;
    // Signed along the active direction of travel: positive under traction and negative while
    // coasting or braking. Consumers do not have to infer physical motion from a UI phase.
    double longitudinalAccelerationMetresPerSecondSquared = 0.0;
    double targetSpeedMetresPerSecond = 0.0;
    int direction = 1;
    std::size_t currentStation = 0;
    std::size_t nextStation = 1;
    double dwellRemainingSeconds = 12.0;
    ServicePhase phase = ServicePhase::Dwelling;
    bool doorsOpen = true;
    // Relative to the active direction of travel; use platformOnNegativeTrainX for fixed
    // train-local rendering and interaction coordinates.
    bool platformOnLeft = true;
    std::uint64_t completedLegs = 0;
    double serviceElapsedSeconds = 0.0;
    double scheduledNextArrivalSeconds = 0.0;
    double lastArrivalDeviationSeconds = 0.0;
};

[[nodiscard]] constexpr bool platformOnNegativeTrainX(const ServiceState& state) noexcept
{
    return state.direction > 0 ? state.platformOnLeft : !state.platformOnLeft;
}

class RouteSimulation
{
public:
    explicit RouteSimulation(const Route& route);

    void update(double simulationSeconds);
    void setDevelopmentDwellScale(double scale);
    void teleportToStation(std::size_t stationIndex, int preferredDirection = 1);
    void teleportToRouteDistance(double routeDistanceMetres, int preferredDirection = 1,
                                 double speedMetresPerSecond = 33.33);
    void freeze(bool frozen) noexcept
    {
        frozen_ = frozen;
        if (frozen_) state_.longitudinalAccelerationMetresPerSecondSquared = 0.0;
    }
    [[nodiscard]] bool frozen() const noexcept { return frozen_; }
    [[nodiscard]] const ServiceState& state() const noexcept { return state_; }
    [[nodiscard]] double distanceToNextStation() const noexcept;
    // Fixed train-local side of the next scheduled platform. This deliberately does not use
    // ServiceState::platformOnLeft, whose meaning follows the active direction at a stop.
    [[nodiscard]] bool nextPlatformOnNegativeTrainX() const noexcept;
    [[nodiscard]] double currentSpeedLimitMetresPerSecond() const noexcept;
    [[nodiscard]] double estimatedSecondsToNextStation() const noexcept;
    [[nodiscard]] double predictedScheduleDeviationSeconds() const noexcept;
    [[nodiscard]] double journeyProgress() const noexcept;
    [[nodiscard]] std::string destinationName() const;
    [[nodiscard]] std::string phaseName() const;

private:
    void beginDwell();
    void finishDwell();
    [[nodiscard]] double scheduledTravelSecondsToNext() const noexcept;

    const Route& route_;
    ServiceState state_;
    double dwellScale_ = 1.0;
    double closingRemaining_ = 0.0;
    bool frozen_ = false;
};

} // namespace TrainSim::Sim
