#include <Engine/Render/MeshPrimitives.h>

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    MeshVertex MakeVertex(const XMFLOAT3& position, const XMFLOAT3& normal, float u, float v)
    {
        MeshVertex vertex;
        vertex.Position = XMFLOAT4(position.x, position.y, position.z, 1.0f);
        vertex.Normal = XMFLOAT4(normal.x, normal.y, normal.z, 0.0f);
        vertex.TexCoord = XMFLOAT4(u, v, 0.0f, 0.0f);
        return vertex;
    }

    /**
     * Quad a-b-c-d listed clockwise as seen from its front: two clockwise triangles. With the
     * left-handed convention of the engine, clockwise means cross(b - a, c - a) points to the front.
     */
    void AddQuad(MeshLodData& mesh, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
    {
        mesh.Indices.insert(mesh.Indices.end(), {a, b, c, a, c, d});
    }
}

namespace MeshPrimitives
{
    MeshLodData Sphere(float radius, int slices, int stacks)
    {
        slices = std::max(slices, 3);
        stacks = std::max(stacks, 2);
        constexpr float Pi = 3.14159265358979f;

        MeshLodData mesh;
        // A seam column (slice == slices) and per-pole rings keep the UVs continuous.
        for (int stack = 0; stack <= stacks; ++stack)
        {
            const float v = static_cast<float>(stack) / static_cast<float>(stacks);
            const float polar = v * Pi;
            for (int slice = 0; slice <= slices; ++slice)
            {
                const float u = static_cast<float>(slice) / static_cast<float>(slices);
                const float azimuth = u * 2.0f * Pi;
                const XMFLOAT3 normal(std::sin(polar) * std::cos(azimuth), std::cos(polar), std::sin(polar) * std::sin(azimuth));
                const XMFLOAT3 position(normal.x * radius, normal.y * radius, normal.z * radius);
                mesh.Vertices.push_back(MakeVertex(position, normal, u, v));
            }
        }

        const uint32_t row = static_cast<uint32_t>(slices + 1);
        for (uint32_t stack = 0; stack < static_cast<uint32_t>(stacks); ++stack)
        {
            for (uint32_t slice = 0; slice < static_cast<uint32_t>(slices); ++slice)
            {
                const uint32_t top = stack * row + slice;
                const uint32_t bottom = top + row;
                // Seen from outside the azimuth grows to the right and the stack index downwards.
                AddQuad(mesh, top, top + 1, bottom + 1, bottom);
            }
        }

        // Pole quads collapse into one triangle each: drop the degenerate halves.
        std::vector<uint32_t> indices;
        indices.reserve(mesh.Indices.size());
        for (size_t i = 0; i < mesh.Indices.size(); i += 3)
        {
            const XMFLOAT4& a = mesh.Vertices[mesh.Indices[i]].Position;
            const XMFLOAT4& b = mesh.Vertices[mesh.Indices[i + 1]].Position;
            const XMFLOAT4& c = mesh.Vertices[mesh.Indices[i + 2]].Position;
            const XMVECTOR cross = XMVector3Cross(XMVectorSubtract(XMLoadFloat4(&b), XMLoadFloat4(&a)),
                                                  XMVectorSubtract(XMLoadFloat4(&c), XMLoadFloat4(&a)));
            if (XMVectorGetX(XMVector3LengthSq(cross)) > radius * radius * 1e-10f)
            {
                indices.insert(indices.end(), mesh.Indices.begin() + i, mesh.Indices.begin() + i + 3);
            }
        }
        mesh.Indices = std::move(indices);
        return mesh;
    }

    MeshLodData Cube(float size)
    {
        const float h = size * 0.5f;
        struct Face
        {
            XMFLOAT3 Normal;
            XMFLOAT3 Right; // +u direction
            XMFLOAT3 Up;    // -v direction
        };
        const Face faces[6] = {
            {{ 1, 0, 0}, { 0, 0, 1}, {0, 1, 0}},
            {{-1, 0, 0}, { 0, 0,-1}, {0, 1, 0}},
            {{ 0, 1, 0}, { 1, 0, 0}, {0, 0, 1}},
            {{ 0,-1, 0}, { 1, 0, 0}, {0, 0,-1}},
            {{ 0, 0, 1}, {-1, 0, 0}, {0, 1, 0}},
            {{ 0, 0,-1}, { 1, 0, 0}, {0, 1, 0}},
        };

        MeshLodData mesh;
        for (const Face& face : faces)
        {
            const uint32_t base = static_cast<uint32_t>(mesh.Vertices.size());
            auto corner = [&](float right, float up)
            {
                return XMFLOAT3((face.Normal.x + face.Right.x * right + face.Up.x * up) * h,
                                (face.Normal.y + face.Right.y * right + face.Up.y * up) * h,
                                (face.Normal.z + face.Right.z * right + face.Up.z * up) * h);
            };
            // Seen from the front (looking along -Normal) Right points right and Up points up.
            mesh.Vertices.push_back(MakeVertex(corner(-1.0f,  1.0f), face.Normal, 0.0f, 0.0f));
            mesh.Vertices.push_back(MakeVertex(corner( 1.0f,  1.0f), face.Normal, 1.0f, 0.0f));
            mesh.Vertices.push_back(MakeVertex(corner( 1.0f, -1.0f), face.Normal, 1.0f, 1.0f));
            mesh.Vertices.push_back(MakeVertex(corner(-1.0f, -1.0f), face.Normal, 0.0f, 1.0f));
            AddQuad(mesh, base, base + 1, base + 2, base + 3);
        }
        return mesh;
    }

    MeshLodData Plane(float width, float depth, int subdivisions)
    {
        subdivisions = std::max(subdivisions, 1);
        const XMFLOAT3 normal(0.0f, 1.0f, 0.0f);

        MeshLodData mesh;
        for (int row = 0; row <= subdivisions; ++row)
        {
            const float v = static_cast<float>(row) / static_cast<float>(subdivisions);
            for (int column = 0; column <= subdivisions; ++column)
            {
                const float u = static_cast<float>(column) / static_cast<float>(subdivisions);
                const XMFLOAT3 position((u - 0.5f) * width, 0.0f, (0.5f - v) * depth);
                mesh.Vertices.push_back(MakeVertex(position, normal, u, v));
            }
        }

        const uint32_t rowSize = static_cast<uint32_t>(subdivisions + 1);
        for (uint32_t row = 0; row < static_cast<uint32_t>(subdivisions); ++row)
        {
            for (uint32_t column = 0; column < static_cast<uint32_t>(subdivisions); ++column)
            {
                const uint32_t topLeft = row * rowSize + column; // far edge (+Z) is "top" seen from above
                AddQuad(mesh, topLeft, topLeft + 1, topLeft + rowSize + 1, topLeft + rowSize);
            }
        }
        return mesh;
    }
}
