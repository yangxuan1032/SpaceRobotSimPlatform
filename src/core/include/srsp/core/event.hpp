/**
 * @file event.hpp
 * @brief_zh 确定性事件键与有界命令队列。
 *
 * docs/01 section 5.1.2 requires that (a) every producer submits commands
 * through a queue — there is no direct write path into simulation state — and
 * (b) ordering is deterministic with the key `(t_tai_ns, priority, source_id,
 * seq)`. The queue is bounded and reports rejection instead of silently
 * dropping work; callers decide whether to block, retry or surface an error.
 */
#ifndef SRSP_CORE_EVENT_HPP
#define SRSP_CORE_EVENT_HPP

#include <cstdint>
#include <vector>

#include <cstddef>
#include <queue>

namespace srsp::core {

/** @brief Total order over scheduled work. */
/** @brief_zh 调度工作的全序。 */
struct EventKey {
  std::int64_t t_tai_ns =
      0;  ///< TAI nanoseconds; simulation time only. / 仅用于仿真时间的 TAI 纳秒。
  std::int32_t priority =
      0;  ///< Lower runs first at the same timestamp. / 同一时刻下数值小者先执行。
  std::uint32_t source_id =
      0;                  ///< Producer identity (subsystem/plugin). / 生产者标识（子系统或插件）。
  std::uint64_t seq = 0;  ///< Monotonic per producer. / 每个生产者内单调递增。

  [[nodiscard]] bool operator==(const EventKey&) const noexcept = default;
};

/** @brief Strict weak ordering used by the queue. */
/** @brief_zh 队列使用的严格弱序。 */
[[nodiscard]] inline bool operator<(const EventKey& a, const EventKey& b) noexcept {
  if (a.t_tai_ns != b.t_tai_ns) return a.t_tai_ns < b.t_tai_ns;
  if (a.priority != b.priority) return a.priority < b.priority;
  if (a.source_id != b.source_id) return a.source_id < b.source_id;
  return a.seq < b.seq;
}

/** @brief One queued command. */
/** @brief_zh 一条入队命令。 */
struct Command {
  EventKey key{};
  std::uint32_t kind = 0;             ///< Producer-defined tag. / 生产者自定义的标签。
  std::vector<std::uint8_t> payload;  ///< Opaque to the queue. / 队列不解释其内容。
};

/** @brief Result of a submission; nothing is dropped implicitly. */
/** @brief_zh 提交结果；不会隐式丢弃。 */
enum class PushResult {
  kAccepted = 0,
  kRejectedFull = 1,
  kRejectedInvalid = 2,
};

/** @brief Bounded, deterministic command queue. */
/** @brief_zh 有界且确定性的命令队列。 */
class CommandQueue {
 public:
  /** @brief Largest payload accepted, to bound a single submission. */
  /** @brief_zh 单次提交允许的最大载荷。 */
  static constexpr std::size_t kMaxPayloadBytes = 64 * 1024;

  /**
   * @brief Creates a queue holding at most @p capacity commands.
   * @param capacity Positive bound; a zero value is treated as one.
   */
  explicit CommandQueue(std::size_t capacity) : capacity_(capacity == 0 ? 1 : capacity) {}

  /**
   * @brief Submits a command.
   *
   * Rejects instead of growing without bound, so backpressure is observable:
   * the caller can block, retry or raise a diagnostic.
   */
  PushResult push(Command command) {
    if (command.key.t_tai_ns < 0 || command.payload.size() > kMaxPayloadBytes) {
      ++rejected_invalid_;
      return PushResult::kRejectedInvalid;
    }
    if (heap_.size() >= capacity_) {
      ++rejected_full_;
      return PushResult::kRejectedFull;
    }
    heap_.push(std::move(command));
    ++accepted_;
    return PushResult::kAccepted;
  }

  /**
   * @brief Removes every command with `t_tai_ns <= t_ns` in key order.
   * @param t_ns Inclusive upper bound in TAI nanoseconds.
   */
  [[nodiscard]] std::vector<Command> pop_until(std::int64_t t_ns) {
    std::vector<Command> due;
    while (!heap_.empty() && heap_.top().key.t_tai_ns <= t_ns) {
      due.push_back(heap_.top());
      heap_.pop();
    }
    return due;
  }

  [[nodiscard]] std::size_t size() const noexcept { return heap_.size(); }
  [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
  [[nodiscard]] std::uint64_t accepted() const noexcept { return accepted_; }
  [[nodiscard]] std::uint64_t rejected_full() const noexcept { return rejected_full_; }
  [[nodiscard]] std::uint64_t rejected_invalid() const noexcept { return rejected_invalid_; }

 private:
  struct Later {
    bool operator()(const Command& a, const Command& b) const noexcept { return b.key < a.key; }
  };

  std::size_t capacity_;
  std::priority_queue<Command, std::vector<Command>, Later> heap_;
  std::uint64_t accepted_ = 0;
  std::uint64_t rejected_full_ = 0;
  std::uint64_t rejected_invalid_ = 0;
};

}  // namespace srsp::core

#endif  // SRSP_CORE_EVENT_HPP
