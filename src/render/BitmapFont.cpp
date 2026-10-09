#include "trainsim/render/BitmapFont.hpp"
#include "trainsim/render/PixelGlyphs.hpp"

#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"

#include <algorithm>
#include <vector>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace TrainSim::Render {

BitmapFont::BitmapFont(GraphicsDevice& device) : texture_(device, 128, 48)
{
    // SpriteBatch expects premultiplied alpha. Empty texels need zero RGB as well as zero alpha;
    // transparent white leaks as solid glyph-cell rectangles on some OpenGL blend paths.
    constexpr std::size_t atlasWidth = 128U;
    constexpr std::size_t atlasHeight = 48U;
    std::vector<Color> pixels(atlasWidth * atlasHeight, Color::Transparent);
    for (int character = 32; character < 128; ++character)
    {
        const int glyph = character - 32;
        const int ox = (glyph % 16) * 8;
        const int oy = (glyph / 16) * 8;
        const PixelGlyphRows bits = pixelGlyphRows(static_cast<char>(character));
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 5; ++x)
                if ((bits[static_cast<std::size_t>(y)] & (0x10U >> x)) != 0)
                    pixels[static_cast<std::size_t>(oy + y) * atlasWidth
                           + static_cast<std::size_t>(ox + x)] = Color::White;
    }
    texture_.SetData(pixels.data(), static_cast<int>(pixels.size()));
}

void BitmapFont::draw(SpriteBatch& batch, std::string_view text, Vector2 position,
                      const Color& color, float scale) const
{
    float x = position.X;
    for (char character : text)
    {
        if (character == '\n') { x = position.X; position.Y += 9.0f * scale; continue; }
        const unsigned char code = static_cast<unsigned char>(character);
        const int glyph = std::clamp(static_cast<int>(code), 32, 127) - 32;
        const Rectangle source((glyph % 16) * 8, (glyph / 16) * 8, 5, 7);
        batch.Draw(texture_, Vector2{x, position.Y}, std::optional<Rectangle>(source), color, 0.0f,
                   Vector2::Zero, scale, SpriteEffects::None, 0.0f);
        x += 6.0f * scale;
    }
}

float BitmapFont::measure(std::string_view text, float scale) const noexcept
{
    return static_cast<float>(text.size()) * 6.0f * scale;
}

} // namespace TrainSim::Render
