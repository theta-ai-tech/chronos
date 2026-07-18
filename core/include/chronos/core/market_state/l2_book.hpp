#pragma once

#include "chronos/contracts/fixed_point.hpp"
#include "chronos/contracts/value_objects.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace chronos::core::market_state {

enum class L2Operation : std::uint8_t { SetAbsolute, Delete };

enum class L2BookShape : std::uint8_t {
  Normal,
  Locked,
  Crossed,
  OneSided,
  Empty,
  Unknown,
};

enum class L2SideCompleteness : std::uint8_t {
  Complete,
  BoundedWithProvenTop,
  BoundaryExhausted,
  Unknown,
};

enum class L2TransitionFailure : std::uint8_t {
  None,
  WrongListing,
  DefinitionMismatch,
  InvalidPrice,
  InvalidQuantity,
  InvalidOperation,
  InvalidCompleteness,
  DuplicateLevel,
  DuplicateChange,
  ResourceLimitExceeded,
};

struct L2BookConfig final {
  contracts::ListingId listing_id;
  contracts::VersionRef price_definition;
  contracts::VersionRef quantity_definition;
  std::size_t maximum_levels_per_side{};
  std::size_t maximum_changes_per_delta{};
};

struct L2Level final {
  contracts::Price price;
  contracts::Quantity quantity;

  bool operator==(const L2Level &) const = default;
};

struct L2Change final {
  contracts::Price price;
  contracts::Quantity quantity;
  L2Operation operation{L2Operation::SetAbsolute};

  bool operator==(const L2Change &) const = default;
};

struct L2Snapshot final {
  contracts::ListingId listing_id;
  std::vector<L2Level> bids;
  std::vector<L2Level> asks;
  L2SideCompleteness bid_completeness{L2SideCompleteness::Unknown};
  L2SideCompleteness ask_completeness{L2SideCompleteness::Unknown};
};

struct L2Delta final {
  contracts::ListingId listing_id;
  std::vector<L2Change> bid_changes;
  std::vector<L2Change> ask_changes;
};

struct L2TransitionResult final {
  L2TransitionFailure failure{L2TransitionFailure::None};
  bool content_changed{};
  std::uint64_t transition_sequence{};

  [[nodiscard]] bool ok() const noexcept {
    return failure == L2TransitionFailure::None;
  }
};

struct L2TopOfBook final {
  std::optional<L2Level> best_bid;
  std::optional<L2Level> best_ask;
  std::optional<contracts::Price> spread;
  L2SideCompleteness bid_completeness{L2SideCompleteness::Unknown};
  L2SideCompleteness ask_completeness{L2SideCompleteness::Unknown};
  L2BookShape shape{L2BookShape::Unknown};

  bool operator==(const L2TopOfBook &) const = default;
};

struct L2StorageProfile final {
  std::size_t bid_capacity{};
  std::size_t ask_capacity{};
  std::size_t scratch_bid_capacity{};
  std::size_t scratch_ask_capacity{};
  std::size_t validation_capacity{};

  bool operator==(const L2StorageProfile &) const = default;
};

class L2Book final {
public:
  [[nodiscard]] static std::optional<L2Book> create(L2BookConfig config);

  L2Book(L2Book &&) noexcept;
  L2Book &operator=(L2Book &&) noexcept;
  ~L2Book();

  [[nodiscard]] L2TransitionResult apply_snapshot(const L2Snapshot &snapshot);
  [[nodiscard]] L2TransitionResult apply_delta(const L2Delta &delta);

  [[nodiscard]] contracts::ListingId listing_id() const noexcept;
  [[nodiscard]] std::span<const L2Level> bids() const noexcept;
  [[nodiscard]] std::span<const L2Level> asks() const noexcept;
  [[nodiscard]] L2TopOfBook top_of_book() const noexcept;
  [[nodiscard]] std::uint64_t transition_sequence() const noexcept;
  [[nodiscard]] L2StorageProfile storage_profile() const noexcept;

private:
  struct State;
  explicit L2Book(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
};

} // namespace chronos::core::market_state
