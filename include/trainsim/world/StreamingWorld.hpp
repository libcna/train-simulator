#pragma once

#include "trainsim/sim/Route.hpp"

#include <cstdint>
#include <vector>

namespace TrainSim::Sim {
struct ServiceState;
}

namespace TrainSim::World {

enum class FeatureKind : std::uint8_t {
    GrassPatch, FieldCrop, HayBale, Tree, Bush, House, Townhouse, Apartment, Tower, Warehouse, Farmstead,
    RoadOverpass, LevelCrossing, Signal, CatenaryPole, PowerPylon, Substation, RoadVehicle,
    OtherTrain, Count
};

// Warehouse variants reserve their low two bits for the industrial material family. Higher bits
// describe a bounded multi-building compound without growing the cache-dense WorldFeature record.
inline constexpr std::uint32_t kWarehouseCompoundLeader = 1U << 3U;
inline constexpr std::uint32_t kWarehouseThreeBuildingCompound = 1U << 4U;

// Rural houses whose variant has bit 2 set own a detached barn or garage beside them along the
// route (ENV-127). Variant bit 0 selects the side; the outbuilding's footprint is a parcel.
[[nodiscard]] constexpr bool houseHasOutbuilding(std::uint32_t variant,double urbanity) noexcept
{
    return urbanity<0.35 && (variant&4U)!=0U;
}
inline constexpr double kHouseOutbuildingOffset=13.0;
inline constexpr double kHouseOutbuildingHalfWidth=3.4;
inline constexpr double kHouseOutbuildingHalfLength=3.9;
[[nodiscard]] constexpr double houseOutbuildingDirection(std::uint32_t variant) noexcept
{
    return (variant&1U)!=0U ? 1.0 : -1.0;
}

struct WorldFeature
{
    double routeDistance = 0.0;
    double lateralMetres = 0.0;
    double scale = 1.0;
    std::uint32_t variant = 0;
    FeatureKind kind = FeatureKind::Tree;

    constexpr WorldFeature() noexcept = default;
    constexpr WorldFeature(FeatureKind featureKind, double distance, double lateral,
                           double featureScale, std::uint32_t featureVariant) noexcept
        : routeDistance(distance), lateralMetres(lateral), scale(featureScale),
          variant(featureVariant), kind(featureKind)
    {
    }
};

enum class SignalAspect : std::uint8_t { Red, Yellow, Green };

[[nodiscard]] int signalFacingDirection(const WorldFeature& feature) noexcept;
[[nodiscard]] SignalAspect signalAspect(const WorldFeature& feature, const Sim::Route& route,
                                        const Sim::ServiceState& service) noexcept;
[[nodiscard]] bool terrainFeatureAllowed(const Sim::RoutePose& pose,
                                         double lateralMetres) noexcept;
[[nodiscard]] double movingOtherTrainDistance(const WorldFeature& feature, double seconds,
                                              double routeLengthMetres) noexcept;
[[nodiscard]] bool movingOtherTrainVisible(const WorldFeature& feature, double seconds,
                                           double routeLengthMetres) noexcept;
[[nodiscard]] double levelCrossingClosureAmount(double crossingDistanceMetres,
                                                double trainDistanceMetres) noexcept;

struct Chunk
{
    std::int64_t index = 0;
    double beginDistance = 0.0;
    double endDistance = 0.0;
    std::vector<WorldFeature> features;
};

class StreamingWorld
{
public:
    explicit StreamingWorld(const Sim::Route& route, double chunkLengthMetres = 250.0, int radius = 3);

    void update(double trainDistanceMetres);
    [[nodiscard]] const std::vector<Chunk>& chunks() const noexcept { return chunks_; }
    [[nodiscard]] std::int64_t currentChunk() const noexcept { return currentChunk_; }
    [[nodiscard]] int radius() const noexcept { return radius_; }
    [[nodiscard]] double chunkLength() const noexcept { return chunkLength_; }
    [[nodiscard]] std::size_t generatedChunkCount() const noexcept { return generatedChunkCount_; }
    [[nodiscard]] double requiredLevelCrossingClosure(
        double crossingDistanceMetres, double trainDistanceMetres,
        double seconds) const noexcept;

private:
    [[nodiscard]] Chunk generate(std::int64_t index, bool resolveVegetation = true) const;
    [[nodiscard]] std::uint64_t hash(std::uint64_t value) const noexcept;

    const Sim::Route& route_;
    double chunkLength_ = 250.0;
    int radius_ = 3;
    std::int64_t currentChunk_ = -1;
    std::vector<Chunk> chunks_;
    std::vector<Chunk> nextChunksScratch_;
    std::size_t generatedChunkCount_ = 0;
};

} // namespace TrainSim::World
