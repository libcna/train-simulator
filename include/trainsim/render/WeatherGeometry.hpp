#pragma once

#include "trainsim/render/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace TrainSim::Render {

// Rain (ENV-139). Streaks fill a periodic tile in train-local metres that the renderer repeats
// around the camera, shears along the relative wind and scrolls with time. The tile leaves the
// passenger corridor empty, so no rain ever falls inside a car.
inline constexpr float kRainTileLength = 30.0f;     // Along the train, Z.
inline constexpr float kRainTileHeight = 18.0f;     // Y, centred on the camera height.
inline constexpr float kRainTileHalfWidth = 26.0f;  // |X| reach.
inline constexpr float kRainCorridorHalfWidth = 2.40f; // A metre clear of the bodyside glass.

namespace weather_detail {
struct Random
{
    std::uint32_t state;
    float next()
    {
        state = state*1664525U+1013904223U;
        return static_cast<float>(state >> 8U)/16777216.0f;
    }
};
} // namespace weather_detail

// Two crossed thin vertical quads per streak, so streaks stay visible from every yaw. Streak
// tops start at y in [-H/2, H/2); the renderer offsets the tile to the camera.
// side is -1 or +1 for one half of the tile, or 0 for both sides.
inline int addRainStreaks(MeshBuilder& mesh,int count,std::uint32_t seed,float side = 0.0f)
{
    using namespace Microsoft::Xna::Framework;
    weather_detail::Random random{seed};
    for (int i = 0; i < count; ++i)
    {
        const float pick = random.next() < 0.5f ? -1.0f : 1.0f;
        const float x0 = side != 0.0f ? side : pick;
        // Denser near the windows, where individual streaks read best.
        const float reach = std::pow(random.next(),1.6f);
        const float x = x0*(kRainCorridorHalfWidth+0.05f
            + (kRainTileHalfWidth-kRainCorridorHalfWidth-0.05f)*reach);
        const float y = (random.next()-0.5f)*kRainTileHeight;
        const float z = random.next()*kRainTileLength;
        const float length = 0.55f+0.40f*random.next();
        const float halfWidth = 0.005f+0.003f*random.next();
        // Fainter close to the windows, where the relative-wind shear stretches streaks most.
        const auto alpha = static_cast<int>((34.0f+36.0f*random.next())*(0.55f+0.45f*reach));
        const Color top(196,206,214,0), bottom(196,206,214,alpha);
        const float cx = x;
        mesh.addQuad(Vector3{cx-halfWidth,y-length,z},Vector3{cx+halfWidth,y-length,z},
                     Vector3{cx+halfWidth,y,z},Vector3{cx-halfWidth,y,z},
                     bottom,bottom,top,top);
        mesh.addQuad(Vector3{cx,y-length,z-halfWidth},Vector3{cx,y-length,z+halfWidth},
                     Vector3{cx,y,z+halfWidth},Vector3{cx,y,z-halfWidth},
                     bottom,bottom,top,top);
    }
    return count;
}

// Rain on the outside of one passenger window pane at x (outward face), spanning [y0, y1] and
// [z0, z1]: round beads and a few short wind-driven trails, in translucent pale colours.
inline int addWindowDroplets(MeshBuilder& mesh,float x,float y0,float y1,float z0,float z1,
                             std::uint32_t seed)
{
    using namespace Microsoft::Xna::Framework;
    weather_detail::Random random{seed};
    const float area = std::max(0.0f,(y1-y0)*(z1-z0));
    const int beads = static_cast<int>(area*70.0f);
    int added = 0;
    for (int i = 0; i < beads; ++i)
    {
        const float y = y0+0.03f+(y1-y0-0.06f)*random.next();
        const float z = z0+0.03f+(z1-z0-0.06f)*random.next();
        const float r = 0.0020f+0.0035f*random.next()*random.next();
        const bool trail = random.next() < 0.15f;
        const float along = trail ? r*(2.5f+3.5f*random.next()) : r;
        const float drop = trail ? r*(0.8f+1.2f*random.next()) : 0.0f;
        const Color body(150,166,176,trail ? 30 : 40), lit(206,216,222,trail ? 30 : 56);
        // Beads are small diamonds; trails slant down and back along the pane.
        mesh.addQuad(Vector3{x,y-r-drop,z},Vector3{x,y,z-along},Vector3{x,y+r,z},
                     Vector3{x,y,z+r},body,body,lit,body);
        ++added;
    }
    return added;
}

} // namespace TrainSim::Render
