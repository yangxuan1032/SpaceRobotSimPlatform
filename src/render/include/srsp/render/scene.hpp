/**
 * @file scene.hpp
 * @brief Renderer-agnostic scene description handed to a backend each frame.
 */
#ifndef SRSP_RENDER_SCENE_HPP
#define SRSP_RENDER_SCENE_HPP

#include <string>
#include <vector>

namespace srsp::render {

/** @brief Immediate-mode sphere used to represent a body or a planet. */
struct Sphere {
  double position_m[3] = {0.0, 0.0, 0.0};        ///< World position of the centre.
  double radius_m = 1.0;                         ///< Radius in metres.
  float color[3] = {0.8f, 0.8f, 0.8f};           ///< Linear RGB in [0, 1].
  std::string label;                             ///< Optional body id for overlays.
  std::string mesh_uri;                          ///< "box" selects the procedural box mesh.
  std::string material_id;                       ///< Material key (reserved for glTF).
  double orientation[4] = {0.0, 0.0, 0.0, 1.0};  ///< Body -> world quaternion (x,y,z,w).
};

/** @brief One frame of scene data. */
struct Scene {
  std::vector<Sphere> bodies;                 ///< Robots and payloads.
  Sphere earth;                               ///< Planet sphere (radius typically 6.371e6).
  bool has_earth = false;                     ///< Draw the planet when true.
  double sun_direction[3] = {1.0, 0.0, 0.0};  ///< Unit vector towards the Sun.
  bool eclipse = false;                       ///< True when in umbra.
};

}  // namespace srsp::render

#endif  // SRSP_RENDER_SCENE_HPP
