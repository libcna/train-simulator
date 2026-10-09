#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <array>
#include <string_view>

namespace TrainSim::Render {

class BitmapFont
{
public:
    explicit BitmapFont(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);
    void draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch, std::string_view text,
              Microsoft::Xna::Framework::Vector2 position,
              const Microsoft::Xna::Framework::Color& color, float scale = 2.0f) const;
    [[nodiscard]] float measure(std::string_view text, float scale = 2.0f) const noexcept;

private:
    Microsoft::Xna::Framework::Graphics::Texture2D texture_;
};

} // namespace TrainSim::Render
