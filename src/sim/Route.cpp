#include "trainsim/sim/Route.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace TrainSim::Sim {

namespace {
constexpr double kPi = 3.14159265358979323846;

double routeLateral(double s) noexcept
{
    return 410.0*std::sin(s/9'500.0)+170.0*std::sin(s/3'700.0+0.8)
         +38.0*std::sin(s/850.0);
}

double routeSlope(double s) noexcept
{
    return 410.0/9'500.0*std::cos(s/9'500.0)
         +170.0/3'700.0*std::cos(s/3'700.0+0.8)
         +38.0/850.0*std::cos(s/850.0);
}

double routeElevation(double distanceMetres) noexcept
{
    return 22.0 + 14.0 * std::sin(distanceMetres / 15'000.0)
                + 4.5 * std::sin(distanceMetres / 2'700.0 + 1.4);
}

const double kRiverWaterSurfaceElevation = routeElevation(74'500.0)-8.2;
}

Route Route::NorthernValleyExpress()
{
    return Route("Northern Valley Express", {
        {"Alderwick Central",       0.0, 90.0, true,  true,    0.0,  0.00},
        {"Merefield",           18'600.0, 45.0, false, false, 605.0, 33.33},
        {"Hawthorn Park",       39'800.0, 50.0, true,  false, 600.0, 38.89},
        {"Greenford Vale",      63'200.0, 55.0, false, false, 590.0, 44.44},
        {"Lakehurst",           88'700.0, 60.0, true,  true,  635.0, 44.44},
        {"Pine Crossing",      111'900.0, 40.0, true,  false, 655.0, 38.89},
        {"Stonebridge",        138'300.0, 55.0, false, false, 655.0, 44.44},
        {"Meadow Junction",    165'800.0, 65.0, true,  true,  685.0, 44.44},
        {"Northbridge",        192'400.0, 50.0, false, true,  740.0, 38.89},
        {"Redwater",           218'900.0, 45.0, true,  false, 660.0, 44.44},
        {"Eastmere",           247'600.0, 55.0, false, false, 795.0, 38.89},
        {"Westhaven Central",  286'400.0, 120.0, true, true,  935.0, 44.44},
    }, 0x4e6f72746856616cULL);
}

Route::Route(std::string serviceName, std::vector<Station> stations, std::uint64_t seed)
    : serviceName_(std::move(serviceName)), stations_(std::move(stations)), seed_(seed)
{
    if (serviceName_.empty() || stations_.size() < 2)
        throw std::invalid_argument("A route needs a service name and at least two stations");
    if (!std::is_sorted(stations_.begin(), stations_.end(),
                        [](const Station& a, const Station& b) { return a.distanceMetres < b.distanceMetres; }))
        throw std::invalid_argument("Route stations must be ordered by distance");
    if (stations_.front().distanceMetres != 0.0)
        throw std::invalid_argument("The first station must be at route distance zero");
    if (stations_.front().scheduledTravelSecondsFromPrevious != 0.0)
        throw std::invalid_argument("The first station cannot have inbound travel time");
    if (stations_.front().speedLimitMetresPerSecondFromPrevious != 0.0)
        throw std::invalid_argument("The first station cannot have an inbound speed limit");
    for (std::size_t index = 0; index < stations_.size(); ++index)
    {
        if (stations_[index].dwellSeconds <= 0.0)
            throw std::invalid_argument("Station dwell times must be positive");
        if (index > 0 && (stations_[index].distanceMetres <= stations_[index - 1].distanceMetres
                          || stations_[index].scheduledTravelSecondsFromPrevious <= 0.0
                          || stations_[index].speedLimitMetresPerSecondFromPrevious < 16.0
                          || stations_[index].speedLimitMetresPerSecondFromPrevious > 50.0))
            throw std::invalid_argument("Station distances and travel times must increase the timetable");
    }
}

double Route::lengthMetres() const noexcept
{
    return stations_.back().distanceMetres;
}

double Route::scheduledOneWaySeconds() const noexcept
{
    double seconds = 0.0;
    for (std::size_t index = 1; index < stations_.size(); ++index)
    {
        seconds += stations_[index].scheduledTravelSecondsFromPrevious;
        if (index + 1 < stations_.size()) seconds += stations_[index].dwellSeconds;
    }
    return seconds;
}

double Route::parallelTrackLateralMetres(double distanceMetres) const noexcept
{
    double offset = -5.0;
    for (const Station& station : stations_)
    {
        if (!station.platformOnLeft) continue;
        const double normalized = std::clamp(
            1.0 - std::abs(distanceMetres - station.distanceMetres) / 700.0, 0.0, 1.0);
        const double eased = normalized * normalized * (3.0 - 2.0 * normalized);
        offset -= 4.0 * eased;
    }
    return offset;
}

RoutePose Route::sample(double distanceMetres) const noexcept
{
    const double s = std::clamp(distanceMetres, 0.0, lengthMetres());

    // Route-relative double precision is kept in the simulation. Rendering subtracts the train's
    // current distance before converting to float, so the camera never sees 286 km coordinates.
    const double x = routeLateral(s);
    const double dx = routeSlope(s);
    const double ddx = -410.0 / (9'500.0 * 9'500.0) * std::sin(s / 9'500.0)
                     - 170.0 / (3'700.0 * 3'700.0) * std::sin(s / 3'700.0 + 0.8)
                     - 38.0 / (850.0 * 850.0) * std::sin(s / 850.0);
    const double elevation = routeElevation(s);

    double urbanity = 0.06;
    for (const Station& station : stations_)
    {
        const double d = (s - station.distanceMetres) / (station.major ? 4'200.0 : 2'000.0);
        urbanity = std::max(urbanity, std::exp(-d * d) * (station.major ? 1.0 : 0.62));
    }

    const bool longTunnel = s > 121'000.0 && s < 126'800.0;
    const bool shortTunnel = s > 229'500.0 && s < 231'100.0;
    const bool riverBridge = s > 73'900.0 && s < 75'100.0;
    const bool valleyViaduct = s > kValleyViaductBeginMetres && s < kValleyViaductEndMetres;
    // Lakehurst is approached beside a broad upland lake. It is deliberately offset from the
    // railway rather than crossed, making it a long window-view landmark distinct from Redwater.
    const bool lakehurstShore = s > 80'500.0 && s < 87'200.0;

    return RoutePose{
        .x = x,
        .elevation = elevation,
        .distance = s,
        .headingRadians = std::atan2(dx, 1.0),
        .curvature = ddx / std::pow(1.0 + dx * dx, 1.5),
        .tunnel = longTunnel || shortTunnel,
        .bridge = riverBridge || valleyViaduct,
        .waterCrossing = riverBridge,
        .waterCrossingSurfaceElevation = riverBridge
            ? kRiverWaterSurfaceElevation : 0.0,
        .adjacentWaterSide = lakehurstShore ? 1 : 0,
        .adjacentWaterSurfaceElevation = lakehurstShore ? 8.0 : 0.0,
        .urbanity = std::clamp(urbanity, 0.0, 1.0),
    };
}

RelativeRoutePose Route::relativePose(double referenceDistanceMetres,
                                      double sampleDistanceMetres) const noexcept
{
    struct ExtendedPose
    {
        double x;
        double elevation;
        double distance;
        double headingRadians;
    };
    const auto extendEndpointTangent = [this](double distance) {
        // Relative geometry needs no station urbanity, curvature, water or tunnel metadata.
        // Avoid recomputing all station influence exponentials for every mesh vertex/fixture.
        const double s = std::clamp(distance,0.0,lengthMetres());
        const ExtendedPose base{routeLateral(s),routeElevation(s),s,
                                std::atan2(routeSlope(s),1.0)};
        const double extra = distance - base.distance;
        double elevationSlope = 0.0;
        if (extra != 0.0)
        {
            const double probeDistance = base.distance <= 0.0
                ? std::min(1.0,lengthMetres())
                : std::max(0.0,lengthMetres()-1.0);
            const double span = probeDistance-base.distance;
            if (std::abs(span) > 1e-9)
                elevationSlope = (routeElevation(probeDistance)-base.elevation) / span;
        }
        return ExtendedPose{
            base.x + std::tan(base.headingRadians) * extra,
            base.elevation + elevationSlope * extra,
            base.distance + extra,
            base.headingRadians};
    };

    // Simulation sampling remains clamped to the 286.4 km service. Rendering, however, authors
    // platforms, canopies and the consist around the endpoint datum. Tangent extrapolation keeps
    // those small terminal offsets spatially distinct instead of collapsing half their supports
    // onto the route endpoint.
    const ExtendedPose reference = extendEndpointTangent(referenceDistanceMetres);
    const ExtendedPose target = extendEndpointTangent(sampleDistanceMetres);
    const double globalX = target.x - reference.x;
    const double globalZ = target.distance - reference.distance;
    const double cosine = std::cos(reference.headingRadians);
    const double sine = std::sin(reference.headingRadians);
    const double headingDelta = target.headingRadians - reference.headingRadians;
    return RelativeRoutePose{
        .x = globalX * cosine - globalZ * sine,
        .elevation = target.elevation - reference.elevation,
        .z = globalX * sine + globalZ * cosine,
        .headingRadians = std::atan2(std::sin(headingDelta), std::cos(headingDelta)),
    };
}

std::size_t Route::nearestStation(double distanceMetres) const noexcept
{
    const auto it = std::min_element(stations_.begin(), stations_.end(),
        [distanceMetres](const Station& a, const Station& b) {
            return std::abs(a.distanceMetres - distanceMetres)
                 < std::abs(b.distanceMetres - distanceMetres);
        });
    return static_cast<std::size_t>(std::distance(stations_.begin(), it));
}

} // namespace TrainSim::Sim
