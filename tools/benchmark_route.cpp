#include "trainsim/sim/Route.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>

using namespace TrainSim::Sim;

// Reference path before geometry-only sampling. Keep this deliberately independent of the
// optimized implementation so station influence computation remains part of the baseline.
RelativeRoutePose metadataRelativePose(const Route& route, double reference, double distance)
{
    const auto extended = [&](double s) {
        auto pose = route.sample(s);
        const double extra = s-pose.distance;
        if (extra != 0)
        {
            const double probeDistance = pose.distance <= 0 ? std::min(1.0,route.lengthMetres())
                : std::max(0.0,route.lengthMetres()-1.0);
            const auto probe = route.sample(probeDistance);
            const double span = probe.distance-pose.distance;
            if (std::abs(span) > 1e-9)
                pose.elevation += (probe.elevation-pose.elevation)/span*extra;
            pose.x += std::tan(pose.headingRadians)*extra;
            pose.distance += extra;
        }
        return pose;
    };
    const auto a=extended(reference), b=extended(distance);
    const double dx=b.x-a.x, dz=b.distance-a.distance, angle=b.headingRadians-a.headingRadians;
    return {dx*std::cos(a.headingRadians)-dz*std::sin(a.headingRadians),
            b.elevation-a.elevation,
            dx*std::sin(a.headingRadians)+dz*std::cos(a.headingRadians),
            std::atan2(std::sin(angle),std::cos(angle))};
}

int main()
{
    const auto route = Route::NorthernValleyExpress();
    struct Input { double reference, target; };
    std::array<Input,1024> inputs{};
    for (std::size_t i=0;i<inputs.size();++i)
    {
        const double reference = (route.lengthMetres()+400)*static_cast<double>(i)
            /static_cast<double>(inputs.size()-1)-200;
        inputs[i]={reference,reference+static_cast<double>((i*137)%1501)-750};
        const auto old = metadataRelativePose(route,inputs[i].reference,inputs[i].target);
        const auto now = route.relativePose(inputs[i].reference,inputs[i].target);
        if (std::abs(old.x-now.x)>1e-9 || std::abs(old.z-now.z)>1e-9
            || std::abs(old.elevation-now.elevation)>1e-9
            || std::abs(old.headingRadians-now.headingRadians)>1e-9)
        {
            std::cerr << "Route equivalence failed at input " << i << '\n';
            return 1;
        }
    }
    struct Measurement { double ms, checksum; };
    const auto measure = [&](bool metadata) {
        double checksum = 0;
        const auto begin = std::chrono::steady_clock::now();
        for (int repeat=0;repeat<256;++repeat)
            for (const auto& input : inputs)
            {
                const auto pose = metadata
                    ? metadataRelativePose(route,input.reference,input.target)
                    : route.relativePose(input.reference,input.target);
                checksum += pose.x+pose.elevation+pose.z+pose.headingRadians;
            }
        return Measurement{std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-begin).count(),checksum};
    };
    std::array<double,6> baseline{}, optimized{};
    std::cout << std::fixed << std::setprecision(3);
    for (std::size_t round=0;round<baseline.size();++round)
    {
        const auto first = measure(round%2 == 0);
        const auto second = measure(round%2 != 0);
        const auto old = round%2 == 0 ? first : second;
        const auto now = round%2 == 0 ? second : first;
        if (std::abs(old.checksum-now.checksum)>1e-6) return 2;
        baseline[round]=old.ms; optimized[round]=now.ms;
        std::cout << "round=" << round << " metadata_ms=" << old.ms
                  << " geometry_ms=" << now.ms << " checksum=" << now.checksum << '\n';
    }
    std::sort(baseline.begin(),baseline.end());
    std::sort(optimized.begin(),optimized.end());
    const double oldMedian=(baseline[2]+baseline[3])*0.5;
    const double newMedian=(optimized[2]+optimized[3])*0.5;
    std::cout << "calls_per_batch=262144 metadata_median_ms=" << oldMedian
              << " geometry_median_ms=" << newMedian << " ratio=" << oldMedian/newMedian << '\n';
}
