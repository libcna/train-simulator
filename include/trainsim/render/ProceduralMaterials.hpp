#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"

#include <memory>
#include <vector>

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
class Texture2D;
}

namespace TrainSim::Render {

struct ProceduralImage
{
    int width = 0;
    int height = 0;
    std::vector<Microsoft::Xna::Framework::Color> pixels;

    [[nodiscard]] ProceduralImage downsampled() const;
    [[nodiscard]] ProceduralImage downsampledForAlphaTest(int referenceAlpha,double coverage) const;
};

// Neutral detail is deliberate: the vertex colour remains authoritative for grass, soil, urban
// verge, and biome tint while this reusable texture contributes fine structure and mip stability.
[[nodiscard]] ProceduralImage makeTerrainDetailImage(int size = 128);
[[nodiscard]] ProceduralImage makeFoliageDetailImage(int size = 128);
[[nodiscard]] ProceduralImage makeNeedleDetailImage(int size = 128);
[[nodiscard]] ProceduralImage makeLeafSprayImage(int size = 128);
[[nodiscard]] ProceduralImage makeNeedleSprayImage(int size = 128);
[[nodiscard]] Microsoft::Xna::Framework::Color ruralTerrainSurfaceTint(double distance,double lateral);
[[nodiscard]] ProceduralImage makeRoofDetailImage(int size = 128);

// Absolute route coordinates keep broad urban verge colour continuous across streamed chunks.
[[nodiscard]] Microsoft::Xna::Framework::Color urbanGroundTint(double distance, double lateral);
[[nodiscard]] Microsoft::Xna::Framework::Color ruralParcelTint(double distance,double lateral);
[[nodiscard]] Microsoft::Xna::Framework::Color terminalGroundTint(
    double distance,double lateral,double routeLength);

// Neutral greyscale sources are multiplied by authored vertex tints. Keeping colour identity in
// geometry lets one compact texture serve every ballast bed and every carriage carpet variant.
[[nodiscard]] ProceduralImage makeBallastDetailImage(int size = 128);
[[nodiscard]] ProceduralImage makeCarpetDetailImage(int size = 128);
[[nodiscard]] ProceduralImage makeUpholsteryDetailImage(int size = 256);
[[nodiscard]] ProceduralImage makeConcreteDetailImage(int size = 128);
[[nodiscard]] ProceduralImage makeInteriorPanelDetailImage(int size = 128);
[[nodiscard]] ProceduralImage makeWaterDetailImage(int size = 128);
[[nodiscard]] ProceduralImage makeResilientFloorDetailImage(int size = 128);
[[nodiscard]] ProceduralImage makeMasonryDetailImage(int size = 128);
[[nodiscard]] ProceduralImage makeRenderedFacadeDetailImage(int size = 128);
// Non-tiling fair-weather cloud atlas with a transparent feathered silhouette. It is generated
// once and shared by every sky billboard, avoiding external imagery and horizon-shaped polygons.
[[nodiscard]] ProceduralImage makeCloudDetailImage(int size = 256,unsigned variant = 0);
[[nodiscard]] ProceduralImage makeCloudAtlasImage(int tileSize = 256);

[[nodiscard]] std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D>
uploadMipmappedTexture(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                       const ProceduralImage& image,int alphaTestReference = -1);

} // namespace TrainSim::Render
