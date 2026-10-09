#pragma once
#include <memory>
#include "Microsoft/Xna/Framework/Matrix.hpp"
namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice; class BasicEffect; class ShaderEffect;
}
namespace TrainSim::Render {
class GpuMesh;
// Renderer-local experiment: opaque exterior facades only; no simulation dependency.
class OpenGL33FacadeEffect
{
public:
    explicit OpenGL33FacadeEffect(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);
    ~OpenGL33FacadeEffect();
    void draw(GpuMesh& mesh,Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
        Microsoft::Xna::Framework::Graphics::BasicEffect& reference,
        const Microsoft::Xna::Framework::Matrix& world,const Microsoft::Xna::Framework::Matrix& view,
        const Microsoft::Xna::Framework::Matrix& projection,bool linear);
private:
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::ShaderEffect> effect_;
};
}
