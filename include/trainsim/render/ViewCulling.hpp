#pragma once
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include <array>
#include <cmath>

namespace TrainSim::Render {
// Conservative row-vector clip test. Deliberately omit near/far clipping: body parts crossing
// the near plane and unusual depth conventions must remain visible. No occlusion assumptions.
inline bool sphereMayBeInView(const Microsoft::Xna::Framework::Matrix& m,
    const Microsoft::Xna::Framework::Vector3& centre, float radius)
{
    const std::array<std::array<float,4>,5> planes{{
        {m.M14+m.M11,m.M24+m.M21,m.M34+m.M31,m.M44+m.M41},
        {m.M14-m.M11,m.M24-m.M21,m.M34-m.M31,m.M44-m.M41},
        {m.M14+m.M12,m.M24+m.M22,m.M34+m.M32,m.M44+m.M42},
        {m.M14-m.M12,m.M24-m.M22,m.M34-m.M32,m.M44-m.M42},
        {m.M14,m.M24,m.M34,m.M44}
    }};
    for (const auto& p : planes)
    {
        const float length = std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
        const float distance = p[0]*centre.X+p[1]*centre.Y+p[2]*centre.Z+p[3];
        if (distance < -radius*length) return false;
    }
    return true;
}
} // namespace TrainSim::Render
