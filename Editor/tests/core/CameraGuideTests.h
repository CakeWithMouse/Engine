#pragma once
#include "../../UI/CameraGuide.h"
#include <stdexcept>
inline void RunCameraGuideTests(int &checks) {
  auto check = [&](bool ok, const char *message) {
    if (!ok)
      throw std::runtime_error(message);
    ++checks;
  };
  IEditorHost::SceneObject camera;
  camera.id = 2;
  camera.gameplayKind = 2;
  camera.transform = {{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}};
  camera.projectData = R"({"fov":90,"nearClip":0.1,"farClip":100})";
  IEditorHost::Snapshot scene;
  scene.objects = {camera};
  auto v = CameraGuide::Read(camera, scene, 2);
  check(glm::length(v.forward - glm::vec3(0, 0, -1)) < 1e-5f,
        "camera looks along local -Z");
  auto corners = v.Corners(10);
  check(std::abs(glm::distance(corners[0], corners[1]) - 40) < .001f,
        "perspective horizontal frame matches FOV/aspect");
  check(std::abs(glm::distance(corners[1], corners[2]) - 20) < .001f,
        "vertical frame at distance 10");
  camera.projectData = R"({"orthographic":true,"orthoHeight":6,"viewPlane":2})";
  v = CameraGuide::Read(camera, scene, 2);
  check(v.forward == glm::vec3(0, -1, 0) && v.up == glm::vec3(0, 0, -1),
        "XZ view overrides camera rotation");
  check(v.Height(1) == 6 && v.Height(100) == 6,
        "orthographic frame does not grow with distance");
  IEditorHost::SceneObject parent;
  parent.id = 1;
  parent.transform = {{5, 2, 0}, {0, 0, 0, 1}, {2, 2, 2}};
  camera.parent = 1;
  camera.transform.position[0] = 3;
  scene.objects = {parent, camera};
  check(std::abs(CameraGuide::Read(camera, scene, 1).position.x - 11) < .001f,
        "parent scale affects camera position");
  camera.inheritScale = false;
  check(std::abs(CameraGuide::Read(camera, scene, 1).position.x - 8) < .001f,
        "camera can ignore parent scale");
  camera.projectData = "{}";
  parent.transform.rotation[1] = std::sin(glm::radians(45.f));
  parent.transform.rotation[3] = std::cos(glm::radians(45.f));
  scene.objects = {parent, camera};
  v = CameraGuide::Read(camera, scene, 1);
  check(glm::length(v.forward - glm::vec3(-1, 0, 0)) < .001f,
        "parent rotation changes view direction");
  auto lines = CameraGuide::Lines(v, true, 10, false, false);
  for (const auto &line : lines)
    check(std::isfinite(line.x1) && std::isfinite(line.z1),
          "finite camera geometry");
  check(lines.size() > CameraGuide::Lines(v, false, 10, false, false).size(),
        "only selected camera has full frustum");
  v.nearClip = 20;
  v.farClip = 25;
  lines = CameraGuide::Lines(v, true, 10, false, false);
  check(glm::distance(glm::vec3(lines[17].x0, lines[17].y0, lines[17].z0),
                      v.Corners(20)[0]) < .001f,
        "guide depth clamps to near plane");
}
