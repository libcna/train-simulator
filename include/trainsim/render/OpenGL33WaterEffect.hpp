#pragma once
#include <memory>
#include "Microsoft/Xna/Framework/Matrix.hpp"
namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice; class BasicEffect; class ShaderEffect;
}
namespace TrainSim::Render {
class GpuMesh;
// Opaque, single-submission water approximation confined to the renderer.
class OpenGL33WaterEffect
{
public:
    explicit OpenGL33WaterEffect(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);
    ~OpenGL33WaterEffect();
    bool valid() const;
    void draw(GpuMesh& mesh,Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
        Microsoft::Xna::Framework::Graphics::BasicEffect& reference,
        const Microsoft::Xna::Framework::Matrix& world,const Microsoft::Xna::Framework::Matrix& view,
        const Microsoft::Xna::Framework::Matrix& projection,double serviceSeconds);
private:
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::ShaderEffect> effect_;
};
}
