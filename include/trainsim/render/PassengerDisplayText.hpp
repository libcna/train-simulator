#pragma once
#include "trainsim/render/DisplayFontData.hpp"
#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/render/ProceduralMaterials.hpp"
#include <algorithm>
#include <string_view>
#include <stdexcept>

namespace TrainSim::Render {
inline ProceduralImage makePassengerDisplayFontImage()
{
    ProceduralImage image;
    image.width=512;
    image.height=256;
    image.pixels.reserve(512*256);
    for (const auto run : DisplayFontData::CoverageRuns)
        image.pixels.insert(image.pixels.end(),run>>8,
            Microsoft::Xna::Framework::Color(255,255,255,run&255));
    if (image.pixels.size()!=512*256) throw std::logic_error("Invalid passenger font coverage");
    return image;
}

inline void addPassengerScreenText(MeshBuilder& mesh,std::string_view text,float baseline,
                                    float z,bool mirrored,
                                    const Microsoft::Xna::Framework::Color& color,
                                    float metresPerPixel,float leftEdge=-0.85f,float maximumWidth=1.70f)
{
    using namespace Microsoft::Xna::Framework;
    if (text.empty()) return;
    const auto glyph=[](unsigned char code)->const DisplayFontData::Glyph& {
        return DisplayFontData::Glyphs[(code>=32 && code<=126 ? code : '?')-32];
    };
    float advance=0;
    for (unsigned char code : text) advance+=static_cast<float>(glyph(code).advance64)/64;
    const float scale=std::min(metresPerPixel,maximumWidth/(advance+4));
    float cursor=leftEdge;
    for (unsigned char code : text)
    {
        const auto& g=glyph(code);
        if (g.width>0 && g.height>0)
        {
            const float left=cursor+static_cast<float>(g.left)*scale;
            const float right=left+static_cast<float>(g.width)*scale;
            const float top=baseline-static_cast<float>(g.top)*scale;
            const float bottom=top-static_cast<float>(g.height)*scale;
            const float x0=mirrored ? -left : left,x1=mirrored ? -right : right;
            const Vector3 a{x0,bottom,z},b{x1,bottom,z},c{x1,top,z},d{x0,top,z};
            const Vector2 uvA{static_cast<float>(g.x)/512,static_cast<float>(g.y+g.height)/256};
            const Vector2 uvB{static_cast<float>(g.x+g.width)/512,uvA.Y};
            const Vector2 uvC{uvB.X,static_cast<float>(g.y)/256},uvD{uvA.X,uvC.Y};
            mesh.addTexturedQuad(a,b,c,d,color,uvA,uvB,uvC,uvD);
        }
        cursor+=static_cast<float>(g.advance64)/64*scale;
    }
}

// Printed world signage shares the licensed antialiased atlas, with one flat quad
// per visible glyph. Placement controls which platform face points toward the train.
inline void addPrintedSignText(MeshBuilder& mesh,std::string_view text,float maximumWidth,
    const Microsoft::Xna::Framework::Matrix& placement,
    const Microsoft::Xna::Framework::Color& colour)
{
    using namespace Microsoft::Xna::Framework;
    if (text.empty() || maximumWidth<=0) return;
    float advance=0;
    int ascent=0;
    for (unsigned char code : text)
    {
        const auto& glyph=DisplayFontData::Glyphs[(code>=32 && code<=126 ? code : '?')-32];
        advance+=static_cast<float>(glyph.advance64)/64;
        ascent=std::max(ascent,-glyph.top);
    }
    const float scale=std::min(0.011f,maximumWidth/(advance+4));
    MeshBuilder glyphs;
    addPassengerScreenText(glyphs,text,-static_cast<float>(ascent)*scale,0,false,
        colour,scale,-advance*scale*0.5f,maximumWidth);
    const auto& vertices=glyphs.data().vertices;
    for (std::size_t i=0;i+3<vertices.size();i+=4)
        mesh.addTexturedQuad(Vector3::Transform(vertices[i].position,placement),
            Vector3::Transform(vertices[i+1].position,placement),
            Vector3::Transform(vertices[i+2].position,placement),
            Vector3::Transform(vertices[i+3].position,placement),colour,
            vertices[i].textureCoordinate,vertices[i+1].textureCoordinate,
            vertices[i+2].textureCoordinate,vertices[i+3].textureCoordinate);
}
} // namespace TrainSim::Render
