#include "trainsim/render/GpuMesh.hpp"

#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicVertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicIndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/SetDataOptions.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementUsage.hpp"

#include <cstddef>
#include <stdexcept>

using namespace Microsoft::Xna::Framework::Graphics;

namespace TrainSim::Render {

namespace {
const VertexDeclaration& litColorDeclaration()
{
    static const VertexDeclaration declaration(static_cast<int>(sizeof(LitColorVertex)), {
        VertexElement(static_cast<int>(offsetof(LitColorVertex, position)),
                      VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
        VertexElement(static_cast<int>(offsetof(LitColorVertex, normal)),
                      VertexElementFormat::Vector3, VertexElementUsage::Normal, 0),
        VertexElement(static_cast<int>(offsetof(LitColorVertex, textureCoordinate)),
                      VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0),
        VertexElement(static_cast<int>(offsetof(LitColorVertex, color)),
                      VertexElementFormat::Color, VertexElementUsage::Color, 0),
    });
    return declaration;
}
}

GpuMesh::GpuMesh(GraphicsDevice& device, const MeshData& data, bool streamed)
    : streamed_(streamed)
{
    if (streamed_) { replace(device,data); return; }
    if (data.empty() || data.indices.size() % 3 != 0)
        throw std::invalid_argument("GpuMesh requires a non-empty triangle list");
    vertexCount_ = static_cast<int>(data.vertices.size());
    primitiveCount_ = static_cast<int>(data.indices.size() / 3);
    vertices_ = std::make_unique<VertexBuffer>(device,
        litColorDeclaration(), vertexCount_, BufferUsage::WriteOnly);
    vertices_->SetDataRaw(data.vertices.data(), vertexCount_, static_cast<int>(sizeof(LitColorVertex)));
    indices_ = std::make_unique<IndexBuffer>(device, IndexElementSize::ThirtyTwoBits,
                                             static_cast<int>(data.indices.size()), BufferUsage::WriteOnly);
    indices_->SetData(data.indices.data(), static_cast<int>(data.indices.size()));
}

int GpuMesh::replace(GraphicsDevice& device, const MeshData& data)
{
    if (!streamed_) throw std::logic_error("Only streamed meshes support replacement");
    if (data.empty() || data.indices.size()%3 != 0)
        throw std::invalid_argument("GpuMesh requires a non-empty triangle list");
    const int vertices=static_cast<int>(data.vertices.size());
    const int primitives=static_cast<int>(data.indices.size()/3);
    const int reused=(vertices_ && vertices == vertexCount_ ? 1 : 0)
                    +(indices_ && primitives == primitiveCount_ ? 1 : 0);
    if (!vertices_ || vertices != vertexCount_)
        vertices_=std::make_unique<DynamicVertexBuffer>(device,litColorDeclaration(),vertices,
                                                       BufferUsage::WriteOnly);
    if (!indices_ || primitives != primitiveCount_)
        indices_=std::make_unique<DynamicIndexBuffer>(device,IndexElementSize::ThirtyTwoBits,
                                                     primitives*3,BufferUsage::WriteOnly);
    static_cast<DynamicVertexBuffer&>(*vertices_).SetDataRawWithOptionsEXT(
        data.vertices.data(),vertices,static_cast<int>(sizeof(LitColorVertex)),SetDataOptions::Discard);
    static_cast<DynamicIndexBuffer&>(*indices_).SetData(
        data.indices.data(),0,primitives*3,SetDataOptions::Discard);
    vertexCount_=vertices;
    primitiveCount_=primitives;
    return reused;
}

GpuMesh::~GpuMesh() = default;

void GpuMesh::draw(GraphicsDevice& device) const
{
    device.SetVertexBuffer(vertices_.get());
    device.SetIndexBuffer(indices_.get());
    device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, vertexCount_, 0, primitiveCount_);
}

} // namespace TrainSim::Render
