#pragma once
#include "chronos/core/market_state/listing_view_publisher.hpp"
#include "chronos/core/recommendation/recommendation.hpp"
#include "chronos/runtime/datasets/replay.hpp"
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace chronos::applications::replay_runner {
// Versioned, fixed BTCUSDT linear-public-data profile; accounting downstream is
// quantity/quote paper accounting, not perpetual margin or funding accounting.
struct MarketReplayProfile final {
  contracts::VersionRef profile_version;
  contracts::RunId run_id;
  contracts::IntegrityId run_manifest_integrity_id;
  contracts::IntegrityId replay_evidence_id;
  contracts::PortfolioId portfolio_id;
  contracts::AccountId account_id;
  contracts::ListingId listing_id;
  contracts::CanonicalInstrumentId instrument_id;
  contracts::VersionRef price_definition;
  contracts::VersionRef quantity_definition;
  // View prices count 0.10 USDT ticks (scale 1), quantities count 0.001 BTC
  // steps (scale 3). Downstream exposure uses scale 6: one lot = 1000 units.
  contracts::DecimalScale price_scale;
  contracts::DecimalScale quantity_scale;
  contracts::DecimalScale exposure_scale;
  contracts::AmountUnits price_tick_units;
  contracts::AmountUnits quantity_step_units;
  bool synthetic_fixture{};
};
struct MarketReplayCut final {
  const MarketReplayProfile &profile;
  const core::recommendation::TradeRecommendation &recommendation;
  const core::market_state::ListingStateView &view;
  const core::market_state::StateViewBundle &bundle;
  std::uint64_t capture_sequence;
  std::uint64_t run_input_sequence;
  std::int64_t logical_time_nanoseconds;
};
using MarketReplayCallback = std::function<bool(const MarketReplayCut &)>;
struct MarketReplayResult final {
  bool completed{};
  std::string error;
  std::optional<runtime::datasets::ReplayRunManifest> manifest;
  std::optional<MarketReplayProfile> profile;
  std::uint64_t capture_records{}, normalized_book_facts{},
      normalized_trade_facts{};
  std::uint64_t trade_observations_without_continuity{}, control_frames{},
      unsupported_frames{};
  std::uint64_t run_inputs{}, published_views{}, evaluations{}, abstentions{},
      recommendations{}, holds{};
  bool recommendation_cardinality_proven{};
  std::optional<core::market_state::ListingStateView> final_view;
  std::optional<core::market_state::StateViewBundle> final_bundle;
  contracts::Sha256Digest semantic_checksum{};
};
// Callback is synchronous; references are valid only during invocation. False
// stops replay visibly. State is stored in memory for this invocation only;
// this application does not claim crash recovery or durable publication.
[[nodiscard]] MarketReplayResult
run_market_replay(const std::filesystem::path &dataset,
                  const MarketReplayCallback &callback);
} // namespace chronos::applications::replay_runner
