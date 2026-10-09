#pragma once

#include "trainsim/sim/Route.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace TrainSim::Render {

struct LocalRoutePoint
{
    float x,y,z,yaw;
};

// Short-lived, bounded cache for one geometry build. Lateral offsets share a route sample.
// Keys retain exact doubles: no coordinate quantization or global cross-route state.
class RoutePointCache
{
public:
    LocalRoutePoint point(const Sim::Route& route,double reference,double distance,double lateral=0)
    {
        auto bits=std::bit_cast<std::uint64_t>(distance);
        bits=(bits^(bits>>30))*0xbf58476d1ce4e5b9ULL;
        bits=(bits^(bits>>27))*0x94d049bb133111ebULL;
        auto& entry=entries_[(bits^(bits>>31))%entries_.size()];
        if (!entry.valid || entry.route != &route || entry.reference != reference || entry.distance != distance)
        {
            entry={&route,reference,distance,route.relativePose(reference,distance),0,0,true};
            entry.cosine=std::cos(entry.pose.headingRadians);
            entry.sine=std::sin(entry.pose.headingRadians);
            ++misses_;
        }
        else ++hits_;
        return {static_cast<float>(entry.pose.x+lateral*entry.cosine),
                static_cast<float>(entry.pose.elevation),
                static_cast<float>(entry.pose.z-lateral*entry.sine),
                static_cast<float>(entry.pose.headingRadians)};
    }
    [[nodiscard]] std::uint64_t hits() const noexcept { return hits_; }
    [[nodiscard]] std::uint64_t misses() const noexcept { return misses_; }
private:
    struct Entry
    {
        const Sim::Route* route=nullptr;
        double reference=0,distance=0;
        Sim::RelativeRoutePose pose;
        double cosine=0,sine=0;
        bool valid=false;
    };
    std::array<Entry,128> entries_{};
    std::uint64_t hits_=0,misses_=0;
};

} // namespace TrainSim::Render
