/**
 * @file test_mesh_source.cpp
 * @brief URI classification shared by the Qt and RHI backends (A6).
 */
#include <srsp/render/mesh_source.hpp>

#include <string>

#include <catch2/catch_test_macros.hpp>

using srsp::render::mesh_source_from_uri;
using srsp::render::MeshSource;
using srsp::render::to_string;

TEST_CASE("mesh URI classifies into geometry kinds", "[mesh_source]") {
  CHECK(mesh_source_from_uri("") == MeshSource::kSphere);
  CHECK(mesh_source_from_uri("box") == MeshSource::kBox);
  CHECK(mesh_source_from_uri("meshes/effector.glb") == MeshSource::kGltf);
}

TEST_CASE("mesh source names are stable", "[mesh_source]") {
  CHECK(std::string(to_string(MeshSource::kSphere)) == "sphere");
  CHECK(std::string(to_string(MeshSource::kBox)) == "box");
  CHECK(std::string(to_string(MeshSource::kGltf)) == "gltf");
}