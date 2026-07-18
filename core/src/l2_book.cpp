#include "chronos/core/market_state/l2_book.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace chronos::core::market_state {
namespace {

bool price_less(const L2Level &left, const L2Level &right) {
  return left.price.units() < right.price.units();
}

bool valid_price(const L2BookConfig &config, const contracts::Price &price) {
  return price.definition_ref() == config.price_definition && price.units() > 0;
}

bool valid_quantity(const L2BookConfig &config,
                    const contracts::Quantity &quantity) {
  return quantity.definition_ref() == config.quantity_definition &&
         quantity.units() >= 0;
}

L2TransitionFailure validate_levels(const L2BookConfig &config,
                                    std::vector<L2Level> &levels) {
  if (levels.size() > config.maximum_levels_per_side)
    return L2TransitionFailure::ResourceLimitExceeded;
  for (const auto &level : levels) {
    if (!valid_price(config, level.price))
      return level.price.definition_ref() == config.price_definition
                 ? L2TransitionFailure::InvalidPrice
                 : L2TransitionFailure::DefinitionMismatch;
    if (!valid_quantity(config, level.quantity))
      return level.quantity.definition_ref() == config.quantity_definition
                 ? L2TransitionFailure::InvalidQuantity
                 : L2TransitionFailure::DefinitionMismatch;
  }
  std::sort(levels.begin(), levels.end(), price_less);
  const auto duplicate =
      std::adjacent_find(levels.begin(), levels.end(),
                         [](const L2Level &left, const L2Level &right) {
                           return left.price.units() == right.price.units();
                         });
  if (duplicate != levels.end())
    return L2TransitionFailure::DuplicateLevel;
  levels.erase(std::remove_if(levels.begin(), levels.end(),
                              [](const L2Level &level) {
                                return level.quantity.units() == 0;
                              }),
               levels.end());
  return L2TransitionFailure::None;
}

L2TransitionFailure
validate_changes(const L2BookConfig &config,
                 const std::vector<L2Change> &changes,
                 std::vector<contracts::AmountUnits> &prices) {
  prices.clear();
  for (const auto &change : changes) {
    if (change.operation != L2Operation::SetAbsolute &&
        change.operation != L2Operation::Delete) {
      return L2TransitionFailure::InvalidOperation;
    }
    if (!valid_price(config, change.price))
      return change.price.definition_ref() == config.price_definition
                 ? L2TransitionFailure::InvalidPrice
                 : L2TransitionFailure::DefinitionMismatch;
    if (!valid_quantity(config, change.quantity))
      return change.quantity.definition_ref() == config.quantity_definition
                 ? L2TransitionFailure::InvalidQuantity
                 : L2TransitionFailure::DefinitionMismatch;
    if (change.operation == L2Operation::Delete &&
        change.quantity.units() != 0) {
      return L2TransitionFailure::InvalidQuantity;
    }
    prices.push_back(change.price.units());
  }
  std::sort(prices.begin(), prices.end());
  if (std::adjacent_find(prices.begin(), prices.end()) != prices.end())
    return L2TransitionFailure::DuplicateChange;
  return L2TransitionFailure::None;
}

void apply_change(std::vector<L2Level> &levels, const L2Change &change) {
  const auto found = std::lower_bound(
      levels.begin(), levels.end(), change.price.units(),
      [](const L2Level &level, contracts::AmountUnits price_units) {
        return level.price.units() < price_units;
      });
  const bool exists =
      found != levels.end() && found->price.units() == change.price.units();
  if (change.operation == L2Operation::Delete || change.quantity.units() == 0) {
    if (exists)
      levels.erase(found);
    return;
  }
  if (exists) {
    found->quantity = change.quantity;
    return;
  }
  levels.insert(found,
                L2Level{.price = change.price, .quantity = change.quantity});
}

bool apply_changes_bounded(std::vector<L2Level> &levels,
                           const std::vector<L2Change> &changes,
                           std::size_t maximum_levels) {
  for (const auto &change : changes) {
    if (change.operation == L2Operation::Delete || change.quantity.units() == 0)
      apply_change(levels, change);
  }
  for (const auto &change : changes) {
    if (change.operation == L2Operation::Delete || change.quantity.units() == 0)
      continue;
    const auto found = std::lower_bound(
        levels.begin(), levels.end(), change.price.units(),
        [](const L2Level &level, contracts::AmountUnits price_units) {
          return level.price.units() < price_units;
        });
    if ((found == levels.end() ||
         found->price.units() != change.price.units()) &&
        levels.size() == maximum_levels) {
      return false;
    }
    apply_change(levels, change);
  }
  return true;
}

} // namespace

struct L2Book::State final {
  explicit State(L2BookConfig initial_config)
      : config(std::move(initial_config)) {
    bids.reserve(config.maximum_levels_per_side);
    asks.reserve(config.maximum_levels_per_side);
    scratch_bids.reserve(config.maximum_levels_per_side);
    scratch_asks.reserve(config.maximum_levels_per_side);
    validation_prices.reserve(config.maximum_changes_per_delta);
  }

  L2BookConfig config;
  std::vector<L2Level> bids;
  std::vector<L2Level> asks;
  std::vector<L2Level> scratch_bids;
  std::vector<L2Level> scratch_asks;
  std::vector<contracts::AmountUnits> validation_prices;
  std::uint64_t transition_sequence{};
};

L2Book::L2Book(std::unique_ptr<State> state) : state_(std::move(state)) {}
L2Book::L2Book(L2Book &&) noexcept = default;
L2Book &L2Book::operator=(L2Book &&) noexcept = default;
L2Book::~L2Book() = default;

std::optional<L2Book> L2Book::create(L2BookConfig config) {
  if (config.maximum_levels_per_side == 0 ||
      config.maximum_changes_per_delta == 0)
    return std::nullopt;
  return L2Book(std::make_unique<State>(std::move(config)));
}

L2TransitionResult L2Book::apply_snapshot(const L2Snapshot &snapshot) {
  if (snapshot.listing_id != state_->config.listing_id)
    return {.failure = L2TransitionFailure::WrongListing};
  if (snapshot.bids.size() > state_->config.maximum_levels_per_side ||
      snapshot.asks.size() > state_->config.maximum_levels_per_side) {
    return {.failure = L2TransitionFailure::ResourceLimitExceeded};
  }
  if (state_->transition_sequence == std::numeric_limits<std::uint64_t>::max())
    return {.failure = L2TransitionFailure::ResourceLimitExceeded};
  state_->scratch_bids.assign(snapshot.bids.begin(), snapshot.bids.end());
  state_->scratch_asks.assign(snapshot.asks.begin(), snapshot.asks.end());
  const auto bid_failure =
      validate_levels(state_->config, state_->scratch_bids);
  if (bid_failure != L2TransitionFailure::None)
    return {.failure = bid_failure};
  const auto ask_failure =
      validate_levels(state_->config, state_->scratch_asks);
  if (ask_failure != L2TransitionFailure::None)
    return {.failure = ask_failure};
  const bool changed = state_->scratch_bids != state_->bids ||
                       state_->scratch_asks != state_->asks;
  state_->bids.swap(state_->scratch_bids);
  state_->asks.swap(state_->scratch_asks);
  ++state_->transition_sequence;
  return {.content_changed = changed,
          .transition_sequence = state_->transition_sequence};
}

L2TransitionResult L2Book::apply_delta(const L2Delta &delta) {
  if (delta.listing_id != state_->config.listing_id)
    return {.failure = L2TransitionFailure::WrongListing};
  if (state_->transition_sequence == std::numeric_limits<std::uint64_t>::max())
    return {.failure = L2TransitionFailure::ResourceLimitExceeded};
  const auto maximum_changes = state_->config.maximum_changes_per_delta;
  if (delta.bid_changes.size() > maximum_changes ||
      delta.ask_changes.size() > maximum_changes - delta.bid_changes.size()) {
    return {.failure = L2TransitionFailure::ResourceLimitExceeded};
  }
  const auto bid_failure = validate_changes(state_->config, delta.bid_changes,
                                            state_->validation_prices);
  if (bid_failure != L2TransitionFailure::None)
    return {.failure = bid_failure};
  const auto ask_failure = validate_changes(state_->config, delta.ask_changes,
                                            state_->validation_prices);
  if (ask_failure != L2TransitionFailure::None)
    return {.failure = ask_failure};

  state_->scratch_bids.assign(state_->bids.begin(), state_->bids.end());
  state_->scratch_asks.assign(state_->asks.begin(), state_->asks.end());
  if (!apply_changes_bounded(state_->scratch_bids, delta.bid_changes,
                             state_->config.maximum_levels_per_side) ||
      !apply_changes_bounded(state_->scratch_asks, delta.ask_changes,
                             state_->config.maximum_levels_per_side)) {
    return {.failure = L2TransitionFailure::ResourceLimitExceeded};
  }
  const bool changed = state_->scratch_bids != state_->bids ||
                       state_->scratch_asks != state_->asks;
  state_->bids.swap(state_->scratch_bids);
  state_->asks.swap(state_->scratch_asks);
  ++state_->transition_sequence;
  return {.content_changed = changed,
          .transition_sequence = state_->transition_sequence};
}

contracts::ListingId L2Book::listing_id() const noexcept {
  return state_->config.listing_id;
}

std::span<const L2Level> L2Book::bids() const noexcept { return state_->bids; }
std::span<const L2Level> L2Book::asks() const noexcept { return state_->asks; }

std::uint64_t L2Book::transition_sequence() const noexcept {
  return state_->transition_sequence;
}

L2StorageProfile L2Book::storage_profile() const noexcept {
  return {
      .bid_capacity = state_->bids.capacity(),
      .ask_capacity = state_->asks.capacity(),
      .scratch_bid_capacity = state_->scratch_bids.capacity(),
      .scratch_ask_capacity = state_->scratch_asks.capacity(),
      .validation_capacity = state_->validation_prices.capacity(),
  };
}

} // namespace chronos::core::market_state
