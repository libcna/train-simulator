#include "TestHarness.hpp"

#include "trainsim/render/PixelGlyphs.hpp"

using TrainSim::Render::PixelGlyphRows;
using TrainSim::Render::pixelGlyphRows;

TEST_CASE("bitmap glyph atlas contains announcement punctuation")
{
    const PixelGlyphRows expectedComma = {0,0,0,0,0,4,8};
    REQUIRE(pixelGlyphRows(',') == expectedComma);
    REQUIRE(pixelGlyphRows(',') != pixelGlyphRows('?'));
    REQUIRE(pixelGlyphRows('.') != pixelGlyphRows(','));
}
