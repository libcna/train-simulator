#pragma once

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarBone.hpp"
#include <span>

namespace TrainSim::Render {

// Apply once to a fresh animation pose, never to an already adjusted previous-frame pose.
// CNA's documented custom-bone path propagates local scale to face, hair and native headwear.
inline void applyPassengerProportions(std::span<Microsoft::Xna::Framework::Matrix> bones)
{
    using namespace Microsoft::Xna::Framework;
    const auto head=static_cast<std::size_t>(GamerServices::AvatarBone::Head);
    if (head >= bones.size()) return;
    bones[head]=Matrix::CreateScale(0.74f)*bones[head];
}

} // namespace TrainSim::Render
