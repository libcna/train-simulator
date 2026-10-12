#pragma once

#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/world/StreamingWorld.hpp"

#include <array>
#include <algorithm>
#include <cmath>

namespace TrainSim::Render {

// CPU-only output. No graphics device, GPU objects or live simulation state are accessible
// while building the next window on the worker. Counters publish with the matching meshes.
struct WorldGeometryCounters
{
    bool operator==(const WorldGeometryCounters&) const = default;
    int staticWorldLakePatches_ = 0;
    int staticWorldLakeBankTrees_ = 0;
    int staticWorldHayBaleStacks_ = 0;
    int staticWorldRiverWaterSections_ = 0;
    int staticWorldRiverBankClumps_ = 0;
    int staticWorldStationWaterfrontSections_ = 0;
    int staticWorldCatenaryDroppers_ = 0;
    int staticWorldCatenaryFoundations_ = 0;
    int staticWorldCatenaryMessengerSegments_ = 0;
    int staticWorldCatenaryRegistrationAssemblies_ = 0;
    int staticWorldLinesideEquipmentCabinets_ = 0;
    int staticWorldTunnelLuminaires_ = 0;
    int staticWorldTunnelSafetyNiches_ = 0;
    int staticWorldTunnelLiningRings_ = 0;
    int staticWorldTunnelMarkerPlates_ = 0;
    int staticWorldBridgeSections_ = 0;
    int staticWorldRiverBridgeSections_ = 0;
    int staticWorldViaductSections_ = 0;
    int staticWorldBridgeAbutments_ = 0;
    int staticWorldTerminalBufferStops_ = 0;
    int staticWorldTerminalShedColumns_ = 0;
    int staticWorldBridgeParapetPosts_ = 0;
    int staticWorldFencePosts_ = 0;
    int staticWorldCableTroughUnits_ = 0;
    int staticWorldVergeFlowerClumps_ = 0;
    int staticWorldPalisadePosts_ = 0;
    int staticWorldHouseOutbuildings_ = 0;
    int staticWorldBlockBalconies_ = 0;
    int staticWorldCanopyValanceBoards_ = 0;
    int staticWorldTramlineSegments_ = 0;
    std::size_t worldGpuBytes_ = 0;
    int worldGpuSegments_ = 0;
    std::size_t largestMaterialBytes_ = 0;
    std::size_t largestMaterialVertices_ = 0;
    int largestMaterial_ = -1;
    int staticWorldPlatformSurfaceJoints_ = 0;
    int staticWorldPlatformCanopyLights_ = 0;
    int staticWorldPlatformCanopyDrainageAssemblies_ = 0;
    int staticWorldPlatformPlanters_ = 0;
    int staticWorldPlatformPosterCases_ = 0;
    int staticWorldPlatformEquipmentCabinets_ = 0;
    int staticWorldPlatformBenchAssemblies_ = 0;
    int staticWorldPlatformWeatheringMarks_ = 0;
    int staticWorldStationFacadeWeatheringStreaks_ = 0;
    int staticWorldPlatformBoundarySections_ = 0;
    int staticWorldTracksideDrainageSections_ = 0;
    int staticWorldTracksideVergeTufts_ = 0;
    int staticWorldFieldBoundarySegments_ = 0;
    int staticWorldFieldBoundaryTrees_ = 0;
    int staticWorldDistantWoodlandTrees_ = 0;
    int staticWorldDistantRidgeMasses_ = 0;
    int staticWorldTreeBranchCues_ = 0;
    int staticWorldConiferBranchTiers_ = 0;
    int staticWorldUnderstoreyGroups_ = 0;
    int staticWorldDistantSettlementBuildings_ = 0;
    int staticWorldBuildingEndWallWindows_ = 0;
    int staticWorldTowerEndWallWindows_ = 0;
    int staticWorldDetailedWarehouses_ = 0;
    int staticWorldIndustrialCompounds_ = 0;
    int staticWorldIndustrialYardVehicles_ = 0;
    int staticWorldIndustrialTrailers_ = 0;
    int staticWorldIndustrialUtilityGroups_ = 0;
    int staticWorldIndustrialEdgePlantingGroups_ = 0;
    int staticWorldUrbanRoadEdgeMarkingSections_ = 0;
    int staticWorldUrbanRoadDrainGrates_ = 0;
    int staticWorldUrbanResidentialPlotSurfaces_ = 0;
    int staticWorldUrbanStreetPockets_ = 0;
    int staticWorldUrbanResidentialEntrancePaths_ = 0;
};

enum class WorldMaterial : std::size_t {
    Terrain, Foliage, Needles, LeafSprays, NeedleSprays, Roof, Water, Facade,
    RenderedFacade, Opaque, Lettering, Shadows, Platform, Luminaires, Count
};
inline constexpr std::size_t kWorldMaterialCount = static_cast<std::size_t>(WorldMaterial::Count);

// Keep endpoint chunks valid in both directions, including a fractional final chunk.
inline std::int64_t nextWorldChunk(std::int64_t current, int direction,
                                    double routeLength, double chunkLength)
{
    const auto last = static_cast<std::int64_t>(std::floor(routeLength/chunkLength));
    return std::clamp(current + (direction < 0 ? -1 : 1), std::int64_t{0}, last);
}

class WorldGeometry
{
public:
    void build(const Sim::Route& route, const World::StreamingWorld& world,
               double anchorDistance, int graphicsQuality, float vegetationDistance);
    std::array<MeshBuilder,kWorldMaterialCount> meshes;
    // GPU buffer spans per material, prepared with the geometry so the render thread can stage
    // one profile-safe segment per frame (PERF-002).
    std::array<std::vector<MeshSegment>,kWorldMaterialCount> segments;
    WorldGeometryCounters counters;
    double generationMs = 0;
    std::uint64_t cacheHits = 0, cacheMisses = 0;
private:
    int graphicsQuality_ = 2;
    float vegetationDistanceMetres_ = 750;
};

// Bytes that must reach a hidden GPU window per frame for it to be complete two frames before
// the boundary, given the remaining data, the distance left and the recent metres per frame.
// Zero when nothing remains; the whole remainder when the boundary is imminent.
[[nodiscard]] inline std::size_t stagedUploadBytesPerFrame(std::size_t remainingBytes,
    double remainingMetres,double metresPerFrame) noexcept
{
    if (remainingBytes==0) return 0;
    const double frames=std::max(1.0,std::abs(remainingMetres)/std::max(metresPerFrame,1e-3)-2.0);
    return static_cast<std::size_t>(std::ceil(static_cast<double>(remainingBytes)/frames));
}

// CPU-only 2 km permanent-way batch (sleepers, rails, fasteners, turnouts and kilometre posts)
// around a 500 m anchor. Like WorldGeometry it is prepared on a worker ahead of the next anchor,
// so the render thread only stages GPU segments and swaps at the boundary (PERF-002).
inline constexpr std::size_t kTrackMaterialCount = 3;
struct TrackGeometry
{
    void build(const Sim::Route& route, double anchorDistance, int graphicsQuality);
    [[nodiscard]] std::array<const MeshBuilder*,kTrackMaterialCount> materials() const noexcept
    {
        return {&track,&trackBedMaterial,&railHeadMaterial};
    }
    MeshBuilder track, trackBedMaterial, railHeadMaterial;
    std::array<std::vector<MeshSegment>,kTrackMaterialCount> segments;
    int kilometrePosts = 0, fastenerAssemblies = 0, turnoutPointMotors = 0, turnoutBladeAssemblies = 0;
    double anchor = -1;
    int quality = -1;
    double generationMs = 0;
};

} // namespace TrainSim::Render
