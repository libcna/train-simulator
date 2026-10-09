#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace TrainSim::Sim {

struct Station
{
    std::string name;
    double distanceMetres = 0.0;
    double dwellSeconds = 45.0;
    bool platformOnLeft = true;
    bool major = false;
    double scheduledTravelSecondsFromPrevious = 0.0;
    double speedLimitMetresPerSecondFromPrevious = 0.0;
};

struct RoutePose
{
    double x = 0.0;
    double elevation = 0.0;
    double distance = 0.0;
    double headingRadians = 0.0;
    double curvature = 0.0;
    bool tunnel = false;
    bool bridge = false;
    bool waterCrossing = false;
    // Absolute elevation keeps a crossed river level while the railway climbs or descends.
    double waterCrossingSurfaceElevation = 0.0;
    // Zero means no adjacent water body; otherwise the sign identifies its train-local side.
    // The absolute surface elevation lets rendering carve a level lake despite railway grades.
    int adjacentWaterSide = 0;
    double adjacentWaterSurfaceElevation = 0.0;
    double urbanity = 0.0;
};

struct RelativeRoutePose
{
    double x = 0.0;
    double elevation = 0.0;
    double z = 0.0;
    double headingRadians = 0.0;
};

class Route
{
public:
    static Route NorthernValleyExpress();

    Route(std::string serviceName, std::vector<Station> stations, std::uint64_t seed);

    [[nodiscard]] const std::string& serviceName() const noexcept { return serviceName_; }
    [[nodiscard]] const std::vector<Station>& stations() const noexcept { return stations_; }
    [[nodiscard]] double lengthMetres() const noexcept;
    [[nodiscard]] double scheduledOneWaySeconds() const noexcept;
    [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
    [[nodiscard]] RoutePose sample(double distanceMetres) const noexcept;
    [[nodiscard]] double parallelTrackLateralMetres(double distanceMetres) const noexcept;
    [[nodiscard]] RelativeRoutePose relativePose(double referenceDistanceMetres,
                                                 double sampleDistanceMetres) const noexcept;
    [[nodiscard]] std::size_t nearestStation(double distanceMetres) const noexcept;

private:
    std::string serviceName_;
    std::vector<Station> stations_;
    std::uint64_t seed_ = 0;
};

} // namespace TrainSim::Sim
