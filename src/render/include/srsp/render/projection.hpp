/**
 * @file projection.hpp
 * @brief Pinhole projection with an explicit origin (dual-origin rendering).
 * @brief_zh 带显式原点的针孔投影（双原点渲染）。
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
/** @brief_zh 单点投影的屏幕空间结果。 */
struct ScreenPoint {
  double x = 0.0;        ///< Pixels from the left edge. / 距左边缘的像素数。
  double y = 0.0;        ///< Pixels from the top edge. / 距上边缘的像素数。
  double depth = 0.0;    ///< Metres along the view direction. / 沿视线方向的深度（米）。
  double scale = 0.0;    ///< Pixels per metre at that depth. / 该深度处每米对应的像素数。
  bool visible = false;  ///< False when behind the near plane. / 位于近平面之后时为 false。
};

/**
 * @brief Projects @p point into the viewport.
 * @brief_zh 将点投影到视口。
 * @param camera Camera description (world frame).
 * @param viewport Framebuffer size.
 * @param point World position to project.
 * @param origin World origin subtracted from both camera and point.
 */
ScreenPoint project(const CameraView& camera, const Viewport& viewport, const double point[3],
                    const double origin[3]);

/** @brief Normalises a 3-vector in place-safe form; returns false on zero. */
/** @brief_zh 归一化三维向量（不修改入参）；零向量返回 false。 */
bool normalize3(double vector[3]);

/** @brief Dot product of two 3-vectors. */
/** @brief_zh 两个三维向量的点积。 */
double dot3(const double a[3], const double b[3]);

/** @brief Cross product a x b written into out. */
/** @brief_zh 叉积 a×b，写入 out。 */
void cross3(const double a[3], const double b[3], double out[3]);

}  // namespace srsp::render

#endif  // SRSP_RENDER_PROJECTION_HPP
