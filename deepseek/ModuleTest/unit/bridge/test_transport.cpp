/**
 * @file test_transport.cpp
 * @brief Transport name parsing shared by the CLI and other front ends (A5).
 */
#include <srsp/bridge/transport.hpp>

#include <string>

#include <catch2/catch_test_macros.hpp>

using srsp::bridge::parse_transport;
using srsp::bridge::to_string;
using srsp::bridge::TransportKind;

TEST_CASE("transport names parse to the documented kinds", "[transport]") {
  const auto* inproc = parse_transport("inproc");
  REQUIRE(inproc != nullptr);
  CHECK(*inproc == TransportKind::kInProc);

  const auto* sidecar = parse_transport("sidecar");
  REQUIRE(sidecar != nullptr);
  CHECK(*sidecar == TransportKind::kSidecar);

  CHECK(parse_transport("grpc") == nullptr);
  CHECK(parse_transport("") == nullptr);
}

TEST_CASE("transport names round-trip", "[transport]") {
  CHECK(std::string(to_string(TransportKind::kInProc)) == "inproc");
  CHECK(std::string(to_string(TransportKind::kSidecar)) == "sidecar");
}