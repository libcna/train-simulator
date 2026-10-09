#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstddef>
#include <array>
#include <cstdint>
#include <vector>

namespace TrainSim::Render {

// CNA BasicEffect's lit vertex-colour path follows XNA's position/normal/colour/UV layout. Keep
// this as a packed GPU stream rather than storing Color objects, whose C++ wrapper is not four
// bytes even though the corresponding vertex element is.
struct LitColorVertex
{
    Microsoft::Xna::Framework::Vector3 position;
    Microsoft::Xna::Framework::Vector3 normal;
    std::uint32_t color = 0;
    Microsoft::Xna::Framework::Vector2 textureCoordinate;
};
static_assert(sizeof(LitColorVertex) == 36);

struct MeshData
{
    std::vector<LitColorVertex> vertices;
    std::vector<std::uint32_t> indices;
    [[nodiscard]] bool empty() const noexcept { return vertices.empty() || indices.empty(); }
};

class MeshBuilder
{
public:
    void clear() noexcept { data_.vertices.clear(); data_.indices.clear(); }
    [[nodiscard]] const MeshData& data() const noexcept { return data_; }
    // Fixed front projection for thin panels; avoids dominant-axis ties at rounded edges.
    void projectXYDetailUVs(std::size_t firstVertex,
                           const Microsoft::Xna::Framework::Vector3& origin,float metresPerTile);
    // Dominant-face planar detail mapping, relative to an object-local origin.
    void projectDetailUVs(std::size_t firstVertex,
        const Microsoft::Xna::Framework::Vector3& origin,float metresPerTile);
    void addSmoothTexturedQuad(
        const std::array<Microsoft::Xna::Framework::Vector3,4>& positions,
        const std::array<Microsoft::Xna::Framework::Vector3,4>& normals,
        const std::array<Microsoft::Xna::Framework::Vector2,4>& uvs,
        const Microsoft::Xna::Framework::Color& color);
    void reserve(std::size_t vertexCapacity, std::size_t indexCapacity)
    {
        data_.vertices.reserve(vertexCapacity);
        data_.indices.reserve(indexCapacity);
    }
    void addBox(const Microsoft::Xna::Framework::Vector3& minimum,
                const Microsoft::Xna::Framework::Vector3& maximum,
                const Microsoft::Xna::Framework::Color& color);
    void addBox(const Microsoft::Xna::Framework::Vector3& minimum,
                const Microsoft::Xna::Framework::Vector3& maximum,
                const Microsoft::Xna::Framework::Color& color,
                const Microsoft::Xna::Framework::Matrix& transform);
    // Axis-aligned cuboid whose four vertical corners are rounded in the XZ plane. This is a
    // bounded low-poly primitive for cushions, tabletops, and similar horizontal fixtures.
    void addRoundedBoxXZ(const Microsoft::Xna::Framework::Vector3& minimum,
                         const Microsoft::Xna::Framework::Vector3& maximum,
                         float cornerRadius, int cornerSegments,
                         const Microsoft::Xna::Framework::Color& color);
    // Axis-aligned cuboid whose front/back silhouette is rounded in the XY plane. Intended for
    // seat backs and other thin upright fixtures viewed at passenger-eye height.
    void addRoundedBoxXY(const Microsoft::Xna::Framework::Vector3& minimum,
                         const Microsoft::Xna::Framework::Vector3& maximum,
                         float cornerRadius, int cornerSegments,
                         const Microsoft::Xna::Framework::Color& color);
    // Axis-aligned cuboid whose aisle-facing silhouette is rounded in the YZ plane. Intended for
    // luggage and wall-side fixtures viewed along the train's lateral axis.
    void addRoundedBoxYZ(const Microsoft::Xna::Framework::Vector3& minimum,
                         const Microsoft::Xna::Framework::Vector3& maximum,
                         float cornerRadius, int cornerSegments,
                         const Microsoft::Xna::Framework::Color& color);
    void addCylinder(const Microsoft::Xna::Framework::Vector3& centre, float radius,
                     float height, int sides, const Microsoft::Xna::Framework::Color& color);
    void addCone(const Microsoft::Xna::Framework::Vector3& baseCentre, float radius,
                 float height, int sides, const Microsoft::Xna::Framework::Color& color);
    void addEllipsoid(const Microsoft::Xna::Framework::Vector3& centre,
                      const Microsoft::Xna::Framework::Vector3& radii,
                      int slices, int stacks,
                      const Microsoft::Xna::Framework::Color& color, bool smoothNormals = false);
    void addFoliageLobe(const Microsoft::Xna::Framework::Vector3& centre,
                        const Microsoft::Xna::Framework::Vector3& radii,
                        int slices, int stacks,
                        const Microsoft::Xna::Framework::Color& color, bool smooth);
    void addDiscXY(const Microsoft::Xna::Framework::Vector3& centre, float radius,
                   int sides, const Microsoft::Xna::Framework::Color& color);
    void addDiscXZ(const Microsoft::Xna::Framework::Vector3& centre, float radius,
                   int sides, const Microsoft::Xna::Framework::Color& color);
    void addQuad(const Microsoft::Xna::Framework::Vector3& a,
                 const Microsoft::Xna::Framework::Vector3& b,
                 const Microsoft::Xna::Framework::Vector3& c,
                 const Microsoft::Xna::Framework::Vector3& d,
                 const Microsoft::Xna::Framework::Color& color,
                 const Microsoft::Xna::Framework::Vector3& normalA,
                 const Microsoft::Xna::Framework::Vector3& normalB,
                 const Microsoft::Xna::Framework::Vector3& normalC,
                 const Microsoft::Xna::Framework::Vector3& normalD);
    void addQuad(const Microsoft::Xna::Framework::Vector3& a,
                 const Microsoft::Xna::Framework::Vector3& b,
                 const Microsoft::Xna::Framework::Vector3& c,
                 const Microsoft::Xna::Framework::Vector3& d,
                 const Microsoft::Xna::Framework::Color& color);
    void addQuad(const Microsoft::Xna::Framework::Vector3& a,
                 const Microsoft::Xna::Framework::Vector3& b,
                 const Microsoft::Xna::Framework::Vector3& c,
                 const Microsoft::Xna::Framework::Vector3& d,
                 const Microsoft::Xna::Framework::Color& colorA,
                 const Microsoft::Xna::Framework::Color& colorB,
                 const Microsoft::Xna::Framework::Color& colorC,
                 const Microsoft::Xna::Framework::Color& colorD);
    void addTexturedQuad(const Microsoft::Xna::Framework::Vector3& a,
                         const Microsoft::Xna::Framework::Vector3& b,
                         const Microsoft::Xna::Framework::Vector3& c,
                         const Microsoft::Xna::Framework::Vector3& d,
                         const Microsoft::Xna::Framework::Color& color,
                         const Microsoft::Xna::Framework::Vector2& uvA,
                         const Microsoft::Xna::Framework::Vector2& uvB,
                         const Microsoft::Xna::Framework::Vector2& uvC,
                         const Microsoft::Xna::Framework::Vector2& uvD);
    void addTexturedQuad(const Microsoft::Xna::Framework::Vector3& a,
                         const Microsoft::Xna::Framework::Vector3& b,
                         const Microsoft::Xna::Framework::Vector3& c,
                         const Microsoft::Xna::Framework::Vector3& d,
                         const Microsoft::Xna::Framework::Color& colorA,
                         const Microsoft::Xna::Framework::Color& colorB,
                         const Microsoft::Xna::Framework::Color& colorC,
                         const Microsoft::Xna::Framework::Color& colorD,
                         const Microsoft::Xna::Framework::Vector2& uvA,
                         const Microsoft::Xna::Framework::Vector2& uvB,
                         const Microsoft::Xna::Framework::Vector2& uvC,
                         const Microsoft::Xna::Framework::Vector2& uvD);
    [[nodiscard]] MeshData take() { return std::move(data_); }

    void addTriangle(const Microsoft::Xna::Framework::Vector3& a,
                     const Microsoft::Xna::Framework::Vector3& b,
                     const Microsoft::Xna::Framework::Vector3& c,
                     const Microsoft::Xna::Framework::Color& color);
    void addTexturedTriangle(const std::array<Microsoft::Xna::Framework::Vector3,3>& positions,
                             const std::array<Microsoft::Xna::Framework::Color,3>& colors,
                             const std::array<Microsoft::Xna::Framework::Vector2,3>& uvs);
private:
    MeshData data_;
};

} // namespace TrainSim::Render
