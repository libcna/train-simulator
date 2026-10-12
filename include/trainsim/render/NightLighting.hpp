#pragma once

#include "trainsim/render/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace TrainSim::Render {

// Night glow (ENV-138). These meshes are drawn additively, unlit, with an alpha that follows
// the atmosphere's night factor, so by day they cost nothing and change nothing on screen.

// Deterministic share of lit rooms: about two windows in five, in three lamp temperatures.
[[nodiscard]] inline bool nightWindowLit(std::uint32_t seed) noexcept
{
    seed ^= seed >> 16U;
    seed *= 0x7FEB352DU;
    seed ^= seed >> 15U;
    seed *= 0x846CA68BU;
    seed ^= seed >> 16U;
    return seed % 100U < 42U;
}
[[nodiscard]] inline Microsoft::Xna::Framework::Color nightWindowColour(std::uint32_t seed)
{
    using Microsoft::Xna::Framework::Color;
    switch ((seed >> 5U) % 3U)
    {
        case 0: return Color(236,178,104,255);
        case 1: return Color(214,196,150,255);
        default: return Color(188,186,170,255);
    }
}

// Radial pool of lamp light on a horizontal surface: bright centre, black (invisible when
// added) rim. Up-facing triangle fan.
inline void addLightPool(MeshBuilder& mesh,const Microsoft::Xna::Framework::Vector3& centre,
                         float radius,const Microsoft::Xna::Framework::Color& colour,
                         int segments = 12)
{
    using namespace Microsoft::Xna::Framework;
    constexpr float tau = 6.2831853071795864769f;
    const Color rim(0,0,0,255);
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = tau*static_cast<float>(i)/static_cast<float>(segments);
        const float a1 = tau*static_cast<float>(i+1)/static_cast<float>(segments);
        const Vector3 p0 = centre+Vector3{std::cos(a0)*radius,0.0f,std::sin(a0)*radius};
        const Vector3 p1 = centre+Vector3{std::cos(a1)*radius,0.0f,std::sin(a1)*radius};
        // Wound so the face normal points up (MeshBuilder emits base, base+2, base+1).
        mesh.addTexturedTriangle({centre,p1,p0},{colour,rim,rim},
                                 {Vector2{0.5f,0.5f},Vector2{0,0},Vector2{1,0}});
    }
}

// Lit passenger windows and lamps of a vehicle mesh: every triangle whose (face-shaded)
// colour matches a source colour is copied, lifted slightly off its face and recoloured.
struct NightGlowMatch
{
    Microsoft::Xna::Framework::Color source;
    Microsoft::Xna::Framework::Color glow;
};

namespace night_detail {
[[nodiscard]] inline bool shadedFrom(std::uint32_t packed,const Microsoft::Xna::Framework::Color& source)
{
    const auto channel = [packed](int shift) {
        return static_cast<float>((packed >> static_cast<unsigned>(shift)) & 0xFFU);
    };
    const float r = channel(0), g = channel(8), b = channel(16);
    const float sr = static_cast<float>(source.getRProperty());
    const float sg = static_cast<float>(source.getGProperty());
    const float sb = static_cast<float>(source.getBProperty());
    const float total = sr+sg+sb;
    if (total <= 0.0f) return r+g+b <= 0.0f;
    // MeshBuilder shades box faces by 0.62..1.08 and truncates each channel.
    const float k = (r+g+b)/total;
    if (k < 0.58f || k > 1.12f) return false;
    return std::abs(r-sr*k) <= 2.5f && std::abs(g-sg*k) <= 2.5f && std::abs(b-sb*k) <= 2.5f;
}
} // namespace night_detail

[[nodiscard]] inline MeshData nightGlowOverlay(const MeshData& source,
                                              std::span<const NightGlowMatch> matches,
                                              float lift = 0.006f)
{
    MeshData overlay;
    for (std::size_t i = 0; i+2 < source.indices.size(); i += 3)
    {
        const LitColorVertex& first = source.vertices[source.indices[i]];
        const NightGlowMatch* match = nullptr;
        for (const NightGlowMatch& candidate : matches)
            if (night_detail::shadedFrom(first.color,candidate.source)) { match = &candidate; break; }
        if (match == nullptr) continue;
        const auto base = static_cast<std::uint32_t>(overlay.vertices.size());
        for (std::size_t corner = 0; corner < 3; ++corner)
        {
            LitColorVertex vertex = source.vertices[source.indices[i+corner]];
            vertex.position = vertex.position+vertex.normal*lift;
            vertex.color = match->glow.getPackedValueProperty();
            overlay.vertices.push_back(vertex);
        }
        overlay.indices.insert(overlay.indices.end(),{base,base+1,base+2});
    }
    return overlay;
}

} // namespace TrainSim::Render
