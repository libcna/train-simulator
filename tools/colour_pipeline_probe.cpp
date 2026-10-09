// Project-owned OpenGL 3.3 colour calibration experiment, not a replacement game renderer.
#include "trainsim/render/GpuMesh.hpp"
#include "trainsim/render/ProceduralMaterials.hpp"
#include "trainsim/render/Daylight.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/ShaderEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {
constexpr const char* vertex=R"(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location=3) in vec4 colour;
out vec4 tint;
out vec3 surfaceNormal;
out vec2 texcoord;
out float depth;
uniform vec2 pixelCentre;
void main() {
    gl_Position=vec4(position.xy+pixelCentre,0.0,1.0); tint=colour;
    surfaceNormal=normal; texcoord=uv; depth=-position.z;
}
)";
constexpr const char* fragment=R"(#version 330 core
in vec4 tint;
in vec3 surfaceNormal;
in vec2 texcoord;
in float depth;
uniform vec4 calibration; // x: diffuse illumination; y: enable linear-light calculation
uniform float materialMode;
uniform vec3 sunDirection, ambient, diffuse, fogColour;
uniform sampler2D materialTexture;
out vec4 result;
vec3 decode(vec3 c) {
    return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),c));
}
vec3 encode(vec3 c) {
    return mix(c*12.92,1.055*pow(c,vec3(1.0/2.4))-0.055,step(vec3(0.0031308),c));
}
void main() {
    vec3 albedo=tint.rgb;
    vec3 lighting=vec3(calibration.x);
    float visibility=1.0;
    if(materialMode>0.5) {
        albedo*=texture(materialTexture,texcoord).rgb;
        lighting=ambient+diffuse*max(dot(normalize(surfaceNormal),-sunDirection),0.0);
        visibility=clamp(1.0-depth/100.0,0.0,1.0);
    }
    vec3 legacy=mix(fogColour,albedo*lighting,visibility);
    // The encoded palette-times-detail product is treated as authored surface colour.
    vec3 linear=encode(mix(decode(fogColour),decode(albedo)*lighting,visibility));
    result=vec4(mix(legacy,linear,calibration.y),1.0);
}
)";
double decode(double c) { return c<=0.04045 ? c/12.92 : std::pow((c+0.055)/1.055,2.4); }
double encode(double c) { return c<=0.0031308 ? c*12.92 : 1.055*std::pow(c,1.0/2.4)-0.055; }

class ColourProbe final : public Game
{
    GraphicsDeviceManager graphics_{this};
    std::string output_;
    bool drawn_=false;
    bool materials_=false;
    bool uvRamp_=false;
    bool cold_=false;
public:
    int result=1;
    explicit ColourProbe(std::string output,bool materials,bool uvRamp,bool cold)
        : output_(std::move(output)),materials_(materials),uvRamp_(uvRamp),cold_(cold)
    {
        graphics_.setPreferredBackBufferWidthProperty(900);
        graphics_.setGraphicsProfileProperty(GraphicsProfile::HiDef);
        graphics_.setPreferredBackBufferHeightProperty(360);
        graphics_.setSynchronizeWithVerticalRetraceProperty(false);
    }
protected:
    void Draw(const GameTime&) override
    {
        if (drawn_) return;
        drawn_=true;
        auto& device=getGraphicsDeviceProperty();
        device.Clear(Color(20,24,28,255));
        device.SetDepthTestEnabled(false);
        device.setBlendStateProperty(BlendState::Opaque);
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        BasicEffect stock(device);
        stock.setWorldProperty(Matrix::getIdentityProperty());
        stock.setViewProperty(Matrix::getIdentityProperty());
        stock.setProjectionProperty(Matrix::getIdentityProperty());
        stock.setVertexColorEnabledProperty(true);
        stock.setLightingEnabledProperty(true);
        stock.setPreferPerPixelLightingProperty(true);
        stock.setDiffuseColorProperty(Vector3::One);
        stock.setSpecularColorProperty(Vector3::Zero);
        stock.DirectionalLight0.setEnabledProperty(false);
        stock.DirectionalLight1.setEnabledProperty(false);
        stock.DirectionalLight2.setEnabledProperty(false);
        ShaderEffect comparison(device,vertex,fragment);
        if (!comparison.IsEffectValid()) throw std::runtime_error("Calibration shader compilation failed");
        std::unique_ptr<Texture2D> material;
        const auto daylight=TrainSim::Render::exteriorLighting(0);
        const auto sun=TrainSim::Render::daylightDirection(0);
        const Vector3 fog{0.56f,0.66f,0.73f};
        if (materials_)
        {
            auto image=TrainSim::Render::makeMasonryDetailImage();
            if (uvRamp_)
                for (int y=0;y<image.height;++y)
                    for (int x=0;x<image.width;++x)
                        image.pixels[static_cast<std::size_t>(y*image.width+x)]=Color(x*2,y*2,255,255);
            std::vector<std::uint8_t> bytes;
            for (const auto& c : image.pixels)
                for (const auto channel : {c.getRProperty(),c.getGProperty(),c.getBProperty(),c.getAProperty()})
                    bytes.push_back(channel);
            material=std::make_unique<Texture2D>(Texture2D::CreateFromPixels(device,image.width,image.height,bytes));
            stock.setTextureProperty(material.get());
            stock.setTextureEnabledProperty(true);
            stock.setProjectionProperty(Matrix::CreateOrthographic(2,2,0,100));
            stock.setAmbientLightColorProperty(daylight.ambient);
            stock.DirectionalLight0.setEnabledProperty(true);
            stock.DirectionalLight0.setDirectionProperty(sun);
            stock.DirectionalLight0.setDiffuseColorProperty(daylight.diffuse);
            stock.DirectionalLight0.setSpecularColorProperty(Vector3::Zero);
            stock.setFogEnabledProperty(true);
            stock.setFogStartProperty(0);
            stock.setFogEndProperty(100);
            stock.setFogColorProperty(fog);
            device.getSamplerStatesProperty()[0]=SamplerState::PointClamp;
        }
        const std::array<Color,3> palette={Color(128,128,128,255),Color(192,166,139,255),Color(77,108,66,255)};
        const std::array<float,3> light={0.25f,0.5f,1.0f};
        const int width=device.getViewportProperty().getWidthProperty();
        const int height=device.getViewportProperty().getHeightProperty();
        int checked=0;
        bool valid=true;
        std::array<std::array<int,3>,9> stockPixels{};
        std::vector<std::unique_ptr<TrainSim::Render::GpuMesh>> meshes;
        const std::array<Vector3,3> normals={Vector3::Up,Vector3::Left,Vector3::Right};
        for (int row=0;row<3;++row)
            for (int col=0;col<9;++col)
            {
                const auto& color=palette[materials_ ? 1U : static_cast<std::size_t>(col/3)];
                const float intensity=light[static_cast<std::size_t>(col%3)];
                const float x=-1+2.0f*static_cast<float>(col)/9;
                const float y=1-2.0f*static_cast<float>(row)/3;
                TrainSim::Render::MeshBuilder builder;
                const float z=materials_ ? -50.0f*static_cast<float>(col/3) : 0;
                builder.addTexturedQuad({x+0.01f,y-0.02f,z},{x+0.01f,y-0.64f,z},
                    {x+0.212f,y-0.64f,z},{x+0.212f,y-0.02f,z},color,{0,0},{0,1},{1,1},{1,0});
                auto data=builder.take();
                if (materials_) for (auto& v : data.vertices) v.normal=normals[static_cast<std::size_t>(col%3)];
                meshes.push_back(std::make_unique<TrainSim::Render::GpuMesh>(device,data));
                auto& mesh=*meshes.back();
                if (row==0)
                {
                    if (materials_) device.getTexturesProperty()(0,material.get());
                    if (!materials_) stock.setAmbientLightColorProperty(Vector3{intensity,intensity,intensity});
                    stock.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
                }
                else
                {
                    comparison.Apply();
                    // Match CNA OPENGL33's non-MSAA XNA pixel-centre convention, not a gamma change.
                    comparison.SetUniformVec2("pixelCentre",(63.0f/64.0f)/static_cast<float>(width),
                        -(63.0f/64.0f)/static_cast<float>(height));
                    comparison.SetUniformVec4("calibration",intensity,row==2 ? 1.0f : 0.0f,0,0);
                    comparison.SetUniformFloat("materialMode",materials_ ? 1.0f : 0.0f);
                    if (materials_)
                    {
                        comparison.SetUniformVec3("sunDirection",sun.X,sun.Y,sun.Z);
                        comparison.SetUniformVec3("ambient",daylight.ambient.X,daylight.ambient.Y,daylight.ambient.Z);
                        comparison.SetUniformVec3("diffuse",daylight.diffuse.X,daylight.diffuse.Y,daylight.diffuse.Z);
                        comparison.SetUniformVec3("fogColour",fog.X,fog.Y,fog.Z);
                        comparison.SetTexture(0,*material);
                        comparison.SetUniformInt("materialTexture",0);
                        device.getSamplerStatesProperty()[0]=SamplerState::PointClamp;
                    }
                }
                mesh.draw(device);
                // Separate first-use state from the colour comparison; --materials-cold reproduces it.
                if (materials_ && row==0 && col==0 && !cold_)
                {
                    stock.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
                    mesh.draw(device);
                }
                const Rectangle region((2*col+1)*width/18,(2*row+1)*height/6,1,1);
                Color pixel;
                device.GetBackBufferData(&region,&pixel,0,1);
                const std::array<int,3> input={color.getRProperty(),color.getGProperty(),color.getBProperty()};
                const std::array<int,3> actual={pixel.getRProperty(),pixel.getGProperty(),pixel.getBProperty()};
                if (materials_ && row==0) stockPixels[static_cast<std::size_t>(col)]=actual;
                for (std::size_t c=0;c<3;++c)
                {
                    if (materials_)
                    {
                        const int original=stockPixels[static_cast<std::size_t>(col)][c];
                        if ((row==1 && std::abs(actual[c]-original)>2)
                            || (col>=6 && std::abs(actual[c]-static_cast<int>(std::lround(
                                (c==0 ? fog.X : c==1 ? fog.Y : fog.Z)*255)))>2)
                            || (row==2 && col<6 && actual[c]<original-2)
                            || (row==2 && col==2 && actual[c]<original+5))
                        {
                            valid=false;
                            std::cerr<<"Material lighting/fog mismatch row="<<row<<" col="<<col
                                <<" channel="<<c<<" actual="<<actual[c]<<" stock="<<original<<'\n';
                        }
                        continue;
                    }
                    const double v=static_cast<double>(input[c])/255;
                    const int expected=static_cast<int>(std::lround(255*(row==2 ? encode(decode(v)*intensity) : v*intensity)));
                    if (std::abs(actual[c]-expected)>2)
                        throw std::runtime_error("Colour readback mismatch row="+std::to_string(row)
                            +" col="+std::to_string(col)+" actual="+std::to_string(actual[c])
                            +" expected="+std::to_string(expected));
                }
                ++checked;
                if (col==1) std::cout<<"ROW "<<row<<(materials_ ? " masonry_red=" : " neutral_half=")<<actual[0]
                    <<" green="<<actual[1]<<" blue="<<actual[2]<<'\n';
            }
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width*height)*4);
        const Rectangle all(0,0,width,height);
        device.GetBackBufferData(&all,pixels.data(),0,static_cast<int>(pixels.size()));
        auto screenshot=Texture2D::CreateFromPixels(device,width,height,pixels);
        screenshot.SaveAsPng(output_);
        std::cout<<(valid ? (materials_ ? "COLOR_MATERIAL_PASS patches=" : "COLOR_PIPELINE_PASS patches=")
                         : "COLOR_MATERIAL_FAIL patches=")<<checked<<" output="<<output_<<'\n';
        result=valid ? 0 : 1;
        Exit();
    }
};
}
int main(int argc,char** argv)
{
    try {
        const std::string mode=argc>2 ? argv[2] : "";
        if (!mode.empty() && mode!="--materials" && mode!="--materials-uv" && mode!="--materials-cold")
            throw std::invalid_argument("Expected --materials, --materials-uv or --materials-cold");
        ColourProbe probe(argc>1 ? argv[1] : "build/colour-pipeline.png",
            !mode.empty(),mode=="--materials-uv",mode=="--materials-cold");
        probe.Run(); return probe.result;
    }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
