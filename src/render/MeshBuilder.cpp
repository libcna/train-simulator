#include "trainsim/render/MeshBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace TrainSim::Render {

void MeshBuilder::projectXYDetailUVs(std::size_t firstVertex,const Vector3& origin,float metresPerTile)
{
    if (metresPerTile<=0) return;
    for (std::size_t i=firstVertex;i<data_.vertices.size();++i)
    {
        auto& vertex=data_.vertices[i];
        const auto p=(vertex.position-origin)/metresPerTile;
        vertex.textureCoordinate={p.X,p.Y};
    }
}

void MeshBuilder::projectDetailUVs(std::size_t firstVertex,const Vector3& origin,float metresPerTile)
{
    if (metresPerTile<=0) return;
    for (std::size_t i=firstVertex;i<data_.vertices.size();++i)
    {
        auto& vertex=data_.vertices[i];
        const auto p=(vertex.position-origin)/metresPerTile;
        const auto& n=vertex.normal;
        if (std::abs(n.Y)>=std::max(std::abs(n.X),std::abs(n.Z)))
            vertex.textureCoordinate={p.X,p.Z};
        else if (std::abs(n.X)>std::abs(n.Z)) vertex.textureCoordinate={p.Z,p.Y};
        else vertex.textureCoordinate={p.X,p.Y};
    }
}

namespace {
Color Shade(const Color& color, float factor)
{
    return Color(static_cast<int>(static_cast<float>(color.getRProperty()) * factor),
                 static_cast<int>(static_cast<float>(color.getGProperty()) * factor),
                 static_cast<int>(static_cast<float>(color.getBProperty()) * factor),
                 static_cast<int>(color.getAProperty()));
}

Vector3 faceNormal(const Vector3& a, const Vector3& b, const Vector3& c)
{
    // Clockwise front faces have the opposite cross product to a right-handed indexed
    // triangle. Lighting normals must point OUT of the visible surface, not into it.
    Vector3 normal = Vector3::Cross(b - a, c - a);
    if (normal.LengthSquared() < 1.0e-10f) return Vector3::Up;
    normal.Normalize();
    return normal;
}

LitColorVertex vertex(const Vector3& position, const Vector3& normal, const Color& color,
                      const Vector2& textureCoordinate = Vector2::Zero)
{
    return LitColorVertex{position, normal, color.getPackedValueProperty(), textureCoordinate};
}

}

void MeshBuilder::addQuad(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d,
                          const Color& color, const Vector3& na, const Vector3& nb,
                          const Vector3& nc, const Vector3& nd)
{
    const std::uint32_t base = static_cast<std::uint32_t>(data_.vertices.size());
    const auto unit = [&](Vector3 normal) {
        if (normal.LengthSquared() < 1e-10f) return faceNormal(a,b,c);
        normal.Normalize();
        return normal;
    };
    data_.vertices.push_back(vertex(a,unit(na),color));
    data_.vertices.push_back(vertex(b,unit(nb),color));
    data_.vertices.push_back(vertex(c,unit(nc),color));
    data_.vertices.push_back(vertex(d,unit(nd),color));
    data_.indices.insert(data_.indices.end(), {base,base+2,base+1,base,base+3,base+2});
}

void MeshBuilder::addQuad(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d,
                          const Color& color)
{
    addQuad(a, b, c, d, color, color, color, color);
}

void MeshBuilder::addQuad(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d,
                          const Color& colorA, const Color& colorB,
                          const Color& colorC, const Color& colorD)
{
    const std::uint32_t base = static_cast<std::uint32_t>(data_.vertices.size());
    const Vector3 normal = faceNormal(a, b, c);
    data_.vertices.push_back(vertex(a, normal, colorA));
    data_.vertices.push_back(vertex(b, normal, colorB));
    data_.vertices.push_back(vertex(c, normal, colorC));
    data_.vertices.push_back(vertex(d, normal, colorD));
    // CNA follows XNA's clockwise-front convention.
    data_.indices.insert(data_.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
}

void MeshBuilder::addTexturedQuad(const Vector3& a, const Vector3& b,
                                  const Vector3& c, const Vector3& d,
                                  const Color& color,
                                  const Vector2& uvA, const Vector2& uvB,
                                  const Vector2& uvC, const Vector2& uvD)
{
    const std::uint32_t base = static_cast<std::uint32_t>(data_.vertices.size());
    const Vector3 normal = faceNormal(a,b,c);
    data_.vertices.push_back(vertex(a,normal,color,uvA));
    data_.vertices.push_back(vertex(b,normal,color,uvB));
    data_.vertices.push_back(vertex(c,normal,color,uvC));
    data_.vertices.push_back(vertex(d,normal,color,uvD));
    data_.indices.insert(data_.indices.end(), {base,base+2,base+1,base,base+3,base+2});
}

void MeshBuilder::addTexturedQuad(const Vector3& a, const Vector3& b,
                                  const Vector3& c, const Vector3& d,
                                  const Color& colorA, const Color& colorB,
                                  const Color& colorC, const Color& colorD,
                                  const Vector2& uvA, const Vector2& uvB,
                                  const Vector2& uvC, const Vector2& uvD)
{
    const std::uint32_t base = static_cast<std::uint32_t>(data_.vertices.size());
    const Vector3 normal = faceNormal(a,b,c);
    data_.vertices.push_back(vertex(a,normal,colorA,uvA));
    data_.vertices.push_back(vertex(b,normal,colorB,uvB));
    data_.vertices.push_back(vertex(c,normal,colorC,uvC));
    data_.vertices.push_back(vertex(d,normal,colorD,uvD));
    data_.indices.insert(data_.indices.end(), {base,base+2,base+1,base,base+3,base+2});
}

void MeshBuilder::addSmoothTexturedQuad(const std::array<Vector3,4>& positions,
    const std::array<Vector3,4>& normals, const std::array<Vector2,4>& uvs, const Color& color)
{
    const auto base = data_.vertices.size();
    addQuad(positions[0],positions[1],positions[2],positions[3],color,
            normals[0],normals[1],normals[2],normals[3]);
    for (std::size_t i = 0; i < 4; ++i) data_.vertices[base+i].textureCoordinate = uvs[i];
}

void MeshBuilder::addTriangle(const Vector3& a, const Vector3& b, const Vector3& c, const Color& color)
{
    const std::uint32_t base = static_cast<std::uint32_t>(data_.vertices.size());
    const Vector3 normal = faceNormal(a, b, c);
    data_.vertices.push_back(vertex(a, normal, color));
    data_.vertices.push_back(vertex(b, normal, color));
    data_.vertices.push_back(vertex(c, normal, color));
    data_.indices.insert(data_.indices.end(), {base, base + 2, base + 1});
}

void MeshBuilder::addTexturedTriangle(const std::array<Vector3,3>& positions,
    const std::array<Color,3>& colors,const std::array<Vector2,3>& uvs)
{
    const auto base=static_cast<std::uint32_t>(data_.vertices.size());
    const Vector3 normal=faceNormal(positions[0],positions[1],positions[2]);
    for (std::size_t i=0;i<3;++i)
        data_.vertices.push_back(vertex(positions[i],normal,colors[i],uvs[i]));
    data_.indices.insert(data_.indices.end(),{base,base+2,base+1});
}

void MeshBuilder::addBox(const Vector3& minimum, const Vector3& maximum, const Color& color)
{
    addBox(minimum, maximum, color, Matrix::getIdentityProperty());
}

void MeshBuilder::addBox(const Vector3& minimum, const Vector3& maximum, const Color& color,
                         const Matrix& transform)
{
    const float x0 = minimum.X, y0 = minimum.Y, z0 = minimum.Z;
    const float x1 = maximum.X, y1 = maximum.Y, z1 = maximum.Z;
    const Vector3 a = Vector3::Transform(Vector3{x0,y0,z0}, transform);
    const Vector3 b = Vector3::Transform(Vector3{x1,y0,z0}, transform);
    const Vector3 c = Vector3::Transform(Vector3{x1,y1,z0}, transform);
    const Vector3 d = Vector3::Transform(Vector3{x0,y1,z0}, transform);
    const Vector3 e = Vector3::Transform(Vector3{x0,y0,z1}, transform);
    const Vector3 f = Vector3::Transform(Vector3{x1,y0,z1}, transform);
    const Vector3 g = Vector3::Transform(Vector3{x1,y1,z1}, transform);
    const Vector3 h = Vector3::Transform(Vector3{x0,y1,z1}, transform);
    addQuad(b, a, d, c, Shade(color, 0.82f));
    addQuad(e, f, g, h, color);
    addQuad(a, e, h, d, Shade(color, 0.90f));
    addQuad(f, b, c, g, Shade(color, 0.74f));
    addQuad(d, h, g, c, Shade(color, 1.08f));
    addQuad(a, b, f, e, Shade(color, 0.62f));
}

void MeshBuilder::addRoundedBoxXZ(const Vector3& minimum, const Vector3& maximum,
                                  float cornerRadius, int cornerSegments,
                                  const Color& color)
{
    const float halfX = std::max(0.0f,(maximum.X-minimum.X)*0.5f);
    const float halfZ = std::max(0.0f,(maximum.Z-minimum.Z)*0.5f);
    const float radius = std::clamp(std::abs(cornerRadius),0.0f,
                                    std::max(0.0f,std::min(halfX,halfZ)-0.0001f));
    if (radius < 0.0001f || maximum.Y <= minimum.Y)
    {
        addBox(minimum,maximum,color);
        return;
    }

    cornerSegments = std::max(cornerSegments,1);
    const float centreX = (minimum.X+maximum.X)*0.5f;
    const float centreZ = (minimum.Z+maximum.Z)*0.5f;
    const float innerHalfX = halfX-radius;
    const float innerHalfZ = halfZ-radius;
    constexpr float halfPi = 1.5707963267948966192f;
    std::vector<Vector3> bottom;
    bottom.reserve((static_cast<std::size_t>(cornerSegments)+1U)*4U);
    for (int corner = 0; corner < 4; ++corner)
    {
        const float cornerX = centreX + (corner == 0 || corner == 3 ? innerHalfX : -innerHalfX);
        const float cornerZ = centreZ + (corner < 2 ? innerHalfZ : -innerHalfZ);
        // Retain both tangent points so the intervening straight side is axis-aligned.
        for (int segment = 0; segment <= cornerSegments; ++segment)
        {
            const float angle = static_cast<float>(corner)*halfPi
                              + static_cast<float>(segment)*halfPi
                                / static_cast<float>(cornerSegments);
            bottom.push_back(Vector3{cornerX+std::cos(angle)*radius,minimum.Y,
                                     cornerZ+std::sin(angle)*radius});
        }
    }

    const Vector3 bottomCentre{centreX,minimum.Y,centreZ};
    const Vector3 topCentre{centreX,maximum.Y,centreZ};
    for (std::size_t index = 0; index < bottom.size(); ++index)
    {
        const std::size_t next = (index+1U)%bottom.size();
        const Vector3& lower0 = bottom[index];
        const Vector3& lower1 = bottom[next];
        const Vector3 upper0{lower0.X,maximum.Y,lower0.Z};
        const Vector3 upper1{lower1.X,maximum.Y,lower1.Z};
        // XZ reverses handedness relative to XY/YZ. Preserve addBox's exterior culling
        // convention on both caps and walls instead of exposing the inside of this solid.
        addTriangle(topCentre,upper1,upper0,Shade(color,1.08f));
        addTriangle(bottomCentre,lower0,lower1,Shade(color,0.62f));
        const float angle = std::atan2(
            (lower0.Z+lower1.Z)*0.5f-centreZ,
            (lower0.X+lower1.X)*0.5f-centreX);
        addQuad(upper0,upper1,lower1,lower0,
                Shade(color,0.80f+0.16f*std::cos(angle)));
    }
}

void MeshBuilder::addRoundedBoxXY(const Vector3& minimum, const Vector3& maximum,
                                  float cornerRadius, int cornerSegments,
                                  const Color& color)
{
    const float halfX = std::max(0.0f,(maximum.X-minimum.X)*0.5f);
    const float halfY = std::max(0.0f,(maximum.Y-minimum.Y)*0.5f);
    const float radius = std::clamp(std::abs(cornerRadius),0.0f,
                                    std::max(0.0f,std::min(halfX,halfY)-0.0001f));
    if (radius < 0.0001f || maximum.Z <= minimum.Z)
    {
        addBox(minimum,maximum,color);
        return;
    }

    cornerSegments = std::max(cornerSegments,1);
    const float centreX = (minimum.X+maximum.X)*0.5f;
    const float centreY = (minimum.Y+maximum.Y)*0.5f;
    const float innerHalfX = halfX-radius;
    const float innerHalfY = halfY-radius;
    constexpr float halfPi = 1.5707963267948966192f;
    std::vector<Vector3> front;
    front.reserve((static_cast<std::size_t>(cornerSegments)+1U)*4U);
    for (int corner = 0; corner < 4; ++corner)
    {
        const float cornerX = centreX + (corner == 0 || corner == 3 ? innerHalfX : -innerHalfX);
        const float cornerY = centreY + (corner < 2 ? innerHalfY : -innerHalfY);
        for (int segment = 0; segment <= cornerSegments; ++segment)
        {
            const float angle = static_cast<float>(corner)*halfPi
                              + static_cast<float>(segment)*halfPi
                                / static_cast<float>(cornerSegments);
            front.push_back(Vector3{cornerX+std::cos(angle)*radius,
                                    cornerY+std::sin(angle)*radius,minimum.Z});
        }
    }

    const Vector3 frontCentre{centreX,centreY,minimum.Z};
    const Vector3 backCentre{centreX,centreY,maximum.Z};
    for (std::size_t index = 0; index < front.size(); ++index)
    {
        const std::size_t next = (index+1U)%front.size();
        const Vector3& front0 = front[index];
        const Vector3& front1 = front[next];
        const Vector3 back0{front0.X,front0.Y,maximum.Z};
        const Vector3 back1{front1.X,front1.Y,maximum.Z};
        addTriangle(frontCentre,front1,front0,Shade(color,0.78f));
        addTriangle(backCentre,back0,back1,Shade(color,1.02f));
        const float angle = std::atan2(
            (front0.Y+front1.Y)*0.5f-centreY,
            (front0.X+front1.X)*0.5f-centreX);
        addQuad(front0,front1,back1,back0,
                Shade(color,0.82f+0.16f*std::cos(angle)));
    }
}

void MeshBuilder::addRoundedBoxYZ(const Vector3& minimum, const Vector3& maximum,
                                  float cornerRadius, int cornerSegments,
                                  const Color& color)
{
    const float halfY = std::max(0.0f,(maximum.Y-minimum.Y)*0.5f);
    const float halfZ = std::max(0.0f,(maximum.Z-minimum.Z)*0.5f);
    const float radius = std::clamp(std::abs(cornerRadius),0.0f,
                                    std::max(0.0f,std::min(halfY,halfZ)-0.0001f));
    if (radius < 0.0001f || maximum.X <= minimum.X)
    {
        addBox(minimum,maximum,color);
        return;
    }

    cornerSegments = std::max(cornerSegments,1);
    const float centreY = (minimum.Y+maximum.Y)*0.5f;
    const float centreZ = (minimum.Z+maximum.Z)*0.5f;
    const float innerHalfY = halfY-radius;
    const float innerHalfZ = halfZ-radius;
    constexpr float halfPi = 1.5707963267948966192f;
    std::vector<Vector3> left;
    left.reserve((static_cast<std::size_t>(cornerSegments)+1U)*4U);
    for (int corner = 0; corner < 4; ++corner)
    {
        const float cornerY = centreY + (corner == 0 || corner == 3 ? innerHalfY : -innerHalfY);
        const float cornerZ = centreZ + (corner < 2 ? innerHalfZ : -innerHalfZ);
        for (int segment = 0; segment <= cornerSegments; ++segment)
        {
            const float angle = static_cast<float>(corner)*halfPi
                              + static_cast<float>(segment)*halfPi
                                / static_cast<float>(cornerSegments);
            left.push_back(Vector3{minimum.X,cornerY+std::cos(angle)*radius,
                                   cornerZ+std::sin(angle)*radius});
        }
    }

    const Vector3 leftCentre{minimum.X,centreY,centreZ};
    const Vector3 rightCentre{maximum.X,centreY,centreZ};
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        const std::size_t next = (index+1U)%left.size();
        const Vector3& left0 = left[index];
        const Vector3& left1 = left[next];
        const Vector3 right0{maximum.X,left0.Y,left0.Z};
        const Vector3 right1{maximum.X,left1.Y,left1.Z};
        addTriangle(leftCentre,left1,left0,Shade(color,0.78f));
        addTriangle(rightCentre,right0,right1,Shade(color,1.02f));
        const float angle = std::atan2(
            (left0.Z+left1.Z)*0.5f-centreZ,
            (left0.Y+left1.Y)*0.5f-centreY);
        addQuad(left0,left1,right1,right0,
                Shade(color,0.82f+0.16f*std::cos(angle)));
    }
}

void MeshBuilder::addCylinder(const Vector3& centre, float radius, float height, int sides, const Color& color)
{
    sides = std::max(sides, 3);
    constexpr float tau = 6.2831853071795864769f;
    for (int side = 0; side < sides; ++side)
    {
        const float a0 = tau * static_cast<float>(side) / static_cast<float>(sides);
        const float a1 = tau * static_cast<float>(side + 1) / static_cast<float>(sides);
        const Vector3 p0{centre.X + std::cos(a0) * radius, centre.Y, centre.Z + std::sin(a0) * radius};
        const Vector3 p1{centre.X + std::cos(a1) * radius, centre.Y, centre.Z + std::sin(a1) * radius};
        const Vector3 q0{p0.X, centre.Y + height, p0.Z};
        const Vector3 q1{p1.X, centre.Y + height, p1.Z};
        addQuad(q0, q1, p1, p0, Shade(color, 0.78f + 0.22f * std::cos(a0)));
        addTriangle(Vector3{centre.X, centre.Y + height, centre.Z}, q1, q0, color);
    }
}

void MeshBuilder::addCone(const Vector3& baseCentre, float radius, float height, int sides,
                          const Color& color)
{
    sides = std::max(sides, 3);
    constexpr float tau = 6.2831853071795864769f;
    const Vector3 apex{baseCentre.X, baseCentre.Y + height, baseCentre.Z};
    for (int side = 0; side < sides; ++side)
    {
        const float a0 = tau * static_cast<float>(side) / static_cast<float>(sides);
        const float a1 = tau * static_cast<float>(side + 1) / static_cast<float>(sides);
        const Vector3 p0{baseCentre.X + std::cos(a0) * radius, baseCentre.Y,
                         baseCentre.Z + std::sin(a0) * radius};
        const Vector3 p1{baseCentre.X + std::cos(a1) * radius, baseCentre.Y,
                         baseCentre.Z + std::sin(a1) * radius};
        addTriangle(p1, p0, apex, Shade(color, 0.78f + 0.22f * std::cos(a0)));
        addTriangle(baseCentre, p0, p1, Shade(color, 0.68f));
    }
}

void MeshBuilder::addEllipsoid(const Vector3& centre, const Vector3& radii,
                               int slices, int stacks, const Color& color, bool smoothNormals)
{
    const std::size_t firstVertex = data_.vertices.size();
    slices = std::max(slices, 3);
    stacks = std::max(stacks, 2);
    constexpr float pi = 3.14159265358979323846f;
    constexpr float tau = 2.0f * pi;
    const Vector3 safeRadii{
        std::max(std::abs(radii.X), 0.001f),
        std::max(std::abs(radii.Y), 0.001f),
        std::max(std::abs(radii.Z), 0.001f)};
    // Most scene primitives have 6–16 slices. Cache each longitude once on the stack rather
    // than repeating four trig calls at every face corner; keep a general large-mesh fallback.
    std::array<Vector2,65> localLongitudes;
    std::vector<Vector2> largeLongitudes;
    Vector2* longitudes = localLongitudes.data();
    if (slices > 64)
    {
        largeLongitudes.resize(static_cast<std::size_t>(slices)+1);
        longitudes = largeLongitudes.data();
    }
    for (int slice = 0; slice <= slices; ++slice)
    {
        const float angle = tau*static_cast<float>(slice)/static_cast<float>(slices);
        longitudes[slice] = {std::cos(angle),std::sin(angle)};
    }
    const auto point = [&centre,&safeRadii,longitudes](const Vector2& latitude, int slice) {
        return Vector3{
            centre.X + safeRadii.X * latitude.X * longitudes[slice].X,
            centre.Y + safeRadii.Y * latitude.Y,
            centre.Z + safeRadii.Z * latitude.X * longitudes[slice].Y};
    };

    for (int stack = 0; stack < stacks; ++stack)
    {
        const float latitude0 = -pi * 0.5f
            + pi * static_cast<float>(stack) / static_cast<float>(stacks);
        const float latitude1 = -pi * 0.5f
            + pi * static_cast<float>(stack + 1) / static_cast<float>(stacks);
        const Vector2 ring0{std::cos(latitude0),std::sin(latitude0)};
        const Vector2 ring1{std::cos(latitude1),std::sin(latitude1)};
        for (int slice = 0; slice < slices; ++slice)
        {
            const Color faceColor = smoothNormals ? color
                : Shade(color,0.80f + 0.20f * longitudes[slice].X);
            if (stack == 0)
            {
                const Vector3 bottom{centre.X,centre.Y-safeRadii.Y,centre.Z};
                addTriangle(bottom,point(ring1,slice),point(ring1,slice+1),
                            faceColor);
            }
            else if (stack + 1 == stacks)
            {
                const Vector3 top{centre.X,centre.Y+safeRadii.Y,centre.Z};
                addTriangle(point(ring0,slice+1),point(ring0,slice),top,
                            faceColor);
            }
            else
            {
                addQuad(point(ring1,slice),point(ring1,slice+1),
                        point(ring0,slice+1),point(ring0,slice),faceColor);
            }
        }
    }
    if (smoothNormals)
        for (std::size_t index = firstVertex; index < data_.vertices.size(); ++index)
        {
            auto& v = data_.vertices[index];
            const Vector3 offset = v.position-centre;
            v.normal = Vector3{offset.X/(safeRadii.X*safeRadii.X),
                               offset.Y/(safeRadii.Y*safeRadii.Y),
                               offset.Z/(safeRadii.Z*safeRadii.Z)};
            v.normal.Normalize();
        }
}

void MeshBuilder::addFoliageLobe(const Vector3& centre, const Vector3& radii,
                                int slices, int stacks, const Color& color, bool smooth)
{
    const auto first = data_.vertices.size();
    addEllipsoid(centre,radii,slices,stacks,color,smooth);
    if (!smooth) return;
    // Continuous object-local colour variation avoids face-by-face bands and remains stable
    // when the route origin rebases. Darker undersides approximate canopy self-occlusion.
    for (auto i = first; i < data_.vertices.size(); ++i)
    {
        auto& v = data_.vertices[i];
        const auto p = v.position-centre;
        const float x = p.X/std::max(std::abs(radii.X),0.001f);
        const float y = p.Y/std::max(std::abs(radii.Y),0.001f);
        const float z = p.Z/std::max(std::abs(radii.Z),0.001f);
        // A bounded radial deformation replaces the perfectly smooth ellipsoid silhouette.
        // Its analytic tangent gradient keeps lighting consistent with the deformed surface;
        // simply retaining ellipsoid normals would conceal the new dents under smooth shading.
        const float a=4.0f*x+3.0f*z, b=5.0f*y-2.0f*z;
        const float sinA=std::sin(a), sinB=std::sin(b);
        const float cosA=std::cos(a), cosB=std::cos(b);
        const float radiusFactor=0.92f+0.08f*sinA*sinB;
        const Vector3 direction{x,y,z};
        const Vector3 gradient{0.32f*cosA*sinB,0.40f*sinA*cosB,
                               0.24f*cosA*sinB-0.16f*sinA*cosB};
        const Vector3 tangent=gradient-direction*Vector3::Dot(gradient,direction);
        const Vector3 radialNormal=direction-tangent/radiusFactor;
        v.position=centre+p*radiusFactor;
        v.normal={radialNormal.X/std::max(std::abs(radii.X),0.001f),
                  radialNormal.Y/std::max(std::abs(radii.Y),0.001f),
                  radialNormal.Z/std::max(std::abs(radii.Z),0.001f)};
        v.normal.Normalize();
        const float shade = 0.90f+0.08f*y+0.045f*std::sin(x*4.1f+z*3.7f+y*2.3f);
        v.color = Shade(color,shade).getPackedValueProperty();
        v.textureCoordinate.Y = (p.Y+radii.Y)/1.6f;
    }
    // Assign longitudes from the generating topology, not rounded world-space atan2 values.
    // Each pole belongs to one cap triangle and receives its midpoint longitude. Integer
    // repeats close the texture seam exactly, including after floating-origin rebasing.
    slices=std::max(slices,3);
    stacks=std::max(stacks,2);
    const float repeats=std::max(1.0f,std::round(6.283185307f
        *std::max(std::abs(radii.X),std::abs(radii.Z))/1.6f));
    auto cursor=first;
    for (int stack=0;stack<stacks;++stack)
        for (int slice=0;slice<slices;++slice)
    {
        const float u0=repeats*static_cast<float>(slice)/static_cast<float>(slices);
        const float u1=repeats*static_cast<float>(slice+1)/static_cast<float>(slices);
        const auto write=[&](float u) { data_.vertices[cursor++].textureCoordinate.X=u; };
        if (stack==0) { write((u0+u1)*0.5f); write(u0); write(u1); }
        else if (stack+1==stacks) { write(u1); write(u0); write((u0+u1)*0.5f); }
        else
        {
            write(u0); write(u1); write(u1); write(u0);
        }
    }
}

void MeshBuilder::addDiscXY(const Vector3& centre, float radius, int sides, const Color& color)
{
    sides = std::max(sides, 3);
    constexpr float tau = 6.2831853071795864769f;
    for (int side = 0; side < sides; ++side)
    {
        const float a0 = tau * static_cast<float>(side) / static_cast<float>(sides);
        const float a1 = tau * static_cast<float>(side + 1) / static_cast<float>(sides);
        const Vector3 p0{centre.X + std::cos(a0) * radius,
                         centre.Y + std::sin(a0) * radius, centre.Z};
        const Vector3 p1{centre.X + std::cos(a1) * radius,
                         centre.Y + std::sin(a1) * radius, centre.Z};
        addTriangle(centre, p1, p0, color);
        addTriangle(centre, p0, p1, color);
    }
}

void MeshBuilder::addDiscXZ(const Vector3& centre, float radius, int sides, const Color& color)
{
    sides = std::max(sides, 3);
    constexpr float tau = 6.2831853071795864769f;
    for (int side = 0; side < sides; ++side)
    {
        const float a0 = tau * static_cast<float>(side) / static_cast<float>(sides);
        const float a1 = tau * static_cast<float>(side + 1) / static_cast<float>(sides);
        const Vector3 p0{centre.X + std::cos(a0) * radius, centre.Y,
                         centre.Z + std::sin(a0) * radius};
        const Vector3 p1{centre.X + std::cos(a1) * radius, centre.Y,
                         centre.Z + std::sin(a1) * radius};
        addTriangle(centre, p1, p0, color);
    }
}

void MeshBuilder::append(const MeshData& other)
{
    const auto base=static_cast<std::uint32_t>(data_.vertices.size());
    data_.vertices.insert(data_.vertices.end(),other.vertices.begin(),other.vertices.end());
    data_.indices.reserve(data_.indices.size()+other.indices.size());
    for (const auto index : other.indices) data_.indices.push_back(base+index);
}

std::vector<MeshSegment> meshSegments(const MeshData& data,std::size_t maxVertices)
{
    if (data.empty()) return {};
    if (maxVertices<3) throw std::invalid_argument("A mesh segment must hold at least one triangle");
    if (data.vertices.size()<=maxVertices)
        return {MeshSegment{0,data.vertices.size(),0,data.indices.size()}};
    std::vector<MeshSegment> segments;
    std::size_t low=0,high=0,firstIndex=0;
    bool open=false;
    for (std::size_t i=0;i+2<data.indices.size();i+=3)
    {
        const std::size_t a=data.indices[i],b=data.indices[i+1],c=data.indices[i+2];
        const std::size_t triangleLow=std::min({a,b,c}),triangleHigh=std::max({a,b,c});
        if (triangleHigh-triangleLow+1>maxVertices)
            throw std::logic_error("A single triangle spans more vertices than one mesh segment");
        if (open && std::max(high,triangleHigh)-std::min(low,triangleLow)+1>maxVertices)
        {
            segments.push_back({low,high-low+1,firstIndex,i-firstIndex});
            open=false;
        }
        if (!open)
        {
            low=triangleLow; high=triangleHigh; firstIndex=i; open=true;
        }
        else
        {
            low=std::min(low,triangleLow); high=std::max(high,triangleHigh);
        }
    }
    if (open) segments.push_back({low,high-low+1,firstIndex,data.indices.size()-firstIndex});
    return segments;
}

} // namespace TrainSim::Render
