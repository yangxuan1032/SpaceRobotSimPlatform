/**
 * @file mjcf_importer.hpp
 * @brief MJCF (MuJoCo XML) subset -> SRM conversion.
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
LoadResult import_mjcf(const std::string& xml);

/** @brief Reads an MJCF file from disk and converts it. */
LoadResult import_mjcf_file(const std::string& path);

}  // namespace srsp::model

#endif  // SRSP_MODEL_MJCF_IMPORTER_HPP
