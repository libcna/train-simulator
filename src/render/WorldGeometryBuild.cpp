#include "SceneShared.hpp"

#include <atomic>
#include <future>
#include <vector>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::GamerServices;

namespace TrainSim::Render {

namespace {

// Planted or paved plot length along the route for a house or block plan (ENV-134); urban
// houses never carry a rear wing, so the plot width is unchanged.
float residentialPlotLength(const World::WorldFeature& feature)
{
    if (feature.kind==World::FeatureKind::House)
        return std::max(12.4f,static_cast<float>(World::houseLength(
            World::housePlan(feature.variant,1.0)))+2.4f);
    return std::max(15.6f,apartmentLength(feature.variant)+2.6f);
}

// One world-geometry build context: the target material builders, counters and route cache
// of either one 250 m chunk (built in parallel) or the station pass. Every name the former
// single-function build used is available unchanged, so chunk output is identical.
struct WorldBuildContext
{
    const Sim::Route& route;
    const World::StreamingWorld& world;
    double anchorDistance;
    int graphicsQuality_;
    float vegetationDistanceMetres_;
    double anchorElevation;
    std::array<MeshBuilder,kWorldMaterialCount>& meshes;
    WorldGeometryCounters& counters;
    RoutePointCache& routePointCache;
    // Shadow-casting light baked into this window (ENV-138); the reference sun by default.
    SunPosition sun;
    [[nodiscard]] Vector3 sunRays(double heading) const { return sunRayDirection(heading,sun); }
    // Night glow (ENV-138): each building reseeds from its absolute identity and numbers its
    // windows in generation order, so a room stays lit or dark across every streamed window.
    std::uint32_t lightSeed = 0;
    std::uint32_t windowOrdinal = 0;
    [[nodiscard]] MeshBuilder& nightGlow()
    {
        return meshes[static_cast<std::size_t>(WorldMaterial::NightGlow)];
    }
    void addWindowGlow(const LocalRoutePoint& point,float centreHeight,float across,
                       float height,float along)
    {
        const std::uint32_t seed = lightSeed*2654435761U + (windowOrdinal++)*40503U + 1U;
        if (!nightWindowLit(seed)) return;
        addOrientedBox(nightGlow(),point.x,point.y+centreHeight+0.015f,point.z,
                       across,height*0.90f,along*0.90f,point.yaw,nightWindowColour(seed));
    }

    LocalRoutePoint localRoutePoint(const Sim::Route& sampledRoute,double reference,double distance,
                                    double lateral=0.0)
    {
        return routePointCache.point(sampledRoute,reference,distance,lateral);
    }
        static void addFacadeMaterial(
            MeshBuilder& target, const LocalRoutePoint& point, bool positiveLateral,
            float width, float lowerHeight, float upperHeight, float length, const Color& color)
        {
            const float localX = positiveLateral ? -width*0.5f : width*0.5f;
            const float halfLength = length*0.5f;
            const Matrix transform = Matrix::CreateRotationY(point.yaw)
                                   * Matrix::CreateTranslation(point.x,point.y,point.z);
            addFacadeEndMaterials(target,transform,width,length,lowerHeight,upperHeight,color);
            const Vector3 lowerBegin = Vector3::Transform(
                Vector3{localX,lowerHeight,-halfLength},transform);
            const Vector3 lowerEnd = Vector3::Transform(
                Vector3{localX,lowerHeight,halfLength},transform);
            const Vector3 upperEnd = Vector3::Transform(
                Vector3{localX,upperHeight,halfLength},transform);
            const Vector3 upperBegin = Vector3::Transform(
                Vector3{localX,upperHeight,-halfLength},transform);
            constexpr float tileMetres = 2.0f;
            const Vector2 uvLowerBegin{-halfLength/tileMetres,lowerHeight/tileMetres};
            const Vector2 uvLowerEnd{halfLength/tileMetres,lowerHeight/tileMetres};
            const Vector2 uvUpperEnd{halfLength/tileMetres,upperHeight/tileMetres};
            const Vector2 uvUpperBegin{-halfLength/tileMetres,upperHeight/tileMetres};
            if (positiveLateral)
                target.addTexturedQuad(
                    lowerBegin,lowerEnd,upperEnd,upperBegin,color,
                    uvLowerBegin,uvLowerEnd,uvUpperEnd,uvUpperBegin);
            else
                target.addTexturedQuad(
                    lowerEnd,lowerBegin,upperBegin,upperEnd,color,
                    uvLowerEnd,uvLowerBegin,uvUpperBegin,uvUpperEnd);
            // The material is the wall shell, not a nearly coplanar overlay on a solid box.
            // Close the rear elevation as well; roofs and foundations close its vertical extent.
            const auto rear=[&](float y,float z) {
                return Vector3::Transform(Vector3{-localX,y,z},transform);
            };
            if (positiveLateral)
                target.addTexturedQuad(rear(lowerHeight,halfLength),rear(lowerHeight,-halfLength),
                    rear(upperHeight,-halfLength),rear(upperHeight,halfLength),color,
                    uvLowerEnd,uvLowerBegin,uvUpperBegin,uvUpperEnd);
            else
                target.addTexturedQuad(rear(lowerHeight,-halfLength),rear(lowerHeight,halfLength),
                    rear(upperHeight,halfLength),rear(upperHeight,-halfLength),color,
                    uvLowerBegin,uvLowerEnd,uvUpperEnd,uvUpperBegin);
        }
        void addFacadeWindow(const LocalRoutePoint& point,
                                                  float centreHeight, float height, float length,
                                                  std::optional<unsigned> room=std::nullopt,
                                                  std::optional<Color> frameTone=std::nullopt,
                                                  bool stationRoom=false)
        {
            MeshBuilder& mesh=meshes[9];
            const Color frame=frameTone.value_or(Color(213,211,196,255));
            const Color glazing(48,83,99,255);
            const float detailDistance = graphicsQuality_ == 0 ? 300.0f
                                       : graphicsQuality_ == 1 ? 475.0f : 650.0f;
            if (std::abs(point.z) > detailDistance)
            {
                // Beyond the passenger-readable range one inset pane preserves the facade rhythm.
                // Building every sill and mullion across all seven loaded chunks only creates
                // sub-pixel geometry and materially increases the static batch.
                addOrientedBox(mesh,point.x,point.y+centreHeight,point.z,
                               0.12f,height,length,point.yaw,glazing);
                addWindowGlow(point,centreHeight-0.015f,0.15f,height,length);
                return;
            }
            if (graphicsQuality_ >= 1)
                addRaisedWindowFrame(mesh,length,height,
                    Matrix::CreateRotationY(point.yaw+1.570796327f)
                        *Matrix::CreateTranslation(point.x,point.y+centreHeight,point.z),frame);
            else
                addOrientedBox(mesh,point.x,point.y+centreHeight,point.z,
                               0.105f,height+0.18f,length+0.20f,point.yaw,frame);
            if (room && stationRoom && graphicsQuality_>=1)
                addStationGlazing(mesh,length,height,
                    Matrix::CreateRotationY(point.yaw+1.570796327f)
                        *Matrix::CreateTranslation(point.x,point.y+centreHeight+0.015f,point.z),*room);
            else if (room && graphicsQuality_>=1)
                addResidentialGlazing(mesh,length,height,
                    Matrix::CreateRotationY(point.yaw+1.570796327f)
                        *Matrix::CreateTranslation(point.x,point.y+centreHeight+0.015f,point.z),*room);
            else
                addOrientedBox(mesh,point.x,point.y+centreHeight+0.015f,point.z,
                               0.135f,height,length,point.yaw,glazing);
            addWindowGlow(point,centreHeight,0.16f,height,length);
            addOrientedBox(mesh,point.x,point.y+centreHeight-height*0.5f-0.055f,point.z,
                           0.18f,0.075f,length+0.30f,point.yaw,Color(155,153,144,255));
            if (graphicsQuality_ >= 1)
            {
                addOrientedBox(mesh,point.x,point.y+centreHeight+0.025f,point.z,
                               0.145f,height-0.07f,0.045f,point.yaw,frame);
                addOrientedBox(mesh,point.x,point.y+centreHeight+0.04f,point.z,
                               0.145f,0.045f,length-0.07f,point.yaw,frame);
            }
        }
        void addEndWallWindow(const LocalRoutePoint& point,
                                                   float centreHeight, float width, float height,
                                                   std::optional<unsigned> room=std::nullopt)
        {
            MeshBuilder& mesh=meshes[9];
            const Color frame(205,204,193,255);
            const Color glazing(47,80,94,255);
            const float detailDistance = graphicsQuality_ == 0 ? 260.0f
                                       : graphicsQuality_ == 1 ? 430.0f : 600.0f;
            if (std::abs(point.z) > detailDistance)
            {
                addOrientedBox(mesh,point.x,point.y+centreHeight,point.z,
                               width,height,0.11f,point.yaw,glazing);
                addWindowGlow(point,centreHeight-0.015f,width,height,0.14f);
            }
            else
            {
                addWindowGlow(point,centreHeight,width,height,0.16f);
                if (graphicsQuality_ >= 1)
                    addRaisedWindowFrame(mesh,width,height,
                        Matrix::CreateRotationY(point.yaw)
                            *Matrix::CreateTranslation(point.x,point.y+centreHeight,point.z),frame);
                else
                    addOrientedBox(mesh,point.x,point.y+centreHeight,point.z,
                                   width+0.18f,height+0.18f,0.105f,point.yaw,frame);
                if (room && graphicsQuality_>=1)
                    addResidentialGlazing(mesh,width,height,
                        Matrix::CreateRotationY(point.yaw)
                            *Matrix::CreateTranslation(point.x,point.y+centreHeight+0.015f,point.z),*room);
                else
                    addOrientedBox(mesh,point.x,point.y+centreHeight+0.015f,point.z,
                                   width,height,0.135f,point.yaw,glazing);
                addOrientedBox(mesh,point.x,point.y+centreHeight-height*0.5f-0.055f,point.z,
                               width+0.28f,0.075f,0.18f,point.yaw,Color(150,149,141,255));
                if (graphicsQuality_ >= 1)
                {
                    addOrientedBox(mesh,point.x,point.y+centreHeight+0.025f,point.z,
                                   0.045f,height-0.07f,0.145f,point.yaw,frame);
                    addOrientedBox(mesh,point.x,point.y+centreHeight+0.04f,point.z,
                                   width-0.07f,0.045f,0.145f,point.yaw,frame);
                }
            }
            ++counters.staticWorldBuildingEndWallWindows_;
        }
    void buildChunk(const World::Chunk& chunk);
    void buildStations();
};

void WorldBuildContext::buildChunk(const World::Chunk& chunk)
{
    [[maybe_unused]] auto& terrain = meshes[0];
    [[maybe_unused]] auto& foliageMaterial = meshes[1];
    [[maybe_unused]] auto& needleMaterial = meshes[2];
    [[maybe_unused]] auto& leafSprays = meshes[3];
    [[maybe_unused]] auto& needleSprays = meshes[4];
    [[maybe_unused]] auto& roofMaterial = meshes[5];
    [[maybe_unused]] auto& waterMaterial = meshes[6];
    [[maybe_unused]] auto& facadeMaterial = meshes[7];
    [[maybe_unused]] auto& renderedFacadeMaterial = meshes[8];
    [[maybe_unused]] auto& mesh = meshes[9];
    [[maybe_unused]] auto& stationLettering = meshes[10];
    [[maybe_unused]] auto& groundShadows = meshes[11];
    [[maybe_unused]] auto& platformMaterial = meshes[12];
    [[maybe_unused]] auto& worldLuminaires = meshes[13];
    [[maybe_unused]] MeshBuilder& roofSurface=graphicsQuality_>=1 ? roofMaterial : mesh;
    const double middle = (chunk.beginDistance + chunk.endDistance) * 0.5;
    const Sim::RoutePose pose = route.sample(middle);
    const Color ground = pose.urbanity > 0.55 ? Color(92,99,96,255) : Color(91,133,72,255);

    // A curved, deterministic terrain ribbon extends beyond the fog start. The railway
    // corridor stays level while progressively stronger relief builds toward the horizon.
    constexpr std::array<double, 22> ordinaryLateralBands = {
        -1'200.0, -600.0, -260.0, -200.0, -150.0, -100.0, -40.0, -26.0, -12.0,
        -10.4, -8.8, 8.8, 10.4, 12.0,
        26.0, 40.0, 100.0, 150.0, 200.0, 260.0, 600.0, 1'200.0};
    constexpr std::array<double,38> lakeLateralBands={
        -1200,-600,-560,-520,-500,-480,-455,-260,-200,-150,-110,-100,-92,-80,
        -40,-26,-12,-10.4,-8.8,8.8,10.4,12,26,40,80,92,100,110,150,200,260,
        455,480,500,520,560,600,1200};
    const std::span<const double> lateralBands=pose.adjacentWaterSide!=0
        ? std::span<const double>{lakeLateralBands} : std::span<const double>{ordinaryLateralBands};
    const bool coarseBegin=route.sample(chunk.beginDistance-world.chunkLength()*0.5).adjacentWaterSide==0;
    const bool coarseEnd=route.sample(chunk.endDistance+world.chunkLength()*0.5).adjacentWaterSide==0;
    const auto terrainHeight=[&](const Sim::RoutePose& sample,double distance,double lateral) {
        if (pose.adjacentWaterSide!=0
            && ((coarseBegin && std::abs(distance-chunk.beginDistance)<0.001)
                || (coarseEnd && std::abs(distance-chunk.endDistance)<0.001)))
        {
            // Match the coarse neighbour's exact edge, including additional shore samples.
            const auto upper=std::upper_bound(ordinaryLateralBands.begin(),ordinaryLateralBands.end(),lateral);
            const auto band=std::min(ordinaryLateralBands.size()-2,
                static_cast<std::size_t>(std::max<std::ptrdiff_t>(1,upper-ordinaryLateralBands.begin())-1));
            const double lo=ordinaryLateralBands[band],hi=ordinaryLateralBands[band+1];
            const float t=static_cast<float>((lateral-lo)/(hi-lo));
            return std::lerp(terrainOffset(sample,distance,lo),terrainOffset(sample,distance,hi),t);
        }
        return terrainOffset(sample,distance,lateral);
    };
    constexpr double kTerrainStep = 50.0;
    double cachedTerrainDistance=-1e30;
    std::size_t cachedTerrainBand=lateralBands.size();
    std::array<Vector3,4> terrainCorners;
    const auto terrainSurfacePoint = [&](double distance,double lateral) {
        // Match the rendered cell even for non-50-m-aligned chunks and terminal cells.
        distance=std::clamp(distance,0.0,route.lengthMetres()-0.001);
        const double begin=std::floor(distance/world.chunkLength())*world.chunkLength();
        const double end=std::min(begin+world.chunkLength(),route.lengthMetres());
        const double d0=begin+std::floor((distance-begin)/kTerrainStep)*kTerrainStep;
        const double d1=std::min(d0+kTerrainStep,end);
        const auto upper=std::upper_bound(lateralBands.begin(),lateralBands.end(),lateral);
        const auto band=std::min(lateralBands.size()-2,
            static_cast<std::size_t>(std::max<std::ptrdiff_t>(1,upper-lateralBands.begin())-1));
        if (d0 != cachedTerrainDistance || band != cachedTerrainBand)
        {
            const auto vertex = [&](double along,double across) {
                auto p=localRoutePoint(route,anchorDistance,along,across);
                p.y += terrainHeight(route.sample(along),along,across);
                return Vector3{p.x,p.y,p.z};
            };
            terrainCorners={vertex(d0,lateralBands[band]),vertex(d1,lateralBands[band]),
                            vertex(d1,lateralBands[band+1]),vertex(d0,lateralBands[band+1])};
            cachedTerrainDistance=d0;
            cachedTerrainBand=band;
        }
        return terrainQuadPoint(terrainCorners,static_cast<float>((distance-d0)/(d1-d0)),
            static_cast<float>((lateral-lateralBands[band])/(lateralBands[band+1]-lateralBands[band])));
    };
    auto [terrainBegin,terrainEnd]=terminalTerrainRange(
        chunk.beginDistance,chunk.endDistance,route.lengthMetres());
    // Background-only apron beyond the seven detail chunks. At least 1,250 m
    // behind/ahead of the train, safely beyond the 1,050 m fog end. No extra
    // simulation chunks, buildings, NPCs, collisions or GPU submissions.
    const auto [backdropBegin,backdropEnd]=streamedBackdropRange(
        chunk.beginDistance,chunk.endDistance,route.lengthMetres(),
        chunk.index==world.chunks().front().index,chunk.index==world.chunks().back().index);
    terrainBegin=std::min(terrainBegin,backdropBegin);
    terrainEnd=std::max(terrainEnd,backdropEnd);
    for (double d0 = terrainBegin; d0 < terrainEnd; d0 += kTerrainStep)
    {
        const double d1 = std::min(terrainEnd, d0 + kTerrainStep);
        const Sim::RoutePose pose0 = route.sample(d0);
        const Sim::RoutePose pose1 = route.sample(d1);
        for (std::size_t band = 0; band + 1 < lateralBands.size(); ++band)
        {
            const double l0 = lateralBands[band];
            const double l1 = lateralBands[band + 1];
            LocalRoutePoint a = localRoutePoint(route, anchorDistance, d0, l0);
            LocalRoutePoint b = localRoutePoint(route, anchorDistance, d1, l0);
            LocalRoutePoint c = localRoutePoint(route, anchorDistance, d1, l1);
            LocalRoutePoint d = localRoutePoint(route, anchorDistance, d0, l1);
            a.y += terrainHeight(pose0, d0, l0);
            b.y += terrainHeight(pose1, d1, l0);
            c.y += terrainHeight(pose1, d1, l1);
            d.y += terrainHeight(pose0, d0, l1);
            Color patchColor = ground;
            if (pose.urbanity <= 0.55)
            {
                patchColor = ruralParcelTint((d0+d1)*0.5,(l0+l1)*0.5);
            }
            constexpr float kTerrainTextureTileMetres = 8.0f;
            if (graphicsQuality_>=1 && (pose.waterCrossing
                || (pose.adjacentWaterSide!=0
                    && (l0+l1)*static_cast<double>(pose.adjacentWaterSide)>52.0)))
            {
                const float waterHeight=static_cast<float>(
                    (pose.waterCrossing ? pose.waterCrossingSurfaceElevation+0.015
                                        : pose.adjacentWaterSurfaceElevation+0.025)-anchorElevation);
                addShoreTerrainCell(terrain,
                    {Vector3{a.x,a.y,a.z},Vector3{b.x,b.y,b.z},Vector3{c.x,c.y,c.z},Vector3{d.x,d.y,d.z}},
                    {Vector2{static_cast<float>(l0/8),static_cast<float>(d0/8)},
                     Vector2{static_cast<float>(l0/8),static_cast<float>(d1/8)},
                     Vector2{static_cast<float>(l1/8),static_cast<float>(d1/8)},
                     Vector2{static_cast<float>(l1/8),static_cast<float>(d0/8)}},
                    waterHeight,patchColor);
                continue;
            }
            if (graphicsQuality_>=1 && pose.urbanity<=0.55
                && std::max(std::abs(l0),std::abs(l1))<=26
                && d0>=chunk.beginDistance && d0<chunk.endDistance)
            {
                // The near verge is the lower third of every seated window view: metre-scale
                // cess, mown, fence-line, drain and headland zones replace one flat colour per
                // 50 m cell. Cuts split on the source diagonal and never change terrain height.
                const std::array<Vector3,4> corners={Vector3{a.x,a.y,a.z},Vector3{b.x,b.y,b.z},
                                                   Vector3{c.x,c.y,c.z},Vector3{d.x,d.y,d.z}};
                std::array<float,32> vCuts{};
                std::size_t vCount=0;
                vCuts[vCount++]=0;
                for (double sign : {-1.0,1.0})
                    for (const double cut : vergeLateralCuts(graphicsQuality_))
                        if (sign*cut>l0+0.05 && sign*cut<l1-0.05 && vCount<vCuts.size()-1)
                            vCuts[vCount++]=static_cast<float>((sign*cut-l0)/(l1-l0));
                std::sort(vCuts.begin()+1,vCuts.begin()+static_cast<std::ptrdiff_t>(vCount));
                vCuts[vCount++]=1;
                // The running-line band holds the visible verge between ballast and fence; a
                // finer detail tile than the fields gives it grass-scale grain.
                const bool runningBand=std::max(std::abs(l0),std::abs(l1))<=kVergeFenceLineEnd+0.01;
                const float tileMetres=runningBand ? 3.0f : kTerrainTextureTileMetres;
                std::array<float,21> uCuts{};
                const int alongSegments=std::clamp(static_cast<int>(std::lround(
                    (d1-d0)/vergeAlongStep(graphicsQuality_))),1,20);
                for (int i=0;i<=alongSegments;++i)
                    uCuts[static_cast<std::size_t>(i)]=static_cast<float>(i)/static_cast<float>(alongSegments);
                addGridTerrainCell(terrain,corners,
                    std::span<const float>{uCuts.data(),static_cast<std::size_t>(alongSegments+1)},
                    std::span<const float>{vCuts.data(),vCount},
                    [&](float u,float v) { return ruralVergeTint(d0+(d1-d0)*u,l0+(l1-l0)*v); },
                    [&](float u,float v) {
                        return Vector2{static_cast<float>(l0+(l1-l0)*v)/tileMetres,
                                       static_cast<float>(d0+(d1-d0)*u)/tileMetres};
                    });
                continue;
            }
            if (graphicsQuality_>=1 && pose.urbanity<=0.55
                && std::min(std::abs(l0),std::abs(l1))>=26
                && std::max(std::abs(l0),std::abs(l1))<=260)
            {
                // Matching U/V splits preserve the original a-c terrain diagonal exactly.
                // Five-metre samples near the 50 m cell edge retain narrow field margins
                // instead of smearing a whole cell or changing its authoritative height.
                const std::array<Vector3,4> corners={Vector3{a.x,a.y,a.z},Vector3{b.x,b.y,b.z},
                                                   Vector3{c.x,c.y,c.z},Vector3{d.x,d.y,d.z}};
                const auto colour=[&](float u,float v) {
                    return ruralTerrainSurfaceTint(d0+(d1-d0)*u,l0+(l1-l0)*v);
                };
                const auto uv=[&](float u,float v) {
                    return Vector2{static_cast<float>(l0+(l1-l0)*v)/kTerrainTextureTileMetres,
                                   static_cast<float>(d0+(d1-d0)*u)/kTerrainTextureTileMetres};
                };
                addSubdividedTerrainCell(terrain,corners,colour,uv);
                continue;
            }
            if (graphicsQuality_>=1 && pose.urbanity>0.55 && !pose.tunnel
                && std::max(std::abs(l0),std::abs(l1))<=kUrbanVergeEnd+0.01
                && d0>=chunk.beginDistance && d0<chunk.endDistance)
            {
                // Urban lineside: gravel cess, weedy ballast dust and a scrub strip along the
                // palisade replace the flat street-ground gradient below the window.
                const std::array<Vector3,4> corners={Vector3{a.x,a.y,a.z},Vector3{b.x,b.y,b.z},
                                                   Vector3{c.x,c.y,c.z},Vector3{d.x,d.y,d.z}};
                std::array<float,24> vCuts{};
                std::size_t vCount=0;
                vCuts[vCount++]=0;
                for (double sign : {-1.0,1.0})
                    for (const double cut : urbanVergeLateralCuts(graphicsQuality_))
                        if (sign*cut>l0+0.05 && sign*cut<l1-0.05 && vCount<vCuts.size()-1)
                            vCuts[vCount++]=static_cast<float>((sign*cut-l0)/(l1-l0));
                std::sort(vCuts.begin()+1,vCuts.begin()+static_cast<std::ptrdiff_t>(vCount));
                vCuts[vCount++]=1;
                std::array<float,21> uCuts{};
                const int alongSegments=std::clamp(static_cast<int>(std::lround(
                    (d1-d0)/vergeAlongStep(graphicsQuality_))),1,20);
                for (int i=0;i<=alongSegments;++i)
                    uCuts[static_cast<std::size_t>(i)]=static_cast<float>(i)/static_cast<float>(alongSegments);
                const double routeLength=route.lengthMetres();
                addGridTerrainCell(terrain,corners,
                    std::span<const float>{uCuts.data(),static_cast<std::size_t>(alongSegments+1)},
                    std::span<const float>{vCuts.data(),vCount},
                    [&](float u,float v) {
                        return urbanVergeTint(d0+(d1-d0)*u,l0+(l1-l0)*v,routeLength);
                    },
                    [&](float u,float v) {
                        return Vector2{static_cast<float>(l0+(l1-l0)*v)/3.0f,
                                       static_cast<float>(d0+(d1-d0)*u)/3.0f};
                    });
                continue;
            }
            const float u0 = static_cast<float>(l0) / kTerrainTextureTileMetres;
            const float u1 = static_cast<float>(l1) / kTerrainTextureTileMetres;
            const float v0 = static_cast<float>(d0) / kTerrainTextureTileMetres;
            const float v1 = static_cast<float>(d1) / kTerrainTextureTileMetres;
            terrain.addTexturedQuad(
                Vector3{a.x,a.y,a.z},Vector3{b.x,b.y,b.z},
                Vector3{c.x,c.y,c.z},Vector3{d.x,d.y,d.z},
                pose.urbanity > 0.55 ? terminalGroundTint(d0,l0,route.lengthMetres()) : patchColor,
                pose.urbanity > 0.55 ? terminalGroundTint(d1,l0,route.lengthMetres()) : patchColor,
                pose.urbanity > 0.55 ? terminalGroundTint(d1,l1,route.lengthMetres()) : patchColor,
                pose.urbanity > 0.55 ? terminalGroundTint(d0,l1,route.lengthMetres()) : patchColor,
                Vector2{u0,v0},Vector2{u0,v1},Vector2{u1,v1},Vector2{u1,v0});
        }
        if (d0 >= chunk.beginDistance && d0 < chunk.endDistance
            && !pose.tunnel && !pose.bridge && pose.urbanity < 0.55
            && pose.adjacentWaterSide == 0)
            counters.staticWorldTracksideDrainageSections_ += 2;
    }

    // Absolute route slots add the modest signalling/telecom cabinets that line a maintained
    // electrified railway. They alternate side, avoid platforms and major structures, follow
    // terrain, and retain deterministic condition colours across chunk unload/reload. A
    // concrete plinth, inset door, louvres, warning plate and adjacent cable marker form one
    // bounded group in the existing static and contact-shadow batches.
    constexpr double kCabinetSpacing = 420.0;
    constexpr double kCabinetOffset = 137.0;
    const auto firstCabinetSlot = static_cast<std::int64_t>(std::ceil(
        (chunk.beginDistance-kCabinetOffset)/kCabinetSpacing));
    for (std::int64_t slot = firstCabinetSlot;; ++slot)
    {
        const double cabinetDistance = static_cast<double>(slot)*kCabinetSpacing
                                     + kCabinetOffset;
        if (cabinetDistance >= chunk.endDistance) break;
        if (cabinetDistance < chunk.beginDistance || cabinetDistance < 0.0
            || cabinetDistance > route.lengthMetres()) continue;
        const Sim::RoutePose cabinetPose = route.sample(cabinetDistance);
        if (cabinetPose.tunnel || cabinetPose.bridge || cabinetPose.waterCrossing) continue;
        const Sim::Station& nearestStation = route.stations()[
            route.nearestStation(cabinetDistance)];
        if (std::abs(nearestStation.distanceMetres-cabinetDistance) < 230.0) continue;

        const double side = (slot&1) == 0 ? -1.0 : 1.0;
        const int variant = static_cast<int>((std::abs(slot)*5+3)%4);
        const double lateral = side*(6.75+static_cast<double>(variant)*0.16);
        LocalRoutePoint cabinet = localRoutePoint(
            route,anchorDistance,cabinetDistance,lateral);
        cabinet.y += terrainOffset(cabinetPose,cabinetDistance,lateral);
        const std::array<Color,4> cabinetColours = {
            Color(115,124,119,255),Color(103,119,112,255),
            Color(118,121,109,255),Color(96,113,116,255)};
        addOrientedBox(mesh,cabinet.x,cabinet.y+0.08f,cabinet.z,
                       1.02f,0.16f,0.72f,cabinet.yaw,Color(128,127,117,255));
        addOrientedBox(mesh,cabinet.x,cabinet.y+0.72f,cabinet.z,
                       0.80f,1.20f,0.50f,cabinet.yaw,
                       cabinetColours[static_cast<std::size_t>(variant)]);
        const double faceLateral = lateral-side*0.415;
        const LocalRoutePoint face = localRoutePoint(
            route,anchorDistance,cabinetDistance,faceLateral);
        addOrientedBox(mesh,face.x,cabinet.y+0.73f,face.z,
                       0.035f,1.02f,0.39f,face.yaw,Color(145,151,144,255));
        for (float ventHeight : {0.92f,0.99f,1.06f})
            addOrientedBox(mesh,face.x,cabinet.y+ventHeight,face.z,
                           0.041f,0.022f,0.24f,face.yaw,Color(70,79,77,255));
        addOrientedBox(mesh,face.x,cabinet.y+0.57f,face.z,
                       0.043f,0.14f,0.17f,face.yaw,Color(230,191,52,255));

        const double markerDistance = cabinetDistance+2.15;
        const LocalRoutePoint marker = localRoutePoint(
            route,anchorDistance,markerDistance,lateral);
        addOrientedBox(mesh,marker.x,cabinet.y+0.48f,marker.z,
                       0.10f,0.82f,0.10f,marker.yaw,Color(102,108,105,255));
        addOrientedBox(mesh,marker.x,cabinet.y+0.91f,marker.z,
                       0.18f,0.12f,0.18f,marker.yaw,Color(226,180,45,255));
        if (graphicsQuality_ >= 1)
            addOrientedGroundQuad(groundShadows,cabinet.x,cabinet.y+0.025f,cabinet.z,
                                  1.22f,0.90f,cabinet.yaw,Color(8,12,11,34));
        ++counters.staticWorldLinesideEquipmentCabinets_;
    }

    // The Lakehurst approach follows a level upland lake for several kilometres. Route data
    // supplies its side and absolute surface elevation; the surrounding terrain helper carves
    // a basin, while these streamed patches provide one continuous material surface. The
    // neutral mipmapped wave detail is tinted here, keeping lake identity in route geometry.
    constexpr double kLakeStep = 50.0;
    for (double d0 = backdropBegin; d0 < backdropEnd; d0 += kLakeStep)
    {
        const double d1 = std::min(backdropEnd,d0+kLakeStep);
        const double midpoint = (d0+d1)*0.5;
        const Sim::RoutePose lakePose = route.sample(midpoint);
        if (lakePose.adjacentWaterSide == 0) continue;
        const double side = static_cast<double>(lakePose.adjacentWaterSide);
        // Extend under dry ground: the graded terrain, not the quad edge, defines the shore.
        const double inner0 = side*60.0;
        const double inner1 = side*60.0;
        const double outer0 = side*(520.0+16.0*std::sin(d0/410.0+0.7));
        const double outer1 = side*(520.0+16.0*std::sin(d1/410.0+0.7));
        LocalRoutePoint a = localRoutePoint(route,anchorDistance,d0,inner0);
        LocalRoutePoint b = localRoutePoint(route,anchorDistance,d1,inner1);
        LocalRoutePoint c = localRoutePoint(route,anchorDistance,d1,outer1);
        LocalRoutePoint d = localRoutePoint(route,anchorDistance,d0,outer0);
        const float waterY = static_cast<float>(
            lakePose.adjacentWaterSurfaceElevation-anchorElevation+0.025);
        a.y = waterY;
        b.y = waterY;
        c.y = waterY;
        d.y = waterY;
        const Color shoreWater(76,143,161,255);
        const Color deepWater(45,111,140,255);
        constexpr float kWaterTextureTileMetres = 7.5f;
        const Vector2 uvA{static_cast<float>(inner0)/kWaterTextureTileMetres,
                          static_cast<float>(d0)/kWaterTextureTileMetres};
        const Vector2 uvB{static_cast<float>(inner1)/kWaterTextureTileMetres,
                          static_cast<float>(d1)/kWaterTextureTileMetres};
        const Vector2 uvC{static_cast<float>(outer1)/kWaterTextureTileMetres,
                          static_cast<float>(d1)/kWaterTextureTileMetres};
        const Vector2 uvD{static_cast<float>(outer0)/kWaterTextureTileMetres,
                          static_cast<float>(d0)/kWaterTextureTileMetres};
        if (side > 0.0)
            waterMaterial.addTexturedQuad(
                Vector3{a.x,a.y,a.z},Vector3{b.x,b.y,b.z},
                Vector3{c.x,c.y,c.z},Vector3{d.x,d.y,d.z},
                shoreWater,shoreWater,deepWater,deepWater,uvA,uvB,uvC,uvD);
        else
            waterMaterial.addTexturedQuad(
                Vector3{d.x,d.y,d.z},Vector3{c.x,c.y,c.z},
                Vector3{b.x,b.y,b.z},Vector3{a.x,a.y,a.z},
                deepWater,deepWater,shoreWater,shoreWater,uvD,uvC,uvB,uvA);
        ++counters.staticWorldLakePatches_;

        if (d0<chunk.beginDistance || d0>=chunk.endDistance) continue;

        // Reeds make the near bank readable from a side window. Highlights are now
        // view-dependent water shading, not detached bright bars in the world mesh.
        for (int reed = -2; reed <= 2; ++reed)
        {
            const double reedDistance = midpoint+static_cast<double>(reed)*6.2;
            const auto shore=shoreCoordinateAtHeight([&](double across) {
                return terrainSurfacePoint(reedDistance,side*across).Y;
            },40,150,waterY+0.12f);
            if (!shore) continue;
            const auto seed=treeShapeSeed(reedDistance,side*(*shore),17U);
            const int blades=graphicsQuality_==0 ? 3 : 7;
            for (int blade=0;blade<blades;++blade)
            {
                const float phase=static_cast<float>(seed%97U)*0.37f+static_cast<float>(blade)*2.4f;
                const auto root=terrainSurfacePoint(reedDistance+std::sin(phase)*0.65,
                    side*(*shore+std::cos(phase)*0.28));
                addGrassBlade(mesh,root,1.0f+0.45f*(0.5f+0.5f*std::sin(phase*1.7f)),
                    0.06f,phase,blade%2==0 ? Color(101,115,62,255) : Color(76,101,48,255));
            }
        }
        const auto stripe = static_cast<std::int64_t>(std::floor(d0/kLakeStep));

        // A layered wooded far bank prevents the water from terminating at a bare, flat
        // horizon. Low keeps a sparse silhouette; Medium and High add staggered deciduous
        // and conifer crowns. Everything remains in the chunk's one immutable world batch.
        const int bankTreeStride = graphicsQuality_ == 0 ? 2 : 1;
        if (stripe % bankTreeStride == 0)
        {
            const int bankTreeCount = graphicsQuality_ == 0 ? 1
                                    : graphicsQuality_ == 1 ? 2 : 4;
            for (int tree = 0; tree < bankTreeCount; ++tree)
            {
                const auto placement=lakeBankTreePlacement(stripe,tree*(4/bankTreeCount));
                const double treeDistance = placement.distance;
                const double bankLateral = side*placement.lateral;
                const Vector3 bankGround=terrainSurfacePoint(treeDistance,bankLateral);
                const LocalRoutePoint bank{bankGround.X,bankGround.Y,bankGround.Z,0};
                // The bank is more than half a kilometre from the passenger. Mature
                // 9-14 m trees retain a readable silhouette there; sapling-sized geometry
                // collapses to sub-pixel dots even though it is technically present.
                const float treeScale = placement.scale;
                addTaperedTreeTrunk(mesh,Vector3{bank.x,bank.y,bank.z},treeScale,
                                   3.1f,6,Color(83,66,44,255));
                const int variant = static_cast<int>(placement.variant);
                if (variant == 0)
                {
                    counters.staticWorldConiferBranchTiers_ += addLayeredConifer(
                        mesh,Vector3{bank.x,bank.y,bank.z},treeScale,7,
                        static_cast<std::uint32_t>(variant),Color(38,83,45,255),
                        Color(54,97,55,255),false);
                }
                else
                {
                    const Color crown = variant == 1 ? Color(47,101,51,255)
                                                     : Color(55,94,42,255);
                    mesh.addEllipsoid(
                        Vector3{bank.x,bank.y+3.7f*treeScale,bank.z},
                        Vector3{2.15f*treeScale,2.25f*treeScale,1.75f*treeScale},
                        7,4,crown);
                }
                ++counters.staticWorldLakeBankTrees_;
            }
        }
    }
    // The crossed estuary uses the route's absolute surface elevation and the same 50 m
    // curved cadence as terrain. Sampling each strip removes the former 250 m chunk-boundary
    // truncation and prevents the water from inheriting the railway's longitudinal grade.
    // Five lateral colour bands keep the broad channel from reading as a single blue slab;
    // all bands share the lake's deterministic mipmapped wave material and remain one draw.
    constexpr double kRiverWaterStep = 50.0;
    constexpr std::array<double,6> kRiverLateralBands = {
        -1'250.0,-560.0,-150.0,150.0,560.0,1'250.0};
    const std::array<Color,6> kRiverBandColours = {
        Color(70,137,157,255), Color(57,125,151,255), Color(46,111,143,255),
        Color(46,111,143,255), Color(57,125,151,255), Color(70,137,157,255)};
    constexpr float kRiverWaterTextureTileMetres = 8.5f;
    for (double waterBegin = backdropBegin; waterBegin < backdropEnd;
         waterBegin += kRiverWaterStep)
    {
        const double waterEnd = std::min(backdropEnd,waterBegin+kRiverWaterStep);
        const double waterMiddle = (waterBegin+waterEnd)*0.5;
        const Sim::RoutePose waterPose = route.sample(waterMiddle);
        if (!waterPose.waterCrossing) continue;
        const float waterY = static_cast<float>(
            waterPose.waterCrossingSurfaceElevation-anchorElevation+0.015);
        for (std::size_t band = 0; band + 1 < kRiverLateralBands.size(); ++band)
        {
            const double lateral0 = kRiverLateralBands[band];
            const double lateral1 = kRiverLateralBands[band+1];
            LocalRoutePoint a = localRoutePoint(
                route,anchorDistance,waterBegin,lateral0);
            LocalRoutePoint b = localRoutePoint(
                route,anchorDistance,waterEnd,lateral0);
            LocalRoutePoint c = localRoutePoint(
                route,anchorDistance,waterEnd,lateral1);
            LocalRoutePoint d = localRoutePoint(
                route,anchorDistance,waterBegin,lateral1);
            a.y = waterY;
            b.y = waterY;
            c.y = waterY;
            d.y = waterY;
            const Vector2 uvA{
                static_cast<float>(lateral0)/kRiverWaterTextureTileMetres,
                static_cast<float>(waterBegin)/kRiverWaterTextureTileMetres};
            const Vector2 uvB{
                static_cast<float>(lateral0)/kRiverWaterTextureTileMetres,
                static_cast<float>(waterEnd)/kRiverWaterTextureTileMetres};
            const Vector2 uvC{
                static_cast<float>(lateral1)/kRiverWaterTextureTileMetres,
                static_cast<float>(waterEnd)/kRiverWaterTextureTileMetres};
            const Vector2 uvD{
                static_cast<float>(lateral1)/kRiverWaterTextureTileMetres,
                static_cast<float>(waterBegin)/kRiverWaterTextureTileMetres};
            waterMaterial.addTexturedQuad(
                Vector3{a.x,a.y,a.z},Vector3{b.x,b.y,b.z},
                Vector3{c.x,c.y,c.z},Vector3{d.x,d.y,d.z},
                kRiverBandColours[band],kRiverBandColours[band],
                kRiverBandColours[band+1],kRiverBandColours[band+1],
                uvA,uvB,uvC,uvD);
        }
        ++counters.staticWorldRiverWaterSections_;
    }

    // Riparian vegetation belongs to the actual bank contour, not the field parcel edge.
    // Absolute route slots preserve identities while chunks are recycled.
    if (graphicsQuality_>=1)
        for (double distance=std::ceil(chunk.beginDistance/23.0)*23.0;
             distance<chunk.endDistance;distance+=23.0)
        {
            const auto riverPose=route.sample(distance);
            if (!riverPose.waterCrossing) continue;
            const float waterHeight=static_cast<float>(
                riverPose.waterCrossingSurfaceElevation-anchorElevation+0.015);
            for (double side : {-1.0,1.0})
            {
                const auto shore=shoreCoordinateAtHeight([&](double across) {
                    return terrainSurfacePoint(distance,side*across).Y;
                },80,300,waterHeight+0.45f);
                if (!shore) continue;
                const auto seed=treeShapeSeed(distance,side*(*shore),31U);
                const int blades=graphicsQuality_==1 ? 7 : 13;
                for (int blade=0;blade<blades;++blade)
                {
                    const float phase=static_cast<float>(seed%97U)*0.37f+static_cast<float>(blade)*2.4f;
                    const auto root=terrainSurfacePoint(distance+std::sin(phase)*2.7,
                        side*(*shore+std::cos(phase)*0.65));
                    addGrassBlade(mesh,root,1.25f+0.75f*(0.5f+0.5f*std::sin(phase*1.7f)),
                        0.09f,phase,blade%2==0 ? Color(105,118,61,255) : Color(66,99,48,255));
                }
                if (seed%3U!=0)
                {
                    const auto root=terrainSurfacePoint(distance+3.0,side*(*shore+4.0));
                    foliageMaterial.addFoliageLobe(root+Vector3{0,0.75f,0},
                        Vector3{2.2f,0.95f,1.7f},7,4,Color(68,104,49,255),true);
                }
                ++counters.staticWorldRiverBankClumps_;
            }
        }
    if (pose.urbanity > 0.28 && !pose.tunnel && !pose.bridge)
    {
      // Shared endpoint samples prevent gaps between curved pavement/road strips and at
      // chunk seams. Only the exposed top and side skirts need geometry.
      const auto addStreetStrip = [&](double lateral, float width, float top,
                                      float thickness, const Color& color,
                                      MeshBuilder* surface=nullptr) {
        const auto vertexAt = [&](double distance, double offset, float height) {
            const auto p = localRoutePoint(route,anchorDistance,distance,offset);
            return Vector3{p.x,p.y+height,p.z};
        };
        for (double begin = chunk.beginDistance; begin < chunk.endDistance; begin += 25.0)
        {
            const double end = std::min(begin+25.0,chunk.endDistance);
            const Vector3 a = vertexAt(begin,lateral-width*0.5,top);
            const Vector3 b = vertexAt(end,lateral-width*0.5,top);
            const Vector3 c = vertexAt(end,lateral+width*0.5,top);
            const Vector3 d = vertexAt(begin,lateral+width*0.5,top);
            const Vector3 down{0,-thickness,0};
            if (surface && graphicsQuality_>=1)
            {
                const float u0=static_cast<float>(begin-chunk.beginDistance)/0.65f;
                const float u1=static_cast<float>(end-chunk.beginDistance)/0.65f;
                surface->addTexturedQuad(a,b,c,d,color,{u0,0},{u1,0},
                    {u1,width/0.65f},{u0,width/0.65f});
            }
            else mesh.addQuad(a,b,c,d,color);
            mesh.addQuad(a,a+down,b+down,b,color);
            mesh.addQuad(c,c+down,d+down,d,color);
            if (begin == chunk.beginDistance)
                mesh.addQuad(a,d,d+down,a+down,color);
            if (end == chunk.endDistance)
                mesh.addQuad(b,b+down,c+down,c,color);
        }
      };
      for (double roadSide : {-1.0,1.0})
      {
        addStreetStrip(roadSide*18.3,7.0f,-0.22f,0.12f,Color(75,78,80,255),&terrain);
        for (double edgeMagnitude : {14.80, 21.80})
        {
            if (graphicsQuality_==0)
                addStreetStrip(roadSide*edgeMagnitude,0.24f,-0.08f,0.16f,
                               Color(151,151,143,255));
            else
                for (double begin=chunk.beginDistance;begin<chunk.endDistance;begin+=25.0)
                    addStreetKerb(platformMaterial,[&](float offset,double distance) {
                        const auto p=localRoutePoint(route,anchorDistance,distance,
                            roadSide*edgeMagnitude+offset);
                        return Vector3{p.x,p.y,p.z};
                    },begin,std::min(begin+25.0,chunk.endDistance));
        }
        // Continuous edge lines and sparse kerb inlets make the parallel road legible even
        // between centre-line dashes. They follow the same chunk curve as the carriageway,
        // avoiding a new road mesh or dynamic traffic dependency.
        for (double edgeMagnitude : {15.18,21.42})
        {
            addStreetStrip(roadSide*edgeMagnitude,0.095f,-0.1925f,0.025f,
                           Color(215,211,194,255));
            ++counters.staticWorldUrbanRoadEdgeMarkingSections_;
        }
        double inletDistance = std::ceil(chunk.beginDistance/75.0)*75.0;
        for (; inletDistance < chunk.endDistance; inletDistance += 75.0)
        {
            const LocalRoutePoint inlet = localRoutePoint(
                route,anchorDistance,inletDistance,roadSide*21.69);
            addOrientedBox(mesh,inlet.x,inlet.y-0.070f,inlet.z,
                           0.32f,0.035f,0.62f,inlet.yaw,Color(45,50,50,255));
            for (float slot : {-0.19f,0.0f,0.19f})
            {
                const Matrix inletPlacement = Matrix::CreateTranslation(0.0f,0.0f,slot)
                                            * Matrix::CreateRotationY(inlet.yaw)
                                            * Matrix::CreateTranslation(
                                                inlet.x,inlet.y-0.047f,inlet.z);
                mesh.addBox(Vector3{-0.12f,-0.008f,-0.018f},
                            Vector3{0.12f,0.008f,0.018f},
                            Color(92,98,96,255),inletPlacement);
            }
            ++counters.staticWorldUrbanRoadDrainGrates_;
        }
        for (double markingDistance = chunk.beginDistance + 6.0;
             markingDistance < chunk.endDistance; markingDistance += 18.0)
        {
            const LocalRoutePoint marking = localRoutePoint(
                route,anchorDistance,markingDistance,roadSide*18.3);
            addOrientedBox(mesh,marking.x,marking.y-0.205f,marking.z,
                           0.11f,0.025f,7.0f,marking.yaw,Color(224,221,202,255));
        }

        // Narrow raised footways turn the road and nearby architecture into a coherent
        // streetscape. Their inner/outer edges meet the kerbs and the townhouse setback,
        // rather than leaving an unexplained strip of terrain between them.
        for (double lateralMagnitude : {13.4, 23.3})
        {
            const float width=lateralMagnitude<20 ? 2.6f : 2.8f;
            addStreetStrip(roadSide*lateralMagnitude,width,-0.07f,0.09f,
                           Color(148,148,140,255),&platformMaterial);
            if (graphicsQuality_>=1)
            {
                const auto joint=[&](double d0,double d1,double l0,double l1) {
                    const auto p=[&](double distance,double lateral) {
                        const auto point=localRoutePoint(route,anchorDistance,distance,lateral);
                        return Vector3{point.x,point.y-0.066f,point.z};
                    };
                    mesh.addQuad(p(d0,l0),p(d1,l0),p(d1,l1),p(d0,l1),Color(112,113,107,255));
                };
                const double low=roadSide*lateralMagnitude-width*0.5;
                const double high=low+width;
                // Thin 650 mm slab joints only within a passenger-readable near range.
                for (double d=std::ceil(chunk.beginDistance/0.65)*0.65;d<chunk.endDistance;d+=0.65)
                {
                    const auto centre=localRoutePoint(route,anchorDistance,d,roadSide*lateralMagnitude);
                    if (std::abs(centre.z)>250.0f) continue;
                    const double end=std::min(d+0.65,chunk.endDistance);
                    joint(d,std::min(d+0.006,chunk.endDistance),low,high);
                    for (int row=1;row<4;++row)
                    {
                        const double edge=low+static_cast<double>(width)*row/4.0;
                        joint(d,end,edge-0.003,edge+0.003);
                    }
                }
            }
        }

        // Absolute route alignment keeps lamps stable at chunk boundaries. Only the outer
        // footway receives columns, preserving a clear railway-side verge and keeping the
        // silhouette sparse enough to read from a moving passenger window.
        double lampDistance = std::ceil(chunk.beginDistance / 55.0) * 55.0;
        for (; lampDistance < chunk.endDistance; lampDistance += 55.0)
        {
            const LocalRoutePoint pole = localRoutePoint(
                route,anchorDistance,lampDistance,roadSide*23.65);
            addOrientedBox(mesh,pole.x,pole.y+2.08f,pole.z,
                           0.10f,4.35f,0.10f,pole.yaw,Color(67,72,72,255));
            const LocalRoutePoint arm = localRoutePoint(
                route,anchorDistance,lampDistance,roadSide*23.05);
            addOrientedBox(mesh,arm.x,arm.y+4.18f,arm.z,
                           1.30f,0.09f,0.10f,arm.yaw,Color(67,72,72,255));
            const LocalRoutePoint head = localRoutePoint(
                route,anchorDistance,lampDistance,roadSide*22.43);
            addOrientedBox(mesh,head.x,head.y+4.10f,head.z,
                           0.42f,0.13f,0.30f,head.yaw,Color(238,224,176,255));
            addOrientedBox(nightGlow(),head.x,head.y+4.10f,head.z,
                           0.44f,0.15f,0.32f,head.yaw,Color(255,214,150,255));
            addLightPool(nightGlow(),Vector3{head.x,head.y+0.06f,head.z},7.5f,
                         Color(96,74,44,255));
        }
      }
    }

    // A contact wire, sagged messenger and absolute 25 m dropper rhythm make the electrified
    // route legible at window-viewing speed. Both running lines remain part of this one static
    // chunk batch, including through tunnels and across bridges.
    constexpr float kContactWireHeight = 5.85f;
    constexpr float kMessengerWireHeight = 6.38f;
    constexpr float kMessengerSag = 0.22f;
    // Both wires are chords between absolute-cadence route points, so curves and the
    // messenger sag stay continuous across chunk seams instead of stepping sideways.
    const double messengerStep = graphicsQuality_ >= 2 ? 12.5 : 25.0;
    const auto messengerSag = [&](double distance) {
        const double spanPhase = std::fmod(distance,50.0)/50.0;
        return static_cast<float>(4.0*spanPhase*(1.0-spanPhase))*kMessengerSag;
    };
    const auto wireEnd = [&](double distance,bool parallel,float height) {
        const double lateral = parallel ? route.parallelTrackLateralMetres(distance) : 0.0;
        const LocalRoutePoint p = localRoutePoint(route,anchorDistance,distance,lateral);
        return Vector3{p.x,p.y+height,p.z};
    };
    double wireBegin = chunk.beginDistance;
    while (wireBegin < chunk.endDistance)
    {
        const double wireEndDistance = std::min(chunk.endDistance,
            (std::floor(wireBegin/messengerStep)+1.0)*messengerStep);
        for (bool parallel : {false,true})
        {
            addBarBetween(mesh,wireEnd(wireBegin,parallel,kContactWireHeight),
                          wireEnd(wireEndDistance,parallel,kContactWireHeight),
                          0.045f,Color(72,76,73,255),0.03f);
            addBarBetween(mesh,
                          wireEnd(wireBegin,parallel,kMessengerWireHeight-messengerSag(wireBegin)),
                          wireEnd(wireEndDistance,parallel,
                                  kMessengerWireHeight-messengerSag(wireEndDistance)),
                          0.038f,Color(67,72,69,255),0.03f);
            ++counters.staticWorldCatenaryMessengerSegments_;
        }
        wireBegin = wireEndDistance;
    }
    const auto firstDropper = static_cast<std::int64_t>(
        std::ceil(chunk.beginDistance/25.0));
    const auto endDropper = static_cast<std::int64_t>(
        std::ceil(chunk.endDistance/25.0));
    for (std::int64_t dropper = firstDropper; dropper < endDropper; ++dropper)
    {
        const double dropperDistance = static_cast<double>(dropper)*25.0;
        const double spanPhase = std::fmod(dropperDistance,50.0)/50.0;
        const float sag = static_cast<float>(
            4.0*spanPhase*(1.0-spanPhase))*kMessengerSag;
        const float messengerHeight = kMessengerWireHeight-sag;
        const std::array<double,2> trackLaterals = {
            0.0,route.parallelTrackLateralMetres(dropperDistance)};
        for (double lateral : trackLaterals)
        {
            const LocalRoutePoint hanger = localRoutePoint(
                route,anchorDistance,dropperDistance,lateral);
            addOrientedBox(mesh,hanger.x,
                           hanger.y+(kContactWireHeight+messengerHeight)*0.5f,
                           hanger.z,0.032f,
                           messengerHeight-kContactWireHeight,0.032f,
                           hanger.yaw,Color(76,82,78,255));
            ++counters.staticWorldCatenaryDroppers_;
        }
    }

    const auto addTunnelPortal = [&](double portalDistance) {
        const LocalRoutePoint portal = localRoutePoint(
            route, anchorDistance, portalDistance, -2.5);
        addOrientedBox(mesh,portal.x,portal.y+6.4f,portal.z,
                       15.0f,1.4f,1.0f,portal.yaw,Color(103,99,89,255));
        for (double lateral : {-9.3, 4.3})
        {
            const LocalRoutePoint side = localRoutePoint(
                route, anchorDistance, portalDistance, lateral);
            addOrientedBox(mesh,side.x,side.y+3.0f,side.z,
                           1.4f,6.0f,1.0f,side.yaw,Color(103,99,89,255));
        }
    };
    const TunnelLiningStyle tunnelStyle = tunnelLiningStyle();
    const auto tunnelPoint = [&](double distance,double lateral) {
        return localRoutePoint(route,anchorDistance,distance,lateral);
    };
    for (double tunnelBegin = chunk.beginDistance; tunnelBegin < chunk.endDistance;
         tunnelBegin += 20.0)
    {
        const double tunnelEnd = std::min(chunk.endDistance, tunnelBegin + 20.0);
        const double tunnelMiddle = (tunnelBegin + tunnelEnd) * 0.5;
        if (!route.sample(tunnelMiddle).tunnel) continue;
        const float tunnelLength = static_cast<float>(tunnelEnd - tunnelBegin) + 0.3f;
        const LocalRoutePoint leftWall = localRoutePoint(
            route, anchorDistance, tunnelMiddle, 4.2);
        const LocalRoutePoint rightWall = localRoutePoint(
            route, anchorDistance, tunnelMiddle, -9.2);
        const LocalRoutePoint roof = localRoutePoint(
            route, anchorDistance, tunnelMiddle, -2.5);
        addOrientedBox(mesh,leftWall.x,leftWall.y+3.5f,leftWall.z,
                       0.55f,7.0f,tunnelLength,leftWall.yaw,Color(98,96,90,255));
        addOrientedBox(mesh,rightWall.x,rightWall.y+3.5f,rightWall.z,
                       0.55f,7.0f,tunnelLength,rightWall.yaw,Color(98,96,90,255));
        addOrientedBox(mesh,roof.x,roof.y+7.0f,roof.z,
                       14.2f,0.55f,tunnelLength,roof.yaw,Color(82,80,75,255));
        // Raised evacuation walks make both sides usable railway infrastructure instead of
        // bare walls. Their pale inner edges remain legible during the tunnel light fade.
        for (double lateral : {3.45, -8.45})
        {
            const LocalRoutePoint walkway = localRoutePoint(
                route,anchorDistance,tunnelMiddle,lateral);
            addOrientedBox(mesh,walkway.x,walkway.y+0.10f,walkway.z,
                           1.15f,0.20f,tunnelLength,walkway.yaw,
                           Color(104,105,98,255));
        }
        for (double lateral : {2.88, -7.88})
        {
            const LocalRoutePoint edge = localRoutePoint(
                route,anchorDistance,tunnelMiddle,lateral);
            addOrientedBox(mesh,edge.x,edge.y+0.23f,edge.z,
                           0.10f,0.10f,tunnelLength,edge.yaw,
                           Color(194,193,166,255));
        }

        // Precast lining with ring and longitudinal joints, a roof haunch, handrail and
        // cable tray on brackets. The textured face shares the streamed concrete material.
        for (const auto& [face,inward] : {std::pair{3.925,-1.0f},std::pair{-8.925,1.0f}})
        {
            counters.staticWorldTunnelLiningRings_ += addTunnelLiningSegment(
                mesh,platformMaterial,tunnelPoint,tunnelBegin,tunnelEnd,face,inward,
                graphicsQuality_>0,tunnelStyle);
            addTunnelServiceRuns(mesh,tunnelPoint,tunnelBegin,tunnelEnd,face,inward,
                                 tunnelStyle);
        }
        if (!route.sample(std::max(0.0, tunnelBegin - 0.1)).tunnel)
            addTunnelPortal(tunnelBegin);
        if (!route.sample(std::min(route.lengthMetres(), tunnelEnd + 0.1)).tunnel)
            addTunnelPortal(tunnelEnd);
    }

    // Absolute 100 m placement avoids duplicates at 250 m chunk seams. Each emergency bay is
    // a shallow applied recess rather than a hole in the structural wall, a robust OpenGL 3.3
    // approximation that reads clearly from either passenger-facing window.
    double nicheDistance = std::ceil(chunk.beginDistance / 100.0) * 100.0;
    for (; nicheDistance < chunk.endDistance; nicheDistance += 100.0)
    {
        if (!route.sample(nicheDistance).tunnel) continue;
        for (double side : {1.0, -1.0})
        {
            ++counters.staticWorldTunnelSafetyNiches_;
            const double wallLateral = side > 0.0 ? 3.88 : -8.88;
            const double faceLateral = side > 0.0 ? 3.82 : -8.82;
            const LocalRoutePoint recess = localRoutePoint(
                route,anchorDistance,nicheDistance,wallLateral);
            addOrientedBox(mesh,recess.x,recess.y+1.48f,recess.z,
                           0.16f,2.95f,2.35f,recess.yaw,Color(45,50,49,255));
            const LocalRoutePoint door = localRoutePoint(
                route,anchorDistance,nicheDistance,faceLateral);
            addOrientedBox(mesh,door.x,door.y+1.40f,door.z,
                           0.10f,2.55f,1.72f,door.yaw,Color(91,101,98,255));
            for (double jambOffset : {-0.94, 0.94})
            {
                const LocalRoutePoint jamb = localRoutePoint(
                    route,anchorDistance,nicheDistance+jambOffset,faceLateral);
                addOrientedBox(mesh,jamb.x,jamb.y+1.48f,jamb.z,
                               0.11f,2.95f,0.10f,jamb.yaw,Color(157,160,145,255));
            }
            const LocalRoutePoint lintel = localRoutePoint(
                route,anchorDistance,nicheDistance,faceLateral);
            addOrientedBox(mesh,lintel.x,lintel.y+2.92f,lintel.z,
                           0.11f,0.10f,1.98f,lintel.yaw,Color(157,160,145,255));

            const LocalRoutePoint sign = localRoutePoint(
                route,anchorDistance,nicheDistance,faceLateral-side*0.025);
            addOrientedBox(worldLuminaires,sign.x,sign.y+3.48f,sign.z,
                           0.09f,0.46f,1.24f,sign.yaw,Color(59,177,112,255));
            const LocalRoutePoint signMark = localRoutePoint(
                route,anchorDistance,nicheDistance,faceLateral-side*0.045);
            addOrientedBox(worldLuminaires,signMark.x,signMark.y+3.48f,signMark.z,
                           0.08f,0.10f,0.72f,signMark.yaw,Color(224,242,216,255));

            const LocalRoutePoint cabinet = localRoutePoint(
                route,anchorDistance,nicheDistance+1.55,faceLateral);
            addOrientedBox(mesh,cabinet.x,cabinet.y+1.18f,cabinet.z,
                           0.20f,0.82f,0.52f,cabinet.yaw,Color(164,42,36,255));
            const LocalRoutePoint handle = localRoutePoint(
                route,anchorDistance,nicheDistance+1.31,faceLateral-side*0.12);
            addOrientedBox(worldLuminaires,handle.x,handle.y+1.18f,handle.z,
                           0.06f,0.08f,0.05f,handle.yaw,Color(241,218,175,255));

            // Every bay carries railway utility text: an escape-route sign giving the
            // direction and distance to the nearer portal, and a hectometre plate.
            const double liningFace = side > 0.0 ? 3.925 : -8.925;
            const float inward = side > 0.0 ? -1.0f : 1.0f;
            if (const auto extent = tunnelExtentAt(nicheDistance))
            {
                addTunnelWallPlate(mesh,tunnelPoint,nicheDistance+2.75,liningFace,inward,
                                   2.20f,0.78f,0.30f,Color(32,112,72,255));
                addRoutePrintedText(stationLettering,route,anchorDistance,
                    tunnelExitLabel(nicheDistance,extent->first,extent->second),
                    nicheDistance+2.75,liningFace+inward*0.06,2.13f,
                    Color(236,246,232,255),0.66f);
                ++counters.staticWorldTunnelMarkerPlates_;
            }
            addTunnelWallPlate(mesh,tunnelPoint,nicheDistance-2.75,liningFace,inward,
                               2.20f,0.46f,0.26f,Color(228,228,218,255));
            addRoutePrintedText(stationLettering,route,anchorDistance,
                tunnelChainageLabel(nicheDistance),nicheDistance-2.75,
                liningFace+inward*0.06,2.14f,Color(28,30,30,255),0.38f);
            ++counters.staticWorldTunnelMarkerPlates_;
        }
    }

    // Tunnel lighting is kept in an unlit batch. A real point-light chain would be wasteful
    // for the long route and is not available through BasicEffect, while bright fixture faces
    // and restrained wall wash panels remain convincing from a fast-moving train. Aligning
    // fixtures to absolute route distance prevents popping or duplicated rows at chunk seams.
    constexpr double kLuminaireSpacing = 10.0;
    double luminaireDistance = std::ceil(chunk.beginDistance / kLuminaireSpacing)
                             * kLuminaireSpacing;
    for (; luminaireDistance < chunk.endDistance;
         luminaireDistance += kLuminaireSpacing)
    {
        if (!route.sample(luminaireDistance).tunnel) continue;
        ++counters.staticWorldTunnelLuminaires_;

        const LocalRoutePoint ceiling = localRoutePoint(
            route, anchorDistance, luminaireDistance, -2.5);
        addOrientedBox(worldLuminaires,ceiling.x,ceiling.y+6.65f,ceiling.z,
                       2.4f,0.08f,0.42f,ceiling.yaw,Color(255,232,170,255));

        // Both running lines share this broad tunnel. Wall-mounted fixtures ensure that a
        // passenger looking squarely through either side window sees depth and speed cues.
        for (const auto& [face,inward] : {std::pair{3.925,-1.0f},std::pair{-8.925,1.0f}})
            addTunnelWallLuminaire(mesh,platformMaterial,worldLuminaires,tunnelPoint,
                                   luminaireDistance,face,inward,tunnelStyle);
    }

    // Sample long crossings at a 20 m cadence rather than quantizing their deck and rails to
    // 250 m stream chunks. This follows visible route curvature and keeps both authored ends
    // close to their actual boundaries while still joining the one static scenery batch.
    for (double bridgeCursor = chunk.beginDistance; bridgeCursor < chunk.endDistance;)
    {
        const double bridgeBegin = bridgeCursor;
        const double nextAbsoluteBoundary =
            (std::floor(bridgeBegin/20.0)+1.0)*20.0;
        const double bridgeEnd = std::min(chunk.endDistance,nextAbsoluteBoundary);
        bridgeCursor = bridgeEnd;
        const double bridgeMiddle = (bridgeBegin + bridgeEnd) * 0.5;
        const Sim::RoutePose bridgePose = route.sample(bridgeMiddle);
        if (!bridgePose.bridge) continue;

        ++counters.staticWorldBridgeSections_;
        if (bridgePose.waterCrossing) ++counters.staticWorldRiverBridgeSections_;
        else ++counters.staticWorldViaductSections_;
        const float bridgeLength = static_cast<float>(bridgeEnd-bridgeBegin)+0.3f;
        const Color deckColor = bridgePose.waterCrossing
            ? Color(82,91,94,255) : Color(112,113,107,255);
        const LocalRoutePoint deck = localRoutePoint(
            route,anchorDistance,bridgeMiddle,-2.5);
        addOrientedBox(mesh,deck.x,deck.y-0.34f,deck.z,
                       11.5f,0.45f,bridgeLength,deck.yaw,deckColor);

        // Two longitudinal edge beams and absolute transverse members make the underside read
        // as a load-bearing structure from oblique windows and from the valley approaches.
        for (double lateral : {-6.45, 1.45})
        {
            const LocalRoutePoint girder = localRoutePoint(
                route,anchorDistance,bridgeMiddle,lateral);
            addOrientedBox(mesh,girder.x,girder.y-0.79f,girder.z,
                           0.46f,0.76f,bridgeLength,girder.yaw,
                           bridgePose.waterCrossing ? Color(48,66,74,255)
                                                    : Color(91,93,90,255));
        }
        double crossBeamDistance = std::ceil(bridgeBegin / 12.5) * 12.5;
        for (; crossBeamDistance < bridgeEnd; crossBeamDistance += 12.5)
        {
            const LocalRoutePoint crossBeam = localRoutePoint(
                route,anchorDistance,crossBeamDistance,-2.5);
            addOrientedBox(mesh,crossBeam.x,crossBeam.y-0.66f,crossBeam.z,
                           10.5f,0.28f,0.34f,crossBeam.yaw,
                           bridgePose.waterCrossing ? Color(55,72,79,255)
                                                    : Color(96,97,92,255));
        }

        const bool beginsCrossing = !route.sample(
            std::max(0.0,bridgeBegin-0.1)).bridge;
        const bool endsCrossing = !route.sample(
            std::min(route.lengthMetres(),bridgeEnd+0.1)).bridge;
        // Both deck edges carry a finished parapet: concrete upstand and coping, cable
        // trough and a slender tubular handrail instead of broad rectangular beams.
        const BridgeParapetStyle parapetStyle = bridgeParapetStyle(bridgePose.waterCrossing);
        const auto parapetPoint = [&](double distance,double lateral) {
            return localRoutePoint(route,anchorDistance,distance,lateral);
        };
        for (const auto& [lateral,outward] : {std::pair{-8.0,-1.0f},std::pair{3.0,1.0f}})
        {
            counters.staticWorldBridgeParapetPosts_ += addBridgeParapet(
                mesh,parapetPoint,bridgeBegin,bridgeEnd,lateral,outward,
                beginsCrossing,endsCrossing,parapetStyle,graphicsQuality_>0);
            if (bridgePose.waterCrossing)
            {
                const LocalRoutePoint fascia = localRoutePoint(
                    route,anchorDistance,bridgeMiddle,lateral+outward*0.06);
                addOrientedBox(mesh,fascia.x,fascia.y-0.58f,fascia.z,
                               0.30f,0.64f,bridgeLength,fascia.yaw,
                               Color(53,72,80,255));
            }
        }

        if (beginsCrossing || endsCrossing)
        {
            ++counters.staticWorldBridgeAbutments_;
            const double boundaryDistance = beginsCrossing ? bridgeBegin : bridgeEnd;
            const Color abutmentColor = bridgePose.waterCrossing
                ? Color(116,119,115,255) : Color(132,130,121,255);
            const LocalRoutePoint breast = localRoutePoint(
                route,anchorDistance,boundaryDistance,-2.5);
            addOrientedBox(mesh,breast.x,breast.y-0.88f,breast.z,
                           11.8f,1.45f,1.25f,breast.yaw,abutmentColor);
            const double landwardDistance = boundaryDistance
                +(beginsCrossing ? -2.6 : 2.6);
            for (double lateral : {-8.45, 3.45})
            {
                const LocalRoutePoint wing = localRoutePoint(
                    route,anchorDistance,landwardDistance,lateral);
                addOrientedBox(mesh,wing.x,wing.y-0.66f,wing.z,
                               1.10f,1.18f,6.2f,wing.yaw,abutmentColor);
            }
            const LocalRoutePoint joint = localRoutePoint(
                route,anchorDistance,boundaryDistance,-2.5);
            addOrientedBox(mesh,joint.x,joint.y-0.085f,joint.z,
                           10.9f,0.035f,0.18f,joint.yaw,Color(47,50,49,255));
        }
    }

    // Piers use an absolute cadence independently of deck segmentation. River spans sit on
    // broad blade piers; the dry valley viaduct uses paired slender shafts and a visible cap.
    double pierDistance = std::ceil(chunk.beginDistance / 55.0) * 55.0;
    for (; pierDistance < chunk.endDistance; pierDistance += 55.0)
    {
        const Sim::RoutePose pierPose = route.sample(pierDistance);
        if (!pierPose.bridge) continue;
        const float groundOffset = terrainOffset(pierPose,pierDistance,-2.5);
        constexpr float kPierTop = -0.72f;
        const float pierHeight = std::max(1.0f,kPierTop-groundOffset);
        const LocalRoutePoint cap = localRoutePoint(
            route,anchorDistance,pierDistance,-2.5);
        addOrientedBox(mesh,cap.x,cap.y-0.68f,cap.z,
                       8.8f,0.56f,1.65f,cap.yaw,
                       pierPose.waterCrossing ? Color(98,105,104,255)
                                               : Color(121,121,113,255));
        if (pierPose.waterCrossing)
        {
            const LocalRoutePoint pier = localRoutePoint(
                route, anchorDistance, pierDistance, -2.5);
            addOrientedBox(mesh,pier.x,pier.y+groundOffset+pierHeight*0.5f,pier.z,
                           6.0f,pierHeight,1.25f,pier.yaw,Color(100,107,106,255));
        }
        else
        {
            // Paired shafts thicken with height, stand on their own footing and are tied
            // together at intervals, so tall valley piers read as load-bearing structure.
            const float shaftWidth = std::clamp(1.15f+0.035f*pierHeight,1.15f,2.4f);
            for (double lateral : {-5.25, 0.25})
            {
                const float shaftGround = std::min(
                    groundOffset,terrainOffset(pierPose,pierDistance,lateral))-0.6f;
                const float shaftHeight = kPierTop-shaftGround;
                const LocalRoutePoint pier = localRoutePoint(
                    route,anchorDistance,pierDistance,lateral);
                addOrientedBox(mesh,pier.x,pier.y+shaftGround+shaftHeight*0.5f,pier.z,
                               shaftWidth,shaftHeight,shaftWidth,pier.yaw,
                               Color(118,118,110,255));
                if (pierHeight > 4.0f)
                    addOrientedBox(mesh,pier.x,pier.y+shaftGround+0.85f,pier.z,
                                   shaftWidth+1.1f,0.9f,shaftWidth+1.1f,pier.yaw,
                                   Color(106,105,98,255));
            }
            const int ties = static_cast<int>(pierHeight/13.0f);
            for (int tie = 1; tie <= ties; ++tie)
            {
                const float height = pierHeight*static_cast<float>(tie)
                                   / static_cast<float>(ties+1);
                const LocalRoutePoint beam = localRoutePoint(
                    route,anchorDistance,pierDistance,-2.5);
                addOrientedBox(mesh,beam.x,beam.y+groundOffset+height,beam.z,
                               5.5f,0.95f,shaftWidth*0.8f,beam.yaw,
                               Color(112,112,104,255));
            }
        }
    }

    if (linesideFenceAllowed(pose))
    {
        // Slim posts and wire strands follow the route chord; strained end posts close the
        // fence where the neighbouring chunk is a bridge, tunnel or urban corridor.
        const bool fenceEndsAtBegin = !linesideFenceAllowed(
            route.sample(std::max(0.0,middle-world.chunkLength())));
        const bool fenceEndsAtEnd = !linesideFenceAllowed(
            route.sample(std::min(route.lengthMetres(),middle+world.chunkLength())));
        const auto fencePoint = [&](double distance,double lateral) {
            return localRoutePoint(route,anchorDistance,distance,lateral);
        };
        for (double side : {-1.0, 1.0})
            counters.staticWorldFencePosts_ += addLinesideFence(
                mesh,fencePoint,chunk.beginDistance,chunk.endDistance,
                side*kFenceLateral,fenceEndsAtBegin,fenceEndsAtEnd);
    }

    if (!pose.bridge && !pose.tunnel && pose.urbanity < 0.55
        && pose.adjacentWaterSide == 0)
    {
        // Clustered grass at the outer drain bank breaks the otherwise immaculate channel edge.
        // Absolute cadence prevents popping patterns at chunk boundaries; Low keeps only one
        // alternating side while higher profiles retain both banks at a tighter spacing.
        const double tuftStep = vergeTuftStep(graphicsQuality_);
        const int bladeCount = vergeBladeCount(graphicsQuality_);
        double tuftDistance = std::ceil(chunk.beginDistance/tuftStep)*tuftStep;
        for (; tuftDistance < chunk.endDistance; tuftDistance += tuftStep)
        {
            const auto tuftIndex = static_cast<std::int64_t>(
                std::llround(tuftDistance/tuftStep));
            for (int sideIndex = 0; sideIndex < 2; ++sideIndex)
            {
                if (graphicsQuality_ == 0 && ((tuftIndex+sideIndex)&1) != 0) continue;
                for (int blade = 0; blade < bladeCount; ++blade)
                {
                    const auto placement=vergeBladePlacement(tuftDistance,sideIndex,graphicsQuality_,blade);
                    const double bladeDistance=placement.distance;
                    if (bladeDistance < chunk.beginDistance || bladeDistance >= chunk.endDistance)
                        continue;
                    const double bladeLateral=placement.lateral;
                    const Sim::RoutePose bladePose = route.sample(bladeDistance);
                    const auto& nearest = route.stations()[route.nearestStation(bladeDistance)];
                    if (bladePose.tunnel || bladePose.bridge || bladePose.urbanity >= 0.55
                        || bladePose.adjacentWaterSide != 0
                        || std::abs(bladeDistance-nearest.distanceMetres) < 240.0
                        || !World::terrainFeatureAllowed(bladePose,bladeLateral)) continue;
                    LocalRoutePoint bladePoint = localRoutePoint(
                        route,anchorDistance,bladeDistance,bladeLateral);
                    bladePoint.y += terrainOffset(bladePose,bladeDistance,bladeLateral);
                    addGrassBlade(mesh,Vector3{bladePoint.x,bladePoint.y,bladePoint.z},
                                   placement.height,placement.width,
                                   bladePoint.yaw+placement.yaw,
                                   blade%3 == 0 ? Color(76,113,53,255)
                                              : Color(86,124,57,255));
                }
                ++counters.staticWorldTracksideVergeTufts_;
            }
        }
    }

    if (graphicsQuality_>=1 && !pose.bridge && !pose.tunnel && pose.adjacentWaterSide == 0)
    {
        // Precast cable troughing on the open cess, wildflower drifts along the inner rural
        // fence line and a steel palisade between urban railway and street. Both use absolute route slots and stop at platforms, structures and
        // level crossings, so they neither pop at chunk seams nor cross a road.
        std::array<double,8> crossings{};
        std::size_t crossingCount=0;
        for (const World::WorldFeature& feature : chunk.features)
            if (feature.kind==World::FeatureKind::LevelCrossing && crossingCount<crossings.size())
                crossings[crossingCount++]=feature.routeDistance;
        const auto clearOfStructures=[&](double distance,double margin,bool rural) {
            const Sim::RoutePose sample=route.sample(distance);
            if (sample.tunnel || sample.bridge || (rural && sample.urbanity>=0.55)
                || sample.adjacentWaterSide!=0) return false;
            const auto& nearest=route.stations()[route.nearestStation(distance)];
            if (std::abs(nearest.distanceMetres-distance)<260.0) return false;
            for (std::size_t i=0;i<crossingCount;++i)
                if (std::abs(crossings[i]-distance)<margin) return false;
            return true;
        };
        for (double unit=std::ceil(chunk.beginDistance/kCableTroughUnitLength)*kCableTroughUnitLength;
             unit<chunk.endDistance;unit+=kCableTroughUnitLength)
        {
            const double unitCentre=unit+kCableTroughUnitLength*0.5;
            if (!clearOfStructures(unitCentre,14.0,false)) continue;
            const LocalRoutePoint troughPoint=localRoutePoint(
                route,anchorDistance,unitCentre,kCableTroughLateral);
            const float troughGround=terrainOffset(route.sample(unitCentre),unitCentre,kCableTroughLateral);
            addCableTroughUnit(mesh,Matrix::CreateRotationY(troughPoint.yaw)
                *Matrix::CreateTranslation(troughPoint.x,troughPoint.y+troughGround,troughPoint.z),
                static_cast<unsigned>(static_cast<std::int64_t>(unit/kCableTroughUnitLength)%17));
            ++counters.staticWorldCableTroughUnits_;
        }
        const double flowerStep=vergeFlowerStep(graphicsQuality_);
        for (double slot=std::ceil(chunk.beginDistance/flowerStep)*flowerStep;
             slot<chunk.endDistance;slot+=flowerStep)
            for (int side : {-1,1})
            {
                const auto placement=vergeFlowerPlacement(slot,side);
                if (placement.distance<chunk.beginDistance || placement.distance>=chunk.endDistance
                    || !clearOfStructures(placement.distance,10.0,true)) continue;
                // The parallel track's ballast reaches the left fence line near stations.
                if (side<0 && std::abs(route.parallelTrackLateralMetres(placement.distance))+3.7
                              >std::abs(placement.lateral)) continue;
                LocalRoutePoint root=localRoutePoint(
                    route,anchorDistance,placement.distance,placement.lateral);
                root.y+=terrainOffset(route.sample(placement.distance),placement.distance,
                                      placement.lateral);
                addVergeFlowerClump(mesh,Vector3{root.x,root.y,root.z},placement);
                ++counters.staticWorldVergeFlowerClumps_;
            }
        if (!linesideFenceAllowed(pose))
        {
            // Absolute 2.75 m slots: a span is drawn by the chunk holding its first post when
            // both of its posts are clear, so runs continue across chunk seams and stop on a
            // post at stations, structures and level crossings.
            const auto urbanBoundary=[&](double distance) {
                return route.sample(distance).urbanity>=0.62 && clearOfStructures(distance,12.0,false);
            };
            const auto palisadePoint=[&](double distance,double lateral) {
                auto p=localRoutePoint(route,anchorDistance,distance,lateral);
                p.y+=terrainOffset(route.sample(distance),distance,lateral);
                return p;
            };
            const bool detailed=graphicsQuality_>=2;
            bool previousOpen=urbanBoundary(std::ceil(chunk.beginDistance/kPalisadePostSpacing)
                *kPalisadePostSpacing-kPalisadePostSpacing);
            for (double post=std::ceil(chunk.beginDistance/kPalisadePostSpacing)*kPalisadePostSpacing;
                 post<chunk.endDistance;post+=kPalisadePostSpacing)
            {
                const bool open=urbanBoundary(post);
                const bool nextOpen=open && urbanBoundary(post+kPalisadePostSpacing);
                if (open && (previousOpen || nextOpen))
                    for (double side : {-1.0,1.0})
                    {
                        addPalisadePost(mesh,palisadePoint,post,side*kPalisadeLateral);
                        ++counters.staticWorldPalisadePosts_;
                        if (nextOpen)
                            addPalisadeSpan(mesh,palisadePoint,post,post+kPalisadePostSpacing,
                                            side*kPalisadeLateral,detailed);
                    }
                previousOpen=open;
            }
        }
    }

    // Field boundaries follow the irregular per-side parcel edges of the surface tint:
    // hedges, gated hedges, hedgerow trees and open margins of differing extent replace the
    // former identical hedge pair at every 500 m.
    for (int side : {-1,1})
    {
        const auto firstParcel=ruralParcelIndex(chunk.beginDistance,side);
        const auto lastParcel=ruralParcelIndex(chunk.endDistance,side)+1;
        for (auto parcel=firstParcel;parcel<=lastParcel;++parcel)
        {
            const double boundaryDistance=ruralParcelBoundary(parcel,side);
            if (boundaryDistance<chunk.beginDistance || boundaryDistance>=chunk.endDistance)
                continue;
            const Sim::RoutePose boundaryPose = route.sample(boundaryDistance);
            bool nearStation = false;
            for (const Sim::Station& station : route.stations())
                nearStation = nearStation
                    || std::abs(station.distanceMetres-boundaryDistance) < 700.0;
            if (nearStation || boundaryPose.tunnel || boundaryPose.bridge
                || boundaryPose.urbanity >= 0.45 || boundaryPose.adjacentWaterSide != 0)
                continue;
            const FieldBoundaryLayout layout=fieldBoundaryLayout(parcel,side);
            if (layout.kind==FieldBoundaryKind::Open) continue;
            const auto addHedgeRun=[&](float inner,float outer,float height,std::uint32_t seed) {
                if (outer-inner<3.0f) return;
                const float centre=(inner+outer)*0.5f;
                addTerrainHedge(graphicsQuality_>=1 ? foliageMaterial : mesh,[&](float x,float z) {
                    auto point=terrainSurfacePoint(boundaryDistance+z,side*static_cast<double>(centre+x));
                    point.Y -= 0.08f; // Bury the base without flattening the sloping crown.
                    return point;
                },outer-inner,1.10f,height,Color(55,92,48,255),seed,
                    graphicsQuality_>=1 ? &leafSprays : nullptr);
                counters.staticWorldFieldBoundarySegments_ +=
                    std::max(1,static_cast<int>(std::lround((outer-inner)/9.6f)));
            };
            const float inner=12.0f;
            if (layout.kind==FieldBoundaryKind::GatedHedge)
            {
                addHedgeRun(inner,layout.gateLateral-2.5f,layout.height,layout.seed);
                addHedgeRun(layout.gateLateral+2.5f,layout.outerLateral,layout.height,layout.seed+7U);
                continue;
            }
            const bool treeLine=layout.kind==FieldBoundaryKind::TreeLine;
            addHedgeRun(inner,layout.outerLateral,treeLine ? layout.height*0.75f : layout.height,
                        layout.seed);
            if (!treeLine) continue;
            // Hedgerow standards: broadleaf trees grown out of the hedge every 18-30 m.
            float lateral=22.0f+static_cast<float>(layout.seed%9U);
            while (lateral<layout.outerLateral-6.0f)
            {
                const double treeLateral=side*static_cast<double>(lateral);
                const auto seed=treeShapeSeed(boundaryDistance,treeLateral,11U);
                lateral+=18.0f+static_cast<float>(seed%13U);
                const Vector3 treeRoot=terrainSurfacePoint(
                    boundaryDistance+0.35*std::sin(static_cast<double>(seed%31U)),treeLateral);
                const float scale=1.05f+static_cast<float>(seed%5U)*0.11f;
                addTaperedTreeTrunk(mesh,treeRoot,scale,3.8f,graphicsQuality_>=1 ? 7 : 5,
                                    Color(88,68,46,255));
                const Color foliage=seed%2U==0U ? Color(46,95,45,255) : Color(52,102,49,255);
                const Color foliageLight=seed%2U==0U ? Color(66,118,56,255) : Color(72,124,60,255);
                if (graphicsQuality_>=1)
                {
                    addClusteredBroadleafCrown(foliageMaterial,treeRoot,scale,seed,foliage,foliageLight,
                                               graphicsQuality_==1);
                    addBroadleafEdgeSprays(leafSprays,treeRoot,scale,seed,foliageLight,
                                           graphicsQuality_==1);
                }
                else
                    mesh.addFoliageLobe(treeRoot+Vector3{0,4.4f*scale,0},
                                        Vector3{1.8f*scale,1.7f*scale,1.7f*scale},6,4,foliage,false);
                ++counters.staticWorldFieldBoundaryTrees_;
            }
        }
    }

    if (graphicsQuality_>=1 && pose.urbanity < 0.45 && !pose.tunnel && !pose.bridge
        && pose.adjacentWaterSide == 0)
    {
        // Tramlines on stubble and ploughed parcels: darker paired wheelings sampled on the
        // terrain surface every 10 m, stopping short of headlands, buildings and crop plots.
        const auto obstructed=[&](double distance,double lateral) {
            for (const World::WorldFeature& feature : chunk.features)
            {
                double halfAlong=0,halfAcross=0;
                switch (feature.kind)
                {
                    case World::FeatureKind::FieldCrop: halfAlong=34; halfAcross=13; break;
                    case World::FeatureKind::House: halfAlong=24; halfAcross=9; break;
                    case World::FeatureKind::Farmstead: halfAlong=22; halfAcross=20; break;
                    case World::FeatureKind::HayBale: halfAlong=4; halfAcross=4; break;
                    case World::FeatureKind::PowerPylon:
                    case World::FeatureKind::Substation: halfAlong=18; halfAcross=18; break;
                    default: continue;
                }
                if (std::abs(feature.routeDistance-distance)<halfAlong*feature.scale
                    && std::abs(feature.lateralMetres-lateral)<halfAcross*feature.scale) return true;
            }
            return false;
        };
        const Color wheeling(98,88,66,255);
        constexpr double kTramlineStep=10.0;
        for (double side : {-1.0,1.0})
            for (const double line : kTramlineLaterals)
            {
                const double centre=side*line;
                for (double d=std::ceil(chunk.beginDistance/kTramlineStep)*kTramlineStep;
                     d<chunk.endDistance;d+=kTramlineStep)
                {
                    const double e=std::min(d+kTramlineStep,chunk.endDistance);
                    const auto parcel=ruralParcelIndex(d,side);
                    if (ruralParcelIndex(e,side)!=parcel || !ruralParcelArable(d,centre)
                        || d-ruralParcelBoundary(parcel,side)<kTramlineHeadland
                        || ruralParcelBoundary(parcel+1,side)-e<kTramlineHeadland
                        || obstructed(d,centre) || obstructed(e,centre)
                        || !World::terrainFeatureAllowed(route.sample(d),centre)) continue;
                    for (const double wheel : {-kTramlineHalfGauge,kTramlineHalfGauge})
                    {
                        const double l0=centre+wheel-kTramlineWheelWidth*0.5;
                        const double l1=centre+wheel+kTramlineWheelWidth*0.5;
                        auto a=terrainSurfacePoint(d,l0),b=terrainSurfacePoint(e,l0);
                        auto c=terrainSurfacePoint(e,l1),dd=terrainSurfacePoint(d,l1);
                        for (auto* vertex : {&a,&b,&c,&dd}) vertex->Y+=0.035f;
                        if (side>0) terrain.addTexturedQuad(a,b,c,dd,wheeling,
                            {static_cast<float>(l0/8),static_cast<float>(d/8)},
                            {static_cast<float>(l0/8),static_cast<float>(e/8)},
                            {static_cast<float>(l1/8),static_cast<float>(e/8)},
                            {static_cast<float>(l1/8),static_cast<float>(d/8)});
                        else terrain.addTexturedQuad(dd,c,b,a,wheeling,
                            {static_cast<float>(l1/8),static_cast<float>(d/8)},
                            {static_cast<float>(l1/8),static_cast<float>(e/8)},
                            {static_cast<float>(l0/8),static_cast<float>(e/8)},
                            {static_cast<float>(l0/8),static_cast<float>(d/8)});
                    }
                    ++counters.staticWorldTramlineSegments_;
                }
            }
    }

    if (pose.urbanity < 0.45 && !pose.tunnel && !pose.bridge)
    {
        // A second, desaturated tree line sits beyond the interactive woodland. Its small
        // quality-scaled population closes bare horizon gaps without retaining more chunks or
        // introducing billboards. Every trunk and crown remains in the static world batch.
        const int treesPerSide = graphicsQuality_ == 0 ? 2
                               : graphicsQuality_ == 1 ? 3 : 4;
        for (int sideIndex = 0; sideIndex < 2; ++sideIndex)
        {
            const double side = sideIndex == 0 ? -1.0 : 1.0;
            for (int tree = 0; tree < treesPerSide; ++tree)
            {
                const double copsePhase = static_cast<double>(chunk.index)*1.618
                    + sideIndex*2.71;
                const double treeAngle = copsePhase+tree*2.399963;
                const double slot = 0.5+0.25*std::sin(copsePhase)
                    + 0.065*std::cos(treeAngle);
                const double treeDistance = chunk.beginDistance
                    + slot*(chunk.endDistance-chunk.beginDistance);
                const Sim::RoutePose treePose = route.sample(treeDistance);
                const auto variant = static_cast<std::uint32_t>(
                    (chunk.index*7+tree*3+sideIndex*11)&7);
                const double lateral = side*(360.0+35.0*std::sin(copsePhase*1.7)
                    + 16.0*std::sin(treeAngle));
                if (treePose.tunnel || treePose.bridge
                    || !World::terrainFeatureAllowed(treePose,lateral))
                    continue;
                LocalRoutePoint treePoint = localRoutePoint(
                    route,anchorDistance,treeDistance,lateral);
                treePoint.y += terrainOffset(treePose,treeDistance,lateral);
                const float scale = 1.45f+static_cast<float>(variant%4U)*0.18f;
                addTaperedTreeTrunk(mesh,
                                   Vector3{treePoint.x,treePoint.y,treePoint.z},
                                   scale,2.8f,5,Color(82,75,59,255));
                if (variant%3U == 0U)
                {
                    counters.staticWorldConiferBranchTiers_ += addLayeredConifer(
                        mesh,Vector3{treePoint.x,treePoint.y,treePoint.z},scale,6,
                        variant,Color(50,82,58,255),Color(59,91,64,255),false);
                }
                else
                {
                    mesh.addEllipsoid(
                        Vector3{treePoint.x,treePoint.y+3.25f*scale,treePoint.z},
                        Vector3{1.72f*scale,1.55f*scale,1.42f*scale},
                        6,3,variant%2U == 0U ? Color(57,91,63,255)
                                              : Color(64,98,67,255));
                }
                ++counters.staticWorldDistantWoodlandTrees_;
            }
        }
    }

    if (pose.urbanity < 0.42 && !pose.tunnel && !pose.bridge
        && pose.adjacentWaterSide == 0)
    {
        // Broad faceted hill masses close long agricultural horizons behind the detailed
        // woodland. They sit well inside the fog range and use progressively lighter,
        // desaturated colours, producing atmospheric depth without a volumetric pass. Low
        // emits one alternating side; Medium emits both; High adds a farther second layer.
        const int ridgeLayers = graphicsQuality_ >= 2 ? 2 : 1;
        for (int layer = 0; layer < ridgeLayers; ++layer)
        {
            for (int sideIndex = 0; sideIndex < 2; ++sideIndex)
            {
                if (graphicsQuality_ == 0
                    && sideIndex != (std::abs(chunk.index)&1)) continue;
                const double side = sideIndex == 0 ? -1.0 : 1.0;
                const int variant = ridgeVariant(chunk.index,sideIndex,layer);
                const double lateral = side*(520.0+static_cast<double>(layer)*175.0
                    + static_cast<double>(variant)*18.0);
                if (!World::terrainFeatureAllowed(pose,lateral)) continue;
                const float height = 21.0f+static_cast<float>(variant)*3.8f
                                   + static_cast<float>(layer)*7.0f;
                const float radius = 112.0f+static_cast<float>(variant)*9.0f
                                   + static_cast<float>(layer)*24.0f;
                const Color ridgeColor = layer == 0
                    ? (variant%2 == 0 ? Color(106,132,123,255)
                                      : Color(112,136,126,255))
                    : (variant%2 == 0 ? Color(126,148,143,255)
                                      : Color(132,152,147,255));
                addGroundedHill(mesh,[&](float along,float across) {
                    return terrainSurfacePoint(middle+along,lateral+across);
                },radius,height,static_cast<unsigned>(chunk.index*13+sideIndex*7+layer),
                    graphicsQuality_>=2 ? 12 : graphicsQuality_==1 ? 8 : 4,ridgeColor);
                ++counters.staticWorldDistantRidgeMasses_;
            }
        }
    }

    if (pose.urbanity >= 0.28 && !pose.tunnel && !pose.bridge)
    {
        // A restrained outer settlement band closes the gap between detailed trackside
        // buildings and the terrain horizon. Muted colours and simple roof silhouettes keep
        // it subordinate to the foreground, while deterministic slots avoid visible popping
        // when a chunk is unloaded and regenerated.
        const int buildingsPerSide = graphicsQuality_ == 0 ? 1
                                   : graphicsQuality_ == 1 ? 2 : 3;
        const std::array<Color,4> distantFacades = {
            Color(132,137,133,255),Color(139,133,124,255),
            Color(126,136,142,255),Color(143,141,132,255)};
        for (int sideIndex = 0; sideIndex < 2; ++sideIndex)
        {
            const double side = sideIndex == 0 ? -1.0 : 1.0;
            for (int building = 0; building < buildingsPerSide; ++building)
            {
                const auto variant = static_cast<std::uint32_t>(
                    (std::abs(chunk.index)*11+building*7+sideIndex*3)&7);
                const double slot = (static_cast<double>(building)+0.34
                    + 0.07*static_cast<double>(variant%3U))
                    / static_cast<double>(buildingsPerSide);
                const double buildingDistance = chunk.beginDistance
                    + slot*(chunk.endDistance-chunk.beginDistance);
                const Sim::RoutePose buildingPose = route.sample(buildingDistance);
                const double lateral = side
                    *(155.0+static_cast<double>(variant%4U)*28.0);
                if (!World::terrainFeatureAllowed(buildingPose,lateral)) continue;
                LocalRoutePoint buildingPoint = localRoutePoint(
                    route,anchorDistance,buildingDistance,lateral);
                buildingPoint.y += terrainOffset(
                    buildingPose,buildingDistance,lateral);
                const float width = 8.0f+static_cast<float>(variant%3U)*2.2f;
                const float depth = 12.0f+static_cast<float>((variant+1U)%3U)*3.0f;
                const float height = 4.8f+static_cast<float>(variant%4U)*1.25f;
                const Color& facade = distantFacades[variant%distantFacades.size()];
                addOrientedBox(mesh,buildingPoint.x,
                               buildingPoint.y+height*0.5f,buildingPoint.z,
                               width,height,depth,buildingPoint.yaw,facade);
                if (variant%3U != 0U)
                    addPitchedRoof(mesh,roofSurface,buildingPoint.x,buildingPoint.y+height,
                                   buildingPoint.z,width+0.45f,depth+0.45f,
                                   1.35f,buildingPoint.yaw,Color(91,91,88,255));
                else
                    addOrientedBox(mesh,buildingPoint.x,buildingPoint.y+height+0.15f,
                                   buildingPoint.z,width+0.35f,0.30f,depth+0.35f,
                                   buildingPoint.yaw,Color(76,82,83,255));
                ++counters.staticWorldDistantSettlementBuildings_;
            }
        }
    }

    if (graphicsQuality_>=1 && pose.urbanity>0.55 && !pose.bridge && !pose.tunnel)
        for (int side : {-1,1})
        {
            const auto pocket=World::streetPocket(chunk.features,side);
            if (!pocket) continue;
            bool clear=true;
            for (const auto& station : route.stations())
                if (std::abs(station.distanceMetres-pocket->distance)<350) clear=false;
            for (const auto& neighbour : world.chunks())
                if (neighbour.index!=chunk.index)
                    for (const auto& f : neighbour.features)
                        if ((std::abs(f.routeDistance-pocket->distance)<40
                            && std::abs(f.lateralMetres-pocket->lateral)<30)
                            || World::streetPocketEntranceConflict(*pocket,f)) clear=false;
            const auto p=route.sample(pocket->distance);
            if (!clear || !World::terrainFeatureAllowed(p,pocket->lateral)) continue;
            const auto pocketSurface=[&](float x,float z) {
                return terrainSurfacePoint(pocket->distance+z,pocket->lateral+static_cast<double>(side)*x);
            };
            const float centre=pocketSurface(0,0).Y;
            for (float x : {-5.4f,4.0f})
                for (float z : {-6.0f,6.0f})
                {
                    if (std::abs(pocketSurface(x,z).Y-centre)>0.25f) clear=false;
                    if (!World::terrainFeatureAllowed(route.sample(pocket->distance+z),
                            pocket->lateral+static_cast<double>(side)*x)) clear=false;
                }
            if (!clear) continue;
            const auto placement=localRoutePoint(route,anchorDistance,pocket->distance,pocket->lateral);
            addStreetPocket(mesh,pocketSurface,placement.yaw,side);
            ++counters.staticWorldUrbanStreetPockets_;
        }

    if (pose.urbanity>0.45 && !pose.tunnel && !pose.bridge)
        for (int side : {-1,1})
            for (const auto& [begin,end] : World::frontageBoundarySpans(chunk.features,side))
                for (double d=begin;d<end;d+=6.0)
                {
                    const double finish=std::min(d+6.0,end),centre=(d+finish)*0.5;
                    const double lateral=static_cast<double>(side)*25.28;
                    if (!World::terrainFeatureAllowed(route.sample(centre),lateral)) continue;
                    auto p=localRoutePoint(route,anchorDistance,centre,lateral);
                    p.y+=terrainOffset(route.sample(centre),centre,lateral);
                    const float span=static_cast<float>(finish-d);
                    const auto firstVertex=facadeMaterial.data().vertices.size();
                    addOrientedBox(facadeMaterial,p.x,p.y+0.37f,p.z,0.20f,0.62f,span,p.yaw,
                                   Color(112,108,98,255));
                    facadeMaterial.projectDetailUVs(firstVertex,{p.x,p.y,p.z},1.2f);
                    addOrientedBox(mesh,p.x,p.y+0.70f,p.z,0.25f,0.06f,span,p.yaw,
                                   Color(147,145,132,255));
                }

    for (const World::WorldFeature& feature : chunk.features)
    {
        if (feature.kind == World::FeatureKind::RoadVehicle
            || feature.kind == World::FeatureKind::OtherTrain) continue;
        lightSeed = treeShapeSeed(feature.routeDistance,feature.lateralMetres,0x4C49U);
        windowOrdinal = 0;
        if ((feature.kind == World::FeatureKind::GrassPatch
             || feature.kind == World::FeatureKind::FieldCrop
             || feature.kind == World::FeatureKind::HayBale
             || feature.kind == World::FeatureKind::Tree
             || feature.kind == World::FeatureKind::Bush)
            && std::abs(feature.routeDistance - anchorDistance)
                > static_cast<double>(std::min(
                    vegetationDistanceMetres_, graphicsQuality_ == 0 ? 400.0f
                                               : graphicsQuality_ == 1 ? 700.0f
                                                                       : 1'000.0f)))
            continue;
        const Sim::RoutePose featurePose = route.sample(feature.routeDistance);
        const bool featureInTunnel = featurePose.tunnel;
        if (featureInTunnel && feature.kind != World::FeatureKind::Signal) continue;
        LocalRoutePoint point = localRoutePoint(
            route, anchorDistance, feature.routeDistance, feature.lateralMetres);
        const bool followsTerrain = feature.kind == World::FeatureKind::GrassPatch
            || feature.kind == World::FeatureKind::FieldCrop
            || feature.kind == World::FeatureKind::HayBale
            || feature.kind == World::FeatureKind::Tree
            || feature.kind == World::FeatureKind::Bush
            || feature.kind == World::FeatureKind::House
            || feature.kind == World::FeatureKind::Townhouse
            || feature.kind == World::FeatureKind::Apartment
            || feature.kind == World::FeatureKind::Tower
            || feature.kind == World::FeatureKind::Warehouse
            || feature.kind == World::FeatureKind::Farmstead
            || feature.kind == World::FeatureKind::PowerPylon
            || feature.kind == World::FeatureKind::Substation;
        if (followsTerrain
            && !World::terrainFeatureAllowed(featurePose,feature.lateralMetres)) continue;
        if (followsTerrain)
            point.y += terrainOffset(featurePose,
                                     feature.routeDistance, feature.lateralMetres);
        const float scale = static_cast<float>(feature.scale);
        const auto roomSeed=[&](double column,int floor) {
            return static_cast<unsigned>(std::llround(feature.routeDistance*10.0))
                ^(feature.variant*37U)
                ^(static_cast<unsigned>(std::llround((column+32.0)*10.0))*11U)
                ^(static_cast<unsigned>(floor)*101U);
        };
        const bool urbanResidential = featurePose.urbanity > 0.45
            && (feature.kind == World::FeatureKind::House
                || feature.kind == World::FeatureKind::Townhouse
                || feature.kind == World::FeatureKind::Apartment
                || feature.kind == World::FeatureKind::Tower);
        // Residential plots have a 64 mm raised top. Shadows at terrain height would be
        // completely occluded by that opaque surface, even with correct blend ordering.
        constexpr float residentialPlotThickness = 0.064f;
        const float contactShadowBase = point.y
            +(urbanResidential ? residentialPlotThickness : 0.0f);

        // A soft two-band footprint cue grounds the largest procedural structures. It is a
        // single transparent streamed batch, not a shadow map, and therefore remains stable
        // on the OpenGL 3.3 renderer while giving façades and yards contact with the terrain.
        if (graphicsQuality_ >= 1)
        {
            float shadowWidth = 0.0f;
            float shadowLength = 0.0f;
            switch (feature.kind)
            {
                case World::FeatureKind::House:
                    shadowWidth = 7.4f; shadowLength = 10.4f; break;
                case World::FeatureKind::Townhouse:
                    shadowWidth = 6.4f; shadowLength = 9.6f; break;
                case World::FeatureKind::Apartment:
                    shadowWidth = 10.5f; shadowLength = 13.5f; break;
                case World::FeatureKind::Tower:
                    shadowWidth = 15.0f; shadowLength = 18.5f; break;
                case World::FeatureKind::Warehouse:
                    shadowWidth = 17.5f; shadowLength = 22.5f; break;
                case World::FeatureKind::Farmstead:
                    shadowWidth = 12.5f; shadowLength = 16.0f; break;
                case World::FeatureKind::Substation:
                    shadowWidth = 15.0f; shadowLength = 20.0f; break;
                default: break;
            }
            if (shadowWidth > 0.0f)
            {
                addOrientedGroundQuad(
                    groundShadows,point.x,contactShadowBase+0.026f,point.z,
                    shadowWidth*scale+0.70f,shadowLength*scale+0.70f,point.yaw,
                    Color(12,17,18,24));
                addOrientedGroundQuad(
                    groundShadows,point.x,contactShadowBase+0.029f,point.z,
                    shadowWidth*scale+0.18f,shadowLength*scale+0.18f,point.yaw,
                    Color(9,13,14,34));
            }
            if (feature.kind == World::FeatureKind::Tree || feature.kind == World::FeatureKind::Bush)
            {
                const bool tree=feature.kind == World::FeatureKind::Tree;
                const auto addFootprint = [&](double shiftX,double shiftZ,float rx,float rz,
                                               const Color& colour) {
                    const auto shadowSurface = [&](float x,float z) {
                        const double distance=feature.routeDistance+shiftZ+z;
                        const double lateral=feature.lateralMetres+shiftX+x;
                        auto p=terrainSurfacePoint(distance,lateral);
                        p.Y += 0.035f;
                        return p;
                    };
                    addSoftVegetationShadow(groundShadows,shadowSurface,rx,rz,colour,feature.variant);
                };
                addFootprint(0,0,(tree ? 1.45f : 1.05f)*scale,
                             (tree ? 1.55f : 0.90f)*scale,Color(10,18,15,40));
                if (tree && std::abs(feature.lateralMetres) < 85.0)
                {
                    const Vector3 rays=sunRays(featurePose.headingRadians);
                    const double height=4.4*scale;
                    addFootprint(height*rays.X/-rays.Y,height*rays.Z/-rays.Y,
                                 2.15f*scale,2.0f*scale,Color(12,20,19,40));
                }
            }
            if (feature.kind == World::FeatureKind::House
                || feature.kind == World::FeatureKind::Townhouse
                || feature.kind == World::FeatureKind::Apartment)
            {
                // A bounded ground-only silhouette supplements contact darkening. The
                // projected roof follows the fixed sunny light direction; it is not a
                // shadow map and cannot cast onto another building or passing vehicle.
                const auto envelope=feature.kind == World::FeatureKind::House
                    ? houseEnvelope(scale,houseStyle(treeShapeSeed(
                          feature.routeDistance,feature.lateralMetres,29U)),
                          static_cast<float>(World::houseLength(
                              World::housePlan(feature.variant,featurePose.urbanity))))
                    : feature.kind == World::FeatureKind::Townhouse
                    ? townhouseEnvelope(scale,feature.variant) : apartmentEnvelope(scale,feature.variant);
                const double height = envelope.roofHeight;
                const Vector3 sunlight = sunRays(featurePose.headingRadians);
                const double dx = height*sunlight.X/-sunlight.Y;
                const double dz = height*sunlight.Z/-sunlight.Y;
                const auto shadowPoint = [&](double lateralOffset, double alongOffset) {
                    const double distance = feature.routeDistance+alongOffset;
                    const double lateral = feature.lateralMetres+lateralOffset;
                    auto p = terrainSurfacePoint(distance,lateral);
                    const bool house=feature.kind==World::FeatureKind::House;
                    const bool apartment=feature.kind==World::FeatureKind::Apartment;
                    const float plotWidth=house ? 10.0f : apartment ? 12.8f : 8.6f;
                    const float plotLength=house ? residentialPlotLength(feature) : apartment
                        ? residentialPlotLength(feature) : 11.4f;
                    if (urbanResidential && std::abs(lateralOffset)<=plotWidth*scale*0.5
                        && std::abs(alongOffset)<=plotLength*scale*0.5)
                        p.Y=std::max(p.Y,point.y+residentialPlotThickness);
                    p.Y+=0.035f;
                    return p;
                };
                addProjectedBoxShadow(groundShadows,envelope.roofWidth,envelope.roofLength,
                    static_cast<float>(dx),static_cast<float>(dz),shadowPoint,Color(12,20,25,54));
            }
        }

        // Residential buildings in dense areas need a local land-use boundary; otherwise
        // every footprint appears to rise directly from one route-wide grey terrain sheet.
        // Houses and townhouses receive a muted planted plot, while larger blocks use a
        // paved forecourt. The thin surfaces remain part of the streamed opaque world mesh.
        if (urbanResidential)
        {
            float plotWidth = 0.0f;
            float plotLength = 0.0f;
            Color plotColour(113,117,111,255);
            switch (feature.kind)
            {
                case World::FeatureKind::House:
                    plotWidth = 10.0f; plotLength = residentialPlotLength(feature);
                    plotColour = feature.variant%2U == 0U
                        ? Color(78,100,70,255) : Color(86,106,76,255);
                    break;
                case World::FeatureKind::Townhouse:
                    plotWidth = 8.6f; plotLength = 11.4f;
                    plotColour = feature.variant%2U == 0U
                        ? Color(91,105,82,255) : Color(99,109,89,255);
                    break;
                case World::FeatureKind::Apartment:
                    plotWidth = 12.8f; plotLength = residentialPlotLength(feature);
                    plotColour = feature.variant%2U == 0U
                        ? Color(125,127,121,255) : Color(132,132,124,255);
                    break;
                case World::FeatureKind::Tower:
                    plotWidth = 17.6f; plotLength = 21.0f;
                    plotColour = feature.variant%2U == 0U
                        ? Color(119,123,120,255) : Color(129,128,120,255);
                    break;
                default: break;
            }
            addOrientedBox(mesh,point.x,point.y+residentialPlotThickness*0.5f,point.z,
                           plotWidth*scale,residentialPlotThickness,plotLength*scale,
                           point.yaw,plotColour);
            // Low planted boundaries give domestic plots a readable edge at window height.
            // Keep the railway-facing frontage open for the existing entrance path; the
            // three remaining edges stay within the already reserved plot footprint.
            if (feature.kind == World::FeatureKind::House
                || feature.kind == World::FeatureKind::Townhouse)
            {
                const float halfWidth = plotWidth*scale*0.5f;
                const float halfLength = plotLength*scale*0.5f;
                const float outerSide = feature.lateralMetres < 0.0 ? -1.0f : 1.0f;
                const Matrix placement = Matrix::CreateRotationY(point.yaw)
                    * Matrix::CreateTranslation(point.x,point.y,point.z);
                const Color masonry(112,103,88,255);
                const Color foliage = feature.variant%2U == 0U
                    ? Color(55,83,48,255) : Color(65,91,54,255);
                for (float end : {-1.0f,1.0f})
                {
                    const float edge = end*(halfLength-0.22f);
                    mesh.addBox(Vector3{-halfWidth,0.064f,edge-0.17f},
                                Vector3{halfWidth,0.29f,edge+0.17f},masonry,placement);
                    mesh.addBox(Vector3{-halfWidth+0.10f,0.27f,edge-0.20f},
                                Vector3{halfWidth-0.10f,0.82f*scale,edge+0.20f},
                                foliage,placement);
                }
                const float edge = outerSide*(halfWidth-0.22f);
                mesh.addBox(Vector3{edge-0.17f,0.064f,-halfLength+0.42f},
                            Vector3{edge+0.17f,0.29f,halfLength-0.42f},masonry,placement);
                mesh.addBox(Vector3{edge-0.20f,0.27f,-halfLength+0.42f},
                            Vector3{edge+0.20f,0.82f*scale,halfLength-0.42f},
                            foliage,placement);
            }
            ++counters.staticWorldUrbanResidentialPlotSurfaces_;
            if (const auto link=World::frontageLink(chunk.features,feature))
            {
                // Shared service passage joins paired parcels; its footprint is also
                // reserved by the generator so no tree can grow through the paving.
                const double side=feature.lateralMetres<0 ? -1.0 : 1.0;
                const auto p=[&](double distance,double across) {
                    auto v=terrainSurfacePoint(distance,side*across);
                    v.Y+=residentialPlotThickness+0.006f;
                    return v;
                };
                const auto a=p(link->begin,link->inner),b=p(link->end,link->inner);
                const auto c=p(link->end,link->outer),d=p(link->begin,link->outer);
                const Color paving(122,125,119,255);
                if (side>0) mesh.addQuad(a,b,c,d,paving);
                else mesh.addQuad(d,c,b,a,paving);
            }
            if (feature.kind==World::FeatureKind::Townhouse && graphicsQuality_>=1
                && std::abs(point.z)<500.0f)
                addTownhouseFrontGarden(mesh,{point.x,point.y,point.z},scale,point.yaw,
                    feature.lateralMetres<0.0 ? -1.0f : 1.0f,feature.variant);
        }

        const auto addUrbanEntrancePath = [&](double doorDistance,
                                               double facadeLateral,
                                               float depth,
                                               float width) {
            if (!urbanResidential) return;
            const double side = feature.lateralMetres < 0.0 ? -1.0 : 1.0;
            const double pathLateral = facadeLateral
                - side*static_cast<double>(depth)*0.5;
            LocalRoutePoint path = localRoutePoint(
                route,anchorDistance,doorDistance,pathLateral);
            path.y = point.y;
            addOrientedBox(mesh,path.x,path.y+0.070f,path.z,
                           depth,0.076f,width,path.yaw,Color(158,157,146,255));
            // Join the apron to the outer pavement, never across the carriageway. Distant
            // skyline buildings are deliberately excluded from this frontage-only path.
            const double apronEdge = std::abs(facadeLateral)-depth;
            constexpr double pavementEdge = 24.6;
            if (apronEdge > pavementEdge && apronEdge < 80.0
                && pose.urbanity > 0.28 && !pose.bridge && !pose.tunnel)
            {
                const double span = apronEdge-pavementEdge;
                const int panels = static_cast<int>(std::ceil(span/2.0));
                const auto pathVertex = [&](double fraction, double along) {
                    const double lateral = side*(pavementEdge+span*fraction);
                    LocalRoutePoint p = localRoutePoint(
                        route,anchorDistance,doorDistance+along,lateral);
                    const auto samplePose = route.sample(doorDistance+along);
                    // Match the street formation and its transition mesh bands, not only
                    // the continuous height function between the sampled vertices.
                    const double magnitude = std::abs(lateral);
                    const double inner = magnitude < 26.0 ? 12.0 : magnitude < 40.0 ? 26.0 : 40.0;
                    const double outer = magnitude < 26.0 ? 26.0 : magnitude < 40.0 ? 40.0 : 100.0;
                    const float innerHeight = terrainOffset(
                        samplePose,doorDistance+along,side*inner);
                    const float outerHeight = terrainOffset(
                        samplePose,doorDistance+along,side*outer);
                    const float stripHeight = innerHeight+(outerHeight-innerHeight)
                        *static_cast<float>((magnitude-inner)/(outer-inner));
                    const float groundY = p.y+std::max(stripHeight,terrainOffset(
                        samplePose,doorDistance+along,lateral));
                    const float pavementY = p.y-0.065f;
                    p.y = pavementY+(point.y+0.108f-pavementY)
                        *static_cast<float>(fraction);
                    p.y = std::max(p.y,groundY+0.08f);
                    return Vector3{p.x,p.y,p.z};
                };
                for (int panel = 0; panel < panels; ++panel)
                {
                    const double f0 = static_cast<double>(panel)/panels;
                    const double f1 = static_cast<double>(panel+1)/panels;
                    const Vector3 a = pathVertex(f0,-width*0.5);
                    const Vector3 b = pathVertex(f0,width*0.5);
                    const Vector3 c = pathVertex(f1,width*0.5);
                    const Vector3 d = pathVertex(f1,-width*0.5);
                    const Color paving = panel%3 == 0
                        ? Color(147,147,137,255) : Color(158,157,146,255);
                    if (graphicsQuality_>=1)
                    {
                        const float u0=static_cast<float>(f0*span)/0.65f;
                        const float u1=static_cast<float>(f1*span)/0.65f;
                        if (side>0)
                            platformMaterial.addTexturedQuad(a,b,c,d,paving,
                                {u0,0},{u0,width/0.65f},{u1,width/0.65f},{u1,0});
                        else
                            platformMaterial.addTexturedQuad(d,c,b,a,paving,
                                {u1,0},{u1,width/0.65f},{u0,width/0.65f},{u0,0});
                    }
                    else if (side > 0.0) mesh.addQuad(a,b,c,d,paving);
                    else mesh.addQuad(d,c,b,a,paving);
                }
                if (graphicsQuality_>=1 && std::abs(point.z)<350.0f)
                    addAccessGarden(mesh,pathVertex,static_cast<float>(span),width);
            }
            const LocalRoutePoint threshold = localRoutePoint(
                route,anchorDistance,doorDistance,
                facadeLateral-side*0.16*static_cast<double>(scale));
            addOrientedBox(mesh,threshold.x,path.y+0.115f,threshold.z,
                           0.42f*scale,0.15f,1.24f*scale,threshold.yaw,
                           Color(133,132,124,255));
            ++counters.staticWorldUrbanResidentialEntrancePaths_;
        };
        switch (feature.kind)
        {
            case World::FeatureKind::GrassPatch:
            {
                if (graphicsQuality_ == 0 && feature.variant % 2U != 0U) break;
                const std::array<Color, 3> grasses = {
                    Color(68,119,54,255), Color(78,128,57,255), Color(91,137,61,255)};
                const Color& grass = grasses[feature.variant % grasses.size()];
                const int patchBlades=graphicsQuality_==0 ? 5 : graphicsQuality_==1 ? 9 : 15;
                const float patchPhase=static_cast<float>(treeShapeSeed(feature.routeDistance,
                    feature.lateralMetres,feature.variant)%97U)*0.0648f;
                for (int blade = 0; blade < patchBlades; ++blade)
                {
                    const float angle=patchPhase+static_cast<float>(blade)*2.399963f;
                    const float reach=0.22f*scale*std::sqrt(static_cast<float>(blade+1)/static_cast<float>(patchBlades));
                    addGrassBlade(mesh,Vector3{point.x+std::cos(angle)*reach,point.y,
                        point.z+std::sin(angle)*reach},
                        (0.21f+0.035f*static_cast<float>(blade%4))*scale,
                        0.032f*scale,point.yaw+angle,grass);
                }
                break;
            }
            case World::FeatureKind::FieldCrop:
            {
                const std::array<Color, 4> crops = {
                    Color(111,139,54,255), Color(139,151,57,255),
                    Color(166,151,62,255), Color(104,132,50,255)};
                const std::array<Color, 4> fieldGrounds = {
                    Color(105,119,57,255), Color(126,119,62,255),
                    Color(142,126,67,255), Color(91,116,57,255)};
                const Color& crop = crops[feature.variant % crops.size()];
                const double halfFieldLength = 30.0 * feature.scale;
                const double halfFieldWidth = 9.0 * feature.scale;
                const double fieldBegin = feature.routeDistance - halfFieldLength;
                const double fieldEnd = feature.routeDistance + halfFieldLength;
                const double innerLateral = feature.lateralMetres - halfFieldWidth;
                const double outerLateral = feature.lateralMetres + halfFieldWidth;
                const Sim::RoutePose beginPose = route.sample(fieldBegin);
                const Sim::RoutePose endPose = route.sample(fieldEnd);
                const bool completeFieldAllowed = fieldBegin >= 0.0
                    && fieldEnd <= route.lengthMetres()
                    && World::terrainFeatureAllowed(beginPose,innerLateral)
                    && World::terrainFeatureAllowed(beginPose,outerLateral)
                    && World::terrainFeatureAllowed(endPose,innerLateral)
                    && World::terrainFeatureAllowed(endPose,outerLateral);
                if (!completeFieldAllowed) break;

                // Use the same cell/chunk breaks and a-c diagonal as the terrain,
                // including portions of a crop parcel spanning its owning chunk edge.
                for (double cursor=fieldBegin;cursor<fieldEnd;)
                {
                    const double cellChunk=std::floor(cursor/world.chunkLength())*world.chunkLength();
                    const double d0=cellChunk+std::floor((cursor-cellChunk)/kTerrainStep)*kTerrainStep;
                    const double d1=std::min({d0+kTerrainStep,cellChunk+world.chunkLength(),route.lengthMetres()});
                    for (std::size_t band=0;band+1<lateralBands.size();++band)
                    {
                        const double l0=lateralBands[band],l1=lateralBands[band+1];
                        if (l1<=innerLateral || l0>=outerLateral) continue;
                        const std::array<Vector3,4> corners={terrainSurfacePoint(d0,l0),
                            terrainSurfacePoint(d1,l0),terrainSurfacePoint(d1,l1),terrainSurfacePoint(d0,l1)};
                        addTerrainCellPatch(mesh,corners,
                            static_cast<float>((cursor-d0)/(d1-d0)),
                            static_cast<float>((std::min(fieldEnd,d1)-d0)/(d1-d0)),
                            static_cast<float>((std::max(innerLateral,l0)-l0)/(l1-l0)),
                            static_cast<float>((std::min(outerLateral,l1)-l0)/(l1-l0)),
                            fieldGrounds[feature.variant%fieldGrounds.size()]);
                    }
                    cursor=d1;
                }

                const int rowRadius = graphicsQuality_ == 0 ? 3
                                    : graphicsQuality_ == 1 ? 5 : 7;
                for (int row = -rowRadius; row <= rowRadius; ++row)
                {
                    const double rowLateral = feature.lateralMetres
                        + static_cast<double>(row) * halfFieldWidth
                            / static_cast<double>(rowRadius);
                    addTerrainCropRow(mesh,[&](float x,float z) {
                        return terrainSurfacePoint(feature.routeDistance-x,rowLateral+z);
                    },static_cast<float>(halfFieldLength*2),0.48f*scale,0.26f*scale,crop,
                        treeShapeSeed(feature.routeDistance,rowLateral,feature.variant));
                }

                // Each edge is one continuous profile sampled on the actual terrain mesh.
                // Alternating variants leave some parcels open.
                if (feature.variant % 2U == 0U)
                {
                    const Color hedge(47,91,43,255);
                    for (double edgeLateral : {innerLateral,outerLateral})
                        addRouteAlignedHedge(graphicsQuality_>=1 ? foliageMaterial : mesh,terrainSurfacePoint,feature.routeDistance,
                            edgeLateral,static_cast<float>(halfFieldLength*2),0.62f*scale,
                            0.76f*scale,true,hedge,treeShapeSeed(feature.routeDistance,edgeLateral,0),
                            graphicsQuality_>=1 ? &leafSprays : nullptr);
                    for (double endDistance : {fieldBegin,fieldEnd})
                        addRouteAlignedHedge(graphicsQuality_>=1 ? foliageMaterial : mesh,terrainSurfacePoint,endDistance,
                            feature.lateralMetres,static_cast<float>(halfFieldWidth*2),0.62f*scale,
                            0.76f*scale,false,hedge,treeShapeSeed(endDistance,feature.lateralMetres,0),
                            graphicsQuality_>=1 ? &leafSprays : nullptr);
                }
                break;
            }
            case World::FeatureKind::HayBale:
            {
                const std::array<Color,4> straw = {
                    Color(186,151,66,255),Color(202,166,73,255),
                    Color(174,139,58,255),Color(194,157,61,255)};
                const Color& bale = straw[feature.variant%straw.size()];
                const float yaw = point.yaw
                    + (static_cast<float>(feature.variant)-1.5f)*0.12f;
                addOrientedBox(mesh,point.x,point.y+0.40f*scale,point.z,
                               2.45f*scale,0.80f*scale,1.28f*scale,yaw,bale);
                addOrientedBox(mesh,point.x,point.y+1.18f*scale,point.z,
                               2.18f*scale,0.76f*scale,1.18f*scale,yaw,
                               Color(211,174,76,255));
                for (float strap : {-0.57f,0.57f})
                    addOrientedBox(mesh,point.x+std::sin(yaw)*strap*scale,
                                   point.y+1.20f*scale,
                                   point.z+std::cos(yaw)*strap*scale,
                                   2.24f*scale,0.055f*scale,0.065f*scale,yaw,
                                   Color(88,78,55,255));
                ++counters.staticWorldHayBaleStacks_;
                break;
            }
            case World::FeatureKind::Tree:
            {
                const bool distant = std::abs(feature.lateralMetres) > 85.0;
                if (graphicsQuality_ == 0 && distant && feature.variant % 2U != 0U) break;
                const int sides = distant ? 6 : 9;
                addTaperedTreeTrunk(mesh,Vector3{point.x,point.y,point.z},scale,
                                   3.8f,sides,Color(91,69,45,255));
                const std::array<Color,4> foliagePalette = {
                    Color(40,82,43,255), Color(47,99,47,255),
                    Color(55,108,51,255), Color(49,92,43,255)};
                const std::array<Color,4> highlightPalette = {
                    Color(61,105,54,255), Color(66,119,56,255),
                    Color(73,127,61,255), Color(67,112,53,255)};
                const Color& foliage = foliagePalette[feature.variant%foliagePalette.size()];
                const Color& foliageLight =
                    highlightPalette[feature.variant%highlightPalette.size()];
                if (feature.variant == 0U || feature.variant == 3U)
                {
                    counters.staticWorldConiferBranchTiers_ += addLayeredConifer(
                        (!distant && graphicsQuality_>=1) ? needleMaterial : mesh,
                        Vector3{point.x,point.y,point.z},scale,sides,
                        treeShapeSeed(feature.routeDistance,feature.lateralMetres,feature.variant),
                        foliage,foliageLight,!distant,
                        !distant && graphicsQuality_>=1 ? &needleSprays : nullptr);
                }
                else
                {
                    if (!distant)
                    {
                        // Three low-poly limbs connect the trunk to the irregular crown.
                        // They are rotated boxes rooted at the trunk, so the silhouette gains
                        // believable branching without a separate mesh or cylinder transform.
                        for (int branch = 0; branch < 3; ++branch)
                        {
                            const float branchIndex = static_cast<float>(branch);
                            const float azimuth = point.yaw
                                + branchIndex*2.0944f
                                + static_cast<float>(feature.variant)*0.19f;
                            const float branchLength = (1.35f+0.14f*branchIndex)*scale;
                            const float tilt = 0.58f+0.07f*branchIndex;
                            const Matrix branchTransform =
                                Matrix::CreateScale(0.13f*scale,branchLength,0.13f*scale)
                                * Matrix::CreateRotationZ(tilt)
                                * Matrix::CreateRotationY(azimuth)
                                * Matrix::CreateTranslation(
                                    point.x,point.y+(2.15f+0.18f*branchIndex)*scale,point.z);
                            mesh.addBox(Vector3{-0.5f,0.0f,-0.5f},
                                        Vector3{0.5f,1.0f,0.5f},
                                        Color(87,66,44,255),branchTransform);
                            ++counters.staticWorldTreeBranchCues_;
                        }
                    }
                    if (!distant && graphicsQuality_ >= 1)
                    {
                        addClusteredBroadleafCrown(foliageMaterial,{point.x,point.y,point.z},scale,
                            treeShapeSeed(feature.routeDistance,feature.lateralMetres,feature.variant),
                            foliage,foliageLight,graphicsQuality_ == 1);
                        addBroadleafEdgeSprays(leafSprays,{point.x,point.y,point.z},scale,
                            treeShapeSeed(feature.routeDistance,feature.lateralMetres,feature.variant),
                            foliageLight,graphicsQuality_ == 1);
                    }
                    else
                    {
                    // Distant crowns also need continuous shading: their large flat facets
                    // remain conspicuous through windows. Keep the existing LOD topology;
                    // smooth normals and local canopy colour need no additional triangles.
                    const bool smoothCrown = graphicsQuality_ >= 1;
                    mesh.addFoliageLobe(Vector3{point.x,point.y+4.45f*scale,point.z},
                                      Vector3{1.82f*scale,1.72f*scale,1.72f*scale},
                                      sides,4,foliage,smoothCrown);
                    mesh.addFoliageLobe(Vector3{point.x-1.00f*scale,point.y+4.15f*scale,
                                              point.z+0.38f*scale},
                                      Vector3{1.22f*scale,1.25f*scale,1.18f*scale},
                                      sides,4,foliageLight,smoothCrown);
                    mesh.addFoliageLobe(Vector3{point.x+0.94f*scale,point.y+4.20f*scale,
                                              point.z-0.34f*scale},
                                      Vector3{1.25f*scale,1.30f*scale,1.20f*scale},
                                      sides,4,foliage,smoothCrown);
                    if (!distant)
                    {
                        mesh.addFoliageLobe(Vector3{point.x-0.34f*scale,
                                                   point.y+5.28f*scale,
                                                   point.z-0.62f*scale},
                                          Vector3{1.12f*scale,1.04f*scale,1.08f*scale},
                                          sides,4,foliageLight,smoothCrown);
                        mesh.addFoliageLobe(Vector3{point.x+0.36f*scale,
                                                   point.y+3.76f*scale,
                                                   point.z+0.92f*scale},
                                          Vector3{1.08f*scale,1.10f*scale,1.02f*scale},
                                          sides,4,foliage,smoothCrown);
                    }
                    }
                }
                if (!distant && graphicsQuality_ >= 1)
                {
                    // Mature railway-edge trees rarely stand on a perfectly clipped lawn.
                    // Add a bounded, deterministic outward-side understorey and occasional
                    // younger tree around the existing feature. It shares the streamed batch,
                    // follows the route terrain, and never consumes extra simulation objects.
                    const double outward = feature.lateralMetres < 0.0 ? -1.0 : 1.0;
                    const int shrubCount = graphicsQuality_ >= 2 ? 3 : 2;
                    int placed = 0;
                    for (int shrubIndex = 0; shrubIndex < shrubCount; ++shrubIndex)
                    {
                        const double alternating = ((feature.variant+static_cast<std::uint32_t>(
                            shrubIndex))&1U) == 0U ? -1.0 : 1.0;
                        const double shrubDistance = std::clamp(
                            feature.routeDistance+alternating*(1.05+0.72*shrubIndex)*scale,
                            0.0,route.lengthMetres());
                        const double shrubLateral = feature.lateralMetres
                            + outward*(1.20+0.78*shrubIndex)*scale;
                        const Sim::RoutePose shrubPose = route.sample(shrubDistance);
                        if (!World::terrainFeatureAllowed(shrubPose,shrubLateral)) continue;
                        LocalRoutePoint shrubPoint = localRoutePoint(
                            route,anchorDistance,shrubDistance,shrubLateral);
                        shrubPoint.y += terrainOffset(
                            shrubPose,shrubDistance,shrubLateral);
                        const float shrubScale = scale*(0.30f+0.055f
                            *static_cast<float>(shrubIndex));
                        const Color shrubColor = shrubIndex%2 == 0
                            ? Color(50,99+static_cast<int>(feature.variant)*4,46,255)
                            : Color(61,112+static_cast<int>(feature.variant)*3,51,255);
                        addLeafyShrub(foliageMaterial,leafSprays,
                            {shrubPoint.x,shrubPoint.y,shrubPoint.z},shrubScale,
                            treeShapeSeed(shrubDistance,shrubLateral,feature.variant),shrubColor);
                        addOrientedGroundQuad(
                            groundShadows,shrubPoint.x,shrubPoint.y+0.028f,shrubPoint.z,
                            1.30f*shrubScale,1.12f*shrubScale,shrubPoint.yaw,
                            Color(10,15,12,25));
                        ++placed;
                    }
                    if (graphicsQuality_ >= 2 && feature.variant%2U == 0U)
                    {
                        const double saplingDistance = std::clamp(
                            feature.routeDistance+(feature.variant%4U < 2U ? -2.6 : 2.6)
                                *static_cast<double>(scale),
                            0.0,route.lengthMetres());
                        const double saplingLateral = feature.lateralMetres
                            + outward*3.45*static_cast<double>(scale);
                        const Sim::RoutePose saplingPose = route.sample(saplingDistance);
                        if (World::terrainFeatureAllowed(saplingPose,saplingLateral))
                        {
                            LocalRoutePoint sapling = localRoutePoint(
                                route,anchorDistance,saplingDistance,saplingLateral);
                            sapling.y += terrainOffset(
                                saplingPose,saplingDistance,saplingLateral);
                            const float saplingScale = 0.46f*scale;
                            addTaperedTreeTrunk(mesh,
                                Vector3{sapling.x,sapling.y,sapling.z},saplingScale,
                                3.0f,6,Color(88,68,46,255));
                            mesh.addEllipsoid(
                                Vector3{sapling.x,sapling.y+3.15f*saplingScale,sapling.z},
                                Vector3{1.32f*saplingScale,1.45f*saplingScale,
                                        1.25f*saplingScale},
                                7,4,foliageLight);
                            addOrientedGroundQuad(
                                groundShadows,sapling.x,sapling.y+0.028f,sapling.z,
                                1.55f*saplingScale,1.75f*saplingScale,sapling.yaw,
                                Color(9,14,11,28));
                            ++placed;
                        }
                    }
                    if (placed > 0) ++counters.staticWorldUnderstoreyGroups_;
                }
                break;
            }
            case World::FeatureKind::Bush:
            {
                const Color shrub(48,105 + static_cast<int>(feature.variant)*8,48,255);
                if (graphicsQuality_>=1)
                {
                    addLeafyShrub(foliageMaterial,leafSprays,{point.x,point.y,point.z},scale,
                        treeShapeSeed(feature.routeDistance,feature.lateralMetres,feature.variant),shrub);
                    addSoftVegetationShadow(groundShadows,[&](float x,float z) {
                        return Vector3{point.x+x,point.y+0.026f,point.z+z};
                    },0.94f*scale,0.72f*scale,Color(10,15,12,28),feature.variant);
                    break;
                }
                addOrientedBox(mesh,point.x+0.20f*scale,point.y+0.010f,
                               point.z+0.34f*scale,0.95f*scale,0.020f,1.25f*scale,
                               point.yaw-0.24f,Color(55,87,47,255));
                mesh.addEllipsoid(Vector3{point.x,point.y+0.62f*scale,point.z},
                                  Vector3{0.78f*scale,0.62f*scale,0.72f*scale},
                                  7,3,shrub);
                mesh.addEllipsoid(Vector3{point.x+0.52f*scale,point.y+0.48f*scale,
                                           point.z+0.30f*scale},
                                  Vector3{0.58f*scale,0.48f*scale,0.55f*scale},
                                  7,3,shrub);
                if (graphicsQuality_ >= 1)
                    mesh.addEllipsoid(Vector3{point.x-0.46f*scale,point.y+0.44f*scale,
                                               point.z-0.22f*scale},
                                      Vector3{0.54f*scale,0.46f*scale,0.50f*scale},
                                      7,3,Color(58,112 + static_cast<int>(
                                          feature.variant)*6,51,255));
                break;
            }
            case World::FeatureKind::House:
            {
                const HouseStyle style=houseStyle(treeShapeSeed(
                    feature.routeDistance,feature.lateralMetres,29U));
                const World::HousePlan plan=World::housePlan(feature.variant,featurePose.urbanity);
                const float houseLength=static_cast<float>(World::houseLength(plan));
                const auto envelope=houseEnvelope(scale,style,houseLength);
                addFacadeMaterial(renderedFacadeMaterial,point,
                                  feature.lateralMetres > 0.0,envelope.width,0.0f,
                                  envelope.wallHeight,envelope.length,style.facade);
                const bool closeHouse=graphicsQuality_>=1 && std::abs(point.z)<450;
                if (plan==World::HousePlan::Cottage)
                    addHippedRoof(mesh,roofSurface,point.x,point.y+envelope.wallHeight,point.z,
                                  envelope.roofWidth,envelope.roofLength,
                                  envelope.roofHeight-envelope.wallHeight,point.yaw,style.roof);
                else
                    addPitchedRoof(mesh,roofSurface,point.x,point.y+envelope.wallHeight,point.z,
                                   envelope.roofWidth,envelope.roofLength,
                                   envelope.roofHeight-envelope.wallHeight,point.yaw,
                                   style.roof,closeHouse ? envelope.width : 0.0f,
                                   closeHouse ? envelope.wallHeight : 0.0f);
                if (plan==World::HousePlan::RearWing)
                {
                    // A one-storey kitchen or workshop wing behind the house, its ridge running
                    // away from the railway, offset towards one gable.
                    const double away=std::copysign(1.0,feature.lateralMetres);
                    const double wingAlong=(houseLength*0.5-World::kHouseRearWingLength*0.5-0.6)
                        *((feature.variant&2U)!=0U ? 1.0 : -1.0)*static_cast<double>(scale);
                    const double wingLateral=feature.lateralMetres
                        +away*(3.5+World::kHouseRearWingDepth*0.5-0.1)*static_cast<double>(scale);
                    LocalRoutePoint wing=localRoutePoint(route,anchorDistance,
                        feature.routeDistance+wingAlong,wingLateral);
                    wing.y=point.y;
                    const float wingWall=std::min(envelope.wallHeight,3.2f*scale);
                    const float wingDepth=static_cast<float>(World::kHouseRearWingDepth)*scale;
                    const float wingLength=static_cast<float>(World::kHouseRearWingLength)*scale;
                    addFacadeMaterial(renderedFacadeMaterial,wing,feature.lateralMetres>0.0,
                                      wingDepth,0.0f,wingWall,wingLength,style.facade);
                    addPitchedRoof(mesh,roofSurface,wing.x,wing.y+wingWall,wing.z,
                                   wingLength+0.5f*scale,wingDepth+0.4f*scale,1.9f*scale*style.pitch,
                                   wing.yaw+1.570796327f,style.roof);
                    ++counters.staticWorldHouseRearWings_;
                }
                const Matrix houseTransform = Matrix::CreateRotationY(point.yaw)
                                            * Matrix::CreateTranslation(
                                                point.x,point.y,point.z);
                // Chimney stacks: ridge-side, either hand, or a pair at both gable ends.
                const float stackTop=envelope.roofHeight+0.55f*scale;
                const auto addChimney=[&](float localX,float localZ) {
                    const float base=envelope.wallHeight+0.6f*scale;
                    const Vector3 stack=Vector3::Transform(
                        Vector3{localX,(base+stackTop)*0.5f,localZ},houseTransform);
                    addOrientedBox(mesh,stack.X,stack.Y,stack.Z,0.58f*scale,stackTop-base,
                                   0.72f*scale,point.yaw,Color(126,99,78,255));
                    addOrientedBox(mesh,stack.X,stack.Y+(stackTop-base)*0.5f+0.07f*scale,stack.Z,
                                   0.72f*scale,0.15f*scale,0.86f*scale,point.yaw,
                                   Color(79,69,61,255));
                };
                if (plan==World::HousePlan::Cottage) addChimney(0.0f,0.0f);
                else if (style.chimneys==1) addChimney(0.35f*scale,1.85f*scale);
                else if (style.chimneys==2) addChimney(-0.35f*scale,-1.85f*scale);
                else if (style.chimneys==3)
                    for (float end : {-1.0f,1.0f}) addChimney(0.0f,end*(houseLength*0.5f-0.45f)*scale);
                const double facade = feature.lateralMetres
                    - std::copysign(3.55 * static_cast<double>(scale), feature.lateralMetres);
                const std::array<float,2> floors={style.bungalow ? 1.65f : 2.0f,4.25f};
                const int floorCount=style.bungalow ? 1 : 2;
                // The door sits on the bay nearest its style position; a cottage keeps it central.
                const auto bays=houseBays(houseLength);
                const double doorOffset=plan==World::HousePlan::Cottage ? 0.0
                    : std::clamp(2.8*static_cast<double>(style.doorBay),
                                 static_cast<double>(bays.front()),static_cast<double>(bays.back()));
                for (const float bay : bays)
                {
                    const double zOffset=bay;
                    LocalRoutePoint window = localRoutePoint(
                        route, anchorDistance,
                        feature.routeDistance + zOffset * static_cast<double>(scale), facade);
                    window.y = point.y;
                    for (int floor=0;floor<floorCount;++floor)
                    {
                        const float floorHeight=floors[static_cast<std::size_t>(floor)];
                        // The ground-floor door bay belongs to the entrance, not a window.
                        if (floor==0 && std::abs(zOffset-doorOffset)<1.0) continue;
                        addFacadeWindow(window,floorHeight*scale,
                                        (style.bungalow ? 1.05f : 0.78f)*scale,1.15f*scale,
                                        roomSeed(zOffset,static_cast<int>(floorHeight*4)));
                    }
                }
                const double doorDistance=feature.routeDistance
                    +doorOffset*static_cast<double>(scale);
                LocalRoutePoint door = localRoutePoint(route, anchorDistance, doorDistance, facade);
                door.y = point.y;
                addOrientedBox(mesh,door.x,door.y+0.95f*scale,door.z,
                               0.12f,1.9f*scale,0.95f*scale,door.yaw,
                               Color(91,64,45,255));
                if (graphicsQuality_>=1 && std::abs(point.z)<250.0f)
                    addResidentialDoorKit(mesh,0.95f*scale,1.9f*scale,
                        Matrix::CreateRotationY(door.yaw+1.570796327f)
                            *Matrix::CreateTranslation(door.x,door.y,door.z));
                if (style.porch)
                {
                    // A shallow lean-to canopy on two slim posts shelters the entrance.
                    const double outward=-std::copysign(0.75*static_cast<double>(scale),
                                                        feature.lateralMetres);
                    const LocalRoutePoint canopy=localRoutePoint(
                        route,anchorDistance,doorDistance,facade+outward);
                    addOrientedBox(mesh,canopy.x,point.y+2.35f*scale,canopy.z,1.55f*scale,
                                   0.10f*scale,1.85f*scale,canopy.yaw,style.roof);
                    for (double post : {-0.82,0.82})
                    {
                        const LocalRoutePoint postPoint=localRoutePoint(route,anchorDistance,
                            doorDistance+post*static_cast<double>(scale),facade+outward*1.85);
                        addOrientedBox(mesh,postPoint.x,point.y+1.15f*scale,postPoint.z,
                                       0.09f*scale,2.3f*scale,0.09f*scale,postPoint.yaw,
                                       Color(214,210,200,255));
                    }
                }
                addUrbanEntrancePath(doorDistance,facade,2.6f*scale,1.30f*scale);
                for (double end : {-1.0,1.0})
                    for (double lateralOffset : {-1.65,1.65})
                    {
                        LocalRoutePoint endWindow = localRoutePoint(
                            route,anchorDistance,
                            feature.routeDistance+end*(houseLength*0.5+0.05)*static_cast<double>(scale),
                            feature.lateralMetres
                                +lateralOffset*static_cast<double>(scale));
                        endWindow.y = point.y;
                        for (int floor=0;floor<floorCount;++floor)
                        {
                            const float floorHeight=floors[static_cast<std::size_t>(floor)];
                            addEndWallWindow(endWindow,floorHeight*scale,
                                            0.82f*scale,1.10f*scale,
                                            roomSeed(lateralOffset+end*8,static_cast<int>(floorHeight*4)));
                        }
                    }
                if (World::houseHasOutbuilding(feature.variant,featurePose.urbanity))
                {
                    const double outbuildingDistance=feature.routeDistance
                        +World::houseOutbuildingDirection(feature.variant)
                            *World::kHouseOutbuildingOffset*static_cast<double>(scale);
                    LocalRoutePoint shed=localRoutePoint(route,anchorDistance,outbuildingDistance,
                                                         feature.lateralMetres);
                    shed.y+=terrainOffset(route.sample(outbuildingDistance),outbuildingDistance,
                                          feature.lateralMetres);
                    // Local +X points away from the railway on either side.
                    const float awayYaw=feature.lateralMetres>0.0 ? shed.yaw : shed.yaw+3.14159265f;
                    addRuralOutbuilding(mesh,mesh,Matrix::CreateRotationY(awayYaw)
                        *Matrix::CreateTranslation(shed.x,shed.y,shed.z),scale,style.barn,
                        style.facade,style.roof);
                    ++counters.staticWorldHouseOutbuildings_;
                }
                break;
            }
            case World::FeatureKind::Townhouse:
            {
                const std::array<Color, 4> facades = {
                    Color(192,166,139,255), Color(174,181,172,255),
                    Color(184,154,145,255), Color(201,190,158,255)};
                const auto envelope=townhouseEnvelope(scale,feature.variant);
                const auto storeys=townhouseStoreys(scale,feature.variant);
                const float height = envelope.wallHeight;
                const Color& facadeColor = facades[feature.variant % facades.size()];
                addFacadeMaterial(facadeMaterial,point,feature.lateralMetres > 0.0,
                                  envelope.width,0.0f,height,envelope.length,facadeColor);
                addOrientedBox(mesh,point.x,point.y+(height+envelope.roofHeight)*0.5f,point.z,
                               envelope.roofWidth,envelope.roofHeight-height,envelope.roofLength,point.yaw,
                               Color(91,83,75,255));
                if (graphicsQuality_>=1 && std::abs(point.z)<450.0f)
                    addFlatRoofFinish(mesh,terrain,point.x,point.y+envelope.roofHeight,point.z,
                        envelope.roofWidth,envelope.roofLength,point.yaw,feature.variant);
                addOrientedBox(mesh,point.x,point.y+0.28f*scale,point.z,
                               6.15f*scale,0.56f*scale,9.35f*scale,point.yaw,
                               Color(104,94,83,255));
                const double facade = feature.lateralMetres
                    - std::copysign(3.05 * static_cast<double>(scale), feature.lateralMetres);
                constexpr std::array<double, 3> windowOffsets = {-2.65, 0.0, 2.65};
                for (std::size_t column = 0; column < windowOffsets.size(); ++column)
                {
                    LocalRoutePoint window = localRoutePoint(
                        route,anchorDistance,
                        feature.routeDistance + windowOffsets[column]
                            * static_cast<double>(scale),facade);
                    window.y = point.y;
                    for (int floor = 0; floor < storeys.count; ++floor)
                    {
                        addFacadeWindow(
                            window,storeys.windowCentre(floor),
                            storeys.windowHeight,0.92f*scale,roomSeed(static_cast<double>(column),floor));
                        if (floor == 1
                            && (column + static_cast<std::size_t>(feature.variant)) % 3U == 0U)
                            addWindowPlanter(mesh,{window.x,window.y,window.z},
                                storeys.windowCentre(floor)-storeys.windowHeight*0.5f-0.055f-0.0375f,
                                scale,window.yaw,feature.lateralMetres>0 ? -1.0f : 1.0f);
                    }
                }
                const double doorOffset = feature.variant % 2U == 0U ? -1.55 : 1.55;
                LocalRoutePoint door = localRoutePoint(
                    route,anchorDistance,
                    feature.routeDistance + doorOffset * static_cast<double>(scale),facade);
                door.y = point.y;
                addOrientedBox(mesh,door.x,door.y+1.12f*scale,door.z,
                               0.13f,2.24f*scale,1.15f*scale,door.yaw,
                               Color(72,62,52,255));
                if (graphicsQuality_>=1 && std::abs(point.z)<250.0f)
                    addResidentialDoorKit(mesh,1.15f*scale,2.24f*scale,
                        Matrix::CreateRotationY(door.yaw+1.570796327f)
                            *Matrix::CreateTranslation(door.x,door.y,door.z));
                addUrbanEntrancePath(
                    feature.routeDistance+doorOffset*static_cast<double>(scale),
                    facade,2.35f*scale,1.42f*scale);
                const LocalRoutePoint rooftop = localRoutePoint(
                    route,anchorDistance,
                    feature.routeDistance + 2.6 * static_cast<double>(scale),
                    feature.lateralMetres);
                addOrientedBox(mesh,rooftop.x,point.y+height+0.70f*scale,rooftop.z,
                               0.68f*scale,1.40f*scale,0.78f*scale,rooftop.yaw,
                               Color(82,77,70,255));
                for (double end : {-1.0,1.0})
                    for (double lateralOffset : {-1.55,1.55})
                    {
                        LocalRoutePoint endWindow = localRoutePoint(
                            route,anchorDistance,
                            feature.routeDistance+end*4.65*static_cast<double>(scale),
                            feature.lateralMetres
                                +lateralOffset*static_cast<double>(scale));
                        endWindow.y = point.y;
                        for (int floor = 0; floor < storeys.count; ++floor)
                            addEndWallWindow(
                                endWindow,storeys.windowCentre(floor),
                                0.82f*scale,storeys.windowHeight,roomSeed(lateralOffset+end*8,floor));
                    }
                break;
            }
            case World::FeatureKind::Apartment:
            {
                const BlockStyle style=blockStyle(treeShapeSeed(
                    feature.routeDistance,feature.lateralMetres,31U));
                const auto envelope=apartmentEnvelope(scale,feature.variant);
                const float height = envelope.wallHeight;
                const int floors = apartmentFloorCount(feature.variant);
                const Color& facadeColor = style.facade;
                const unsigned planBits=(feature.variant>>3U)&3U;
                const bool terrace=planBits==1U,setback=planBits==2U,stepped=planBits==3U;
                // Stepped blocks drop one storey over the half towards `lowSide` along the route.
                const double lowSide=(feature.variant&1U)!=0U ? 1.0 : -1.0;
                const float storey=2.35f*scale;
                const auto addBlockMass=[&](double alongCentre,float length,float wall) {
                    LocalRoutePoint mass=localRoutePoint(route,anchorDistance,
                        feature.routeDistance+alongCentre,feature.lateralMetres);
                    mass.y=point.y;
                    addFacadeMaterial(renderedFacadeMaterial,mass,feature.lateralMetres > 0.0,
                                      envelope.width,0.0f,wall,length,facadeColor);
                    addOrientedBox(mesh,mass.x,mass.y+wall+0.25f,mass.z,envelope.roofWidth,
                                   envelope.roofHeight-height,length+0.5f,mass.yaw,Color(72,77,77,255));
                    if (graphicsQuality_>=1 && std::abs(mass.z)<450.0f)
                        addFlatRoofFinish(mesh,terrain,mass.x,mass.y+wall+(envelope.roofHeight-height),
                            mass.z,envelope.roofWidth,length+0.5f,mass.yaw,feature.variant);
                };
                if (stepped)
                {
                    const double quarter=static_cast<double>(envelope.length)*0.25;
                    addBlockMass(-lowSide*quarter,envelope.length*0.5f,height);
                    addBlockMass(lowSide*quarter,envelope.length*0.5f,height-storey);
                }
                else addBlockMass(0.0,envelope.length,height);
                const double facade = feature.lateralMetres
                    - std::copysign(5.05 * static_cast<double>(scale), feature.lateralMetres);
                const std::vector<double> bays=terrace
                    ? std::vector<double>{-7.0,-4.2,-1.4,1.4,4.2,7.0}
                    : std::vector<double>{-4.2,-1.4,1.4,4.2};
                // A storey is missing over the lower half of a stepped block.
                const auto storeyBuilt=[&](int floor,double along) {
                    return !(stepped && floor==floors-1 && along*lowSide>0.0);
                };
                if (setback)
                {
                    // Set-back penthouse: a glazed top storey behind a roof terrace railing.
                    const double away=std::copysign(1.0,feature.lateralMetres);
                    const float penthouseWidth=envelope.width-3.4f*scale;
                    LocalRoutePoint penthouse=localRoutePoint(route,anchorDistance,feature.routeDistance,
                        feature.lateralMetres+away*1.5*static_cast<double>(scale));
                    penthouse.y=point.y+envelope.roofHeight;
                    addFacadeMaterial(renderedFacadeMaterial,penthouse,feature.lateralMetres>0.0,
                                      penthouseWidth,0.0f,2.8f*scale,envelope.length-4.0f*scale,facadeColor);
                    addOrientedBox(mesh,penthouse.x,penthouse.y+2.9f*scale,penthouse.z,
                                   penthouseWidth+0.5f,0.2f*scale,envelope.length-3.5f*scale,
                                   penthouse.yaw,Color(72,77,77,255));
                    const LocalRoutePoint glazing=localRoutePoint(route,anchorDistance,feature.routeDistance,
                        feature.lateralMetres+away*1.5*static_cast<double>(scale)
                            -away*static_cast<double>(penthouseWidth)*0.5-away*0.03);
                    addOrientedBox(mesh,glazing.x,penthouse.y+1.45f*scale,glazing.z,0.06f,1.9f*scale,
                                   envelope.length-5.0f*scale,glazing.yaw,Color(58,86,98,255));
                    const LocalRoutePoint railing=localRoutePoint(route,anchorDistance,feature.routeDistance,
                        facade-away*0.05);
                    addOrientedBox(mesh,railing.x,penthouse.y+0.5f*scale,railing.z,0.06f,1.0f*scale,
                                   envelope.length,railing.yaw,style.railing);
                    ++counters.staticWorldBlockPenthouses_;
                }
                const double apartmentDoorOffset = feature.variant%2U == 0U
                    ? -0.62*static_cast<double>(scale)
                    : 0.62*static_cast<double>(scale);
                for (int floor = 0; floor < floors; ++floor)
                    for (const double zOffset : bays)
                    {
                        if (floor==0 && std::abs(zOffset*scale-apartmentDoorOffset)<1.26*scale)
                            continue;
                        if (!storeyBuilt(floor,zOffset)) continue;
                        LocalRoutePoint window = localRoutePoint(
                            route, anchorDistance,
                            feature.routeDistance + zOffset * static_cast<double>(scale), facade);
                        window.y = point.y;
                        addFacadeWindow(
                            window,(2.0f+static_cast<float>(floor)*2.35f)*scale,
                            0.85f*scale,0.90f*scale,roomSeed(zOffset,floor));
                    }
                {
                    // Plinth skin and first-floor string course on the railway facade, then
                    // projecting balconies with railings on the chosen bay rhythm.
                    const double towardTrack=-std::copysign(1.0,feature.lateralMetres);
                    const LocalRoutePoint skin=localRoutePoint(route,anchorDistance,
                        feature.routeDistance,facade+towardTrack*0.03);
                    addOrientedBox(mesh,skin.x,point.y+0.55f*scale,skin.z,0.06f,1.1f*scale,
                                   envelope.length+0.06f,skin.yaw,style.plinth);
                    addOrientedBox(mesh,skin.x,point.y+3.22f*scale,skin.z,0.08f,0.16f*scale,
                                   envelope.length+0.08f,skin.yaw,style.plinth);
                    const bool closeBlock=graphicsQuality_>=1 && std::abs(point.z)<500.0f;
                    for (int floor = 1; closeBlock && floor < floors; ++floor)
                        for (const double bay : bays)
                        {
                            if (!blockBayHasBalcony(style.balconyPattern,bay)
                                || !storeyBuilt(floor,bay)) continue;
                            const float slabY=point.y+(0.95f+static_cast<float>(floor)*2.35f)*scale;
                            const LocalRoutePoint slab=localRoutePoint(route,anchorDistance,
                                feature.routeDistance+bay*static_cast<double>(scale),
                                facade+towardTrack*0.55*static_cast<double>(scale));
                            addOrientedBox(mesh,slab.x,slabY,slab.z,1.1f*scale,0.14f*scale,
                                           2.15f*scale,slab.yaw,Color(150,148,142,255));
                            const LocalRoutePoint front=localRoutePoint(route,anchorDistance,
                                feature.routeDistance+bay*static_cast<double>(scale),
                                facade+towardTrack*1.07*static_cast<double>(scale));
                            addOrientedBox(mesh,front.x,slabY+0.52f*scale,front.z,0.05f,
                                           0.9f*scale,2.15f*scale,front.yaw,style.railing);
                            addOrientedBox(mesh,front.x,slabY+1.0f*scale,front.z,0.08f,
                                           0.06f*scale,2.2f*scale,front.yaw,Color(70,74,76,255));
                            ++counters.staticWorldBlockBalconies_;
                        }
                }
                LocalRoutePoint apartmentDoor = localRoutePoint(
                    route,anchorDistance,
                    feature.routeDistance+apartmentDoorOffset,facade);
                apartmentDoor.y = point.y;
                addOrientedBox(mesh,apartmentDoor.x,
                               apartmentDoor.y+1.18f*scale,apartmentDoor.z,
                               0.13f,2.36f*scale,1.42f*scale,
                               apartmentDoor.yaw,Color(52,78,84,255));
                if (graphicsQuality_>=1 && std::abs(point.z)<250.0f)
                    addResidentialDoorKit(mesh,1.42f*scale,2.36f*scale,
                        Matrix::CreateRotationY(apartmentDoor.yaw+1.570796327f)
                            *Matrix::CreateTranslation(apartmentDoor.x,apartmentDoor.y,apartmentDoor.z),true);
                addOrientedBox(mesh,apartmentDoor.x,
                               apartmentDoor.y+2.41f*scale,apartmentDoor.z,
                               0.18f,0.13f*scale,1.72f*scale,
                               apartmentDoor.yaw,Color(91,96,93,255));
                addUrbanEntrancePath(
                    feature.routeDistance+apartmentDoorOffset,facade,
                    3.1f*scale,1.62f*scale);
                for (double end : {-1.0,1.0})
                    for (double lateralOffset : {-2.55,2.55})
                    {
                        LocalRoutePoint endWindow = localRoutePoint(
                            route,anchorDistance,
                            feature.routeDistance+end*(envelope.length/scale*0.5+0.05)
                                *static_cast<double>(scale),
                            feature.lateralMetres
                                +lateralOffset*static_cast<double>(scale));
                        endWindow.y = point.y;
                        for (int floor = 0; floor < floors; ++floor)
                            if (storeyBuilt(floor,end))
                            addEndWallWindow(
                                endWindow,(2.0f+static_cast<float>(floor)*2.35f)*scale,
                                0.90f*scale,0.90f*scale,roomSeed(lateralOffset+end*8,floor));
                    }
                break;
            }
            case World::FeatureKind::Tower:
            {
                const std::array<Color,6> facades = {
                    Color(133,144,148,255),Color(176,166,150,255),
                    Color(196,192,182,255),Color(161,156,145,255),
                    Color(170,132,112,255),Color(146,160,152,255)};
                const Color& facadeColor = facades[treeShapeSeed(
                    feature.routeDistance,feature.lateralMetres,37U)%facades.size()];
                const float unscaledBodyHeight = 24.0f
                    + static_cast<float>(feature.variant%4U)*4.2f;
                const float bodyHeight = unscaledBodyHeight*scale;
                addOrientedBox(mesh,point.x,point.y+2.1f*scale,point.z,
                               14.5f*scale,4.2f*scale,18.0f*scale,point.yaw,
                               Color(112,116,114,255));
                addOrientedBox(mesh,point.x,point.y+4.2f*scale+bodyHeight*0.5f,point.z,
                               11.5f*scale,bodyHeight,14.0f*scale,point.yaw,facadeColor);
                addOrientedBox(mesh,point.x,point.y+4.2f*scale+bodyHeight+2.4f*scale,
                               point.z,8.2f*scale,4.8f*scale,9.2f*scale,point.yaw,
                               facadeColor);
                addOrientedBox(mesh,point.x,point.y+4.2f*scale+bodyHeight+4.95f*scale,
                               point.z,8.6f*scale,0.30f*scale,9.6f*scale,point.yaw,
                               Color(68,75,76,255));
                const double facade = feature.lateralMetres
                    - std::copysign(5.80*static_cast<double>(scale),
                                    feature.lateralMetres);
                const int floors = 8+static_cast<int>(feature.variant%4U);
                {
                    // Spandrel bands every floor on the railway face break the sheer slab.
                    const LocalRoutePoint band=localRoutePoint(route,anchorDistance,
                        feature.routeDistance,facade-std::copysign(0.03,feature.lateralMetres));
                    const Color spandrel(static_cast<int>(facadeColor.getRProperty())*82/100,
                                         static_cast<int>(facadeColor.getGProperty())*82/100,
                                         static_cast<int>(facadeColor.getBProperty())*82/100,255);
                    for (int floor = 0; floor <= floors; ++floor)
                        addOrientedBox(mesh,band.x,point.y+(5.3f+static_cast<float>(floor)*2.65f)*scale,
                                       band.z,0.07f,0.32f*scale,14.05f*scale,band.yaw,spandrel);
                }
                for (int floor = 0; floor < floors; ++floor)
                    for (double zOffset : {-4.5,-1.5,1.5,4.5})
                    {
                        const LocalRoutePoint window = localRoutePoint(
                            route,anchorDistance,
                            feature.routeDistance+zOffset*static_cast<double>(scale),
                            facade);
                        addOrientedBox(mesh,window.x,
                                       point.y+(6.1f+static_cast<float>(floor)*2.65f)*scale,
                                       window.z,0.11f,1.05f*scale,1.05f*scale,window.yaw,
                                       (floor+static_cast<int>(feature.variant))%3 == 0
                                           ? Color(115,151,163,255)
                                           : Color(49,78,91,255));
                    }

                const double towerEntranceFacade = feature.lateralMetres
                    - std::copysign(7.30*static_cast<double>(scale),
                                    feature.lateralMetres);
                LocalRoutePoint towerDoor = localRoutePoint(
                    route,anchorDistance,feature.routeDistance,towerEntranceFacade);
                towerDoor.y = point.y;
                addOrientedBox(mesh,towerDoor.x,towerDoor.y+1.38f*scale,towerDoor.z,
                               0.14f,2.76f*scale,2.45f*scale,towerDoor.yaw,
                               Color(46,76,86,255));
                addOrientedBox(mesh,towerDoor.x,towerDoor.y+2.85f*scale,towerDoor.z,
                               0.20f,0.18f*scale,2.85f*scale,towerDoor.yaw,
                               Color(78,85,84,255));
                addUrbanEntrancePath(feature.routeDistance,towerEntranceFacade,
                                     3.6f*scale,2.65f*scale);

                // Towers sit behind the foreground street edge and are usually read at a
                // shallow angle from a moving carriage. Both route-facing ends therefore
                // need the same bounded glazing rhythm as the near facade; otherwise one
                // blank slab can dominate an entire city-window view.
                for (double end : {-1.0,1.0})
                {
                    for (double lateralOffset : {-3.5,0.0,3.5})
                    {
                        LocalRoutePoint endWindow = localRoutePoint(
                            route,anchorDistance,
                            feature.routeDistance+end*7.05*static_cast<double>(scale),
                            feature.lateralMetres
                                +lateralOffset*static_cast<double>(scale));
                        endWindow.y = point.y;
                        for (int floor = 0; floor < floors; ++floor)
                        {
                            addEndWallWindow(
                                endWindow,
                                (6.1f+static_cast<float>(floor)*2.65f)*scale,
                                0.98f*scale,1.05f*scale);
                            ++counters.staticWorldTowerEndWallWindows_;
                        }
                    }
                    for (double lateralOffset : {-2.25,2.25})
                    {
                        LocalRoutePoint upperWindow = localRoutePoint(
                            route,anchorDistance,
                            feature.routeDistance+end*4.65*static_cast<double>(scale),
                            feature.lateralMetres
                                +lateralOffset*static_cast<double>(scale));
                        upperWindow.y = point.y;
                        addEndWallWindow(
                            upperWindow,(6.55f+unscaledBodyHeight)*scale,
                            1.02f*scale,1.08f*scale);
                        ++counters.staticWorldTowerEndWallWindows_;
                    }
                }
                const LocalRoutePoint roofPlant = localRoutePoint(
                    route,anchorDistance,feature.routeDistance,
                    feature.lateralMetres+2.0*static_cast<double>(scale));
                addOrientedBox(mesh,roofPlant.x,
                               point.y+4.2f*scale+bodyHeight+5.7f*scale,
                               roofPlant.z,2.8f*scale,1.2f*scale,3.2f*scale,
                               roofPlant.yaw,Color(82,88,87,255));
                break;
            }
            case World::FeatureKind::Warehouse:
            {
                const double side = feature.lateralMetres < 0.0 ? -1.0 : 1.0;
                // Light profiled cladding: in the cool sky ambient the earlier mid-grey
                // tones read as unlit charcoal silhouettes on every shaded face (ENV-132).
                const std::array<Color,4> industrialWalls = {
                    Color(170,178,180,255),Color(186,180,164,255),
                    Color(156,176,170,255),Color(194,190,180,255)};
                const std::array<Color,4> industrialShutters = {
                    Color(82,94,98,255),Color(92,91,84,255),
                    Color(73,96,95,255),Color(91,96,94,255)};
                const std::array<Color,4> industrialBands = {
                    Color(66,103,116,255),Color(108,83,63,255),
                    Color(63,105,99,255),Color(94,91,74,255)};
                const Color& wall = industrialWalls[
                    feature.variant%industrialWalls.size()];
                const Color& shutter = industrialShutters[
                    feature.variant%industrialShutters.size()];

                if ((feature.variant&World::kWarehouseCompoundLeader) != 0U)
                {
                    const int buildingCount =
                        (feature.variant&World::kWarehouseThreeBuildingCompound) != 0U
                            ? 3 : 2;
                    const double spacing = 30.0*static_cast<double>(scale);
                    const double compoundCentreDistance = feature.routeDistance
                        + 0.5*static_cast<double>(buildingCount-1)*spacing;
                    const double innerYardLateral = feature.lateralMetres
                        - side*14.7*static_cast<double>(scale);
                    const float compoundLength = static_cast<float>(
                        26.0*static_cast<double>(scale)
                        + static_cast<double>(buildingCount-1)*spacing);
                    // One hardstanding, a gated front fence and marked staff parking make the
                    // group read as a shared industrial premises rather than neighbouring
                    // random boxes. The ground slab is split along the curve to avoid a long
                    // rigid chord on tighter approaches.
                    for (int yardSection = 0; yardSection < buildingCount; ++yardSection)
                    {
                        const double yardDistance = feature.routeDistance
                            + static_cast<double>(yardSection)*spacing;
                        LocalRoutePoint section = localRoutePoint(
                            route,anchorDistance,yardDistance,innerYardLateral);
                        section.y = point.y;
                        addOrientedBox(mesh,section.x,section.y+0.052f,section.z,
                                       12.0f*scale,0.104f,29.5f*scale,section.yaw,
                                       Color(91,94,90,255));
                    }

                    const double frontFenceLateral = feature.lateralMetres
                        - side*21.0*static_cast<double>(scale);
                    const double compoundBegin = compoundCentreDistance
                        - 0.5*static_cast<double>(compoundLength);
                    const double compoundEnd = compoundCentreDistance
                        + 0.5*static_cast<double>(compoundLength);
                    constexpr double gateHalfWidth = 5.2;
                    for (const std::array<double,2>& fenceSpan :
                         {std::array<double,2>{compoundBegin,
                                               compoundCentreDistance-gateHalfWidth},
                          std::array<double,2>{compoundCentreDistance+gateHalfWidth,
                                               compoundEnd}})
                    {
                        const double fenceLength = fenceSpan[1]-fenceSpan[0];
                        if (fenceLength <= 0.0) continue;
                        LocalRoutePoint rail = localRoutePoint(
                            route,anchorDistance,0.5*(fenceSpan[0]+fenceSpan[1]),
                            frontFenceLateral);
                        rail.y = point.y;
                        for (float railHeight : {0.72f,1.42f})
                            addOrientedBox(mesh,rail.x,rail.y+railHeight*scale,rail.z,
                                           0.09f,0.08f*scale,
                                           static_cast<float>(fenceLength),rail.yaw,
                                           Color(91,101,100,255));
                    }
                    const int fencePosts = 5+buildingCount*4;
                    for (int post = 0; post < fencePosts; ++post)
                    {
                        const double t = static_cast<double>(post)
                            / static_cast<double>(fencePosts-1);
                        const double postDistance = std::lerp(
                            compoundBegin,compoundEnd,t);
                        if (std::abs(postDistance-compoundCentreDistance)
                            < gateHalfWidth-0.9) continue;
                        LocalRoutePoint fencePost = localRoutePoint(
                            route,anchorDistance,postDistance,frontFenceLateral);
                        fencePost.y = point.y;
                        addOrientedBox(mesh,fencePost.x,
                                       fencePost.y+0.78f*scale,fencePost.z,
                                       0.14f*scale,1.56f*scale,0.14f*scale,
                                       fencePost.yaw,Color(72,82,82,255));
                    }
                    for (double gatePostOffset : {-gateHalfWidth,gateHalfWidth})
                    {
                        LocalRoutePoint gatePost = localRoutePoint(
                            route,anchorDistance,
                            compoundCentreDistance+gatePostOffset,frontFenceLateral);
                        gatePost.y = point.y;
                        addOrientedBox(mesh,gatePost.x,gatePost.y+0.90f*scale,
                                       gatePost.z,0.24f*scale,1.80f*scale,
                                       0.24f*scale,gatePost.yaw,
                                       industrialBands[feature.variant
                                           % industrialBands.size()]);
                    }

                    // Two compact planted buffers soften the long hardstanding/fence edge
                    // without turning the service yard into a park. They are kept inside the
                    // premises, clear of the entrance and vehicle paths. Low planting beds,
                    // deliberately irregular shrub lobes and a restrained ground shadow all
                    // remain in the existing streamed static submissions.
                    const double plantingLateral = frontFenceLateral
                        + side*1.45*static_cast<double>(scale);
                    for (double plantingDirection : {-1.0,1.0})
                    {
                        const double plantingDistance = compoundCentreDistance
                            + plantingDirection*0.38
                                *static_cast<double>(compoundLength);
                        LocalRoutePoint bed = localRoutePoint(
                            route,anchorDistance,plantingDistance,plantingLateral);
                        bed.y = point.y;
                        addOrientedBox(mesh,bed.x,bed.y+0.085f*scale,bed.z,
                                       1.42f*scale,0.17f*scale,5.15f*scale,
                                       bed.yaw,Color(112,112,100,255));
                        addOrientedBox(mesh,bed.x,bed.y+0.145f*scale,bed.z,
                                       1.16f*scale,0.12f*scale,4.82f*scale,
                                       bed.yaw,Color(61,55,42,255));
                        addOrientedGroundQuad(
                            groundShadows,bed.x,bed.y+0.030f,bed.z,
                            1.75f*scale,5.45f*scale,bed.yaw,
                            Color(9,14,10,27));
                        for (int shrub = -1; shrub <= 1; ++shrub)
                        {
                            const double shrubDistance = plantingDistance
                                + static_cast<double>(shrub)*1.48
                                    *static_cast<double>(scale);
                            LocalRoutePoint shrubPoint = localRoutePoint(
                                route,anchorDistance,shrubDistance,plantingLateral);
                            shrubPoint.y = point.y;
                            const float heightScale = 0.88f
                                + 0.07f*static_cast<float>(
                                    (feature.variant+static_cast<std::uint32_t>(shrub+1))
                                    % 3U);
                            const Color shrubColour = shrub == 0
                                ? Color(54,99,48,255)
                                : Color(63,111,53,255);
                            mesh.addEllipsoid(
                                Vector3{shrubPoint.x,
                                        shrubPoint.y+(0.62f*heightScale+0.17f)*scale,
                                        shrubPoint.z},
                                Vector3{0.59f*scale,0.62f*heightScale*scale,
                                        0.68f*scale},
                                7,3,shrubColour);
                            if (graphicsQuality_ >= 2)
                                mesh.addEllipsoid(
                                    Vector3{shrubPoint.x+0.20f*scale,
                                            shrubPoint.y+(0.64f*heightScale+0.17f)*scale,
                                            shrubPoint.z-0.22f*scale},
                                    Vector3{0.37f*scale,0.40f*heightScale*scale,
                                            0.43f*scale},
                                    7,3,Color(72,124,59,255));
                        }
                        ++counters.staticWorldIndustrialEdgePlantingGroups_;
                    }

                    const double parkingLateral = innerYardLateral
                        - side*3.5*static_cast<double>(scale);
                    for (int marking = -2; marking <= 2; ++marking)
                    {
                        const double parkingDistance = compoundCentreDistance
                            + static_cast<double>(marking)*3.2
                                * static_cast<double>(scale);
                        LocalRoutePoint line = localRoutePoint(
                            route,anchorDistance,parkingDistance,parkingLateral);
                        line.y = point.y;
                        addOrientedBox(mesh,line.x,line.y+0.112f,line.z,
                                       4.8f*scale,0.025f,0.075f*scale,line.yaw,
                                       Color(205,205,187,215));
                    }
                    for (double palletOffset : {-0.32,0.34})
                    {
                        const double palletDistance = compoundCentreDistance
                            + palletOffset*static_cast<double>(compoundLength);
                        const double palletLateral = feature.lateralMetres
                            - side*9.9*static_cast<double>(scale);
                        LocalRoutePoint pallet = localRoutePoint(
                            route,anchorDistance,palletDistance,palletLateral);
                        pallet.y = point.y;
                        addOrientedBox(mesh,pallet.x,pallet.y+0.18f*scale,pallet.z,
                                       1.55f*scale,0.36f*scale,2.2f*scale,
                                       pallet.yaw,Color(123,91,57,255));
                        addOrientedBox(mesh,pallet.x,pallet.y+0.53f*scale,pallet.z,
                                       1.35f*scale,0.34f*scale,1.95f*scale,
                                       pallet.yaw,Color(149,133,91,255));
                    }

                    // A single rigid delivery truck gives the loading apron an active use
                    // without introducing another moving-traffic actor or draw submission.
                    // It is backed squarely onto the centre shutter of the leader building.
                    const std::array<Color,4> deliveryCabs = {
                        Color(54,102,119,255),Color(126,79,55,255),
                        Color(58,111,101,255),Color(103,101,70,255)};
                    const double truckDistance = feature.routeDistance;
                    const double boxLateral = feature.lateralMetres
                        - side*11.35*static_cast<double>(scale);
                    const double cabLateral = feature.lateralMetres
                        - side*15.0*static_cast<double>(scale);
                    const double chassisLateral = feature.lateralMetres
                        - side*12.75*static_cast<double>(scale);
                    auto groundedPoint = [&](double distance, double lateral) {
                        LocalRoutePoint vehiclePoint = localRoutePoint(
                            route,anchorDistance,distance,lateral);
                        vehiclePoint.y = point.y;
                        return vehiclePoint;
                    };
                    const LocalRoutePoint chassis = groundedPoint(
                        truckDistance,chassisLateral);
                    addOrientedBox(mesh,chassis.x,chassis.y+0.58f*scale,chassis.z,
                                   7.9f*scale,0.30f*scale,2.48f*scale,chassis.yaw,
                                   Color(48,52,52,255));
                    const LocalRoutePoint cargoBox = groundedPoint(
                        truckDistance,boxLateral);
                    addOrientedBox(mesh,cargoBox.x,cargoBox.y+2.12f*scale,
                                   cargoBox.z,4.8f*scale,3.18f*scale,2.42f*scale,
                                   cargoBox.yaw,Color(203,205,196,255));
                    addOrientedBox(mesh,cargoBox.x,cargoBox.y+1.28f*scale,
                                   cargoBox.z,4.84f*scale,0.34f*scale,2.46f*scale,
                                   cargoBox.yaw,deliveryCabs[
                                       feature.variant%deliveryCabs.size()]);
                    const LocalRoutePoint cab = groundedPoint(truckDistance,cabLateral);
                    addOrientedBox(mesh,cab.x,cab.y+1.35f*scale,cab.z,
                                   2.25f*scale,2.34f*scale,2.35f*scale,cab.yaw,
                                   deliveryCabs[feature.variant%deliveryCabs.size()]);
                    const LocalRoutePoint windscreen = groundedPoint(
                        truckDistance,cabLateral-side*1.14*static_cast<double>(scale));
                    addOrientedBox(mesh,windscreen.x,windscreen.y+1.77f*scale,
                                   windscreen.z,0.055f,0.78f*scale,1.62f*scale,
                                   windscreen.yaw,Color(44,72,81,255));
                    for (double axleLateral : {boxLateral, cabLateral})
                        for (double routeOffset : {-1.18,1.18})
                        {
                            const LocalRoutePoint wheel = groundedPoint(
                                truckDistance+routeOffset*static_cast<double>(scale),
                                axleLateral);
                            addOrientedBox(mesh,wheel.x,wheel.y+0.42f*scale,wheel.z,
                                           0.48f*scale,0.72f*scale,0.30f*scale,
                                           wheel.yaw,Color(28,31,31,255));
                        }
                    for (double routeOffset : {-0.67,0.67})
                    {
                        const LocalRoutePoint lamp = groundedPoint(
                            truckDistance+routeOffset*static_cast<double>(scale),
                            cabLateral-side*1.18*static_cast<double>(scale));
                        addOrientedBox(mesh,lamp.x,lamp.y+0.86f*scale,lamp.z,
                                       0.06f,0.20f*scale,0.28f*scale,lamp.yaw,
                                       Color(244,225,160,255));
                    }
                    ++counters.staticWorldIndustrialYardVehicles_;

                    if ((feature.variant&World::kWarehouseThreeBuildingCompound) != 0U)
                    {
                        // Larger premises receive one compact service van in a marked bay.
                        // It remains part of the streamed static batch: the extra activity cue
                        // costs geometry, but no actor, physics object, or draw submission.
                        const double vanDistance = compoundCentreDistance
                            + 6.0*static_cast<double>(scale);
                        const double vanLateral = parkingLateral;
                        const Color vanBody = deliveryCabs[
                            (feature.variant+1U)%deliveryCabs.size()];
                        const LocalRoutePoint van = groundedPoint(
                            vanDistance,vanLateral);
                        addOrientedBox(mesh,van.x,van.y+0.86f*scale,van.z,
                                       1.92f*scale,1.42f*scale,4.45f*scale,
                                       van.yaw,vanBody);
                        addOrientedBox(mesh,van.x,van.y+1.58f*scale,van.z,
                                       1.84f*scale,0.22f*scale,3.62f*scale,
                                       van.yaw,Color(222,220,207,255));
                        for (double end : {-1.0,1.0})
                        {
                            const LocalRoutePoint vanGlass = groundedPoint(
                                vanDistance+end*2.24*static_cast<double>(scale),
                                vanLateral);
                            addOrientedBox(mesh,vanGlass.x,vanGlass.y+1.34f*scale,
                                           vanGlass.z,1.48f*scale,0.58f*scale,
                                           0.055f,vanGlass.yaw,
                                           Color(43,72,82,255));
                        }
                        const double visibleVanSide = vanLateral
                            - side*0.98*static_cast<double>(scale);
                        for (double windowOffset : {-0.78,0.86})
                        {
                            const LocalRoutePoint sideGlass = groundedPoint(
                                vanDistance+windowOffset*static_cast<double>(scale),
                                visibleVanSide);
                            addOrientedBox(mesh,sideGlass.x,
                                           sideGlass.y+1.34f*scale,sideGlass.z,
                                           0.055f,0.57f*scale,1.18f*scale,
                                           sideGlass.yaw,Color(43,72,82,255));
                        }
                        for (double axle : {-1.38,1.38})
                            for (double wheelSide : {-0.84,0.84})
                            {
                                const LocalRoutePoint vanWheel = groundedPoint(
                                    vanDistance+axle*static_cast<double>(scale),
                                    vanLateral+wheelSide*static_cast<double>(scale));
                                addOrientedBox(mesh,vanWheel.x,
                                               vanWheel.y+0.36f*scale,vanWheel.z,
                                               0.30f*scale,0.60f*scale,
                                               0.34f*scale,vanWheel.yaw,
                                               Color(27,30,30,255));
                            }
                        ++counters.staticWorldIndustrialYardVehicles_;

                        // A detached box trailer occupies a second loading bay. Its rear-door
                        // ribs, tandem axle and landing legs make the larger compound feel in
                        // use without adding a moving road actor or another draw submission.
                        const double trailerDistance = feature.routeDistance+spacing;
                        const double trailerLateral = feature.lateralMetres
                            - side*12.85*static_cast<double>(scale);
                        const LocalRoutePoint trailer = groundedPoint(
                            trailerDistance,trailerLateral);
                        addOrientedBox(mesh,trailer.x,trailer.y+0.62f*scale,trailer.z,
                                       6.70f*scale,0.28f*scale,2.46f*scale,
                                       trailer.yaw,Color(53,57,57,255));
                        addOrientedBox(mesh,trailer.x,trailer.y+2.18f*scale,trailer.z,
                                       5.85f*scale,3.18f*scale,2.38f*scale,
                                       trailer.yaw,Color(194,198,193,255));
                        const double rearLateral = trailerLateral
                            - side*2.95*static_cast<double>(scale);
                        const LocalRoutePoint rear = groundedPoint(
                            trailerDistance,rearLateral);
                        addOrientedBox(mesh,rear.x,rear.y+2.16f*scale,rear.z,
                                       0.055f,2.86f*scale,2.14f*scale,
                                       rear.yaw,Color(156,164,162,255));
                        for (float ribHeight : {1.08f,1.73f,2.38f,3.03f})
                            addOrientedBox(mesh,rear.x,rear.y+ribHeight*scale,rear.z,
                                           0.060f,0.045f*scale,2.02f*scale,
                                           rear.yaw,Color(94,104,103,255));
                        for (double axleLateralOffset : {-1.55,-0.68})
                            for (double wheelRouteOffset : {-0.94,0.94})
                            {
                                const LocalRoutePoint wheel = groundedPoint(
                                    trailerDistance
                                        + wheelRouteOffset*static_cast<double>(scale),
                                    trailerLateral
                                        + side*axleLateralOffset
                                            *static_cast<double>(scale));
                                addOrientedBox(mesh,wheel.x,wheel.y+0.40f*scale,wheel.z,
                                               0.46f*scale,0.70f*scale,
                                               0.32f*scale,wheel.yaw,
                                               Color(26,29,29,255));
                            }
                        for (double legRouteOffset : {-0.82,0.82})
                        {
                            const LocalRoutePoint leg = groundedPoint(
                                trailerDistance
                                    + legRouteOffset*static_cast<double>(scale),
                                trailerLateral+side*2.25*static_cast<double>(scale));
                            addOrientedBox(mesh,leg.x,leg.y+0.48f*scale,leg.z,
                                           0.18f*scale,0.88f*scale,0.18f*scale,
                                           leg.yaw,Color(70,75,74,255));
                        }
                        ++counters.staticWorldIndustrialYardVehicles_;
                        ++counters.staticWorldIndustrialTrailers_;
                    }
                    ++counters.staticWorldIndustrialCompounds_;
                }
                addOrientedBox(mesh, point.x,point.y+4.2f*scale,point.z,
                               17.0f*scale,8.4f*scale,22.0f*scale,point.yaw,
                               wall);
                addOrientedBox(mesh,point.x,point.y+0.28f*scale,point.z,
                               17.25f*scale,0.56f*scale,22.25f*scale,point.yaw,
                               Color(92,98,98,255));
                addOrientedBox(mesh, point.x,point.y+8.55f*scale,point.z,
                               17.5f*scale,0.35f*scale,22.5f*scale,point.yaw,
                               Color(76,83,85,255));
                const double facade = feature.lateralMetres
                    - std::copysign(8.55 * static_cast<double>(scale), feature.lateralMetres);

                // A paved loading apron and three framed roller shutters turn the formerly
                // blank industrial box into a readable rail-adjacent service building.
                const double apronLateral = facade-side*1.35*static_cast<double>(scale);
                const LocalRoutePoint apron = localRoutePoint(
                    route,anchorDistance,feature.routeDistance,apronLateral);
                addOrientedBox(mesh,apron.x,point.y+0.045f,apron.z,
                               2.75f*scale,0.09f,19.5f*scale,apron.yaw,
                               Color(85,89,87,255));
                for (double zOffset : {-6.5, 0.0, 6.5})
                {
                    const double bayDistance = feature.routeDistance
                        + zOffset*static_cast<double>(scale);
                    const LocalRoutePoint bay = localRoutePoint(
                        route,anchorDistance,bayDistance,facade);
                    LocalRoutePoint groundedBay = bay;
                    groundedBay.y = point.y;
                    addOrientedBox(mesh,groundedBay.x,groundedBay.y+1.75f*scale,
                                   groundedBay.z,0.12f,3.5f*scale,3.5f*scale,
                                   groundedBay.yaw,shutter);
                    for (double jamb : {-1.82,1.82})
                    {
                        LocalRoutePoint frame = localRoutePoint(
                            route,anchorDistance,
                            bayDistance+jamb*static_cast<double>(scale),facade);
                        frame.y = point.y;
                        addOrientedBox(mesh,frame.x,frame.y+1.82f*scale,frame.z,
                                       0.16f,3.64f*scale,0.16f*scale,frame.yaw,
                                       Color(191,196,193,255));
                    }
                    addOrientedBox(mesh,groundedBay.x,groundedBay.y+3.57f*scale,
                                   groundedBay.z,0.16f,0.18f*scale,3.82f*scale,
                                   groundedBay.yaw,Color(191,196,193,255));
                    for (int slat = 1; slat <= 6; ++slat)
                        addOrientedBox(mesh,groundedBay.x,
                                       groundedBay.y+static_cast<float>(slat)*0.49f*scale,
                                       groundedBay.z,0.14f,0.035f*scale,3.28f*scale,
                                       groundedBay.yaw,Color(53,64,68,255));

                    const double dockLateral = facade-side*0.58*static_cast<double>(scale);
                    LocalRoutePoint dock = localRoutePoint(
                        route,anchorDistance,bayDistance,dockLateral);
                    dock.y = point.y;
                    addOrientedBox(mesh,dock.x,dock.y+0.38f*scale,dock.z,
                                   1.05f*scale,0.76f*scale,4.15f*scale,dock.yaw,
                                   Color(119,119,111,255));
                    for (double bumper : {-1.52,1.52})
                    {
                        LocalRoutePoint stop = localRoutePoint(
                            route,anchorDistance,
                            bayDistance+bumper*static_cast<double>(scale),
                            facade-side*0.10*static_cast<double>(scale));
                        stop.y = point.y;
                        addOrientedBox(mesh,stop.x,stop.y+0.78f*scale,stop.z,
                                       0.25f*scale,0.72f*scale,0.24f*scale,
                                       stop.yaw,Color(38,43,44,255));
                    }
                }

                // Clerestory glazing, cladding ribs and a restrained service panel break the
                // remaining broad wall area without introducing a warehouse-only material.
                for (double zOffset : {-7.8,-2.6,2.6,7.8})
                {
                    LocalRoutePoint window = localRoutePoint(
                        route,anchorDistance,
                        feature.routeDistance+zOffset*static_cast<double>(scale),facade);
                    window.y = point.y;
                    addFacadeWindow(window,6.30f*scale,1.90f*scale,1.02f*scale);
                }
                for (int rib = -5; rib <= 5; ++rib)
                {
                    LocalRoutePoint seam = localRoutePoint(
                        route,anchorDistance,
                        feature.routeDistance+static_cast<double>(rib)*1.95
                            *static_cast<double>(scale),facade-side*0.035);
                    seam.y = point.y;
                    addOrientedBox(mesh,seam.x,seam.y+4.25f*scale,seam.z,
                                   0.055f,7.65f*scale,0.055f*scale,seam.yaw,
                                   Color(157,164,164,255));
                }
                LocalRoutePoint servicePanel = localRoutePoint(
                    route,anchorDistance,feature.routeDistance,facade-side*0.055);
                servicePanel.y = point.y;
                addOrientedBox(mesh,servicePanel.x,servicePanel.y+7.55f*scale,
                               servicePanel.z,0.08f,0.72f*scale,4.2f*scale,
                               servicePanel.yaw,industrialBands[
                                   feature.variant%industrialBands.size()]);

                for (double zOffset : {-5.2,5.2})
                {
                    const LocalRoutePoint rooflight = localRoutePoint(
                        route,anchorDistance,
                        feature.routeDistance+zOffset*static_cast<double>(scale),
                        feature.lateralMetres);
                    addOrientedBox(mesh,rooflight.x,point.y+8.86f*scale,rooflight.z,
                                   5.2f*scale,0.24f*scale,3.2f*scale,
                                   rooflight.yaw,Color(105,139,148,255));
                }
                for (double end : {-1.0,1.0})
                    for (double lateralOffset : {-5.2,0.0,5.2})
                    {
                        LocalRoutePoint endWindow = localRoutePoint(
                            route,anchorDistance,
                            feature.routeDistance+end*11.05*static_cast<double>(scale),
                            feature.lateralMetres
                                +lateralOffset*static_cast<double>(scale));
                        endWindow.y = point.y;
                        addEndWallWindow(endWindow,6.15f*scale,
                                         2.10f*scale,1.25f*scale);
                    }

                // Rear-wall plant makes the parts not facing the loading apron functional as
                // well: two louvred HVAC cabinets, a downpipe and paired roof extractors.
                // Every piece stays in the warehouse batch and is deliberately confined to
                // the far facade so the shared yard remains clear for vehicles.
                const double rearFacade = feature.lateralMetres
                    + side*8.62*static_cast<double>(scale);
                for (double equipmentOffset : {-4.7,4.2})
                {
                    LocalRoutePoint cabinet = localRoutePoint(
                        route,anchorDistance,
                        feature.routeDistance
                            + equipmentOffset*static_cast<double>(scale),
                        rearFacade);
                    cabinet.y = point.y;
                    addOrientedBox(mesh,cabinet.x,cabinet.y+0.86f*scale,cabinet.z,
                                   0.74f*scale,1.72f*scale,2.20f*scale,
                                   cabinet.yaw,Color(103,113,112,255));
                    for (int vent = -2; vent <= 2; ++vent)
                    {
                        LocalRoutePoint louvre = localRoutePoint(
                            route,anchorDistance,
                            feature.routeDistance
                                +(equipmentOffset+static_cast<double>(vent)*0.34)
                                    *static_cast<double>(scale),
                            rearFacade+side*0.39*static_cast<double>(scale));
                        louvre.y = point.y;
                        addOrientedBox(mesh,louvre.x,louvre.y+1.05f*scale,louvre.z,
                                       0.035f,0.055f*scale,0.25f*scale,
                                       louvre.yaw,Color(48,57,58,255));
                    }
                }
                LocalRoutePoint downpipe = localRoutePoint(
                    route,anchorDistance,
                    feature.routeDistance+9.4*static_cast<double>(scale),
                    rearFacade+side*0.11*static_cast<double>(scale));
                downpipe.y = point.y;
                addOrientedBox(mesh,downpipe.x,downpipe.y+3.95f*scale,downpipe.z,
                               0.18f*scale,7.70f*scale,0.18f*scale,
                               downpipe.yaw,Color(77,85,85,255));
                for (double extractorOffset : {-5.3,5.3})
                {
                    const LocalRoutePoint extractor = localRoutePoint(
                        route,anchorDistance,
                        feature.routeDistance
                            + extractorOffset*static_cast<double>(scale),
                        feature.lateralMetres
                            + side*3.3*static_cast<double>(scale));
                    mesh.addCylinder(Vector3{extractor.x,point.y+8.68f*scale,
                                            extractor.z},
                                     0.48f*scale,0.72f*scale,8,
                                     Color(91,101,101,255));
                    mesh.addCylinder(Vector3{extractor.x,point.y+9.36f*scale,
                                            extractor.z},
                                     0.67f*scale,0.18f*scale,8,
                                     Color(72,81,82,255));
                }
                ++counters.staticWorldIndustrialUtilityGroups_;
                ++counters.staticWorldDetailedWarehouses_;
                break;
            }
            case World::FeatureKind::Farmstead:
            {
                const double side = feature.lateralMetres < 0.0 ? -1.0 : 1.0;
                const Color yard(117,105,77,255);
                const std::array<Color,3> barnColors = {
                    Color(139,72,55,255),Color(111,90,66,255),Color(133,94,58,255)};
                const Color& barnColor = barnColors[feature.variant%barnColors.size()];
                const Color roofColor(82,74,67,255);
                const Color timber(76,61,43,255);
                addOrientedBox(mesh,point.x,point.y+0.035f,point.z,
                               30.0f*scale,0.07f,34.0f*scale,point.yaw,yard);

                const double barnDistance = feature.routeDistance
                    - 4.5*static_cast<double>(scale);
                const double barnLateral = feature.lateralMetres
                    + side*1.5*static_cast<double>(scale);
                const LocalRoutePoint barn = localRoutePoint(
                    route,anchorDistance,barnDistance,barnLateral);
                addOrientedBox(mesh,barn.x,point.y+2.80f*scale,barn.z,
                               11.0f*scale,5.60f*scale,14.0f*scale,barn.yaw,barnColor);

                // The barn roof uses two real sloped planes rather than a thick flat cap. The
                // duplicate reverse faces keep it readable from either side of the train.
                const auto roofVertex = [&](double distance, double lateral, float height) {
                    const LocalRoutePoint roofPoint = localRoutePoint(
                        route,anchorDistance,distance,lateral);
                    return Vector3{roofPoint.x,point.y+height,roofPoint.z};
                };
                const double roofHalfWidth = 6.0*static_cast<double>(scale);
                const double roofHalfLength = 7.35*static_cast<double>(scale);
                const double roofLow = barnLateral-roofHalfWidth;
                const double roofHigh = barnLateral+roofHalfWidth;
                const double roofBegin = barnDistance-roofHalfLength;
                const double roofEnd = barnDistance+roofHalfLength;
                const float eaveHeight = 5.55f*scale;
                const float ridgeHeight = 7.05f*scale;
                const Vector3 lowBegin = roofVertex(roofBegin,roofLow,eaveHeight);
                const Vector3 lowEnd = roofVertex(roofEnd,roofLow,eaveHeight);
                const Vector3 ridgeBegin = roofVertex(roofBegin,barnLateral,ridgeHeight);
                const Vector3 ridgeEnd = roofVertex(roofEnd,barnLateral,ridgeHeight);
                const Vector3 highBegin = roofVertex(roofBegin,roofHigh,eaveHeight);
                const Vector3 highEnd = roofVertex(roofEnd,roofHigh,eaveHeight);
                mesh.addQuad(lowBegin,lowEnd,ridgeEnd,ridgeBegin,roofColor);
                mesh.addQuad(ridgeBegin,ridgeEnd,lowEnd,lowBegin,roofColor);
                mesh.addQuad(ridgeBegin,ridgeEnd,highEnd,highBegin,roofColor);
                mesh.addQuad(highBegin,highEnd,ridgeEnd,ridgeBegin,roofColor);

                const double barnFacade = barnLateral
                    - side*5.55*static_cast<double>(scale);
                const LocalRoutePoint barnDoor = localRoutePoint(
                    route,anchorDistance,barnDistance,barnFacade);
                addOrientedBox(mesh,barnDoor.x,point.y+2.05f*scale,barnDoor.z,
                               0.13f,4.1f*scale,5.0f*scale,barnDoor.yaw,timber);
                const LocalRoutePoint doorSplit = localRoutePoint(
                    route,anchorDistance,barnDistance,barnFacade-side*0.08);
                addOrientedBox(mesh,doorSplit.x,point.y+2.05f*scale,doorSplit.z,
                               0.05f,4.0f*scale,0.10f,doorSplit.yaw,
                               Color(173,154,111,255));

                const double houseDistance = feature.routeDistance
                    + 10.2*static_cast<double>(scale);
                const double houseLateral = feature.lateralMetres
                    - side*6.8*static_cast<double>(scale);
                const LocalRoutePoint house = localRoutePoint(
                    route,anchorDistance,houseDistance,houseLateral);
                addOrientedBox(mesh,house.x,point.y+2.55f*scale,house.z,
                               7.0f*scale,5.1f*scale,9.0f*scale,house.yaw,
                               Color(190,174,142,255));
                addOrientedBox(mesh,house.x,point.y+5.45f*scale,house.z,
                               7.5f*scale,0.70f*scale,9.5f*scale,house.yaw,
                               Color(105,66,55,255));
                const double houseFacade = houseLateral
                    - side*3.55*static_cast<double>(scale);
                for (double along : {-2.25,2.25})
                {
                    const LocalRoutePoint window = localRoutePoint(
                        route,anchorDistance,
                        houseDistance+along*static_cast<double>(scale),houseFacade);
                    for (float height : {1.85f,3.85f})
                        addOrientedBox(mesh,window.x,point.y+height*scale,window.z,
                                       0.10f,0.72f*scale,0.85f*scale,window.yaw,
                                       Color(57,91,105,255));
                }
                const LocalRoutePoint houseDoor = localRoutePoint(
                    route,anchorDistance,houseDistance,houseFacade);
                addOrientedBox(mesh,houseDoor.x,point.y+1.0f*scale,houseDoor.z,
                               0.12f,2.0f*scale,0.92f*scale,houseDoor.yaw,timber);

                const double siloDistance = feature.routeDistance
                    - 10.5*static_cast<double>(scale);
                const double siloLateral = feature.lateralMetres
                    + side*9.0*static_cast<double>(scale);
                const LocalRoutePoint silo = localRoutePoint(
                    route,anchorDistance,siloDistance,siloLateral);
                mesh.addCylinder(Vector3{silo.x,point.y,silo.z},2.15f*scale,
                                 8.4f*scale,12,Color(148,151,143,255));
                mesh.addCone(Vector3{silo.x,point.y+8.4f*scale,silo.z},2.35f*scale,
                             2.6f*scale,12,Color(92,94,91,255));

                for (int bale = 0; bale < 4; ++bale)
                {
                    const double baleDistance = feature.routeDistance
                        + (-4.0+static_cast<double>(bale%2)*2.2)
                            * static_cast<double>(scale);
                    const double baleLateral = feature.lateralMetres
                        - side*(7.5+static_cast<double>(bale/2)*2.0)
                            * static_cast<double>(scale);
                    const LocalRoutePoint hay = localRoutePoint(
                        route,anchorDistance,baleDistance,baleLateral);
                    addOrientedBox(mesh,hay.x,point.y+0.55f*scale,hay.z,
                                   1.55f*scale,1.10f*scale,1.75f*scale,hay.yaw,
                                   Color(186,154,74,255));
                }

                for (double fenceSide : {-1.0,1.0})
                {
                    const double fenceLateral = feature.lateralMetres
                        + fenceSide*15.0*static_cast<double>(scale);
                    const LocalRoutePoint sideFence = localRoutePoint(
                        route,anchorDistance,feature.routeDistance,fenceLateral);
                    const double fenceDistance = feature.routeDistance
                        + fenceSide*17.0*static_cast<double>(scale);
                    const LocalRoutePoint endFence = localRoutePoint(
                        route,anchorDistance,fenceDistance,feature.lateralMetres);
                    for (float railHeight : {0.43f,0.88f})
                    {
                        addOrientedBox(mesh,sideFence.x,
                                       point.y+railHeight*scale,sideFence.z,
                                       0.08f,0.08f,34.0f*scale,sideFence.yaw,
                                       Color(106,91,65,255));
                        addOrientedBox(mesh,endFence.x,
                                       point.y+railHeight*scale,endFence.z,
                                       30.0f*scale,0.08f,0.08f,endFence.yaw,
                                       Color(106,91,65,255));
                    }
                    for (double along = -15.0; along <= 15.0; along += 6.0)
                    {
                        const LocalRoutePoint post = localRoutePoint(
                            route,anchorDistance,
                            feature.routeDistance+along*static_cast<double>(scale),
                            fenceLateral);
                        addOrientedBox(mesh,post.x,point.y+0.58f*scale,post.z,
                                       0.11f,1.16f*scale,0.11f,post.yaw,
                                       Color(91,76,55,255));
                    }
                    for (double across = -12.0; across <= 12.0; across += 6.0)
                    {
                        const LocalRoutePoint post = localRoutePoint(
                            route,anchorDistance,fenceDistance,
                            feature.lateralMetres
                                + across*static_cast<double>(scale));
                        addOrientedBox(mesh,post.x,point.y+0.58f*scale,post.z,
                                       0.11f,1.16f*scale,0.11f,post.yaw,
                                       Color(91,76,55,255));
                    }
                }
                break;
            }
            case World::FeatureKind::RoadOverpass:
            {
                constexpr float deckTop = 7.85f;
                constexpr float deckThickness = 0.90f;
                const double halfSpan = 33.0 * static_cast<double>(scale);
                const double leftEnd = feature.lateralMetres - halfSpan;
                const double rightEnd = feature.lateralMetres + halfSpan;
                const float roadWidth = 8.8f * scale;
                const Color concrete(126,128,125,255);
                const Color darkConcrete(101,104,102,255);
                const Color asphalt(67,70,71,255);
                const Color steel(79,84,83,255);

                addOrientedBox(mesh,point.x,point.y+deckTop-deckThickness*0.5f,point.z,
                               static_cast<float>(halfSpan*2.0),deckThickness,roadWidth,
                               point.yaw,concrete);
                addOrientedBox(mesh,point.x,point.y+deckTop+0.035f,point.z,
                               static_cast<float>(halfSpan*2.0),0.07f,roadWidth*0.94f,
                               point.yaw,asphalt);
                for (float edge : {-1.0f,1.0f})
                {
                    const LocalRoutePoint fascia = localRoutePoint(
                        route,anchorDistance,
                        feature.routeDistance
                            + static_cast<double>(edge * roadWidth * 0.49f),
                        feature.lateralMetres);
                    addOrientedBox(mesh,fascia.x,fascia.y+deckTop-0.40f,fascia.z,
                                   static_cast<float>(halfSpan*2.0),0.82f,0.22f,
                                   fascia.yaw,darkConcrete);
                    addOrientedBox(mesh,fascia.x,fascia.y+deckTop+0.62f,fascia.z,
                                   static_cast<float>(halfSpan*2.0),0.10f,0.12f,
                                   fascia.yaw,steel);
                }

                // Two piers deliberately avoid both running lines and the parallel urban
                // road. Abutments sit under the span ends; all supports reach the local
                // procedural terrain rather than assuming a perfectly flat corridor.
                for (double pierLateral : {-15.5, 8.5})
                {
                    const LocalRoutePoint pier = localRoutePoint(
                        route,anchorDistance,feature.routeDistance,pierLateral);
                    const float terrainGround = terrainOffset(
                        featurePose,feature.routeDistance,pierLateral);
                    const float height = std::max(
                        1.0f,deckTop-deckThickness-terrainGround);
                    addOrientedBox(mesh,pier.x,
                                   pier.y+terrainGround+height*0.5f,pier.z,
                                   1.45f*scale,height,2.45f*scale,pier.yaw,concrete);
                    addOrientedBox(mesh,pier.x,pier.y+deckTop-deckThickness-0.12f,pier.z,
                                   2.55f*scale,0.34f,3.05f*scale,pier.yaw,darkConcrete);
                }
                for (double abutmentLateral : {leftEnd,rightEnd})
                {
                    const LocalRoutePoint abutment = localRoutePoint(
                        route,anchorDistance,feature.routeDistance,abutmentLateral);
                    const float terrainGround = terrainOffset(
                        featurePose,feature.routeDistance,abutmentLateral);
                    const float height = std::max(
                        1.0f,deckTop-deckThickness-terrainGround);
                    addOrientedBox(mesh,abutment.x,
                                   abutment.y+terrainGround+height*0.5f,abutment.z,
                                   2.2f*scale,height,roadWidth*1.12f,
                                   abutment.yaw,darkConcrete);
                }

                const auto rampVertex = [&](double routeOffset, double lateral,
                                            float relativeHeight) {
                    const LocalRoutePoint ramp = localRoutePoint(
                        route,anchorDistance,feature.routeDistance+routeOffset,lateral);
                    return Vector3{ramp.x,ramp.y+relativeHeight,ramp.z};
                };
                const auto addRamp = [&](double innerLateral, double outerLateral) {
                    const float outerHeight = terrainOffset(
                        featurePose,feature.routeDistance,outerLateral) + 0.10f;
                    const double lowLateral = std::min(innerLateral,outerLateral);
                    const double highLateral = std::max(innerLateral,outerLateral);
                    const float lowHeight = lowLateral == innerLateral ? deckTop : outerHeight;
                    const float highHeight = highLateral == innerLateral ? deckTop : outerHeight;
                    const double halfRoad = static_cast<double>(roadWidth) * 0.47;
                    const Vector3 a = rampVertex(-halfRoad,lowLateral,lowHeight);
                    const Vector3 b = rampVertex(halfRoad,lowLateral,lowHeight);
                    const Vector3 c = rampVertex(halfRoad,highLateral,highHeight);
                    const Vector3 d = rampVertex(-halfRoad,highLateral,highHeight);
                    mesh.addQuad(a,b,c,d,asphalt);
                    for (double routeOffset : {-halfRoad,halfRoad})
                    {
                        const Vector3 outer = rampVertex(
                            routeOffset,outerLateral,outerHeight);
                        const Vector3 inner = rampVertex(
                            routeOffset,innerLateral,deckTop);
                        const Vector3 innerBottom{inner.X,inner.Y-0.48f,inner.Z};
                        const Vector3 outerBottom{outer.X,outer.Y-0.48f,outer.Z};
                        mesh.addQuad(outer,inner,innerBottom,outerBottom,darkConcrete);
                        mesh.addQuad(outerBottom,innerBottom,inner,outer,darkConcrete);
                    }
                };
                addRamp(leftEnd,leftEnd-42.0*static_cast<double>(scale));
                addRamp(rightEnd,rightEnd+42.0*static_cast<double>(scale));

                // Repeated posts and centre markings provide strong parallax when the train
                // passes underneath without requiring a unique bridge material or draw call.
                for (double lateral = leftEnd+3.0; lateral < rightEnd-2.0; lateral += 6.0)
                {
                    for (float edge : {-1.0f,1.0f})
                    {
                        const LocalRoutePoint post = localRoutePoint(
                            route,anchorDistance,
                            feature.routeDistance
                                + static_cast<double>(edge * roadWidth * 0.49f),lateral);
                        addOrientedBox(mesh,post.x,post.y+deckTop+0.31f,post.z,
                                       0.10f,0.62f,0.10f,post.yaw,steel);
                    }
                }
                for (double lateral = leftEnd+4.0; lateral < rightEnd-2.0; lateral += 10.0)
                {
                    const LocalRoutePoint marking = localRoutePoint(
                        route,anchorDistance,feature.routeDistance,lateral);
                    addOrientedBox(mesh,marking.x,marking.y+deckTop+0.085f,marking.z,
                                   5.0f,0.025f,0.13f,marking.yaw,
                                   Color(224,221,202,255));
                }
                break;
            }
            case World::FeatureKind::CatenaryPole:
            {
                addOrientedBox(mesh,point.x,point.y+0.14f,point.z,
                               0.62f,0.28f,0.78f,point.yaw,
                               Color(126,126,119,255));
                addOrientedBox(mesh, point.x,point.y+3.4f,point.z,
                               0.16f,6.8f,0.16f,point.yaw,Color(91,95,91,255));
                const double inward = feature.lateralMetres < 0.0 ? 2.0 : -2.0;
                const LocalRoutePoint arm = localRoutePoint(
                    route, anchorDistance, feature.routeDistance,
                    feature.lateralMetres + inward);
                addOrientedBox(mesh, arm.x,arm.y+6.58f,arm.z,
                               4.8f,0.12f,0.12f,arm.yaw,Color(91,95,91,255));
                const double supportedTrack = feature.lateralMetres < 0.0
                    ? route.parallelTrackLateralMetres(feature.routeDistance) : 0.0;
                const LocalRoutePoint insulator = localRoutePoint(
                    route,anchorDistance,feature.routeDistance,supportedTrack);
                addOrientedBox(mesh,insulator.x,insulator.y+6.47f,insulator.z,
                               0.075f,0.30f,0.075f,insulator.yaw,
                               Color(118,151,147,255));
                const LocalRoutePoint braceTop = localRoutePoint(
                    route,anchorDistance,feature.routeDistance,
                    feature.lateralMetres+inward*1.55);
                const Vector3 braceBase{point.x,point.y+4.95f,point.z};
                const Vector3 braceEnd{braceTop.x,braceTop.y+6.52f,braceTop.z};
                const float tangentX = std::sin(point.yaw)*0.045f;
                const float tangentZ = std::cos(point.yaw)*0.045f;
                const Color braceColor(88,93,90,255);
                mesh.addQuad(
                    braceBase+Vector3{0.0f,-0.045f,0.0f},
                    braceEnd+Vector3{0.0f,-0.045f,0.0f},
                    braceEnd+Vector3{0.0f,0.045f,0.0f},
                    braceBase+Vector3{0.0f,0.045f,0.0f},braceColor);
                mesh.addQuad(
                    braceBase+Vector3{-tangentX,0.0f,-tangentZ},
                    braceEnd+Vector3{-tangentX,0.0f,-tangentZ},
                    braceEnd+Vector3{tangentX,0.0f,tangentZ},
                    braceBase+Vector3{tangentX,0.0f,tangentZ},braceColor);

                const double stagger = feature.variant%2U == 0U ? 0.20 : -0.20;
                const LocalRoutePoint registrationEnd = localRoutePoint(
                    route,anchorDistance,feature.routeDistance,supportedTrack+stagger);
                const Vector3 registrationBase{
                    insulator.x,insulator.y+6.31f,insulator.z};
                const Vector3 registrationTip{
                    registrationEnd.x,registrationEnd.y+5.91f,registrationEnd.z};
                mesh.addQuad(
                    registrationBase+Vector3{0.0f,-0.035f,0.0f},
                    registrationTip+Vector3{0.0f,-0.035f,0.0f},
                    registrationTip+Vector3{0.0f,0.035f,0.0f},
                    registrationBase+Vector3{0.0f,0.035f,0.0f},
                    Color(76,82,79,255));
                addOrientedBox(mesh,insulator.x,insulator.y+6.22f,insulator.z,
                               0.13f,0.055f,0.13f,insulator.yaw,
                               Color(139,164,158,255));
                ++counters.staticWorldCatenaryRegistrationAssemblies_;
                ++counters.staticWorldCatenaryFoundations_;
                break;
            }
            case World::FeatureKind::PowerPylon:
            {
                const Color steel(102,108,106,255);
                const float height = 22.0f * scale;
                for (float lateralOffset : {-1.15f, 1.15f})
                {
                    const double legLateral = feature.lateralMetres
                        + static_cast<double>(lateralOffset * scale);
                    const LocalRoutePoint leg = localRoutePoint(
                        route,anchorDistance,feature.routeDistance,legLateral);
                    addOrientedBox(mesh,leg.x,point.y+height*0.34f,leg.z,
                                   0.30f*scale,height*0.68f,0.30f*scale,
                                   leg.yaw,steel);
                }
                addOrientedBox(mesh,point.x,point.y+height*0.70f,point.z,
                               0.42f*scale,height*0.60f,0.42f*scale,
                               point.yaw,steel);
                constexpr std::array<float, 3> armHeights = {13.0f,17.0f,21.0f};
                constexpr std::array<float, 3> armWidths = {11.0f,14.0f,9.0f};
                for (std::size_t level = 0; level < armHeights.size(); ++level)
                {
                    const float armHeight = armHeights[level] * scale;
                    const float armWidth = armWidths[level] * scale;
                    addOrientedBox(mesh,point.x,point.y+armHeight,point.z,
                                   armWidth,0.24f*scale,0.28f*scale,
                                   point.yaw,steel);
                    for (float wireSide : {-1.0f, 1.0f})
                    {
                        const double wireLateral = feature.lateralMetres
                            + static_cast<double>(wireSide * armWidth * 0.43f);
                        const LocalRoutePoint insulator = localRoutePoint(
                            route,anchorDistance,feature.routeDistance,wireLateral);
                        addOrientedBox(mesh,insulator.x,point.y+armHeight-0.58f*scale,
                                       insulator.z,0.08f,1.0f*scale,0.08f,
                                       insulator.yaw,Color(77,91,91,255));
                        // Each pylon owns the span centred on its 250 m chunk; adjacent
                        // features overlap slightly to avoid gaps at chunk boundaries.
                        addOrientedBox(mesh,insulator.x,point.y+armHeight-1.10f*scale,
                                       insulator.z,0.045f,0.045f,254.0f,
                                       insulator.yaw,Color(63,67,65,255));
                    }
                }
                break;
            }
            case World::FeatureKind::Substation:
            {
                const Color steel(99,106,104,255);
                addOrientedBox(mesh,point.x,point.y+0.08f,point.z,
                               15.0f*scale,0.16f,20.0f*scale,point.yaw,
                               Color(117,116,105,255));
                addOrientedBox(mesh,point.x,point.y+1.45f*scale,point.z,
                               5.6f*scale,2.9f*scale,4.2f*scale,point.yaw,
                               Color(78,91,93,255));
                for (float offset : {-2.4f,-1.2f,0.0f,1.2f,2.4f})
                {
                    const LocalRoutePoint fin = localRoutePoint(
                        route,anchorDistance,
                        feature.routeDistance + static_cast<double>(2.16f * scale),
                        feature.lateralMetres + static_cast<double>(offset * scale));
                    addOrientedBox(mesh,fin.x,point.y+1.45f*scale,fin.z,
                                   0.10f,2.4f*scale,0.18f,
                                   fin.yaw,Color(51,62,64,255));
                }
                for (float lateralOffset : {-5.2f, 5.2f})
                    for (float alongOffset : {-6.2f,0.0f,6.2f})
                    {
                        const double equipmentLateral = feature.lateralMetres
                            + static_cast<double>(lateralOffset * scale);
                        const LocalRoutePoint equipment = localRoutePoint(
                            route,anchorDistance,
                            feature.routeDistance + static_cast<double>(alongOffset * scale),
                            equipmentLateral);
                        addOrientedBox(mesh,equipment.x,point.y+1.65f*scale,equipment.z,
                                       0.16f,3.3f*scale,0.16f,equipment.yaw,steel);
                        mesh.addCylinder(Vector3{equipment.x,point.y+2.15f*scale,equipment.z},
                                         0.16f*scale,1.15f*scale,8,
                                         Color(93,123,122,255));
                    }
                for (float alongOffset : {-8.7f, 8.7f})
                {
                    const LocalRoutePoint fence = localRoutePoint(
                        route,anchorDistance,
                        feature.routeDistance + static_cast<double>(alongOffset * scale),
                        feature.lateralMetres);
                    addOrientedBox(mesh,fence.x,point.y+1.05f,fence.z,
                                   15.6f*scale,2.1f,0.08f,fence.yaw,
                                   Color(79,86,83,255));
                }
                for (float lateralOffset : {-7.7f, 7.7f})
                {
                    const LocalRoutePoint fence = localRoutePoint(
                        route,anchorDistance,feature.routeDistance,
                        feature.lateralMetres
                            + static_cast<double>(lateralOffset * scale));
                    addOrientedBox(mesh,fence.x,point.y+1.05f,fence.z,
                                   0.08f,2.1f,20.2f*scale,fence.yaw,
                                   Color(79,86,83,255));
                }
                break;
            }
            case World::FeatureKind::Signal:
            {
                // Head, lenses, ladder, cabinet and plate come from SignalGeometry.hpp; the
                // live pass below overlays exactly one lit lens at the same positions.
                const int facingDirection = World::signalFacingDirection(feature);
                addColourLightSignal(mesh,[&](double distance,double lateral) {
                    return localRoutePoint(route,anchorDistance,distance,lateral);
                },feature.routeDistance,feature.lateralMetres,facingDirection,
                graphicsQuality_>0);
                const double lensLateral = feature.lateralMetres
                    - std::copysign(kSignalLensInset,feature.lateralMetres);
                const int signalNumber = static_cast<int>(
                    std::floor(feature.routeDistance/250.0)) % 100;
                addTrackFacingText(mesh,route,anchorDistance,
                                   "S"+std::to_string(signalNumber),
                                   feature.routeDistance
                                       - 0.32*static_cast<double>(facingDirection),
                                   lensLateral,3.265f,facingDirection,
                                   Color(37,40,39,255),0.022f);
                break;
            }
            case World::FeatureKind::LevelCrossing:
            {
                // The road, equipment cabinets and dark warning lenses stay in the streamed
                // batch. Only the two striped booms and currently illuminated lenses move.
                addOrientedBox(mesh,point.x,point.y+0.01f,point.z,
                               120.0f,0.12f,6.8f,point.yaw,Color(68,70,71,255));
                for (double edge : {-2.78,2.78})
                {
                    const LocalRoutePoint marking = localRoutePoint(
                        route,anchorDistance,feature.routeDistance+edge);
                    addOrientedBox(mesh,marking.x,marking.y+0.082f,marking.z,
                                   120.0f,0.025f,0.10f,marking.yaw,
                                   Color(221,218,197,255));
                }
                for (double side : {-1.0,1.0})
                {
                    const LocalRoutePoint barrier = localRoutePoint(
                        route,anchorDistance,feature.routeDistance-4.2,side*8.3);
                    addOrientedBox(mesh,barrier.x,barrier.y+0.85f,barrier.z,
                                   0.18f,1.7f,0.18f,barrier.yaw,
                                   Color(62,65,63,255));
                    addOrientedBox(mesh,barrier.x,barrier.y+2.15f,barrier.z,
                                   0.24f,0.56f,0.62f,barrier.yaw,
                                   Color(31,32,31,255));
                    for (double lensOffset : {-0.18,0.18})
                    {
                        const LocalRoutePoint lens = localRoutePoint(
                            route,anchorDistance,
                            feature.routeDistance-4.2+lensOffset,side*8.44);
                        addOrientedBox(mesh,lens.x,lens.y+2.15f,lens.z,
                                       0.045f,0.18f,0.18f,lens.yaw,
                                       Color(82,26,24,255));
                    }
                    const LocalRoutePoint cabinet = localRoutePoint(
                        route,anchorDistance,feature.routeDistance-5.05,side*8.55);
                    addOrientedBox(mesh,cabinet.x,cabinet.y+0.65f,cabinet.z,
                                   0.48f,1.30f,0.62f,cabinet.yaw,
                                   Color(98,103,99,255));
                }
                break;
            }
            case World::FeatureKind::RoadVehicle:
            case World::FeatureKind::OtherTrain:
            case World::FeatureKind::Count:
                break;
        }
    }
}

void WorldBuildContext::buildStations()
{
    [[maybe_unused]] auto& terrain = meshes[0];
    [[maybe_unused]] auto& foliageMaterial = meshes[1];
    [[maybe_unused]] auto& needleMaterial = meshes[2];
    [[maybe_unused]] auto& leafSprays = meshes[3];
    [[maybe_unused]] auto& needleSprays = meshes[4];
    [[maybe_unused]] auto& roofMaterial = meshes[5];
    [[maybe_unused]] auto& waterMaterial = meshes[6];
    [[maybe_unused]] auto& facadeMaterial = meshes[7];
    [[maybe_unused]] auto& renderedFacadeMaterial = meshes[8];
    [[maybe_unused]] auto& mesh = meshes[9];
    [[maybe_unused]] auto& stationLettering = meshes[10];
    [[maybe_unused]] auto& groundShadows = meshes[11];
    [[maybe_unused]] auto& platformMaterial = meshes[12];
    [[maybe_unused]] auto& worldLuminaires = meshes[13];
    [[maybe_unused]] MeshBuilder& roofSurface=graphicsQuality_>=1 ? roofMaterial : mesh;
    // Station furniture is authored, but joins the same static stream batch as its host chunks.
    if (!world.chunks().empty())
    {
        const double visibleBegin = world.chunks().front().beginDistance - 200.0;
        const double visibleEnd = world.chunks().back().endDistance + 200.0;
        for (std::size_t stationIndex = 0; stationIndex < route.stations().size(); ++stationIndex)
        {
            const Sim::Station& station = route.stations()[stationIndex];
            if (station.distanceMetres < visibleBegin || station.distanceMetres > visibleEnd) continue;
            lightSeed = static_cast<std::uint32_t>(stationIndex)*7'919U + 17U;
            windowOrdinal = 0;
            const bool terminal = stationIndex == 0 || stationIndex + 1 == route.stations().size();
            const bool suburban = stationIndex == 2 || stationIndex == 10;
            const bool rural = stationIndex == 3 || stationIndex == 5;
            const bool waterfront = stationIndex == 4 || stationIndex == 9;
            const bool urban = stationIndex == 7 || stationIndex == 8;
            const double inward = stationIndex == 0 ? 1.0 : -1.0;
            const double side = station.platformOnLeft ? -1.0 : 1.0;

            // The terrain ribbon extends beyond both terminal endpoints. The skyline follows
            // that ground instead of sitting on the former untextured, fixed-height apron.
            if (terminal)
            {
                // Authored terminus composition: buffer stops on both tracks past the platform
                // end, a train shed over the final stretch and a head house closing the tracks.
                const auto terminalPoint = [&](double distance,double lateral) {
                    return localRoutePoint(route,anchorDistance,distance,lateral);
                };
                const double pairLateral =
                    route.parallelTrackLateralMetres(station.distanceMetres)*0.5;
                for (double trackLateral : {0.0,route.parallelTrackLateralMetres(station.distanceMetres)})
                {
                    addBufferStop(mesh,terminalPoint,
                                  station.distanceMetres-inward*kTerminalBufferOffset,
                                  trackLateral,inward);
                    ++counters.staticWorldTerminalBufferStops_;
                }
                addTerminusHeadHouse(mesh,terminalPoint,station.distanceMetres,inward,pairLateral);
                counters.staticWorldTerminalShedColumns_ += addTrainShed(
                    mesh,terminalPoint,station.distanceMetres,inward,pairLateral);
                {
                    std::string headHouseName = station.name;
                    for (char& c : headHouseName) c = static_cast<char>(std::toupper(
                        static_cast<unsigned char>(c)));
                    addTrackFacingText(mesh,route,anchorDistance,headHouseName,
                                       station.distanceMetres-inward*(kTerminalHeadHouseNear-0.45),
                                       pairLateral,13.95f,static_cast<int>(inward),
                                       Color(238,239,224,255),0.15f);
                }
                if (graphicsQuality_>=1)
                {
                    // Terrain-conforming public apron on the authored endpoint continuation.
                    // Share the actual terrain cell diagonal with the adjoining city sidewalk.
                    const auto forecourtPoint=[&](float along,float across) {
                        const double distance=station.distanceMetres+inward*along;
                        const double lateral=side*across;
                        constexpr std::array<double,14> bands{-100,-40,-26,-12,-10.4,-8.8,
                            0,8.8,10.4,12,26,40,100,1200};
                        const auto upper=std::upper_bound(bands.begin(),bands.end(),lateral);
                        const auto band=static_cast<std::size_t>(upper-bands.begin()-1);
                        const double d0=std::floor(distance/50.0)*50.0;
                        const auto corner=[&](double d,double l) {
                            auto p=localRoutePoint(route,anchorDistance,d,l);
                            p.y+=terrainOffset(route.sample(d),d,l);
                            return Vector3{p.x,p.y,p.z};
                        };
                        auto point=terrainQuadPoint({corner(d0,bands[band]),corner(d0+50,bands[band]),
                            corner(d0+50,bands[band+1]),corner(d0,bands[band+1])},
                            static_cast<float>((distance-d0)/50),
                            static_cast<float>((lateral-bands[band])/(bands[band+1]-bands[band])));
                        point.Y+=World::forecourtHeightAboveGround(across);
                        return point;
                    };
                    addTerminalForecourt(mesh,platformMaterial,forecourtPoint);
                    unsigned plant=0;
                    for (const auto& bed : World::ForecourtBeds)
                    {
                        const Vector3 root=forecourtPoint(bed[0],bed[1])+Vector3{0,0.20f,0};
                        const float scale=0.92f+static_cast<float>(plant%3)*0.08f;
                        addTaperedTreeTrunk(mesh,root,scale,3.8f,8,Color(92,73,51,255));
                        addClusteredBroadleafCrown(foliageMaterial,root,scale,plant+31,
                            Color(48,94,49,255),Color(77,118,61,255),graphicsQuality_==1);
                        addBroadleafEdgeSprays(leafSprays,root,scale,plant+31,
                            Color(77,118,61,255),graphicsQuality_==1);
                        for (int branch=0;branch<3;++branch)
                        {
                            const float index=static_cast<float>(branch);
                            mesh.addBox({-0.5f,0,-0.5f},{0.5f,1,0.5f},Color(87,66,44,255),
                                Matrix::CreateScale(0.13f*scale,(1.35f+0.14f*index)*scale,0.13f*scale)
                                *Matrix::CreateRotationZ(0.58f+0.07f*index)
                                *Matrix::CreateRotationY(index*2.0944f+static_cast<float>(plant)*0.19f)
                                *Matrix::CreateTranslation(root+Vector3{0,(2.15f+0.18f*index)*scale,0}));
                        }
                        for (float offset : {-3.0f,3.0f})
                        {
                            const auto shrub=forecourtPoint(bed[0]+offset,bed[1])+Vector3{0,0.48f,0};
                            foliageMaterial.addFoliageLobe(shrub,{1.1f,0.45f,1.0f},9,4,
                                Color(72,111,58,255),true);
                        }
                        addSoftVegetationShadow(groundShadows,[&](float x,float z) {
                            return forecourtPoint(bed[0]+z,bed[1]+x)+Vector3{0,0.205f,0};
                        },2.1f,2.5f,Color(13,25,17,34),plant++);
                    }
                    for (float along : {-55.0f,-25.0f})
                        for (float across : {14.0f,48.0f})
                        {
                            const auto root=forecourtPoint(along,across)+Vector3{0,0.004f,0};
                            const auto pose=localRoutePoint(route,anchorDistance,
                                station.distanceMetres+inward*along,side*across);
                            addForecourtBench(mesh,Matrix::CreateRotationY(pose.yaw+static_cast<float>(side)*kPi*0.5f)
                                *Matrix::CreateTranslation(root));
                        }
                    const auto rankPose=localRoutePoint(route,anchorDistance,
                        station.distanceMetres-inward*55.0,side*23.5);
                    addTaxiRank(mesh,[&](float along,float across) {
                        return forecourtPoint(along,across)+Vector3{0,0.004f,0};
                    },rankPose.yaw,static_cast<unsigned>(stationIndex));
                }
                // A restrained two-sided skyline gives the terminal apron depth even past the
                // final route sample. These are deliberately distant blocks: nearby procedural
                // buildings retain facade detail, while this cheap layer only has to break the
                // flat horizon and suggest that both termini belong to substantial towns.
                const std::array<Color, 4> skylineColors = {
                    Color(128,137,137,255), Color(139,130,119,255),
                    Color(117,132,143,255), Color(146,143,130,255)};
                for (float skylineSide : {-1.0f, 1.0f})
                {
                    if (graphicsQuality_ >= 1)
                    {
                        const auto terminalGround=[&](float along,float across) {
                            const double distance=station.distanceMetres+along;
                            const double lateral=skylineSide*58.0+across;
                            // Both streets lie wholly in the existing 40–100 m terrain band.
                            // Terminal terrain uses the route's 50 m cells on either side of its
                            // endpoint; interpolate its actual diagonal, not the analytic height.
                            const double d0=std::floor(distance/50.0)*50.0;
                            const double l0=skylineSide>0 ? 40.0 : -100.0;
                            const auto corner=[&](double d,double l) {
                                auto p=localRoutePoint(route,anchorDistance,d,l);
                                p.y+=terrainOffset(route.sample(d),d,l);
                                return Vector3{p.x,p.y,p.z};
                            };
                            return terrainQuadPoint({corner(d0,l0),corner(d0+50,l0),
                                corner(d0+50,l0+60),corner(d0,l0+60)},
                                static_cast<float>((distance-d0)/50),
                                static_cast<float>((lateral-l0)/60));
                        };
                        addTerminalStreet(mesh,terminalGround);
                        // Only the authored continuation beyond the route endpoint: no streamed
                        // buildings/traffic live there. Retain a generous station-building gap
                        // on the platform side and a clear margin beside the new street sidewalk.
                        for (int tree=0;tree<10;++tree)
                        {
                            const auto placement=terminalVergeTree(tree,
                                static_cast<unsigned>(stationIndex)*31U+(skylineSide>0 ? 7U : 19U));
                            if (skylineSide == side && placement.outward < 90) continue;
                            const float along=-static_cast<float>(inward)*placement.outward;
                            const float across=skylineSide*(placement.lateral-58.0f);
                            const Vector3 root=terminalGround(along,across);
                            addTaperedTreeTrunk(mesh,root,placement.scale,3.8f,7,Color(88,71,49,255));
                            addClusteredBroadleafCrown(foliageMaterial,root,placement.scale,
                                placement.variant,Color(46,88,46,255),Color(67,110,54,255),
                                graphicsQuality_ == 1);
                            addSoftVegetationShadow(groundShadows,[&](float x,float z) {
                                return terminalGround(along+z,across+x)+Vector3{0,0.03f,0};
                            },2.4f*placement.scale,2.0f*placement.scale,
                                Color(12,24,17,50),placement.variant);
                        }
                    }
                    for (int block = 0; block < 12; ++block)
                    {
                        const float lateral = skylineSide
                            * (88.0f + static_cast<float>(block % 3) * 34.0f);
                        const float longitudinal = -275.0f + static_cast<float>(block) * 50.0f;
                        const double buildingDistance=station.distanceMetres+longitudinal;
                        auto base=localRoutePoint(route,anchorDistance,buildingDistance,lateral);
                        base.y += terrainOffset(route.sample(buildingDistance),buildingDistance,lateral);
                        const float x=base.x,z=base.z;
                        const float height = 8.0f
                            + static_cast<float>((block * 5 + static_cast<int>(stationIndex)) % 6)
                                * 2.1f;
                        const float width = 18.0f + static_cast<float>(block % 4) * 3.5f;
                        const float skylineLength = 30.0f
                            + static_cast<float>((block + 2) % 3) * 6.0f;
                        // A buried foundation tolerates the broad terrain cells beneath the
                        // footprint instead of leaving an exposed gap on the downhill edge.
                        addOrientedBox(mesh,x,base.y+(height-2.0f)*0.5f,z,
                                       width,height+2.0f,skylineLength,base.yaw,
                                       skylineColors[(static_cast<std::size_t>(block) + stationIndex)
                                                     % skylineColors.size()]);
                        addOrientedBox(mesh,x,base.y+height+0.21f,z,
                                       width+0.6f,0.42f,skylineLength+0.6f,base.yaw,
                                       Color(65,71,72,255));
                        addTerminalSkylineFacade(mesh,{x,base.y,z},width,height,
                                                 skylineLength,base.yaw,skylineSide);
                        if (graphicsQuality_ >= 1)
                            addTerminalFrontage(mesh,{x,base.y,z},width,skylineLength,
                                                base.yaw,skylineSide);
                    }
                }
            }

            const LocalRoutePoint platform = localRoutePoint(
                route, anchorDistance, station.distanceMetres,
                side * Sim::TrainInterior::PlatformCentrelineFromTrack);
            const float platformLength = station.major ? 240.0f : 180.0f;
            const Color platformBase = terminal ? Color(165,169,168,255)
                                     : suburban ? Color(178,179,173,255)
                                     : rural ? Color(177,166,147,255)
                                     : waterfront ? Color(166,175,173,255)
                                     : urban ? Color(160,166,168,255)
                                             : Color(181,169,153,255);
            const Color platformSurface = terminal ? Color(181,185,184,255)
                                        : suburban ? Color(192,190,181,255)
                                        : rural ? Color(190,179,159,255)
                                        : waterfront ? Color(178,185,181,255)
                                        : urban ? Color(173,178,179,255)
                                                : Color(193,181,164,255);
            addOrientedBox(mesh, platform.x,
                           platform.y+kPlatformSurfaceElevation-0.36f,platform.z,
                           static_cast<float>(Sim::TrainInterior::PlatformWidth),
                           0.72f,platformLength,platform.yaw,platformBase);
            addOrientedTexturedGroundQuad(
                platformMaterial,platform.x,
                platform.y+kPlatformSurfaceElevation+0.002f,platform.z,
                static_cast<float>(Sim::TrainInterior::PlatformWidth)-0.08f,
                platformLength-0.10f,platform.yaw,0.90f,platformSurface);

            // The train body and platform edge retain a realistic 20 cm horizontal gap. A pale
            // coping stone, high-contrast safety line and tiled tactile band make the boundary
            // readable from both the vestibule and a window seat without expensive textures.
            const double platformInnerEdge = side
                * Sim::TrainInterior::PlatformInnerEdgeFromTrack;
            const auto addPlatformStrip = [&](double lateral, float width, float height,
                                              const Color& color) {
                const LocalRoutePoint strip = localRoutePoint(
                    route, anchorDistance, station.distanceMetres, lateral);
                addOrientedBox(mesh,strip.x,
                               strip.y+kPlatformSurfaceElevation+height*0.5f,strip.z,
                               width,height,platformLength-2.0f,strip.yaw,color);
            };
            addPlatformStrip(platformInnerEdge + side * 0.13,0.26f,0.010f,
                             Color(221,218,199,255));
            addPlatformStrip(platformInnerEdge + side * 0.20,0.07f,0.006f,
                             Color(238,198,52,255));
            addPlatformStrip(platformInnerEdge + side * 0.49,0.42f,0.008f,
                             Color(205,187,111,255));
            if (graphicsQuality_>=1)
            {
                const LocalRoutePoint tactile=localRoutePoint(route,anchorDistance,
                    station.distanceMetres,platformInnerEdge+side*0.49);
                addPlatformTactile(mesh,platformMaterial,Matrix::CreateRotationY(tactile.yaw)
                    *Matrix::CreateTranslation(tactile.x,tactile.y+kPlatformSurfaceElevation,tactile.z),
                    platformLength);
            }
            else for (double offset = -platformLength*0.5 + 1.4;
                 offset <= platformLength*0.5 - 1.4; offset += 1.4)
            {
                const LocalRoutePoint joint = localRoutePoint(
                    route, anchorDistance, station.distanceMetres + offset,
                    platformInnerEdge + side * 0.49);
                addOrientedBox(mesh,joint.x,
                               joint.y+kPlatformSurfaceElevation+0.009f,joint.z,
                               0.44f,0.004f,0.035f,joint.yaw,Color(151,137,84,255));
            }

            if (graphicsQuality_>=1)
                counters.staticWorldPlatformSurfaceJoints_+=addPlatformPaving(platformMaterial,
                    Matrix::CreateRotationY(platform.yaw)*Matrix::CreateTranslation(
                        platform.x,platform.y+kPlatformSurfaceElevation,platform.z),
                    platformLength,static_cast<float>(side),platformSurface);
            else
            {
            // Low retains the cheaper broad expansion-joint grid. Integer bays keep
            // the pattern deterministic and symmetrical without accumulating floating-point loop
            // error. Three longitudinal seams complete a readable grid outside the tactile strip.
            constexpr float kPlatformPavingBay = 3.0f;
            const int pavingBays = static_cast<int>(platformLength/kPlatformPavingBay);
            for (int bay = 1; bay < pavingBays; ++bay)
            {
                const double offset = -static_cast<double>(platformLength)*0.5
                    + static_cast<double>(bay)*static_cast<double>(kPlatformPavingBay);
                const LocalRoutePoint joint = localRoutePoint(
                    route,anchorDistance,station.distanceMetres+offset,
                    side*Sim::TrainInterior::PlatformCentrelineFromTrack);
                addOrientedBox(mesh,joint.x,
                               joint.y+kPlatformSurfaceElevation+0.005f,joint.z,
                               4.56f,0.006f,0.030f,joint.yaw,Color(143,145,140,255));
                ++counters.staticWorldPlatformSurfaceJoints_;
            }
            for (double across : {1.35,2.75,4.15})
            {
                const double lateral = platformInnerEdge+side*across;
                const LocalRoutePoint joint = localRoutePoint(
                    route,anchorDistance,station.distanceMetres,lateral);
                addOrientedBox(mesh,joint.x,
                               joint.y+kPlatformSurfaceElevation+0.005f,joint.z,
                               0.030f,0.006f,platformLength-2.0f,joint.yaw,
                               Color(143,145,140,255));
                ++counters.staticWorldPlatformSurfaceJoints_;
            }

            }
            // Bounded deterministic wear keeps each platform from reading as newly poured,
            // uniformly coloured concrete. These low-alpha patches collect around waiting zones
            // and canopy runoff, stay outside the tactile edge, and share the existing contact-
            // shadow submission. Low quality omits them entirely.
            if (graphicsQuality_ >= 1)
            {
                const int weatheringCount = station.major ? 7 : 5;
                for (int mark = 0; mark < weatheringCount; ++mark)
                {
                    const double along = -static_cast<double>(platformLength)*0.34
                        + static_cast<double>(mark)
                            * (static_cast<double>(platformLength)*0.68
                               / static_cast<double>(weatheringCount-1));
                    const int variant = (static_cast<int>(stationIndex)*5+mark*3)%4;
                    const double lateral = side*(2.30+static_cast<double>(variant)*0.93);
                    const LocalRoutePoint stain = localRoutePoint(
                        route,anchorDistance,station.distanceMetres+along,lateral);
                    addOrientedGroundQuad(
                        groundShadows,stain.x,
                        stain.y+kPlatformSurfaceElevation+0.016f,stain.z,
                        0.32f+static_cast<float>(variant)*0.13f,
                        1.05f+static_cast<float>((variant+mark)%3)*0.48f,
                        stain.yaw,Color(48,55,53,12+variant*2));
                    ++counters.staticWorldPlatformWeatheringMarks_;
                }
            }
            const auto stationFamily=rural ? StationFacadeFamily::Rural
                : waterfront ? StationFacadeFamily::Waterfront
                : station.major ? StationFacadeFamily::Urban : StationFacadeFamily::Suburban;
            const CanopyPalette canopyColours=canopyPalette(stationFamily,terminal);
            const LocalRoutePoint canopy = localRoutePoint(
                route, anchorDistance, station.distanceMetres, side * 5.0);
            const float canopyLength = platformCanopyLength(terminal,station.major);
            if (graphicsQuality_ >= 1)
            {
                auto shadow=canopyShadowBounds(canopyLength,kPlatformSurfaceElevation,
                    sunRays(route.sample(station.distanceMetres).headingRadians));
                const float platformOffset=static_cast<float>(side
                    *(Sim::TrainInterior::PlatformCentrelineFromTrack-5.0));
                const float halfWidth=static_cast<float>(Sim::TrainInterior::PlatformWidth)*0.5f;
                shadow.minX=std::max(shadow.minX,platformOffset-halfWidth+0.02f);
                shadow.maxX=std::min(shadow.maxX,platformOffset+halfWidth-0.02f);
                shadow.minZ=std::max(shadow.minZ,-platformLength*0.5f+0.02f);
                shadow.maxZ=std::min(shadow.maxZ,platformLength*0.5f-0.02f);
                if (shadow.maxX > shadow.minX && shadow.maxZ > shadow.minZ)
                {
                    const Vector3 centre=Vector3::Transform(
                        {(shadow.minX+shadow.maxX)*0.5f,kPlatformSurfaceElevation+0.020f,
                         (shadow.minZ+shadow.maxZ)*0.5f},
                        Matrix::CreateRotationY(canopy.yaw)
                            *Matrix::CreateTranslation(canopy.x,canopy.y,canopy.z));
                    addOrientedGroundQuad(groundShadows,centre.X,centre.Y,centre.Z,
                        shadow.maxX-shadow.minX,shadow.maxZ-shadow.minZ,canopy.yaw,
                        Color(13,23,35,100));
                }
            }
            addOrientedBox(mesh, canopy.x,canopy.y+3.6f,canopy.z,
                           4.6f,0.18f,canopyLength,canopy.yaw,canopyColours.deck);
            addCanopySoffit(mesh,{canopy.x,canopy.y,canopy.z},canopyLength,canopy.yaw,
                graphicsQuality_>=1 ? &platformMaterial : nullptr,canopyColours);
            if (canopyColours.valance && graphicsQuality_>=1)
                for (float eave : {-2.30f,2.30f})
                    counters.staticWorldCanopyValanceBoards_ += addCanopyValance(mesh,
                        Matrix::CreateRotationY(canopy.yaw)
                            *Matrix::CreateTranslation(canopy.x,canopy.y,canopy.z),
                        canopyLength,eave,canopyColours.fascia);
            for (double edge : {-1.0,1.0})
            {
                const LocalRoutePoint fascia = localRoutePoint(
                    route,anchorDistance,station.distanceMetres,side*(5.0+edge*2.25));
                addOrientedBox(mesh,fascia.x,fascia.y+3.52f,fascia.z,
                               0.12f,0.34f,canopyLength+0.16f,fascia.yaw,
                               canopyColours.fascia);
                addOrientedBox(mesh,fascia.x,fascia.y+3.36f,fascia.z,
                               0.16f,0.11f,canopyLength+0.20f,fascia.yaw,
                               canopyColours.fasciaLip);
            }
            // The platform-rear gutter drains through two visible downpipes into the authored
            // rear channel. Short floor-level shoes make the connection legible instead of
            // ending each pipe abruptly above the paving.
            for (double offset : {-0.37,0.37})
            {
                const double along = station.distanceMetres
                    + offset*static_cast<double>(canopyLength);
                const LocalRoutePoint downpipe = localRoutePoint(
                    route,anchorDistance,along,side*7.22);
                addOrientedBox(mesh,downpipe.x,downpipe.y+2.12f,downpipe.z,
                               0.085f,2.76f,0.085f,downpipe.yaw,Color(57,67,68,255));
                const LocalRoutePoint shoe = localRoutePoint(
                    route,anchorDistance,along,side*6.91);
                addOrientedBox(mesh,shoe.x,
                               shoe.y+kPlatformSurfaceElevation+0.055f,shoe.z,
                               0.62f,0.09f,0.12f,shoe.yaw,Color(50,61,62,255));
            }
            ++counters.staticWorldPlatformCanopyDrainageAssemblies_;
            const int canopyLightCount = std::max(
                3,static_cast<int>(std::floor(canopyLength/18.0f)));
            for (int light = 0; light < canopyLightCount; ++light)
            {
                const double offset=canopyLightOffset(canopyLength,light,canopyLightCount);
                const LocalRoutePoint panel = localRoutePoint(
                    route,anchorDistance,station.distanceMetres+offset,side*5.0);
                addCanopyLuminaire(mesh,worldLuminaires,Matrix::CreateRotationY(panel.yaw)
                    *Matrix::CreateTranslation(panel.x,panel.y,panel.z));
                addLightPool(nightGlow(),
                             Vector3{panel.x,panel.y+kPlatformSurfaceElevation+0.04f,panel.z},
                             4.2f,Color(84,80,70,255));
                ++counters.staticWorldPlatformCanopyLights_;
            }
            for (double nominal = -canopyLength * 0.42; nominal <= canopyLength * 0.42;
                 nominal += canopySupportSpacing)
            {
                const double offset=canopySupportOffset(nominal,terminal,station.major);
                const LocalRoutePoint support = localRoutePoint(
                    route, anchorDistance, station.distanceMetres + offset, side * 5.0);
                addCanopyFrame(mesh,Matrix::CreateRotationY(support.yaw)
                    *Matrix::CreateTranslation(support.x,support.y,support.z),
                    -static_cast<float>(side)*0.20f,kPlatformSurfaceElevation,canopyColours);
            }

            const auto stationMaterials=stationMaterialPalette(stationFamily,terminal);
            const double buildingDistance = station.distanceMetres
                + inward * (terminal ? 42.0 : station.major ? 28.0 : 12.0);
            const double buildingLateral = side
                * (terminal ? 17.5 : station.major ? 13.0 : 10.0);
            const LocalRoutePoint building = localRoutePoint(
                route,anchorDistance,buildingDistance,buildingLateral);
            const float buildingWidth = terminal ? 18.0f : station.major ? 16.0f : 8.0f;
            const float buildingHeight = terminal ? 10.5f : station.major ? 9.5f : 4.8f;
            const float buildingLength = terminal ? 58.0f : station.major ? 42.0f : 16.0f;
            const Color& buildingColor = stationMaterials.wall;
            const float stationPlinthHeight = terminal || station.major ? 1.25f : suburban ? 1.35f : 0.82f;
            addFacadeMaterial(facadeMaterial,building,buildingLateral > 0.0,
                              buildingWidth,0.0f,stationPlinthHeight,
                              buildingLength,stationMaterials.plinth);
            addFacadeMaterial(stationMaterials.brickUpper ? facadeMaterial : renderedFacadeMaterial,
                              building,buildingLateral > 0.0,
                              buildingWidth,stationPlinthHeight,buildingHeight,
                              buildingLength,buildingColor);
            if (terminal || station.major)
                addOrientedBox(mesh, building.x,building.y+buildingHeight+0.3f,building.z,
                               buildingWidth+0.8f,0.6f,buildingLength+0.8f,building.yaw,
                               Color(77,74,69,255));
            else
                addPitchedRoof(mesh,roofSurface,building.x,building.y+buildingHeight,building.z,
                               buildingWidth+0.9f,buildingLength+1.0f,2.15f,
                               building.yaw,Color(105,68,57,255));

            if (graphicsQuality_ >= 1)
            {
                addOrientedGroundQuad(
                    groundShadows,building.x,building.y+0.026f,building.z,
                    buildingWidth+0.75f,buildingLength+0.75f,building.yaw,
                    Color(9,13,14,34));
            }

            // Track-facing doors and repeated framed windows stop the common station mass from
            // reading as a plain cuboid. Column counts scale with the building family, while all
            // pieces stay in the existing streamed batch.
            const double facadeLateral = buildingLateral
                - side*static_cast<double>(buildingWidth*0.5f+0.07f);
            const int facadeColumns = terminal ? 8 : station.major ? 6 : 4;
            const auto storeys=stationStoreys(buildingHeight,kPlatformSurfaceElevation);
            const double doorOffset = static_cast<double>(buildingLength)
                * (stationIndex%2U == 0U ? 0.07 : -0.07);
            if (graphicsQuality_>=1)
                addStationWallFinish(mesh,Matrix::CreateRotationY(building.yaw)
                    *Matrix::CreateTranslation(building.x,building.y,building.z),
                    buildingWidth,buildingLength,buildingHeight,stationPlinthHeight,
                    static_cast<float>(doorOffset),static_cast<float>(side),stationMaterials.trim,
                    stationMaterials.brickUpper || terminal);
            const float windowWidth=rural ? 1.05f : waterfront ? 1.70f : station.major ? 1.85f : 1.35f;
            const float windowHeight=rural ? 1.35f : station.major ? 1.55f : 1.45f;
            const Color windowFrame=station.major ? Color(115,130,134,255)
                : waterfront ? Color(216,221,207,255) : Color(208,205,182,255);
            if (!terminal && graphicsQuality_>=1)
            {
                const auto front=localRoutePoint(route,anchorDistance,buildingDistance,facadeLateral);
                addStationFacadeIdentity(mesh,
                    Matrix::CreateRotationY(front.yaw-static_cast<float>(side)*1.570796327f)
                        *Matrix::CreateTranslation(front.x,building.y,front.z),
                    buildingLength,buildingHeight,static_cast<float>(side*doorOffset),stationFamily);
            }
            for (int column = 0; column < facadeColumns; ++column)
            {
                const double normalized = (static_cast<double>(column)+0.5)
                    / static_cast<double>(facadeColumns)-0.5;
                const double offset = normalized*static_cast<double>(buildingLength)*0.82;
                const LocalRoutePoint window = localRoutePoint(
                    route,anchorDistance,buildingDistance+offset,facadeLateral);
                for (int floor = 0; floor < storeys.count; ++floor)
                {
                    if (floor == 0 && !stationWindowClearsEntrance(offset,doorOffset,windowWidth)) continue;
                    addFacadeWindow(window,storeys.windowCentre(floor),windowHeight,windowWidth,
                        static_cast<unsigned>(stationIndex*17+static_cast<std::size_t>(column)*7
                            +static_cast<std::size_t>(floor)*3),windowFrame,true);
                }
            }
            const LocalRoutePoint stationDoor = localRoutePoint(
                route,anchorDistance,buildingDistance+doorOffset,facadeLateral);
            addStationEntrance(mesh,
                Matrix::CreateRotationY(stationDoor.yaw+(side>0 ? -1.570796327f : 1.570796327f))
                    *Matrix::CreateTranslation(stationDoor.x,
                        stationDoor.y+kPlatformSurfaceElevation+0.004f,stationDoor.z));

            // Rain runoff gives the otherwise clean modular elevation a restrained vertical age
            // cue. Narrow deterministic marks descend from the roof edge between window bays;
            // they sit a few centimetres in front of the track-facing wall, remain opaque and
            // low-contrast for stable OpenGL 3.3 batching, and disappear at Low quality.
            if (graphicsQuality_ >= 1)
            {
                const std::array<Color,4> weatheredFacadeColors = {
                    Color(151,131,105,255),Color(131,145,139,255),
                    Color(157,148,124,255),Color(123,136,150,255)};
                const int streakCount = terminal ? 6 : station.major ? 5 : 3;
                const double streakLateral = facadeLateral-side*0.035;
                for (int streak = 0; streak < streakCount; ++streak)
                {
                    const double normalized =
                        (static_cast<double>(streak)+0.5)
                            / static_cast<double>(streakCount)-0.5;
                    const double offset = normalized*static_cast<double>(buildingLength)*0.72
                        + (static_cast<int>(stationIndex)%3-1)*0.16;
                    LocalRoutePoint mark = localRoutePoint(
                        route,anchorDistance,buildingDistance+offset,streakLateral);
                    mark.y = building.y;
                    const int variant = (static_cast<int>(stationIndex)*3+streak*5)%4;
                    const float markHeight = 0.62f+static_cast<float>(variant)*0.19f;
                    const float markTop = buildingHeight-0.34f
                        - static_cast<float>((streak+static_cast<int>(stationIndex))%2)*0.12f;
                    addOrientedBox(mesh,mark.x,
                                   mark.y+markTop-markHeight*0.5f,mark.z,
                                   0.035f,markHeight,
                                   0.09f+static_cast<float>(variant)*0.025f,
                                   mark.yaw,
                                   weatheredFacadeColors[stationIndex
                                       % weatheredFacadeColors.size()]);
                    ++counters.staticWorldStationFacadeWeatheringStreaks_;
                }
            }

            if (terminal)
            {
                // Endpoint halls use the same efficient box vocabulary as other stations, but a
                // taller mass, track-facing glass wall, layered roof and clock tower give each
                // terminus a recognizable silhouette through the train windows.
                const LocalRoutePoint facade = localRoutePoint(
                    route, anchorDistance, station.distanceMetres + inward * 42.0, side * 8.35);
                addOrientedBox(mesh, facade.x,facade.y+4.7f,facade.z,
                               0.28f,7.8f,46.0f,facade.yaw,Color(70,125,145,255));
                for (double offset = -20.0; offset <= 20.0; offset += 8.0)
                {
                    const LocalRoutePoint mullion = localRoutePoint(
                        route, anchorDistance,
                        station.distanceMetres + inward * (42.0 + offset), side * 8.18);
                    addOrientedBox(mesh, mullion.x,mullion.y+4.7f,mullion.z,
                                   0.16f,8.0f,0.18f,mullion.yaw,Color(53,62,66,255));
                }
                const LocalRoutePoint upperRoof = localRoutePoint(
                    route, anchorDistance, station.distanceMetres + inward * 42.0, side * 17.5);
                addOrientedBox(mesh, upperRoof.x,upperRoof.y+11.35f,upperRoof.z,
                               18.8f,0.9f,52.0f,upperRoof.yaw,Color(61,68,70,255));
                const LocalRoutePoint tower = localRoutePoint(
                    route, anchorDistance, station.distanceMetres + inward * 66.0, side * 19.5);
                addOrientedBox(mesh, tower.x,tower.y+9.0f,tower.z,
                               6.4f,18.0f,6.4f,tower.yaw,Color(151,145,126,255));
                addOrientedBox(mesh, tower.x,tower.y+18.5f,tower.z,
                               7.2f,1.0f,7.2f,tower.yaw,Color(66,68,67,255));
                addOrientedBox(mesh, tower.x,tower.y+14.3f,tower.z,
                               0.32f,2.4f,2.4f,tower.yaw,Color(238,239,224,255));
            }
            else if (stationIndex == 2 || stationIndex == 10)
            {
                // Suburban stops use a pedestrian bridge and glazed lift towers.
                const double crossingDistance = station.distanceMetres + 42.0;
                const LocalRoutePoint bridge = localRoutePoint(
                    route, anchorDistance, crossingDistance, -1.0);
                addOrientedBox(mesh,bridge.x,bridge.y+6.4f,bridge.z,
                               20.0f,0.65f,2.8f,bridge.yaw,Color(84,96,101,255));
                for (double lateral : {-8.0, 8.0})
                {
                    const LocalRoutePoint lift = localRoutePoint(
                        route, anchorDistance, crossingDistance, lateral);
                    addOrientedBox(mesh,lift.x,lift.y+3.2f,lift.z,
                                   2.4f,6.4f,2.6f,lift.yaw,Color(73,126,145,255));
                    addOrientedBox(mesh,lift.x,lift.y+6.75f,lift.z,
                                   2.8f,0.35f,3.0f,lift.yaw,Color(62,69,71,255));
                }
            }
            else if (stationIndex == 4 || stationIndex == 9)
            {
                // Lakehurst and Redwater carry the shared wave material right to their
                // promenades. Three shore-to-depth strips replace the former flat blue box while
                // staying in the one streamed water submission used by the lake and estuary.
                const std::array<Color,3> waterfrontColours = {
                    Color(79,145,159,255),Color(59,126,151,255),Color(46,108,139,255)};
                for (std::size_t band = 0; band < waterfrontColours.size(); ++band)
                {
                    const double inwardLateral = 20.0+static_cast<double>(band)*16.0;
                    const LocalRoutePoint water = localRoutePoint(
                        route,anchorDistance,station.distanceMetres,side*inwardLateral);
                    addOrientedTexturedGroundQuad(
                        waterMaterial,water.x,water.y-0.125f,water.z,
                        band == 2 ? 20.0f : 16.2f,250.0f,water.yaw,7.5f,
                        waterfrontColours[band]);
                    ++counters.staticWorldStationWaterfrontSections_;
                }
                const LocalRoutePoint promenade = localRoutePoint(
                    route, anchorDistance, station.distanceMetres, side * 13.0);
                addOrientedBox(mesh,promenade.x,promenade.y+0.12f,promenade.z,
                               6.0f,0.24f,170.0f,promenade.yaw,Color(155,151,139,255));
                for (double offset : {-48.0, 0.0, 48.0})
                {
                    const LocalRoutePoint mast = localRoutePoint(
                        route, anchorDistance, station.distanceMetres + offset, side * 13.0);
                    addOrientedBox(mesh,mast.x,mast.y+2.0f,mast.z,
                                   0.12f,4.0f,0.12f,mast.yaw,Color(69,76,77,255));
                }
            }
            else if (stationIndex == 7 || stationIndex == 8)
            {
                // The junction and city station use a broad elevated concourse spanning tracks.
                const LocalRoutePoint concourse = localRoutePoint(
                    route, anchorDistance, station.distanceMetres - 46.0, -1.5);
                addOrientedBox(mesh,concourse.x,concourse.y+7.2f,concourse.z,
                               24.0f,4.2f,15.0f,concourse.yaw,Color(105,126,135,255));
                addOrientedBox(mesh,concourse.x,concourse.y+9.45f,concourse.z,
                               25.0f,0.45f,16.0f,concourse.yaw,Color(54,63,66,255));
                for (double lateral : {-9.0, 7.0})
                {
                    const LocalRoutePoint support = localRoutePoint(
                        route, anchorDistance, station.distanceMetres - 46.0, lateral);
                    addOrientedBox(mesh,support.x,support.y+3.0f,support.z,
                                   1.0f,6.0f,1.0f,support.yaw,Color(72,79,80,255));
                }
            }
            else
            {
                // Merefield and Stonebridge retain a brick market-town clock turret.
                const LocalRoutePoint turret = localRoutePoint(
                    route, anchorDistance, station.distanceMetres + 24.0, side * 12.0);
                addFacadeMaterial(facadeMaterial,turret,side>0,4.4f,0,10.8f,4.4f,Color(165,119,87,255));
                addStationWallFinish(mesh,Matrix::CreateRotationY(turret.yaw)
                    *Matrix::CreateTranslation(turret.x,turret.y,turret.z),
                    4.4f,4.4f,10.8f,1.15f,0,static_cast<float>(side),Color(170,166,148,255),true);
                const Matrix turretFace=Matrix::CreateRotationY(turret.yaw-static_cast<float>(side)*kPi*0.5f)
                    *Matrix::CreateTranslation(turret.x,turret.y,turret.z);
                addMarketTurretFace(mesh,turretFace);
                mesh.addBox({-0.55f,0.10f,2.211f},{0.55f,2.25f,2.232f},Color(60,72,70,255),turretFace);
                addResidentialDoorKit(mesh,1.10f,2.15f,
                    Matrix::CreateTranslation(0,0.10f,2.21f)*turretFace,true);
                addOrientedBox(mesh,turret.x,turret.y+11.05f,turret.z,
                               5.2f,0.55f,5.2f,turret.yaw,Color(78,70,65,255));
            }

            // Platform identity: lamps, benches, a ticket machine, and a readable color-band sign.
            for (double offset : {-60.0,-30.0,0.0,30.0,60.0})
            {
                if (!station.major && std::abs(offset) > 31.0) continue;
                const LocalRoutePoint lamp = localRoutePoint(
                    route, anchorDistance, station.distanceMetres + offset, side * 4.9);
                addOrientedBox(mesh, lamp.x,lamp.y+2.25f,lamp.z,
                               0.10f,4.5f,0.10f,lamp.yaw,Color(62,69,69,255));
                addOrientedBox(mesh, lamp.x,lamp.y+4.55f,lamp.z,
                               0.45f,0.16f,0.28f,lamp.yaw,Color(246,231,178,255));
                addOrientedBox(nightGlow(),lamp.x,lamp.y+4.55f,lamp.z,
                               0.47f,0.18f,0.30f,lamp.yaw,Color(255,226,170,255));
                addLightPool(nightGlow(),
                             Vector3{lamp.x,lamp.y+kPlatformSurfaceElevation+0.035f,lamp.z},
                             5.5f,Color(92,78,54,255));
                if (std::abs(offset) < 1.0)
                {
                    // A paired camera pod makes the centre mast read as monitored railway
                    // infrastructure rather than another bare lighting post.
                    addOrientedBox(mesh,lamp.x,lamp.y+3.76f,lamp.z,
                                   0.72f,0.07f,0.08f,lamp.yaw,
                                   Color(103,110,109,255));
                    const LocalRoutePoint camera = localRoutePoint(
                        route,anchorDistance,station.distanceMetres+offset+0.34,side*4.9);
                    addOrientedBox(mesh,camera.x,camera.y+3.71f,camera.z,
                                   0.22f,0.15f,0.38f,camera.yaw,
                                   Color(53,61,63,255));
                }
            }
            const Color benchSlat = terminal ? Color(54,82,96,255)
                                  : suburban ? Color(70,100,108,255)
                                  : rural ? Color(116,82,50,255)
                                  : waterfront ? Color(62,101,117,255)
                                  : urban ? Color(74,80,82,255)
                                          : Color(126,84,55,255);
            const std::array<double,3> benchOffsets = station.major
                ? std::array<double,3>{-38.0,0.0,38.0}
                : std::array<double,3>{-24.0,24.0,0.0};
            const int benchCount = station.major ? 3 : 2;
            // Furniture follows the station family (ENV-136): heritage slatted benches at rural
            // stops, perforated steel seats at major and terminal stations, plain benches elsewhere.
            const PlatformBenchStyle benchStyle = rural ? PlatformBenchStyle::Slatted
                : (urban || terminal) ? PlatformBenchStyle::Perforated : PlatformBenchStyle::Classic;
            // Local +Z faces the track, as for the platform bin.
            const auto furniturePlacement=[&](double offset,double lateral) {
                const LocalRoutePoint p=localRoutePoint(
                    route,anchorDistance,station.distanceMetres+offset,side*lateral);
                return Matrix::CreateRotationY(p.yaw-static_cast<float>(side)*kPi*0.5f)
                    *Matrix::CreateTranslation(p.x,p.y+kPlatformSurfaceElevation,p.z);
            };
            for (int benchIndex = 0; benchIndex < benchCount; ++benchIndex)
            {
                const double offset = benchOffsets[static_cast<std::size_t>(benchIndex)];
                const LocalRoutePoint bench = localRoutePoint(
                    route, anchorDistance, station.distanceMetres + offset, side * kPlatformBenchLateral);
                addPlatformBench(mesh,furniturePlacement(offset,kPlatformBenchLateral),benchStyle,benchSlat);
                if (graphicsQuality_ >= 1)
                    addOrientedGroundQuad(
                        groundShadows,bench.x,
                        bench.y+kPlatformSurfaceElevation+0.018f,bench.z,
                        0.92f,2.72f,bench.yaw,Color(8,12,13,26));
                ++counters.staticWorldPlatformBenchAssemblies_;
            }
            if (rural)
            {
                // The wind shelter encloses the second bench; it formerly stood 3 m further on
                // as a solid box that the bench and its seated figures cut through.
                addPlatformWindShelter(mesh,furniturePlacement(benchOffsets[1],kPlatformBenchLateral));
                ++counters.staticWorldPlatformAmenities_;
            }
            addPlatformHelpPoint(mesh,furniturePlacement(kPlatformHelpPointOffset,kPlatformAmenityLateral));
            for (const double validator : kPlatformValidatorOffsets)
                addTicketValidator(mesh,furniturePlacement(validator,kPlatformAmenityLateral));
            counters.staticWorldPlatformAmenities_ += 3;
            if (station.major)
            {
                addVendingMachine(mesh,worldLuminaires,
                    furniturePlacement(kPlatformVendingOffset,kPlatformAmenityLateral+0.15),
                    static_cast<unsigned>(stationIndex));
                ++counters.staticWorldPlatformAmenities_;
            }
            else
            {
                addCycleStands(mesh,furniturePlacement(kPlatformCycleStandOffset,kPlatformAmenityLateral+0.15));
                ++counters.staticWorldPlatformAmenities_;
            }
            if (station.major || terminal)
            {
                addLuggageTrolley(mesh,furniturePlacement(kPlatformTrolleyOffset,kPlatformAmenityLateral));
                ++counters.staticWorldPlatformAmenities_;
            }
            const LocalRoutePoint ticket = localRoutePoint(
                route, anchorDistance, station.distanceMetres + 7.0, side * 4.5);
            addOrientedBox(mesh, ticket.x,ticket.y+1.05f,ticket.z,
                           0.65f,2.1f,0.62f,ticket.yaw,Color(41,116,145,255));
            const LocalRoutePoint ticketScreen = localRoutePoint(
                route,anchorDistance,station.distanceMetres+7.0,side*(4.5-0.335));
            addOrientedBox(mesh,ticketScreen.x,ticketScreen.y+1.52f,ticketScreen.z,
                           0.035f,0.42f,0.44f,ticketScreen.yaw,Color(24,48,58,255));

            // Glazed poster cases and electrical cabinets make the platform rear read as an
            // operated public space rather than a line of generic props. Their compact coloured
            // inserts vary by station family while frames, vents and warning plates stay shared.
            const std::array<Color,4> posterColours = {
                Color(191,89,66,255),Color(58,117,148,255),
                Color(91,139,88,255),Color(194,151,58,255)};
            for (double posterOffset : {-18.0,18.0})
            {
                const double posterLateral = side*5.62;
                const LocalRoutePoint poster = localRoutePoint(
                    route,anchorDistance,station.distanceMetres+posterOffset,posterLateral);
                addOrientedBox(mesh,poster.x,
                               poster.y+kPlatformSurfaceElevation+1.43f,poster.z,
                               0.18f,2.05f,1.48f,poster.yaw,Color(67,73,73,255));
                const LocalRoutePoint posterFace = localRoutePoint(
                    route,anchorDistance,station.distanceMetres+posterOffset,
                    posterLateral-side*0.10);
                addOrientedBox(mesh,posterFace.x,
                               posterFace.y+kPlatformSurfaceElevation+1.43f,posterFace.z,
                               0.035f,1.72f,1.19f,posterFace.yaw,
                               posterColours[(stationIndex
                                   +(posterOffset > 0.0 ? 1U : 0U))%posterColours.size()]);
                addOrientedBox(mesh,posterFace.x,
                               posterFace.y+kPlatformSurfaceElevation+2.09f,posterFace.z,
                               0.040f,0.14f,0.91f,posterFace.yaw,
                               Color(235,232,211,255));
                ++counters.staticWorldPlatformPosterCases_;
            }

            const int cabinetCount = station.major ? 2 : 1;
            for (int cabinetIndex = 0; cabinetIndex < cabinetCount; ++cabinetIndex)
            {
                const double cabinetOffset = cabinetIndex == 0 ? -38.0 : 38.0;
                const double cabinetLateral = side*5.88;
                const LocalRoutePoint cabinet = localRoutePoint(
                    route,anchorDistance,station.distanceMetres+cabinetOffset,
                    cabinetLateral);
                addOrientedBox(mesh,cabinet.x,
                               cabinet.y+kPlatformSurfaceElevation+0.85f,cabinet.z,
                               0.78f,1.70f,0.92f,cabinet.yaw,Color(111,119,115,255));
                const LocalRoutePoint cabinetFace = localRoutePoint(
                    route,anchorDistance,station.distanceMetres+cabinetOffset,
                    cabinetLateral-side*0.405);
                addOrientedBox(mesh,cabinetFace.x,
                               cabinetFace.y+kPlatformSurfaceElevation+0.88f,cabinetFace.z,
                               0.035f,1.48f,0.72f,cabinetFace.yaw,Color(143,150,144,255));
                for (float ventHeight : {0.47f,0.59f,0.71f})
                    addOrientedBox(mesh,cabinetFace.x,
                                   cabinetFace.y+kPlatformSurfaceElevation+ventHeight,
                                   cabinetFace.z,0.040f,0.035f,0.43f,cabinetFace.yaw,
                                   Color(74,81,79,255));
                addOrientedBox(mesh,cabinetFace.x,
                               cabinetFace.y+kPlatformSurfaceElevation+1.27f,
                               cabinetFace.z,0.042f,0.30f,0.30f,cabinetFace.yaw,
                               Color(229,190,48,255));
                ++counters.staticWorldPlatformEquipmentCabinets_;
            }

            if (station.major)
            {
                // Major stations receive two restrained planted islands at the platform rear.
                // Separate soil and varied evergreen lobes make them readable from a carriage
                // without introducing another material or draw call.
                for (double planterOffset : {-42.0,42.0})
                {
                    const double planterDistance = station.distanceMetres+planterOffset;
                    const double planterLateral = side*5.72;
                    const LocalRoutePoint planter = localRoutePoint(
                        route,anchorDistance,planterDistance,planterLateral);
                    addOrientedBox(mesh,planter.x,
                                   planter.y+kPlatformSurfaceElevation+0.29f,planter.z,
                                   0.82f,0.58f,2.45f,planter.yaw,Color(109,105,94,255));
                    addOrientedBox(mesh,planter.x,
                                   planter.y+kPlatformSurfaceElevation+0.595f,planter.z,
                                   0.70f,0.05f,2.28f,planter.yaw,Color(75,57,41,255));
                    for (double shrubOffset : {-0.70,0.0,0.70})
                    {
                        const LocalRoutePoint shrub = localRoutePoint(
                            route,anchorDistance,planterDistance+shrubOffset,planterLateral);
                        const float variation = shrubOffset == 0.0 ? 1.0f : 0.84f;
                        mesh.addEllipsoid(
                            Vector3{shrub.x,
                                    shrub.y+kPlatformSurfaceElevation+0.93f*variation,
                                    shrub.z},
                            Vector3{0.37f*variation,0.43f*variation,0.42f*variation},
                            8,4,shrubOffset < 0.0 ? Color(55,105,61,255)
                                                  : Color(63,116,68,255));
                    }
                    ++counters.staticWorldPlatformPlanters_;
                }
            }

            const LocalRoutePoint bin = localRoutePoint(
                route,anchorDistance,station.distanceMetres-13.0,side*4.72);
            addPlatformBin(mesh,Matrix::CreateRotationY(bin.yaw-static_cast<float>(side)*kPi*0.5f)
                *Matrix::CreateTranslation(bin.x,bin.y+kPlatformSurfaceElevation,bin.z));
            addRoutePrintedText(stationLettering,route,anchorDistance,"LITTER",
                station.distanceMetres-13.0,side*(4.72-0.284),kPlatformSurfaceElevation+0.56f,
                Color(222,230,217,255),0.25f);

            // A drainage channel at the platform rear gives the paving a plausible construction
            // boundary. Short inset slots avoid turning the whole edge into one black stripe.
            const double drainageLateral = side*6.62;
            for (double drainOffset = -platformLength*0.42;
                 drainOffset <= platformLength*0.42; drainOffset += 4.0)
            {
                const LocalRoutePoint drain = localRoutePoint(
                    route,anchorDistance,station.distanceMetres+drainOffset,drainageLateral);
                addOrientedBox(mesh,drain.x,
                               drain.y+kPlatformSurfaceElevation+0.011f,drain.z,
                               0.28f,0.012f,2.9f,drain.yaw,Color(65,70,68,255));
            }

            // A real rear boundary gives the raised slab a safe, legible edge instead of letting
            // it dissolve directly into terrain. Modular rail bays leave a deliberate access gap
            // aligned with the station building. Rural/suburban stops add restrained planting
            // beyond the fence; everything remains in the existing static batch.
            const double accessOffset = inward*(terminal ? 42.0 : station.major ? 28.0 : 12.0);
            const double boundaryLateral = side*6.66;
            for (const auto& run : platformFenceRuns(platformLength,accessOffset))
            {
                if (run[1]-run[0]<0.1) continue;
                const int bays=static_cast<int>(std::ceil((run[1]-run[0])/6.0));
                const double step=(run[1]-run[0])/bays;
                for (int index=0;index<=bays;++index)
                {
                    const double offset=run[0]+static_cast<double>(index)*step;
                    const LocalRoutePoint post=localRoutePoint(route,anchorDistance,
                        station.distanceMetres+offset,boundaryLateral);
                    addPlatformFencePost(mesh,Matrix::CreateRotationY(post.yaw)
                        *Matrix::CreateTranslation(post.x,post.y+kPlatformSurfaceElevation,post.z));
                    if (index==bays) continue;
                    const LocalRoutePoint rail=localRoutePoint(route,anchorDistance,
                        station.distanceMetres+offset+step*0.5,boundaryLateral);
                    for (float height : {0.42f,1.16f})
                        addOrientedBox(mesh,rail.x,rail.y+kPlatformSurfaceElevation+height,rail.z,
                            0.075f,0.070f,static_cast<float>(step),rail.yaw,Color(83,92,90,255));
                    ++counters.staticWorldPlatformBoundarySections_;
                }
            }
            if (rural || suburban)
            {
                const std::array<double,5> plantingOffsets = {-58.0,-31.0,0.0,31.0,58.0};
                for (std::size_t planting = 0; planting < plantingOffsets.size(); ++planting)
                {
                    const double offset = plantingOffsets[planting];
                    if (!station.major && std::abs(offset) > platformLength*0.42) continue;
                    if (std::abs(offset-accessOffset) < 5.5) continue;
                    const LocalRoutePoint shrub = localRoutePoint(
                        route,anchorDistance,station.distanceMetres+offset,side*7.55);
                    const float scaleVariation = 0.86f
                        + 0.08f*static_cast<float>((planting+stationIndex)%3U);
                    mesh.addEllipsoid(
                        Vector3{shrub.x,shrub.y+0.62f*scaleVariation,shrub.z},
                        Vector3{0.72f*scaleVariation,0.62f*scaleVariation,
                                1.12f*scaleVariation},
                        8,3,planting%2U == 0U ? Color(55,103,54,255)
                                              : Color(64,115,58,255));
                }
            }
            for (double offset : stationNameBoardOffsets(station.major))
            {
                const double distance=station.distanceMetres+offset;
                const double signLateral=stationNameBoardLateral(offset);
                const auto sign=localRoutePoint(route,anchorDistance,distance,side*signLateral);
                addStationNameBoard(mesh,
                    Matrix::CreateRotationY(sign.yaw-static_cast<float>(side)*1.570796327f)
                        *Matrix::CreateTranslation(sign.x,sign.y,sign.z),kPlatformSurfaceElevation);
                addRoutePrintedText(stationLettering,route,anchorDistance,station.name,
                    distance,side*(signLateral-0.075),2.58f,Color(238,244,239,255),2.50f);
            }

            const double platformNumberDistance = station.distanceMetres + platformIdentifierOffset;
            const LocalRoutePoint platformNumber = localRoutePoint(
                route, anchorDistance, platformNumberDistance, side * 5.4);
            addOrientedBox(mesh,platformNumber.x,platformNumber.y+3.12f,platformNumber.z,
                           0.15f,0.65f,0.80f,platformNumber.yaw,Color(25,71,101,255));
            addOrientedBox(mesh,platformNumber.x,platformNumber.y+3.56f,platformNumber.z,
                           0.055f,0.23f,0.055f,platformNumber.yaw,Color(105,115,116,255));
            addRoutePrintedText(stationLettering, route, anchorDistance,
                             "P" + std::to_string(1 + stationIndex % 4),
                             platformNumberDistance, side * 5.30, 3.24f,
                             Color(244,203,55,255),0.65f);

            // Direction-specific departure boards show the actual limit used by simulation on
            // the next segment. The face is offset toward the approaching cab and the glyph order
            // is mirrored for reverse running, so both boards read correctly from their train.
            const auto addSpeedBoard = [&](double distance, int direction, double limit) {
                if (distance < 0.0 || distance > route.lengthMetres()) return;
                const double lateral = direction > 0 ? 3.15 : -3.15;
                const LocalRoutePoint pole = localRoutePoint(
                    route, anchorDistance, distance, lateral);
                addOrientedBox(mesh,pole.x,pole.y+1.20f,pole.z,
                               0.10f,2.40f,0.10f,pole.yaw,Color(72,77,76,255));
                const LocalRoutePoint backing = localRoutePoint(
                    route, anchorDistance, distance, lateral);
                addOrientedBox(mesh,backing.x,backing.y+2.40f,backing.z,
                               1.45f,1.28f,0.14f,backing.yaw,Color(31,34,33,255));
                const LocalRoutePoint face = localRoutePoint(
                    route, anchorDistance, distance-direction*0.085, lateral);
                addOrientedBox(mesh,face.x,face.y+2.40f,face.z,
                               1.25f,1.08f,0.05f,face.yaw,Color(238,236,220,255));
                const int kilometresPerHour = static_cast<int>(std::lround(limit * 3.6));
                addTrackFacingText(mesh,route,anchorDistance,
                                   std::to_string(kilometresPerHour),
                                   distance-direction*0.125,lateral,2.62f,direction,
                                   Color(33,37,36,255));
            };
            const double boardOffset = static_cast<double>(platformLength) * 0.5 + 18.0;
            if (stationIndex + 1 < route.stations().size())
                addSpeedBoard(station.distanceMetres + boardOffset, 1,
                              route.stations()[stationIndex + 1]
                                  .speedLimitMetresPerSecondFromPrevious);
            if (stationIndex > 0)
                addSpeedBoard(station.distanceMetres - boardOffset, -1,
                              station.speedLimitMetresPerSecondFromPrevious);

            if (station.major)
            {
                const LocalRoutePoint clock = localRoutePoint(
                    route, anchorDistance, station.distanceMetres, side * 5.1);
                addOrientedBox(mesh, clock.x,clock.y+4.6f,clock.z,
                               0.24f,2.1f,0.24f,clock.yaw,Color(58,64,65,255));
                addOrientedBox(mesh, clock.x,clock.y+5.75f,clock.z,
                               0.30f,1.15f,1.15f,clock.yaw,Color(238,239,224,255));
                for (int marker = 0; marker < 12; ++marker)
                {
                    const float angle = 2.0f*kPi*static_cast<float>(marker)/12.0f;
                    const LocalRoutePoint tick = localRoutePoint(
                        route,anchorDistance,
                        station.distanceMetres+static_cast<double>(std::sin(angle)*0.45f),
                        side*4.92);
                    addOrientedBox(mesh,tick.x,tick.y+5.75f+std::cos(angle)*0.45f,tick.z,
                                   0.055f,0.075f,0.075f,tick.yaw,Color(54,58,57,255));
                }
                const LocalRoutePoint clockBoss = localRoutePoint(
                    route,anchorDistance,station.distanceMetres,side*4.91);
                addOrientedBox(mesh,clockBoss.x,clockBoss.y+5.75f,clockBoss.z,
                               0.065f,0.10f,0.10f,clockBoss.yaw,Color(54,58,57,255));
            }
        }
    }

}

} // namespace

void WorldGeometry::build(const Sim::Route& route,
                         const World::StreamingWorld& world,
                         double anchorDistance, int graphicsQuality, float vegetationDistance,
                         const SunPosition& sun)
{
    graphicsQuality_ = graphicsQuality;
    sun_ = sun;
    vegetationDistanceMetres_ = vegetationDistance;
    const auto generationStart = std::chrono::steady_clock::now();
    for (auto& batch : meshes) batch.clear();
    counters = {};
    // Seven high-quality chunks, a nearby station, and dense rural vegetation fit below these
    // capacities on the authored route. Reserving once keeps streaming rebuilds allocation-light.
    meshes[0].reserve(40'000,60'000);
    meshes[6].reserve(1'200,1'800);
    meshes[7].reserve(2'000,3'000);
    meshes[8].reserve(2'000,3'000);
    meshes[13].reserve(40'000, 60'000);
    meshes[11].reserve(2'000,3'000);
    meshes[12].reserve(256,384);
    const double anchorElevation = route.sample(anchorDistance).elevation;
    // Chunks are independent: each builds into its own builders on a small thread pool, then
    // the parts are appended in chunk order, so the merged output equals a sequential build.
    const auto& chunks = world.chunks();
    if (chunkParts_.size() < chunks.size()) chunkParts_.resize(chunks.size());
    const auto buildPart = [&](std::size_t index) {
        auto& part = chunkParts_[index];
        for (auto& batch : part.meshes) batch.clear();
        part.counters = {};
        part.cache = RoutePointCache{};
        WorldBuildContext context{route,world,anchorDistance,graphicsQuality_,vegetationDistanceMetres_,
                                  anchorElevation,part.meshes,part.counters,part.cache,sun_};
        context.buildChunk(chunks[index]);
    };
    const std::size_t workers = std::min<std::size_t>(chunks.size(), workerThreads_);
    std::atomic<std::size_t> nextChunk{0};
    const auto drain = [&] {
        for (std::size_t index = nextChunk++; index < chunks.size(); index = nextChunk++)
            buildPart(index);
    };
    std::vector<std::future<void>> helpers;
    for (std::size_t i = 1; i < workers; ++i) helpers.push_back(std::async(std::launch::async,drain));
    drain();
    for (auto& helper : helpers) helper.get();
    cacheHits = 0;
    cacheMisses = 0;
    for (std::size_t index = 0; index < chunks.size(); ++index)
    {
        auto& part = chunkParts_[index];
        for (std::size_t material = 0; material < kWorldMaterialCount; ++material)
            meshes[material].append(part.meshes[material].data());
        counters += part.counters;
        cacheHits += part.cache.hits();
        cacheMisses += part.cache.misses();
    }
    RoutePointCache routePointCache;
    WorldBuildContext stations{route,world,anchorDistance,graphicsQuality_,vegetationDistanceMetres_,
                               anchorElevation,meshes,counters,routePointCache,sun_};
    stations.buildStations();
    cacheHits += routePointCache.hits();
    cacheMisses += routePointCache.misses();
    // GPU footprint of this window: the largest single material bounds one upload and the
    // graphics-profile buffer limit; the total is what each of the two GPU windows holds.
    for (std::size_t i=0;i<meshes.size();++i)
    {
        const auto& data=meshes[i].data();
        segments[i]=meshSegments(data);
        counters.worldGpuSegments_+=static_cast<int>(segments[i].size());
        const std::size_t bytes=data.vertices.size()*sizeof(LitColorVertex)
                               +data.indices.size()*sizeof(std::uint32_t);
        counters.worldGpuBytes_+=bytes;
        if (bytes>counters.largestMaterialBytes_)
        {
            counters.largestMaterialBytes_=bytes;
            counters.largestMaterial_=static_cast<int>(i);
        }
        counters.largestMaterialVertices_=std::max(counters.largestMaterialVertices_,data.vertices.size());
    }
    generationMs = std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-generationStart).count();
}


} // namespace TrainSim::Render
