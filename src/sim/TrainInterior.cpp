#include "trainsim/sim/TrainInterior.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace TrainSim::Sim {

bool TrainInterior::toiletAvailableToPassengers(
    double playerX, double playerZ, bool playerDoorOpen) noexcept
{
    if (insideToilet(playerX,playerZ)) return false;
    return !playerDoorOpen || std::hypot(playerX-0.34,playerZ-4.1) > 1.8;
}

std::optional<int> TrainInterior::informationDisplayCar(double z, double radius) noexcept
{
    if (!std::isfinite(z) || !std::isfinite(radius) || radius < 0.0) return std::nullopt;
    for (int car = 0; car < PassengerCars; ++car)
    {
        const double centre = FirstCarCentre + static_cast<double>(car) * CarLength;
        if (std::abs(z - centre) < radius) return car;
    }
    return std::nullopt;
}

bool TrainInterior::gangwaySensorContains(int index, double z) noexcept
{
    return index >= 0 && index < GangwayCount && std::isfinite(z)
        && std::abs(z - gangwayZ(index)) < GangwaySensorRadius;
}

bool TrainInterior::nonCarpetedFloorAt(double x, double z) noexcept
{
    if (!std::isfinite(x) || !std::isfinite(z)) return false;
    if (insideToilet(x,z)) return true;
    for (int car = 0; car < PassengerCars; ++car)
        if (std::abs(z-passengerDoorZ(car)) <= VestibuleFloorHalfLength) return true;
    for (int gangway = 0; gangway < GangwayCount; ++gangway)
        if (std::abs(z-gangwayZ(gangway)) <= GangwayFloorHalfLength) return true;
    return false;
}

int TrainInterior::seatRowIndex(int car, double z) noexcept
{
    if (car < 0 || car >= PassengerCars || !std::isfinite(z)) return -1;
    const double first = seatRowZ(car, 0);
    return std::clamp(static_cast<int>(std::lround((z - first) / WindowBayPitch)),
                      0, SeatRowsPerCar - 1);
}

bool TrainInterior::interactionTargetVisible(
    double playerX, double playerZ, double playerYaw,
    double targetX, double targetZ, double maximumDistance) noexcept
{
    if (!std::isfinite(playerX) || !std::isfinite(playerZ) || !std::isfinite(playerYaw)
        || !std::isfinite(targetX) || !std::isfinite(targetZ)
        || !std::isfinite(maximumDistance) || maximumDistance <= 0.0)
        return false;

    const double offsetX = targetX - playerX;
    const double offsetZ = targetZ - playerZ;
    const double distance = std::hypot(offsetX, offsetZ);
    if (distance >= maximumDistance) return false;
    if (distance < 0.10) return true;

    constexpr double minimumForwardAlignment = 0.15;
    const double forwardX = std::sin(playerYaw);
    const double forwardZ = -std::cos(playerYaw);
    return (offsetX * forwardX + offsetZ * forwardZ) / distance
        >= minimumForwardAlignment;
}

bool TrainInterior::canOccupy(double x, double z, bool toiletDoorOpen) noexcept
{
    if (x < -1.27 || x > 1.27 || z < PassengerMinimumZ || z > PassengerMaximumZ)
        return false;

    // Passenger seats leave a continuous 0.84 m aisle through all six cars.
    for (int car = 0; car < PassengerCars; ++car)
    {
        for (int row = 0; row < SeatRowsPerCar; ++row)
        {
            const double seatZ = seatRowZ(car, row);
            const int side = x < 0.0 ? -1 : 1;
            if (!seatSidePresent(car, row, side)) continue;
            const double innerBoundary = innerSeatPresent(car, row) ? 0.43 : 0.73;
            if (std::abs(z - seatZ) < 0.48 && std::abs(x) > innerBoundary) return false;
        }

        // Three facing-seat bays use real tables. Keep the 0.84 m central aisle continuous, but
        // prevent the player from clipping through the slab/leg volume between the seat rows.
        const int side = x < 0.0 ? -1 : 1;
        for (int table = 0; table < TableBaysPerCar; ++table)
            if (tableSidePresent(car,table,side)
                && std::abs(z-tableBayZ(car,table)) < 0.43 && std::abs(x) > 0.47)
                return false;
    }

    // Luggage towers occupy the left side of each vestibule without blocking the gangway.
    constexpr std::array<double, 6> luggagePositions = {-61.0, -40.0, -19.0, 23.0, 44.0, 61.0};
    for (double luggageZ : luggagePositions)
        if (std::abs(z - luggageZ) < 0.84 && x < -0.81) return false;

    // The accessible toilet has a single aisle doorway around z=4.07. The inner return wall
    // prevents entering through the rest of the compartment side.
    if (z > 3.40 && z < 8.15 && x > 0.29)
    {
        if (!toiletDoorOpen && z < 4.62) return false;
        if (z >= 4.59 && x < 0.49) return false;
        if (x > 1.18 || z > 7.72 || (z < 4.55 && x > 0.72)) return false;
    }
    return true;
}

} // namespace TrainSim::Sim
