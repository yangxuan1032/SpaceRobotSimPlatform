/**
 * @file projection.hpp
 * @brief Pinhole projection with an explicit origin (dual-origin rendering).
 *
 * Orbital scenes span 1e7 m while robots need millimetre precision, so the
 * projection subtracts an explicit origin from both the camera and the point
 * before doing any float arithmetic. Passing the planet centre for the planet
 * and the camera position for nearby bodies keeps the cancellation bounded.
 */
#ifndef SRSP_RENDER_PROJECTION_HPP
#define SRSP_RENDER_PROJECTION_HPP

#include <srsp/render/renderer.hpp>

namespace srsp::render {

/** @brief Screen-space result of projecting one point. */
struct ScreenPoint {
  double x = 0.0;        ///< Pixels from the left edge.
  double y = 0.0;        ///< Pixels from the top edge.
  double depth = 0.0;    ///< Metres along the view direction.
  double scale = 0.0;    ///< Pixels per metre at that depth.
  bool visible = false;  ///< False when behind the near plane.
};

/**
 * @brief Projects @p point into the viewport.
 * @param camera Camera description (world frame).
 * @param viewport Framebuffer size.
 * @param point World position to project.
 * @param origin World origin subtracted from both camera and point.
 */
ScreenPoint project(const CameraView& camera, const Viewport& viewport, const double point[3],
                    const double origin[3]);

/** @brief Normalises a 3-vector in place-safe form; returns false on zero. */
bool normalize3(double vector[3]);

/** @brief Dot product of two 3-vectors. */
double dot3(const double a[3], const double b[3]);

/** @brief Cross product a x b written into out. */
void cross3(const double a[3], const double b[3], double out[3]);

}  // namespace srsp::render

#endif  // SRSP_RENDER_PROJECTION_HPP
