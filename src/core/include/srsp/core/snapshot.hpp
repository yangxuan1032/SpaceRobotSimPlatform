/**
 * @file snapshot.hpp
 * @brief Immutable snapshots with generation ids and read leases.
 *
 * The design rule (docs/01 section 5.1.2) is that the simulation owner
 * publishes immutable snapshots, readers hold a lease, and a buffer must not
 * be reused while a lease is outstanding. A snapshot id is
 * `step_index + generation`: acquiring an id that has already been evicted
 * from the store must fail instead of silently returning newer data.
 *
 * Header-only so tests and the future scheduler share exactly one definition.
 */
#ifndef SRSP_CORE_SNAPSHOT_HPP
#define SRSP_CORE_SNAPSHOT_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <cstddef>
#include <deque>

namespace srsp::core {

/** @brief Identifies one published snapshot. */
struct SnapshotId {
  std::uint64_t step_index = 0;
  std::uint64_t generation = 0;

  [[nodiscard]] bool operator==(const SnapshotId&) const noexcept = default;
};

/**
 * @brief Bounded store of immutable snapshots with generation tracking.
 *
 * Not thread-safe: the scheduler publishes and readers acquire on their own
 * threads only through a caller-provided synchronisation policy; keeping the
 * type lock-free makes the ownership rules explicit.
 */
class SnapshotStore {
 public:
  using Bytes = std::vector<std::uint8_t>;

  /** @brief Read-only view of one published snapshot. */
  class Lease {
   public:
    Lease() = default;
    Lease(SnapshotId id, std::shared_ptr<const Bytes> data) : id_(id), data_(std::move(data)) {}

    [[nodiscard]] bool valid() const noexcept { return data_ != nullptr; }
    [[nodiscard]] SnapshotId id() const noexcept { return id_; }
    [[nodiscard]] const Bytes& bytes() const noexcept { return *data_; }

    /** @brief Drops this reference; other leases keep the data alive. */
    void release() noexcept { data_.reset(); }

   private:
    SnapshotId id_{};
    std::shared_ptr<const Bytes> data_;
  };

  /**
   * @brief Keeps at most @p retained snapshots alive for acquisition.
   * @param retained Number of recent snapshots that can still be acquired.
   */
  explicit SnapshotStore(std::size_t retained = 4) : retained_(retained == 0 ? 1 : retained) {}

  /**
   * @brief Publishes a copy of @p data as the newest snapshot.
   * @param step_index Simulation step the data belongs to.
   * @return The id of the new snapshot (generation monotonic).
   */
  SnapshotId publish(std::uint64_t step_index, const void* data, std::size_t size) {
    auto bytes = std::make_shared<Bytes>(size);
    if (size > 0 && data != nullptr) {
      const auto* source = static_cast<const std::uint8_t*>(data);
      bytes->assign(source, source + size);
    }
    SnapshotId id{step_index, ++generation_};
    entries_.emplace_back(id, std::move(bytes));
    while (entries_.size() > retained_) {
      entries_.pop_front();  // leases still hold their own reference
    }
    latest_ = id;
    valid_ = true;
    return id;
  }

  /**
   * @brief Acquires a lease for @p id.
   * @return Empty when the id was never published or has been evicted.
   */
  [[nodiscard]] std::optional<Lease> acquire(SnapshotId id) const {
    for (const auto& entry : entries_) {
      if (entry.first == id) {
        return Lease{entry.first, entry.second};
      }
    }
    return std::nullopt;
  }

  /** @brief Convenience: lease on the newest snapshot. */
  [[nodiscard]] std::optional<Lease> acquire_latest() const {
    return valid_ ? acquire(latest_) : std::nullopt;
  }

  [[nodiscard]] SnapshotId latest_id() const noexcept { return latest_; }
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

 private:
  std::size_t retained_;
  std::uint64_t generation_ = 0;
  SnapshotId latest_{};
  bool valid_ = false;
  std::deque<std::pair<SnapshotId, std::shared_ptr<const Bytes>>> entries_;
};

}  // namespace srsp::core

#endif  // SRSP_CORE_SNAPSHOT_HPP
