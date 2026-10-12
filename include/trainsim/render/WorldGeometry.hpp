#pragma once

#include "trainsim/render/Atmosphere.hpp"
#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/render/RoutePointCache.hpp"
#include "trainsim/world/StreamingWorld.hpp"

#include <array>
#include <thread>
#include <vector>
#include <algorithm>
#include <cmath>

namespace TrainSim::Render {

// CPU-only output. No graphics device, GPU objects or live simulation state are accessible
// while building the next window on the worker. Counters publish with the matching meshes.
struct WorldGeometryCounters
{
    bool operator==(const WorldGeometryCounters&) const = default;
    // Chunk parts merge by summing every count. The largest-material figures describe the
    // merged window and are recomputed after the merge, so they are not summed.
    WorldGeometryCounters& operator+=(const WorldGeometryCounters& other) noexcept
    {
        staticWorldLakePatches_ += other.staticWorldLakePatches_;
        staticWorldLakeBankTrees_ += other.staticWorldLakeBankTrees_;
        staticWorldHayBaleStacks_ += other.staticWorldHayBaleStacks_;
        staticWorldRiverWaterSections_ += other.staticWorldRiverWaterSections_;
        staticWorldRiverBankClumps_ += other.staticWorldRiverBankClumps_;
        staticWorldStationWaterfrontSections_ += other.staticWorldStationWaterfrontSections_;
        staticWorldCatenaryDroppers_ += other.staticWorldCatenaryDroppers_;
        staticWorldCatenaryFoundations_ += other.staticWorldCatenaryFoundations_;
        staticWorldCatenaryMessengerSegments_ += other.staticWorldCatenaryMessengerSegments_;
        staticWorldCatenaryRegistrationAssemblies_ += other.staticWorldCatenaryRegistrationAssemblies_;
        staticWorldLinesideEquipmentCabinets_ += other.staticWorldLinesideEquipmentCabinets_;
        staticWorldTunnelLuminaires_ += other.staticWorldTunnelLuminaires_;
        staticWorldTunnelSafetyNiches_ += other.staticWorldTunnelSafetyNiches_;
        staticWorldTunnelLiningRings_ += other.staticWorldTunnelLiningRings_;
        staticWorldTunnelMarkerPlates_ += other.staticWorldTunnelMarkerPlates_;
        staticWorldBridgeSections_ += other.staticWorldBridgeSections_;
        staticWorldRiverBridgeSections_ += other.staticWorldRiverBridgeSections_;
        staticWorldViaductSections_ += other.staticWorldViaductSections_;
        staticWorldBridgeAbutments_ += other.staticWorldBridgeAbutments_;
        staticWorldTerminalBufferStops_ += other.staticWorldTerminalBufferStops_;
        staticWorldTerminalShedColumns_ += other.staticWorldTerminalShedColumns_;
        staticWorldBridgeParapetPosts_ += other.staticWorldBridgeParapetPosts_;
        staticWorldFencePosts_ += other.staticWorldFencePosts_;
        staticWorldCableTroughUnits_ += other.staticWorldCableTroughUnits_;
        staticWorldVergeFlowerClumps_ += other.staticWorldVergeFlowerClumps_;
        staticWorldPalisadePosts_ += other.staticWorldPalisadePosts_;
        staticWorldHouseOutbuildings_ += other.staticWorldHouseOutbuildings_;
        staticWorldHouseRearWings_ += other.staticWorldHouseRearWings_;
        staticWorldBlockPenthouses_ += other.staticWorldBlockPenthouses_;
        staticWorldPlatformAmenities_ += other.staticWorldPlatformAmenities_;
        staticWorldBlockBalconies_ += other.staticWorldBlockBalconies_;
        staticWorldCanopyValanceBoards_ += other.staticWorldCanopyValanceBoards_;
        staticWorldTramlineSegments_ += other.staticWorldTramlineSegments_;
        worldGpuBytes_ += other.worldGpuBytes_;
        worldGpuSegments_ += other.worldGpuSegments_;
        staticWorldPlatformSurfaceJoints_ += other.staticWorldPlatformSurfaceJoints_;
        staticWorldPlatformCanopyLights_ += other.staticWorldPlatformCanopyLights_;
        staticWorldPlatformCanopyDrainageAssemblies_ += other.staticWorldPlatformCanopyDrainageAssemblies_;
        staticWorldPlatformPlanters_ += other.staticWorldPlatformPlanters_;
        staticWorldPlatformPosterCases_ += other.staticWorldPlatformPosterCases_;
        staticWorldPlatformEquipmentCabinets_ += other.staticWorldPlatformEquipmentCabinets_;
        staticWorldPlatformBenchAssemblies_ += other.staticWorldPlatformBenchAssemblies_;
        staticWorldPlatformWeatheringMarks_ += other.staticWorldPlatformWeatheringMarks_;
        staticWorldStationFacadeWeatheringStreaks_ += other.staticWorldStationFacadeWeatheringStreaks_;
        staticWorldPlatformBoundarySections_ += other.staticWorldPlatformBoundarySections_;
        staticWorldTracksideDrainageSections_ += other.staticWorldTracksideDrainageSections_;
        staticWorldTracksideVergeTufts_ += other.staticWorldTracksideVergeTufts_;
        staticWorldFieldBoundarySegments_ += other.staticWorldFieldBoundarySegments_;
        staticWorldFieldBoundaryTrees_ += other.staticWorldFieldBoundaryTrees_;
        staticWorldDistantWoodlandTrees_ += other.staticWorldDistantWoodlandTrees_;
        staticWorldDistantRidgeMasses_ += other.staticWorldDistantRidgeMasses_;
        staticWorldTreeBranchCues_ += other.staticWorldTreeBranchCues_;
        staticWorldConiferBranchTiers_ += other.staticWorldConiferBranchTiers_;
        staticWorldUnderstoreyGroups_ += other.staticWorldUnderstoreyGroups_;
        staticWorldDistantSettlementBuildings_ += other.staticWorldDistantSettlementBuildings_;
        staticWorldBuildingEndWallWindows_ += other.staticWorldBuildingEndWallWindows_;
        staticWorldTowerEndWallWindows_ += other.staticWorldTowerEndWallWindows_;
        staticWorldDetailedWarehouses_ += other.staticWorldDetailedWarehouses_;
        staticWorldIndustrialCompounds_ += other.staticWorldIndustrialCompounds_;
        staticWorldIndustrialYardVehicles_ += other.staticWorldIndustrialYardVehicles_;
        staticWorldIndustrialTrailers_ += other.staticWorldIndustrialTrailers_;
        staticWorldIndustrialUtilityGroups_ += other.staticWorldIndustrialUtilityGroups_;
        staticWorldIndustrialEdgePlantingGroups_ += other.staticWorldIndustrialEdgePlantingGroups_;
        staticWorldUrbanRoadEdgeMarkingSections_ += other.staticWorldUrbanRoadEdgeMarkingSections_;
        staticWorldUrbanRoadDrainGrates_ += other.staticWorldUrbanRoadDrainGrates_;
        staticWorldUrbanResidentialPlotSurfaces_ += other.staticWorldUrbanResidentialPlotSurfaces_;
        staticWorldUrbanStreetPockets_ += other.staticWorldUrbanStreetPockets_;
        staticWorldUrbanResidentialEntrancePaths_ += other.staticWorldUrbanResidentialEntrancePaths_;
        return *this;
    }
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
    int staticWorldHouseRearWings_ = 0;
    int staticWorldBlockPenthouses_ = 0;
    int staticWorldPlatformAmenities_ = 0;
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
    RenderedFacade, Opaque, Lettering, Shadows, Platform, Luminaires,
    // Lit windows, lamp heads and light pools, drawn additively only at night (ENV-138).
    NightGlow, Count
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
    // Chunk generation threads, including the calling thread. One reproduces the former
    // sequential build exactly; any count yields identical output.
    explicit WorldGeometry(std::size_t workerThreads = defaultWorkerThreads())
        : workerThreads_(std::max<std::size_t>(1,workerThreads)) {}
    [[nodiscard]] static std::size_t defaultWorkerThreads() noexcept
    {
        // Bounded: the host is shared and the render thread must keep a core.
        return std::clamp<std::size_t>(std::thread::hardware_concurrency()/2,1,4);
    }
    void build(const Sim::Route& route, const World::StreamingWorld& world,
               double anchorDistance, int graphicsQuality, float vegetationDistance,
               const SunPosition& sun = sunPosition(Sim::kDefaultServiceStartHours));
    std::array<MeshBuilder,kWorldMaterialCount> meshes;
    // GPU buffer spans per material, prepared with the geometry so the render thread can stage
    // one profile-safe segment per frame (PERF-002).
    std::array<std::vector<MeshSegment>,kWorldMaterialCount> segments;
    WorldGeometryCounters counters;
    double generationMs = 0;
    std::uint64_t cacheHits = 0, cacheMisses = 0;
private:
    struct ChunkPart
    {
        std::array<MeshBuilder,kWorldMaterialCount> meshes;
        WorldGeometryCounters counters;
        RoutePointCache cache;
    };
    std::size_t workerThreads_ = 1;
    std::vector<ChunkPart> chunkParts_;
    int graphicsQuality_ = 2;
    float vegetationDistanceMetres_ = 750;
    SunPosition sun_ = sunPosition(Sim::kDefaultServiceStartHours);
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
