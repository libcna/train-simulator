#include "trainsim/sim/RouteSimulation.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>

namespace TrainSim::Sim {

namespace {
constexpr double kMaximumSpeed = 44.44;        // 160 km/h
constexpr double kAcceleration = 0.62;         // comfortable intercity acceleration
constexpr double kServiceBrake = 0.78;
constexpr double kCoastingDeceleration = 0.035;
constexpr double kCoastingLeadDistance = 650.0;
constexpr double kClosingSeconds = 4.0;
constexpr double kStopTolerance = 0.45;
}

RouteSimulation::RouteSimulation(const Route& route) : route_(route)
{
    state_.platformOnLeft = route_.stations().front().platformOnLeft;
    state_.dwellRemainingSeconds = route_.stations().front().dwellSeconds;
    state_.scheduledNextArrivalSeconds = state_.dwellRemainingSeconds + kClosingSeconds
        + scheduledTravelSecondsToNext();
}

void RouteSimulation::setDevelopmentDwellScale(double scale)
{
    if (!(scale > 0.0 && scale <= 1.0))
        throw std::invalid_argument("Dwell scale must be in (0, 1]");
    dwellScale_ = scale;
    if (state_.phase == ServicePhase::Dwelling)
    {
        const double previousRemaining = state_.dwellRemainingSeconds;
        state_.dwellRemainingSeconds = std::min(
            previousRemaining, route_.stations()[state_.currentStation].dwellSeconds * scale);
        state_.scheduledNextArrivalSeconds += state_.dwellRemainingSeconds - previousRemaining;
    }
}

void RouteSimulation::teleportToStation(std::size_t stationIndex, int preferredDirection)
{
    if (stationIndex >= route_.stations().size())
        throw std::out_of_range("Station index is outside the route");
    const std::size_t last = route_.stations().size() - 1;
    int direction = preferredDirection < 0 ? -1 : 1;
    if (stationIndex == 0) direction = 1;
    if (stationIndex == last) direction = -1;

    state_.routeDistanceMetres = route_.stations()[stationIndex].distanceMetres;
    state_.speedMetresPerSecond = 0.0;
    state_.longitudinalAccelerationMetresPerSecondSquared = 0.0;
    state_.targetSpeedMetresPerSecond = 0.0;
    state_.direction = direction;
    state_.currentStation = stationIndex;
    state_.nextStation = direction > 0 ? stationIndex + 1 : stationIndex - 1;
    state_.dwellRemainingSeconds = route_.stations()[stationIndex].dwellSeconds * dwellScale_;
    state_.phase = ServicePhase::Dwelling;
    state_.doorsOpen = true;
    const bool eastboundLeft = route_.stations()[stationIndex].platformOnLeft;
    state_.platformOnLeft = direction > 0 ? eastboundLeft : !eastboundLeft;
    state_.serviceElapsedSeconds = 0.0;
    state_.lastArrivalDeviationSeconds = 0.0;
    state_.scheduledNextArrivalSeconds = state_.dwellRemainingSeconds + kClosingSeconds
        + scheduledTravelSecondsToNext();
    closingRemaining_ = 0.0;
}

void RouteSimulation::teleportToRouteDistance(double routeDistanceMetres, int preferredDirection,
                                               double speedMetresPerSecond)
{
    if (!std::isfinite(routeDistanceMetres) || routeDistanceMetres < 0.0
        || routeDistanceMetres > route_.lengthMetres())
        throw std::out_of_range("Route distance is outside the route");
    if (!std::isfinite(speedMetresPerSecond) || speedMetresPerSecond < 0.0)
        throw std::invalid_argument("Teleport speed must be finite and non-negative");

    const auto& stations = route_.stations();
    const std::size_t last = stations.size() - 1;
    int direction = preferredDirection < 0 ? -1 : 1;
    if (routeDistanceMetres <= stations.front().distanceMetres) direction = 1;
    if (routeDistanceMetres >= stations.back().distanceMetres) direction = -1;

    std::size_t current = 0;
    if (direction > 0)
    {
        const auto upper = std::upper_bound(
            stations.begin(), stations.end(), routeDistanceMetres,
            [](double distance, const Station& station) {
                return distance < station.distanceMetres;
            });
        current = upper == stations.begin()
            ? 0 : static_cast<std::size_t>(std::distance(stations.begin(), upper) - 1);
        if (current == last) { direction = -1; current = last; }
    }
    else
    {
        const auto lower = std::lower_bound(
            stations.begin(), stations.end(), routeDistanceMetres,
            [](const Station& station, double distance) {
                return station.distanceMetres < distance;
            });
        current = lower == stations.end()
            ? last : static_cast<std::size_t>(std::distance(stations.begin(), lower));
        if (current == 0) { direction = 1; current = 0; }
    }

    state_.routeDistanceMetres = routeDistanceMetres;
    state_.direction = direction;
    state_.currentStation = current;
    state_.nextStation = direction > 0 ? current + 1 : current - 1;
    state_.doorsOpen = false;
    state_.dwellRemainingSeconds = 0.0;
    const bool eastboundLeft = stations[current].platformOnLeft;
    state_.platformOnLeft = direction > 0 ? eastboundLeft : !eastboundLeft;
    state_.serviceElapsedSeconds = 0.0;
    state_.lastArrivalDeviationSeconds = 0.0;
    closingRemaining_ = 0.0;
    const double limit = currentSpeedLimitMetresPerSecond();
    state_.speedMetresPerSecond = std::min(speedMetresPerSecond, limit);
    state_.longitudinalAccelerationMetresPerSecondSquared = 0.0;
    state_.targetSpeedMetresPerSecond = limit;
    state_.phase = state_.speedMetresPerSecond >= limit - 0.3
        ? ServicePhase::Cruising : ServicePhase::Accelerating;
    state_.scheduledNextArrivalSeconds = estimatedSecondsToNextStation();
}

double RouteSimulation::scheduledTravelSecondsToNext() const noexcept
{
    const std::size_t segmentEnd = std::max(state_.currentStation, state_.nextStation);
    return route_.stations()[segmentEnd].scheduledTravelSecondsFromPrevious;
}

double RouteSimulation::distanceToNextStation() const noexcept
{
    const double next = route_.stations()[state_.nextStation].distanceMetres;
    return std::max(0.0, (next - state_.routeDistanceMetres) * static_cast<double>(state_.direction));
}

bool RouteSimulation::nextPlatformOnNegativeTrainX() const noexcept
{
    // Station sides are authored in the consist's stable local coordinates. The service-state
    // value is direction-relative because it is also presented to passengers as "left/right".
    return route_.stations()[state_.nextStation].platformOnLeft;
}

double RouteSimulation::currentSpeedLimitMetresPerSecond() const noexcept
{
    const std::size_t segmentEnd = std::max(state_.currentStation, state_.nextStation);
    return std::min(kMaximumSpeed,
                    route_.stations()[segmentEnd].speedLimitMetresPerSecondFromPrevious);
}

double RouteSimulation::estimatedSecondsToNextStation() const noexcept
{
    const double distance = distanceToNextStation();
    const double maximumSpeed = currentSpeedLimitMetresPerSecond();
    const double speed = std::clamp(state_.speedMetresPerSecond, 0.0, maximumSpeed);
    const double accelerationDistance = std::max(
        0.0, (maximumSpeed * maximumSpeed - speed * speed) / (2.0 * kAcceleration));
    const double brakingDistance = maximumSpeed * maximumSpeed / (2.0 * kServiceBrake);
    double travelSeconds = 0.0;
    if (accelerationDistance + brakingDistance <= distance)
    {
        travelSeconds = (maximumSpeed - speed) / kAcceleration
                      + (distance - accelerationDistance - brakingDistance) / maximumSpeed
                      + maximumSpeed / kServiceBrake;
    }
    else
    {
        const double peakSquared = std::max(0.0,
            (2.0 * kAcceleration * kServiceBrake * distance
             + kServiceBrake * speed * speed) / (kAcceleration + kServiceBrake));
        const double peak = std::sqrt(peakSquared);
        travelSeconds = peak >= speed
            ? (peak - speed) / kAcceleration + peak / kServiceBrake
            : speed / kServiceBrake;
    }

    if (state_.phase == ServicePhase::Dwelling)
        travelSeconds += std::max(0.0, state_.dwellRemainingSeconds) + kClosingSeconds;
    else if (state_.phase == ServicePhase::ClosingDoors)
        travelSeconds += std::max(0.0, closingRemaining_);
    return travelSeconds;
}

double RouteSimulation::predictedScheduleDeviationSeconds() const noexcept
{
    return state_.serviceElapsedSeconds + estimatedSecondsToNextStation()
         - state_.scheduledNextArrivalSeconds;
}

double RouteSimulation::journeyProgress() const noexcept
{
    const double raw = state_.routeDistanceMetres / route_.lengthMetres();
    return state_.direction > 0 ? raw : 1.0 - raw;
}

std::string RouteSimulation::destinationName() const
{
    return state_.direction > 0 ? route_.stations().back().name : route_.stations().front().name;
}

std::string RouteSimulation::phaseName() const
{
    switch (state_.phase)
    {
        case ServicePhase::Dwelling: return "DWELLING";
        case ServicePhase::ClosingDoors: return "DOORS CLOSING";
        case ServicePhase::Accelerating: return "ACCELERATING";
        case ServicePhase::Cruising: return "CRUISING";
        case ServicePhase::Coasting: return "COASTING";
        case ServicePhase::Braking: return "BRAKING";
    }
    return "UNKNOWN";
}

void RouteSimulation::beginDwell()
{
    const double arrivedOnSchedule = state_.scheduledNextArrivalSeconds;
    state_.routeDistanceMetres = route_.stations()[state_.nextStation].distanceMetres;
    state_.speedMetresPerSecond = 0.0;
    state_.longitudinalAccelerationMetresPerSecondSquared = 0.0;
    state_.targetSpeedMetresPerSecond = 0.0;
    state_.currentStation = state_.nextStation;
    const std::size_t last = route_.stations().size() - 1;
    if ((state_.direction > 0 && state_.currentStation == last)
        || (state_.direction < 0 && state_.currentStation == 0))
        state_.direction = -state_.direction;
    state_.nextStation = state_.direction > 0
        ? state_.currentStation + 1 : state_.currentStation - 1;
    state_.phase = ServicePhase::Dwelling;
    state_.doorsOpen = true;
    const bool eastboundLeft = route_.stations()[state_.currentStation].platformOnLeft;
    state_.platformOnLeft = state_.direction > 0 ? eastboundLeft : !eastboundLeft;
    state_.dwellRemainingSeconds = route_.stations()[state_.currentStation].dwellSeconds * dwellScale_;
    state_.lastArrivalDeviationSeconds = state_.serviceElapsedSeconds
                                       - arrivedOnSchedule;
    state_.scheduledNextArrivalSeconds = arrivedOnSchedule
        + state_.dwellRemainingSeconds + kClosingSeconds + scheduledTravelSecondsToNext();
    ++state_.completedLegs;
}

void RouteSimulation::finishDwell()
{
    state_.doorsOpen = false;
    state_.phase = ServicePhase::ClosingDoors;
    state_.longitudinalAccelerationMetresPerSecondSquared = 0.0;
    closingRemaining_ = kClosingSeconds;
}

void RouteSimulation::update(double simulationSeconds)
{
    if (frozen_ || simulationSeconds <= 0.0) return;

    double remaining = std::min(simulationSeconds, 2.0);
    state_.serviceElapsedSeconds += remaining;
    while (remaining > 1e-9)
    {
        const double dt = std::min(remaining, 0.05);
        remaining -= dt;

        if (state_.phase == ServicePhase::Dwelling)
        {
            state_.longitudinalAccelerationMetresPerSecondSquared = 0.0;
            state_.dwellRemainingSeconds -= dt;
            if (state_.dwellRemainingSeconds <= 0.0) finishDwell();
            continue;
        }
        if (state_.phase == ServicePhase::ClosingDoors)
        {
            state_.longitudinalAccelerationMetresPerSecondSquared = 0.0;
            closingRemaining_ -= dt;
            if (closingRemaining_ <= 0.0) state_.phase = ServicePhase::Accelerating;
            continue;
        }

        const double distance = distanceToNextStation();
        const double speedLimit = currentSpeedLimitMetresPerSecond();
        // The maximum speed from which the remaining distance can be stopped with service brake.
        // Unlike a binary stopping-distance trigger, this envelope still asks for a small positive
        // speed when the train is a few metres short, so numerical integration cannot strand the
        // consist just outside the platform marker.
        const double stoppingSpeed = std::sqrt(2.0 * kServiceBrake * std::max(0.0, distance - 0.20));
        state_.targetSpeedMetresPerSecond = std::min(speedLimit, stoppingSpeed);
        const bool mustBrake = state_.speedMetresPerSecond > state_.targetSpeedMetresPerSecond + 0.02;
        const double brakingDistanceAtCurrentSpeed =
            state_.speedMetresPerSecond * state_.speedMetresPerSecond / (2.0 * kServiceBrake);
        const bool shouldCoast = state_.speedMetresPerSecond > 10.0
            && distance > brakingDistanceAtCurrentSpeed + 0.20
            && distance < brakingDistanceAtCurrentSpeed + kCoastingLeadDistance;
        state_.phase = mustBrake ? ServicePhase::Braking
                     : shouldCoast ? ServicePhase::Coasting
                     : (state_.speedMetresPerSecond < speedLimit - 0.3
                            ? ServicePhase::Accelerating : ServicePhase::Cruising);

        const double previousSpeed = state_.speedMetresPerSecond;
        if (mustBrake)
            state_.speedMetresPerSecond = std::max(state_.targetSpeedMetresPerSecond,
                                                   state_.speedMetresPerSecond - kServiceBrake * dt);
        else if (shouldCoast)
            state_.speedMetresPerSecond = std::max(
                0.0, state_.speedMetresPerSecond - kCoastingDeceleration * dt);
        else
            state_.speedMetresPerSecond = std::min(state_.targetSpeedMetresPerSecond,
                                                   state_.speedMetresPerSecond + kAcceleration * dt);
        state_.longitudinalAccelerationMetresPerSecondSquared =
            (state_.speedMetresPerSecond-previousSpeed)/dt;

        const double travel = state_.speedMetresPerSecond * dt * static_cast<double>(state_.direction);
        state_.routeDistanceMetres = std::clamp(state_.routeDistanceMetres + travel, 0.0, route_.lengthMetres());

        if (distance <= kStopTolerance && state_.speedMetresPerSecond < 0.9)
            beginDwell();
    }
}

} // namespace TrainSim::Sim
