/**
 * @file test_diagnostics.cpp
 * @brief Unit tests for srsp::core::Diagnostics.
 */
#include <srsp/core/diagnostics.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using srsp::core::Diagnostic;
using srsp::core::Diagnostics;
using srsp::core::Severity;

TEST_CASE("diagnostics counts records by severity", "[diagnostics]") {
  Diagnostics diagnostics;
  diagnostics.emit(Severity::kInfo, "core.ready", "ready");
  diagnostics.emit(Severity::kWarn, "core.slow", "slow step");
  diagnostics.emit(Severity::kError, "core.fail", "failed");

  CHECK(diagnostics.count() == 3);
  CHECK(diagnostics.count(Severity::kInfo) == 1);
  CHECK(diagnostics.count(Severity::kWarn) == 1);
  CHECK(diagnostics.count(Severity::kError) == 1);
  CHECK(diagnostics.count(Severity::kDebug) == 0);
}

TEST_CASE("diagnostics forwards records to the sink", "[diagnostics]") {
  std::vector<Diagnostic> seen;
  Diagnostics diagnostics([&seen](const Diagnostic& record) { seen.push_back(record); });

  diagnostics.emit(Severity::kCritical, "core.crash", "crashed", "{\"step\":7}");

  REQUIRE(seen.size() == 1);
  CHECK(seen[0].severity == Severity::kCritical);
  CHECK(seen[0].code == "core.crash");
  CHECK(seen[0].context_json == "{\"step\":7}");
}

TEST_CASE("diagnostics renders a single-line json record", "[diagnostics]") {
  Diagnostic record;
  record.severity = Severity::kWarn;
  record.code = "core.quote";
  record.message = "line\nwith \"quotes\"";

  const std::string line = Diagnostics::to_json_line(record);
  CHECK(line.find("\"severity\":\"warn\"") != std::string::npos);
  CHECK(line.find("\"code\":\"core.quote\"") != std::string::npos);
  CHECK(line.find("line\\nwith \\\"quotes\\\"") != std::string::npos);
  CHECK(line.find('\n') == std::string::npos);
}

TEST_CASE("diagnostics history honours the capacity limit", "[diagnostics]") {
  Diagnostics diagnostics;
  diagnostics.set_history_capacity(2);
  for (int i = 0; i < 5; ++i) {
    diagnostics.emit(Severity::kInfo, "core.step", std::to_string(i));
  }

  CHECK(diagnostics.count() == 2);
  REQUIRE(diagnostics.records().size() == 2);
  CHECK(diagnostics.records()[0].message == "3");
  CHECK(diagnostics.records()[1].message == "4");
}

TEST_CASE("diagnostics capacity zero disables retention", "[diagnostics]") {
  Diagnostics diagnostics;
  diagnostics.set_history_capacity(0);
  diagnostics.emit(Severity::kInfo, "core.step", "x");

  CHECK(diagnostics.count() == 0);
  CHECK(diagnostics.records().empty());
}
