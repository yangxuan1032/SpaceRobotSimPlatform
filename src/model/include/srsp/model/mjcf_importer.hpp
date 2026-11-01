/**
 * @file mjcf_importer.hpp
 * @brief MJCF (MuJoCo XML) subset -> SRM conversion.
 * @brief_zh MJCF（MuJoCo XML）子集到 SRM 的转换。
 *
 * Supported: nested <body> with pos/quat, <joint type="hinge"|"slide"> with
 * axis/range/pos, <inertial pos/mass/diaginertia> (or mass/diaginertia on the
 * body element), and <freejoint>. Ball/plane joints, tendons, actuators,
 * equality constraints and geoms beyond a size hint are reported as issues
 * instead of being silently ignored.
 */
#ifndef SRSP_MODEL_MJCF_IMPORTER_HPP
#define SRSP_MODEL_MJCF_IMPORTER_HPP

#include <srsp/model/srm.hpp>

#include <string>

namespace srsp::model {

/** @brief Converts an MJCF document into an SRM model. */
/** @brief_zh 将 MJCF 文档转换为 SRM 模型。 */
LoadResult import_mjcf(const std::string& xml);

/** @brief Reads an MJCF file from disk and converts it. */
/** @brief_zh 从磁盘读取 MJCF 文件并转换。 */
LoadResult import_mjcf_file(const std::string& path);

}  // namespace srsp::model

#endif  // SRSP_MODEL_MJCF_IMPORTER_HPP
