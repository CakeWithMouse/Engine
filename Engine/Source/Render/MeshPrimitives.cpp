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

    uint32_t AddVertex(MeshLodData& mesh, const XMFLOAT3& position, const XMFLOAT3& normal, float u, float v)
    {
        mesh.Vertices.push_back(MakeVertex(position, normal, u, v));
        return static_cast<uint32_t>(mesh.Vertices.size() - 1);
    }

    /** Triangle of existing vertices, listed clockwise as seen from the side its vertex normals face. */
    void AddFacingTriangle(MeshLodData& mesh, uint32_t a, uint32_t b, uint32_t c)
    {
        const XMVECTOR pa = XMLoadFloat4(&mesh.Vertices[a].Position);
        const XMVECTOR cross = XMVector3Cross(XMVectorSubtract(XMLoadFloat4(&mesh.Vertices[b].Position), pa),
                                              XMVectorSubtract(XMLoadFloat4(&mesh.Vertices[c].Position), pa));
        const XMVECTOR normal = XMVectorAdd(XMVectorAdd(XMLoadFloat4(&mesh.Vertices[a].Normal), XMLoadFloat4(&mesh.Vertices[b].Normal)),
                                            XMLoadFloat4(&mesh.Vertices[c].Normal));
        if (XMVectorGetX(XMVector3Dot(cross, normal)) < 0.0f)
        {
            std::swap(b, c);
        }
        mesh.Indices.insert(mesh.Indices.end(), {a, b, c});
    }

    /** Flat round cap at height y, facing +Y or -Y; the disc maps onto the [0, 1] UV square. */
    void AddCap(MeshLodData& mesh, float radius, float y, bool bUp, int slices)
    {
        const XMFLOAT3 normal(0.0f, bUp ? 1.0f : -1.0f, 0.0f);
        const uint32_t centre = AddVertex(mesh, XMFLOAT3(0.0f, y, 0.0f), normal, 0.5f, 0.5f);
        const uint32_t first = static_cast<uint32_t>(mesh.Vertices.size());
        for (int slice = 0; slice <= slices; ++slice)
        {
            const float angle = 2.0f * XM_PI * static_cast<float>(slice) / static_cast<float>(slices);
            const float x = std::cos(angle), z = std::sin(angle);
            AddVertex(mesh, XMFLOAT3(x * radius, y, z * radius), normal, 0.5f + 0.5f * x, 0.5f - 0.5f * z);
        }
        for (uint32_t slice = 0; slice < static_cast<uint32_t>(slices); ++slice)
        {
            AddFacingTriangle(mesh, centre, first + slice, first + slice + 1);
        }
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

    MeshLodData Cylinder(float radius, float height, int slices)
    {
        slices = std::max(slices, 3);
        const float top = height * 0.5f;

        MeshLodData mesh;
        for (int slice = 0; slice <= slices; ++slice)
        {
            const float u = static_cast<float>(slice) / static_cast<float>(slices);
            const float angle = u * 2.0f * XM_PI;
            const XMFLOAT3 normal(std::cos(angle), 0.0f, std::sin(angle));
            AddVertex(mesh, XMFLOAT3(normal.x * radius, top, normal.z * radius), normal, u, 0.0f);
            AddVertex(mesh, XMFLOAT3(normal.x * radius, -top, normal.z * radius), normal, u, 1.0f);
        }
        for (uint32_t slice = 0; slice < static_cast<uint32_t>(slices); ++slice)
        {
            const uint32_t upper = slice * 2, lower = upper + 1;
            AddFacingTriangle(mesh, upper, lower, upper + 2);
            AddFacingTriangle(mesh, upper + 2, lower, lower + 2);
        }
        AddCap(mesh, radius, top, true, slices);
        AddCap(mesh, radius, -top, false, slices);
        return mesh;
    }

    MeshLodData Cone(float radius, float height, int slices)
    {
        slices = std::max(slices, 3);
        const float top = height * 0.5f;

        MeshLodData mesh;
        for (int slice = 0; slice < slices; ++slice)
        {
            // The apex gets a vertex per slice with the normal of the slice's middle.
            const float u0 = static_cast<float>(slice) / static_cast<float>(slices);
            const float u1 = static_cast<float>(slice + 1) / static_cast<float>(slices);
            auto sideNormal = [&](float u)
            {
                const float angle = u * 2.0f * XM_PI;
                XMFLOAT3 normal;
                XMStoreFloat3(&normal, XMVector3Normalize(XMVectorSet(height * std::cos(angle), radius, height * std::sin(angle), 0.0f)));
                return normal;
            };
            auto rim = [&](float u)
            {
                const float angle = u * 2.0f * XM_PI;
                return AddVertex(mesh, XMFLOAT3(std::cos(angle) * radius, -top, std::sin(angle) * radius), sideNormal(u), u, 1.0f);
            };
            const uint32_t apex = AddVertex(mesh, XMFLOAT3(0.0f, top, 0.0f), sideNormal((u0 + u1) * 0.5f), (u0 + u1) * 0.5f, 0.0f);
            const uint32_t a = rim(u0);
            AddFacingTriangle(mesh, apex, a, rim(u1));
        }
        AddCap(mesh, radius, -top, false, slices);
        return mesh;
    }

    MeshLodData Pyramid(float size, float height)
    {
        const float h = size * 0.5f, top = height * 0.5f;
        const XMFLOAT3 base[4] = {{-h, -top, -h}, {h, -top, -h}, {h, -top, h}, {-h, -top, h}};
        const XMFLOAT3 apex(0.0f, top, 0.0f);

        MeshLodData mesh;
        for (int side = 0; side < 4; ++side)
        {
            const XMFLOAT3& a = base[side];
            const XMFLOAT3& b = base[(side + 1) % 4];
            // Outward face normal: the cross product turned away from the axis.
            XMVECTOR normal = XMVector3Normalize(XMVector3Cross(XMVectorSubtract(XMLoadFloat3(&b), XMLoadFloat3(&a)),
                                                                XMVectorSubtract(XMLoadFloat3(&apex), XMLoadFloat3(&a))));
            const XMVECTOR outward = XMVectorSet(a.x + b.x, 0.0f, a.z + b.z, 0.0f);
            if (XMVectorGetX(XMVector3Dot(normal, outward)) < 0.0f)
            {
                normal = XMVectorNegate(normal);
            }
            XMFLOAT3 n;
            XMStoreFloat3(&n, normal);
            AddFacingTriangle(mesh, AddVertex(mesh, a, n, 0.0f, 1.0f), AddVertex(mesh, b, n, 1.0f, 1.0f), AddVertex(mesh, apex, n, 0.5f, 0.0f));
        }
        const XMFLOAT3 down(0.0f, -1.0f, 0.0f);
        const uint32_t first = static_cast<uint32_t>(mesh.Vertices.size());
        for (int corner = 0; corner < 4; ++corner)
        {
            AddVertex(mesh, base[corner], down, base[corner].x / size + 0.5f, base[corner].z / size + 0.5f);
        }
        AddFacingTriangle(mesh, first, first + 1, first + 2);
        AddFacingTriangle(mesh, first, first + 2, first + 3);
        return mesh;
    }
}
