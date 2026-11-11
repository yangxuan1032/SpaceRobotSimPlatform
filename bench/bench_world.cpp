/**
 * @file bench_world.cpp
 * @brief Benchmark: cost of one World step with gravity, SRP and drag.
 *
 * Prints machine-readable lines so a regression is visible:
 *   entities=N step_us=<median> budget_us=1000 verdict=PASS|FAIL
 *
 * The budget is informational: the benchmark records numbers instead of
 * gating a build on machine speed.
 */
#include <srsp/core/environment.hpp>
#include <srsp/core/world.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#include <cstdio>

namespace {

constexpr int kEntities = 16;
constexpr int kSamples = 200;
constexpr double kDt = 0.01;
constexpr double kBudgetUs = 1000.0;

srsp::core::EnvironmentSpec make_environment() {
  srsp::core::EnvironmentSpec spec;
  spec.earth.id = "earth";
  spec.earth.mu = 3.986004418e14;
  spec.earth.radius_m = 6.371e6;
  spec.earth.j2 = 1.08263e-3;
  spec.earth.reference_radius_m = 6.378e6;
  spec.sun.id = "sun";
  spec.sun.position_m = {1.495978707e11, 0.0, 0.0};
  spec.use_j2 = true;
  spec.use_third_body = true;
  spec.atmosphere.enabled = true;
  spec.atmosphere.rho0_kg_m3 = 4.0e-12;
  spec.atmosphere.reference_altitude_m = 400.0e3;
  spec.atmosphere.scale_height_m = 60.0e3;
  return spec;
}

}  // namespace

int main() {
  srsp::core::World world(4);
  const auto spec = make_environment();
  world.set_environment(spec);

  const double radius = 6.771e6;
  const double speed = srsp::core::circular_speed(spec.earth.mu, radius);
  for (int i = 0; i < kEntities; ++i) {
    srsp::core::WorldEntity entity;
    entity.id = "entity/bench" + std::to_string(i);
    entity.mass_kg = 500.0 + static_cast<double>(i);
    entity.area_m2 = 4.0;
    entity.reflectivity = 1.3;
    entity.drag_coefficient = 2.2;
    entity.position_m = {radius * std::cos(0.1 * i), radius * std::sin(0.1 * i), 0.0};
    entity.velocity_m_s = {-speed * std::sin(0.1 * i), speed * std::cos(0.1 * i), 0.0};
    std::string error;
    if (!world.add_entity(entity, &error)) {
      std::fprintf(stderr, "setup failed: %s\n", error.c_str());
      return 1;
    }
  }

  std::vector<double> samples;
  samples.reserve(kSamples);
  std::string error;
  for (int i = 0; i < kSamples; ++i) {
    const auto start = std::chrono::steady_clock::now();
    if (!world.step(kDt, &error)) {
      std::fprintf(stderr, "step failed: %s\n", error.c_str());
      return 1;
    }
    const auto end = std::chrono::steady_clock::now();
    samples.push_back(std::chrono::duration<double, std::micro>(end - start).count());
  }

  std::sort(samples.begin(), samples.end());
  const double median = samples[samples.size() / 2];
  const double p95 = samples[static_cast<std::size_t>(0.95 * samples.size())];
  std::printf("entities=%d step_us=%.3f p95_us=%.3f budget_us=%.0f verdict=%s\n", kEntities, median,
              p95, kBudgetUs, median <= kBudgetUs ? "PASS" : "FAIL");
  return 0;
}
