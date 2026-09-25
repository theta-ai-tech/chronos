#include "chronos/applications/replay_runner/paper_replay.hpp"
#include "microtest.hpp"

#include <filesystem>

namespace {
namespace app = chronos::applications::replay_runner;

class Fixture final {
public:
  Fixture() {
    static std::uint64_t sequence{};
    path_ = std::filesystem::temp_directory_path() /
            ("chronos-m6-paper-loop-" + std::to_string(++sequence));
    std::filesystem::remove_all(path_);
    std::filesystem::remove_all(path_.string() + ".partial");
    std::string error;
    if (!app::write_synthetic_paper_replay_fixture(path_, error))
      throw std::runtime_error(error);
  }

  ~Fixture() { std::filesystem::remove_all(path_); }

  [[nodiscard]] const std::filesystem::path &path() const noexcept {
    return path_;
  }

private:
  std::filesystem::path path_;
};
} // namespace

TEST_CASE(
    "paper replay crosses exposure and derives deterministic ledger PnL") {
  Fixture fixture;
  const auto first = app::run_paper_replay(fixture.path());
  const auto second = app::run_paper_replay(fixture.path());

  CHECK(first.completed);
  CHECK(first.error.empty());
  CHECK(first.targets >= 2);
  CHECK(first.risk_decisions == first.targets);
  CHECK(first.reservations == first.risk_decisions);
  CHECK(first.intents == first.reservations);
  CHECK(first.fills == first.intents);
  CHECK(first.ledger_transactions == first.fills);
  CHECK(first.ledger_balanced);
  CHECK(first.ledger_idempotent);
  CHECK(first.reservation_capacity_reconciled);
  CHECK(first.position_units == -750000);
  CHECK(first.realized_gross_units != 0);
  CHECK(first.total_net_units != 0);
  CHECK(first.last_target_id.has_value());
  CHECK(first.last_risk_decision_id.has_value());
  CHECK(first.last_reservation_id.has_value());
  CHECK(first.last_intent_id.has_value());
  CHECK(first.last_fill_id.has_value());
  CHECK(first.last_transaction_id.has_value());
  CHECK(first.final_mark_view_id.has_value());
  CHECK(first.mark_policy_version.has_value());
  CHECK(first.valuation_policy_version.has_value());
  CHECK(first.closing_events.has_value());
  CHECK(*first.closing_events > 0);
  CHECK(first.fees_units > 0);
  CHECK(first.semantic_checksum == second.semantic_checksum);
  CHECK(app::paper_replay_json(first) == app::paper_replay_json(second));
}

TEST_CASE("missing market risk evidence cannot reach broker or ledger") {
  Fixture fixture;
  const auto result = app::run_paper_replay(
      fixture.path(), {.provide_market_risk_evidence = false});

  CHECK(result.completed);
  CHECK(result.targets > 0);
  CHECK(result.risk_decisions == 0);
  CHECK(result.risk_unavailable == result.targets);
  CHECK(result.reservations == 0);
  CHECK(result.intents == 0);
  CHECK(result.fills == 0);
  CHECK(result.ledger_transactions == 0);
  CHECK(result.position_units == 0);
}
