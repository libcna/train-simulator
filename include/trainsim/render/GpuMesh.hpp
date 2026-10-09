#pragma once

#include "trainsim/render/MeshBuilder.hpp"

#include <memory>

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
class IndexBuffer;
class VertexBuffer;
}

namespace TrainSim::Render {

class GpuMesh
{
public:
    GpuMesh(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const MeshData& data,
            bool streamed = false);
    // Full Discard uploads reuse exact-sized buffers; changed topology reallocates as needed.
    int replace(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const MeshData& data);
    ~GpuMesh();
    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;
    void draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device) const;
    [[nodiscard]] int triangles() const noexcept { return primitiveCount_; }

private:
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vertices_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> indices_;
    int vertexCount_ = 0;
    int primitiveCount_ = 0;
    bool streamed_ = false;
};

} // namespace TrainSim::Render
