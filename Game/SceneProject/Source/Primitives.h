#pragma once
#include <Engine/Components/ComponentType.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/SpecificComponents/SphereComponent.h>

// CPU geometry only. Scene/Game creates GPU resources when a viewport is attached.
namespace SceneProject
{
class Cube final : public CubeComponent
{
public:
    Cube() { InitCube(); SetColor({0.65f, 0.78f, 0.95f, 1}); }
};
class Sphere final : public SphereComponent
{
public:
    Sphere() { InitSphere(); SetColor({0.9f, 0.65f, 0.3f, 1}); }
};
class Mesh : public GameComponent
{
protected:
    std::vector<DirectX::XMFLOAT4> points;
    std::vector<int> triangles;
    void Triangle(glm::vec3 a, glm::vec3 b, glm::vec3 c)
    {
        const auto normal = glm::normalize(glm::cross(b - a, c - a));
        const float shade = 0.55f + 0.45f * std::abs(glm::dot(normal, glm::normalize(glm::vec3(1, 2, 3))));
        for (auto p : {a, b, c})
        {
            triangles.push_back(static_cast<int>(points.size() / 2));
            points.emplace_back(p.x, p.y, p.z, 1.0f);
            points.emplace_back(shade, shade, shade, 1.0f);
        }
    }
    void Finish(glm::vec4 color)
    {
        SetColor(color);
        InitPoints(points.data(), static_cast<int>(points.size()), triangles.data(), static_cast<int>(triangles.size()));
    }
};
class Plane final : public Mesh
{
public:
    Plane()
    {
        // Two sides, useful both as a floor and as a rotated wall.
        glm::vec3 a(-0.5f, 0, -0.5f), b(0.5f, 0, -0.5f), c(0.5f, 0, 0.5f), d(-0.5f, 0, 0.5f);
        Triangle(a, c, b); Triangle(a, d, c); Triangle(a, b, c); Triangle(a, c, d);
        Finish({0.4f, 0.58f, 0.42f, 1});
    }
};
class Cylinder final : public Mesh
{
public:
    Cylinder()
    {
        for (int i = 0; i < 32; ++i)
        {
            const float a = i * glm::two_pi<float>() / 32, b = (i + 1) * glm::two_pi<float>() / 32;
            glm::vec3 p(0.5f * cos(a), -0.5f, 0.5f * sin(a)), q(0.5f * cos(b), -0.5f, 0.5f * sin(b));
            const auto r = p + glm::vec3(0, 1, 0), s = q + glm::vec3(0, 1, 0);
            Triangle(p, r, s); Triangle(p, s, q);
            Triangle({0, -0.5f, 0}, p, q); Triangle({0, 0.5f, 0}, s, r);
        }
        Finish({0.45f, 0.7f, 0.85f, 1});
    }
};
class Cone final : public Mesh
{
public:
    Cone()
    {
        for (int i = 0; i < 32; ++i)
        {
            const float a = i * glm::two_pi<float>() / 32, b = (i + 1) * glm::two_pi<float>() / 32;
            glm::vec3 p(0.5f * cos(a), -0.5f, 0.5f * sin(a)), q(0.5f * cos(b), -0.5f, 0.5f * sin(b));
            Triangle(p, {0, 0.5f, 0}, q); Triangle({0, -0.5f, 0}, p, q);
        }
        Finish({0.9f, 0.5f, 0.35f, 1});
    }
};
class Pyramid final : public Mesh
{
public:
    Pyramid()
    {
        glm::vec3 p[] = {{-0.5f,-0.5f,-0.5f}, {0.5f,-0.5f,-0.5f}, {0.5f,-0.5f,0.5f}, {-0.5f,-0.5f,0.5f}};
        for (int i = 0; i < 4; ++i) Triangle(p[i], {0,0.5f,0}, p[(i+1)%4]);
        Triangle(p[0], p[1], p[2]); Triangle(p[0], p[2], p[3]);
        Finish({0.75f, 0.55f, 0.85f, 1});
    }
};
inline ComponentTypeCatalog GetComponentTypes()
{
    static const ComponentType types[] = {
        {"Primitive.Cube", "Куб", EngineComponentDetail::Create<Cube>},
        {"Primitive.Sphere", "Сфера", EngineComponentDetail::Create<Sphere>},
        {"Primitive.Plane", "Плоскость", EngineComponentDetail::Create<Plane>},
        {"Primitive.Cylinder", "Цилиндр", EngineComponentDetail::Create<Cylinder>},
        {"Primitive.Cone", "Конус", EngineComponentDetail::Create<Cone>},
        {"Primitive.Pyramid", "Пирамида", EngineComponentDetail::Create<Pyramid>}
    };
    return {types, sizeof(types) / sizeof(types[0])};
}
}
