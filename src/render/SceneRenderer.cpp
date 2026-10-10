#include "trainsim/render/SceneRenderer.hpp"

#include "trainsim/render/AvatarMotion.hpp"
#include "trainsim/render/BridgeGeometry.hpp"
#include "trainsim/render/TunnelGeometry.hpp"
#include "trainsim/render/TrackGeometry.hpp"
#include "trainsim/render/LinesideGeometry.hpp"
#include "trainsim/render/SignalGeometry.hpp"
#include "trainsim/render/InteriorFittingsGeometry.hpp"
#include "trainsim/render/AvatarAppearance.hpp"
#include "trainsim/render/AvatarProportions.hpp"
#include "trainsim/render/PlatformLayout.hpp"
#include "trainsim/render/VegetationGeometry.hpp"
#include "trainsim/render/TerrainPatchGeometry.hpp"
#include "trainsim/render/SkyGeometry.hpp"
#include "trainsim/render/FacadeGeometry.hpp"
#include "trainsim/render/StreetPocketGeometry.hpp"
#include "trainsim/render/RoadVehicleGeometry.hpp"
#include "trainsim/world/StreetLayout.hpp"
#include "trainsim/render/GroundShadowGeometry.hpp"
#include "trainsim/render/StationGeometry.hpp"
#include "trainsim/render/ForecourtGeometry.hpp"
#include "trainsim/render/RoutePointCache.hpp"
#include "trainsim/render/LandscapeVariation.hpp"
#include "trainsim/render/ViewCulling.hpp"
#include "trainsim/render/InteriorGeometry.hpp"
#include "trainsim/render/GpuMesh.hpp"
#include "trainsim/render/OpenGL33FacadeEffect.hpp"
#include "trainsim/render/OpenGL33WaterEffect.hpp"
#include "trainsim/render/ShoreGeometry.hpp"
#include <span>
#include "trainsim/render/Daylight.hpp"
#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/render/PixelGlyphs.hpp"
#include "trainsim/render/PassengerDisplayText.hpp"
#include "trainsim/render/PassengerDoorGeometry.hpp"
#include "trainsim/render/CarRoleGeometry.hpp"
#include "trainsim/render/ProceduralMaterials.hpp"
#include "trainsim/sim/TrainInterior.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarAnimation.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarAnimationPreset.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarBone.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarBodyType.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarDescription.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarRenderer.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "System/TimeSpan.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::GamerServices;

namespace TrainSim::Render {

namespace {
constexpr float kCarLength = 21.0f;
constexpr float kFirstCarCentre = -52.5f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTrainFloorElevation =
    static_cast<float>(Sim::TrainInterior::FloorElevationAboveRail);
constexpr float kPlatformSurfaceElevation =
    static_cast<float>(Sim::TrainInterior::PlatformSurfaceAboveRail);
constexpr std::size_t kOnboardAvatarVisuals = 12;
constexpr std::size_t kPlatformAvatarVisuals = 6;
constexpr std::size_t kTotalAvatarVisuals = kOnboardAvatarVisuals + kPlatformAvatarVisuals;
// CNA reports Ready before the first Draw() performs EasyGL resource upload. Leaving several
// complete presentation intervals between that upload and the next background model build avoids
// driver/dependency work leaking across the boundary on slower or heavily loaded desktops.
constexpr int kAvatarUploadQuiescenceFrames = 8;

std::size_t onboardAvatarBudget(int graphicsQuality) noexcept
{
    return graphicsQuality <= 0 ? 6U : graphicsQuality == 1 ? 9U : kOnboardAvatarVisuals;
}

std::size_t platformAvatarBudget(int graphicsQuality) noexcept
{
    return graphicsQuality <= 0 ? 2U : graphicsQuality == 1 ? 4U : kPlatformAvatarVisuals;
}

std::size_t totalAvatarBudget(int graphicsQuality) noexcept
{
    return onboardAvatarBudget(graphicsQuality) + platformAvatarBudget(graphicsQuality);
}

bool avatarUsesNativeCap(const AvatarDescription& description)
{
    // CNA descriptions are opaque to games, but CNA's own documented format keeps the optional
    // little-endian hat item at bytes 41-42; catalog v3 assigns 100 to its covered-hair cap. We
    // only inspect public CreateRandom() results here;
    // the description is never modified or reconstructed, so AvatarRenderer remains authoritative.
    const auto bytes = description.getDescriptionProperty();
    return bytes.size() > 42U && bytes[41] == 100 && bytes[42] == 0;
}

int civilianAppearanceScore(const AvatarDescription& description)
{
    return civilianAppearanceScoreBytes(description.getDescriptionProperty());
}

int staffAppearanceScore(const AvatarDescription& description)
{
    const auto bytes = description.getDescriptionProperty();
    if (bytes.size() <= 42U) return 0;
    const bool headwear = avatarUsesNativeCap(description);
    const auto isServiceBlue = [&bytes](std::size_t offset) {
        return bytes[offset] == 60 && bytes[offset+1] == 80 && bytes[offset+2] == 180;
    };
    return 4 * static_cast<int>(headwear)
         + static_cast<int>(isServiceBlue(19U))
         + static_cast<int>(isServiceBlue(28U));
}

Vector3 rgb(const Color& color)
{
    return Vector3(static_cast<float>(color.getRProperty()) / 255.0f,
                   static_cast<float>(color.getGProperty()) / 255.0f,
                   static_cast<float>(color.getBProperty()) / 255.0f);
}

Matrix boxMatrix(float x, float y, float z, float sx, float sy, float sz)
{
    return Matrix::CreateScale(sx, sy, sz) * Matrix::CreateTranslation(x, y, z);
}

Matrix orientedBoxMatrix(float x, float y, float z, float sx, float sy, float sz, float yaw)
{
    return Matrix::CreateScale(sx, sy, sz)
         * Matrix::CreateRotationY(yaw)
         * Matrix::CreateTranslation(x, y, z);
}

LocalRoutePoint localRoutePoint(const Sim::Route& route, double referenceDistance,
                                double sampleDistance, double lateral = 0.0)
{
    const Sim::RelativeRoutePose pose = route.relativePose(referenceDistance, sampleDistance);
    const double cosine = std::cos(pose.headingRadians);
    const double sine = std::sin(pose.headingRadians);
    return LocalRoutePoint{
        static_cast<float>(pose.x + lateral * cosine),
        static_cast<float>(pose.elevation),
        static_cast<float>(pose.z - lateral * sine),
        static_cast<float>(pose.headingRadians),
    };
}

std::optional<double> stationSidingLateral(const Sim::Route& route, double distance)
{
    for (const Sim::Station& station : route.stations())
    {
        if (!station.major) continue;
        const double separation = std::abs(distance - station.distanceMetres);
        if (separation >= 650.0) continue;
        const double blend = std::clamp((650.0 - separation) / 250.0, 0.0, 1.0);
        const double eased = blend * blend * (3.0 - 2.0 * blend);
        const double target = station.platformOnLeft ? 6.4 : -13.5;
        return target * eased;
    }
    return std::nullopt;
}

void addOrientedBox(MeshBuilder& mesh, float x, float y, float z,
                    float sx, float sy, float sz, float yaw, const Color& color)
{
    mesh.addBox(Vector3{-0.5f,-0.5f,-0.5f}, Vector3{0.5f,0.5f,0.5f}, color,
                orientedBoxMatrix(x, y, z, sx, sy, sz, yaw));
}

int addLayeredConifer(MeshBuilder& mesh, const Vector3& ground, float scale, int sides,
                      std::uint32_t variant, const Color& foliage,
                      const Color& foliageLight, bool closeDetail,MeshBuilder* sprays=nullptr)
{
    return addConiferCrown(mesh,ground,scale,sides,variant,foliage,foliageLight,closeDetail,sprays);
}

void addOrientedGroundQuad(MeshBuilder& mesh, float x, float y, float z,
                           float width, float length, float yaw, const Color& color)
{
    const Matrix transform = Matrix::CreateRotationY(yaw)
                           * Matrix::CreateTranslation(x,y,z);
    const float halfWidth = width*0.5f;
    const float halfLength = length*0.5f;
    mesh.addQuad(
        Vector3::Transform(Vector3{-halfWidth,0.0f,-halfLength},transform),
        Vector3::Transform(Vector3{-halfWidth,0.0f, halfLength},transform),
        Vector3::Transform(Vector3{ halfWidth,0.0f, halfLength},transform),
        Vector3::Transform(Vector3{ halfWidth,0.0f,-halfLength},transform),color);
}

void addOrientedTexturedGroundQuad(MeshBuilder& mesh, float x, float y, float z,
                                   float width, float length, float yaw, float tileMetres,
                                   const Color& color)
{
    const Matrix transform = Matrix::CreateRotationY(yaw)
                           * Matrix::CreateTranslation(x,y,z);
    const float halfWidth = width*0.5f;
    const float halfLength = length*0.5f;
    mesh.addTexturedQuad(
        Vector3::Transform(Vector3{-halfWidth,0.0f,-halfLength},transform),
        Vector3::Transform(Vector3{-halfWidth,0.0f, halfLength},transform),
        Vector3::Transform(Vector3{ halfWidth,0.0f, halfLength},transform),
        Vector3::Transform(Vector3{ halfWidth,0.0f,-halfLength},transform),color,
        Vector2{0.0f,0.0f},Vector2{0.0f,length/tileMetres},
        Vector2{width/tileMetres,length/tileMetres},Vector2{width/tileMetres,0.0f});
}

float terrainOffset(const Sim::RoutePose& pose, double distance, double lateral)
{
    const double corridorBlend = std::clamp((std::abs(lateral) - 12.0) / 150.0, 0.0, 1.0);
    if (pose.waterCrossing)
    {
        // The estuary bed remains safely below the water near the railway. Beyond roughly 100 m,
        // a smooth deterministic bank rises above the water plane instead of leaving an endless
        // blue horizon. Existing wide terrain bands interpolate the transition cheaply.
        const double shore = std::clamp((riverShoreCoordinate(distance,lateral) - 90.0) / 140.0, 0.0, 1.0);
        const double easedShore = shore * shore * (3.0 - 2.0 * shore);
        const double distantRelief = 2.2 * std::sin(distance / 1'350.0 + lateral / 210.0);
        return static_cast<float>(-11.0 + easedShore * (14.0 + distantRelief));
    }
    const double relief = 7.0
                        + 8.5 * std::sin(distance / 1'900.0 + lateral / 83.0)
                        + 5.0 * std::sin(distance / 720.0 - lateral / 145.0);
    const double ridgeBlend = std::clamp((std::abs(lateral) - 230.0) / 760.0, 0.0, 1.0);
    const double ridge = ridgeBlend
        * (26.0 + 18.0 * std::sin(distance / 4'200.0 + lateral / 390.0));
    const double urbanScale = 1.0 - pose.urbanity * 0.72;
    double naturalTerrain = -0.34 + corridorBlend * (relief + ridge) * urbanScale;
    // Urban streets and both pavements occupy the 12--24.7 m lateral band. Keep their
    // formation below the road surface, then ease back into the surrounding terrain.
    // Explicit mesh samples at 26/40 m below preserve this flat corridor after tessellation.
    if (pose.urbanity > 0.28 && !pose.tunnel)
    {
        const double blend = std::clamp((std::abs(lateral)-26.0)/14.0,0.0,1.0);
        const double eased = blend*blend*(3.0-2.0*blend);
        naturalTerrain = -0.34+(naturalTerrain+0.34)*eased;
    }
    if (!pose.tunnel && pose.urbanity < 0.55 && pose.adjacentWaterSide == 0)
    {
        // A shallow drain begins outside the railway fence rather than cutting into either
        // ballast shoulder. Explicit terrain-band samples below give this depression actual
        // banks; it is not a dark stripe painted over an otherwise flat field.
        const double distanceFromCentre = std::abs(lateral);
        if (distanceFromCentre > 8.8 && distanceFromCentre < 12.0)
        {
            const double acrossDrain = (distanceFromCentre-8.8)/3.2;
            const double bowl = std::sin(acrossDrain*3.14159265358979323846);
            naturalTerrain -= 0.34*bowl*bowl;
        }
    }
    if (pose.adjacentWaterSide != 0)
    {
        const double waterwardLateral = lateral
            * static_cast<double>(pose.adjacentWaterSide);
        return static_cast<float>(lakeShoreTerrainOffset(
            naturalTerrain+lakeFarBankRelief(distance,waterwardLateral),
            lakeShoreCoordinate(distance,waterwardLateral),
            pose.adjacentWaterSurfaceElevation-pose.elevation));
    }
    // The second authored crossing is a dry viaduct over a valley running across the line.
    return static_cast<float>(viaductValleyTerrain(distance,lateral,naturalTerrain));
}

Color roadVehicleColor(std::uint32_t variant)
{
    static const std::array<Color, 6> colors = {
        Color(186,51,45,255), Color(44,92,142,255), Color(225,220,204,255),
        Color(57,61,63,255), Color(205,145,45,255), Color(79,126,80,255)};
    return colors[variant % colors.size()];
}

AvatarAnimationPreset avatarPreset(std::size_t index)
{
    constexpr std::array<AvatarAnimationPreset, 8> presets = {
        AvatarAnimationPreset::Stand0, AvatarAnimationPreset::Stand1,
        AvatarAnimationPreset::FemaleIdleLookAround, AvatarAnimationPreset::MaleIdleLookAround,
        AvatarAnimationPreset::Stand2, AvatarAnimationPreset::FemaleIdleShiftWeight,
        AvatarAnimationPreset::MaleIdleShiftWeight, AvatarAnimationPreset::Stand5,
    };
    return presets[index % presets.size()];
}

Matrix rotatedBone(const Matrix& local, const Matrix& rotation)
{
    return rotation * Matrix::CreateTranslation(local.getTranslationProperty());
}

void addDisplayText(MeshBuilder& mesh, std::string_view text, float y, float z,
                    bool mirrored, const Color& color, float centreX = 0.0f,
                    float maximumWidth = 100.0f)
{
    if (text.empty()) return;
    const float pixel = std::min(0.0153f,maximumWidth/(static_cast<float>(text.size())*6.0f));
    const float width = static_cast<float>(text.size()) * 6.0f * pixel;
    const float left = centreX - width * 0.5f;
    for (std::size_t character = 0; character < text.size(); ++character)
    {
        const PixelGlyphRows rows = pixelGlyphRows(text[character]);
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < 5; ++column)
            {
                if ((rows[static_cast<std::size_t>(row)] & (0x10U >> column)) == 0) continue;
                float x = left + (static_cast<float>(character) * 6.0f
                                + static_cast<float>(column)) * pixel;
                if (mirrored) x = 2.0f * centreX - x - pixel;
                const float y0 = y - static_cast<float>(row) * pixel;
                const float x1 = x + pixel * 0.82f;
                const float y1 = y0 + pixel * 0.82f;
                if (mirrored)
                    mesh.addQuad(Vector3{x1,y0,z}, Vector3{x,y0,z},
                                 Vector3{x,y1,z}, Vector3{x1,y1,z}, color);
                else
                    mesh.addQuad(Vector3{x,y0,z}, Vector3{x1,y0,z},
                                 Vector3{x1,y1,z}, Vector3{x,y1,z}, color);
            }
    }
}

void addRouteSignText(MeshBuilder& mesh, const Sim::Route& route, double anchorDistance,
                      std::string_view text, double centreDistance, double lateral,
                      float baselineY, const Color& color, float maximumWidth = 3.2f)
{
    if (text.empty()) return;
    const float pixel = std::min(0.034f,
        maximumWidth / (static_cast<float>(text.size()) * 6.0f));
    const double width = static_cast<double>(text.size()) * 6.0 * pixel;
    for (std::size_t character = 0; character < text.size(); ++character)
    {
        const PixelGlyphRows rows = pixelGlyphRows(text[character]);
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < 5; ++column)
            {
                if ((rows[static_cast<std::size_t>(row)] & (0x10U >> column)) == 0) continue;
                const double unmirroredAlong = -width * 0.5
                    + (static_cast<double>(character) * 6.0 + column) * pixel;
                // Every platform face points inward toward the train. Reversing the along-track
                // glyph order on the negative-lateral side keeps station names and live boards
                // readable from either side instead of exposing mirrored text through the window.
                const double along = lateral < 0.0
                    ? -unmirroredAlong - pixel
                    : unmirroredAlong;
                const LocalRoutePoint point = localRoutePoint(
                    route, anchorDistance, centreDistance + along, lateral);
                addOrientedBox(mesh,point.x,
                               point.y+baselineY-static_cast<float>(row)*pixel,
                               point.z,0.045f,pixel*0.82f,pixel*0.82f,point.yaw,color);
            }
    }
}

void addRoutePrintedText(MeshBuilder& mesh,const Sim::Route& route,double anchorDistance,
    std::string_view text,double distance,double lateral,float baselineY,
    const Color& colour,float maximumWidth=3.2f)
{
    const auto point=localRoutePoint(route,anchorDistance,distance,lateral);
    const float side=lateral<0 ? -1.0f : 1.0f;
    addPrintedSignText(mesh,text,maximumWidth,
        Matrix::CreateRotationY(point.yaw-side*1.570796327f)
            *Matrix::CreateTranslation(point.x,point.y+baselineY,point.z),colour);
}

void addTrackFacingText(MeshBuilder& mesh, const Sim::Route& route, double anchorDistance,
                        std::string_view text, double centreDistance, double lateral,
                        float baselineY, int direction, const Color& color,
                        float pixel = 0.065f)
{
    const double width = static_cast<double>(text.size()) * 6.0 * pixel;
    for (std::size_t character = 0; character < text.size(); ++character)
    {
        const PixelGlyphRows rows = pixelGlyphRows(text[character]);
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < 5; ++column)
            {
                if ((rows[static_cast<std::size_t>(row)] & (0x10U >> column)) == 0) continue;
                const double horizontal = -width * 0.5
                    + (static_cast<double>(character) * 6.0 + column) * pixel;
                const LocalRoutePoint point = localRoutePoint(
                    route, anchorDistance, centreDistance,
                    lateral + horizontal * static_cast<double>(direction));
                addOrientedBox(mesh,point.x,
                               point.y+baselineY-static_cast<float>(row)*pixel,
                               point.z,pixel*0.82f,pixel*0.82f,0.045f,
                               point.yaw,color);
            }
    }
}
}

struct SceneRenderer::AvatarVisual
{
    std::unique_ptr<AvatarDescription> description;
    std::unique_ptr<AvatarRenderer> renderer;
    std::unique_ptr<AvatarAnimation> animation;
    std::vector<Matrix> boneScratch;
    std::array<int,71> parentBones{};
    float seatedPoseBlend = 0.0f;
    float walkingPoseBlend = 0.0f;
    float facingYaw = 0.0f;
    float gazeYaw = 0.0f;
    float gazePitch = 0.0f;
    float gaitPhaseRadians = 0.0f;
    double previousPassengerX = 0.0;
    double previousPassengerZ = 0.0;
    bool motionPositionInitialized = false;
    bool facingInitialized = false;
    bool gpuPrimed = false;
};

SceneRenderer::SceneRenderer(GraphicsDevice& device, float vegetationDistanceMetres,
                             int graphicsQuality, bool passengerDoorsInitiallyOpen,
                             bool avatarRenderingEnabled)
    : device_(device),
      doorOpenAmount_(passengerDoorsInitiallyOpen ? 1.0f : 0.0f),
      vegetationDistanceMetres_(std::clamp(vegetationDistanceMetres, 250.0f, 1'000.0f)),
      graphicsQuality_(std::clamp(graphicsQuality, 0, 2))
{
    ProceduralImage whiteMaterialImage;
    whiteMaterialImage.width = 8;
    whiteMaterialImage.height = 8;
    whiteMaterialImage.pixels.assign(64U,Color::White);
    whiteMaterialTexture_ = uploadMipmappedTexture(device_,whiteMaterialImage);
    passengerDisplayFontTexture_ = uploadMipmappedTexture(device_,makePassengerDisplayFontImage());
    passengerDisplayEffect_ = std::make_unique<BasicEffect>(device_);
    passengerDisplayEffect_->setVertexColorEnabledProperty(true);
    passengerDisplayEffect_->setLightingEnabledProperty(false);
    passengerDisplayEffect_->setTextureEnabledProperty(true);
    passengerDisplayEffect_->setTextureProperty(passengerDisplayFontTexture_.get());
    effect_ = std::make_unique<BasicEffect>(device_);
    effect_->setVertexColorEnabledProperty(true);
    effect_->setLightingEnabledProperty(true);
    effect_->setPreferPerPixelLightingProperty(false);
    effect_->setAmbientLightColorProperty(Vector3{0.56f,0.58f,0.62f});
    effect_->setSpecularColorProperty(Vector3::Zero);
    effect_->DirectionalLight0.setEnabledProperty(true);
    effect_->DirectionalLight0.setDirectionProperty(
        daylightDirection(0.0));
    effect_->DirectionalLight0.setDiffuseColorProperty(Vector3{0.46f,0.43f,0.37f});
    effect_->DirectionalLight0.setSpecularColorProperty(Vector3::Zero);
    effect_->DirectionalLight1.setEnabledProperty(false);
    effect_->DirectionalLight2.setEnabledProperty(false);
    // A complete white mip chain is the identity material for legacy vertex-coloured batches.
    // Keeping it explicit makes sampler ownership deterministic as textured material passes are
    // introduced, rather than relying on a renderer-provided implicit fallback texture.
    effect_->setTextureProperty(whiteMaterialTexture_.get());
    effect_->setTextureEnabledProperty(true);
    effect_->setFogEnabledProperty(true);
    effect_->setFogColorProperty(rgb(daylightSkyColor(0)));
    effect_->setFogStartProperty(360.0f);
    effect_->setFogEndProperty(1'050.0f);
    terrainDetailTexture_ = uploadMipmappedTexture(device_,makeTerrainDetailImage());
    foliageDetailTexture_ = uploadMipmappedTexture(device_,makeFoliageDetailImage());
    needleDetailTexture_ = uploadMipmappedTexture(device_,makeNeedleDetailImage());
    leafSprayTexture_=uploadMipmappedTexture(device_,makeLeafSprayImage(),96);
    needleSprayTexture_=uploadMipmappedTexture(device_,makeNeedleSprayImage(),96);
    leafSprayEffect_=std::make_unique<AlphaTestEffect>(device_);
    leafSprayEffect_->setTextureProperty(leafSprayTexture_.get());
    leafSprayEffect_->setVertexColorEnabledProperty(true);
    leafSprayEffect_->setReferenceAlphaProperty(96);
    roofDetailTexture_ = uploadMipmappedTexture(device_,makeRoofDetailImage());
    ballastDetailTexture_ = uploadMipmappedTexture(device_,makeBallastDetailImage());
    carpetDetailTexture_ = uploadMipmappedTexture(device_,makeCarpetDetailImage());
    upholsteryDetailTexture_ = uploadMipmappedTexture(device_,makeUpholsteryDetailImage());
    concreteDetailTexture_ = uploadMipmappedTexture(device_,makeConcreteDetailImage());
    interiorPanelDetailTexture_ = uploadMipmappedTexture(
        device_,makeInteriorPanelDetailImage());
    waterDetailTexture_ = uploadMipmappedTexture(device_,makeWaterDetailImage());
    resilientFloorDetailTexture_ = uploadMipmappedTexture(
        device_,makeResilientFloorDetailImage());
    masonryDetailTexture_ = uploadMipmappedTexture(device_,makeMasonryDetailImage());
    renderedFacadeDetailTexture_ = uploadMipmappedTexture(
        device_,makeRenderedFacadeDetailImage());
    cloudDetailTexture_ = uploadMipmappedTexture(device_,makeCloudAtlasImage());
    // Keep textured materials on their own stock effect so their lighting, texture ownership,
    // and future specular policy remain independent from the legacy vertex-colour batches.
    materialEffect_ = std::make_unique<BasicEffect>(device_);
    materialEffect_->setVertexColorEnabledProperty(true);
    materialEffect_->setLightingEnabledProperty(true);
    materialEffect_->setPreferPerPixelLightingProperty(graphicsQuality_ >= 1);
    materialEffect_->setAmbientLightColorProperty(Vector3{0.56f,0.58f,0.62f});
    materialEffect_->setSpecularColorProperty(Vector3::Zero);
    materialEffect_->DirectionalLight0.setEnabledProperty(true);
    materialEffect_->DirectionalLight0.setDirectionProperty(
        daylightDirection(0.0));
    materialEffect_->DirectionalLight0.setDiffuseColorProperty(Vector3{0.46f,0.43f,0.37f});
    materialEffect_->DirectionalLight0.setSpecularColorProperty(Vector3::Zero);
    materialEffect_->DirectionalLight1.setEnabledProperty(false);
    materialEffect_->DirectionalLight2.setEnabledProperty(false);
    materialEffect_->setTextureProperty(terrainDetailTexture_.get());
    materialEffect_->setTextureEnabledProperty(true);
    materialEffect_->setFogEnabledProperty(true);
    materialEffect_->setFogColorProperty(rgb(daylightSkyColor(0)));
    materialEffect_->setFogStartProperty(360.0f);
    materialEffect_->setFogEndProperty(1'050.0f);
    buildInterior();

    MeshBuilder cube;
    cube.addBox(Vector3{-0.5f,-0.5f,-0.5f}, Vector3{0.5f,0.5f,0.5f}, Color::White);
    cube_ = std::make_unique<GpuMesh>(device_, cube.take());

    MeshBuilder sky;
    addDaylightSky(sky,1'070.0f);

    sky_ = std::make_unique<GpuMesh>(device_, sky.take());

    MeshBuilder clouds;
    constexpr float tau = 6.2831853071795864769f;
    struct CloudBillboard { float angle; float y; float width; float height; };
    constexpr std::array<CloudBillboard,8> cloudLayout = {{
        {tau*0.03f,155.0f,184.0f,78.0f}, {tau*0.36f,245.0f,165.0f,128.0f},
        {tau*0.53f,108.0f,325.0f,105.0f}, {tau*0.86f,172.0f,220.0f,98.0f},
        {tau*0.19f,105.0f,260.0f,85.0f}, {tau*0.70f,195.0f,118.0f,105.0f},
        {tau*0.11f, 75.0f,390.0f,90.0f}, {tau*0.62f, 95.0f,290.0f,83.0f}
    }};
    skyCloudBillboardCount_ = graphicsQuality_ == 0 ? 4 : graphicsQuality_ == 1 ? 6 : 8;
    constexpr float cloudRadius = 925.0f;
    for (int index = 0; index < skyCloudBillboardCount_; ++index)
    {
        const CloudBillboard& cloud = cloudLayout[static_cast<std::size_t>(index)];
        const Vector3 radial{std::cos(cloud.angle),0.0f,std::sin(cloud.angle)};
        const Vector3 tangent{-std::sin(cloud.angle),0.0f,std::cos(cloud.angle)};
        const Vector3 centre = radial*cloudRadius+Vector3{0.0f,cloud.y,0.0f};
        const Vector3 lowerRight = centre+tangent*(cloud.width*0.5f)
                                 - Vector3::Up*(cloud.height*0.5f);
        const Vector3 lowerLeft = centre-tangent*(cloud.width*0.5f)
                                - Vector3::Up*(cloud.height*0.5f);
        const Vector3 upperLeft = centre-tangent*(cloud.width*0.5f)
                                + Vector3::Up*(cloud.height*0.5f);
        const Vector3 upperRight = centre+tangent*(cloud.width*0.5f)
                                 + Vector3::Up*(cloud.height*0.5f);
        const bool mirror = index%2 != 0;
        const float tileX=static_cast<float>(index%2)*0.5f;
        const float tileY=static_cast<float>((index%4)/2)*0.5f;
        const float left=tileX+(mirror ? 0.5f : 0.0f);
        const float right=tileX+(mirror ? 0.0f : 0.5f);
        clouds.addTexturedQuad(lowerRight,lowerLeft,upperLeft,upperRight,Color::White,
            Vector2{left,tileY+0.5f},Vector2{right,tileY+0.5f},
            Vector2{right,tileY},Vector2{left,tileY});
    }
    clouds_ = std::make_unique<GpuMesh>(device_,clouds.take());

    MeshBuilder sun;
    sun.addDiscXY(Vector3::Zero, 1.0f, 24, Color(255,238,166,255));
    sun_ = std::make_unique<GpuMesh>(device_, sun.take());

    // Road traffic uses six color variants of the same compact mesh. Each vehicle remains one
    // submission, but the silhouette now reads as a car rather than a moving cuboid.
    roadVehicles_.reserve(6);
    for (std::uint32_t variant = 0; variant < 6; ++variant)
    {
        MeshBuilder car;
        const Color body = roadVehicleColor(variant);
        const Color glass(42,72,84,255);
        const Color rubber(29,31,31,255);
        addRoadCarBody(car,body);
        // A tapered shell leaves the sloping glazing outside the body. A solid upright cabin
        // previously intersected both windscreens and hid their upper halves.
        car.addRoundedBoxXZ(Vector3{-0.64f,1.50f,-0.92f},
                            Vector3{0.64f,1.58f,0.96f},0.12f,2,
                            body);
        for (float side : {-1.0f, 1.0f})
        {
            const auto sidePanel = [&](float lowerY, float upperY, float lowerBack,
                                       float lowerFront, float upperBack, float upperFront,
                                       float outward, const Color& colour,bool glazing=false) {
                const auto xAt = [&](float y) {
                    return side*(0.76f-(y-1.02f)*0.12f/0.48f+outward);
                };
                const Vector3 a{xAt(lowerY),lowerY,lowerBack};
                const Vector3 b{xAt(lowerY),lowerY,lowerFront};
                const Vector3 c{xAt(upperY),upperY,upperFront};
                const Vector3 d{xAt(upperY),upperY,upperBack};
                const Color lower=glazing ? Color(32,46,51,255) : colour;
                const Color upper=glazing ? Color(91,119,133,255) : colour;
                if (side > 0.0f) car.addQuad(d,c,b,a,upper,upper,lower,lower);
                else car.addQuad(a,b,c,d,lower,lower,upper,upper);
            };
            sidePanel(1.02f,1.50f,-1.18f,1.18f,-0.92f,0.92f,0.0f,body);
            sidePanel(1.08f,1.44f,-1.08f,-0.055f,-0.88f,-0.055f,0.008f,glass,true);
            sidePanel(1.08f,1.44f,0.055f,1.08f,0.055f,0.88f,0.008f,glass,true);
            sidePanel(1.02f,1.06f,-1.16f,1.16f,-1.14f,1.14f,0.012f,Color(60,65,64,255));
            for (float doorZ : {-0.75f,0.40f})
                car.addBox({std::min(side*0.901f,side*0.923f),0.84f,doorZ},
                           {std::max(side*0.901f,side*0.923f),0.874f,doorZ+0.16f},
                           Color(174,179,176,255));
            car.addBox({std::min(side*0.901f,side*0.904f),0.37f,-0.009f},
                       {std::max(side*0.901f,side*0.904f),1.015f,0.009f},Color(48,53,51,255));
            for (float axle : {-1.30f, 1.30f})
            {
                const float wheelX = side * 0.88f;
                car.addRoundedBoxYZ(Vector3{wheelX-0.12f,0.0f,axle-0.30f},
                                    Vector3{wheelX+0.12f,0.60f,axle+0.30f},
                                    0.30f,3,rubber);
                const float hubInner = side < 0.0f ? -1.008f : 0.998f;
                const float hubOuter = side < 0.0f ? -0.998f : 1.008f;
                car.addRoundedBoxYZ(Vector3{hubInner,0.12f,axle-0.18f},
                                    Vector3{hubOuter,0.48f,axle+0.18f},
                                    0.18f,3,Color(48,54,53,255));
                addRoadWheelHub(car,side,axle);
            }
            const float mirrorInner = side < 0.0f ? -0.98f : 0.80f;
            const float mirrorOuter = side < 0.0f ? -0.80f : 0.98f;
            car.addRoundedBoxXZ(Vector3{mirrorInner,1.11f,0.55f},
                                Vector3{mirrorOuter,1.28f,0.82f},0.05f,1,
                                body);
        }
        for (float end : {-1.0f,1.0f})
        {
            const auto endPanel = [&](float lowerY, float upperY, float lowerWidth,
                                      float upperWidth, float outward, const Color& colour,bool glazing=false) {
                const auto zAt = [&](float y) {
                    return end*(1.18f-(y-1.02f)*0.26f/0.48f+outward);
                };
                const Vector3 a{-lowerWidth,lowerY,zAt(lowerY)};
                const Vector3 b{-upperWidth,upperY,zAt(upperY)};
                const Vector3 c{upperWidth,upperY,zAt(upperY)};
                const Vector3 d{lowerWidth,lowerY,zAt(lowerY)};
                const Color lower=glazing ? Color(32,46,51,255) : colour;
                const Color upper=glazing ? Color(91,119,133,255) : colour;
                if (end > 0.0f) car.addQuad(d,c,b,a,lower,upper,upper,lower);
                else car.addQuad(a,b,c,d,lower,upper,upper,lower);
            };
            endPanel(1.02f,1.50f,0.76f,0.64f,0.0f,body);
            endPanel(1.08f,1.44f,0.665f,0.575f,0.008f,glass,true);
        }
        car.addBox(Vector3{-0.72f,0.43f,2.055f}, Vector3{-0.37f,0.65f,2.085f},
                   Color(247,231,174,255));
        car.addBox(Vector3{0.37f,0.43f,2.055f}, Vector3{0.72f,0.65f,2.085f},
                   Color(247,231,174,255));
        car.addBox(Vector3{-0.72f,0.43f,-2.085f}, Vector3{-0.37f,0.65f,-2.055f},
                   Color(190,43,37,255));
        car.addBox(Vector3{0.37f,0.43f,-2.085f}, Vector3{0.72f,0.65f,-2.055f},
                   Color(190,43,37,255));
        car.addBox(Vector3{-0.94f,0.25f,-2.12f}, Vector3{0.94f,0.36f,-1.98f},
                   Color(54,58,58,255));
        car.addBox(Vector3{-0.94f,0.25f,1.98f}, Vector3{0.94f,0.36f,2.12f},
                   Color(54,58,58,255));
        car.addBox({-0.31f,0.38f,2.055f},{0.31f,0.52f,2.089f},Color(38,43,43,255));
        for (float end : {-1.0f,1.0f})
            car.addBox({-0.22f,0.56f,std::min(end*2.086f,end*2.096f)},
                       {0.22f,0.66f,std::max(end*2.086f,end*2.096f)},Color(213,214,203,255));
        roadVehicles_.push_back(std::make_unique<GpuMesh>(device_, car.take()));
    }

    MeshBuilder otherTrain;
    MeshBuilder pantograph;
    MeshBuilder frontCab;
    MeshBuilder rearCab;
    const float centre = 0.0f;
    {
        const Color bodyRed(188,52,48,255);
        const Color bodyCream(226,225,211,255);
        const Color windowBlue(43,79,96,255);
        const Color underframe(47,49,49,255);
        otherTrain.addBox(Vector3{-1.38f,0.28f,centre-8.8f},
                          Vector3{1.38f,3.45f,centre+8.8f}, bodyRed);
        otherTrain.addBox(Vector3{-1.40f,3.18f,centre-8.55f},
                          Vector3{1.40f,3.62f,centre+8.55f}, bodyCream);
        for (float side : {-1.0f, 1.0f})
        {
            const float inner = side < 0.0f ? -1.405f : 1.385f;
            const float outer = side < 0.0f ? -1.385f : 1.405f;
            otherTrain.addBox(Vector3{inner,1.10f,centre-8.25f},
                              Vector3{outer,3.12f,centre+8.25f}, bodyCream);
            otherTrain.addBox(Vector3{inner,0.88f,centre-8.30f},
                              Vector3{outer,1.25f,centre+8.30f}, Color(139,38,38,255));
            for (float windowZ : {-5.65f,-4.05f,-2.45f,-0.85f,0.85f,2.45f,4.05f,5.65f})
                otherTrain.addBox(Vector3{inner-0.004f,1.66f,centre+windowZ-0.57f},
                                  Vector3{outer+0.004f,2.70f,centre+windowZ+0.57f}, windowBlue);
            for (float doorZ : {-7.22f, 7.22f})
            {
                otherTrain.addBox(Vector3{inner-0.006f,0.52f,centre+doorZ-0.56f},
                                  Vector3{outer+0.006f,2.96f,centre+doorZ+0.56f},
                                  Color(197,69,58,255));
                otherTrain.addBox(Vector3{inner-0.010f,1.70f,centre+doorZ-0.34f},
                                  Vector3{outer+0.010f,2.60f,centre+doorZ+0.34f}, windowBlue);
            }
        }
        otherTrain.addBox(Vector3{-1.08f,0.08f,centre-6.9f},
                          Vector3{1.08f,0.34f,centre+6.9f}, underframe);
        for (float bogieZ : {-5.65f, 5.65f})
        {
            otherTrain.addBox(Vector3{-1.12f,0.02f,centre+bogieZ-0.82f},
                              Vector3{1.12f,0.31f,centre+bogieZ+0.82f}, Color(39,42,42,255));
            for (float axleZ : {-0.48f, 0.48f})
                for (float side : {-1.0f, 1.0f})
                {
                    const float wheelX = side * 1.18f;
                    otherTrain.addBox(Vector3{wheelX-0.12f,0.00f,centre+bogieZ+axleZ-0.27f},
                                      Vector3{wheelX+0.12f,0.48f,centre+bogieZ+axleZ+0.27f},
                                      Color(30,32,32,255));
                }
        }
        for (float end : {-1.0f, 1.0f})
        {
            const float endZ = centre + end * 8.82f;
            const float couplerEndZ = endZ + end * 0.42f;
            otherTrain.addBox(Vector3{-0.72f,1.36f,endZ-0.035f},
                              Vector3{0.72f,2.76f,endZ+0.035f}, Color(34,43,46,255));
            otherTrain.addBox(Vector3{-0.22f,0.38f,std::min(endZ,couplerEndZ)},
                              Vector3{0.22f,0.58f,std::max(endZ,couplerEndZ)}, underframe);
        }
        otherTrain.addBox(Vector3{-0.74f,3.57f,centre-1.85f},
                          Vector3{0.74f,3.82f,centre+1.85f}, Color(105,110,108,255));

        // A raised lightweight pantograph visually connects the centre motor car to the 5.85 m
        // contact wire. Two fore/aft arm planes and a broad carbon strip form one reusable mesh;
        // moving and parked consists submit it only for their centre car.
        const Color pantographSteel(67,72,71,255);
        pantograph.addBox(Vector3{-0.72f,3.84f,centre-0.68f},
                          Vector3{0.72f,3.94f,centre-0.54f},pantographSteel);
        pantograph.addBox(Vector3{-0.72f,3.84f,centre+0.54f},
                          Vector3{0.72f,3.94f,centre+0.68f},pantographSteel);
        const auto addPantographArm = [&pantograph,&pantographSteel](
            float baseX, float topX, float z) {
            constexpr float baseY = 3.94f;
            constexpr float topY = 5.67f;
            const float deltaX = topX-baseX;
            const float deltaY = topY-baseY;
            const float length = std::hypot(deltaX,deltaY);
            const float angle = std::atan2(deltaY,deltaX);
            const Matrix placement = Matrix::CreateRotationZ(angle)
                * Matrix::CreateTranslation((baseX+topX)*0.5f,
                                            (baseY+topY)*0.5f,z);
            pantograph.addBox(Vector3{-length*0.5f,-0.045f,-0.045f},
                              Vector3{length*0.5f,0.045f,0.045f},
                              pantographSteel,placement);
        };
        for (float z : {centre-0.58f,centre+0.58f})
        {
            addPantographArm(-0.62f,0.0f,z);
            addPantographArm(0.62f,0.0f,z);
        }
        pantograph.addBox(Vector3{-0.08f,5.62f,centre-0.66f},
                          Vector3{0.08f,5.73f,centre+0.66f},pantographSteel);
        pantograph.addBox(Vector3{-1.12f,5.78f,centre-0.09f},
                          Vector3{1.12f,5.86f,centre+0.09f},Color(44,48,47,255));

        // The reusable passenger-car shell intentionally has gangway faces at both ends. Short
        // environmental consists receive these overlays only on their two exposed ends, turning
        // them into a plausible double-ended EMU without duplicating the comparatively heavy car
        // mesh. Both overlays are authored at local +Z; the trailing one is rotated into place.
        const auto addDrivingCab = [&bodyRed,&bodyCream,&windowBlue](
            MeshBuilder& cab, const Color& markerLight, bool leading) {
            constexpr float end = 8.78f;
            cab.addBox(Vector3{-1.38f,0.30f,end},
                       Vector3{1.38f,3.18f,end+0.28f},bodyRed);
            cab.addBox(Vector3{-1.37f,2.88f,end+0.01f},
                       Vector3{1.37f,3.52f,end+0.23f},bodyCream);
            cab.addBox(Vector3{-1.02f,1.88f,end+0.285f},
                       Vector3{1.02f,2.91f,end+0.335f},windowBlue);
            cab.addBox(Vector3{-0.035f,1.87f,end+0.337f},
                       Vector3{0.035f,2.92f,end+0.347f},Color(31,38,41,255));
            cab.addBox(Vector3{-0.76f,3.02f,end+0.245f},
                       Vector3{0.76f,3.29f,end+0.305f},Color(27,37,40,255));
            // Amber pixels suggest a destination display without adding a dynamic text draw.
            for (int pixel = 0; pixel < 7; ++pixel)
            {
                const float x = -0.53f + static_cast<float>(pixel) * 0.175f;
                cab.addBox(Vector3{x,3.105f,end+0.307f},
                           Vector3{x+0.095f,3.185f,end+0.318f},Color(238,171,47,255));
            }
            cab.addBox(Vector3{-1.10f,0.31f,end+0.27f},
                       Vector3{1.10f,0.77f,end+0.48f},Color(119,34,35,255));
            cab.addBox(Vector3{-1.15f,0.17f,end+0.22f},
                       Vector3{1.15f,0.36f,end+0.53f},Color(42,46,46,255));
            for (float x : {-0.78f,0.78f})
            {
                cab.addBox(Vector3{x-0.13f,0.66f,end+0.485f},
                           Vector3{x+0.13f,0.88f,end+0.535f},markerLight);
                if (leading)
                    cab.addBox(Vector3{x-0.055f,1.02f,end+0.34f},
                               Vector3{x+0.055f,1.13f,end+0.38f},Color(240,244,226,255));
            }
            // Side cab glazing makes the different vehicle type readable during a close pass.
            for (float side : {-1.0f,1.0f})
            {
                const float inner = side < 0.0f ? -1.405f : 1.385f;
                const float outer = side < 0.0f ? -1.385f : 1.405f;
                cab.addBox(Vector3{inner,1.91f,7.91f},
                           Vector3{outer,2.82f,8.57f},windowBlue);
            }
            // Two restrained wipers break up the otherwise flat windscreen.
            for (float x : {-0.43f,0.43f})
            {
                const Matrix wiper = Matrix::CreateRotationZ(x < 0.0f ? -0.53f : 0.53f)
                    * Matrix::CreateTranslation(x,2.16f,end+0.353f);
                cab.addBox(Vector3{-0.34f,-0.018f,-0.012f},
                           Vector3{0.34f,0.018f,0.012f},Color(21,25,26,255),wiper);
            }
        };
        addDrivingCab(frontCab,Color(250,239,181,255),true);
        addDrivingCab(rearCab,Color(211,40,38,255),false);
    }
    otherTrain_ = std::make_unique<GpuMesh>(device_, otherTrain.take());
    otherTrainPantograph_ = std::make_unique<GpuMesh>(device_, pantograph.take());
    otherTrainFrontCab_ = std::make_unique<GpuMesh>(device_, frontCab.take());
    otherTrainRearCab_ = std::make_unique<GpuMesh>(device_, rearCab.take());

    MeshBuilder crossingBarrier;
    constexpr float crossingBarrierLength = 4.8f;
    constexpr int crossingBarrierBands = 8;
    for (int band = 0; band < crossingBarrierBands; ++band)
    {
        const float begin = crossingBarrierLength
            * static_cast<float>(band)/static_cast<float>(crossingBarrierBands);
        const float end = crossingBarrierLength
            * static_cast<float>(band+1)/static_cast<float>(crossingBarrierBands);
        crossingBarrier.addBox(Vector3{-0.065f,-0.065f,begin},
                               Vector3{0.065f,0.065f,end},
                               band%2 == 0 ? Color(236,230,207,255)
                                           : Color(207,52,45,255));
    }
    crossingBarrier.addBox(Vector3{-0.13f,-0.16f,-0.14f},
                           Vector3{0.13f,0.16f,0.17f},Color(69,72,69,255));
    levelCrossingBarrier_ = std::make_unique<GpuMesh>(device_,crossingBarrier.take());

    MeshBuilder contactShadow;
    contactShadow.addDiscXZ(Vector3::Zero, 1.0f, 16, Color(8,12,14,82));
    contactShadow_ = std::make_unique<GpuMesh>(device_, contactShadow.take());

    // A compact wheeled case gives selected platform passengers a travel purpose. One shared
    // mesh keeps the detail inexpensive even at a major station; the colored tag and extended
    // handle remain readable from a window without relying on textures.
    MeshBuilder platformLuggage;
    // Two moulded shells meet at a narrow dark zipper gasket. Shallow vertical ribs break up
    // the face without the former oversized horizontal bars or sharp crate-like corners.
    platformLuggage.addRoundedBoxXY(Vector3{-0.23f,0.08f,-0.14f},
                           Vector3{0.23f,0.64f,0.14f},0.055f,3,Color(43,69,91,255));
    platformLuggage.addRoundedBoxXY(Vector3{-0.233f,0.077f,-0.008f},
                           Vector3{0.233f,0.643f,0.008f},0.055f,3,Color(27,32,37,255));
    for (float rib : {-0.14f,-0.07f,0.0f,0.07f,0.14f})
        for (float face : {-1.0f,1.0f})
            platformLuggage.addRoundedBoxXY(Vector3{rib-0.012f,0.17f,face*0.144f-0.006f},
                Vector3{rib+0.012f,0.55f,face*0.144f+0.006f},0.011f,2,Color(58,82,102,255));
    for (float x : {-0.15f,0.15f})
    {
        platformLuggage.addBox(Vector3{x-0.010f,0.61f,-0.07f},
                               Vector3{x+0.010f,0.91f,-0.05f},Color(149,158,163,255));
        for (float z : {-0.105f,0.105f})
            platformLuggage.addEllipsoid(Vector3{x,0.045f,z},
                Vector3{0.028f,0.045f,0.045f},8,4,Color(30,33,34,255),true);
    }
    platformLuggage.addRoundedBoxXY(Vector3{-0.17f,0.89f,-0.08f},
                           Vector3{0.17f,0.93f,-0.035f},0.018f,3,Color(52,57,58,255));
    platformLuggage.addBox(Vector3{0.145f,0.34f,-0.151f},
                           Vector3{0.215f,0.48f,-0.145f},Color(224,180,46,255));
    platformLuggage_ = std::make_unique<GpuMesh>(device_,platformLuggage.take());

    // A tiny shared handset supports a restrained standing phone-check idle. Its luminous screen
    // and side key remain legible at saloon distance without a texture or unique passenger mesh.
    MeshBuilder passengerPhone;
    passengerPhone.addRoundedBoxXZ(Vector3{-0.075f,-0.14f,-0.018f},
                                    Vector3{0.075f,0.14f,0.018f},0.018f,2,
                                    Color(31,37,40,255));
    passengerPhone.addBox(Vector3{-0.060f,-0.105f,0.018f},
                          Vector3{0.060f,0.097f,0.024f},Color(58,117,139,255));
    passengerPhone.addBox(Vector3{-0.048f,0.068f,0.025f},
                          Vector3{0.048f,0.082f,0.028f},Color(133,207,218,255));
    passengerPhone.addBox(Vector3{0.074f,-0.025f,-0.010f},
                          Vector3{0.082f,0.035f,0.010f},Color(87,94,95,255));
    passengerPhone_ = std::make_unique<GpuMesh>(device_,passengerPhone.take());

    // One reusable staff handheld gives the conductor a visible task during inspections and
    // station duty. Housing, inset screen, status bar, keys, and wrist loop share one small mesh.
    MeshBuilder conductorHandheld;
    conductorHandheld.addRoundedBoxXZ(Vector3{-0.105f,-0.15f,-0.034f},
                                      Vector3{0.105f,0.15f,0.034f},0.025f,2,
                                      Color(38,48,53,255));
    conductorHandheld.addBox(Vector3{-0.078f,-0.055f,0.034f},
                             Vector3{0.078f,0.105f,0.041f},Color(67,133,151,255));
    conductorHandheld.addBox(Vector3{-0.064f,0.074f,0.042f},
                             Vector3{0.064f,0.091f,0.045f},Color(123,211,222,255));
    for (float keyX : {-0.052f,0.0f,0.052f})
        conductorHandheld.addBox(Vector3{keyX-0.016f,-0.115f,0.035f},
                                 Vector3{keyX+0.016f,-0.083f,0.043f},
                                 Color(197,184,116,255));
    conductorHandheld.addBox(Vector3{0.102f,-0.19f,-0.012f},
                             Vector3{0.122f,-0.13f,0.012f},Color(30,35,37,255));
    conductorHandheld_ = std::make_unique<GpuMesh>(device_,conductorHandheld.take());

    MeshBuilder passengerDoor;
    addPassengerDoorLeaf(passengerDoor);
    passengerDoor_ = std::make_unique<GpuMesh>(device_, passengerDoor.take());
    MeshBuilder passengerDoorButton;
    addPassengerDoorButtonFaces(passengerDoorButton);
    passengerDoorButton_=std::make_unique<GpuMesh>(device_,passengerDoorButton.take());

    // One shared retractable bridge plate closes the authored 20 cm horizontal platform gap at
    // every released doorway. Its outer yellow lip and three shallow anti-slip bars remain part
    // of this tiny reusable mesh rather than becoming per-door geometry allocations.
    MeshBuilder boardingStep;
    const float stepTravel = static_cast<float>(Sim::TrainInterior::BoardingStepTravel);
    constexpr float stepHalfLength = 0.70f;
    constexpr float innerTop = 0.018f;
    constexpr float outerTop = 0.004f;
    constexpr float stepThickness = 0.028f;
    const Color stepMetal(132,137,134,255);
    const Vector3 topA{0.0f,innerTop,-stepHalfLength};
    const Vector3 topB{stepTravel,outerTop,-stepHalfLength};
    const Vector3 topC{stepTravel,outerTop,stepHalfLength};
    const Vector3 topD{0.0f,innerTop,stepHalfLength};
    const Vector3 bottomA{0.0f,innerTop-stepThickness,-stepHalfLength};
    const Vector3 bottomB{stepTravel,outerTop-stepThickness,-stepHalfLength};
    const Vector3 bottomC{stepTravel,outerTop-stepThickness,stepHalfLength};
    const Vector3 bottomD{0.0f,innerTop-stepThickness,stepHalfLength};
    boardingStep.addQuad(topA,topB,topC,topD,stepMetal);
    boardingStep.addQuad(bottomD,bottomC,bottomB,bottomA,stepMetal);
    boardingStep.addQuad(topA,bottomA,bottomB,topB,stepMetal);
    boardingStep.addQuad(topB,bottomB,bottomC,topC,stepMetal);
    boardingStep.addQuad(topC,bottomC,bottomD,topD,stepMetal);
    boardingStep.addQuad(topD,bottomD,bottomA,topA,stepMetal);
    boardingStep.addBox(Vector3{stepTravel-0.070f,outerTop+0.001f,-stepHalfLength},
                        Vector3{stepTravel,outerTop+0.014f,stepHalfLength},
                        Color(235,184,43,255));
    for (float barZ : {-0.42f,0.0f,0.42f})
        for (int segment = 0; segment < 4; ++segment)
        {
            const float x0 = 0.025f + static_cast<float>(segment) * 0.045f;
            const float x1 = x0 + 0.038f;
            const float middle = (x0+x1)*0.5f;
            const float surface = innerTop + (outerTop-innerTop) * middle / stepTravel;
            boardingStep.addBox(Vector3{x0,surface+0.001f,barZ-0.018f},
                                Vector3{x1,surface+0.006f,barZ+0.018f},
                                Color(48,54,54,255));
        }
    boardingStep_ = std::make_unique<GpuMesh>(device_, boardingStep.take());

    MeshBuilder gangwayDoor;
    const Color gangwayFrame(59,82,91,255);
    gangwayDoor.addBox(Vector3{-0.29f,-1.10f,-0.035f},
                       Vector3{-0.235f,1.10f,0.035f},gangwayFrame);
    gangwayDoor.addBox(Vector3{0.235f,-1.10f,-0.035f},
                       Vector3{0.29f,1.10f,0.035f},gangwayFrame);
    gangwayDoor.addBox(Vector3{-0.235f,-1.10f,-0.035f},
                       Vector3{0.235f,-1.035f,0.035f},gangwayFrame);
    gangwayDoor.addBox(Vector3{-0.235f,1.035f,-0.035f},
                       Vector3{0.235f,1.10f,0.035f},gangwayFrame);
    gangwayDoor.addBox(Vector3{-0.235f,-0.98f,-0.018f},
                       Vector3{0.235f,0.98f,0.018f},Color(116,184,201,34));
    // A dark inner seal, brushed kick plate and compact request sensor give the shared leaf the
    // layered construction of a real inter-car door. These pieces deliberately stay within the
    // original 58 cm leaf envelope, so animation clearance and the walkable opening are unchanged.
    const Color gangwaySeal(25,34,37,255);
    gangwayDoor.addBox(Vector3{-0.235f,-0.99f,-0.041f},
                       Vector3{-0.215f,0.99f,0.041f},gangwaySeal);
    gangwayDoor.addBox(Vector3{0.215f,-0.99f,-0.041f},
                       Vector3{0.235f,0.99f,0.041f},gangwaySeal);
    gangwayDoor.addBox(Vector3{-0.215f,-0.99f,-0.041f},
                       Vector3{0.215f,-0.965f,0.041f},gangwaySeal);
    gangwayDoor.addBox(Vector3{-0.215f,0.965f,-0.041f},
                       Vector3{0.215f,0.99f,0.041f},gangwaySeal);
    gangwayDoor.addBox(Vector3{-0.212f,-0.955f,-0.045f},
                       Vector3{0.212f,-0.705f,0.045f},Color(142,149,148,255));
    for (float ribY : {-0.90f,-0.83f,-0.76f})
        gangwayDoor.addBox(Vector3{-0.18f,ribY-0.009f,-0.049f},
                           Vector3{0.18f,ribY+0.009f,0.049f},Color(85,94,95,255));
    gangwayDoor.addBox(Vector3{-0.235f,-0.04f,-0.045f},
                       Vector3{0.235f,0.035f,0.045f},Color(211,180,67,255));
    gangwayDoor.addBox(Vector3{-0.070f,0.67f,-0.048f},
                       Vector3{0.070f,0.84f,0.048f},Color(40,57,63,255));
    gangwayDoor.addBox(Vector3{-0.035f,0.745f,-0.052f},
                       Vector3{0.035f,0.795f,0.052f},Color(78,197,126,255));
    gangwayDoor_ = std::make_unique<GpuMesh>(device_, gangwayDoor.take());

    // One direction-aware luminous instrument cluster is placed in the operating cab at draw
    // time. The inactive desk keeps its physical bezels but no bright screens, which makes a
    // terminal reversal visible through the locked passenger bulkhead for one bounded draw.
    MeshBuilder cabActiveDisplay;
    cabActiveDisplay.addRoundedBoxXY(Vector3{-0.56f,-0.15f,-0.018f},
                                      Vector3{-0.05f,0.15f,0.018f},0.030f,2,
                                      Color(46,105,126,255));
    cabActiveDisplay.addRoundedBoxXY(Vector3{0.06f,-0.15f,-0.018f},
                                      Vector3{0.52f,0.15f,0.018f},0.030f,2,
                                      Color(36,83,105,255));
    cabActiveDisplay.addBox(Vector3{-0.51f,0.092f,-0.023f},
                            Vector3{-0.10f,0.119f,0.023f},Color(112,215,224,255));
    for (int bar = 0; bar < 5; ++bar)
    {
        const float x = 0.11f+static_cast<float>(bar)*0.071f;
        const float height = 0.035f+static_cast<float>((bar*3)%5)*0.016f;
        cabActiveDisplay.addBox(Vector3{x,-0.105f,-0.023f},
                                Vector3{x+0.038f,-0.105f+height,0.023f},
                                bar < 3 ? Color(78,208,136,255) : Color(237,183,50,255));
    }
    cabActiveDisplay_ = std::make_unique<GpuMesh>(device_,cabActiveDisplay.take());

    MeshBuilder door;
    door.addBox(Vector3{-0.04f,0.0f,-0.52f}, Vector3{0.04f,2.18f,0.52f}, Color(55,73,83,255));
    toiletDoor_ = std::make_unique<GpuMesh>(device_, door.take());

    if (avatarRenderingEnabled)
    {
        avatars_.reserve(kTotalAvatarVisuals);
        onboardAvatarAssignments_.resize(kOnboardAvatarVisuals);
        readyAvatarScratch_.reserve(kOnboardAvatarVisuals);
        visiblePassengerScratch_.reserve(kOnboardAvatarVisuals + 8);
        addNextAvatarVisual();
    }
}

SceneRenderer::~SceneRenderer() = default;

bool SceneRenderer::avatarLoadingComplete() const noexcept
{
    if (avatars_.empty()) return true;
    const std::size_t target = totalAvatarBudget(graphicsQuality_);
    const AvatarVisual& newest = *avatars_.back();
    const AvatarRendererState state = newest.renderer->getStateProperty();
    // Unavailable is terminal and has no loader thread or deferred upload left to drain.
    if (state == AvatarRendererState::Unavailable) return true;
    if (state == AvatarRendererState::Loading || !newest.gpuPrimed
        || avatarBuildCooldownFrames_ > 0) return false;
    return avatars_.size() >= target;
}

void SceneRenderer::setVegetationDistance(float metres)
{
    const float clamped = std::clamp(metres, 250.0f, 1'000.0f);
    if (std::abs(clamped - vegetationDistanceMetres_) < 0.5f) return;
    vegetationDistanceMetres_ = clamped;
    worldAnchorChunk_ = -1;
}

void SceneRenderer::setGraphicsQuality(int quality)
{
    const int clamped = std::clamp(quality, 0, 2);
    if (clamped == graphicsQuality_) return;
    graphicsQuality_ = clamped;
    materialEffect_->setPreferPerPixelLightingProperty(graphicsQuality_ >= 1);
    trackAnchorDistance_ = -1.0;
    worldAnchorChunk_ = -1;
}

void SceneRenderer::rebuildTrack(const Sim::Route& route, double anchorDistance)
{
    constexpr double kSegmentLength = 6.0;
    constexpr double kTrackRadius = 1'000.0;
    MeshBuilder track;
    MeshBuilder trackBedMaterial;
    MeshBuilder railHeadMaterial;
    // Stable capacities avoid allocator growth/fragmentation every time the 2 km sleeper and rail
    // batch recentres. Excess capacity is modest compared with the resulting GPU buffers.
    track.reserve(420'000, 650'000);
    trackBedMaterial.reserve(4'000,6'000);
    railHeadMaterial.reserve(6'000,9'000);
    const double begin = std::max(0.0, anchorDistance - kTrackRadius);
    const double end = std::min(route.lengthMetres(), anchorDistance + kTrackRadius);
    const double fastenerRadius = graphicsQuality_ == 0 ? 360.0
                                : graphicsQuality_ == 1 ? 460.0 : 560.0;
    tracksideKilometrePosts_ = 0;
    trackFastenerAssemblies_ = 0;
    trackTurnoutPointMotors_ = 0;
    trackTurnoutBladeAssemblies_ = 0;
    const auto addTrackTopQuad = [&track](const Matrix& placement, float x0, float x1,
                                          float z0, float z1, float y,
                                          const Color& color) {
        track.addQuad(
            Vector3::Transform(Vector3{x0,y,z0},placement),
            Vector3::Transform(Vector3{x0,y,z1},placement),
            Vector3::Transform(Vector3{x1,y,z1},placement),
            Vector3::Transform(Vector3{x1,y,z0},placement),color);
    };
    const auto addRouteRailSegment = [&](double distance0, double lateral0,
                                         double distance1, double lateral1,
                                         float width, float height,
                                         const Color& color) {
        const LocalRoutePoint a = localRoutePoint(
            route,anchorDistance,distance0,lateral0);
        const LocalRoutePoint b = localRoutePoint(
            route,anchorDistance,distance1,lateral1);
        const float dx = b.x-a.x;
        const float dz = b.z-a.z;
        const float length = std::sqrt(dx*dx+dz*dz);
        if (length <= 0.01f) return;
        const float yaw = std::atan2(dx,dz);
        addOrientedBox(track,(a.x+b.x)*0.5f,(a.y+b.y)*0.5f+0.115f,
                       (a.z+b.z)*0.5f,width,height,length,yaw,color);
    };
    for (double segmentBegin = begin; segmentBegin < end; segmentBegin += kSegmentLength)
    {
        const double segmentEnd = std::min(end, segmentBegin + kSegmentLength);
        const double midpoint = (segmentBegin + segmentEnd) * 0.5;
        const float halfLength = static_cast<float>((segmentEnd - segmentBegin) * 0.5 + 0.08);
        const LocalRoutePoint point = localRoutePoint(route, anchorDistance, midpoint);
        const Matrix placement = Matrix::CreateRotationY(point.yaw)
                               * Matrix::CreateTranslation(point.x, point.y, point.z);
        std::array<float, 3> trackCentres = {
            0.0f, static_cast<float>(route.parallelTrackLateralMetres(midpoint)), 0.0f};
        std::size_t trackCentreCount = 2;
        if (const auto siding = stationSidingLateral(route, midpoint);
            siding.has_value() && std::abs(*siding) > 0.30)
            trackCentres[trackCentreCount++] = static_cast<float>(*siding);
        for (std::size_t trackIndex = 0; trackIndex < trackCentreCount; ++trackIndex)
        {
            const float trackCentre = trackCentres[trackIndex];
            // The formation box stays hidden under the crown; both ballast shoulders are sloped
            // textured quads down to a wobbling toe instead of a vertical dark curb.
            track.addBox(Vector3{trackCentre-2.2f,-0.39f,-halfLength},
                         Vector3{trackCentre+2.2f,-0.09f,halfLength},
                         Color(102,96,88,255), placement);
            constexpr float kBallastTileMetres = 0.88f;
            const Color ballastTint(132,124,113,255);
            const auto bedUV = [&](float x,double distance) {
                return Vector2{x/kBallastTileMetres,
                               static_cast<float>(distance)/kBallastTileMetres};
            };
            const auto bedPoint = [&](float x,float y,float z) {
                return Vector3::Transform(Vector3{x,y,z},placement);
            };
            const float crownLeft = trackCentre-kBallastCrownHalfWidth;
            const float crownRight = trackCentre+kBallastCrownHalfWidth;
            trackBedMaterial.addTexturedQuad(
                bedPoint(crownLeft,kBallastCrownHeight,-halfLength),
                bedPoint(crownLeft,kBallastCrownHeight,halfLength),
                bedPoint(crownRight,kBallastCrownHeight,halfLength),
                bedPoint(crownRight,kBallastCrownHeight,-halfLength),ballastTint,
                bedUV(crownLeft,segmentBegin),bedUV(crownLeft,segmentEnd),
                bedUV(crownRight,segmentEnd),bedUV(crownRight,segmentBegin));
            const float toeBegin = ballastShoulderToe(segmentBegin);
            const float toeEnd = ballastShoulderToe(segmentEnd);
            const Color shoulderTint(124,117,107,255);
            trackBedMaterial.addTexturedQuad(
                bedPoint(trackCentre-toeBegin,kBallastToeHeight,-halfLength),
                bedPoint(trackCentre-toeEnd,kBallastToeHeight,halfLength),
                bedPoint(crownLeft,kBallastCrownHeight,halfLength),
                bedPoint(crownLeft,kBallastCrownHeight,-halfLength),shoulderTint,
                bedUV(-toeBegin,segmentBegin),bedUV(-toeEnd,segmentEnd),
                bedUV(crownLeft,segmentEnd),bedUV(crownLeft,segmentBegin));
            trackBedMaterial.addTexturedQuad(
                bedPoint(crownRight,kBallastCrownHeight,-halfLength),
                bedPoint(crownRight,kBallastCrownHeight,halfLength),
                bedPoint(trackCentre+toeEnd,kBallastToeHeight,halfLength),
                bedPoint(trackCentre+toeBegin,kBallastToeHeight,-halfLength),shoulderTint,
                bedUV(crownRight,segmentBegin),bedUV(crownRight,segmentEnd),
                bedUV(toeEnd,segmentEnd),bedUV(toeBegin,segmentBegin));
            for (float railOffset : {-0.81f, 0.71f})
            {
                const float x = trackCentre + railOffset + 0.05f;
                // A rail is not one silver bar: the oxidised foot and slender web carry a broader
                // head, and only the wheel-polished running surface keeps the metallic highlight.
                addRailProfile(track,placement,x,halfLength);
                const Vector3 railA = Vector3::Transform(
                    Vector3{x-kRailHeadHalfWidth,kRailHeadTop+0.001f,-halfLength},placement);
                const Vector3 railB = Vector3::Transform(
                    Vector3{x-kRailHeadHalfWidth,kRailHeadTop+0.001f,halfLength},placement);
                const Vector3 railC = Vector3::Transform(
                    Vector3{x+kRailHeadHalfWidth,kRailHeadTop+0.001f,halfLength},placement);
                const Vector3 railD = Vector3::Transform(
                    Vector3{x+kRailHeadHalfWidth,kRailHeadTop+0.001f,-halfLength},placement);
                railHeadMaterial.addTexturedQuad(
                    railA,railB,railC,railD,Color(166,174,173,255),
                    Vector2{0.0f,0.0f},Vector2{0.0f,1.0f},
                    Vector2{1.0f,1.0f},Vector2{1.0f,0.0f});
            }
            for (float sleeper = -halfLength + 0.32f; sleeper < halfLength; sleeper += 0.72f)
            {
                const Matrix sleeperPlacement = Matrix::CreateTranslation(0.0f, 0.0f, sleeper)
                                                * placement;
                const std::int64_t sleeperIndex = static_cast<std::int64_t>(std::floor(
                    (midpoint+static_cast<double>(sleeper))/0.72));
                const std::array<Color,3> sleeperTones = {
                    Color(127,124,116,255),Color(117,116,111,255),
                    Color(137,132,121,255)};
                const std::size_t tone = static_cast<std::size_t>(std::abs(
                    sleeperIndex+static_cast<std::int64_t>(trackIndex)*17))
                    % sleeperTones.size();
                // Broad dark baseplates and paired rust-toned elastic clips make the rail/sleeper
                // connection legible from a nearby passenger window. Top faces are sufficient at
                // this scale and avoid turning thousands of fasteners into box-sized geometry.
                // Quality-scaled distance limits keep sub-pixel fittings out of the far corridor;
                // at the largest possible 250 m anchor offset Low still retains 110 m ahead.
                // The chamfered monoblock profile shares that radius; farther sleepers stay boxes.
                const double sleeperDistance = midpoint+static_cast<double>(sleeper);
                const bool closeSleeper = std::abs(sleeperDistance-anchorDistance) <= fastenerRadius;
                addConcreteSleeper(track,sleeperPlacement,trackCentre,sleeperTones[tone],
                                   closeSleeper);
                if (!closeSleeper) continue;
                for (float railOffset : {-0.81f,0.71f})
                {
                    const float centre = trackCentre+railOffset+0.05f;
                    addTrackTopQuad(sleeperPlacement,centre-0.15f,centre+0.15f,
                                    -0.115f,0.115f,0.006f,Color(62,66,65,255));
                    addTrackTopQuad(sleeperPlacement,centre-0.145f,centre-0.088f,
                                    -0.064f,0.064f,0.010f,Color(126,87,58,255));
                    addTrackTopQuad(sleeperPlacement,centre+0.088f,centre+0.145f,
                                    -0.064f,0.064f,0.010f,Color(126,87,58,255));
                    ++trackFastenerAssemblies_;
                }
            }
        }
    }

    // Major-station sidings already diverge smoothly from the main line. Explicit point motors,
    // drive rods and a low route indicator make both ends read as operable turnouts rather than
    // as decorative rails that silently merge. Absolute station-relative placement remains stable
    // across 250 m recenter boundaries and everything stays in the shared track submission.
    for (const Sim::Station& station : route.stations())
    {
        if (!station.major) continue;
        const double sidingSide = station.platformOnLeft ? 1.0 : -1.0;
        for (double approach : {-1.0,1.0})
        {
            const double motorDistance = station.distanceMetres+approach*600.0;
            if (motorDistance < begin || motorDistance >= end
                || motorDistance < 0.0 || motorDistance > route.lengthMetres())
                continue;
            const Sim::RoutePose motorPose = route.sample(motorDistance);
            if (motorPose.tunnel || motorPose.bridge) continue;
            const double motorLateral = sidingSide*1.62;
            const LocalRoutePoint pad = localRoutePoint(
                route,anchorDistance,motorDistance,motorLateral);
            addOrientedBox(track,pad.x,pad.y+0.035f,pad.z,
                           0.78f,0.11f,1.08f,pad.yaw,Color(118,117,109,255));
            addOrientedBox(track,pad.x,pad.y+0.19f,pad.z,
                           0.52f,0.24f,0.72f,pad.yaw,Color(69,76,74,255));
            const LocalRoutePoint rod = localRoutePoint(
                route,anchorDistance,motorDistance,sidingSide*0.82);
            addOrientedBox(track,rod.x,rod.y+0.11f,rod.z,
                           1.55f,0.055f,0.10f,rod.yaw,Color(96,82,67,255));
            const LocalRoutePoint indicator = localRoutePoint(
                route,anchorDistance,motorDistance+approach*0.34,motorLateral);
            addOrientedBox(track,indicator.x,indicator.y+0.49f,indicator.z,
                           0.11f,0.62f,0.11f,indicator.yaw,Color(54,60,59,255));
            addOrientedBox(track,indicator.x,indicator.y+0.82f,indicator.z,
                           0.20f,0.16f,0.30f,indicator.yaw,Color(229,190,49,255));

            // Two polished switch blades follow the authored siding blend from the stock rails
            // toward the diverging alignment. The frog is found from that same function at the
            // first gauge-width separation, so opposite platform sides need no guessed offset.
            const double bladeOuterDistance = motorDistance+approach*4.0;
            const double bladeInnerDistance = motorDistance-approach*18.0;
            const double bladeOuterLateral = stationSidingLateral(
                route,bladeOuterDistance).value_or(0.0);
            const double bladeInnerLateral = stationSidingLateral(
                route,bladeInnerDistance).value_or(0.0);
            for (double railOffset : {-0.81,0.71})
                addRouteRailSegment(
                    bladeOuterDistance,bladeOuterLateral+railOffset,
                    bladeInnerDistance,bladeInnerLateral+railOffset,
                    0.064f,0.070f,Color(150,157,156,255));

            double frogDistance = motorDistance;
            for (int metre = 0; metre <= 100; ++metre)
            {
                const double candidate = motorDistance
                    - approach*static_cast<double>(metre);
                const double sidingLateral = stationSidingLateral(
                    route,candidate).value_or(0.0);
                frogDistance = candidate;
                if (std::abs(sidingLateral) >= 1.52) break;
            }
            const double mainDivergingRail = sidingSide > 0.0 ? 0.71 : -0.81;
            const double branchInnerRailOffset = sidingSide > 0.0 ? -0.81 : 0.71;
            const LocalRoutePoint frogBase = localRoutePoint(
                route,anchorDistance,frogDistance,mainDivergingRail);
            addOrientedBox(track,frogBase.x,frogBase.y+0.035f,frogBase.z,
                           0.52f,0.045f,2.7f,frogBase.yaw,Color(55,60,59,255));
            const double frogOuterDistance = frogDistance+approach*3.2;
            const double frogInnerDistance = frogDistance-approach*3.2;
            addRouteRailSegment(
                frogOuterDistance,mainDivergingRail,
                frogInnerDistance,mainDivergingRail,
                0.072f,0.074f,Color(158,164,163,255));
            addRouteRailSegment(
                frogOuterDistance,
                stationSidingLateral(route,frogOuterDistance).value_or(0.0)
                    + branchInnerRailOffset,
                frogInnerDistance,
                stationSidingLateral(route,frogInnerDistance).value_or(0.0)
                    + branchInnerRailOffset,
                0.072f,0.074f,Color(151,158,157,255));
            ++trackTurnoutPointMotors_;
            ++trackTurnoutBladeAssemblies_;
        }
    }

    // Numbered kilometre posts turn the otherwise continuous procedural corridor into a railway
    // with measurable geography. They use absolute route alignment, stay on the main-line verge,
    // and join the existing 2 km track batch. Their inward face remains readable from passenger
    // windows in either travel direction without adding another draw call.
    const auto firstKilometre = static_cast<std::int64_t>(std::ceil(begin/1'000.0));
    const auto endKilometre = static_cast<std::int64_t>(std::ceil(end/1'000.0));
    for (std::int64_t kilometre = firstKilometre; kilometre < endKilometre; ++kilometre)
    {
        const double markerDistance = static_cast<double>(kilometre)*1'000.0;
        const Sim::RoutePose pose = route.sample(markerDistance);
        bool nearStation = false;
        for (const Sim::Station& station : route.stations())
            nearStation = nearStation
                || std::abs(station.distanceMetres-markerDistance) < 700.0;
        if (nearStation || pose.tunnel || pose.bridge) continue;

        constexpr double kMarkerLateral = 3.65;
        const LocalRoutePoint post = localRoutePoint(
            route,anchorDistance,markerDistance,kMarkerLateral);
        addOrientedBox(track,post.x,post.y+0.79f,post.z,
                       0.13f,1.58f,0.13f,post.yaw,Color(172,174,166,255));
        addOrientedBox(track,post.x,post.y+1.69f,post.z,
                       0.16f,0.97f,1.23f,post.yaw,Color(62,67,65,255));
        addOrientedBox(track,post.x,post.y+1.69f,post.z,
                       0.19f,0.84f,1.10f,post.yaw,Color(226,226,215,255));

        const std::string kilometres = std::to_string(kilometre);
        constexpr double kInwardFaceLateral = kMarkerLateral-0.105;
        addRouteSignText(track,route,anchorDistance,"KM",markerDistance,
                         kInwardFaceLateral,2.00f,Color(45,49,48,255));
        addRouteSignText(track,route,anchorDistance,kilometres,markerDistance,
                         kInwardFaceLateral,1.66f,Color(45,49,48,255));
        ++tracksideKilometrePosts_;
    }
    // CNA's graphics lifetime contract snapshots submitted buffer work, so replacing the owner
    // here is safe even when the renderer replays commands after this method returns.
    trackDetail_ = std::make_unique<GpuMesh>(device_, track.take());
    trackBedMaterial_ = std::make_unique<GpuMesh>(device_,trackBedMaterial.take());
    railHeadMaterial_ = std::make_unique<GpuMesh>(device_,railHeadMaterial.take());
    trackAnchorDistance_ = anchorDistance;
}

void SceneRenderer::rebuildStaticWorld(const Sim::Route& route,
                                       const World::StreamingWorld& world,
                                       double anchorDistance)
{
    const auto generationStart = std::chrono::steady_clock::now();
    RoutePointCache routePointCache;
    const auto localRoutePoint = [&routePointCache](const Sim::Route& sampledRoute,
        double reference,double distance,double lateral=0.0) {
        return routePointCache.point(sampledRoute,reference,distance,lateral);
    };
    // Retain the largest CPU staging allocation between chunk transitions. GpuMesh uploads
    // snapshot their input; clearing this staging buffer next time cannot alter a submitted mesh.
    if (!worldMeshScratch_)
    {
        worldMeshScratch_ = std::make_unique<MeshBuilder>();
        worldMeshScratch_->reserve(190'000,300'000);
    }
    MeshBuilder& mesh = *worldMeshScratch_;
    mesh.clear();
    MeshBuilder terrain;
    // These high-volume batches previously grew fresh vectors at every 250 m transition.
    // Retain one staging buffer per material, just like the opaque world batch. GPU uploads
    // snapshot the data; clear() keeps capacity but removes every previous chunk's vertices.
    if (!foliageMeshScratch_) foliageMeshScratch_=std::make_unique<MeshBuilder>();
    if (!needleMeshScratch_) needleMeshScratch_=std::make_unique<MeshBuilder>();
    MeshBuilder& foliageMaterial=*foliageMeshScratch_;
    MeshBuilder& needleMaterial=*needleMeshScratch_;
    foliageMaterial.clear();
    needleMaterial.clear();
    MeshBuilder leafSprays;
    MeshBuilder needleSprays;
    MeshBuilder roofMaterial;
    MeshBuilder& roofSurface=graphicsQuality_>=1 ? roofMaterial : mesh;
    MeshBuilder waterMaterial;
    MeshBuilder facadeMaterial;
    MeshBuilder renderedFacadeMaterial;
    MeshBuilder worldLuminaires;
    MeshBuilder groundShadows;
    MeshBuilder platformMaterial;
    MeshBuilder stationLettering;
    // Seven high-quality chunks, a nearby station, and dense rural vegetation fit below these
    // capacities on the authored route. Reserving once keeps streaming rebuilds allocation-light.
    terrain.reserve(4'000,6'000);
    waterMaterial.reserve(1'200,1'800);
    facadeMaterial.reserve(2'000,3'000);
    renderedFacadeMaterial.reserve(2'000,3'000);
    worldLuminaires.reserve(40'000, 60'000);
    groundShadows.reserve(2'000,3'000);
    platformMaterial.reserve(256,384);
    staticWorldLakePatches_ = 0;
    staticWorldLakeBankTrees_ = 0;
    staticWorldHayBaleStacks_ = 0;
    staticWorldRiverWaterSections_ = 0;
    staticWorldRiverBankClumps_ = 0;
    staticWorldStationWaterfrontSections_ = 0;
    staticWorldCatenaryDroppers_ = 0;
    staticWorldCatenaryFoundations_ = 0;
    staticWorldCatenaryMessengerSegments_ = 0;
    staticWorldCatenaryRegistrationAssemblies_ = 0;
    staticWorldLinesideEquipmentCabinets_ = 0;
    staticWorldTunnelLuminaires_ = 0;
    staticWorldTunnelSafetyNiches_ = 0;
    staticWorldTunnelLiningRings_ = 0;
    staticWorldTunnelMarkerPlates_ = 0;
    staticWorldBridgeSections_ = 0;
    staticWorldRiverBridgeSections_ = 0;
    staticWorldViaductSections_ = 0;
    staticWorldBridgeAbutments_ = 0;
    staticWorldBridgeParapetPosts_ = 0;
    staticWorldFencePosts_ = 0;
    staticWorldPlatformSurfaceJoints_ = 0;
    staticWorldPlatformCanopyLights_ = 0;
    staticWorldPlatformCanopyDrainageAssemblies_ = 0;
    staticWorldPlatformPlanters_ = 0;
    staticWorldPlatformPosterCases_ = 0;
    staticWorldPlatformEquipmentCabinets_ = 0;
    staticWorldPlatformBenchAssemblies_ = 0;
    staticWorldPlatformWeatheringMarks_ = 0;
    staticWorldStationFacadeWeatheringStreaks_ = 0;
    staticWorldPlatformBoundarySections_ = 0;
    staticWorldTracksideDrainageSections_ = 0;
    staticWorldTracksideVergeTufts_ = 0;
    staticWorldFieldBoundarySegments_ = 0;
    staticWorldDistantWoodlandTrees_ = 0;
    staticWorldDistantRidgeMasses_ = 0;
    staticWorldTreeBranchCues_ = 0;
    staticWorldConiferBranchTiers_ = 0;
    staticWorldUnderstoreyGroups_ = 0;
    staticWorldDistantSettlementBuildings_ = 0;
    staticWorldBuildingEndWallWindows_ = 0;
    staticWorldTowerEndWallWindows_ = 0;
    staticWorldDetailedWarehouses_ = 0;
    staticWorldIndustrialCompounds_ = 0;
    staticWorldIndustrialYardVehicles_ = 0;
    staticWorldIndustrialTrailers_ = 0;
    staticWorldIndustrialUtilityGroups_ = 0;
    staticWorldIndustrialEdgePlantingGroups_ = 0;
    staticWorldUrbanRoadEdgeMarkingSections_ = 0;
    staticWorldUrbanRoadDrainGrates_ = 0;
    staticWorldUrbanResidentialPlotSurfaces_ = 0;
    staticWorldUrbanStreetPockets_ = 0;
    staticWorldUrbanResidentialEntrancePaths_ = 0;
    const double anchorElevation = route.sample(anchorDistance).elevation;
    const auto addFacadeMaterial = [](
        MeshBuilder& target, const LocalRoutePoint& point, bool positiveLateral,
        float width, float lowerHeight, float upperHeight, float length, const Color& color) {
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
    };
    const auto addFacadeWindow = [this,&mesh](const LocalRoutePoint& point,
                                              float centreHeight, float height, float length,
                                              std::optional<unsigned> room=std::nullopt,
                                              std::optional<Color> frameTone=std::nullopt,
                                              bool stationRoom=false) {
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
        addOrientedBox(mesh,point.x,point.y+centreHeight-height*0.5f-0.055f,point.z,
                       0.18f,0.075f,length+0.30f,point.yaw,Color(155,153,144,255));
        if (graphicsQuality_ >= 1)
        {
            addOrientedBox(mesh,point.x,point.y+centreHeight+0.025f,point.z,
                           0.145f,height-0.07f,0.045f,point.yaw,frame);
            addOrientedBox(mesh,point.x,point.y+centreHeight+0.04f,point.z,
                           0.145f,0.045f,length-0.07f,point.yaw,frame);
        }
    };
    const auto addEndWallWindow = [this,&mesh](const LocalRoutePoint& point,
                                               float centreHeight, float width, float height,
                                               std::optional<unsigned> room=std::nullopt) {
        const Color frame(205,204,193,255);
        const Color glazing(47,80,94,255);
        const float detailDistance = graphicsQuality_ == 0 ? 260.0f
                                   : graphicsQuality_ == 1 ? 430.0f : 600.0f;
        if (std::abs(point.z) > detailDistance)
            addOrientedBox(mesh,point.x,point.y+centreHeight,point.z,
                           width,height,0.11f,point.yaw,glazing);
        else
        {
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
        ++staticWorldBuildingEndWallWindows_;
    };
    for (const World::Chunk& chunk : world.chunks())
    {
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
                staticWorldTracksideDrainageSections_ += 2;
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
            ++staticWorldLinesideEquipmentCabinets_;
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
            ++staticWorldLakePatches_;

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
                        staticWorldConiferBranchTiers_ += addLayeredConifer(
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
                    ++staticWorldLakeBankTrees_;
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
            ++staticWorldRiverWaterSections_;
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
                    ++staticWorldRiverBankClumps_;
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
                ++staticWorldUrbanRoadEdgeMarkingSections_;
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
                ++staticWorldUrbanRoadDrainGrates_;
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
                ++staticWorldCatenaryMessengerSegments_;
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
                ++staticWorldCatenaryDroppers_;
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
                staticWorldTunnelLiningRings_ += addTunnelLiningSegment(
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
                ++staticWorldTunnelSafetyNiches_;
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
                    ++staticWorldTunnelMarkerPlates_;
                }
                addTunnelWallPlate(mesh,tunnelPoint,nicheDistance-2.75,liningFace,inward,
                                   2.20f,0.46f,0.26f,Color(228,228,218,255));
                addRoutePrintedText(stationLettering,route,anchorDistance,
                    tunnelChainageLabel(nicheDistance),nicheDistance-2.75,
                    liningFace+inward*0.06,2.14f,Color(28,30,30,255),0.38f);
                ++staticWorldTunnelMarkerPlates_;
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
            ++staticWorldTunnelLuminaires_;

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

            ++staticWorldBridgeSections_;
            if (bridgePose.waterCrossing) ++staticWorldRiverBridgeSections_;
            else ++staticWorldViaductSections_;
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
                staticWorldBridgeParapetPosts_ += addBridgeParapet(
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
                ++staticWorldBridgeAbutments_;
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
                staticWorldFencePosts_ += addLinesideFence(
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
                    ++staticWorldTracksideVergeTufts_;
                }
            }
        }

        // Continuous terrain-following hedges divide the broad rural colour domains into
        // parcels. The 500 m absolute rhythm matches the tint cells above.
        double boundaryDistance = std::ceil(chunk.beginDistance/500.0)*500.0;
        for (; boundaryDistance < chunk.endDistance; boundaryDistance += 500.0)
        {
            const Sim::RoutePose boundaryPose = route.sample(boundaryDistance);
            bool nearStation = false;
            for (const Sim::Station& station : route.stations())
                nearStation = nearStation
                    || std::abs(station.distanceMetres-boundaryDistance) < 700.0;
            if (nearStation || boundaryPose.tunnel || boundaryPose.bridge
                || boundaryPose.urbanity >= 0.45 || boundaryPose.adjacentWaterSide != 0)
                continue;
            for (double side : {-1.0,1.0})
            {
                addTerrainHedge(graphicsQuality_>=1 ? foliageMaterial : mesh,[&](float x,float z) {
                    auto point=terrainSurfacePoint(boundaryDistance+z,side*60.0+x);
                    point.Y -= 0.08f; // Bury the base without flattening the sloping crown.
                    return point;
                },96.0f,1.10f,0.95f,Color(55,92,48,255),
                    static_cast<std::uint32_t>(boundaryDistance)+(side < 0 ? 31U : 0U),
                    graphicsQuality_>=1 ? &leafSprays : nullptr);
                staticWorldFieldBoundarySegments_ += 10; // Ten nominal 9.6 m spans, now joined.
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
                        staticWorldConiferBranchTiers_ += addLayeredConifer(
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
                    ++staticWorldDistantWoodlandTrees_;
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
                    ++staticWorldDistantRidgeMasses_;
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
                    ++staticWorldDistantSettlementBuildings_;
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
                ++staticWorldUrbanStreetPockets_;
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
                        const Vector3 sun=daylightDirection(featurePose.headingRadians);
                        const double height=4.4*scale;
                        addFootprint(height*sun.X/-sun.Y,height*sun.Z/-sun.Y,
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
                        ? houseEnvelope(scale) : feature.kind == World::FeatureKind::Townhouse
                        ? townhouseEnvelope(scale,feature.variant) : apartmentEnvelope(scale,feature.variant);
                    const double height = envelope.roofHeight;
                    const Vector3 sunlight = daylightDirection(featurePose.headingRadians);
                    const double dx = height*sunlight.X/-sunlight.Y;
                    const double dz = height*sunlight.Z/-sunlight.Y;
                    const auto shadowPoint = [&](double lateralOffset, double alongOffset) {
                        const double distance = feature.routeDistance+alongOffset;
                        const double lateral = feature.lateralMetres+lateralOffset;
                        auto p = terrainSurfacePoint(distance,lateral);
                        const bool house=feature.kind==World::FeatureKind::House;
                        const bool apartment=feature.kind==World::FeatureKind::Apartment;
                        const float plotWidth=house ? 10.0f : apartment ? 12.8f : 8.6f;
                        const float plotLength=house ? 12.4f : apartment ? 15.6f : 11.4f;
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
                        plotWidth = 10.0f; plotLength = 12.4f;
                        plotColour = feature.variant%2U == 0U
                            ? Color(78,100,70,255) : Color(86,106,76,255);
                        break;
                    case World::FeatureKind::Townhouse:
                        plotWidth = 8.6f; plotLength = 11.4f;
                        plotColour = feature.variant%2U == 0U
                            ? Color(91,105,82,255) : Color(99,109,89,255);
                        break;
                    case World::FeatureKind::Apartment:
                        plotWidth = 12.8f; plotLength = 15.6f;
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
                ++staticWorldUrbanResidentialPlotSurfaces_;
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
                ++staticWorldUrbanResidentialEntrancePaths_;
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
                    ++staticWorldHayBaleStacks_;
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
                        staticWorldConiferBranchTiers_ += addLayeredConifer(
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
                                ++staticWorldTreeBranchCues_;
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
                        if (placed > 0) ++staticWorldUnderstoreyGroups_;
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
                    const auto envelope=houseEnvelope(scale);
                    const Color facadeColor(188,166,135,255);
                    addFacadeMaterial(renderedFacadeMaterial,point,
                                      feature.lateralMetres > 0.0,envelope.width,0.0f,
                                      envelope.wallHeight,envelope.length,facadeColor);
                    addPitchedRoof(mesh,roofSurface,point.x,point.y+envelope.wallHeight,point.z,
                                   envelope.roofWidth,envelope.roofLength,
                                   envelope.roofHeight-envelope.wallHeight,point.yaw,
                                   Color(121,63,54,255),
                                   graphicsQuality_>=1 && std::abs(point.z)<450 ? envelope.width : 0.0f,
                                   graphicsQuality_>=1 && std::abs(point.z)<450 ? envelope.wallHeight : 0.0f);
                    const Matrix houseTransform = Matrix::CreateRotationY(point.yaw)
                                                * Matrix::CreateTranslation(
                                                    point.x,point.y,point.z);
                    const Vector3 chimney = Vector3::Transform(
                        Vector3{1.55f*scale,7.25f*scale,1.85f*scale},houseTransform);
                    addOrientedBox(mesh,chimney.X,chimney.Y,chimney.Z,
                                   0.58f*scale,2.25f*scale,0.72f*scale,point.yaw,
                                   Color(126,99,78,255));
                    addOrientedBox(mesh,chimney.X,chimney.Y+1.14f*scale,chimney.Z,
                                   0.72f*scale,0.15f*scale,0.86f*scale,point.yaw,
                                   Color(79,69,61,255));
                    const double facade = feature.lateralMetres
                        - std::copysign(3.55 * static_cast<double>(scale), feature.lateralMetres);
                    for (double zOffset : {-2.8, 0.0, 2.8})
                    {
                        LocalRoutePoint window = localRoutePoint(
                            route, anchorDistance,
                            feature.routeDistance + zOffset * static_cast<double>(scale), facade);
                        window.y = point.y;
                        for (float floorHeight : {2.0f, 4.25f})
                        {
                            // The ground-floor centre bay belongs to the entrance, not a window.
                            if (zOffset==0.0 && floorHeight==2.0f) continue;
                            addFacadeWindow(window,floorHeight*scale,
                                            0.78f*scale,1.15f*scale,
                                            roomSeed(zOffset,static_cast<int>(floorHeight*4)));
                        }
                    }
                    LocalRoutePoint door = localRoutePoint(
                        route, anchorDistance, feature.routeDistance, facade);
                    door.y = point.y;
                    addOrientedBox(mesh,door.x,door.y+0.95f*scale,door.z,
                                   0.12f,1.9f*scale,0.95f*scale,door.yaw,
                                   Color(91,64,45,255));
                    if (graphicsQuality_>=1 && std::abs(point.z)<250.0f)
                        addResidentialDoorKit(mesh,0.95f*scale,1.9f*scale,
                            Matrix::CreateRotationY(door.yaw+1.570796327f)
                                *Matrix::CreateTranslation(door.x,door.y,door.z));
                    addUrbanEntrancePath(feature.routeDistance,facade,
                                         2.6f*scale,1.30f*scale);
                    for (double end : {-1.0,1.0})
                        for (double lateralOffset : {-1.65,1.65})
                        {
                            LocalRoutePoint endWindow = localRoutePoint(
                                route,anchorDistance,
                                feature.routeDistance+end*5.05*static_cast<double>(scale),
                                feature.lateralMetres
                                    +lateralOffset*static_cast<double>(scale));
                            endWindow.y = point.y;
                            for (float floorHeight : {2.0f,4.25f})
                                addEndWallWindow(endWindow,floorHeight*scale,
                                                0.82f*scale,1.10f*scale,
                                                roomSeed(lateralOffset+end*8,static_cast<int>(floorHeight*4)));
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
                    const std::array<Color, 3> facades = {
                        Color(164,154,141,255), Color(151,164,166,255), Color(178,159,145,255)};
                    const auto envelope=apartmentEnvelope(scale,feature.variant);
                    const float height = envelope.wallHeight;
                    const int floors = apartmentFloorCount(feature.variant);
                    const Color& facadeColor = facades[feature.variant % facades.size()];
                    addFacadeMaterial(renderedFacadeMaterial,point,
                                      feature.lateralMetres > 0.0,envelope.width,0.0f,
                                      height,envelope.length,facadeColor);
                    addOrientedBox(mesh,point.x,point.y+height+0.25f,point.z,
                                   envelope.roofWidth,envelope.roofHeight-height,
                                   envelope.roofLength,point.yaw,
                                   Color(72,77,77,255));
                    if (graphicsQuality_>=1 && std::abs(point.z)<450.0f)
                        addFlatRoofFinish(mesh,terrain,point.x,point.y+envelope.roofHeight,point.z,
                            envelope.roofWidth,envelope.roofLength,point.yaw,feature.variant);
                    const double facade = feature.lateralMetres
                        - std::copysign(5.05 * static_cast<double>(scale), feature.lateralMetres);
                    const double apartmentDoorOffset = feature.variant%2U == 0U
                        ? -0.62*static_cast<double>(scale)
                        : 0.62*static_cast<double>(scale);
                    for (int floor = 0; floor < floors; ++floor)
                        for (double zOffset : {-4.2, -1.4, 1.4, 4.2})
                        {
                            if (floor==0 && std::abs(zOffset*scale-apartmentDoorOffset)<1.26*scale)
                                continue;
                            LocalRoutePoint window = localRoutePoint(
                                route, anchorDistance,
                                feature.routeDistance + zOffset * static_cast<double>(scale), facade);
                            window.y = point.y;
                            addFacadeWindow(
                                window,(2.0f+static_cast<float>(floor)*2.35f)*scale,
                                0.85f*scale,0.90f*scale,roomSeed(zOffset,floor));
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
                                feature.routeDistance+end*6.55*static_cast<double>(scale),
                                feature.lateralMetres
                                    +lateralOffset*static_cast<double>(scale));
                            endWindow.y = point.y;
                            for (int floor = 0; floor < floors; ++floor)
                                addEndWallWindow(
                                    endWindow,(2.0f+static_cast<float>(floor)*2.35f)*scale,
                                    0.90f*scale,0.90f*scale,roomSeed(lateralOffset+end*8,floor));
                        }
                    break;
                }
                case World::FeatureKind::Tower:
                {
                    const std::array<Color,4> facades = {
                        Color(133,144,148,255),Color(152,145,137,255),
                        Color(124,139,145,255),Color(161,156,145,255)};
                    const Color& facadeColor = facades[feature.variant%facades.size()];
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
                                ++staticWorldTowerEndWallWindows_;
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
                            ++staticWorldTowerEndWallWindows_;
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
                    const std::array<Color,4> industrialWalls = {
                        Color(133,142,145,255),Color(145,143,132,255),
                        Color(124,143,141,255),Color(151,148,139,255)};
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
                            ++staticWorldIndustrialEdgePlantingGroups_;
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
                        ++staticWorldIndustrialYardVehicles_;

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
                            ++staticWorldIndustrialYardVehicles_;

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
                            ++staticWorldIndustrialYardVehicles_;
                            ++staticWorldIndustrialTrailers_;
                        }
                        ++staticWorldIndustrialCompounds_;
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
                    ++staticWorldIndustrialUtilityGroups_;
                    ++staticWorldDetailedWarehouses_;
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
                    ++staticWorldCatenaryRegistrationAssemblies_;
                    ++staticWorldCatenaryFoundations_;
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

    // Station furniture is authored, but joins the same static stream batch as its host chunks.
    if (!world.chunks().empty())
    {
        const double visibleBegin = world.chunks().front().beginDistance - 200.0;
        const double visibleEnd = world.chunks().back().endDistance + 200.0;
        for (std::size_t stationIndex = 0; stationIndex < route.stations().size(); ++stationIndex)
        {
            const Sim::Station& station = route.stations()[stationIndex];
            if (station.distanceMetres < visibleBegin || station.distanceMetres > visibleEnd) continue;
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
                staticWorldPlatformSurfaceJoints_+=addPlatformPaving(platformMaterial,
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
                ++staticWorldPlatformSurfaceJoints_;
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
                ++staticWorldPlatformSurfaceJoints_;
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
                    ++staticWorldPlatformWeatheringMarks_;
                }
            }
            const LocalRoutePoint canopy = localRoutePoint(
                route, anchorDistance, station.distanceMetres, side * 5.0);
            const float canopyLength = platformCanopyLength(terminal,station.major);
            if (graphicsQuality_ >= 1)
            {
                auto shadow=canopyShadowBounds(canopyLength,kPlatformSurfaceElevation,
                    daylightDirection(route.sample(station.distanceMetres).headingRadians));
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
                           4.6f,0.18f,canopyLength,canopy.yaw,Color(67,91,102,255));
            addCanopySoffit(mesh,{canopy.x,canopy.y,canopy.z},canopyLength,canopy.yaw,
                graphicsQuality_>=1 ? &platformMaterial : nullptr);
            for (double edge : {-1.0,1.0})
            {
                const LocalRoutePoint fascia = localRoutePoint(
                    route,anchorDistance,station.distanceMetres,side*(5.0+edge*2.25));
                addOrientedBox(mesh,fascia.x,fascia.y+3.52f,fascia.z,
                               0.12f,0.34f,canopyLength+0.16f,fascia.yaw,
                               Color(48,66,74,255));
                addOrientedBox(mesh,fascia.x,fascia.y+3.36f,fascia.z,
                               0.16f,0.11f,canopyLength+0.20f,fascia.yaw,
                               Color(54,65,67,255));
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
            ++staticWorldPlatformCanopyDrainageAssemblies_;
            const int canopyLightCount = std::max(
                3,static_cast<int>(std::floor(canopyLength/18.0f)));
            for (int light = 0; light < canopyLightCount; ++light)
            {
                const double offset=canopyLightOffset(canopyLength,light,canopyLightCount);
                const LocalRoutePoint panel = localRoutePoint(
                    route,anchorDistance,station.distanceMetres+offset,side*5.0);
                addCanopyLuminaire(mesh,worldLuminaires,Matrix::CreateRotationY(panel.yaw)
                    *Matrix::CreateTranslation(panel.x,panel.y,panel.z));
                ++staticWorldPlatformCanopyLights_;
            }
            for (double nominal = -canopyLength * 0.42; nominal <= canopyLength * 0.42;
                 nominal += canopySupportSpacing)
            {
                const double offset=canopySupportOffset(nominal,terminal,station.major);
                const LocalRoutePoint support = localRoutePoint(
                    route, anchorDistance, station.distanceMetres + offset, side * 5.0);
                addCanopyFrame(mesh,Matrix::CreateRotationY(support.yaw)
                    *Matrix::CreateTranslation(support.x,support.y,support.z),
                    -static_cast<float>(side)*0.20f,kPlatformSurfaceElevation);
            }

            const auto stationFamily=rural ? StationFacadeFamily::Rural
                : waterfront ? StationFacadeFamily::Waterfront
                : station.major ? StationFacadeFamily::Urban : StationFacadeFamily::Suburban;
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
                    ++staticWorldStationFacadeWeatheringStreaks_;
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
            else if (stationIndex == 3 || stationIndex == 5)
            {
                // Rural stops have a small transparent wind shelter and timber-toned roof.
                const LocalRoutePoint shelter = localRoutePoint(
                    route, anchorDistance, station.distanceMetres + 27.0, side * 5.0);
                addOrientedBox(mesh,shelter.x,shelter.y+1.75f,shelter.z,
                               2.2f,2.7f,7.2f,shelter.yaw,Color(91,151,164,255));
                addOrientedBox(mesh,shelter.x,shelter.y+3.25f,shelter.z,
                               2.8f,0.30f,8.0f,shelter.yaw,Color(112,79,48,255));
                for (double offset : {-3.0, 0.0, 3.0})
                {
                    const LocalRoutePoint frame = localRoutePoint(
                        route, anchorDistance, station.distanceMetres + 27.0 + offset,
                        side * 4.85);
                    addOrientedBox(mesh,frame.x,frame.y+1.75f,frame.z,
                                   0.14f,2.9f,0.14f,frame.yaw,Color(57,67,68,255));
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
                    ++staticWorldStationWaterfrontSections_;
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
            for (int benchIndex = 0; benchIndex < benchCount; ++benchIndex)
            {
                const double offset = benchOffsets[static_cast<std::size_t>(benchIndex)];
                const LocalRoutePoint bench = localRoutePoint(
                    route, anchorDistance, station.distanceMetres + offset, side * 4.15);
                addOrientedBox(mesh,bench.x,
                               bench.y+kPlatformSurfaceElevation+0.52f,bench.z,
                               0.70f,0.14f,2.4f,bench.yaw,benchSlat);
                addOrientedBox(mesh,bench.x,
                               bench.y+kPlatformSurfaceElevation+0.92f,bench.z+0.18f,
                               0.12f,0.75f,2.4f,bench.yaw,benchSlat);
                for (double legOffset : {-0.75, 0.75})
                {
                    const LocalRoutePoint leg = localRoutePoint(
                        route, anchorDistance, station.distanceMetres + offset + legOffset,
                        side * 4.15);
                    addOrientedBox(mesh,leg.x,
                                   leg.y+kPlatformSurfaceElevation+0.26f,leg.z,
                                   0.52f,0.52f,0.12f,leg.yaw,Color(69,70,66,255));
                }
                if (graphicsQuality_ >= 1)
                    addOrientedGroundQuad(
                        groundShadows,bench.x,
                        bench.y+kPlatformSurfaceElevation+0.018f,bench.z,
                        0.92f,2.72f,bench.yaw,Color(8,12,13,26));
                ++staticWorldPlatformBenchAssemblies_;
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
                ++staticWorldPlatformPosterCases_;
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
                ++staticWorldPlatformEquipmentCabinets_;
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
                    ++staticWorldPlatformPlanters_;
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
                    ++staticWorldPlatformBoundarySections_;
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

    const auto uploadStart = std::chrono::steady_clock::now();
    routePointCacheHits_ += routePointCache.hits();
    routePointCacheMisses_ += routePointCache.misses();
    const double generationMs = std::chrono::duration<double,std::milli>(
        uploadStart-generationStart).count();
    worldGenerationTotalMs_ += generationMs;
    worldGenerationMaxMs_ = std::max(worldGenerationMaxMs_,generationMs);
    const auto uploadStreamed = [this](std::unique_ptr<GpuMesh>& target,const MeshData& data) {
        if (data.empty()) target.reset();
        else if (target) worldBufferReuses_ += static_cast<std::uint64_t>(target->replace(device_,data));
        else target=std::make_unique<GpuMesh>(device_,data,true);
    };
    uploadStreamed(terrain_,terrain.data());
    uploadStreamed(foliageMaterial_,foliageMaterial.data());
    uploadStreamed(needleMaterial_,needleMaterial.data());
    uploadStreamed(leafSprays_,leafSprays.data());
    uploadStreamed(needleSprays_,needleSprays.data());
    uploadStreamed(roofMaterial_,roofMaterial.data());
    MeshData waterMaterialData = waterMaterial.take();
    uploadStreamed(waterMaterial_,waterMaterialData);
    MeshData facadeMaterialData = facadeMaterial.take();
    uploadStreamed(facadeMaterial_,facadeMaterialData);
    MeshData renderedFacadeMaterialData = renderedFacadeMaterial.take();
    uploadStreamed(renderedFacadeMaterial_,renderedFacadeMaterialData);
    uploadStreamed(staticWorld_,mesh.data());
    uploadStreamed(stationLettering_,stationLettering.data());
    MeshData shadowData = groundShadows.take();
    uploadStreamed(staticWorldShadows_,shadowData);
    MeshData platformMaterialData = platformMaterial.take();
    uploadStreamed(platformMaterial_,platformMaterialData);
    MeshData luminaireData = worldLuminaires.take();
    uploadStreamed(worldLuminaires_,luminaireData);
    worldAnchorDistance_ = anchorDistance;
    worldAnchorChunk_ = world.currentChunk();
    // CPU-side buffer creation/submission only. Deferred GPU execution belongs to draw/present.
    const double uploadMs = std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-uploadStart).count();
    worldUploadTotalMs_ += uploadMs;
    worldUploadMaxMs_ = std::max(worldUploadMaxMs_,uploadMs);
}

void SceneRenderer::addNextAvatarVisual()
{
    const std::size_t index = avatars_.size();
    auto visual = std::make_unique<AvatarVisual>();
    const AvatarBodyType body = index % 2 == 0 ? AvatarBodyType::Female : AvatarBodyType::Male;
    AvatarDescription description = AvatarDescription::CreateRandom(body);
    if (index == 0)
    {
        // Slot zero is the conductor's reserved visual. A native CNA hat selects the matching
        // hair-under-headwear mesh, avoiding the intersections an external rigid cap would cause.
        // Select, rather than alter, a generated description whose stock clothing palette also
        // supplies a consistent service blue for the top and accessory/headwear tint.
        AvatarDescription best = description;
        int bestScore = staffAppearanceScore(best);
        for (int attempt = 0; attempt < 16'383 && bestScore < 6; ++attempt)
        {
            AvatarDescription candidate = AvatarDescription::CreateRandom(body);
            const int score = staffAppearanceScore(candidate);
            if (score <= bestScore) continue;
            best = std::move(candidate);
            bestScore = score;
        }
        description = std::move(best);
    }
    else
    {
        // Random avatars are valuable for population variety, but unrestricted headwear can
        // produce construction-like helmets or novelty silhouettes that dominate a quiet train
        // scene. Select a civilian-looking result with ordinary hair and restrained lower-body
        // colours. This remains a choice among untouched CNA descriptions, not a dependency patch.
        AvatarDescription best = description;
        int bestScore = civilianAppearanceScore(best);
        // A bounded search prefers seat-compatible trousers over catalog-v3 skirts whose hem
        // stretches under the custom seated pose. Descriptions remain untouched; unknown
        // catalogs are not filtered. This is best-effort selection, not a cloth deformation fix.
        for (int attempt = 0; attempt < 127 && bestScore < 29; ++attempt)
        {
            AvatarDescription candidate = AvatarDescription::CreateRandom(body);
            const int score = civilianAppearanceScore(candidate);
            if (score <= bestScore) continue;
            best = std::move(candidate);
            bestScore = score;
        }
        description = std::move(best);
        ++civilianAvatarDescriptionCount_;
    }
    visual->description = std::make_unique<AvatarDescription>(std::move(description));
    visual->renderer = std::make_unique<AvatarRenderer>(visual->description.get(), false);
    visual->renderer->setAmbientLightColorProperty(Vector3{0.50f,0.50f,0.54f});
    visual->renderer->setLightColorProperty(Vector3{0.70f,0.67f,0.60f});
    visual->renderer->setLightDirectionProperty(Vector3{-0.35f,-0.65f,-0.55f});
    const auto parentBones = visual->renderer->getParentBonesProperty();
    for (std::size_t bone = 0; bone < visual->parentBones.size(); ++bone)
        visual->parentBones[bone] = parentBones[static_cast<int>(bone)];
    visual->animation = std::make_unique<AvatarAnimation>(avatarPreset(index));
    avatars_.push_back(std::move(visual));
}

void SceneRenderer::rebuildInformationDisplays(const Sim::Route& route,
                                               const Sim::ServiceState& service,
                                               int speedKilometresPerHour, int etaMinutes,
                                               int informationPage)
{
    const std::string next = service.phase == Sim::ServicePhase::Dwelling
        ? "At " + route.stations()[service.currentStation].name
        : "Next: " + route.stations()[service.nextStation].name;
    const std::string destination = "To " + route.stations()[
        service.direction > 0 ? route.stations().size() - 1 : 0].name;
    std::string progress;
    if (service.phase == Sim::ServicePhase::Dwelling)
    {
        progress = std::string("Doors open: ") + (service.platformOnLeft ? "left" : "right");
    }
    else
    {
        const int following = static_cast<int>(service.nextStation)
                            + service.direction * informationPage;
        if (informationPage > 0 && following >= 0
            && following < static_cast<int>(route.stations().size()))
            progress = std::string(informationPage == 1 ? "Then " : "After ")
                     + route.stations()[static_cast<std::size_t>(following)].name;
        else
            progress = std::to_string(speedKilometresPerHour)
                + " km/h   Arriving in " + std::to_string(etaMinutes) + " min";
    }
    MeshBuilder displays;
    for (int car = 0; car < 6; ++car)
    {
        const float z = kFirstCarCentre + static_cast<float>(car) * kCarLength;
        for (float face : {-1.0f, 1.0f})
        {
            const float surface = z + face * 0.102f;
            const bool mirrored = face < 0.0f;
            addPassengerScreenText(displays,next,2.652f,surface,mirrored,Color(246,232,193,255),0.0050f);
            addPassengerScreenText(displays,destination,2.523f,surface,mirrored,Color(166,211,220,255),0.0040f);
            addPassengerScreenText(displays,progress,2.405f,surface,mirrored,Color(186,204,210,255),0.0036f);
            addPassengerScreenText(displays,PassengerCarRoleLabels[static_cast<std::size_t>(car)],
                2.190f,z+face*0.064f,mirrored,Color(222,233,232,255),0.0038f,-0.85f,1.30f);
        }
    }
    informationDisplays_ = std::make_unique<GpuMesh>(device_, displays.take());
    informationDisplayState_ = InformationDisplayState{
        service.nextStation, service.direction,
        service.phase == Sim::ServicePhase::Dwelling,
        informationPage, speedKilometresPerHour, etaMinutes};
}

void SceneRenderer::rebuildPlatformInformation(const Sim::Route& route,
                                               const Sim::ServiceState& service,
                                               std::size_t stationIndex, int etaMinutes,
                                               double scheduleDeviationSeconds)
{
    const Sim::Station& station = route.stations()[stationIndex];
    const double anchor = station.distanceMetres;
    // Keep the live board readable from the fourth passenger car without placing it squarely in
    // front of the showcase seat. Real platform displays are set behind the principal waiting
    // line and are markedly smaller than an advertising hoarding; the offset also leaves a clear
    // sightline to waiting passengers and the station beyond.
    const bool terminal=stationIndex==0 || stationIndex+1==route.stations().size();
    const double centreDistance = station.distanceMetres + platformDisplayOffset(terminal,station.major);
    const double side = station.platformOnLeft ? -1.0 : 1.0;
    const double lateral = side * 6.20;
    const double faceLateral = lateral - side * 0.110;

    MeshBuilder board;
    MeshBuilder lettering;
    const LocalRoutePoint body = localRoutePoint(route, anchor, centreDistance, lateral);
    addPlatformDepartureBoard(board,Matrix::CreateRotationY(body.yaw-static_cast<float>(side)*kPi*0.5f)
        *Matrix::CreateTranslation(body.x,body.y,body.z),kPlatformSurfaceElevation);

    const std::string destination = "TO " + route.stations()[
        service.direction > 0 ? route.stations().size() - 1 : 0].name;
    const std::string platform = "P" + std::to_string(1 + stationIndex % 4);
    std::string status;
    if (stationIndex == service.currentStation
        && service.phase == Sim::ServicePhase::Dwelling)
        status = platform + "  NOW BOARDING";
    else if (stationIndex == service.currentStation
             && service.phase == Sim::ServicePhase::ClosingDoors)
        status = platform + "  DOORS CLOSING";
    else if (std::abs(scheduleDeviationSeconds) < 45.0)
        status = platform + "  ON TIME  DUE " + std::to_string(std::max(1, etaMinutes)) + " MIN";
    else
    {
        const int minutes = std::max(1, static_cast<int>(
            std::ceil(std::abs(scheduleDeviationSeconds) / 60.0)));
        status = platform + "  " + std::to_string(minutes)
               + (scheduleDeviationSeconds > 0.0 ? " MIN LATE" : " MIN EARLY");
    }
    addRoutePrintedText(lettering,route,anchor,"NORTHERN VALLEY EXPRESS",centreDistance,
                     faceLateral,platformDisplayFirstRow,Color(117,214,226,255),2.62f);
    addRoutePrintedText(lettering,route,anchor,destination,centreDistance,
                     faceLateral,platformDisplayFirstRow-0.32f,Color(242,243,235,255),2.62f);
    addRoutePrintedText(lettering,route,anchor,status,centreDistance,
                     faceLateral,platformDisplayFirstRow-0.64f,Color(244,190,47,255),2.62f);

    platformInformation_ = std::make_unique<GpuMesh>(device_, board.take());
    platformLettering_ = std::make_unique<GpuMesh>(device_,lettering.take());
    platformInformationState_ = PlatformInformationState{
        stationIndex, service.direction, static_cast<int>(service.phase), etaMinutes,
        static_cast<int>(std::round(scheduleDeviationSeconds / 60.0))};
}

void SceneRenderer::buildInterior()
{
    MeshBuilder mesh;
    MeshBuilder glass;
    std::array<MeshBuilder,2> glassReflections;
    MeshBuilder sunlight;
    MeshBuilder interiorMaterial;
    MeshBuilder upholsteryMaterial;
    MeshBuilder interiorPanelMaterial;
    MeshBuilder interiorResilientMaterial;
    MeshBuilder interiorShadows;
    MeshBuilder interiorLighting;
    MeshBuilder interiorEmitters;
    paSpeakerGrilleCount_ = 0;
    hvacVentCount_ = 0;
    carIdentitySignCount_ = 0;
    carFunctionFixtureGroupCount_ = 0;
    cabControlGroupCount_ = 0;
    vestibulePortalDetailGroupCount_ = 0;
    overheadLuggageItemCount_ = 0;
    floorLuggageCaseCount_ = 0;
    floorLuggageWheelPairCount_ = 0;
    roundedSeatCushionCount_ = 0;
    roundedSeatBackCount_ = 0;
    seatPedestalCount_ = 0;
    seatMountFastenerCount_ = 0;
    roundedAisleArmrestCount_ = 0;
    roundedTableTopCount_ = 0;
    tableSupportPlateCount_ = 0;
    windowReflectionAccentCount_ = 0;
    interiorLightGradientQuadCount_ = 0;
    sunlightPatchCount_ = 0;
    aisleRunnerInlayCount_ = 0;
    nonCarpetedFloorZoneCount_ = 0;
    interiorCovingSectionCount_ = 0;
    ceilingPanelSeamCount_ = 0;
    vestibuleWearMarkCount_ = 0;
    const Color shell{211,218,218,255};
    const Color trim{32,102,121,255};
    const Color floor{68,72,76,255};
    const std::array<Color, 6> carSeatColors = {
        Color(31,65,94,255), Color(37,76,111,255), Color(39,101,101,255),
        Color(42,82,105,255), Color(43,70,91,255), Color(37,76,111,255)};
    const std::array<Color, 6> carSeatSeams = {
        Color(44,79,107,255), Color(50,90,124,255), Color(52,115,114,255),
        Color(55,96,118,255), Color(56,84,104,255), Color(50,90,124,255)};
    const std::array<Color, 6> carHeadrests = {
        Color(164,180,185,255), Color(151,184,190,255), Color(146,190,184,255),
        Color(190,181,130,255), Color(151,174,163,255), Color(151,184,190,255)};
    const std::array<Color, 6> carAccents = {
        Color(75,99,119,255), Color(61,101,132,255), Color(41,128,124,255),
        Color(169,137,55,255), Color(75,112,91,255), Color(52,102,124,255)};
    const Color seatBackShell{62,77,85,255};
    const Color seatFrame{95,99,99,255};
    const auto addSeatPanel = [&mesh](float xmin, float xmax, float ymin, float ymax,
                                      float z, bool normalTowardPositiveZ,
                                      const Color& color) {
        const Vector3 lowerLeft{xmin,ymin,z};
        const Vector3 lowerRight{xmax,ymin,z};
        const Vector3 upperRight{xmax,ymax,z};
        const Vector3 upperLeft{xmin,ymax,z};
        if (normalTowardPositiveZ)
            mesh.addQuad(lowerLeft,lowerRight,upperRight,upperLeft,color);
        else
            mesh.addQuad(lowerRight,lowerLeft,upperLeft,upperRight,color);
    };
    const auto addWallPanel = [&interiorPanelMaterial](
        float x, float y0, float y1, float z0, float z1,
        bool inwardNormalPositiveX, const Color& color) {
        constexpr float tileMetres = 0.90f;
        const Vector3 lowerBegin{x,y0,z0};
        const Vector3 lowerEnd{x,y0,z1};
        const Vector3 upperEnd{x,y1,z1};
        const Vector3 upperBegin{x,y1,z0};
        const Vector2 uvLowerBegin{z0/tileMetres,y0/tileMetres};
        const Vector2 uvLowerEnd{z1/tileMetres,y0/tileMetres};
        const Vector2 uvUpperEnd{z1/tileMetres,y1/tileMetres};
        const Vector2 uvUpperBegin{z0/tileMetres,y1/tileMetres};
        if (inwardNormalPositiveX)
            interiorPanelMaterial.addTexturedQuad(
                lowerEnd,lowerBegin,upperBegin,upperEnd,color,
                uvLowerEnd,uvLowerBegin,uvUpperBegin,uvUpperEnd);
        else
            interiorPanelMaterial.addTexturedQuad(
                lowerBegin,lowerEnd,upperEnd,upperBegin,color,
                uvLowerBegin,uvLowerEnd,uvUpperEnd,uvUpperBegin);
    };
    const auto addCeilingPanel = [&interiorPanelMaterial](
        float x0, float x1, float y, float z0, float z1, const Color& color) {
        constexpr float tileMetres = 0.90f;
        interiorPanelMaterial.addTexturedQuad(
            Vector3{x1,y,z0},Vector3{x1,y,z1},
            Vector3{x0,y,z1},Vector3{x0,y,z0},color,
            Vector2{x1/tileMetres,z0/tileMetres},
            Vector2{x1/tileMetres,z1/tileMetres},
            Vector2{x0/tileMetres,z1/tileMetres},
            Vector2{x0/tileMetres,z0/tileMetres});
    };
    const auto addResilientFloor = [&interiorResilientMaterial](
        float x0, float x1, float y, float z0, float z1, const Color& color) {
        constexpr float tileMetres = 0.48f;
        interiorResilientMaterial.addTexturedQuad(
            Vector3{x0,y,z0},Vector3{x0,y,z1},
            Vector3{x1,y,z1},Vector3{x1,y,z0},color,
            Vector2{x0/tileMetres,z0/tileMetres},
            Vector2{x0/tileMetres,z1/tileMetres},
            Vector2{x1/tileMetres,z1/tileMetres},
            Vector2{x1/tileMetres,z0/tileMetres});
    };

    for (int car = 0; car < 6; ++car)
    {
        const float centre = kFirstCarCentre + static_cast<float>(car) * kCarLength;
        const float start = centre - 10.25f;
        const float end = centre + 10.25f;
        const float passengerDoorZ = static_cast<float>(
            Sim::TrainInterior::passengerDoorZ(car));
        const float passengerDoorHalfOpening =
            static_cast<float>(Sim::TrainInterior::PassengerDoorHalfOpening);
        const float passengerDoorBegin = passengerDoorZ - passengerDoorHalfOpening;
        const float passengerDoorEnd = passengerDoorZ + passengerDoorHalfOpening;
        mesh.addBox(Vector3{-1.48f,-0.08f,start}, Vector3{1.48f,0.08f,end}, floor);
        // A restrained hard-wearing aisle runner breaks up the otherwise uniform floor and
        // provides a continuous visual path through the complete passenger consist.
        mesh.addBox(Vector3{-0.39f,0.081f,start+0.12f},
                    Vector3{0.39f,0.094f,end-0.12f}, Color(42,54,61,255));

        // Resilient vestibule flooring gives the surface-aware footstep change a matching visual
        // cue. The authored extent comes from TrainInterior, and shallow transverse joints keep
        // the panel from reading as another flat carpet rectangle. It remains in the interior
        // batch and therefore adds no draw call.
        const float vestibuleBegin = std::max(
            start+0.02f,passengerDoorZ-static_cast<float>(
                Sim::TrainInterior::VestibuleFloorHalfLength));
        const float vestibuleEnd = std::min(
            end-0.02f,passengerDoorZ+static_cast<float>(
                Sim::TrainInterior::VestibuleFloorHalfLength));

        const auto addCarpetSpan = [&interiorMaterial](float spanBegin, float spanEnd) {
            if (spanEnd-spanBegin < 0.12f) return;
            constexpr float tileMetres = 0.85f;
            const auto addSurface = [&](float x0, float x1, float y, const Color& color) {
                interiorMaterial.addTexturedQuad(
                    Vector3{x0,y,spanBegin},Vector3{x0,y,spanEnd},
                    Vector3{x1,y,spanEnd},Vector3{x1,y,spanBegin},color,
                    Vector2{x0/tileMetres,spanBegin/tileMetres},
                    Vector2{x0/tileMetres,spanEnd/tileMetres},
                    Vector2{x1/tileMetres,spanEnd/tileMetres},
                    Vector2{x1/tileMetres,spanBegin/tileMetres});
            };
            addSurface(-1.38f,1.38f,0.083f,Color(82,84,84,255));
            addWornCarpetRunner(interiorMaterial,spanBegin,spanEnd);
        };
        addCarpetSpan(start+0.12f,vestibuleBegin-0.05f);
        addCarpetSpan(vestibuleEnd+0.05f,end-0.12f);

        // Flush woven perimeter tape and narrow installation seams reuse the carpet batch.
        aisleRunnerInlayCount_ += addCarpetBindings(
            interiorMaterial,start+0.12f,vestibuleBegin-0.05f);
        aisleRunnerInlayCount_ += addCarpetBindings(
            interiorMaterial,vestibuleEnd+0.05f,end-0.12f);
        mesh.addBox(Vector3{-1.39f,0.095f,vestibuleBegin},
                    Vector3{1.39f,0.106f,vestibuleEnd},Color(55,72,77,255));
        addResilientFloor(-1.385f,1.385f,0.107f,vestibuleBegin+0.015f,
                          vestibuleEnd-0.015f,Color(74,91,95,255));
        const int floorJointCount = static_cast<int>(
            std::floor((vestibuleEnd-vestibuleBegin)/0.58f));
        for (int joint = 1; joint <= floorJointCount; ++joint)
        {
            const float z = vestibuleBegin+static_cast<float>(joint)*0.58f;
            mesh.addBox(Vector3{-1.34f,0.106f,z-0.010f},
                        Vector3{1.34f,0.110f,z+0.010f},Color(36,49,53,255));
        }
        // Paired low-contrast wheel/scuff tracks run inward from both doorway leaves. They add
        // believable traffic wear to the wipe-clean vestibule without raising a collision edge
        // or muddying the central wayfinding strip.
        for (float side : {-1.0f,1.0f})
        {
            const float wearBeginX = side < 0.0f ? -1.30f : 0.43f;
            const float wearEndX = side < 0.0f ? -0.43f : 1.30f;
            for (float offsetZ : {-0.22f,0.22f})
            {
                const float z = passengerDoorZ+offsetZ;
                mesh.addBox(Vector3{wearBeginX,0.109f,z-0.014f},
                            Vector3{wearEndX,0.112f,z+0.014f},Color(61,77,81,255));
                ++vestibuleWearMarkCount_;
            }
        }
        ++nonCarpetedFloorZoneCount_;
        mesh.addBox(Vector3{-1.48f,2.92f,start}, Vector3{1.48f,3.08f,end}, shell);

        // Close passenger-eye surfaces receive a restrained polymer-grain material. These thin
        // inset skins retain the structural shell and trim in the legacy batch while replacing
        // its largest perfectly flat regions. The ceiling is split around the continuous light,
        // and lower wall spans stop at the passenger doorway rather than covering its opening.
        const Color wallPanel(211,218,218,255);
        const Color ceilingPanel(222,227,226,255);
        addCeilingPanel(-1.40f,-0.115f,2.914f,start+0.03f,end-0.03f,ceilingPanel);
        addCeilingPanel(0.115f,1.40f,2.914f,start+0.03f,end-0.03f,ceilingPanel);

        for (float side : {-1.0f, 1.0f})
        {
            const float x0 = side < 0 ? -1.50f : 1.42f;
            const float x1 = side < 0 ? -1.42f : 1.50f;
            mesh.addBox(Vector3{x0,0.05f,start},
                        Vector3{x1,0.86f,passengerDoorBegin}, shell);
            mesh.addBox(Vector3{x0,0.05f,passengerDoorEnd},
                        Vector3{x1,0.86f,end}, shell);
            mesh.addBox(Vector3{x0,2.34f,start}, Vector3{x1,3.00f,end}, shell);
            const float panelX = side*1.416f;
            addWallPanel(panelX,0.10f,0.80f,start+0.04f,passengerDoorBegin-0.07f,
                         side < 0.0f,wallPanel);
            addWallPanel(panelX,0.10f,0.80f,passengerDoorEnd+0.07f,end-0.04f,
                         side < 0.0f,wallPanel);
            addWallPanel(panelX,2.40f,2.90f,start+0.04f,end-0.04f,
                         side < 0.0f,ceilingPanel);

            // Continuous pale ceiling coving and a darker wipe-clean floor skirting make the
            // wall junctions read as assembled transport panels rather than intersecting boxes.
            addCeilingCove(interiorPanelMaterial,side,start+0.04f,end-0.04f,ceilingPanel);
            ++interiorCovingSectionCount_;
            const float skirtX0 = std::min(side*1.416f,side*1.37f);
            const float skirtX1 = std::max(side*1.416f,side*1.37f);
            mesh.addBox(Vector3{skirtX0,0.095f,start+0.05f},
                        Vector3{skirtX1,0.205f,passengerDoorBegin-0.08f},
                        Color(48,79,86,255));
            mesh.addBox(Vector3{skirtX0,0.095f,passengerDoorEnd+0.08f},
                        Vector3{skirtX1,0.205f,end-0.05f},Color(48,79,86,255));
            interiorCovingSectionCount_ += 2;
            for (int pillar = 0; pillar <= 10; ++pillar)
            {
                const float z = static_cast<float>(Sim::TrainInterior::windowMullionZ(car,pillar));
                if (std::abs(z - passengerDoorZ) < 0.82f) continue;
                const float halfWidth=static_cast<float>(Sim::TrainInterior::WindowMullionWidth)*0.5f;
                mesh.addBox(Vector3{x0,0.82f,z-halfWidth},
                            Vector3{x1,2.38f,z+halfWidth}, trim);
                addPassengerMullionCap(interiorPanelMaterial,side,z,halfWidth);
                const float seamX0 = side < 0.0f ? -1.38f : 0.13f;
                const float seamX1 = side < 0.0f ? -0.13f : 1.38f;
                mesh.addBox(Vector3{seamX0,2.900f,z-0.012f},
                            Vector3{seamX1,2.913f,z+0.012f},Color(174,184,184,255));
                ++ceilingPanelSeamCount_;
                // Lower-wall panel joints continue the window-bay rhythm below the glazing.
                const float innerX0 = side < 0.0f ? -1.414f : 1.408f;
                const float innerX1 = side < 0.0f ? -1.404f : 1.418f;
                mesh.addBox(Vector3{innerX0,0.18f,z-0.020f},
                            Vector3{innerX1,0.78f,z+0.020f},Color(170,181,181,255));
            }
            // The last bay ends before the car-end partition. Close that remaining pier
            // rather than leaving an unglazed strip beyond the final mullion.
            const float lastWindowEnd=static_cast<float>(Sim::TrainInterior::windowClearBeginZ(
                car,Sim::TrainInterior::WindowBaysPerCar-1)+Sim::TrainInterior::WindowClearLength);
            if (lastWindowEnd<end)
                mesh.addBox({x0,0.82f,lastWindowEnd},{x1,2.38f,end},trim);
            for (float jambZ : {passengerDoorBegin, passengerDoorEnd})
                mesh.addBox(Vector3{x0,0.04f,jambZ-0.065f},
                            Vector3{x1,2.40f,jambZ+0.065f}, trim);
            addPassengerDoorControlHousing(mesh,side,passengerDoorBegin);
            const float thresholdInner = side * 1.18f;
            const float thresholdOuter = side * 1.50f;
            mesh.addBox(Vector3{std::min(thresholdInner,thresholdOuter),0.081f,
                                passengerDoorBegin+0.07f},
                        Vector3{std::max(thresholdInner,thresholdOuter),0.098f,
                                passengerDoorEnd-0.07f}, Color(225,184,47,255));
            if (car == Sim::TrainInterior::ToiletCar && side > 0.0f)
            {
                // The accessible toilet replaces this side of the saloon. Split the continuous
                // shelf at its partition instead of letting it protrude through the compartment.
                addLuggageRackSpan(mesh,side,start+0.7f,static_cast<float>(
                    Sim::TrainInterior::ToiletRackClearanceBeginZ));
                addLuggageRackSpan(mesh,side,static_cast<float>(
                    Sim::TrainInterior::ToiletRackClearanceEndZ),end-0.7f);
            }
            else
                addLuggageRackSpan(mesh,side,start+0.7f,end-0.7f);

            const auto addWindowSpan = [&](float windowBegin, float windowEnd,
                                           int reflectionVariant) {
                if (windowEnd - windowBegin < 0.08f) return;
                addPassengerWindowFrame(mesh,side,windowBegin,windowEnd);
                const float innerX = side < 0.0f ? -1.455f : 1.445f;
                const float outerX = side < 0.0f ? -1.445f : 1.455f;
                glass.addBox(Vector3{innerX,0.88f,windowBegin},
                             Vector3{outerX,2.32f,windowEnd}, Color(110,177,201,28));

                // Reflections live in a separate side-specific batch so draw-time view angle can
                // attenuate them. Soft edges and varied position avoid a repeated decal pattern while
                // keeping the landscape readable through the base tint.
                const float span = windowEnd - windowBegin;
                if (span > 1.20f)
                {
                    MeshBuilder& reflection = glassReflections[
                        static_cast<std::size_t>(side > 0.0f ? 1 : 0)];
                    const float glintZ = windowBegin + span
                        * ((reflectionVariant & 1) == 0 ? 0.42f : 0.58f);
                    const float reflectionX=side*1.439f;
                    addSoftWindowReflection(reflection,reflectionX,1.91f,glintZ,
                                            0.18f,span*0.28f,16);
                    addSoftWindowReflection(reflection,reflectionX,2.18f,
                                            windowBegin+span*0.53f,0.09f,span*0.32f,8);
                    windowReflectionAccentCount_ += 2;
                }
            };
            for (int window = 0; window < Sim::TrainInterior::WindowBaysPerCar; ++window)
            {
                const float windowBegin = static_cast<float>(
                    Sim::TrainInterior::windowClearBeginZ(car, window));
                const float windowEnd = windowBegin
                    + static_cast<float>(Sim::TrainInterior::WindowClearLength);
                const int reflectionVariant = car * Sim::TrainInterior::WindowBaysPerCar
                    + window + (side > 0.0f ? 1 : 0);
                if (windowEnd <= passengerDoorBegin || windowBegin >= passengerDoorEnd)
                    addWindowSpan(windowBegin, windowEnd, reflectionVariant);
                else
                {
                    addWindowSpan(windowBegin, std::min(windowEnd, passengerDoorBegin),
                                  reflectionVariant);
                    addWindowSpan(std::max(windowBegin, passengerDoorEnd), windowEnd,
                                  reflectionVariant);
                }
            }
        }

        // Soft window-shaped floor highlights make the sunny exterior affect the carriage
        // without a shadow map. The low-alpha gradient is one transparent batch for all cars;
        // opaque seats and fixtures already in the depth buffer naturally mask it. Avoid the
        // enclosed toilet floor, where the saloon partition blocks this side's direct sun.
        for (int window : {1,3,5,7})
        {
            const float centreZ = static_cast<float>(
                Sim::TrainInterior::windowClearCentreZ(car,window));
            if (car == Sim::TrainInterior::ToiletCar
                && centreZ > static_cast<float>(Sim::TrainInterior::ToiletRackClearanceBeginZ)
                && centreZ < static_cast<float>(Sim::TrainInterior::ToiletRackClearanceEndZ))
                continue;
            const float variation = static_cast<float>((car+window) % 3) * 0.09f;
            const float nearBegin = centreZ-0.54f;
            const float nearEnd = centreZ+0.54f;
            const float farShift = 0.70f+variation;
            // Match the visible top-face winding used by MeshBuilder::addBox. The warm edge is
            // beside the window and the two faint vertices carry the gradient into the aisle.
            sunlight.addQuad(
                Vector3{-1.37f,0.101f,nearBegin},
                Vector3{-1.37f,0.101f,nearEnd},
                Vector3{0.72f,0.101f,nearEnd+farShift},
                Vector3{0.72f,0.101f,nearBegin+farShift},
                Color(255,235,174,64),Color(255,235,174,64),
                Color(255,229,158,8),Color(255,229,158,8));
            ++sunlightPatchCount_;
        }

        // A broad, low-alpha reflected-light gradient makes the continuous luminaire affect its
        // surroundings instead of reading as an isolated glowing box. Four quads per car cover
        // both ceiling slopes and the upper walls in one transparent batch. The alpha remains
        // deliberately restrained so daylight and the polymer material texture still dominate.
        const Color ceilingLightWarm{255,231,166,44};
        const Color wallLightWarm{255,228,158,24};
        const Color noLight{255,231,166,0};
        interiorLighting.addQuad(
            Vector3{-0.108f,2.905f,start+0.08f},
            Vector3{-0.108f,2.905f,end-0.08f},
            Vector3{-1.39f,2.905f,end-0.08f},
            Vector3{-1.39f,2.905f,start+0.08f},
            ceilingLightWarm,ceilingLightWarm,noLight,noLight);
        interiorLighting.addQuad(
            Vector3{1.39f,2.905f,start+0.08f},
            Vector3{1.39f,2.905f,end-0.08f},
            Vector3{0.108f,2.905f,end-0.08f},
            Vector3{0.108f,2.905f,start+0.08f},
            noLight,noLight,ceilingLightWarm,ceilingLightWarm);
        interiorLighting.addQuad(
            Vector3{-1.403f,2.36f,end-0.08f},
            Vector3{-1.403f,2.36f,start+0.08f},
            Vector3{-1.403f,2.89f,start+0.08f},
            Vector3{-1.403f,2.89f,end-0.08f},
            noLight,noLight,wallLightWarm,wallLightWarm);
        interiorLighting.addQuad(
            Vector3{1.403f,2.36f,start+0.08f},
            Vector3{1.403f,2.36f,end-0.08f},
            Vector3{1.403f,2.89f,end-0.08f},
            Vector3{1.403f,2.89f,start+0.08f},
            noLight,noLight,wallLightWarm,wallLightWarm);
        interiorLightGradientQuadCount_ += 4;

        // Serviceable ceiling-light modules and modern passenger-information screens.
        // Recessed aluminium channel with replaceable opal diffuser sections. The emitting
        // face is unlit: sunlight direction must not turn a powered ceiling lamp dark brown.
        for (float segment = start+0.72f; segment < end-0.72f; segment += 1.50f)
        {
            const float finish = std::min(segment+1.38f,end-0.72f);
            addCeilingLightModule(mesh,interiorEmitters,segment,finish);
        }
        addPassengerDisplayHousing(mesh,centre);

        // Two ceiling PA grilles give the onboard chime and announcements a visible source.
        // The shallow panels and perforations remain in the immutable interior batch, so all
        // twelve speakers add detail without adding a material switch or draw call.
        for (const std::pair<float,float>& speaker : {
                 std::pair{-0.54f,-5.35f}, std::pair{0.54f,5.35f}})
        {
            const float speakerX = speaker.first;
            const float speakerZ = centre + speaker.second;
            mesh.addBox(Vector3{speakerX-0.19f,2.805f,speakerZ-0.19f},
                        Vector3{speakerX+0.19f,2.835f,speakerZ+0.19f},
                        Color(149,157,157,255));
            for (int row = -1; row <= 1; ++row)
                for (int column = -1; column <= 1; ++column)
                {
                    const float x = speakerX + static_cast<float>(column)*0.075f;
                    const float z = speakerZ + static_cast<float>(row)*0.075f;
                    mesh.addBox(Vector3{x-0.018f,2.796f,z-0.018f},
                                Vector3{x+0.018f,2.807f,z+0.018f},
                                Color(48,60,63,255));
                }
            ++paSpeakerGrilleCount_;
        }

        // Paired ceiling diffusers make the otherwise quiet climate system legible even while
        // stationary. Their shallow louvres share the same immutable batch as the speaker panels.
        for (const std::pair<float,float>& vent : {
                 std::pair{-0.56f,2.70f}, std::pair{0.56f,-2.70f}})
        {
            const float ventX = vent.first;
            const float ventZ = centre + vent.second;
            mesh.addBox(Vector3{ventX-0.20f,2.806f,ventZ-0.34f},
                        Vector3{ventX+0.20f,2.834f,ventZ+0.34f},
                        Color(162,169,168,255));
            for (int louvre = -3; louvre <= 3; ++louvre)
            {
                const float z = ventZ + static_cast<float>(louvre)*0.080f;
                mesh.addBox(Vector3{ventX-0.145f,2.796f,z-0.013f},
                            Vector3{ventX+0.145f,2.808f,z+0.013f},
                            Color(60,72,74,255));
            }
            ++hvacVentCount_;
        }

        // Restrained pairs of suspended grab handles add useful scale and vestibule detail while
        // leaving the central standing envelope clear below 2.3 m.
        for (float offset : {-8.0f, -4.0f, 4.0f, 8.0f})
            for (float x : {-0.38f, 0.38f})
            {
                const float z = centre + offset;
                addSuspendedPassengerHandle(mesh,interiorResilientMaterial,x,z);
            }

        // Visible rack ribs and a few reusable cases turn the continuous shelf rail into a
        // believable luggage system. Everything remains in the one interior mesh: this detail
        // adds no material switch or draw call, and the sparse placement keeps the saloon airy.
        const std::array<Color, 6> rackLuggageColors = {
            Color(125,67,52,255), Color(48,89,122,255), Color(121,103,52,255),
            Color(64,101,78,255), Color(99,63,111,255), Color(137,80,45,255)};
        constexpr std::array<float, 4> luggageOffsets = {-6.1f,5.15f,-1.8f,1.85f};
        for (float side : {-1.0f, 1.0f})
        {
            for (int pillar : {1,3,5,7,9})
            {
                const float z = static_cast<float>(Sim::TrainInterior::windowMullionZ(car,pillar));
                if (std::abs(z-passengerDoorZ) < 0.82f) continue;
                if (!Sim::TrainInterior::overheadRackPositionAllowed(
                        car,side > 0.0f ? 1 : -1,z)) continue;
                // Bolted to finished structural mullions, never unsupported glazing.
                addLuggageRackBracket(mesh,side,z);
            }
            const int bagCount = car == 4 ? 4 : 2;
            for (int bag = 0; bag < bagCount; ++bag)
            {
                const float z = centre + luggageOffsets[static_cast<std::size_t>(bag)]
                              + side * static_cast<float>((car + bag) % 3) * 0.22f;
                if (!Sim::TrainInterior::overheadRackPositionAllowed(
                        car,side > 0.0f ? 1 : -1,z)) continue;
                const float inner = side * 0.93f;
                const float outer = side * (1.19f + 0.03f*static_cast<float>(bag));
                const float xmin = std::min(inner,outer);
                const float xmax = std::max(inner,outer);
                const float halfLength = bag % 2 == 0 ? 0.43f : 0.33f;
                const Color& caseColor = rackLuggageColors[
                    (static_cast<std::size_t>(car)*2U + static_cast<std::size_t>(bag)
                     + (side > 0.0f ? 1U : 0U)) % rackLuggageColors.size()];
                addRackSuitcase(mesh,xmin,xmax,z,halfLength,
                                2.46f+0.05f*static_cast<float>(bag),caseColor,bag%2==0);
                ++overheadLuggageItemCount_;
            }
        }

        // Fixed function signs make every saloon legible as a deliberate part of one consistent
        // train rather than six repeated rooms. A narrow family-colour band carries that identity
        // without turning the interior into a collection of unrelated themes.
        mesh.addBox(Vector3{-0.94f,2.10f,centre-0.055f},
                    Vector3{0.94f,2.32f,centre+0.055f}, Color(31,51,59,255));
        mesh.addBox(Vector3{-0.94f,2.10f,centre-0.061f},
                    Vector3{0.94f,2.145f,centre+0.061f},
                    carAccents[static_cast<std::size_t>(car)]);
        for (float face : {-1.0f, 1.0f})
            addCarRoleIcon(mesh,car,Matrix::CreateRotationY(face<0 ? kPi : 0)
                *Matrix::CreateTranslation(face*0.76f,2.205f,centre+face*0.064f),
                Color(223,232,230,255));
        ++carIdentitySignCount_;

        // Role names now share the antialiased display atlas. Remove the redundant facility
        // plaque formerly hidden behind the main screen; physical cues below remain visible.
        if (car == 2)
        {
            // Passenger-assistance call point at the accessible bay entrance.
            mesh.addBox(Vector3{-0.94f,1.52f,centre-0.065f},
                        Vector3{-0.62f,2.02f,centre+0.065f},Color(47,75,82,255));
            mesh.addBox(Vector3{-0.85f,1.66f,centre-0.076f},
                        Vector3{-0.71f,1.82f,centre+0.076f},Color(224,184,47,255));
            mesh.addBox(Vector3{-0.82f,1.88f,centre-0.077f},
                        Vector3{-0.74f,1.94f,centre+0.077f},Color(91,205,119,255));
        }
        else if (car == 5)
        {
            // A slim timetable/route-leaflet pocket gives the general saloon a plausible service
            // fixture without occupying the passenger aisle.
            mesh.addBox(Vector3{0.61f,1.55f,centre-0.060f},
                        Vector3{0.92f,2.01f,centre+0.060f},Color(71,82,84,255));
            for (int leaflet = 0; leaflet < 3; ++leaflet)
            {
                const float x = 0.65f+static_cast<float>(leaflet)*0.09f;
                mesh.addBox(Vector3{x,1.68f,centre-0.071f},
                            Vector3{x+0.07f,1.94f,centre+0.071f},
                            leaflet == 1 ? Color(224,184,47,255)
                                         : Color(208,222,221,255));
            }
        }
        ++carFunctionFixtureGroupCount_;

        // Every vestibule carries the safety equipment a passenger expects to find: an
        // end-wall extinguisher, a clearly distinct emergency intercom, and illuminated exit
        // pictograms above both door leaves. The end wall avoids sacrificing glazing or placing
        // equipment behind the final seat row. All pieces remain outside the walkable gangway.
        const float safetyWallZ = car + 1 == Sim::TrainInterior::PassengerCars
            ? 63.08f : centre + kCarLength*0.5f - 0.245f;
        const float fixtureZ = safetyWallZ - 0.09f;
        // Layer the broad vestibule end wall behind the safety fixtures. A shallow inset field,
        // perimeter trim, visible panel fasteners and a dark doorway seal replace the previous
        // uninterrupted slab while remaining outside the authoritative gangway clearance.
        const Color vestibuleSeal(22,31,34,255);
        for (float side : {-1.0f,1.0f})
        {
            const float x0 = side < 0.0f ? -1.40f : 0.67f;
            const float x1 = side < 0.0f ? -0.67f : 1.40f;
            addVestibuleServicePanels(interiorPanelMaterial,side,fixtureZ+0.020f,-1);
            for (float x : {x0+0.075f,x1-0.075f})
                for (float y : {0.29f,2.17f})
                    addSeatedPanelFastener(mesh,x,y,fixtureZ-0.002f,1.0f);
        }
        mesh.addBox(Vector3{-0.650f,0.10f,fixtureZ-0.004f},
                    Vector3{-0.608f,2.38f,fixtureZ+0.014f},vestibuleSeal);
        mesh.addBox(Vector3{0.608f,0.10f,fixtureZ-0.004f},
                    Vector3{0.650f,2.38f,fixtureZ+0.014f},vestibuleSeal);
        mesh.addBox(Vector3{-0.650f,2.34f,fixtureZ-0.004f},
                    Vector3{0.650f,2.39f,fixtureZ+0.014f},vestibuleSeal);
        ++vestibulePortalDetailGroupCount_;
        mesh.addBox(Vector3{0.78f,0.13f,safetyWallZ-0.035f},
                    Vector3{1.22f,0.94f,safetyWallZ},Color(72,79,80,255));
        addExtinguisherFittings(mesh,1.0f,fixtureZ,safetyWallZ);
        mesh.addCylinder(Vector3{1.0f,0.25f,fixtureZ},0.082f,0.46f,12,
                         Color(202,48,40,255));
        mesh.addCone(Vector3{1.0f,0.71f,fixtureZ},0.070f,0.10f,12,
                     Color(202,48,40,255));
        mesh.addCylinder(Vector3{1.0f,0.81f,fixtureZ},0.034f,0.085f,8,
                         Color(45,51,52,255));
        mesh.addBox(Vector3{0.95f,0.86f,fixtureZ-0.065f},
                    Vector3{1.05f,0.91f,fixtureZ+0.065f},Color(50,54,54,255));
        mesh.addBox(Vector3{0.96f,0.44f,fixtureZ-0.086f},
                    Vector3{1.04f,0.55f,fixtureZ-0.078f},Color(245,221,184,255));

        addEmergencyIntercom(mesh,-1.0f,safetyWallZ);
        addDisplayText(mesh,"EMERGENCY",1.447f,safetyWallZ-0.0925f,false,
                       Color(214,224,222,255),-1.0f,0.24f);
        addDisplayText(mesh,"PRESS AND SPEAK",1.199f,safetyWallZ-0.0925f,false,
                       Color(214,224,222,255),-1.0f,0.24f);

        for (float side : {-1.0f,1.0f})
        {
            const float x0 = side < 0.0f ? -1.435f : 1.405f;
            const float x1 = side < 0.0f ? -1.405f : 1.435f;
            mesh.addBox(Vector3{x0,2.48f,passengerDoorZ-0.33f},
                        Vector3{x1,2.76f,passengerDoorZ+0.33f},Color(38,139,78,255));
            mesh.addBox(Vector3{x0-side*0.010f,2.595f,passengerDoorZ-0.18f},
                        Vector3{x1-side*0.010f,2.635f,passengerDoorZ+0.17f},
                        Color(231,241,226,255));
            mesh.addBox(Vector3{x0-side*0.011f,2.635f,passengerDoorZ+0.10f},
                        Vector3{x1-side*0.011f,2.690f,passengerDoorZ+0.18f},
                        Color(231,241,226,255));
            mesh.addBox(Vector3{x0-side*0.011f,2.540f,passengerDoorZ+0.10f},
                        Vector3{x1-side*0.011f,2.595f,passengerDoorZ+0.18f},
                        Color(231,241,226,255));
        }

        for (int row = 0; row < Sim::TrainInterior::SeatRowsPerCar; ++row)
        {
            const float z = static_cast<float>(Sim::TrainInterior::seatRowZ(car, row));
            const bool forward = row % 2 == 0;
            for (int side : {-1, 1})
            {
                if (!Sim::TrainInterior::seatSidePresent(car, row, side)) continue;
                const bool hasInnerSeat = Sim::TrainInterior::innerSeatPresent(car, row);
                const Color& upholstery = carSeatColors[static_cast<std::size_t>(car)];
                const Color& upholsterySeam = carSeatSeams[static_cast<std::size_t>(car)];
                const float backZ = z + (forward ? 0.28f : -0.38f);
                const std::array<float, 2> seatCentres = {
                    1.04f * static_cast<float>(side),
                    0.58f * static_cast<float>(side)};
                const int seatCount = hasInnerSeat ? 2 : 1;
                const float shadowInner = (hasInnerSeat ? 0.33f : 0.70f)
                                        * static_cast<float>(side);
                const float shadowOuter = 1.31f*static_cast<float>(side);
                addSoftContactShadow(interiorShadows,(shadowInner+shadowOuter)*0.5f,0.104f,z,
                                     (std::abs(shadowOuter-shadowInner)+0.12f)*0.5f,0.43f,48);
                for (int seatIndex = 0; seatIndex < seatCount; ++seatIndex)
                {
                    const float seatX = seatCentres[static_cast<std::size_t>(seatIndex)];
                    const float xmin = seatX - 0.225f;
                    const float xmax = seatX + 0.225f;
                    addSeatUpholsteryBody(upholsteryMaterial,seatX,z,backZ,upholstery);
                    ++roundedSeatCushionCount_;
                    ++roundedSeatBackCount_;
                    // The replaceable antimacassar belongs only on the passenger-facing side.
                    // A separate rear shell, grab recess and literature pocket stop every row
                    // from looking like the same pale slab when viewed while walking the aisle.
                    const float frontSurface = forward ? backZ-0.014f : backZ+0.114f;
                    const float rearSurface = forward ? backZ+0.114f : backZ-0.014f;
                    const float frontDepth = forward ? -0.012f : 0.012f;
                    // The shaped inset uses the same physical weave coordinates as the body.
                    const float fabricSurface = frontSurface+frontDepth*1.25f;
                    const auto fabricBegin=upholsteryMaterial.data().vertices.size();
                    addShapedSeatFabric(upholsteryMaterial,xmin+0.040f,xmax-0.040f,0.69f,1.29f,
                                       fabricSurface,!forward,upholstery);
                    upholsteryMaterial.projectDetailUVs(fabricBegin,{xmin,0.42f,z-0.35f},0.16f);
                    addSeatHeadCover(mesh,seatX,backZ,forward,
                                     carHeadrests[static_cast<std::size_t>(car)]);
                    const float seamSurface = frontSurface+frontDepth*1.7f;
                    // Restrained 4 mm piping, not the previous 18–30 mm contrasting bands.
                    // Keep the same batches and silhouettes: this is a textile-scale correction.
                    addSeatPanel(xmin+0.036f,xmin+0.040f,0.68f,1.28f,
                                 seamSurface,!forward,upholsterySeam);
                    addSeatPanel(xmax-0.040f,xmax-0.036f,0.68f,1.28f,
                                 seamSurface,!forward,upholsterySeam);
                    addSeatPanel(xmin+0.040f,xmax-0.040f,1.258f,1.262f,
                                 seamSurface,!forward,upholsterySeam);
                    addSeatCushionWelt(mesh,seatX,z,upholsterySeam);
                    addSeatRearShell(interiorPanelMaterial,seatX,rearSurface,forward ? 1.0f : -1.0f,
                                     seatBackShell);
                    // A slim offset pedestal replaces the pair of broad rectangular side plates.
                    // The support remains inside the old footprint and below the authoritative
                    // cushion collision, but reads as modern railway furniture from the aisle.
                    const float pedestalZ = z+(forward ? 0.10f : -0.10f);
                    addSoftContactShadow(interiorShadows,seatX,0.1048f,pedestalZ,0.16f,0.16f,105);
                    addSeatPedestal(mesh,seatX,pedestalZ,seatFrame);
                    seatMountFastenerCount_ += 2;
                    mesh.addRoundedBoxXZ(Vector3{seatX-0.18f,0.39f,z-0.21f},
                                         Vector3{seatX+0.18f,0.44f,z+0.21f},
                                         0.035f,2,seatFrame);
                    ++seatPedestalCount_;
                }

                // Low aisle armrest, mirrored per side. It stays outside the collision-tested
                // 0.84 m clear aisle and gives the seat rows a more plausible silhouette.
                const float aisleEdge = (hasInnerSeat ? 0.34f : 0.72f)
                                      * static_cast<float>(side);
                mesh.addCylinder(Vector3{aisleEdge,0.59f,z},0.027f,0.31f,8,seatFrame);
                addPassengerArmrest(mesh,interiorResilientMaterial,aisleEdge,z);
                ++roundedAisleArmrestCount_;
            }
        }

        // Facing pairs receive real window tables rather than another decorative decal. The
        // shared TrainInterior layout also supplies their collision footprint, so neither the
        // player nor visual-QA camera can pass through a tabletop. The accessible companion bay
        // and the toilet-replaced side deliberately remain clear.
        const Color tableTop = car == 1 ? Color(181,190,190,255)
                             : car == 3 ? Color(199,193,169,255)
                                        : Color(190,194,187,255);
        for (int table = 0; table < Sim::TrainInterior::TableBaysPerCar; ++table)
        {
            const float z = static_cast<float>(Sim::TrainInterior::tableBayZ(car,table));
            for (int side : {-1,1})
            {
                if (!Sim::TrainInterior::tableSidePresent(car,table,side)) continue;
                const float direction = static_cast<float>(side);
                const float inner = direction*0.48f;
                const float outer = direction*1.36f;
                const float xmin = std::min(inner,outer);
                const float xmax = std::max(inner,outer);
                addPassengerTableTop(interiorPanelMaterial,(xmin+xmax)*0.5f,z,tableTop);
                ++roundedTableTopCount_;
                mesh.addCylinder(Vector3{direction*0.66f,0.08f,z},0.13f,0.025f,12,
                                 Color(103,110,108,255));
                // The narrow pedestal previously met the resilient floor without any local
                // occlusion, which made the table appear to hover in bright daylight. Keep this
                // footprint tight around the base plate: the broad seat shadows already model
                // the softer ambient occlusion beneath the surrounding facing bays.
                addSoftContactShadow(interiorShadows,direction*0.66f,0.1045f,z,0.19f,0.19f,105);
                for (float boltZ : {-0.070f,0.070f})
                    mesh.addCylinder(Vector3{direction*0.66f,0.105f,z+boltZ},
                                     0.013f,0.009f,6,Color(43,48,48,255));
                ++tableSupportPlateCount_;
                mesh.addCylinder(Vector3{direction*0.66f,0.08f,z},0.045f,0.71f,10,
                                 Color(119,125,123,255));
                mesh.addBox(Vector3{std::min(direction*1.41f,direction*1.31f),0.67f,z-0.16f},
                            Vector3{std::max(direction*1.41f,direction*1.31f),0.78f,z+0.16f},
                            Color(116,123,122,255));
                // A small twin power outlet is visible beneath the window-side edge.
                mesh.addBox(Vector3{std::min(direction*1.435f,direction*1.405f),0.58f,z-0.11f},
                            Vector3{std::max(direction*1.435f,direction*1.405f),0.72f,z+0.11f},
                            Color(45,61,66,255));
                for (float socketZ : {-0.055f,0.055f})
                    mesh.addBox(Vector3{std::min(direction*1.442f,direction*1.434f),0.62f,
                                        z+socketZ-0.022f},
                                Vector3{std::max(direction*1.442f,direction*1.434f),0.68f,
                                        z+socketZ+0.022f},Color(218,190,78,255));

                if (car==1)
                {
                    const float powerX=direction*0.56f;
                    mesh.addRoundedBoxXZ({powerX-0.027f,0.861f,z-0.073f},
                        {powerX+0.027f,0.865f,z+0.073f},0.009f,3,Color(48,62,68,255));
                    for (float portZ : {z-0.035f,z+0.035f})
                    {
                        mesh.addRoundedBoxXZ({powerX-0.008f,0.865f,portZ-0.006f},
                            {powerX+0.008f,0.867f,portZ+0.006f},0.003f,3,Color(157,170,173,255));
                        mesh.addBox({powerX-0.006f,0.867f,portZ-0.0025f},
                            {powerX+0.006f,0.8675f,portZ+0.0025f},Color(19,29,34,255));
                    }
                }

                if (car == 0 && table == 0 && side < 0)
                {
                    // A deliberately analogue quiet-zone cue: a slim open book lies flat without
                    // blocking either cup recess or any of the four facing seats.
                    mesh.addBox(Vector3{-1.14f,0.862f,z-0.20f},
                                Vector3{-0.90f,0.875f,z+0.18f},Color(43,69,83,255));
                    mesh.addBox(Vector3{-1.12f,0.876f,z-0.18f},
                                Vector3{-0.91f,0.883f,z-0.01f},Color(226,222,204,255));
                    mesh.addBox(Vector3{-1.12f,0.876f,z+0.01f},
                                Vector3{-0.91f,0.883f,z+0.16f},Color(226,222,204,255));
                }
                else if (car == 1 && table == 1)
                {
                    // The work car gets two open laptops at its central table. Their compact
                    // silhouette, dark screen and keyboard make the power provision meaningful.
                    const float laptopX = direction*0.94f;
                    mesh.addBox(Vector3{laptopX-0.21f,0.862f,z-0.22f},
                                Vector3{laptopX+0.21f,0.878f,z+0.08f},Color(57,63,65,255));
                    mesh.addBox(Vector3{laptopX-0.22f,0.88f,z+0.07f},
                                Vector3{laptopX+0.22f,1.16f,z+0.095f},Color(50,57,60,255));
                    mesh.addBox(Vector3{laptopX-0.19f,0.91f,z+0.064f},
                                Vector3{laptopX+0.19f,1.13f,z+0.068f},Color(62,111,128,255));
                    for (int key = 0; key < 5; ++key)
                    {
                        const float keyX = laptopX-0.16f+static_cast<float>(key)*0.08f;
                        mesh.addBox(Vector3{keyX-0.025f,0.879f,z-0.13f},
                                    Vector3{keyX+0.025f,0.884f,z-0.08f},Color(112,120,121,255));
                    }
                }
                else if (car == 3 && table == 1 && side > 0)
                {
                    addFamilyActivityBook(mesh,z);
                }
            }
        }
    }

    // High-contrast floor zoning makes the widened accessible seating bay readable at a glance.
    mesh.addBox(Vector3{-0.76f,0.081f,-18.25f}, Vector3{0.76f,0.095f,-14.70f},
                Color(44,112,139,255));
    for (float x : {-0.575f,0.575f})
        addCarRoleIcon(mesh,2,Matrix::CreateScale(2.35f)*Matrix::CreateRotationX(-kPi*0.5f)
            *Matrix::CreateTranslation(x,0.104f,-16.90f),Color(204,220,218,255));

    // Double-ended cab shells continue the body beyond the locked passenger bulkheads. They are
    // deliberately generic rather than reproducing a trademarked train, and remain inaccessible.
    for (float direction : {-1.0f, 1.0f})
    {
        const float inner = direction * 63.3f;
        const float nose = direction * 72.8f;
        const float start = std::min(inner, nose);
        const float end = std::max(inner, nose);
        const float windscreenZ0 = nose - direction * 0.08f;
        const float windscreenZ1 = nose + direction * 0.08f;
        mesh.addBox(Vector3{-1.48f,-0.08f,start}, Vector3{1.48f,0.08f,end}, floor);
        mesh.addBox(Vector3{-1.42f,2.92f,start}, Vector3{1.42f,3.08f,end}, shell);
        for (float side : {-1.0f, 1.0f})
        {
            const float x0 = side < 0.0f ? -1.50f : 1.42f;
            const float x1 = side < 0.0f ? -1.42f : 1.50f;
            mesh.addBox(Vector3{x0,0.05f,start}, Vector3{x1,1.15f,end}, trim);
            mesh.addBox(Vector3{x0,2.55f,start}, Vector3{x1,3.0f,end}, shell);
            const float sideWindowStart = std::min(inner + direction * 1.2f,
                                                   nose - direction * 1.2f);
            const float sideWindowEnd = std::max(inner + direction * 1.2f,
                                                 nose - direction * 1.2f);
            glass.addBox(Vector3{x0,1.18f,sideWindowStart},
                         Vector3{x1,2.52f,sideWindowEnd}, Color(67,126,151,42));
        }
        const float noseMin = std::min(windscreenZ0, windscreenZ1);
        const float noseMax = std::max(windscreenZ0, windscreenZ1);
        mesh.addBox(Vector3{-1.46f,0.0f,noseMin}, Vector3{1.46f,1.35f,noseMax}, trim);
        mesh.addBox(Vector3{-1.46f,2.72f,noseMin}, Vector3{1.46f,3.0f,noseMax}, shell);
        mesh.addBox(Vector3{-1.46f,1.32f,noseMin}, Vector3{-1.08f,2.75f,noseMax}, shell);
        mesh.addBox(Vector3{1.08f,1.32f,noseMin}, Vector3{1.46f,2.75f,noseMax}, shell);
        glass.addBox(Vector3{-1.06f,1.42f,noseMin}, Vector3{1.06f,2.68f,noseMax},
                     Color(50,103,126,64));
        const float deskZ0 = nose - direction * 1.2f;
        const float deskZ1 = nose - direction * 0.45f;
        mesh.addBox(Vector3{-1.08f,1.02f,std::min(deskZ0,deskZ1)},
                    Vector3{1.08f,1.28f,std::max(deskZ0,deskZ1)}, Color(50,57,59,255));

        // A low-profile crashworthy desk, two physical display bezels, controls and a power/brake
        // handle replace the old empty shelf. Luminous contents are drawn only in the active cab,
        // so direction reversal remains visible without rebuilding this immutable mesh.
        const float screenZ = nose-direction*1.21f;
        const float screenZ0 = std::min(screenZ-direction*0.040f,
                                        screenZ+direction*0.040f);
        const float screenZ1 = std::max(screenZ-direction*0.040f,
                                        screenZ+direction*0.040f);
        mesh.addRoundedBoxXY(Vector3{-0.63f,1.30f,screenZ0},
                             Vector3{0.59f,1.72f,screenZ1},0.055f,3,
                             Color(27,34,37,255));
        mesh.addBox(Vector3{-1.03f,1.255f,std::min(screenZ-direction*0.42f,screenZ)},
                    Vector3{1.03f,1.34f,std::max(screenZ-direction*0.42f,screenZ)},
                    Color(70,78,78,255));
        const float buttonZ = screenZ-direction*0.25f;
        for (int button = 0; button < 6; ++button)
        {
            const float x = -0.62f+static_cast<float>(button)*0.245f;
            const float z0 = std::min(buttonZ-direction*0.035f,
                                      buttonZ+direction*0.035f);
            const float z1 = std::max(buttonZ-direction*0.035f,
                                      buttonZ+direction*0.035f);
            mesh.addBox(Vector3{x-0.040f,1.34f,z0},Vector3{x+0.040f,1.365f,z1},
                        button == 0 ? Color(205,57,45,255)
                        : button == 5 ? Color(231,183,47,255)
                                      : Color(126,141,141,255));
        }
        const float leverZ = screenZ-direction*0.34f;
        mesh.addCylinder(Vector3{0.78f,1.30f,leverZ},0.035f,0.19f,8,
                         Color(126,132,130,255));
        mesh.addEllipsoid(Vector3{0.78f,1.52f,leverZ},Vector3{0.070f,0.055f,0.070f},
                          10,5,Color(40,47,49,255));

        // Two restrained interior wipers break up the broad windscreen and remain visible from
        // the staff-door observation pane. Their placement follows each cab's operating end.
        for (float x : {-0.42f,0.42f})
        {
            const Matrix wiper = Matrix::CreateRotationZ(x < 0.0f ? -0.49f : 0.49f)
                * Matrix::CreateTranslation(x,2.05f,nose-direction*0.095f);
            mesh.addBox(Vector3{-0.31f,-0.018f,-0.012f},
                        Vector3{0.31f,0.018f,0.012f},Color(20,25,26,255),wiper);
            mesh.addBox(Vector3{-0.025f,-0.10f,-0.018f},
                        Vector3{0.025f,0.10f,0.018f},Color(25,30,31,255),
                        Matrix::CreateTranslation(x,1.79f,nose-direction*0.096f));
        }
        ++cabControlGroupCount_;
        const float seatZ0 = nose - direction * 2.4f;
        const float seatZ1 = nose - direction * 1.75f;
        mesh.addBox(Vector3{-0.42f,0.18f,std::min(seatZ0,seatZ1)},
                    Vector3{0.42f,0.86f,std::max(seatZ0,seatZ1)}, Color(42,58,68,255));
        mesh.addBox(Vector3{-1.48f,0.43f,start}, Vector3{1.48f,0.60f,end},
                    Color(230,181,40,255));
    }

    // Gangways: a short ribbed bellows tunnel, bridge plate and passenger-scale fittings make the
    // connection read as flexible railway hardware rather than a flat black hole. All five join
    // the immutable interior batch, so this detail adds no draw call.
    for (int boundary = 1; boundary < 6; ++boundary)
    {
        const float z = -63.0f + static_cast<float>(boundary) * kCarLength;
        const Color endWall(48,57,61,255);
        const Color brushedMetal(164,168,163,255);
        mesh.addBox(Vector3{-1.47f,0.0f,z-0.24f},
                    Vector3{-0.62f,2.92f,z+0.24f},endWall);
        mesh.addBox(Vector3{0.62f,0.0f,z-0.24f},
                    Vector3{1.47f,2.92f,z+0.24f},endWall);
        // The structural header must clear the complete corrugated skin (top 2.416 m).
        mesh.addBox(Vector3{-0.62f,2.43f,z-0.24f},
                    Vector3{0.62f,2.92f,z+0.24f},endWall);
        // Finish the previously bare reverse face too; the opposite side already carries
        // the car's safety-equipment service panels. Header skins stay behind the sign.
        for (float side : {-1.0f,1.0f})
            addVestibuleServicePanels(interiorPanelMaterial,side,z+0.315f,1);
        for (float face : {-1.0f,1.0f})
        {
            const auto first=interiorPanelMaterial.data().vertices.size();
            const float front=z+face*0.250f,back=z+face*0.240f;
            interiorPanelMaterial.addRoundedBoxXY(
                {-1.415f,2.44f,std::min(front,back)},
                {1.415f,2.895f,std::max(front,back)},0.025f,3,Color(143,156,157,255));
            interiorPanelMaterial.projectXYDetailUVs(first,{0,2.44f,z},0.23f);
        }

        // Continuous rounded rubber folds, not overlapping rigid rectangular frames.
        addFlexibleGangwayBellows(interiorResilientMaterial,z);

        // A slightly crowned metal bridge closes the half-metre structural gap between the two
        // floor slabs. Its inset dark strips stay below the collision plane and provide speed-
        // independent visual footing through the doorway.
        addGangwayBridgePlate(mesh,z);
        ++nonCarpetedFloorZoneCount_;

        // Full-height grab rails have visible wall mounts. A warm header lamp and an illuminated
        // label keep the threshold readable even while the scenery is inside a tunnel.
        for (float x : {-0.46f,0.46f})
        {
            mesh.addCylinder(Vector3{x,0.12f,z},0.025f,2.13f,8,brushedMetal);
            addGrabRailEnds(mesh,x,z,0.12f,2.25f,0.025f,brushedMetal);
            for (float y : {0.31f,2.03f})
            {
                const float x0 = x < 0.0f ? x-0.12f : x;
                const float x1 = x < 0.0f ? x : x+0.12f;
                mesh.addBox(Vector3{x0,y-0.025f,z-0.035f},
                            Vector3{x1,y+0.025f,z+0.035f},brushedMetal);
            }
        }
        mesh.addBox(Vector3{-0.27f,2.305f,z-0.16f},
                    Vector3{0.27f,2.345f,z+0.16f},Color(117,129,132,255));
        interiorEmitters.addQuad(Vector3{-0.245f,2.302f,z-0.135f},
            Vector3{0.245f,2.302f,z-0.135f},Vector3{0.245f,2.302f,z+0.135f},
            Vector3{-0.245f,2.302f,z+0.135f},Color(246,241,218,255));
        mesh.addBox(Vector3{-0.42f,2.47f,z-0.255f},
                    Vector3{0.42f,2.73f,z+0.255f},Color(24,39,45,255));
        for (float face : {-1.0f,1.0f})
        {
            const float surface = z + face*0.262f;
            addDisplayText(mesh,"GANGWAY",2.66f,surface,face < 0.0f,
                           Color(177,215,215,255));
        }
    }

    // Staff-only cab bulkheads make the passenger boundary physical and legible.
    for (float z : {-63.2f, 63.2f})
    {
        // Side panels and the locked centre door leave a glazed observation window. Collision is
        // still governed by the passenger-space boundary, so the opening never grants cab access.
        mesh.addBox(Vector3{-1.48f,0.0f,z-0.10f},
                    Vector3{-0.54f,3.0f,z+0.10f}, Color(56,69,76,255));
        mesh.addBox(Vector3{0.54f,0.0f,z-0.10f},
                    Vector3{1.48f,3.0f,z+0.10f}, Color(56,69,76,255));
        // The leaf sits 15 mm behind the bulkhead face inside a proud frame; hinges, kick
        // plate and lever hardware come from InteriorFittingsGeometry.hpp.
        mesh.addBox(Vector3{-0.54f,0.0f,z-0.085f},
                    Vector3{0.54f,1.22f,z+0.085f}, Color(62,77,84,255));
        mesh.addBox(Vector3{-0.54f,2.38f,z-0.085f},
                    Vector3{0.54f,2.42f,z+0.085f}, Color(62,77,84,255));
        mesh.addBox(Vector3{-0.60f,2.42f,z-0.10f},
                    Vector3{0.60f,3.0f,z+0.10f}, Color(56,69,76,255));
        glass.addBox(Vector3{-0.48f,1.26f,z-0.10f},
                     Vector3{0.48f,2.38f,z+0.10f}, Color(86,143,161,42));
        addCabDoorHardware(mesh,z,z < 0.0f ? 1.0f : -1.0f);
        mesh.addBox(Vector3{-0.62f,0.72f,z-0.12f},
                    Vector3{0.62f,1.12f,z+0.12f}, Color(205,59,45,255));
        const bool mirrored = z > 0.0f;
        const float surface = z + (z < 0.0f ? 0.125f : -0.125f);
        mesh.addBox(Vector3{-0.56f,0.77f,surface-0.012f},
                    Vector3{0.56f,1.08f,surface+0.012f}, Color(18,24,27,255));
        addDisplayText(mesh, "STAFF ONLY", 1.01f,
                       surface + (z < 0.0f ? 0.014f : -0.014f), mirrored,
                       Color(248,202,48,255));
    }

    // Accessible toilet in the fourth car: three walls, sanitary fixtures and a hinged aisle door.
    mesh.addBox(Vector3{0.34f,0.0f,3.45f}, Vector3{1.46f,2.70f,3.55f}, Color(204,211,208,255));
    mesh.addBox(Vector3{0.34f,0.0f,4.59f}, Vector3{0.44f,2.70f,8.10f}, Color(204,211,208,255));
    mesh.addBox(Vector3{1.35f,0.0f,3.45f}, Vector3{1.46f,2.70f,8.10f}, Color(204,211,208,255));
    mesh.addBox(Vector3{0.34f,0.0f,8.00f}, Vector3{1.46f,2.70f,8.10f}, Color(204,211,208,255));
    mesh.addBox(Vector3{0.45f,0.081f,4.60f}, Vector3{1.34f,0.098f,7.98f},
                Color(57,92,102,255));
    addResilientFloor(0.455f,1.335f,0.099f,4.605f,7.975f,
                      Color(64,101,110,255));
    ++nonCarpetedFloorZoneCount_;

    // A wipe-clean lower wall band, coved skirting and panel joints give the compartment its own
    // material language instead of inheriting the saloon's flat grey shell. These pieces remain in
    // the shared interior batch and sit just inside the collision walls.
    const Color toiletWallAccent(55,118,132,255);
    const Color toiletWallJoint(177,188,187,255);
    mesh.addBox(Vector3{0.445f,0.10f,4.62f},Vector3{0.457f,0.72f,7.96f},
                toiletWallAccent);
    mesh.addBox(Vector3{1.333f,0.10f,4.62f},Vector3{1.345f,0.72f,7.96f},
                toiletWallAccent);
    mesh.addBox(Vector3{0.46f,0.10f,7.975f},Vector3{1.33f,0.72f,7.992f},
                toiletWallAccent);
    mesh.addBox(Vector3{0.445f,0.10f,4.62f},Vector3{0.475f,0.20f,7.96f},
                Color(39,77,86,255));
    mesh.addBox(Vector3{1.315f,0.10f,4.62f},Vector3{1.345f,0.20f,7.96f},
                Color(39,77,86,255));
    mesh.addBox(Vector3{0.46f,0.10f,7.955f},Vector3{1.33f,0.20f,7.992f},
                Color(39,77,86,255));
    for (float panelZ : {5.55f,6.68f})
    {
        mesh.addBox(Vector3{0.445f,0.72f,panelZ-0.014f},
                    Vector3{0.459f,2.56f,panelZ+0.014f},toiletWallJoint);
        mesh.addBox(Vector3{1.331f,0.72f,panelZ-0.014f},
                    Vector3{1.345f,2.56f,panelZ+0.014f},toiletWallJoint);
    }
    mesh.addBox(Vector3{0.445f,1.42f,4.62f},Vector3{0.460f,1.57f,7.96f},
                Color(63,137,151,255));
    mesh.addBox(Vector3{1.330f,1.42f,4.62f},Vector3{1.345f,1.57f,7.96f},
                Color(63,137,151,255));
    mesh.addBox(Vector3{0.46f,1.42f,7.964f},Vector3{1.33f,1.57f,7.992f},
                Color(63,137,151,255));
    addWallPanel(0.444f,0.74f,2.55f,4.62f,7.96f,true,
                 Color(207,216,214,255));
    addWallPanel(1.347f,0.74f,2.55f,4.62f,7.96f,false,
                 Color(207,216,214,255));
    // Fine anti-slip lanes break up the blue floor without creating a raised trip edge.
    for (float z = 4.88f; z < 7.82f; z += 0.42f)
        mesh.addBox(Vector3{0.56f,0.098f,z},Vector3{1.23f,0.102f,z+0.035f},
                    Color(94,131,139,255));

    // Rounded sanitary geometry keeps this frequently inspected compartment from reading as a
    // stack of crates while remaining inexpensive and part of the single interior batch.
    const auto sanitaryBowl = [&mesh](Vector3 centre, float rx, float rz, float depth) {
        constexpr int sides = 24;
        const auto point = [=](int index, float radius, float y) {
            const float angle = static_cast<float>(index)*2.0f*kPi/sides;
            return Vector3{centre.X+rx*radius*std::cos(angle),centre.Y+y,
                           centre.Z+rz*radius*std::sin(angle)};
        };
        for (int i = 0; i < sides; ++i)
        {
            const auto normal = [=](int index, bool inside) {
                const float angle = static_cast<float>(index)*2.0f*kPi/sides;
                const float slope = inside ? 0.42f/(depth*0.73f-0.012f) : 0.48f/depth;
                const float sign = inside ? -1.0f : 1.0f;
                return Vector3{sign*std::cos(angle)/rx,-sign*slope,
                               sign*std::sin(angle)/rz};
            };
            const Vector3 outer = point(i,1.0f,0.0f);
            const Vector3 nextOuter = point(i+1,1.0f,0.0f);
            const Vector3 inner = point(i,0.76f,-0.012f);
            const Vector3 nextInner = point(i+1,0.76f,-0.012f);
            const Vector3 bottom = point(i,0.34f,-depth*0.73f);
            const Vector3 nextBottom = point(i+1,0.34f,-depth*0.73f);
            mesh.addQuad(point(i,0.52f,-depth),outer,nextOuter,
                         point(i+1,0.52f,-depth),Color(221,225,221,255),
                         normal(i,false),normal(i,false),normal(i+1,false),normal(i+1,false));
            mesh.addQuad(outer,inner,nextInner,nextOuter,Color(242,243,236,255));
            mesh.addQuad(inner,bottom,nextBottom,nextInner,Color(190,204,203,255),
                         normal(i,true),normal(i,true),normal(i+1,true),normal(i+1,true));
            mesh.addTriangle(Vector3{centre.X,centre.Y-depth*0.73f,centre.Z},
                             nextBottom,bottom,Color(120,148,151,255));
        }
    };
    mesh.addEllipsoid(Vector3{1.04f,0.17f,7.59f},Vector3{0.145f,0.17f,0.19f},16,8,
                      Color(222,227,222,255),true);
    sanitaryBowl(Vector3{1.04f,0.57f,7.50f},0.27f,0.32f,0.25f);
    mesh.addRoundedBoxXY(Vector3{0.83f,0.56f,7.70f}, Vector3{1.25f,0.94f,7.91f},
                0.055f,3,Color(232,233,226,255));
    mesh.addBox(Vector3{0.96f,0.945f,7.76f}, Vector3{1.12f,0.965f,7.84f},
                Color(165,174,172,255));
    // Flush plate and hygienic paper dispenser remain reachable from the transfer side.
    mesh.addBox(Vector3{0.91f,1.09f,7.90f},Vector3{1.17f,1.28f,7.98f},
                Color(61,105,115,255));
    mesh.addBox(Vector3{0.98f,1.145f,7.895f},Vector3{1.10f,1.225f,7.905f},
                Color(214,222,219,255));
    mesh.addBox(Vector3{0.445f,0.82f,6.55f},Vector3{0.52f,1.20f,6.94f},
                Color(224,229,226,255));
    mesh.addBox(Vector3{0.515f,0.89f,6.62f},Vector3{0.53f,1.10f,6.87f},
                Color(58,78,83,255));

    // Compact sink and faucet beside the entrance.
    sanitaryBowl(Vector3{1.04f,0.89f,4.25f},0.25f,0.21f,0.15f);
    mesh.addRoundedBoxXZ(Vector3{0.82f,0.83f,4.00f},Vector3{1.28f,0.89f,4.105f},
                         0.025f,3,Color(224,230,230,255));
    mesh.addBox(Vector3{1.18f,0.69f,4.16f},Vector3{1.345f,0.79f,4.32f},
                Color(170,181,180,255));
    mesh.addDiscXZ(Vector3{1.04f,0.782f,4.25f},0.026f,16,Color(76,88,90,255));
    mesh.addCylinder(Vector3{1.05f,0.89f,4.06f}, 0.025f, 0.18f, 12,
                     Color(167,174,172,255));
    mesh.addRoundedBoxXZ(Vector3{1.02f,1.03f,4.035f},Vector3{1.08f,1.075f,4.235f},
                         0.024f,3,Color(181,188,186,255));
    mesh.addCylinder(Vector3{1.05f,1.01f,4.21f},0.019f,0.023f,12,
                     Color(74,84,86,255));
    // The mirror sits just inside the wall surface; placing it within the wall thickness would
    // make the opaque wall correctly occlude it from the compartment.
    mesh.addBox(Vector3{1.325f,1.05f,4.00f}, Vector3{1.345f,2.10f,4.92f},
                Color(82,145,166,255));
    mesh.addBox(Vector3{1.315f,1.88f,4.10f}, Vector3{1.322f,2.02f,4.80f},
                Color(207,226,229,255));
    const Color mirrorFrame(164,176,176,255);
    mesh.addBox(Vector3{1.305f,1.00f,3.96f},Vector3{1.350f,1.06f,4.96f},mirrorFrame);
    mesh.addBox(Vector3{1.305f,2.09f,3.96f},Vector3{1.350f,2.15f,4.96f},mirrorFrame);
    mesh.addBox(Vector3{1.305f,1.00f,3.96f},Vector3{1.350f,2.15f,4.02f},mirrorFrame);
    mesh.addBox(Vector3{1.305f,1.00f,4.90f},Vector3{1.350f,2.15f,4.96f},mirrorFrame);

    // Accessible fittings: contrasting support rail, alarm call point, soap and waste bin.
    mesh.addBox(Vector3{0.425f,0.98f,6.20f}, Vector3{0.465f,1.06f,7.28f},
                Color(231,187,48,255));
    mesh.addBox(Vector3{0.425f,0.62f,7.20f}, Vector3{0.465f,1.06f,7.28f},
                Color(231,187,48,255));
    mesh.addBox(Vector3{0.410f,1.17f,5.43f}, Vector3{0.470f,1.43f,5.67f},
                Color(205,51,45,255));
    mesh.addBox(Vector3{0.471f,1.245f,5.495f}, Vector3{0.481f,1.355f,5.605f},
                Color(248,213,63,255));
    mesh.addBox(Vector3{1.285f,1.12f,4.92f}, Vector3{1.345f,1.46f,5.18f},
                Color(224,229,226,255));
    mesh.addBox(Vector3{1.275f,1.17f,5.00f}, Vector3{1.284f,1.36f,5.10f},
                Color(61,132,151,255));
    mesh.addBox(Vector3{0.48f,0.09f,5.02f}, Vector3{0.73f,0.58f,5.48f},
                Color(118,130,132,255));
    mesh.addBox(Vector3{0.50f,0.56f,5.06f}, Vector3{0.71f,0.59f,5.44f},
                Color(47,57,60,255));

    // A red emergency pull cord and triangular handle are deliberately prominent against the
    // pale wall. A small high-level ventilation grille adds another familiar railway detail.
    mesh.addBox(Vector3{0.486f,1.13f,5.93f},Vector3{0.504f,2.52f,5.95f},
                Color(190,47,42,255));
    mesh.addBox(Vector3{0.465f,1.08f,5.89f},Vector3{0.525f,1.14f,5.99f},
                Color(235,75,55,255));
    mesh.addBox(Vector3{0.55f,2.15f,7.965f},Vector3{0.94f,2.43f,7.985f},
                Color(80,99,103,255));
    for (float x = 0.59f; x <= 0.90f; x += 0.075f)
        mesh.addBox(Vector3{x,2.18f,7.957f},Vector3{x+0.025f,2.40f,7.991f},
                    Color(178,188,187,255));

    // The WC diffuser shares the unlit saloon batch; its metal frame remains independently lit.
    mesh.addBox(Vector3{0.58f,2.61f,5.20f},Vector3{1.22f,2.68f,6.30f},
                Color(157,169,171,255));
    interiorEmitters.addQuad(Vector3{0.605f,2.607f,5.225f},
        Vector3{1.195f,2.607f,5.225f},Vector3{1.195f,2.607f,6.275f},
        Vector3{0.605f,2.607f,6.275f},Color(246,241,218,255));
    mesh.addBox(Vector3{0.42f,1.72f,3.425f}, Vector3{1.38f,2.22f,3.445f},
                Color(25,74,88,255));
    addDisplayText(mesh, "ACCESSIBLE", 2.13f, 3.416f, true,
                   Color(229,240,237,255), 0.90f);
    addDisplayText(mesh, "WC", 1.91f, 3.416f, true,
                   Color(247,194,48,255), 0.90f);
    mesh.addBox(Vector3{0.42f,1.72f,3.555f}, Vector3{1.38f,2.22f,3.575f},
                Color(25,74,88,255));
    addDisplayText(mesh, "ACCESSIBLE", 2.13f, 3.584f, false,
                   Color(229,240,237,255), 0.90f);
    addDisplayText(mesh, "WC", 1.91f, 3.584f, false,
                   Color(247,194,48,255), 0.90f);
    mesh.addBox(Vector3{0.42f,1.72f,8.105f}, Vector3{1.38f,2.22f,8.125f},
                Color(25,74,88,255));
    addDisplayText(mesh, "ACCESSIBLE", 2.13f, 8.134f, false,
                   Color(229,240,237,255), 0.90f);
    addDisplayText(mesh, "WC", 1.91f, 8.134f, false,
                   Color(247,194,48,255), 0.90f);

    // Vestibule luggage stacks and grab poles.
    const std::array<float, 6> luggagePositions = {-61.0f, -40.0f, -19.0f, 23.0f, 44.0f, 61.0f};
    const std::array<Color, 6> luggageColors = {
        Color(137,72,54,255), Color(53,92,125,255), Color(132,111,55,255),
        Color(72,105,82,255), Color(108,69,118,255), Color(143,84,48,255)};
    for (std::size_t index = 0; index < luggagePositions.size(); ++index)
    {
        const float z = luggagePositions[index];
        // The rack and lower suitcase share one restrained floor contact patch. It is long
        // enough to anchor the full rack footprint but stays behind the clear aisle edge.
        addOrientedGroundQuad(
            interiorShadows,-1.105f,0.1045f,z,
            0.62f,1.48f,0.0f,Color(7,11,12,30));
        if (index == 3U)
            mesh.addBox(Vector3{-1.39f,0.08f,z-0.75f},Vector3{-1.36f,2.10f,z+0.75f},Color(87,94,97,255));
        else
            mesh.addBox(Vector3{-1.32f,0.08f,z-0.75f}, Vector3{-0.87f,1.55f,z+0.75f}, Color(87,94,97,255));
        const Color& caseColor = luggageColors[index];
        mesh.addRoundedBoxYZ(Vector3{-1.35f,0.12f,z-0.62f},
                             Vector3{-0.82f,0.72f,z-0.05f},0.060f,3,caseColor);
        mesh.addRoundedBoxYZ(Vector3{-1.31f,0.77f,z+0.02f},
                             Vector3{-0.84f,1.29f,z+0.57f},0.055f,3,
                             luggageColors[(index+2)%luggageColors.size()]);
        floorLuggageCaseCount_ += 2;

        // Aisle-facing zipper bands, grounded caster wheels and real U-shaped pull handles break
        // up the case silhouettes. They stay behind x=-0.77 m, safely outside the 0.84 m aisle.
        mesh.addBox(Vector3{-0.819f,0.39f,z-0.55f},
                    Vector3{-0.803f,0.43f,z-0.12f},Color(55,59,59,255));
        mesh.addBox(Vector3{-0.839f,1.00f,z+0.08f},
                    Vector3{-0.823f,1.04f,z+0.51f},Color(55,59,59,255));
        for (float wheelZ : {z-0.52f,z-0.15f})
            mesh.addEllipsoid(Vector3{-0.79f,0.115f,wheelZ},Vector3{0.026f,0.045f,0.041f},
                              8,4,Color(38,43,44,255));
        ++floorLuggageWheelPairCount_;
        for (float handleZ : {z-0.47f,z-0.20f})
            mesh.addBox(Vector3{-0.817f,0.68f,handleZ-0.012f},
                        Vector3{-0.795f,0.88f,handleZ+0.012f},Color(64,68,68,255));
        mesh.addBox(Vector3{-0.817f,0.86f,z-0.47f},
                    Vector3{-0.795f,0.90f,z-0.20f},Color(55,59,59,255));
        for (float handleZ : {z+0.15f,z+0.44f})
            mesh.addBox(Vector3{-0.837f,1.25f,handleZ-0.012f},
                        Vector3{-0.815f,1.43f,handleZ+0.012f},Color(64,68,68,255));
        mesh.addBox(Vector3{-0.837f,1.41f,z+0.15f},
                    Vector3{-0.815f,1.45f,z+0.44f},Color(55,59,59,255));
        if (index == 3U) // z=23 is Car 5; the luggage array omits the toilet vestibule.
        {
            for (float strapZ : {z-0.30f,z+0.30f})
            {
                mesh.addBox({-0.785f,0.18f,strapZ-0.014f},{-0.777f,2.0f,strapZ+0.014f},
                            Color(33,48,45,255));
                mesh.addBox({-0.790f,1.08f,strapZ-0.022f},{-0.770f,1.14f,strapZ+0.022f},
                            Color(124,141,140,255));
            }
            // Car 5 owns a full-height vestibule rack rather than the generic two-case stack.
            // Shelves and lips hold two additional cabin bags entirely outside the clear aisle.
            for (float shelfY : {0.76f,1.40f,2.05f})
                mesh.addBox(Vector3{-1.39f,shelfY,z-0.72f},
                            Vector3{-0.79f,shelfY+0.045f,z+0.72f},Color(96,103,104,255));
            for (float railZ : {z-0.72f,z+0.72f})
                mesh.addBox(Vector3{-1.39f,0.08f,railZ-0.025f},
                            Vector3{-1.32f,2.10f,railZ+0.025f},Color(96,103,104,255));
            mesh.addRoundedBoxYZ(Vector3{-1.34f,1.445f,z-0.61f},
                                 Vector3{-0.85f,1.98f,z-0.08f},0.052f,3,
                                 Color(55,96,124,255));
            mesh.addRoundedBoxYZ(Vector3{-1.31f,1.445f,z+0.06f},
                                 Vector3{-0.87f,1.94f,z+0.58f},0.050f,3,
                                 Color(132,105,54,255));
            floorLuggageCaseCount_ += 2;
            mesh.addBox(Vector3{-1.18f,1.92f,z-0.43f},
                        Vector3{-1.01f,2.04f,z-0.26f},Color(55,59,59,255));
            mesh.addBox(Vector3{-1.17f,1.88f,z+0.22f},
                        Vector3{-1.00f,2.00f,z+0.39f},Color(55,59,59,255));
        }
        mesh.addCylinder(Vector3{0.78f,0.05f,z}, 0.028f, 2.35f, 8, Color(226,185,48,255));
    }

    interior_ = std::make_unique<GpuMesh>(device_, mesh.take());
    interiorMaterial_ = std::make_unique<GpuMesh>(device_,interiorMaterial.take());
    upholsteryMaterial_ = std::make_unique<GpuMesh>(device_,upholsteryMaterial.take());
    interiorPanelMaterial_ = std::make_unique<GpuMesh>(
        device_,interiorPanelMaterial.take());
    interiorResilientMaterial_ = std::make_unique<GpuMesh>(
        device_,interiorResilientMaterial.take());
    interiorShadows_ = std::make_unique<GpuMesh>(device_,interiorShadows.take());
    interiorLightGradients_ = std::make_unique<GpuMesh>(device_,interiorLighting.take());
    interiorEmitters_ = std::make_unique<GpuMesh>(device_,interiorEmitters.take());
    windowGlass_ = std::make_unique<GpuMesh>(device_, glass.take());
    for (std::size_t side = 0; side < windowReflections_.size(); ++side)
        windowReflections_[side] = std::make_unique<GpuMesh>(
            device_,glassReflections[side].take());
    sunlightPatches_ = std::make_unique<GpuMesh>(device_, sunlight.take());
}

void SceneRenderer::drawMesh(GpuMesh& mesh, const Matrix& world, const Matrix& view,
                             const Matrix& projection, RenderStats& stats)
{
    effect_->setWorldProperty(world);
    effect_->setViewProperty(view);
    effect_->setProjectionProperty(projection);
    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
    {
        pass.Apply();
        mesh.draw(device_);
        ++stats.drawCalls;
        stats.triangles += mesh.triangles();
    }
}

void SceneRenderer::drawMaterialMesh(GpuMesh& mesh, const Matrix& world, const Matrix& view,
                                     const Matrix& projection, RenderStats& stats)
{
    materialEffect_->setWorldProperty(world);
    materialEffect_->setViewProperty(view);
    materialEffect_->setProjectionProperty(projection);
    for (auto& pass : materialEffect_->getCurrentTechniqueProperty()->getPassesProperty())
    {
        pass.Apply();
        mesh.draw(device_);
        ++stats.drawCalls;
        stats.triangles += mesh.triangles();
    }
}

void SceneRenderer::drawBox(const Matrix& world, const Matrix& view, const Matrix& projection,
                            const Color& color, RenderStats& stats)
{
    // A uniform-colour primitive must not tint the next vertex-coloured mesh (for example
    // the toilet door following a door-status button, or luggage following the staff band).
    const Vector3 previousDiffuse = effect_->getDiffuseColorProperty();
    effect_->setDiffuseColorProperty(rgb(color));
    drawMesh(*cube_, world, view, projection, stats);
    effect_->setDiffuseColorProperty(previousDiffuse);
}

void SceneRenderer::drawAvatars(const Matrix& view, const Matrix& projection,
                                const Sim::PassengerSystem& passengers, double seconds,
                                float renderDelta,
                                float playerX, float playerZ,
    RenderStats& stats)
{
    if (avatars_.empty()) return;
    const auto tick = System::TimeSpan::FromTicks(166667);
    for (auto& avatar : avatars_) avatar->animation->Update(tick, true);

    if (avatarBuildCooldownFrames_ > 0) --avatarBuildCooldownFrames_;
    AvatarVisual& newest = *avatars_.back();
    const bool poolNeedsAnother = avatars_.size() < totalAvatarBudget(graphicsQuality_);
    if (!newest.gpuPrimed
        && newest.renderer->getStateProperty() == AvatarRendererState::Ready)
    {
        // CNA builds avatar models asynchronously and performs their first texture upload in
        // Draw(). Prime every completed renderer off-camera, including the final pool member.
        // Starting another background build is deferred across a short bounded quiescence window,
        // so this upload crosses several presentation boundaries before loader work resumes.
        newest.renderer->setWorldProperty(Matrix::CreateTranslation(0.0f,-1'000.0f,0.0f));
        newest.renderer->setViewProperty(view);
        newest.renderer->setProjectionProperty(projection);
        newest.renderer->Draw(newest.animation.get());
        newest.gpuPrimed = true;
        avatarBuildCooldownFrames_ = kAvatarUploadQuiescenceFrames;
    }
    else if (poolNeedsAnother && newest.gpuPrimed && avatarBuildCooldownFrames_ == 0)
    {
        addNextAvatarVisual();
    }
    auto& readyVisuals = readyAvatarScratch_;
    readyVisuals.clear();
    const std::size_t onboardBudget = onboardAvatarBudget(graphicsQuality_);
    for (std::size_t index = 0;
         index < std::min(avatars_.size(), onboardBudget); ++index)
        if (avatars_[index]->renderer->getStateProperty() == AvatarRendererState::Ready)
            readyVisuals.push_back(index);
    if (readyVisuals.empty()) return;

    auto& visible = visiblePassengerScratch_;
    visible.clear();
    for (const Sim::Passenger& passenger : passengers.passengers())
    {
        if (passenger.state == Sim::PassengerState::Offboard) continue;
        if (std::abs(static_cast<float>(passenger.z) - playerZ) > 28.0f) continue;
        // Keep ordinary conversational distance visible. Player/NPC collision already holds
        // approaching actors just over a metre away; only suppress the pathological case where
        // the near plane would enter an avatar because an AI state changed on top of the camera.
        if (std::hypot(static_cast<float>(passenger.x) - playerX,
                       static_cast<float>(passenger.z) - playerZ) < 0.42f) continue;
        visible.push_back(&passenger);
    }

    // Give stationary aisle and vestibule passengers a concrete social focus instead of leaving
    // them as isolated forward-facing figures. Pair each one with the nearest unclaimed seated
    // passenger in the same car. Keeping this presentation relationship deterministic and local
    // avoids changing the navigation/collision simulation while still producing reciprocal gaze.
    std::vector<std::pair<const Sim::Passenger*,const Sim::Passenger*>> onboardSocialPairs;
    for (const Sim::Passenger& standing : passengers.passengers())
    {
        if (standing.role != Sim::PassengerRole::Passenger || standing.seat.has_value()
            || standing.exitingSeat.has_value()
            || (standing.state != Sim::PassengerState::Standing
                && (standing.state != Sim::PassengerState::WaitingAtVestibule
                    || std::abs(standing.targetZ-standing.z) >= 0.12)))
            continue;
        const Sim::Passenger* nearest = nullptr;
        double nearestDistanceSquared = 2.65*2.65;
        for (const Sim::Passenger& seatedCandidate : passengers.passengers())
        {
            if (seatedCandidate.role != Sim::PassengerRole::Passenger
                || seatedCandidate.state != Sim::PassengerState::Seated
                || !seatedCandidate.seat.has_value() || seatedCandidate.car != standing.car)
                continue;
            if (std::any_of(onboardSocialPairs.begin(),onboardSocialPairs.end(),
                    [&seatedCandidate](const auto& pair) {
                        return pair.second == &seatedCandidate;
                    }))
                continue;
            const double deltaX = seatedCandidate.x-standing.x;
            const double deltaZ = seatedCandidate.z-standing.z;
            const double distanceSquared = deltaX*deltaX+deltaZ*deltaZ;
            if (distanceSquared < 0.65*0.65 || distanceSquared >= nearestDistanceSquared)
                continue;
            nearest = &seatedCandidate;
            nearestDistanceSquared = distanceSquared;
        }
        if (nearest != nullptr)
            onboardSocialPairs.emplace_back(&standing,nearest);
    }
    const auto hasPersistentAssignment = [this](const Sim::Passenger& passenger) {
        return std::any_of(onboardAvatarAssignments_.begin(),onboardAvatarAssignments_.end(),
            [&passenger](const std::optional<AvatarAssignment>& assignment) {
                return assignment.has_value()
                    && assignment->passengerId == passenger.id
                    && assignment->appearanceSeed == passenger.appearanceSeed;
            });
    };
    std::stable_sort(visible.begin(), visible.end(),
        [playerX,playerZ,&hasPersistentAssignment](const auto* a, const auto* b) {
        const auto rolePriority = [](Sim::PassengerRole role) {
            if (role == Sim::PassengerRole::Conductor) return 0;
            if (role == Sim::PassengerRole::Driver) return 1;
            return 2;
        };
        const int priorityA = rolePriority(a->role);
        const int priorityB = rolePriority(b->role);
        if (priorityA != priorityB) return priorityA < priorityB;
        // A modest hysteresis bonus preserves a face and outfit while two nearby passengers trade
        // places around the pool boundary. A substantially closer newcomer still wins, so an
        // assigned person at the edge of the 28 m visibility window cannot starve the foreground.
        const float distanceA = std::hypot(static_cast<float>(a->x) - playerX,
                                           static_cast<float>(a->z) - playerZ)
                              - (hasPersistentAssignment(*a) ? 4.0f : 0.0f);
        const float distanceB = std::hypot(static_cast<float>(b->x) - playerX,
                                           static_cast<float>(b->z) - playerZ)
                              - (hasPersistentAssignment(*b) ? 4.0f : 0.0f);
        return distanceA < distanceB;
    });

    // Soft blob shadows are intentionally local to the train. They ground CNA avatars against
    // the carriage floor without a shadow-map pass or renderer-specific assumptions.
    if (graphicsQuality_ >= 1)
    {
        effect_->setLightingEnabledProperty(false);
        effect_->setDiffuseColorProperty(Vector3::One);
        device_.setBlendStateProperty(BlendState::NonPremultiplied);
        device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
        const std::size_t shadowCount = std::min(visible.size(), readyVisuals.size());
        for (std::size_t index = 0; index < shadowCount; ++index)
        {
            const Sim::Passenger& passenger = *visible[index];
            const bool seated = passenger.state == Sim::PassengerState::Seated
                             || passenger.state == Sim::PassengerState::LookingOutside
                             || passenger.state == Sim::PassengerState::Talking;
            const Matrix shadowWorld = Matrix::CreateScale(seated ? 0.34f : 0.31f, 1.0f,
                                                            seated ? 0.29f : 0.22f)
                                     * Matrix::CreateTranslation(static_cast<float>(passenger.x),
                                                                 kTrainFloorElevation+0.086f,
                                                                 static_cast<float>(passenger.z));
            drawMesh(*contactShadow_, shadowWorld, view, projection, stats);
        }
        device_.setBlendStateProperty(BlendState::Opaque);
        device_.setDepthStencilStateProperty(DepthStencilState::Default);
        effect_->setLightingEnabledProperty(true);
    }

    const std::size_t drawableCount = std::min(visible.size(), readyVisuals.size());
    const Matrix avatarViewProjection = view*projection;
    const auto assignmentIsActive = [&visible,drawableCount](const AvatarAssignment& assignment) {
        return std::any_of(visible.begin(), visible.begin() + static_cast<std::ptrdiff_t>(drawableCount),
            [&assignment](const Sim::Passenger* passenger) {
                return passenger->id == assignment.passengerId
                    && passenger->appearanceSeed == assignment.appearanceSeed;
            });
    };
    for (std::size_t slot = 0; slot < onboardAvatarAssignments_.size(); ++slot)
        if (slot >= readyVisuals.size()
            || (onboardAvatarAssignments_[slot].has_value()
                && !assignmentIsActive(*onboardAvatarAssignments_[slot])))
            onboardAvatarAssignments_[slot].reset();

    std::size_t drawn = 0;
    std::size_t submitted = 0;
    for (const Sim::Passenger* passengerPointer : visible)
    {
        if (drawn >= drawableCount) break;
        const Sim::Passenger& passenger = *passengerPointer;
        const bool seated = passenger.state == Sim::PassengerState::Seated
                         || passenger.state == Sim::PassengerState::LookingOutside
                         || passenger.state == Sim::PassengerState::Talking;
        std::optional<std::size_t> visualSlot;
        for (std::size_t slot = 0; slot < readyVisuals.size(); ++slot)
            if (onboardAvatarAssignments_[slot].has_value()
                && (passenger.role == Sim::PassengerRole::Conductor ? slot == 0 : slot != 0)
                && onboardAvatarAssignments_[slot]->passengerId == passenger.id
                && onboardAvatarAssignments_[slot]->appearanceSeed == passenger.appearanceSeed)
            {
                visualSlot = slot;
                break;
            }
        if (!visualSlot.has_value())
        {
            const std::size_t first = passenger.role == Sim::PassengerRole::Conductor ? 0U : 1U;
            const std::size_t count = passenger.role == Sim::PassengerRole::Conductor
                ? std::min<std::size_t>(1U,readyVisuals.size())
                : readyVisuals.size() - std::min(first,readyVisuals.size());
            const std::size_t preferred = count == 0 ? 0U
                : first + passenger.appearanceSeed % count;
            for (std::size_t offset = 0; offset < count; ++offset)
            {
                const std::size_t candidate = first + (preferred-first+offset) % count;
                if (onboardAvatarAssignments_[candidate].has_value()) continue;
                onboardAvatarAssignments_[candidate] = AvatarAssignment{
                    passenger.id, passenger.appearanceSeed};
                AvatarVisual& assignedVisual = *avatars_[readyVisuals[candidate]];
                assignedVisual.seatedPoseBlend = seated ? 1.0f : 0.0f;
                assignedVisual.walkingPoseBlend = 0.0f;
                assignedVisual.gazeYaw = 0.0f;
                assignedVisual.gazePitch = 0.0f;
                assignedVisual.gaitPhaseRadians = seededAvatarGaitPhase(
                    passenger.appearanceSeed);
                assignedVisual.motionPositionInitialized = false;
                assignedVisual.facingInitialized = false;
                visualSlot = candidate;
                break;
            }
        }
        if (!visualSlot.has_value()) continue;
        AvatarVisual& visual = *avatars_[readyVisuals[*visualSlot]];
        const float seatedTarget = seated ? 1.0f : 0.0f;
        const float seatedStep = renderDelta / 0.55f;
        visual.seatedPoseBlend += std::clamp(
            seatedTarget-visual.seatedPoseBlend,-seatedStep,seatedStep);
        const float poseBlend = visual.seatedPoseBlend * visual.seatedPoseBlend
                              * (3.0f-2.0f*visual.seatedPoseBlend);
        const Sim::Seat* exitingSeat = nullptr;
        if (passenger.exitingSeat.has_value()
            && *passenger.exitingSeat < passengers.seats().size())
            exitingSeat = &passengers.seats()[*passenger.exitingSeat];
        const AvatarSeatExitPlacement seatExitPlacement = stabilizeAvatarSeatExit(
            passenger.x,passenger.z,
            exitingSeat != nullptr ? exitingSeat->x : passenger.x,
            exitingSeat != nullptr ? exitingSeat->z : passenger.z,
            poseBlend,exitingSeat != nullptr);
        const float seatTransferWeight = avatarSeatTransferWeight(
            poseBlend,seated || exitingSeat != nullptr);
        const float avatarX = static_cast<float>(seatExitPlacement.x);
        const int poseRow = Sim::TrainInterior::seatRowIndex(passenger.car,
            exitingSeat != nullptr ? exitingSeat->z : passenger.z);
        // Clear the 0.35 m cushion front with the knee before the shin turns down. Keep this
        // visual fit separate from the navigation/seat-occupancy coordinates.
        const float seatForward = poseRow%2 == 0 ? -1.0f : 1.0f;
        const float avatarZ = static_cast<float>(seatExitPlacement.z)+seatForward*0.10f*poseBlend;
        const bool submitAvatar = sphereMayBeInView(avatarViewProjection,
            Vector3{avatarX,kTrainFloorElevation+1.0f,avatarZ},1.5f);
        if (seatTransferWeight > 0.001f)
            ++stats.seatTransitionAvatars;
        const bool walking = passenger.state == Sim::PassengerState::Walking
                          || passenger.state == Sim::PassengerState::WalkingToSeat
                          || passenger.state == Sim::PassengerState::PreparingToLeave
                          || passenger.state == Sim::PassengerState::VisitingToilet
                          || passenger.state == Sim::PassengerState::LeavingToilet
                          || passenger.state == Sim::PassengerState::Boarding
                          || passenger.state == Sim::PassengerState::Alighting;
        const AvatarGaitStep gait = advanceAvatarGait(
            visual.gaitPhaseRadians,visual.previousPassengerX,visual.previousPassengerZ,
            passenger.x,passenger.z,visual.motionPositionInitialized,walking);
        visual.gaitPhaseRadians = gait.phaseRadians;
        visual.previousPassengerX = passenger.x;
        visual.previousPassengerZ = passenger.z;
        visual.motionPositionInitialized = true;
        // Drive the leg layer from actual displacement. A navigator waiting for another person
        // now settles its feet instead of walking on the spot, and render-only screenshot warm-up
        // cannot advance a frozen passenger's stride.
        const float walkingTarget = gait.moving ? 1.0f : 0.0f;
        const float walkingStep = renderDelta / (gait.moving ? 0.18f : 0.24f);
        visual.walkingPoseBlend += std::clamp(
            walkingTarget-visual.walkingPoseBlend,-walkingStep,walkingStep);
        const float locomotionBlend = visual.walkingPoseBlend*(1.0f-poseBlend);
        if (gait.moving && locomotionBlend > 0.001f)
            ++stats.distanceDrivenGaitAvatars;
        const float gaitSwing = std::sin(visual.gaitPhaseRadians);
        const float standingY = 0.06f
                              + 0.022f*std::abs(gaitSwing)*locomotionBlend;
        const float y = kTrainFloorElevation + standingY + (0.03f-standingY)*poseBlend;
        const float scale = 0.96f + (0.88f-0.96f)*poseBlend;
        const auto socialPair = std::find_if(
            onboardSocialPairs.begin(),onboardSocialPairs.end(),
            [&passenger](const auto& pair) {
                return pair.first == &passenger || pair.second == &passenger;
            });
        const Sim::Passenger* onboardSocialPartner = socialPair == onboardSocialPairs.end()
            ? nullptr : socialPair->first == &passenger ? socialPair->second : socialPair->first;
        const bool onboardSocial = onboardSocialPartner != nullptr && !walking && !gait.moving;
        const bool standingSocialSource = onboardSocial
            && socialPair->first == &passenger;
        float yaw = avatarFacingDirection(0.0,passenger.targetZ-passenger.z);
        if (passenger.state == Sim::PassengerState::Boarding
            || passenger.state == Sim::PassengerState::Alighting)
            yaw = passenger.targetX >= passenger.x ? -1.5707963f : 1.5707963f;
        if (passenger.state == Sim::PassengerState::WalkingToSeat
            && std::abs(passenger.targetZ - passenger.z) < 0.09)
            yaw = passenger.targetX >= passenger.x ? -1.5707963f : 1.5707963f;
        if (exitingSeat != nullptr)
            yaw = exitingSeat->x < 0.0 ? -1.5707963f : 1.5707963f;
        if (passenger.role == Sim::PassengerRole::Driver)
            yaw = avatarFacingDirection(0.0,passenger.z >= 0.0 ? 1.0 : -1.0);
        if (passenger.seat.has_value())
        {
            const int row = Sim::TrainInterior::seatRowIndex(passenger.car, passenger.z);
            // Looking outside must not rotate the seated pelvis and knees into the armrest.
            // Keep the seat orientation authoritative and express the glance in upper bones.
            yaw = avatarSeatFacing(row);
        }
        if (standingSocialSource)
            yaw = avatarFacingDirection(
                static_cast<float>(onboardSocialPartner->x-passenger.x),
                static_cast<float>(onboardSocialPartner->z-passenger.z));
        if (!onboardSocial && !seated && passenger.role != Sim::PassengerRole::Driver
            && std::abs(passenger.x) < 0.50)
        {
            const bool passingShoulderToShoulder = std::any_of(
                passengers.passengers().begin(),passengers.passengers().end(),
                [&passenger](const Sim::Passenger& other) {
                    if (&other == &passenger || other.role == Sim::PassengerRole::Driver
                        || other.seat.has_value() || std::abs(other.x) >= 0.50
                        || passenger.x*other.x >= 0.0) return false;
                    return std::abs(passenger.z-other.z) < 0.72
                        && std::hypot(passenger.x-other.x,passenger.z-other.z) < 0.92;
                });
            if (passingShoulderToShoulder)
                yaw += passenger.x < 0.0 ? -0.42f : 0.42f;
        }
        if (!visual.facingInitialized)
        {
            visual.facingYaw = yaw;
            visual.facingInitialized = true;
        }
        else
        {
            const float yawDelta = std::atan2(
                std::sin(yaw-visual.facingYaw),std::cos(yaw-visual.facingYaw));
            const float maximumTurn = renderDelta*(walking ? 4.8f : 3.2f);
            visual.facingYaw += std::clamp(yawDelta,-maximumTurn,maximumTurn);
        }
        yaw = visual.facingYaw;
        float gazeTargetYaw = 0.0f;
        float gazeTargetPitch = 0.0f;
        bool gazeActive = seated || (!walking && passenger.role != Sim::PassengerRole::Driver);
        const bool readingIdle = seated
            && passenger.state == Sim::PassengerState::Seated
            && !onboardSocial
            && passenger.appearanceSeed%4U == 0U;
        const bool phoneIdle = !walking
            && passenger.role == Sim::PassengerRole::Passenger
            && (passenger.state == Sim::PassengerState::Seated
                || passenger.state == Sim::PassengerState::Standing
                || passenger.state == Sim::PassengerState::WaitingAtVestibule)
            && !readingIdle && !onboardSocial && passenger.appearanceSeed%5U == 1U;
        const float playerDistance = std::hypot(
            avatarX-playerX,avatarZ-playerZ);
        const bool playerAcknowledgement = gazeActive
            && playerDistance > 0.70f && playerDistance < 2.55f
            && (passenger.role == Sim::PassengerRole::Conductor
                || passenger.appearanceSeed%3U == 0U)
            && !onboardSocial
            && passenger.state != Sim::PassengerState::Talking
            && passenger.state != Sim::PassengerState::LookingOutside;
        const bool conductorUsingDevice = passenger.role == Sim::PassengerRole::Conductor
            && !walking && !seated;
        if (passenger.state == Sim::PassengerState::Talking)
        {
            const auto partner = std::find_if(
                passengers.passengers().begin(),passengers.passengers().end(),
                [&passenger](const Sim::Passenger& candidate) {
                    return candidate.id != passenger.id
                        && candidate.state == Sim::PassengerState::Talking
                        && candidate.car == passenger.car
                        && candidate.seat.has_value()
                        && std::abs(candidate.z-passenger.z) < 0.05
                        && candidate.x*passenger.x > 0.0
                        && std::abs(candidate.x-passenger.x) < 0.75;
                });
            if (partner != passengers.passengers().end())
            {
                const float absolutePartnerYaw = avatarFacingDirection(
                    static_cast<float>(partner->x-passenger.x),
                    static_cast<float>(partner->z-passenger.z));
                const float relativePartnerYaw = std::atan2(
                    std::sin(absolutePartnerYaw-yaw),std::cos(absolutePartnerYaw-yaw));
                gazeTargetYaw = std::clamp(relativePartnerYaw,-0.52f,0.52f);
            }
        }
        else if (onboardSocial)
        {
            const float absolutePartnerYaw = avatarFacingDirection(
                static_cast<float>(onboardSocialPartner->x-passenger.x),
                static_cast<float>(onboardSocialPartner->z-passenger.z));
            const float relativePartnerYaw = std::atan2(
                std::sin(absolutePartnerYaw-yaw),std::cos(absolutePartnerYaw-yaw));
            gazeTargetYaw = std::clamp(relativePartnerYaw,-0.52f,0.52f);
            gazeTargetPitch = standingSocialSource ? 0.075f : -0.065f;
        }
        else if (passenger.state == Sim::PassengerState::LookingOutside)
        {
            gazeTargetYaw = avatarWindowGaze(passenger.x,yaw)+0.045f*std::sin(
                static_cast<float>(seconds)*0.31f
                + static_cast<float>(passenger.appearanceSeed%173U)*0.047f);
            gazeTargetPitch = -0.035f+0.025f*std::sin(
                static_cast<float>(seconds)*0.19f
                + static_cast<float>(passenger.appearanceSeed%89U)*0.061f);
        }
        else if (readingIdle)
        {
            gazeTargetPitch = 0.16f;
            gazeTargetYaw = 0.035f*std::sin(
                static_cast<float>(seconds)*0.37f
                + static_cast<float>(passenger.appearanceSeed%67U));
        }
        else if (phoneIdle)
        {
            gazeTargetPitch = 0.18f;
            gazeTargetYaw = 0.025f*std::sin(
                static_cast<float>(seconds)*0.29f
                + static_cast<float>(passenger.appearanceSeed%61U));
        }
        else if (playerAcknowledgement)
        {
            const float absolutePlayerYaw = avatarFacingDirection(
                playerX-avatarX,playerZ-avatarZ);
            const float relativePlayerYaw = std::atan2(
                std::sin(absolutePlayerYaw-yaw),std::cos(absolutePlayerYaw-yaw));
            gazeTargetYaw = std::clamp(relativePlayerYaw,-0.48f,0.48f);
            gazeTargetPitch = -0.025f;
        }
        else if (gazeActive)
        {
            gazeTargetYaw = 0.16f*std::sin(
                static_cast<float>(seconds)*0.23f
                + static_cast<float>(passenger.appearanceSeed%151U)*0.052f);
            gazeTargetPitch = 0.025f*std::sin(
                static_cast<float>(seconds)*0.17f
                + static_cast<float>(passenger.appearanceSeed%107U)*0.043f);
        }
        if (conductorUsingDevice && !playerAcknowledgement)
            gazeTargetPitch = 0.14f;
        const float gazeStep = renderDelta*0.85f;
        visual.gazeYaw += std::clamp(
            gazeTargetYaw-visual.gazeYaw,-gazeStep,gazeStep);
        visual.gazePitch += std::clamp(
            gazeTargetPitch-visual.gazePitch,-gazeStep,gazeStep);
        const bool gazePoseActive = std::abs(visual.gazeYaw) > 0.002f
                                 || std::abs(visual.gazePitch) > 0.002f;
        if (gazeActive) ++stats.animatedGazeAvatars;
        if (readingIdle) ++stats.readingIdleAvatars;
        if (phoneIdle) ++stats.phoneCheckingAvatars;
        if (onboardSocial || passenger.state == Sim::PassengerState::Talking)
            ++stats.onboardSocialAvatars;
        if (playerAcknowledgement) ++stats.playerAcknowledgingAvatars;
        const Matrix world = Matrix::CreateScale(scale)
                           * Matrix::CreateRotationY(yaw)
                           * Matrix::CreateTranslation(avatarX,y,avatarZ);
        const auto cabinLight = avatarCabinLighting(
            effect_->DirectionalLight0.getDirectionProperty(),tunnelLightingMix_);
        visual.renderer->setAmbientLightColorProperty(Vector3{0.56f,0.58f,0.62f});
        visual.renderer->setLightDirectionProperty(cabinLight.direction);
        visual.renderer->setLightColorProperty(cabinLight.diffuse);
        visual.renderer->setWorldProperty(world);
        visual.renderer->setViewProperty(view);
        visual.renderer->setProjectionProperty(projection);
        if (poseBlend > 0.001f || locomotionBlend > 0.001f || gazePoseActive
            || onboardSocial)
        {
            // CNA's stock set has expressive standing idles but no seated or walking clip.
            // Preserve its animated upper body and layer only the major local joints we need.
            const auto source = visual.animation->getBoneTransformsProperty();
            auto& bones = visual.boneScratch;
            bones.assign(source.begin(), source.end());
            const auto hipLeft = static_cast<std::size_t>(AvatarBone::HipLeft);
            const auto hipRight = static_cast<std::size_t>(AvatarBone::HipRight);
            const auto kneeLeft = static_cast<std::size_t>(AvatarBone::KneeLeft);
            const auto kneeRight = static_cast<std::size_t>(AvatarBone::KneeRight);
            const auto ankleLeft = static_cast<std::size_t>(AvatarBone::AnkleLeft);
            const auto ankleRight = static_cast<std::size_t>(AvatarBone::AnkleRight);
            const auto toeLeft = static_cast<std::size_t>(AvatarBone::ToeLeft);
            const auto toeRight = static_cast<std::size_t>(AvatarBone::ToeRight);
            const auto backLower = static_cast<std::size_t>(AvatarBone::BackLower);
            bones[backLower] = rotatedBone(
                bones[backLower],Matrix::CreateRotationX(
                    -0.12f*seatTransferWeight));
            bones[hipLeft] = rotatedBone(
                bones[hipLeft], Matrix::CreateRotationX(
                    1.55f*poseBlend + gaitSwing*0.48f*locomotionBlend));
            bones[hipRight] = rotatedBone(
                bones[hipRight], Matrix::CreateRotationX(
                    1.55f*poseBlend - gaitSwing*0.48f*locomotionBlend));
            bones[kneeLeft] = rotatedBone(
                bones[kneeLeft], Matrix::CreateRotationX(
                    -1.55f*poseBlend
                    + std::max(0.0f,-gaitSwing)*0.55f*locomotionBlend));
            bones[kneeRight] = rotatedBone(
                bones[kneeRight], Matrix::CreateRotationX(
                    -1.55f*poseBlend
                    + std::max(0.0f,gaitSwing)*0.55f*locomotionBlend));
            bones[ankleLeft] = rotatedBone(
                bones[ankleLeft],Matrix::CreateRotationX(
                    gaitSwing*0.18f*locomotionBlend));
            bones[ankleRight] = rotatedBone(
                bones[ankleRight],Matrix::CreateRotationX(
                    -gaitSwing*0.18f*locomotionBlend));
            bones[toeLeft] = rotatedBone(
                bones[toeLeft],Matrix::CreateRotationX(
                    std::max(0.0f,-gaitSwing)*0.11f*locomotionBlend));
            bones[toeRight] = rotatedBone(
                bones[toeRight],Matrix::CreateRotationX(
                    std::max(0.0f,gaitSwing)*0.11f*locomotionBlend));
            float conversationalHeadYaw = 0.0f;
            if (poseBlend > 0.001f)
            {
                // Stock Stand1 clasps hands behind the back; that pose intersects the seat.
                // Establish a lap-rest base before activity-specific gestures override it.
                // Keep native translations and blend during the existing sit/stand transition.
                for (bool left : {true,false})
                {
                    const auto shoulder = static_cast<std::size_t>(left
                        ? AvatarBone::ShoulderLeft : AvatarBone::ShoulderRight);
                    const auto elbow = static_cast<std::size_t>(left
                        ? AvatarBone::ElbowLeft : AvatarBone::ElbowRight);
                    const auto wrist = static_cast<std::size_t>(left
                        ? AvatarBone::WristLeft : AvatarBone::WristRight);
                    const Matrix shoulderRest = rotatedBone(bones[shoulder],
                        Matrix::CreateRotationZ(left ? 0.22f : -0.22f)
                        * Matrix::CreateRotationX(0.15f));
                    const Matrix elbowRest = rotatedBone(bones[elbow],Matrix::CreateRotationX(0.85f));
                    const Matrix wristRest = rotatedBone(bones[wrist],Matrix::getIdentityProperty());
                    bones[shoulder] = Matrix::Lerp(bones[shoulder],shoulderRest,poseBlend);
                    bones[elbow] = Matrix::Lerp(bones[elbow],elbowRest,poseBlend);
                    bones[wrist] = Matrix::Lerp(bones[wrist],wristRest,poseBlend);
                }
            }
            if (seated)
            {
                if (passenger.state == Sim::PassengerState::Talking)
                {
                    const float gesture = std::sin(static_cast<float>(seconds) * 2.15f
                        + static_cast<float>(passenger.appearanceSeed % 131U) * 0.071f);
                    conversationalHeadYaw = gesture*0.10f*poseBlend;
                    const bool useLeftArm = passenger.appearanceSeed % 2U == 0U;
                    const auto shoulder = static_cast<std::size_t>(useLeftArm
                        ? AvatarBone::ShoulderLeft : AvatarBone::ShoulderRight);
                    const auto elbow = static_cast<std::size_t>(useLeftArm
                        ? AvatarBone::ElbowLeft : AvatarBone::ElbowRight);
                    bones[shoulder] = rotatedBone(
                        bones[shoulder], Matrix::CreateRotationX(
                            (-0.16f + gesture * 0.11f)*poseBlend));
                    bones[elbow] = rotatedBone(
                        bones[elbow], Matrix::CreateRotationZ(
                            (useLeftArm ? -1.0f : 1.0f)
                            * (0.24f + std::abs(gesture) * 0.12f)*poseBlend));
                }
                else if (readingIdle)
                {
                    const auto shoulderLeft = static_cast<std::size_t>(
                        AvatarBone::ShoulderLeft);
                    const auto shoulderRight = static_cast<std::size_t>(
                        AvatarBone::ShoulderRight);
                    const auto elbowLeft = static_cast<std::size_t>(AvatarBone::ElbowLeft);
                    const auto elbowRight = static_cast<std::size_t>(AvatarBone::ElbowRight);
                    bones[shoulderLeft] = rotatedBone(
                        bones[shoulderLeft],Matrix::CreateRotationX(-0.24f*poseBlend));
                    bones[shoulderRight] = rotatedBone(
                        bones[shoulderRight],Matrix::CreateRotationX(-0.24f*poseBlend));
                    bones[elbowLeft] = rotatedBone(
                        bones[elbowLeft],Matrix::CreateRotationZ(-0.48f*poseBlend));
                    bones[elbowRight] = rotatedBone(
                        bones[elbowRight],Matrix::CreateRotationZ(0.48f*poseBlend));
                }
            }
            if (standingSocialSource)
            {
                const float gesture = std::sin(static_cast<float>(seconds)*1.73f
                    + static_cast<float>(passenger.appearanceSeed%127U)*0.083f);
                conversationalHeadYaw += gesture*0.045f;
                const bool useLeftArm = passenger.appearanceSeed%2U == 0U;
                const auto shoulder = static_cast<std::size_t>(useLeftArm
                    ? AvatarBone::ShoulderLeft : AvatarBone::ShoulderRight);
                const auto elbow = static_cast<std::size_t>(useLeftArm
                    ? AvatarBone::ElbowLeft : AvatarBone::ElbowRight);
                bones[shoulder] = rotatedBone(
                    bones[shoulder],Matrix::CreateRotationX(-0.10f+gesture*0.055f));
                bones[elbow] = rotatedBone(
                    bones[elbow],Matrix::CreateRotationZ(
                        (useLeftArm ? -1.0f : 1.0f)
                        * (0.16f+std::abs(gesture)*0.10f)));
            }
            if (conductorUsingDevice)
            {
                const auto shoulderLeft = static_cast<std::size_t>(AvatarBone::ShoulderLeft);
                const auto shoulderRight = static_cast<std::size_t>(AvatarBone::ShoulderRight);
                const auto elbowLeft = static_cast<std::size_t>(AvatarBone::ElbowLeft);
                const auto elbowRight = static_cast<std::size_t>(AvatarBone::ElbowRight);
                bones[shoulderLeft] = rotatedBone(
                    bones[shoulderLeft],Matrix::CreateRotationX(-0.42f));
                bones[shoulderRight] = rotatedBone(
                    bones[shoulderRight],Matrix::CreateRotationX(-0.42f));
                bones[elbowLeft] = rotatedBone(
                    bones[elbowLeft],Matrix::CreateRotationZ(-0.68f));
                bones[elbowRight] = rotatedBone(
                    bones[elbowRight],Matrix::CreateRotationZ(0.68f));
            }
            if (phoneIdle)
            {
                const auto shoulderLeft = static_cast<std::size_t>(AvatarBone::ShoulderLeft);
                const auto shoulderRight = static_cast<std::size_t>(AvatarBone::ShoulderRight);
                const auto elbowLeft = static_cast<std::size_t>(AvatarBone::ElbowLeft);
                const auto elbowRight = static_cast<std::size_t>(AvatarBone::ElbowRight);
                bones[shoulderLeft] = rotatedBone(
                    bones[shoulderLeft],Matrix::CreateRotationX(-0.31f));
                bones[shoulderRight] = rotatedBone(
                    bones[shoulderRight],Matrix::CreateRotationX(-0.47f));
                bones[elbowLeft] = rotatedBone(
                    bones[elbowLeft],Matrix::CreateRotationZ(-0.46f));
                bones[elbowRight] = rotatedBone(
                    bones[elbowRight],Matrix::CreateRotationZ(0.72f));
            }
            if (gazePoseActive || std::abs(conversationalHeadYaw) > 0.001f)
            {
                const auto backUpper = static_cast<std::size_t>(AvatarBone::BackUpper);
                bones[backUpper] = rotatedBone(
                    bones[backUpper],
                    Matrix::CreateRotationY(visual.gazeYaw*0.18f)
                    * Matrix::CreateRotationX(visual.gazePitch*0.08f));
                const auto head = static_cast<std::size_t>(AvatarBone::Head);
                bones[head] = rotatedBone(
                    bones[head],
                    Matrix::CreateRotationY(visual.gazeYaw+conversationalHeadYaw)
                    * Matrix::CreateRotationX(visual.gazePitch));
            }
            if (locomotionBlend > 0.001f)
            {
                const auto shoulderLeft = static_cast<std::size_t>(AvatarBone::ShoulderLeft);
                const auto shoulderRight = static_cast<std::size_t>(AvatarBone::ShoulderRight);
                bones[shoulderLeft] = rotatedBone(
                    bones[shoulderLeft], Matrix::CreateRotationX(
                        -gaitSwing*0.32f*locomotionBlend));
                bones[shoulderRight] = rotatedBone(
                    bones[shoulderRight], Matrix::CreateRotationX(
                        gaitSwing*0.32f*locomotionBlend));
            }
            applyPassengerProportions(bones);
            if (submitAvatar)
                visual.renderer->Draw(bones, visual.animation->getExpressionProperty());
        }
        else
        {
            if (submitAvatar)
            {
                const auto source=visual.animation->getBoneTransformsProperty();
                auto& bones=visual.boneScratch;
                bones.assign(source.begin(),source.end());
                applyPassengerProportions(bones);
                visual.renderer->Draw(bones,visual.animation->getExpressionProperty());
            }
        }
        if (!submitAvatar)
        {
            ++drawn; // Preserve the existing nearby-assigned population diagnostic.
            ++stats.culledOnboardAvatars;
            continue;
        }
        ++submitted;
        if (phoneIdle)
        {
            const Vector3 localPhone{0.06f,1.12f,-0.28f};
            const Vector3 phoneOffset = Vector3::Transform(
                localPhone,Matrix::CreateRotationY(yaw));
            const Matrix phoneWorld = Matrix::CreateRotationX(-0.32f)
                * Matrix::CreateRotationY(yaw)
                * Matrix::CreateTranslation(
                    avatarX+phoneOffset.X,
                    y+localPhone.Y,
                    avatarZ+phoneOffset.Z);
            drawMesh(*passengerPhone_,phoneWorld,view,projection,stats);
        }
        if (passenger.role == Sim::PassengerRole::Conductor)
        {
            const Vector3 localDevice = conductorUsingDevice
                ? Vector3{0.0f,1.08f,-0.27f} : Vector3{0.25f,1.02f,0.02f};
            const Vector3 deviceOffset = Vector3::Transform(
                localDevice,Matrix::CreateRotationY(yaw));
            const Matrix deviceWorld = Matrix::CreateRotationX(
                    conductorUsingDevice ? -0.24f : 0.0f)
                * Matrix::CreateRotationY(yaw)
                * Matrix::CreateTranslation(
                    avatarX+deviceOffset.X,
                    y+localDevice.Y,
                    avatarZ+deviceOffset.Z);
            drawMesh(*conductorHandheld_,deviceWorld,view,projection,stats);
            ++stats.conductorHandheldDevices;
        }
        ++drawn;
    }
    stats.visiblePassengers = static_cast<int>(drawn);
    stats.drawCalls += static_cast<int>(submitted);
}

void SceneRenderer::drawPlatformPeople(const Matrix& view, const Matrix& projection,
                                       const Sim::Route& route,
                                       const Sim::ServiceState& service, RenderStats& stats)
{
    const std::size_t stationIndex = route.nearestStation(service.routeDistanceMetres);
    const Sim::Station& station = route.stations()[stationIndex];
    if (std::abs(station.distanceMetres - service.routeDistanceMetres) > 260.0) return;

    const double platformSide = station.platformOnLeft ? -1.0 : 1.0;
    // Uneven paired spacing reads as small travel parties rather than a procedural picket line.
    // Smaller stations select the middle subset and therefore keep their platform population
    // compact around the stopping point.
    const bool terminal = stationIndex == 0 || stationIndex+1 == route.stations().size();
    const auto offsets = platformWaitingOffsets(terminal,station.major);
    const std::size_t offsetStart = graphicsQuality_ == 0 ? 2 : graphicsQuality_ == 1 ? 1 : 0;
    const std::size_t count = graphicsQuality_ == 0 ? 2
                            : graphicsQuality_ == 1 ? (station.major ? 4 : 3)
                                                    : (station.major ? offsets.size() : 4);
    const std::size_t platformStart = onboardAvatarBudget(graphicsQuality_);
    if (avatars_.size() < platformStart + count) return;
    for (std::size_t index = platformStart; index < platformStart + count; ++index)
        if (avatars_[index]->renderer->getStateProperty() != AvatarRendererState::Ready) return;

    const Vector3 canopyRays=daylightDirection(route.sample(station.distanceMetres).headingRadians);
    const float canopyLength=platformCanopyLength(terminal,station.major);
    const auto canopyShadow=canopyShadowBounds(canopyLength,kPlatformSurfaceElevation+1.05f,
                                               canopyRays);
    const auto luggageShadow=canopyShadowBounds(canopyLength,kPlatformSurfaceElevation+0.45f,
                                                canopyRays);
    const auto platformLight=exteriorLighting(0);
    std::array<LocalRoutePoint, 6> points{};
    for (std::size_t index = 0; index < count; ++index)
        points[index] = localRoutePoint(route, service.routeDistanceMetres,
                                        station.distanceMetres + offsets[offsetStart + index],
                                        platformSide * platformWaitingLaterals[offsetStart+index]);

    if (graphicsQuality_ >= 1)
    {
        effect_->setLightingEnabledProperty(false);
        effect_->setDiffuseColorProperty(Vector3::One);
        device_.setBlendStateProperty(BlendState::NonPremultiplied);
        device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
        for (std::size_t index = 0; index < count; ++index)
        {
            const LocalRoutePoint& point = points[index];
            if (std::abs(point.z) > 145.0f) continue;
            const Matrix shadowWorld = Matrix::CreateScale(0.32f, 1.0f, 0.23f)
                                     * Matrix::CreateRotationY(point.yaw)
                                     * Matrix::CreateTranslation(
                                         point.x, point.y+kPlatformSurfaceElevation+0.006f,
                                         point.z);
            drawMesh(*contactShadow_, shadowWorld, view, projection, stats);
        }
        device_.setBlendStateProperty(BlendState::Opaque);
        device_.setDepthStencilStateProperty(DepthStencilState::Default);
        effect_->setLightingEnabledProperty(true);
    }

    int drawn = 0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const LocalRoutePoint& point = points[index];
        if (std::abs(point.z) > 145.0f) continue;
        AvatarVisual& visual = *avatars_[platformStart + index];
        const float faceTrack = platformSide < 0.0 ? -kPi * 0.5f : kPi * 0.5f;
        float localFacing = faceTrack;
        // Select the same physical waiting position at every quality tier. Low skips the first
        // two offsets, so basing this on the authored offset index keeps the phone user near the
        // train centre instead of moving them forty metres down the platform on High.
        const bool phoneUser = offsetStart+index == 3U;
        const bool socialPose = station.major
            && platformConversationSlot(offsetStart+index,offsetStart,count);
        if (phoneUser)
        {
            // A side-on stance exposes both the device and arm pose to passengers looking out;
            // the remaining figures continue to face the train or their conversation partner.
            localFacing = platformSide < 0.0 ? 0.24f : kPi-0.24f;
        }
        else if (socialPose)
        {
            // Face the actual partner, including their lateral stagger. Conversation spacing
            // is 1.6 m rather than the unrelated waiting figures' several-metre gaps.
            const std::size_t slot = offsetStart+index;
            const std::size_t partner = slot ^ 1U;
            localFacing = avatarFacingDirection(
                platformSide*(platformWaitingLaterals[partner]-platformWaitingLaterals[slot]),
                offsets[partner]-offsets[slot]);
        }
        else if (index != 0)
            localFacing += (index%2U == 0U ? -0.16f : 0.13f);
        const float avatarYaw = point.yaw + localFacing;
        const Matrix world = Matrix::CreateScale(0.94f)
                           * Matrix::CreateRotationY(avatarYaw)
                           * Matrix::CreateTranslation(
                               point.x, point.y+kPlatformSurfaceElevation+0.01f, point.z);
        // These visuals share the onboard pool: always restore exterior illumination before
        // drawing a platform figure so a previous cabin/tunnel draw cannot leak its settings.
        visual.renderer->setAmbientLightColorProperty(platformLight.ambient);
        visual.renderer->setLightDirectionProperty(
            daylightDirection(route.sample(service.routeDistanceMetres).headingRadians));
        const float shade=canopyShadeAt(canopyShadow,
            static_cast<float>(platformSide*(platformWaitingLaterals[offsetStart+index]-5.0)),
            static_cast<float>(offsets[offsetStart+index]));
        visual.renderer->setLightColorProperty(platformLight.diffuse*(1.0f-0.88f*shade));
        visual.renderer->setWorldProperty(world);
        visual.renderer->setViewProperty(view);
        visual.renderer->setProjectionProperty(projection);
        Vector3 phonePosition = Vector3::Zero;
        if (phoneUser || socialPose)
        {
            // Platform figures share the same CNA avatar pool as onboard passengers. Layer a
            // small amount of purposeful waiting behaviour over the stock idle so the crowd no
            // longer reads as identical mannequins: one traveller checks a phone, while major-
            // station pairs exchange alternating restrained hand/head gestures.
            const auto source = visual.animation->getBoneTransformsProperty();
            auto& bones = visual.boneScratch;
            bones.assign(source.begin(),source.end());
            const auto shoulderLeft = static_cast<std::size_t>(AvatarBone::ShoulderLeft);
            const auto shoulderRight = static_cast<std::size_t>(AvatarBone::ShoulderRight);
            const auto elbowLeft = static_cast<std::size_t>(AvatarBone::ElbowLeft);
            const auto elbowRight = static_cast<std::size_t>(AvatarBone::ElbowRight);
            const auto head = static_cast<std::size_t>(AvatarBone::Head);
            if (phoneUser)
            {
                // CNA's documented custom-bone sample establishes positive Z on the left
                // shoulder as the dependable raised-arm direction. Bend the forearm back toward
                // the face, then attach the handset to that exact wrist below.
                bones[shoulderLeft] = rotatedBone(
                    bones[shoulderLeft],Matrix::CreateRotationZ(1.02f));
                bones[elbowLeft] = rotatedBone(
                    bones[elbowLeft],Matrix::CreateRotationZ(-1.10f));
                bones[head] = rotatedBone(
                    bones[head],Matrix::CreateRotationZ(-0.055f));
                const auto wrist = static_cast<std::size_t>(AvatarBone::WristLeft);
                Matrix wristLocal = bones[wrist];
                for (int parent = visual.parentBones[wrist]; parent >= 0;
                     parent = visual.parentBones[static_cast<std::size_t>(parent)])
                    wristLocal = wristLocal*bones[static_cast<std::size_t>(parent)];
                phonePosition = Vector3::Transform(Vector3::Zero,wristLocal*world);
                phonePosition.Y += 0.055f;
                ++stats.platformPhoneUsers;
            }
            else
            {
                const float gesture = std::sin(
                    static_cast<float>(service.serviceElapsedSeconds)*1.63f
                    + static_cast<float>(index)*1.21f);
                const bool useLeftArm = index%2U == 0U;
                const auto shoulder = useLeftArm ? shoulderLeft : shoulderRight;
                const auto elbow = useLeftArm ? elbowLeft : elbowRight;
                bones[shoulder] = rotatedBone(
                    bones[shoulder],Matrix::CreateRotationX(-0.12f+gesture*0.09f));
                bones[elbow] = rotatedBone(
                    bones[elbow],Matrix::CreateRotationZ(
                        (useLeftArm ? -1.0f : 1.0f)*(0.20f+std::abs(gesture)*0.16f)));
                bones[head] = rotatedBone(
                    bones[head],Matrix::CreateRotationY(
                        (useLeftArm ? -1.0f : 1.0f)*(0.07f+gesture*0.035f)));
                ++stats.platformSocialAvatars;
            }
            applyPassengerProportions(bones);
            visual.renderer->Draw(bones,visual.animation->getExpressionProperty());
        }
        else
        {
            const auto source=visual.animation->getBoneTransformsProperty();
            auto& bones=visual.boneScratch;
            bones.assign(source.begin(),source.end());
            applyPassengerProportions(bones);
            visual.renderer->Draw(bones,visual.animation->getExpressionProperty());
        }
        ++drawn;

        // Match a passenger's handheld props to their canopy light, not the full-sun world
        // effect left by track rendering. Luggage samples its own lower receiver and location.
        effect_->setAmbientLightColorProperty(platformLight.ambient);
        effect_->DirectionalLight0.setDiffuseColorProperty(platformLight.diffuse*(1.0f-0.88f*shade));
        effect_->setDiffuseColorProperty(Vector3::One);
        if (phoneUser)
        {
            // Attach the prop to the posed wrist rather than relying on a guessed chest-space
            // offset. CNA bone transforms are parent-local, matching XNA's row-vector matrix
            // order, so walking the published parent chain produces the same joint position the
            // avatar renderer consumes. This remains correct across body-height variants.
            const Matrix phoneWorld = Matrix::CreateRotationZ(-0.055f)
                * Matrix::CreateRotationY(avatarYaw)
                * Matrix::CreateTranslation(phonePosition);
            drawMesh(*passengerPhone_,phoneWorld,view,projection,stats);
        }

        // The first person at every station is a clearly readable platform employee.
        if (index == 0)
            drawBox(orientedBoxMatrix(point.x, point.y + 1.93f, point.z,
                                      0.58f, 0.18f, 0.34f, point.yaw + localFacing),
                    view, projection, Color(226,190,42,255), stats);

        if (index % 2U == 1U)
        {
            const double luggageDistance = station.distanceMetres
                + offsets[offsetStart+index] + (index%4U == 1U ? 0.48 : -0.48);
            const LocalRoutePoint luggage = localRoutePoint(
                route,service.routeDistanceMetres,luggageDistance,platformSide*5.27);
            const Matrix luggageWorld = Matrix::CreateRotationY(luggage.yaw+kPi*0.5f)
                * Matrix::CreateTranslation(
                    luggage.x,luggage.y+kPlatformSurfaceElevation+0.01f,luggage.z);
            const float luggageShade=canopyShadeAt(luggageShadow,
                static_cast<float>(platformSide*0.27),
                static_cast<float>(luggageDistance-station.distanceMetres));
            effect_->DirectionalLight0.setDiffuseColorProperty(
                platformLight.diffuse*(1.0f-0.88f*luggageShade));
            drawMesh(*platformLuggage_,luggageWorld,view,projection,stats);
            ++stats.platformLuggageItems;
        }
    }
    const auto restoredLight=exteriorLighting(tunnelLightingMix_);
    effect_->setAmbientLightColorProperty(restoredLight.ambient);
    effect_->DirectionalLight0.setDiffuseColorProperty(restoredLight.diffuse);
    stats.drawCalls += drawn;
    stats.visiblePassengers += drawn;
}

void SceneRenderer::drawStationClock(const Matrix& view, const Matrix& projection,
                                     const Sim::Route& route,
                                     const Sim::ServiceState& service, RenderStats& stats)
{
    const std::size_t stationIndex = route.nearestStation(service.routeDistanceMetres);
    const Sim::Station& station = route.stations()[stationIndex];
    if ((!station.major && !hasMarketTownTurret(stationIndex))
        || std::abs(station.distanceMetres-service.routeDistanceMetres) > 260.0) return;

    // The Northern Valley service day begins at 09:17. Service time, rather than wall time,
    // keeps station clocks coherent with accelerated QA, a frozen train, and indefinite return
    // workings. The hour hand follows the minute hand continuously instead of jumping each hour.
    constexpr double startOfServiceSeconds = 9.0*3'600.0 + 17.0*60.0;
    constexpr double secondsPerDay = 24.0*3'600.0;
    constexpr float tau = 6.2831853071795864769f;
    const double timeOfDay = std::fmod(
        startOfServiceSeconds+std::max(0.0,service.serviceElapsedSeconds),secondsPerDay);
    const float minuteAngle = static_cast<float>(
        std::fmod(timeOfDay/60.0,60.0)/60.0) * tau;
    const float hourAngle = static_cast<float>(
        std::fmod(timeOfDay/3'600.0,12.0)/12.0) * tau;

    const float side = station.platformOnLeft ? -1.0f : 1.0f;
    const auto drawHand = [&](const LocalRoutePoint& pivot,float height,
                              float angle, float length, float width) {
        // Both clock faces point toward their running line. Mirroring the rotation for the
        // opposite platform side preserves clockwise motion as viewed from the train.
        const Matrix handWorld = Matrix::CreateScale(0.065f,length,width)
            * Matrix::CreateTranslation(0.0f,length*0.5f,0.0f)
            * Matrix::CreateRotationX(angle*side)
            * Matrix::CreateRotationY(pivot.yaw)
            * Matrix::CreateTranslation(pivot.x,pivot.y+height,pivot.z);
        drawBox(handWorld,view,projection,Color(38,42,41,255),stats);
        ++stats.stationClockHands;
    };
    if (station.major)
    {
        const auto pivot=localRoutePoint(route,service.routeDistanceMetres,
            station.distanceMetres,static_cast<double>(side)*4.92);
        drawHand(pivot,5.75f,hourAngle,0.34f,0.075f);
        drawHand(pivot,5.75f,minuteAngle,0.49f,0.052f);
    }
    if (hasMarketTownTurret(stationIndex))
    {
        const auto pivot=localRoutePoint(route,service.routeDistanceMetres,
            station.distanceMetres+24,static_cast<double>(side)*(12-marketTowerClockFaceDepth-0.05f));
        drawHand(pivot,marketTowerClockHeight,hourAngle,0.52f,0.08f);
        drawHand(pivot,marketTowerClockHeight,minuteAngle,0.73f,0.055f);
    }
}

RenderStats SceneRenderer::draw(const Matrix& view, const Matrix& projection, const Sim::Route& route,
                                const Sim::ServiceState& service, const World::StreamingWorld& world,
                                const Sim::PassengerSystem& passengers, double seconds,
                                double etaSeconds, double scheduleDeviationSeconds,
                                bool toiletDoorOpen, bool toiletOccupied,
                                float playerX, float playerZ, float cameraYaw)
{
    RenderStats stats;
    const Vector3 daylight = daylightDirection(route.sample(service.routeDistanceMetres).headingRadians);
    effect_->DirectionalLight0.setDirectionProperty(daylight);
    materialEffect_->DirectionalLight0.setDirectionProperty(daylight);
    // Interior ceiling fill belongs only to the passenger-space passes below. Restore the
    // exterior single-sun setup every frame, including after a tunnel or quality change.
    effect_->DirectionalLight1.setEnabledProperty(false);
    effect_->DirectionalLight2.setEnabledProperty(false);
    materialEffect_->DirectionalLight1.setEnabledProperty(false);
    materialEffect_->DirectionalLight2.setEnabledProperty(false);
    effect_->setVertexColorEnabledProperty(true);
    effect_->setLightingEnabledProperty(true);
    const auto setMaterialSpecular = [this](const Vector3& surface, float power,
                                             const Vector3& light) {
        if (graphicsQuality_ == 0)
        {
            materialEffect_->setSpecularColorProperty(Vector3::Zero);
            materialEffect_->setSpecularPowerProperty(1.0f);
            materialEffect_->DirectionalLight0.setSpecularColorProperty(Vector3::Zero);
            return;
        }
        materialEffect_->setSpecularColorProperty(surface);
        materialEffect_->setSpecularPowerProperty(power);
        materialEffect_->DirectionalLight0.setSpecularColorProperty(light);
    };

    const double requestedTrackAnchor = std::clamp(
        std::round(service.routeDistanceMetres / 500.0) * 500.0, 0.0, route.lengthMetres());
    if (!trackDetail_ || requestedTrackAnchor != trackAnchorDistance_)
        rebuildTrack(route, requestedTrackAnchor);
    stats.tracksideKilometrePosts = tracksideKilometrePosts_;
    stats.trackFastenerAssemblies = trackFastenerAssemblies_;
    stats.turnoutPointMotors = trackTurnoutPointMotors_;
    stats.turnoutBladeAssemblies = trackTurnoutBladeAssemblies_;
    const double renderDelta = previousRenderSeconds_ < 0.0
        ? 0.0 : std::clamp(seconds - previousRenderSeconds_, 0.0, 0.1);
    previousRenderSeconds_ = seconds;
    const float tunnelTarget = route.sample(service.routeDistanceMetres).tunnel ? 1.0f : 0.0f;
    const float tunnelLightingStep = static_cast<float>(renderDelta) * 1.35f;
    tunnelLightingMix_ += std::clamp(tunnelTarget - tunnelLightingMix_,
                                     -tunnelLightingStep, tunnelLightingStep);
    const float doorTarget = service.doorsOpen ? 1.0f : 0.0f;
    const float doorStep = static_cast<float>(renderDelta)
                         / static_cast<float>(Sim::TrainInterior::PassengerDoorAnimationSeconds);
    doorOpenAmount_ += std::clamp(doorTarget - doorOpenAmount_, -doorStep, doorStep);
    // The four-second service interlock comfortably outlasts the 1.35-second animation at the
    // production time scale. Developer acceleration can traverse those simulated seconds in only
    // a few rendered frames, however. Never carry a partially open leaf or deployed bridge plate
    // into a moving phase; an accelerated QA run may snap the final fraction closed instead.
    if (service.phase == Sim::ServicePhase::Accelerating
        || service.phase == Sim::ServicePhase::Cruising
        || service.phase == Sim::ServicePhase::Coasting
        || service.phase == Sim::ServicePhase::Braking)
        doorOpenAmount_ = 0.0f;
    const bool glassPlatformNegativeX=Sim::platformOnNegativeTrainX(service);
    if (!passengerDoorGlass_ || passengerDoorGlassOpenness_!=doorOpenAmount_
        || passengerDoorGlassNegativeX_!=glassPlatformNegativeX)
    {
        if (!passengerDoorGlassScratch_)
        {
            passengerDoorGlassScratch_=std::make_unique<MeshBuilder>();
            passengerDoorGlassScratch_->reserve(2880,2880);
        }
        auto& panes=*passengerDoorGlassScratch_;
        panes.clear();
        for (int car=0;car<Sim::TrainInterior::PassengerCars;++car)
            for (float side : {-1.0f,1.0f})
                for (float panel : {-1.0f,1.0f})
                {
                    const bool platformSide=(side<0)==glassPlatformNegativeX;
                    const float openness=platformSide ? doorOpenAmount_ : 0.0f;
                    addPassengerDoorPane(panes,side*1.455f,1.17f,
                        static_cast<float>(Sim::TrainInterior::passengerDoorZ(car))
                            +panel*(0.33f+openness*0.63f));
                }
        if (passengerDoorGlass_) passengerDoorGlass_->replace(device_,panes.data());
        else passengerDoorGlass_=std::make_unique<GpuMesh>(device_,panes.data(),true);
        passengerDoorGlassOpenness_=doorOpenAmount_;
        passengerDoorGlassNegativeX_=glassPlatformNegativeX;
    }
    const float toiletDoorTarget = toiletDoorOpen ? 1.0f : 0.0f;
    const float toiletDoorStep = static_cast<float>(renderDelta) / 0.75f;
    toiletDoorOpenAmount_ += std::clamp(toiletDoorTarget - toiletDoorOpenAmount_,
                                        -toiletDoorStep, toiletDoorStep);
    const float gangwayDoorStep = static_cast<float>(renderDelta) / 0.55f;
    for (int boundary = 0; boundary < Sim::TrainInterior::GangwayCount; ++boundary)
    {
        bool sensorOccupied = Sim::TrainInterior::gangwaySensorContains(boundary, playerZ);
        if (!sensorOccupied)
            sensorOccupied = std::any_of(
                passengers.passengers().begin(),passengers.passengers().end(),
                [boundary](const Sim::Passenger& passenger) {
                    return passenger.role != Sim::PassengerRole::Driver
                        && !passenger.seat.has_value()
                        && Sim::TrainInterior::gangwaySensorContains(boundary, passenger.z);
                });
        const float target = sensorOccupied ? 1.0f : 0.0f;
        float& openness = gangwayDoorOpenAmounts_[static_cast<std::size_t>(boundary)];
        openness += std::clamp(target - openness,-gangwayDoorStep,gangwayDoorStep);
        if (openness > 0.85f) ++stats.openGangwayDoors;
    }

    // Ignore camera translation for distant atmosphere: walking through the consist must not
    // move clouds or push the sky beyond the far clip plane. World geometry still occludes it.
    Matrix atmosphereView=view;
    atmosphereView.setTranslationProperty(Vector3::Zero);
    effect_->setFogEnabledProperty(false);
    effect_->setLightingEnabledProperty(false);
    effect_->setDiffuseColorProperty(Vector3::One);
    device_.setRasterizerStateProperty(RasterizerState::CullNone);
    drawMesh(*sky_, Matrix::getIdentityProperty(), atmosphereView, projection, stats);
    const Vector3 sunPosition = daylight*-1000.0f;
    drawMesh(*sun_, Matrix::CreateScale(8.0f)
                    * Matrix::CreateRotationX(-std::asin(-daylight.Y))
                    * Matrix::CreateRotationY(std::atan2(-daylight.X,-daylight.Z))
                    * Matrix::CreateTranslation(sunPosition),
             atmosphereView, projection, stats);
    effect_->setTextureProperty(cloudDetailTexture_.get());
    device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    device_.setBlendStateProperty(BlendState::NonPremultiplied);
    device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
    // The camera is at the sky centre, but floating-origin and future renderer
    // conventions must not make a single-sided atmospheric card disappear.
    device_.setRasterizerStateProperty(RasterizerState::CullNone);
    drawMesh(*clouds_,Matrix::getIdentityProperty(),atmosphereView,projection,stats);
    stats.skyCloudBillboards = skyCloudBillboardCount_;
    device_.setBlendStateProperty(BlendState::Opaque);
    device_.setDepthStencilStateProperty(DepthStencilState::Default);
    device_.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);
    effect_->setTextureProperty(whiteMaterialTexture_.get());
    effect_->setLightingEnabledProperty(true);
    effect_->setFogEnabledProperty(true);

    // Outdoor illumination fades at tunnel portals while the train's own interior lighting is
    // restored below. Tunnel fixtures remain visible through the deliberately non-black ambient.
    const auto outdoorLight=exteriorLighting(tunnelLightingMix_);
    effect_->setAmbientLightColorProperty(outdoorLight.ambient);
    effect_->DirectionalLight0.setDiffuseColorProperty(outdoorLight.diffuse);
    const Vector3 atmosphericFog=rgb(daylightSkyColor(0))*(1.0f-tunnelLightingMix_)
        +Vector3{0.075f,0.082f,0.090f}*tunnelLightingMix_;
    effect_->setFogColorProperty(atmosphericFog);
    materialEffect_->setAmbientLightColorProperty(outdoorLight.ambient);
    materialEffect_->DirectionalLight0.setDiffuseColorProperty(outdoorLight.diffuse);
    materialEffect_->setFogColorProperty(atmosphericFog);

    // Terrain, vegetation, architecture, stations and fixed railway furniture are rebuilt once
    // per streamed chunk transition and submitted as one vertex-coloured batch.
    const double requestedWorldAnchor = std::clamp(
        (static_cast<double>(world.currentChunk()) + 0.5) * world.chunkLength(),
        0.0, route.lengthMetres());
    if (!staticWorld_ || world.currentChunk() != worldAnchorChunk_)
    {
        const auto begin = std::chrono::steady_clock::now();
        rebuildStaticWorld(route, world, requestedWorldAnchor);
        const double milliseconds = std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-begin).count();
        ++worldRebuilds_;
        worldRebuildTotalMs_ += milliseconds;
        worldRebuildMaxMs_ = std::max(worldRebuildMaxMs_,milliseconds);
    }
    stats.worldRebuilds = worldRebuilds_;
    stats.worldBufferReuses = worldBufferReuses_;
    stats.routePointCacheHits = routePointCacheHits_;
    stats.routePointCacheMisses = routePointCacheMisses_;
    stats.worldRebuildTotalMs = worldRebuildTotalMs_;
    stats.worldRebuildMaxMs = worldRebuildMaxMs_;
    stats.worldGenerationTotalMs = worldGenerationTotalMs_;
    stats.worldGenerationMaxMs = worldGenerationMaxMs_;
    stats.worldUploadTotalMs = worldUploadTotalMs_;
    stats.worldUploadMaxMs = worldUploadMaxMs_;
    effect_->setDiffuseColorProperty(Vector3::One);
    const LocalRoutePoint worldAnchor = localRoutePoint(
        route, service.routeDistanceMetres, worldAnchorDistance_);
    const Matrix worldPlacement = Matrix::CreateRotationY(worldAnchor.yaw)
                                * Matrix::CreateTranslation(worldAnchor.x, worldAnchor.y, worldAnchor.z);
    const auto drawPrintedText=[&](const std::unique_ptr<GpuMesh>& lettering,const Matrix& placement) {
        if (!lettering) return;
        passengerDisplayEffect_->setWorldProperty(placement);
        passengerDisplayEffect_->setViewProperty(view);
        passengerDisplayEffect_->setProjectionProperty(projection);
        device_.getSamplerStatesProperty()[0]=SamplerState::LinearClamp;
        device_.setBlendStateProperty(BlendState::NonPremultiplied);
        device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
        for (auto& pass : passengerDisplayEffect_->getCurrentTechniqueProperty()->getPassesProperty())
        {
            pass.Apply();
            lettering->draw(device_);
            ++stats.drawCalls;
            stats.triangles+=lettering->triangles();
        }
        device_.setDepthStencilStateProperty(DepthStencilState::Default);
        device_.setBlendStateProperty(BlendState::Opaque);
    };
    if (terrain_)
    {
        setMaterialSpecular(Vector3::Zero,1.0f,Vector3::Zero);
        device_.getSamplerStatesProperty()[0] = graphicsQuality_ >= 2
            ? SamplerState::AnisotropicWrap : SamplerState::LinearWrap;
        materialEffect_->setTextureProperty(terrainDetailTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        drawMaterialMesh(*terrain_,worldPlacement,view,projection,stats);
        stats.terrainMaterialTriangles = terrain_->triangles();
        // Terrain alone needs wrapping and anisotropy. Constant-UV neutral materials use the
        // stock clamp sampler; leaving the anisotropic sampler installed makes zero-derivative
        // lookups render black or striped on Mesa llvmpipe and is needless on hardware too.
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (foliageMaterial_)
    {
        setMaterialSpecular(Vector3::Zero,1.0f,Vector3::Zero);
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearWrap;
        materialEffect_->setTextureProperty(foliageDetailTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        drawMaterialMesh(*foliageMaterial_,worldPlacement,view,projection,stats);
        stats.foliageMaterialTriangles = foliageMaterial_->triangles();
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (needleMaterial_)
    {
        setMaterialSpecular(Vector3::Zero,1.0f,Vector3::Zero);
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearWrap;
        materialEffect_->setTextureProperty(needleDetailTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        drawMaterialMesh(*needleMaterial_,worldPlacement,view,projection,stats);
        stats.foliageMaterialTriangles += needleMaterial_->triangles();
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (leafSprays_ || needleSprays_)
    {
        leafSprayEffect_->setWorldProperty(worldPlacement);
        leafSprayEffect_->setViewProperty(view);
        leafSprayEffect_->setProjectionProperty(projection);
        leafSprayEffect_->setFogEnabledProperty(materialEffect_->getFogEnabledProperty());
        leafSprayEffect_->setFogStartProperty(materialEffect_->getFogStartProperty());
        leafSprayEffect_->setFogEndProperty(materialEffect_->getFogEndProperty());
        leafSprayEffect_->setFogColorProperty(materialEffect_->getFogColorProperty());
        for (const auto& [sprayMesh,sprayTexture] : std::array{
                 std::pair{leafSprays_.get(),leafSprayTexture_.get()},
                 std::pair{needleSprays_.get(),needleSprayTexture_.get()}})
        {
            if (!sprayMesh) continue;
            leafSprayEffect_->setTextureProperty(sprayTexture);
            for (auto& pass:leafSprayEffect_->getCurrentTechniqueProperty()->getPassesProperty())
            {
                pass.Apply();
                sprayMesh->draw(device_);
                ++stats.drawCalls;
                stats.triangles+=sprayMesh->triangles();
                stats.foliageMaterialTriangles+=sprayMesh->triangles();
            }
        }
    }
    if (roofMaterial_)
    {
        setMaterialSpecular(Vector3::Zero,1.0f,Vector3::Zero);
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearWrap;
        materialEffect_->setTextureProperty(roofDetailTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        drawMaterialMesh(*roofMaterial_,worldPlacement,view,projection,stats);
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (waterMaterial_)
    {
        // Medium/High use filtered animated normals and analytic sky Fresnel in one pass.
        // Low, or an unavailable custom effect, retains the stock textured material.
        setMaterialSpecular(Vector3{0.34f,0.42f,0.48f},72.0f,
                            Vector3{0.62f,0.62f,0.54f});
        device_.getSamplerStatesProperty()[0] = graphicsQuality_ >= 2
            ? SamplerState::AnisotropicWrap : SamplerState::LinearWrap;
        materialEffect_->setTextureProperty(waterDetailTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        if (graphicsQuality_>=1 && !waterEffect_)
            waterEffect_=std::make_unique<OpenGL33WaterEffect>(device_);
        if (graphicsQuality_>=1 && waterEffect_->valid())
        {
            waterEffect_->draw(*waterMaterial_,device_,*materialEffect_,worldPlacement,
                               view,projection,service.serviceElapsedSeconds);
            ++stats.drawCalls;
            ++stats.waterShaderDraws;
            stats.triangles+=waterMaterial_->triangles();
        }
        else
            drawMaterialMesh(*waterMaterial_,worldPlacement,view,projection,stats);
        stats.waterMaterialTriangles = waterMaterial_->triangles();
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (platformMaterial_)
    {
        setMaterialSpecular(Vector3{0.055f,0.055f,0.05f},18.0f,
                            Vector3{0.30f,0.29f,0.25f});
        device_.getSamplerStatesProperty()[0] = graphicsQuality_ >= 2
            ? SamplerState::AnisotropicWrap : SamplerState::LinearWrap;
        materialEffect_->setTextureProperty(concreteDetailTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        drawMaterialMesh(*platformMaterial_,worldPlacement,view,projection,stats);
        stats.platformMaterialTriangles = platformMaterial_->triangles();
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (facadeMaterial_)
    {
        setMaterialSpecular(Vector3{0.035f,0.033f,0.03f},14.0f,
                            Vector3{0.25f,0.23f,0.20f});
        materialEffect_->setTextureProperty(masonryDetailTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        device_.getSamplerStatesProperty()[0] = graphicsQuality_ >= 2
            ? SamplerState::AnisotropicWrap : SamplerState::LinearWrap;
        if (facadeLightingMode_>0)
        {
            if (!facadeEffect_) facadeEffect_=std::make_unique<OpenGL33FacadeEffect>(device_);
            facadeEffect_->draw(*facadeMaterial_,device_,*materialEffect_,worldPlacement,view,projection,facadeLightingMode_==2);
            ++stats.drawCalls; ++stats.facadeShaderDraws; stats.triangles+=facadeMaterial_->triangles();
        }
        else drawMaterialMesh(*facadeMaterial_,worldPlacement,view,projection,stats);
        stats.facadeMaterialTriangles = facadeMaterial_->triangles();
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (renderedFacadeMaterial_)
    {
        setMaterialSpecular(Vector3{0.022f,0.022f,0.020f},11.0f,
                            Vector3{0.22f,0.21f,0.19f});
        materialEffect_->setTextureProperty(renderedFacadeDetailTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        device_.getSamplerStatesProperty()[0] = graphicsQuality_ >= 2
            ? SamplerState::AnisotropicWrap : SamplerState::LinearWrap;
        if (facadeLightingMode_>0)
        {
            if (!facadeEffect_) facadeEffect_=std::make_unique<OpenGL33FacadeEffect>(device_);
            facadeEffect_->draw(*renderedFacadeMaterial_,device_,*materialEffect_,worldPlacement,view,projection,facadeLightingMode_==2);
            ++stats.drawCalls; ++stats.facadeShaderDraws; stats.triangles+=renderedFacadeMaterial_->triangles();
        }
        else drawMaterialMesh(*renderedFacadeMaterial_,worldPlacement,view,projection,stats);
        stats.renderedFacadeMaterialTriangles = renderedFacadeMaterial_->triangles();
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (staticWorldShadows_)
    {
        effect_->setLightingEnabledProperty(false);
        effect_->setDiffuseColorProperty(Vector3::One);
        device_.setBlendStateProperty(BlendState::NonPremultiplied);
        device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
        drawMesh(*staticWorldShadows_,worldPlacement,view,projection,stats);
        stats.staticContactShadowTriangles = staticWorldShadows_->triangles();
        device_.setBlendStateProperty(BlendState::Opaque);
        device_.setDepthStencilStateProperty(DepthStencilState::Default);
        effect_->setLightingEnabledProperty(true);
    }
    drawMesh(*staticWorld_, worldPlacement, view, projection, stats);
    drawPrintedText(stationLettering_,worldPlacement);
    stats.lakesideWaterPatches = staticWorldLakePatches_;
    stats.lakesideBankTrees = staticWorldLakeBankTrees_;
    stats.hayBaleStacks = staticWorldHayBaleStacks_;
    stats.riverWaterSections = staticWorldRiverWaterSections_;
    stats.riverBankClumps = staticWorldRiverBankClumps_;
    stats.stationWaterfrontSections = staticWorldStationWaterfrontSections_;
    stats.catenaryDroppers = staticWorldCatenaryDroppers_;
    stats.catenaryFoundations = staticWorldCatenaryFoundations_;
    stats.catenaryMessengerSegments = staticWorldCatenaryMessengerSegments_;
    stats.catenaryRegistrationAssemblies = staticWorldCatenaryRegistrationAssemblies_;
    stats.linesideEquipmentCabinets = staticWorldLinesideEquipmentCabinets_;
    stats.tunnelLuminaires = staticWorldTunnelLuminaires_;
    stats.tunnelSafetyNiches = staticWorldTunnelSafetyNiches_;
    stats.tunnelLiningRings = staticWorldTunnelLiningRings_;
    stats.tunnelMarkerPlates = staticWorldTunnelMarkerPlates_;
    stats.bridgeSections = staticWorldBridgeSections_;
    stats.riverBridgeSections = staticWorldRiverBridgeSections_;
    stats.viaductSections = staticWorldViaductSections_;
    stats.bridgeAbutments = staticWorldBridgeAbutments_;
    stats.bridgeParapetPosts = staticWorldBridgeParapetPosts_;
    stats.fencePosts = staticWorldFencePosts_;
    stats.platformSurfaceJoints = staticWorldPlatformSurfaceJoints_;
    stats.platformCanopyLights = staticWorldPlatformCanopyLights_;
    stats.platformCanopyDrainageAssemblies =
        staticWorldPlatformCanopyDrainageAssemblies_;
    stats.platformPlanters = staticWorldPlatformPlanters_;
    stats.platformPosterCases = staticWorldPlatformPosterCases_;
    stats.platformEquipmentCabinets = staticWorldPlatformEquipmentCabinets_;
    stats.platformBenchAssemblies = staticWorldPlatformBenchAssemblies_;
    stats.platformWeatheringMarks = staticWorldPlatformWeatheringMarks_;
    stats.stationFacadeWeatheringStreaks =
        staticWorldStationFacadeWeatheringStreaks_;
    stats.platformBoundarySections = staticWorldPlatformBoundarySections_;
    stats.tracksideDrainageSections = staticWorldTracksideDrainageSections_;
    stats.tracksideVergeTufts = staticWorldTracksideVergeTufts_;
    stats.fieldBoundarySegments = staticWorldFieldBoundarySegments_;
    stats.distantWoodlandTrees = staticWorldDistantWoodlandTrees_;
    stats.distantRidgeMasses = staticWorldDistantRidgeMasses_;
    stats.treeBranchCues = staticWorldTreeBranchCues_;
    stats.coniferBranchTiers = staticWorldConiferBranchTiers_;
    stats.understoreyGroups = staticWorldUnderstoreyGroups_;
    stats.distantSettlementBuildings = staticWorldDistantSettlementBuildings_;
    stats.buildingEndWallWindows = staticWorldBuildingEndWallWindows_;
    stats.towerEndWallWindows = staticWorldTowerEndWallWindows_;
    stats.detailedWarehouses = staticWorldDetailedWarehouses_;
    stats.industrialCompounds = staticWorldIndustrialCompounds_;
    stats.industrialYardVehicles = staticWorldIndustrialYardVehicles_;
    stats.industrialTrailers = staticWorldIndustrialTrailers_;
    stats.industrialUtilityGroups = staticWorldIndustrialUtilityGroups_;
    stats.industrialEdgePlantingGroups = staticWorldIndustrialEdgePlantingGroups_;
    stats.urbanRoadEdgeMarkingSections = staticWorldUrbanRoadEdgeMarkingSections_;
    stats.urbanRoadDrainGrates = staticWorldUrbanRoadDrainGrates_;
    stats.urbanResidentialPlotSurfaces = staticWorldUrbanResidentialPlotSurfaces_;
    stats.urbanStreetPockets = staticWorldUrbanStreetPockets_;
    stats.urbanResidentialEntrancePaths = staticWorldUrbanResidentialEntrancePaths_;
    if (worldLuminaires_)
    {
        effect_->setLightingEnabledProperty(false);
        drawMesh(*worldLuminaires_, worldPlacement, view, projection, stats);
        effect_->setLightingEnabledProperty(true);
    }

    // One live departure board belongs to the platform currently being approached or occupied.
    // It is a small independent batch because its destination and punctuality must survive
    // direction reversals without rebuilding the seven-chunk scenery mesh.
    const std::size_t platformInformationStation =
        (service.phase == Sim::ServicePhase::Dwelling
         || service.phase == Sim::ServicePhase::ClosingDoors)
        ? service.currentStation : service.nextStation;
    const Sim::Station& informationStation = route.stations()[platformInformationStation];
    if (std::abs(informationStation.distanceMetres - service.routeDistanceMetres) < 900.0)
    {
        const int platformEta = std::clamp(
            static_cast<int>(std::ceil(std::max(0.0, etaSeconds) / 60.0)), 0, 99);
        const PlatformInformationState platformState{
            platformInformationStation, service.direction, static_cast<int>(service.phase),
            platformEta,
            static_cast<int>(std::round(scheduleDeviationSeconds / 60.0))};
        if (!platformInformation_ || platformInformationState_ != platformState)
            rebuildPlatformInformation(route,service,platformInformationStation,platformEta,
                                       scheduleDeviationSeconds);
        const LocalRoutePoint placement = localRoutePoint(
            route,service.routeDistanceMetres,informationStation.distanceMetres);
        const Matrix platformWorld = Matrix::CreateRotationY(placement.yaw)
                                   * Matrix::CreateTranslation(
                                       placement.x,placement.y,placement.z);
        effect_->setLightingEnabledProperty(false);
        drawMesh(*platformInformation_,platformWorld,view,projection,stats);
        drawPrintedText(platformLettering_,platformWorld);
        ++stats.platformInformationBoards;
        effect_->setLightingEnabledProperty(true);
    }

    // Only genuinely moving scenery remains outside the static batch.
    const float dynamicRange = graphicsQuality_ == 0 ? 450.0f
                             : graphicsQuality_ == 1 ? 650.0f : 850.0f;
    for (const World::Chunk& chunk : world.chunks())
    {
        for (const World::WorldFeature& feature : chunk.features)
        {
            if (feature.kind != World::FeatureKind::RoadVehicle
                && feature.kind != World::FeatureKind::OtherTrain
                && feature.kind != World::FeatureKind::Signal
                && feature.kind != World::FeatureKind::LevelCrossing) continue;
            const Sim::RoutePose featurePose = route.sample(feature.routeDistance);
            if ((featurePose.tunnel || featurePose.bridge)
                && feature.kind == World::FeatureKind::RoadVehicle) continue;
            const LocalRoutePoint point = localRoutePoint(
                route, service.routeDistanceMetres, feature.routeDistance, feature.lateralMetres);
            // Moving trains are culled from their live consist centre below. Their deterministic
            // feature anchor may be nearly a kilometre from a currently visible pass.
            const float featureRange = feature.kind == World::FeatureKind::LevelCrossing
                ? 900.0f : dynamicRange;
            if (feature.kind != World::FeatureKind::OtherTrain
                && std::abs(point.z) > featureRange) continue;
            const float scale = static_cast<float>(feature.scale);
            if (feature.kind == World::FeatureKind::Signal)
            {
                const int facingDirection = World::signalFacingDirection(feature);
                const World::SignalAspect signalAspect = World::signalAspect(feature, route, service);
                Color aspect;
                float lensHeight = 4.10f;
                switch (signalAspect)
                {
                    case World::SignalAspect::Red:
                        aspect = Color(215,49,39,255);
                        lensHeight = 4.42f;
                        ++stats.redSignals;
                        break;
                    case World::SignalAspect::Yellow:
                        aspect = Color(235,182,36,255);
                        lensHeight = 4.10f;
                        ++stats.yellowSignals;
                        break;
                    case World::SignalAspect::Green:
                        aspect = Color(45,188,75,255);
                        lensHeight = 3.78f;
                        ++stats.greenSignals;
                        break;
                }

                const double lensLateral = feature.lateralMetres
                    - std::copysign(0.20, feature.lateralMetres);
                const LocalRoutePoint lens = localRoutePoint(
                    route, service.routeDistanceMetres,
                    feature.routeDistance - 0.27 * static_cast<double>(facingDirection),
                    lensLateral);
                drawBox(orientedBoxMatrix(lens.x,lens.y+lensHeight,lens.z,
                                          0.18f,0.18f,0.09f,
                                          lens.yaw + (facingDirection < 0 ? kPi : 0.0f)),
                        view, projection, aspect, stats);
            }
            else if (feature.kind == World::FeatureKind::LevelCrossing)
            {
                const double requiredClosure = world.requiredLevelCrossingClosure(
                    feature.routeDistance,service.routeDistanceMetres,seconds);
                const float closure = static_cast<float>(requiredClosure);
                const float barrierAngle = (1.0f-closure)*1.309f;
                effect_->setDiffuseColorProperty(Vector3::One);
                for (double side : {-1.0,1.0})
                {
                    const LocalRoutePoint pivot = localRoutePoint(
                        route,service.routeDistanceMetres,feature.routeDistance-4.2,
                        side*8.3);
                    const Matrix barrierWorld = Matrix::CreateRotationX(-barrierAngle)
                        * Matrix::CreateRotationY(pivot.yaw)
                        * Matrix::CreateTranslation(pivot.x,pivot.y+1.55f,pivot.z);
                    drawMesh(*levelCrossingBarrier_,barrierWorld,view,projection,stats);
                    ++stats.levelCrossingBarriers;
                }

                if (closure > 0.0f)
                {
                    const double lensOffset = (static_cast<int>(seconds*2.2) & 1) == 0
                        ? -0.18 : 0.18;
                    for (double side : {-1.0,1.0})
                    {
                        const LocalRoutePoint lens = localRoutePoint(
                            route,service.routeDistanceMetres,
                            feature.routeDistance-4.2+lensOffset,side*8.44);
                        drawBox(orientedBoxMatrix(lens.x,lens.y+2.15f,lens.z,
                                                  0.055f,0.16f,0.16f,lens.yaw),
                                view,projection,Color(239,49,39,255),stats);
                        ++stats.levelCrossingWarningLights;
                    }
                }
            }
            else if (feature.kind == World::FeatureKind::RoadVehicle)
            {
                const float direction = feature.variant % 2 == 0 ? 1.0f : -1.0f;
                const float motion = std::fmod(static_cast<float>(seconds)
                                               * (7.0f + static_cast<float>(feature.variant))
                                               + static_cast<float>(feature.variant) * 13.0f,
                                               90.0f) - 45.0f;
                const double vehicleDistance = std::clamp(
                    feature.routeDistance + static_cast<double>(motion * direction),
                    0.0, route.lengthMetres());
                const LocalRoutePoint vehiclePoint = localRoutePoint(
                    route, service.routeDistanceMetres, vehicleDistance, feature.lateralMetres);
                const Matrix vehicleWorld = Matrix::CreateScale(scale)
                    * Matrix::CreateRotationY(vehiclePoint.yaw + (direction < 0.0f ? kPi : 0.0f))
                    * Matrix::CreateTranslation(vehiclePoint.x,vehiclePoint.y-0.20f,
                                                vehiclePoint.z);
                effect_->setDiffuseColorProperty(Vector3::One);
                drawMesh(*roadVehicles_[feature.variant % roadVehicles_.size()], vehicleWorld,
                         view, projection, stats);
                ++stats.roadVehicles;
            }
            else
            {
                if (!World::movingOtherTrainVisible(feature,seconds,route.lengthMetres()))
                    continue;
                const double otherDistance = World::movingOtherTrainDistance(
                    feature,seconds,route.lengthMetres());
                const LocalRoutePoint otherCentre = localRoutePoint(
                    route,service.routeDistanceMetres,otherDistance,
                    route.parallelTrackLateralMetres(otherDistance));
                // Include half the short consist so a terminal car enters and leaves the fog
                // naturally, while the hidden lifecycle reset happens beyond the High range.
                if (std::abs(otherCentre.z) > dynamicRange + 28.0f) continue;
                effect_->setDiffuseColorProperty(Vector3::One);
                for (int car = -1; car <= 1; ++car)
                {
                    const double carDistance = otherDistance + static_cast<double>(car) * 19.0;
                    if (carDistance < 0.0 || carDistance > route.lengthMetres()) continue;
                    const LocalRoutePoint otherPoint = localRoutePoint(
                        route, service.routeDistanceMetres, carDistance,
                        route.parallelTrackLateralMetres(carDistance));
                    const Matrix otherWorld = Matrix::CreateRotationY(otherPoint.yaw + kPi)
                                            * Matrix::CreateTranslation(otherPoint.x, otherPoint.y,
                                                                        otherPoint.z);
                    drawMesh(*otherTrain_, otherWorld, view, projection, stats);
                    if (car == 0)
                        drawMesh(*otherTrainPantograph_,otherWorld,view,projection,stats);
                    else if (car == -1)
                    {
                        drawMesh(*otherTrainFrontCab_,otherWorld,view,projection,stats);
                        ++stats.otherTrainCabEnds;
                    }
                    else
                    {
                        const Matrix reversedEnd = Matrix::CreateRotationY(kPi) * otherWorld;
                        drawMesh(*otherTrainRearCab_,reversedEnd,view,projection,stats);
                        ++stats.otherTrainCabEnds;
                    }
                    ++stats.otherTrainCars;
                }
            }
        }
    }

    // Major stations may hold one short parked service on their authored siding. It reuses the
    // articulated other-train car mesh and route sampling, adding railway life without dispatch AI.
    const std::size_t nearbyStationIndex = route.nearestStation(service.routeDistanceMetres);
    const Sim::Station& nearbyStation = route.stations()[nearbyStationIndex];
    if (graphicsQuality_ >= 1 && nearbyStation.major
        && std::abs(nearbyStation.distanceMetres - service.routeDistanceMetres) < 720.0)
    {
        double parkedCentre = nearbyStation.distanceMetres;
        if (nearbyStationIndex == 0) parkedCentre += 48.0;
        else if (nearbyStationIndex + 1 == route.stations().size()) parkedCentre -= 48.0;
        effect_->setDiffuseColorProperty(Vector3::One);
        for (int car = -1; car <= 1; ++car)
        {
            const double carDistance = parkedCentre + static_cast<double>(car) * 19.0;
            if (carDistance < 0.0 || carDistance > route.lengthMetres()) continue;
            const double lateral = stationSidingLateral(route, carDistance)
                .value_or(route.parallelTrackLateralMetres(carDistance));
            const LocalRoutePoint parked = localRoutePoint(
                route, service.routeDistanceMetres, carDistance, lateral);
            if (std::abs(parked.z) > 850.0f) continue;
            const Matrix parkedWorld = Matrix::CreateRotationY(parked.yaw + kPi)
                                     * Matrix::CreateTranslation(parked.x, parked.y, parked.z);
            drawMesh(*otherTrain_, parkedWorld, view, projection, stats);
            if (car == 0)
                drawMesh(*otherTrainPantograph_,parkedWorld,view,projection,stats);
            else if (car == -1)
            {
                drawMesh(*otherTrainFrontCab_,parkedWorld,view,projection,stats);
                ++stats.otherTrainCabEnds;
            }
            else
            {
                const Matrix reversedEnd = Matrix::CreateRotationY(kPi) * parkedWorld;
                drawMesh(*otherTrainRearCab_,reversedEnd,view,projection,stats);
                ++stats.otherTrainCabEnds;
            }
            ++stats.otherTrainCars;
        }
    }

    // Ballast, rails and the dense sleeper field are one batch. Only the small sleeper repeat
    // offset changes, which avoids hundreds of tiny draw calls per frame.
    effect_->setDiffuseColorProperty(Vector3::One);
    const LocalRoutePoint trackAnchor = localRoutePoint(
        route, service.routeDistanceMetres, trackAnchorDistance_);
    const Matrix trackWorld = Matrix::CreateRotationY(trackAnchor.yaw)
                            * Matrix::CreateTranslation(trackAnchor.x, trackAnchor.y, trackAnchor.z);
    if (trackBedMaterial_)
    {
        setMaterialSpecular(Vector3::Zero,1.0f,Vector3::Zero);
        device_.getSamplerStatesProperty()[0] = graphicsQuality_ >= 2
            ? SamplerState::AnisotropicWrap : SamplerState::LinearWrap;
        materialEffect_->setTextureProperty(ballastDetailTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        drawMaterialMesh(*trackBedMaterial_,trackWorld,view,projection,stats);
        stats.ballastMaterialTriangles = trackBedMaterial_->triangles();
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (railHeadMaterial_)
    {
        setMaterialSpecular(Vector3{0.34f,0.36f,0.35f},64.0f,
                            Vector3{0.56f,0.54f,0.48f});
        materialEffect_->setTextureProperty(whiteMaterialTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
        drawMaterialMesh(*railHeadMaterial_,trackWorld,view,projection,stats);
        stats.railHeadMaterialTriangles = railHeadMaterial_->triangles();
    }
    drawMesh(*trackDetail_, trackWorld, view, projection, stats);

    // Draw people before the train shell. The opaque interior can then mask bodies behind seat
    // backs, toilet walls, and cab doors while the final transparent-glass pass keeps the intended
    // window views. Explicit state restoration contains any AvatarRenderer state changes.
    drawAvatars(view, projection, passengers, seconds,
                static_cast<float>(renderDelta), playerX, playerZ, stats);
    drawPlatformPeople(view, projection, route, service, stats);
    device_.setBlendStateProperty(BlendState::Opaque);
    device_.setDepthStencilStateProperty(DepthStencilState::Default);
    effect_->setLightingEnabledProperty(true);
    drawStationClock(view,projection,route,service,stats);

    // The passenger saloon is independently lit, so it stays comfortable and readable while the
    // scenery beyond the windows transitions through a tunnel.
    const auto applySaloonLight = [](auto& effect) {
        // Broad, diffuse ceiling-strip approximation. Opposed longitudinal fill reveals both
        // seat orientations without specular points or a sun-facing/non-sun-facing black side.
        // Keep upward-facing light bounded: these lights supplement, not duplicate, the sun.
        effect.setAmbientLightColorProperty(Vector3{0.60f,0.61f,0.62f});
        effect.DirectionalLight0.setDiffuseColorProperty(Vector3{0.26f,0.25f,0.23f});
        effect.DirectionalLight1.setEnabledProperty(true);
        effect.DirectionalLight1.setDirectionProperty(Vector3{0.0f,-0.70710678f,-0.70710678f});
        effect.DirectionalLight1.setDiffuseColorProperty(Vector3{0.16f,0.17f,0.18f});
        effect.DirectionalLight1.setSpecularColorProperty(Vector3::Zero);
        effect.DirectionalLight2.setEnabledProperty(true);
        effect.DirectionalLight2.setDirectionProperty(Vector3{0.0f,-0.70710678f,0.70710678f});
        effect.DirectionalLight2.setDiffuseColorProperty(Vector3{0.16f,0.17f,0.18f});
        effect.DirectionalLight2.setSpecularColorProperty(Vector3::Zero);
    };
    applySaloonLight(*effect_);
    effect_->setDiffuseColorProperty(Vector3::One);
    const Matrix trainWorld = Matrix::CreateTranslation(0.0f,kTrainFloorElevation,0.0f);
    if (interiorMaterial_)
    {
        // Passenger-space materials use the independent interior illumination even in tunnels.
        applySaloonLight(*materialEffect_);
        setMaterialSpecular(Vector3{0.018f,0.018f,0.016f},8.0f,
                            Vector3{0.22f,0.21f,0.18f});
        materialEffect_->setTextureProperty(carpetDetailTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        device_.getSamplerStatesProperty()[0] = graphicsQuality_ >= 2
            ? SamplerState::AnisotropicWrap : SamplerState::LinearWrap;
        drawMaterialMesh(*interiorMaterial_,trainWorld,view,projection,stats);
        stats.interiorMaterialTriangles = interiorMaterial_->triangles();
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (upholsteryMaterial_)
    {
        setMaterialSpecular(Vector3{0.012f,0.012f,0.012f},6.0f,Vector3{0.20f,0.20f,0.20f});
        materialEffect_->setTextureProperty(upholsteryDetailTexture_.get());
        device_.getSamplerStatesProperty()[0] = graphicsQuality_ >= 2
            ? SamplerState::AnisotropicWrap : SamplerState::LinearWrap;
        drawMaterialMesh(*upholsteryMaterial_,trainWorld,view,projection,stats);
        stats.interiorMaterialTriangles += upholsteryMaterial_->triangles();
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (interiorPanelMaterial_)
    {
        setMaterialSpecular(Vector3{0.12f,0.12f,0.105f},42.0f,
                            Vector3{0.38f,0.36f,0.31f});
        materialEffect_->setTextureProperty(interiorPanelDetailTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        device_.getSamplerStatesProperty()[0] = graphicsQuality_ >= 2
            ? SamplerState::AnisotropicWrap : SamplerState::LinearWrap;
        drawMaterialMesh(*interiorPanelMaterial_,trainWorld,view,projection,stats);
        stats.interiorPanelMaterialTriangles = interiorPanelMaterial_->triangles();
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (interiorResilientMaterial_)
    {
        setMaterialSpecular(Vector3{0.075f,0.075f,0.068f},24.0f,
                            Vector3{0.31f,0.30f,0.26f});
        materialEffect_->setTextureProperty(resilientFloorDetailTexture_.get());
        materialEffect_->setDiffuseColorProperty(Vector3::One);
        device_.getSamplerStatesProperty()[0] = graphicsQuality_ >= 2
            ? SamplerState::AnisotropicWrap : SamplerState::LinearWrap;
        drawMaterialMesh(*interiorResilientMaterial_,trainWorld,view,projection,stats);
        stats.interiorResilientMaterialTriangles = interiorResilientMaterial_->triangles();
        device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    }
    if (graphicsQuality_ >= 1 && interiorShadows_)
    {
        effect_->setLightingEnabledProperty(false);
        effect_->setDiffuseColorProperty(Vector3::One);
        device_.setBlendStateProperty(BlendState::NonPremultiplied);
        device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
        drawMesh(*interiorShadows_,trainWorld,view,projection,stats);
        stats.interiorContactShadowTriangles = interiorShadows_->triangles();
        device_.setBlendStateProperty(BlendState::Opaque);
        device_.setDepthStencilStateProperty(DepthStencilState::Default);
        effect_->setLightingEnabledProperty(true);
    }
    drawMesh(*interior_, trainWorld, view, projection, stats);
    if (interiorEmitters_)
    {
        effect_->setLightingEnabledProperty(false);
        effect_->setDiffuseColorProperty(Vector3::One);
        drawMesh(*interiorEmitters_,trainWorld,view,projection,stats);
        effect_->setLightingEnabledProperty(true);
    }
    stats.paSpeakerGrilles = paSpeakerGrilleCount_;
    stats.hvacVents = hvacVentCount_;
    stats.carIdentitySigns = carIdentitySignCount_;
    stats.carFunctionFixtureGroups = carFunctionFixtureGroupCount_;
    stats.cabControlGroups = cabControlGroupCount_;
    stats.vestibulePortalDetailGroups = vestibulePortalDetailGroupCount_;
    stats.overheadLuggageItems = overheadLuggageItemCount_;
    stats.floorLuggageCases = floorLuggageCaseCount_;
    stats.floorLuggageWheelPairs = floorLuggageWheelPairCount_;
    stats.roundedSeatCushions = roundedSeatCushionCount_;
    stats.roundedSeatBacks = roundedSeatBackCount_;
    stats.seatPedestals = seatPedestalCount_;
    stats.seatMountFasteners = seatMountFastenerCount_;
    stats.roundedAisleArmrests = roundedAisleArmrestCount_;
    stats.roundedTableTops = roundedTableTopCount_;
    stats.tableSupportPlates = tableSupportPlateCount_;
    stats.aisleRunnerInlays = aisleRunnerInlayCount_;
    stats.nonCarpetedFloorZones = nonCarpetedFloorZoneCount_;
    stats.interiorCovingSections = interiorCovingSectionCount_;
    stats.ceilingPanelSeams = ceilingPanelSeamCount_;
    stats.vestibuleWearMarks = vestibuleWearMarkCount_;

    if (cabActiveDisplay_)
    {
        const float operatingDirection = service.direction >= 0 ? 1.0f : -1.0f;
        constexpr float cabNose = 72.8f;
        constexpr float screenFromNose = 1.21f;
        constexpr float screenFaceOffset = 0.061f;
        const float activeDisplayZ = operatingDirection
            * (cabNose-screenFromNose-screenFaceOffset);
        const Matrix activeDisplayWorld = Matrix::CreateRotationY(
            operatingDirection < 0.0f ? kPi : 0.0f)
            * Matrix::CreateTranslation(0.0f,kTrainFloorElevation+1.51f,activeDisplayZ);
        effect_->setLightingEnabledProperty(false);
        drawMesh(*cabActiveDisplay_,activeDisplayWorld,view,projection,stats);
        effect_->setLightingEnabledProperty(true);
        stats.activeCabDisplays = 1;
    }
    if (graphicsQuality_ >= 1 && interiorLightGradients_)
    {
        effect_->setLightingEnabledProperty(false);
        effect_->setDiffuseColorProperty(Vector3::One);
        device_.setBlendStateProperty(BlendState::NonPremultiplied);
        device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
        drawMesh(*interiorLightGradients_,trainWorld,view,projection,stats);
        stats.interiorLightGradientQuads = interiorLightGradientQuadCount_;
        device_.setBlendStateProperty(BlendState::Opaque);
        device_.setDepthStencilStateProperty(DepthStencilState::Default);
        effect_->setLightingEnabledProperty(true);
    }
    const float sunlightAmount = std::clamp(1.0f-tunnelLightingMix_*1.05f,0.0f,1.0f);
    if (sunlightAmount > 0.01f)
    {
        effect_->setLightingEnabledProperty(false);
        effect_->setAlphaProperty(sunlightAmount);
        device_.setBlendStateProperty(BlendState::NonPremultiplied);
        device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
        drawMesh(*sunlightPatches_,trainWorld,view,projection,stats);
        stats.sunlightPatches = sunlightPatchCount_;
        effect_->setAlphaProperty(1.0f);
        device_.setBlendStateProperty(BlendState::Opaque);
        device_.setDepthStencilStateProperty(DepthStencilState::Default);
        effect_->setLightingEnabledProperty(true);
    }
    const int displaySpeed = std::clamp(
        static_cast<int>(std::lround(service.speedMetresPerSecond * 3.6 / 5.0)) * 5, 0, 999);
    const int displayEta = std::clamp(
        static_cast<int>(std::ceil(std::max(0.0, etaSeconds) / 60.0)), 0, 99);
    const int informationDisplayPage = static_cast<int>(seconds / 6.0) % 3;
    const InformationDisplayState displayState{
        service.nextStation, service.direction,
        service.phase == Sim::ServicePhase::Dwelling,
        informationDisplayPage, displaySpeed, displayEta};
    if (!informationDisplays_ || informationDisplayState_ != displayState)
        rebuildInformationDisplays(route, service, displaySpeed, displayEta,
                                   informationDisplayPage);
    passengerDisplayEffect_->setWorldProperty(trainWorld);
    passengerDisplayEffect_->setViewProperty(view);
    passengerDisplayEffect_->setProjectionProperty(projection);
    device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    device_.setBlendStateProperty(BlendState::NonPremultiplied);
    device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
    for (auto& pass : passengerDisplayEffect_->getCurrentTechniqueProperty()->getPassesProperty())
    {
        pass.Apply();
        informationDisplays_->draw(device_);
        ++stats.drawCalls;
        stats.triangles += informationDisplays_->triangles();
    }
    device_.setDepthStencilStateProperty(DepthStencilState::Default);
    device_.setBlendStateProperty(BlendState::Opaque);

    // Each car has a double sliding passenger door. Only the platform side opens; the opposing
    // side remains visibly interlocked. Player collision retains the onboard boundary.
    for (int car = 0; car < 6; ++car)
    {
        const float doorZ = static_cast<float>(Sim::TrainInterior::passengerDoorZ(car));
        for (float side : {-1.0f, 1.0f})
        {
            const bool platformOnNegativeX = Sim::platformOnNegativeTrainX(service);
            const bool platformSide = side < 0.0f ? platformOnNegativeX : !platformOnNegativeX;
            const float openness = platformSide ? doorOpenAmount_ : 0.0f;
            const float panelOffset = 0.33f + openness * 0.63f;
            for (float panel : {-1.0f, 1.0f})
            {
                effect_->setDiffuseColorProperty(Vector3::One);
                drawMesh(*passengerDoor_,
                         Matrix::CreateRotationY(panel < 0.0f ? kPi : 0.0f)
                             * Matrix::CreateTranslation(side * 1.455f,
                                                         kTrainFloorElevation+1.17f,
                                                         doorZ + panel * panelOffset),
                         view, projection, stats);
            }
            drawBox(boxMatrix(side * 1.46f,kTrainFloorElevation+2.32f,doorZ,
                              0.10f,0.08f,2.05f),
                    view, projection, Color(230,181,40,255), stats);
            // A small passenger door button gives an immediate, believable indication of which
            // side is released. It remains dark while the train is moving and turns amber during
            // the interlocked closing phase.
            const Color buttonColor = !platformSide ? Color(49,58,60,255)
                : service.phase == Sim::ServicePhase::Dwelling ? Color(73,215,123,255)
                : service.phase == Sim::ServicePhase::ClosingDoors ? Color(238,178,45,255)
                : Color(49,58,60,255);
            effect_->setDiffuseColorProperty(rgb(buttonColor));
            drawMesh(*passengerDoorButton_,Matrix::CreateTranslation(
                side*1.45f,kTrainFloorElevation+1.36f,
                doorZ-static_cast<float>(Sim::TrainInterior::PassengerDoorHalfOpening)),
                view,projection,stats);
            effect_->setDiffuseColorProperty(Vector3::One);
            if (platformSide && openness > 0.01f)
            {
                // Deploy before the leaves finish opening and overlap the platform coping by two
                // centimetres at full travel. The collision capsule remains inside the body, so
                // this accessibility detail cannot accidentally let the player leave the train.
                const float extension = std::clamp(openness * 1.5f,0.0f,1.0f);
                const Matrix stepWorld = Matrix::CreateScale(extension,1.0f,1.0f)
                    * Matrix::CreateRotationY(side < 0.0f ? kPi : 0.0f)
                    * Matrix::CreateTranslation(side*static_cast<float>(
                        Sim::TrainInterior::BoardingStepInnerEdge),
                        kPlatformSurfaceElevation,doorZ);
                effect_->setDiffuseColorProperty(Vector3::One);
                drawMesh(*boardingStep_,stepWorld,view,projection,stats);
                ++stats.deployedBoardingSteps;
            }
        }
    }

    const Matrix doorWorld = Matrix::CreateRotationY(-1.45f * toiletDoorOpenAmount_)
                           * Matrix::CreateTranslation(0.37f,kTrainFloorElevation,4.07f);
    drawMesh(*toiletDoor_, doorWorld, view, projection, stats);
    const Matrix indicatorWorld = boxMatrix(-0.067f,1.72f,-0.35f,0.038f,0.16f,0.18f)
                                * doorWorld;
    drawBox(indicatorWorld,view,projection,
            toiletOccupied ? Color(210,55,45,255) : Color(57,177,91,255),stats);
    stats.occupiedToiletIndicators = toiletOccupied ? 1 : 0;

    // A low-opacity tint plus a narrow highlight gives the windows readable glass without
    // overpowering the scenery. DepthRead prevents transparent panes from occluding each other.
    device_.setBlendStateProperty(BlendState::NonPremultiplied);
    device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
    effect_->setDiffuseColorProperty(Vector3::One);
    // Automatic inter-car leaves react to both the player and aisle actors. Their transparent
    // centre pane and opaque frame share one mesh and pass; distant boundaries are culled.
    for (int boundary = 0; boundary < Sim::TrainInterior::GangwayCount; ++boundary)
    {
        const float boundaryZ = static_cast<float>(Sim::TrainInterior::gangwayZ(boundary));
        if (std::abs(playerZ - boundaryZ) > 38.0f) continue;
        const float openness = gangwayDoorOpenAmounts_[static_cast<std::size_t>(boundary)];
        for (float panel : {-1.0f, 1.0f})
            drawMesh(*gangwayDoor_,
                     Matrix::CreateTranslation(
                         panel * (0.29f + openness * 0.64f),
                         kTrainFloorElevation + 1.20f,boundaryZ),
                     view,projection,stats);
    }
    drawMesh(*passengerDoorGlass_,trainWorld,view,projection,stats);
    drawMesh(*windowGlass_, trainWorld, view, projection, stats);
    if (graphicsQuality_ >= 1)
    {
        // A fake Fresnel term is weakest when looking squarely through a side window and grows
        // while looking along the pane. It modulates only restrained reflection accents, never
        // the transparent base glass or exterior scene.
        const float faceOn = std::abs(std::sin(cameraYaw));
        effect_->setAlphaProperty(0.13f+(1.0f-faceOn)*0.25f);
        for (auto& reflections : windowReflections_)
            if (reflections) drawMesh(*reflections,trainWorld,view,projection,stats);
        effect_->setAlphaProperty(1.0f);
        stats.windowReflectionAccents = windowReflectionAccentCount_;
    }
    device_.setBlendStateProperty(BlendState::Opaque);
    device_.setDepthStencilStateProperty(DepthStencilState::Default);
    stats.avatarVisuals = static_cast<int>(avatars_.size());
    stats.readyAvatarVisuals = static_cast<int>(std::count_if(
        avatars_.begin(),avatars_.end(),[](const auto& avatar) {
            return avatar->renderer->getStateProperty() == AvatarRendererState::Ready;
        }));
    stats.civilianAvatarDescriptions = civilianAvatarDescriptionCount_;
    return stats;
}

} // namespace TrainSim::Render
