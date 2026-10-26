/**
 * @file urdf_importer.hpp
 * @brief URDF -> SRM conversion.
 *
 * Only the subset needed for M1 is supported: links with an inertial block and
 * optional mesh visuals, and revolute / continuous / prismatic / fixed joints.
 * Unsupported elements are reported as issues instead of being silently
 * dropped.
 */
#ifndef SRSP_MODEL_URDF_IMPORTER_HPP
#define SRSP_MODEL_URDF_IMPORTER_HPP

#include <srsp/model/srm.hpp>

#include <string>

namespace srsp::model {

/**
 * @brief Converts a URDF document (XML text) into an SRM model.
 * @param xml UTF-8 URDF document.
 * @return Parsed model plus issues; `ok()` is false when conversion failed.
 */
LoadResult import_urdf(const std::string& xml);

/** @brief Reads a URDF file from disk and converts it. */
LoadResult import_urdf_file(const std::string& path);

}  // namespace srsp::model

#endif  // SRSP_MODEL_URDF_IMPORTER_HPP
