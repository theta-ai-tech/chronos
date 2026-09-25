#pragma once

#include "chronos/applications/replay_runner/market_replay.hpp"
#include "chronos/contracts/paper_execution.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace chronos::applications::replay_runner {

struct PaperReplayOptions final {
  // Used by fail-closed integration tests. The CLI always uses complete risk
  // evidence.
  bool provide_market_risk_evidence{true};
};

struct PaperReplayResult final {
  bool completed{};
  std::string error;
  MarketReplayResult market;
  std::uint64_t targets{};
  std::uint64_t no_change_targets{};
  std::uint64_t risk_decisions{};
  std::uint64_t risk_unavailable{};
  std::uint64_t reservations{};
  std::uint64_t intents{};
  std::uint64_t fills{};
  std::uint64_t ledger_transactions{};
  bool ledger_balanced{};
  bool ledger_idempotent{};
  bool reservation_capacity_reconciled{};
  contracts::AmountUnits position_units{};
  contracts::AmountUnits realized_gross_units{};
  contracts::AmountUnits unrealized_gross_units{};
  contracts::AmountUnits fees_units{};
  contracts::AmountUnits total_net_units{};
  std::optional<std::uint64_t> profitable_closing_events;
  std::optional<std::uint64_t> closing_events;
  std::optional<contracts::TargetPositionId> last_target_id;
  std::optional<contracts::RiskDecisionId> last_risk_decision_id;
  std::optional<contracts::ReservationId> last_reservation_id;
  std::optional<contracts::ExecutableOrderIntentId> last_intent_id;
  std::optional<contracts::PaperFillId> last_fill_id;
  std::optional<contracts::LedgerTransactionId> last_transaction_id;
  std::optional<contracts::StateViewId> final_mark_view_id;
  std::optional<contracts::VersionRef> mark_policy_version;
  std::optional<contracts::VersionRef> valuation_policy_version;
  contracts::Sha256Digest ledger_checksum{};
  contracts::Sha256Digest semantic_checksum{};
};

[[nodiscard]] PaperReplayResult
run_paper_replay(const std::filesystem::path &dataset,
                 PaperReplayOptions options = {});

// Writes a small deterministic captured dataset with opposing book imbalances.
// It is synthetic fixture evidence, not a live venue capture.
[[nodiscard]] bool
write_synthetic_paper_replay_fixture(const std::filesystem::path &dataset,
                                     std::string &error);

[[nodiscard]] std::string paper_replay_json(const PaperReplayResult &result);

} // namespace chronos::applications::replay_runner
