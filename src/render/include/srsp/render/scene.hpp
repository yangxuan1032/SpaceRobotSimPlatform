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
/** @brief_zh 用于表示天体或行星的即时球体。 */
struct Sphere {
  double position_m[3] = {0.0, 0.0, 0.0};  ///< World position of the centre.
  double radius_m = 1.0;                   ///< Radius in metres.
  float color[3] = {0.8f, 0.8f, 0.8f};     ///< Linear RGB in [0, 1].
  std::string label;                       ///< Optional body id for overlays.
  std::string mesh_uri;                    ///< "box" selects the procedural box mesh.
  std::string material_id;                 ///< Material key (reserved for glTF).
  double orientation[4] = {
      0.0, 0.0, 0.0, 1.0};  ///< Body -> world quaternion (x,y,z,w). / 体到世界的四元数（x,y,z,w）。
};

/** @brief One frame of scene data. */
/** @brief_zh 一帧场景数据。 */
struct Scene {
  std::vector<Sphere> bodies;  ///< Robots and payloads. / 机器人与载荷。
  Sphere earth;  ///< Planet sphere (radius typically 6.371e6). / 行星球体（半径约 6.371e6 米）。
  bool has_earth = false;  ///< Draw the planet when true. / 为真时绘制行星。
  double sun_direction[3] = {1.0, 0.0,
                             0.0};  ///< Unit vector towards the Sun. / 指向太阳的单位向量。
  bool eclipse = false;             ///< True when in umbra. / 处于本影时为真。
};

}  // namespace srsp::render

#endif  // SRSP_RENDER_SCENE_HPP
