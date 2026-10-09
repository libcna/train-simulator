#pragma once

#include "trainsim/render/AvatarMotion.hpp"

namespace TrainSim::Render {

// Rank untouched public CreateRandom results; never rewrite or construct description bytes.
inline int civilianAppearanceScoreBytes(std::span<const std::uint8_t> bytes)
{
    if (bytes.size()!=1021 || (bytes[0]!=1 && bytes[0]!=2)
        || bytes[1]!='C' || bytes[2]!='N' || bytes[3]!='A'
        || bytes[8]!=3 || bytes[9]!=0) return 0;
    const auto chroma=[&](std::size_t offset) {
        const auto [low,high]=std::minmax({bytes[offset],bytes[offset+1],bytes[offset+2]});
        return static_cast<int>(high)-static_cast<int>(low);
    };
    const auto maximum=[&](std::size_t offset) {
        return static_cast<int>(std::max({bytes[offset],bytes[offset+1],bytes[offset+2]}));
    };
    const unsigned hat=bytes[41] | (static_cast<unsigned>(bytes[42])<<8U);
    const bool restrainedAccessory=chroma(28)<100 || maximum(28)<145;
    // Prefer ordinary uncovered hair. Retain subdued caps/beanies/etc. as alternatives, but
    // do not let a bright hat or broad bucket silhouette win through unrelated clothing points.
    int score=hat==0 ? 12 : hat==102 || !restrainedAccessory ? -40 : 4;
    const bool naturalHair=!(bytes[15]>bytes[13]+24 && bytes[15]>bytes[14]+24);
    score += naturalHair ? 4 : -4;
    score += restrainedAccessory ? 4 : -5;
    // Wardrobe must dominate accessory/hairstyle points: an uncovered head must not make
    // fluorescent trousers or a saturated primary-colour top an early-accept candidate.
    // Cream, charcoal, earth tones, teal and muted purple remain available in catalog v3.
    score += chroma(22)<95 || maximum(22)<145 ? 6 : -20;
    score += chroma(25)<80 || maximum(25)<135 ? 3 : -2;
    score += chroma(19)<100 || maximum(19)<145 ? 8 : -24;
    if (avatarHasSeatedSkirtRisk(bytes)) score-=100;
    return score;
}

} // namespace TrainSim::Render
