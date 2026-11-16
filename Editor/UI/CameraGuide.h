#pragma once
#include "../Host/IEditorHost.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

// Geometry shared by the viewport and inspector. Scene snapshots contain LOCAL
// transforms.
namespace CameraGuide {
inline bool IsCamera(const IEditorHost::SceneObject &o) {
  return o.gameplayKind == 2 || o.type == "Gameplay.Camera" ||
         o.type == "Engine.Camera" || o.type == "Camera";
}
inline glm::mat4 World(const IEditorHost::SceneObject &o,
                       const IEditorHost::Snapshot &scene, size_t depth = 0) {
  const auto &t = o.transform;
  glm::quat q(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]);
  q = glm::length(q) > 1e-6f ? glm::normalize(q) : glm::quat(1, 0, 0, 0);
  auto local =
      glm::translate(glm::mat4(1),
                     glm::vec3(t.position[0], t.position[1], t.position[2])) *
      glm::mat4_cast(q) *
      glm::scale(glm::mat4(1), glm::vec3(t.scale[0], t.scale[1], t.scale[2]));
  if (o.parent && depth < scene.objects.size())
    for (const auto &parent : scene.objects)
      if (parent.id == o.parent) {
        auto p = World(parent, scene, depth + 1);
        if (!o.inheritScale)
          for (int i = 0; i < 3; ++i) {
            const float length = glm::length(glm::vec3(p[i]));
            if (length > 1e-6f)
              p[i] /= length;
          }
        return p * local;
      }
  return local;
}
struct View {
  glm::vec3 position{}, forward{0, 0, -1}, up{0, 1, 0}, right{-1, 0, 0};
  float nearClip = .1f, farClip = 10000, fov = 60, orthoHeight = 10,
        aspect = 16.f / 9.f;
  bool orthographic = false;
  int plane = 0;
  float Height(float distance) const {
    return orthographic ? orthoHeight
                        : 2 * distance * std::tan(glm::radians(fov) * .5f);
  }
  std::array<glm::vec3, 4> Corners(float distance) const {
    const auto c = position + forward * distance;
    const auto u = up * (Height(distance) * .5f),
               r = right * (Height(distance) * aspect * .5f);
    return {c - r - u, c + r - u, c + r + u, c - r + u};
  }
};
inline View Read(const IEditorHost::SceneObject &o,
                 const IEditorHost::Snapshot &scene, float aspect) {
  View v;
  v.aspect = std::clamp(aspect, .05f, 20.f);
  const auto data =
      QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData))
          .object();
  auto number = [&](const char *key, float fallback, float lo, float hi) {
    const double n = data[key].toDouble(fallback);
    return std::isfinite(n) ? std::clamp(float(n), lo, hi) : fallback;
  };
  v.nearClip = number("nearClip", .1f, .001f, 100000);
  v.farClip = number("farClip", 10000, v.nearClip + .001f, 1000000);
  v.fov = number("fov", 60, 1, 179);
  v.orthoHeight = number("orthoHeight", 10, .001f, 1000000);
  v.orthographic = data["orthographic"].toBool();
  v.plane = data["viewPlane"].toInt();
  const auto world = World(o, scene);
  v.position = glm::vec3(world[3]);
  glm::vec3 scale, translation, skew;
  glm::vec4 perspective;
  glm::quat rotation(1, 0, 0, 0);
  if (glm::decompose(world, scale, rotation, translation, skew, perspective))
    rotation = glm::normalize(rotation);
  v.forward = rotation * glm::vec3(0, 0, -1);
  v.up = rotation * glm::vec3(0, 1, 0);
  if (v.plane == 1) {
    v.forward = {0, 0, -1};
    v.up = {0, 1, 0};
  }
  if (v.plane == 2) {
    v.forward = {0, -1, 0};
    v.up = {0, 0, -1};
  }
  if (v.plane == 3) {
    v.forward = {-1, 0, 0};
    v.up = {0, 1, 0};
  }
  // Player uses lookAtLH: right = cross(up, forward).
  v.right = glm::normalize(glm::cross(v.up, v.forward));
  v.up = glm::normalize(glm::cross(v.forward, v.right));
  return v;
}
inline std::vector<EngineOverlayLine> Lines(const View &v, bool selected,
                                            float depth, bool full, bool xray) {
  std::vector<EngineOverlayLine> result;
  const uint32_t flags =
      (xray ? OverlayFlags::AlwaysOnTop : OverlayFlags::DepthTest) |
      OverlayFlags::Thickness(2);
  auto line = [&](glm::vec3 a, glm::vec3 b, uint32_t color) {
    result.push_back({a.x, a.y, a.z, b.x, b.y, b.z, color, flags});
  };
  const uint32_t color = selected ? 0xfff2d067 : 0xffb09e72,
                 nearColor = 0xff83db92, farColor = 0xff62c7ff;
  // A small 3D camera body, with a roof notch to make roll visible.
  auto at = [&](float x, float y, float z) {
    return v.position + v.right * x + v.up * y + v.forward * z;
  };
  for (int side = -1; side <= 1; side += 2) {
    line(at(-.3f, -.2f, side * .25f), at(.3f, -.2f, side * .25f), color);
    line(at(.3f, -.2f, side * .25f), at(.3f, .2f, side * .25f), color);
    line(at(.3f, .2f, side * .25f), at(-.3f, .2f, side * .25f), color);
    line(at(-.3f, .2f, side * .25f), at(-.3f, -.2f, side * .25f), color);
  }
  for (int x : {-1, 1})
    for (int y : {-1, 1})
      line(at(x * .3f, y * .2f, -.25f), at(x * .3f, y * .2f, .25f), color);
  line(at(-.2f, .2f, 0), at(0, .42f, 0), color);
  line(at(0, .42f, 0), at(.2f, .2f, 0), color);
  line(v.position, at(0, 0, 1.8f), color);
  line(at(0, 0, 1.8f), at(.2f, 0, 1.4f), color);
  line(at(0, 0, 1.8f), at(-.2f, 0, 1.4f), color);
  if (!selected)
    return result;
  const float end = full ? v.farClip : std::clamp(depth, v.nearClip, v.farClip);
  auto n = v.Corners(v.nearClip), f = v.Corners(end);
  for (int i = 0; i < 4; ++i) {
    line(n[i], n[(i + 1) % 4], nearColor);
    line(f[i], f[(i + 1) % 4], farColor);
    line(n[i], f[i], color);
  }
  line(v.position, v.position + v.forward * end, color);
  // Ten evenly spaced distance marks; their spacing is stated in the inspector.
  for (int i = 1; i <= 10; ++i) {
    auto p = v.position + v.forward * (end * i / 10.f);
    line(p - v.right * .12f, p + v.right * .12f, farColor);
  }
  return result;
}
} // namespace CameraGuide
