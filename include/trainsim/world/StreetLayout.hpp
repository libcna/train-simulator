#pragma once

#include "trainsim/world/StreamingWorld.hpp"
#include <cmath>
#include <optional>
#include <span>
#include <algorithm>
#include <utility>
#include <vector>

namespace TrainSim::World {

struct StreetPocket { double distance, lateral; };

struct FrontageLink { double begin,end,inner,outer; };

struct ResidentialAccess { double distance,side,facade,apronEdge,width; };

inline std::optional<ResidentialAccess> residentialAccess(const WorldFeature& f)
{
    double offset=0,facade=0,depth=0,width=0;
    switch (f.kind)
    {
        case FeatureKind::House: facade=3.55; depth=2.6; width=1.30; break;
        case FeatureKind::Townhouse:
            facade=3.05; depth=2.35; width=1.42; offset=f.variant%2U==0 ? -1.55 : 1.55; break;
        case FeatureKind::Apartment:
            facade=5.05; depth=3.1; width=1.62; offset=f.variant%2U==0 ? -0.62 : 0.62; break;
        case FeatureKind::Tower: facade=7.3; depth=3.6; width=2.65; break;
        default: return std::nullopt;
    }
    const double face=std::abs(f.lateralMetres)-facade*f.scale;
    const double edge=face-depth*f.scale;
    if (f.scale<=0 || edge<=24.6 || edge>=80) return std::nullopt;
    return ResidentialAccess{f.routeDistance+offset*f.scale,f.lateralMetres<0 ? -1.0 : 1.0,
        face,edge,width*f.scale};
}

inline std::optional<FrontageLink> frontageLink(std::span<const WorldFeature> features,
                                               const WorldFeature& first)
{
    if (first.kind!=FeatureKind::Townhouse) return std::nullopt;
    const WorldFeature* next=nullptr;
    for (const auto& other : features)
        if (other.kind==FeatureKind::Townhouse && other.lateralMetres*first.lateralMetres>0
            && other.routeDistance>first.routeDistance
            && other.routeDistance-first.routeDistance<16.0
            && (!next || other.routeDistance<next->routeDistance)) next=&other;
    if (!next) return std::nullopt;
    const double begin=first.routeDistance+5.7*first.scale;
    const double end=next->routeDistance-5.7*next->scale;
    const double inner=std::max(std::abs(first.lateralMetres)-4.3*first.scale,
                               std::abs(next->lateralMetres)-4.3*next->scale);
    const double outer=std::min(std::abs(first.lateralMetres)+4.3*first.scale,
                               std::abs(next->lateralMetres)+4.3*next->scale);
    if (end-begin<0.15 || outer-inner<1.0) return std::nullopt;
    return FrontageLink{begin,end,inner,outer};
}

inline bool streetPocketEntranceConflict(const StreetPocket& pocket,const WorldFeature& f)
{
    if (pocket.lateral*f.lateralMetres<=0) return false;
    double offset=0,facade=0,apron=0,pathWidth=0;
    switch (f.kind)
    {
        case FeatureKind::House: facade=3.55; apron=2.6; pathWidth=1.3; break;
        case FeatureKind::Townhouse:
            facade=3.05; apron=2.35; pathWidth=1.42;
            offset=f.variant%2U==0U ? -1.55 : 1.55; break;
        case FeatureKind::Apartment:
            facade=5.05; apron=3.1; pathWidth=1.62;
            offset=f.variant%2U==0U ? -0.62 : 0.62; break;
        case FeatureKind::Tower: facade=7.3; apron=3.6; pathWidth=2.65; break;
        default: return false;
    }
    const double face=std::abs(f.lateralMetres)-facade*f.scale;
    const double apronEdge=face-apron*f.scale;
    // Same bounded road connection as the renderer; reserve its full width, not just a line.
    return apronEdge>24.6 && apronEdge<80 && face>=26
        && std::abs(f.routeDistance+offset*f.scale-pocket.distance)<6+0.5*pathWidth*f.scale+0.3;
}

// One small public court per street side, only in a genuine gap between residential parcels.
// All candidates are inside their host chunk; fixed feature reservations also cover the court's
// pavement connection. Selection is independent of feature insertion order.
inline std::optional<StreetPocket> streetPocket(std::span<const WorldFeature> features,int side)
{
    std::optional<StreetPocket> result;
    for (const auto& a : features)
    {
        if (a.kind!=FeatureKind::Townhouse || a.lateralMetres*side<=0) continue;
        const WorldFeature* next=nullptr;
        for (const auto& b : features)
            if (b.kind==FeatureKind::Townhouse && b.lateralMetres*side>0
                && b.routeDistance>a.routeDistance
                && (!next || b.routeDistance<next->routeDistance)) next=&b;
        if (!next) continue;
        const double begin=a.routeDistance+5.7*a.scale;
        const double end=next->routeDistance-5.7*next->scale;
        if (end-begin<18) continue;
        const StreetPocket candidate{(begin+end)*0.5,side*30.0};
        bool clear=true;
        for (const auto& f : features)
        {
            if (streetPocketEntranceConflict(candidate,f)) { clear=false; break; }
            double width=0,length=0;
            switch (f.kind)
            {
                case FeatureKind::Townhouse: width=4.3; length=5.7; break;
                case FeatureKind::House: width=5; length=std::max(6.2,houseLength(housePlan(f.variant,1.0))*0.5+1.2); break;
                case FeatureKind::Apartment: width=6.4; length=std::max(7.8,blockLength(blockPlan(f.variant))*0.5+1.3); break;
                case FeatureKind::Tower: width=9; length=11; break;
                case FeatureKind::Warehouse: width=15; length=30; break;
                case FeatureKind::Farmstead: width=14; length=16; break;
                case FeatureKind::Substation: width=18; length=30; break;
                case FeatureKind::Tree: width=4; length=4; break;
                case FeatureKind::Bush: width=2; length=2; break;
                case FeatureKind::PowerPylon: width=8; length=8; break;
                case FeatureKind::RoadOverpass: case FeatureKind::LevelCrossing:
                    width=100; length=15; break;
                default: continue;
            }
            if (std::abs(f.routeDistance-candidate.distance)<length*f.scale+7
                && std::abs(f.lateralMetres-candidate.lateral)<width*f.scale+6)
            { clear=false; break; }
        }
        if (clear && (!result || candidate.distance<result->distance)) result=candidate;
    }
    return result;
}

// Shared street-edge garden boundary, with actual entrances and public access kept open.
inline std::vector<std::pair<double,double>> frontageBoundarySpans(
    std::span<const WorldFeature> features,int side)
{
    double begin=1e30,end=-1e30;
    for (const auto& f : features)
        if (f.kind==FeatureKind::Townhouse && f.lateralMetres*side>0)
        {
            begin=std::min(begin,f.routeDistance-5.7*f.scale);
            end=std::max(end,f.routeDistance+5.7*f.scale);
        }
    if (begin>=end) return {};
    std::vector<std::pair<double,double>> spans={{begin,end}};
    const auto cut=[&](double centre,double halfWidth) {
        std::vector<std::pair<double,double>> next;
        for (auto [a,b] : spans)
        {
            if (centre+halfWidth<=a || centre-halfWidth>=b) { next.emplace_back(a,b); continue; }
            if (centre-halfWidth>a+0.1) next.emplace_back(a,centre-halfWidth);
            if (centre+halfWidth<b-0.1) next.emplace_back(centre+halfWidth,b);
        }
        spans=std::move(next);
    };
    for (const auto& f : features)
    {
        if (f.kind==FeatureKind::RoadOverpass || f.kind==FeatureKind::LevelCrossing)
        { cut(f.routeDistance,16); continue; }
        if (f.lateralMetres*side<=0) continue;
        double offset=0,width=0;
        switch (f.kind)
        {
            case FeatureKind::Townhouse: offset=f.variant%2U==0 ? -1.55 : 1.55; width=1.42; break;
            case FeatureKind::House: width=1.30; break;
            case FeatureKind::Apartment: offset=f.variant%2U==0 ? -0.62 : 0.62; width=1.62; break;
            case FeatureKind::Tower: width=2.65; break;
            case FeatureKind::Substation: width=8; break;
            case FeatureKind::Warehouse: width=6; break;
            default: continue;
        }
        cut(f.routeDistance+offset*f.scale,width*f.scale*0.5+0.45);
    }
    if (const auto court=streetPocket(features,side)) cut(court->distance,7.0);
    return spans;
}

} // namespace TrainSim::World
