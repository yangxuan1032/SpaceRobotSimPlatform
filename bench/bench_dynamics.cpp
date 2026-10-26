/**
 * @file bench_dynamics.cpp
 * @brief Benchmark: cost of one floating-base step for a 100-body chain.
 *
 * Prints machine-readable lines so CI can track regressions:
 *   kin_ms, mass_ms, bias_ms, step_ms  (milliseconds)
 *   budget_us=300 verdict=PASS|FAIL
 *
 * The process exits 0 even when the budget is missed: the benchmark records
 * numbers, it does not gate a build on machine speed. The verdict line is what
 * the nightly job collects.
 */
#include <srsp/dynamics/rigid_body.hpp>
#include <srsp/model/srm.hpp>

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace {

srsp::model::Model make_chain(int bodies) {
  srsp::model::Model model;
  model.name = "bench_chain";
  model.version = "1.0";

  for (int i = 0; i < bodies; ++i) {
    srsp::model::Body body;
    body.id = "link" + std::to_string(i);
    body.inertia.mass = 10.0;
    body.inertia.com = {0.0, 0.0, 0.1};
    body.inertia.inertia = {0.05, 0.0, 0.0, 0.0, 0.05, 0.0, 0.0, 0.0, 0.01};
    if (i > 0) {
      srsp::model::Joint joint;
      joint.id = "j" + std::to_string(i);
      joint.parent_body = "link" + std::to_string(i - 1);
      joint.child_body = body.id;
      joint.type = srsp::model::JointType::kRevolute;
      joint.axis = {0.0, 0.0, 1.0};
      joint.lower = -3.14159;
      joint.upper = 3.14159;
      joint.origin = {0.0, 0.0, 0.2};
      body.parent_joint = joint.id;
      model.joints.push_back(joint);
    }
    model.bodies.push_back(body);
  }
  return model;
}

template <typename Callable>
double time_ms(Callable&& callable, int iterations) {
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    callable();
  }
  const auto stop = std::chrono::steady_clock::now();
  const std::chrono::duration<double, std::milli> elapsed = stop - start;
  return elapsed.count() / static_cast<double>(iterations);
}

}  // namespace

int main() {
  constexpr int kBodies = 100;
  constexpr int kIterations = 50;
  const auto model = make_chain(kBodies);
  const auto validation = srsp::model::validate(model);
  if (!validation.empty()) {
    std::fprintf(stderr, "bench model is invalid (%zu issues)\n", validation.size());
    return 1;
  }

  const srsp::dynamics::RigidBodyDynamics dynamics(model);
  Eigen::VectorXd q = dynamics.neutral_configuration();
  Eigen::VectorXd v = Eigen::VectorXd::Zero(dynamics.nv());
  const Eigen::VectorXd tau = Eigen::VectorXd::Zero(dynamics.nv());
  for (int i = 0; i < dynamics.joint_count(); ++i) {
    q[7 + i] = 0.01 * static_cast<double>(i % 10);
  }

  // One warm-up step so the allocator and cache are hot.
  dynamics.step(&q, &v, tau, 1e-3);
  q = dynamics.neutral_configuration();
  v.setZero();

  const double mass_ms = time_ms([&] { (void)dynamics.mass_matrix(q); }, kIterations);
  const double bias_ms = time_ms([&] { (void)dynamics.bias_forces(q, v); }, kIterations);
  const double step_ms = time_ms([&] { dynamics.step(&q, &v, tau, 1e-3); }, kIterations);

  std::printf("bodies=%d\n", kBodies);
  std::printf("joints=%d\n", dynamics.joint_count());
  std::printf("mass_ms=%.3f\n", mass_ms);
  std::printf("bias_ms=%.3f\n", bias_ms);
  std::printf("step_ms=%.3f\n", step_ms);
  const double step_us = step_ms * 1000.0;
  std::printf("step_us=%.1f\n", step_us);
  std::printf("budget_us=300 verdict=%s\n", step_us <= 300.0 ? "PASS" : "FAIL");
  return 0;
}
