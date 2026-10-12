#include "SceneShared.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::GamerServices;

namespace TrainSim::Render {

// Staged uploads into the hidden GPU window submit at most this much per slice and stop once
// a frame's budget is spent; synchronous loads (start, teleport, late) are unbounded.
constexpr std::size_t kUploadSliceBytes = std::size_t{3} << 20;
constexpr std::size_t kUnboundedUpload = std::numeric_limits<std::size_t>::max();
constexpr double kStagedUploadBudgetMs = 4.0;

void SceneRenderer::updateStaticWorld(const Sim::Route& route,
                                     const World::StreamingWorld& world,
                                     const Sim::ServiceState& service)
{
    const auto begin = std::chrono::steady_clock::now();
    if (!worldStreaming_) worldStreaming_ = std::make_unique<WorldStreamingState>();
    auto& stream = *worldStreaming_;
    const auto current = world.currentChunk();
    const auto next = nextWorldChunk(current, service.direction,
                                    route.lengthMetres(), world.chunkLength());
    const auto collect = [&] {
        if (stream.worker.valid()) stream.geometry = stream.worker.get();
    };
    const auto recordGeneration = [&] {
        if (stream.generationRecorded) return;
        worldGenerationTotalMs_ += stream.geometry->generationMs;
        worldGenerationMaxMs_ = std::max(worldGenerationMaxMs_,stream.geometry->generationMs);
        routePointCacheHits_ += stream.geometry->cacheHits;
        routePointCacheMisses_ += stream.geometry->cacheMisses;
        stream.generationRecorded = true;
    };
    const auto uploadOne = [&](std::size_t sliceBytes,std::size_t& submitted) {
        const auto uploadStart = std::chrono::steady_clock::now();
        // One bounded slice of one profile-safe segment per call: large materials span several
        // calls (frames) without a single long submission. Synchronous loads take a segment whole.
        auto& target = stream.staged[stream.uploaded];
        const auto& data = stream.geometry->meshes[stream.uploaded].data();
        const auto& segments = stream.geometry->segments[stream.uploaded];
        if (data.empty()) target.reset();
        else
        {
            if (!target) target = std::make_unique<GpuMesh>();
            int reused = 0;
            if (target->stageSegment(device_,data,segments,stream.uploadedSegment,sliceBytes,reused,
                                     submitted))
                ++stream.uploadedSegment;
            worldBufferReuses_ += static_cast<std::uint64_t>(reused);
        }
        if (data.empty() || stream.uploadedSegment >= segments.size())
        {
            ++stream.uploaded;
            stream.uploadedSegment = 0;
        }
        const double ms = std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-uploadStart).count();
        worldUploadTotalMs_ += ms;
        worldUploadMaxMs_ = std::max(worldUploadMaxMs_,ms);
    };
    const auto resetJob = [&](std::int64_t chunk) {
        stream.target = chunk;
        stream.anchor = std::clamp((static_cast<double>(chunk)+0.5)*world.chunkLength(),
                                   0.0,route.lengthMetres());
        stream.quality = graphicsQuality_;
        stream.vegetation = vegetationDistanceMetres_;
        stream.uploaded = 0;
        stream.uploadedSegment = 0;
        stream.generationRecorded = false;
        for (auto& staged : stream.staged) if (staged) staged->resetStaging();
    };
    // Quality/range changes, teleports and reversals must never publish obsolete output.
    if (stream.target != -1 && (stream.quality != graphicsQuality_
        || stream.vegetation != vegetationDistanceMetres_
        || (stream.target != current && stream.target != next)))
    {
        collect();
        stream.target = -1;
    }
    if (!staticWorld_ || worldAnchorChunk_ != current)
    {
        if (stream.target == current)
        {
            if (stream.worker.valid()
                && stream.worker.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                ++stream.latePrefetches;
            else if (stream.uploaded == kWorldMaterialCount) ++stream.prefetchHits;
            else ++stream.latePrefetches;
            collect();
        }
        else
        {
            collect();
            resetJob(current);
            stream.geometry->build(route,world,stream.anchor,graphicsQuality_,vegetationDistanceMetres_,
                                   worldSun_);
            ++stream.synchronousBuilds;
        }
        recordGeneration();
        // Initial load / a discontinuous jump still needs a complete scene immediately.
        // A normal boundary only swaps the already prepared buffers and counters.
        std::size_t submitted = 0;
        while (stream.uploaded < kWorldMaterialCount) uploadOne(kUnboundedUpload,submitted);
        std::array<std::unique_ptr<GpuMesh>*,kWorldMaterialCount> active = {
            &terrain_, &foliageMaterial_, &needleMaterial_, &leafSprays_, &needleSprays_,
            &roofMaterial_, &waterMaterial_, &facadeMaterial_, &renderedFacadeMaterial_,
            &staticWorld_, &stationLettering_, &staticWorldShadows_, &platformMaterial_, &worldLuminaires_,
            &worldNightGlow_};
        for (std::size_t i=0; i<active.size(); ++i) active[i]->swap(stream.staged[i]);
        worldCounters_ = stream.geometry->counters;
        worldAnchorChunk_ = current;
        worldAnchorDistance_ = stream.anchor;
        stream.target = -1;
        ++worldRebuilds_;
    }
    else if (stream.target != -1)
    {
        if (stream.worker.valid()
            && stream.worker.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            collect();
        if (!stream.worker.valid())
        {
            recordGeneration();
            // Empty materials need no upload and must not consume a whole frame. Low
            // quality has many empty slots; wasting fourteen frames could miss a boundary
            // even when the few actual uploads take only milliseconds.
            std::size_t submitted = 0;
            const auto skipEmpty = [&] {
                while (stream.uploaded < kWorldMaterialCount
                    && stream.geometry->meshes[stream.uploaded].data().empty())
                    uploadOne(kUploadSliceBytes,submitted);
            };
            skipEmpty();
            // Stage slices into the hidden GPU window until a small per-frame budget is spent;
            // the current scene is untouched. Small materials then share a frame. When fast
            // travel brings the boundary close, keep going until this frame's share is sent.
            std::size_t remainingBytes = 0;
            for (std::size_t i = stream.uploaded; i < kWorldMaterialCount; ++i)
            {
                const auto& data = stream.geometry->meshes[i].data();
                remainingBytes += data.vertices.size()*sizeof(LitColorVertex)
                                + data.indices.size()*sizeof(std::uint32_t);
            }
            const double boundary = (static_cast<double>(current)+(service.direction < 0 ? 0.0 : 1.0))
                                  * world.chunkLength();
            const std::size_t urgent = stagedUploadBytesPerFrame(remainingBytes,
                boundary-service.routeDistanceMetres,streamingMetresPerFrame_);
            const auto stagingStart = std::chrono::steady_clock::now();
            while (stream.uploaded < kWorldMaterialCount)
            {
                uploadOne(kUploadSliceBytes,submitted);
                skipEmpty();
                if (std::chrono::duration<double,std::milli>(
                        std::chrono::steady_clock::now()-stagingStart).count() >= kStagedUploadBudgetMs
                    && submitted >= urgent)
                    break;
            }
        }
    }
    if (stream.target == -1 && next != current)
    {
        resetJob(next);
        // Own the route and world snapshot in the job, so neither simulation updates nor
        // renderer destruction can race geometry generation. Destruction joins the future.
        stream.worker = std::async(std::launch::async,
            [geometry=std::move(stream.geometry), routeCopy=route,
             length=world.chunkLength(), radius=world.radius(), anchor=stream.anchor,
             quality=graphicsQuality_, vegetation=vegetationDistanceMetres_, sun=worldSun_]() mutable {
                World::StreamingWorld snapshot(routeCopy,length,radius);
                snapshot.update(anchor);
                geometry->build(routeCopy,snapshot,anchor,quality,vegetation,sun);
                return std::move(geometry);
            });
    }
    const double ms = std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-begin).count();
    worldRebuildTotalMs_ += ms;
    worldRebuildMaxMs_ = std::max(worldRebuildMaxMs_,ms);
}

void SceneRenderer::updateTrack(const Sim::Route& route, const Sim::ServiceState& service)
{
    const auto begin = std::chrono::steady_clock::now();
    if (!trackStreaming_) trackStreaming_ = std::make_unique<TrackStreamingState>();
    auto& stream = *trackStreaming_;
    const auto anchorFor = [&](double distance) {
        return std::clamp(std::round(distance/500.0)*500.0,0.0,route.lengthMetres());
    };
    const double requested = anchorFor(service.routeDistanceMetres);
    const double next = anchorFor(requested+(service.direction < 0 ? -500.0 : 500.0));
    const auto finished = [&] {
        return !stream.worker.valid()
            || stream.worker.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    };
    const auto collect = [&] {
        if (!stream.worker.valid()) return;
        stream.geometry = stream.worker.get();
        trackGenerationMaxMs_ = std::max(trackGenerationMaxMs_,stream.geometry->generationMs);
    };
    const auto uploadOne = [&](std::size_t sliceBytes,std::size_t& submitted) {
        const auto& data = stream.geometry->materials()[stream.uploaded]->data();
        const auto& segments = stream.geometry->segments[stream.uploaded];
        auto& staged = stream.staged[stream.uploaded];
        if (data.empty()) staged.reset();
        else
        {
            if (!staged) staged = std::make_unique<GpuMesh>();
            int reused = 0;
            if (staged->stageSegment(device_,data,segments,stream.uploadedSegment,sliceBytes,reused,
                                     submitted))
                ++stream.uploadedSegment;
        }
        if (data.empty() || stream.uploadedSegment >= segments.size())
        {
            ++stream.uploaded;
            stream.uploadedSegment = 0;
        }
    };
    const auto resetJob = [&](double anchor) {
        stream.target = anchor;
        stream.quality = graphicsQuality_;
        stream.uploaded = 0;
        stream.uploadedSegment = 0;
        for (auto& staged : stream.staged) if (staged) staged->resetStaging();
    };
    // Quality changes, teleports and reversals must never publish an obsolete batch.
    if (stream.target >= 0 && (stream.quality != graphicsQuality_
        || (stream.target != requested && stream.target != next)))
    {
        collect();
        stream.target = -1;
    }
    if (!trackDetail_ || requested != trackAnchorDistance_)
    {
        if (stream.target == requested)
        {
            if (finished() && !stream.worker.valid() && stream.uploaded == kTrackMaterialCount)
                ++stream.prefetchHits;
            else ++stream.latePrefetches;
            collect();
        }
        else
        {
            // Initial load, teleport or a quality change: build the batch immediately.
            collect();
            resetJob(requested);
            stream.geometry->build(route,requested,graphicsQuality_);
            trackGenerationMaxMs_ = std::max(trackGenerationMaxMs_,stream.geometry->generationMs);
            ++stream.synchronousBuilds;
        }
        std::size_t submitted = 0;
        while (stream.uploaded < kTrackMaterialCount) uploadOne(kUnboundedUpload,submitted);
        trackDetail_.swap(stream.staged[0]);
        trackBedMaterial_.swap(stream.staged[1]);
        railHeadMaterial_.swap(stream.staged[2]);
        tracksideKilometrePosts_ = stream.geometry->kilometrePosts;
        trackFastenerAssemblies_ = stream.geometry->fastenerAssemblies;
        trackTurnoutPointMotors_ = stream.geometry->turnoutPointMotors;
        trackTurnoutBladeAssemblies_ = stream.geometry->turnoutBladeAssemblies;
        trackAnchorDistance_ = requested;
        stream.target = -1;
        ++trackRebuilds_;
    }
    else if (stream.target >= 0)
    {
        if (stream.worker.valid() && finished()) collect();
        if (!stream.worker.valid())
        {
            // Same per-frame staging budget and urgency rule as the world window; the anchor
            // switches halfway between anchors.
            std::size_t remainingBytes = 0;
            for (std::size_t i = stream.uploaded; i < kTrackMaterialCount; ++i)
            {
                const auto& data = stream.geometry->materials()[i]->data();
                remainingBytes += data.vertices.size()*sizeof(LitColorVertex)
                                + data.indices.size()*sizeof(std::uint32_t);
            }
            const double boundary = requested+(service.direction < 0 ? -250.0 : 250.0);
            const std::size_t urgent = stagedUploadBytesPerFrame(remainingBytes,
                boundary-service.routeDistanceMetres,streamingMetresPerFrame_);
            std::size_t submitted = 0;
            const auto stagingStart = std::chrono::steady_clock::now();
            while (stream.uploaded < kTrackMaterialCount)
            {
                uploadOne(kUploadSliceBytes,submitted);
                if (std::chrono::duration<double,std::milli>(
                        std::chrono::steady_clock::now()-stagingStart).count() >= kStagedUploadBudgetMs
                    && submitted >= urgent)
                    break;
            }
        }
    }
    if (stream.target < 0 && next != requested)
    {
        resetJob(next);
        // The job owns its route copy and CPU batch; destruction joins the future.
        stream.worker = std::async(std::launch::async,
            [geometry=std::move(stream.geometry), routeCopy=route, anchor=next,
             quality=graphicsQuality_]() mutable {
                geometry->build(routeCopy,anchor,quality);
                return std::move(geometry);
            });
    }
    const double ms = std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-begin).count();
    trackRebuildTotalMs_ += ms;
    trackRebuildMaxMs_ = std::max(trackRebuildMaxMs_,ms);
}


} // namespace TrainSim::Render
