#include "chronos/applications/replay_runner/paper_replay.hpp"

#include "chronos/accounting/ledger.hpp"
#include "chronos/accounting/valuation.hpp"
#include "chronos/adapters/market_data/capture_dataset.hpp"
#include "chronos/adapters/paper/paper_broker.hpp"
#include "chronos/core/execution_planning/paper_intent.hpp"
#include "chronos/core/portfolio/portfolio_construction.hpp"
#include "chronos/core/risk/reservation.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string_view>
#include <variant>
#include <vector>

namespace chronos::applications::replay_runner {
namespace {
namespace accounting = chronos::accounting;
namespace adapter = chronos::adapters::market_data;
namespace paper = chronos::adapters::paper;
namespace sdk = chronos::adapters::sdk;
namespace execution = chronos::core::execution_planning;
namespace market = chronos::core::market_state;
namespace portfolio = chronos::core::portfolio;
namespace risk = chronos::core::risk;
namespace contracts = chronos::contracts;

template <typename Id> Id seeded_id(std::uint8_t seed) {
  typename Id::bytes_type bytes{};
  bytes.front() = seed;
  return Id::from_bytes(bytes).value();
}

contracts::VersionRef version(std::uint8_t seed) {
  return contracts::VersionRef::from(seeded_id<contracts::DefinitionId>(seed),
                                     1)
      .value();
}

template <typename Id, typename SourceId>
Id converted_id(const SourceId &source) {
  return Id::from_bytes(source.bytes()).value();
}

contracts::DataQuality valid_quality() {
  return contracts::DataQuality::from(contracts::QualityStatus::valid, 0)
      .value();
}

contracts::DataQuality unavailable_quality() {
  return contracts::DataQuality::from(contracts::QualityStatus::invalid, 1)
      .value();
}

bool tradeable(const market::ListingStateView &view) {
  return view.top.best_bid && view.top.best_ask &&
         view.top.best_bid->price.units() > 0 &&
         view.top.best_ask->price.units() >= view.top.best_bid->price.units() &&
         view.top.shape != market::L2BookShape::Crossed &&
         view.top.shape != market::L2BookShape::OneSided &&
         view.top.shape != market::L2BookShape::Empty &&
         view.top.shape != market::L2BookShape::Unknown &&
         view.quality.book_synchronization ==
             market::BookSynchronization::Synchronized &&
         view.quality.book_freshness == market::FreshnessStatus::Fresh;
}

portfolio::TargetKey target_key(const MarketReplayProfile &profile) {
  return portfolio::TargetKey(profile.portfolio_id, profile.account_id,
                              profile.instrument_id, profile.listing_id,
                              version(160));
}

risk::MinimalRiskPolicy risk_policy(const MarketReplayProfile &profile) {
  return risk::MinimalRiskPolicy(
      seeded_id<contracts::RiskScopeId>(161), target_key(profile),
      profile.run_id, contracts::RunMode::backtest, version(162), version(163),
      version(164), version(165), version(166), version(167),
      profile.exposure_scale, risk::RiskExposureDimensionSet::QuantityOnlyV1,
      750000, 1000000000, 1000000000, 1000000000, 1000000000, 1000000000, true);
}

risk::ReservationPolicy reservation_policy(const MarketReplayProfile &profile) {
  return {risk_policy(profile), version(168),
          seeded_id<contracts::StreamId>(169), 1, 750000};
}

accounting::LedgerPolicy ledger_policy(const MarketReplayProfile &profile) {
  return {profile.run_id,
          profile.portfolio_id,
          profile.account_id,
          profile.instrument_id,
          profile.listing_id,
          version(168),
          contracts::RunMode::backtest,
          seeded_id<contracts::StreamId>(169),
          1,
          version(170),
          profile.exposure_scale,
          contracts::DecimalScale::from_exponent(6).value(),
          contracts::RoundingMode::toward_positive,
          contracts::RoundingMode::toward_positive};
}

portfolio::PortfolioConstructionPolicy
portfolio_policy(const MarketReplayProfile &profile,
                 contracts::StrategyInstanceId strategy_id) {
  return portfolio::PortfolioConstructionPolicy(
      version(167), version(171), version(172), version(173),
      target_key(profile), profile.run_id, {strategy_id},
      profile.exposure_scale, 1000, 64, 1000000000, 1000000000);
}

std::optional<contracts::AmountUnits>
midpoint(const market::ListingStateView &view) {
  if (!view.top.best_bid || !view.top.best_ask)
    return {};
  const auto bid = view.top.best_bid->price.units();
  const auto ask = view.top.best_ask->price.units();
  contracts::AmountUnits spread{};
  if (bid <= 0 || ask < bid || __builtin_sub_overflow(ask, bid, &spread))
    return {};
  return bid + spread / 2;
}

bool balanced(const accounting::LedgerTransaction &transaction) {
  __int128 quantity{};
  __int128 money{};
  for (const auto &entry : transaction.entries) {
    if (entry.unit == accounting::LedgerUnit::BaseQuantity)
      quantity += entry.amount_units;
    else if (entry.unit == accounting::LedgerUnit::QuoteCurrency)
      money += entry.amount_units;
    else
      return false;
  }
  return quantity == 0 && money == 0;
}

bool settlement_matches_ledger_tail(
    const contracts::SettledExposureEvidence &settlement,
    const accounting::LedgerAuthority &ledger,
    const contracts::PaperFill &fill) {
  if (ledger.transactions().empty())
    return false;
  const auto &tail = ledger.transactions().back();
  const auto &policy = tail.policy;
  const auto &facts = fill.facts();
  return tail.kind == accounting::LedgerTransactionKind::Fill &&
         tail.source_fill == facts && settlement.run_id == policy.run_id &&
         settlement.portfolio_id == policy.portfolio_id &&
         settlement.account_id == policy.account_id &&
         settlement.canonical_instrument_id == policy.canonical_instrument_id &&
         settlement.listing_id == policy.listing_id &&
         settlement.quote_currency == policy.quote_currency &&
         settlement.run_mode == policy.run_mode &&
         settlement.reservation_id == facts.intent.reservation_id &&
         settlement.intent_id == facts.intent.intent_id &&
         settlement.fill_id == facts.fill_id &&
         settlement.transaction_id == tail.transaction_id &&
         settlement.ledger_cursor == tail.cursor &&
         settlement.ledger_cursor == ledger.cursor() &&
         settlement.ledger_checksum == tail.checksum &&
         settlement.ledger_checksum == ledger.checksum() &&
         settlement.posted_delta_units == tail.signed_fill_quantity_units &&
         settlement.position_units == ledger.position_units() &&
         settlement.exposure_scale == policy.quantity_scale &&
         settlement.quality == facts.intent.execution.quality &&
         settlement.run_input_sequence ==
             facts.intent.execution.run_input_sequence &&
         settlement.logical_time_nanoseconds == facts.fill_time_nanoseconds;
}

class PaperReplaySession final {
public:
  PaperReplaySession(const MarketReplayProfile &profile,
                     PaperReplayOptions options, PaperReplayResult &result)
      : profile_(profile), options_(options), result_(result),
        reservations_(std::make_unique<risk::ReservationAuthority>(
            reservation_policy(profile), 0)),
        ledger_(std::make_unique<accounting::LedgerAuthority>(
            ledger_policy(profile))) {}

  bool accept(const MarketReplayCut &cut) {
    if (cut.recommendation.hold())
      return true;
    if (!cut.recommendation.actionable())
      return fail("non-terminal recommendation reached paper loop");

    const portfolio::PortfolioConstructionCut target_cut(
        cut.run_input_sequence, cut.logical_time_nanoseconds);
    const portfolio::PortfolioStateSnapshot snapshot(
        converted_id<contracts::PortfolioSnapshotId>(
            cut.recommendation.recommendation_id()),
        profile_.run_id, profile_.portfolio_id, profile_.account_id,
        profile_.instrument_id, profile_.listing_id, ledger_->position_units(),
        profile_.exposure_scale, cut.run_input_sequence,
        cut.logical_time_nanoseconds, cut.view.configuration_epoch,
        portfolio::PortfolioSnapshotDisposition::FreshComplete, true);
    const std::array recommendations{cut.recommendation};
    auto constructed = portfolio::PortfolioConstructionAuthority::construct(
        recommendations, snapshot,
        portfolio_policy(profile_, cut.recommendation.strategy_instance_id()),
        target_cut);
    if (!constructed.completed())
      return fail("portfolio construction failed");
    if (std::holds_alternative<portfolio::PortfolioNoChange>(
            *constructed.terminal)) {
      ++result_.no_change_targets;
      return true;
    }
    if (!std::holds_alternative<portfolio::TargetPosition>(
            *constructed.terminal))
      return fail("portfolio construction rejected replay recommendation");
    auto target =
        std::get<portfolio::TargetPosition>(std::move(*constructed.terminal));
    ++result_.targets;
    result_.last_target_id = target.target_position_id();

    const risk::RiskEvaluationCut risk_cut(cut.run_input_sequence,
                                           cut.logical_time_nanoseconds);
    const auto publication = converted_id<contracts::PublicationAttemptId>(
        target.target_position_id());
    const risk::TargetAdmissionEvidence admission(
        risk::TargetPublicationFact(publication, target.target_position_id(),
                                    risk::TargetPublicationState::Published,
                                    cut.run_input_sequence,
                                    cut.logical_time_nanoseconds),
        risk::TargetAcknowledgementFact(
            seeded_id<contracts::ConsumerBoundaryId>(174), publication,
            target.target_position_id(),
            risk::TargetAcknowledgementState::Acknowledged,
            cut.run_input_sequence, cut.logical_time_nanoseconds),
        risk::TargetLifecycleFact(
            converted_id<contracts::EventId>(target.target_position_id()),
            target.target_position_id(), risk::TargetLifecycleState::Active,
            cut.run_input_sequence, cut.logical_time_nanoseconds));
    const auto quality =
        tradeable(cut.view) ? valid_quality() : unavailable_quality();
    std::optional<risk::MarketRiskSnapshot> market_evidence;
    if (options_.provide_market_risk_evidence) {
      market_evidence.emplace(
          cut.view.view_id, profile_.run_id, profile_.instrument_id,
          profile_.listing_id, quality, cut.run_input_sequence,
          cut.logical_time_nanoseconds, tradeable(cut.view));
    }
    const auto policy = risk_policy(profile_);
    const risk::RiskEvaluationEvidence evidence(
        risk::RunRiskContext(
            profile_.run_manifest_integrity_id, profile_.run_id,
            contracts::RunMode::backtest, risk::RiskReplayClass::Faithful,
            profile_.replay_evidence_id, cut.view.configuration_epoch,
            cut.run_input_sequence, cut.logical_time_nanoseconds,
            valid_quality()),
        risk::RiskPolicyActivation(
            seeded_id<contracts::EventId>(175), profile_.run_id,
            policy.risk_scope_id(), cut.view.configuration_epoch,
            policy.risk_policy_version(), policy.limit_set_version(),
            policy.exposure_model_version(), policy.arithmetic_version(),
            policy.authority_version(), cut.run_input_sequence,
            cut.logical_time_nanoseconds, valid_quality()),
        risk::AccountRiskSnapshot(
            converted_id<contracts::StateViewId>(snapshot.snapshot_id()),
            profile_.run_id, profile_.portfolio_id, profile_.account_id,
            ledger_->position_units(), 1000000000000, profile_.exposure_scale,
            valid_quality(), cut.run_input_sequence,
            cut.logical_time_nanoseconds, true),
        market_evidence, reservations_->snapshot(risk_cut),
        risk::KillSwitchSnapshot(seeded_id<contracts::StateViewId>(176),
                                 policy.risk_scope_id(), valid_quality(),
                                 cut.run_input_sequence,
                                 cut.logical_time_nanoseconds, false));
    auto evaluated = risk::MinimalRiskAuthority::evaluate(
        target, policy, risk_cut, admission, evidence);
    if (!evaluated.completed())
      return fail("risk evaluation failed");
    if (!std::holds_alternative<risk::RiskDecision>(*evaluated.terminal)) {
      ++result_.risk_unavailable;
      return true;
    }
    auto decision =
        std::get<risk::RiskDecision>(std::move(*evaluated.terminal));
    if (!decision.authorizes_target()) {
      ++result_.risk_unavailable;
      return true;
    }
    ++result_.risk_decisions;
    result_.last_risk_decision_id = decision.decision_id();

    const auto request =
        converted_id<contracts::ReservationRequestId>(decision.decision_id());
    auto reserved = reservations_->reserve(request, decision, risk_cut);
    if (!reserved.accepted() || !reserved.reservation())
      return fail("reservation authority rejected authorized risk decision");
    ++result_.reservations;
    result_.last_reservation_id = reserved.reservation()->reservation_id();

    std::int64_t execution_expiry{};
    if (__builtin_add_overflow(cut.logical_time_nanoseconds,
                               INT64_C(1000000000), &execution_expiry))
      return fail("paper execution expiry overflow");
    if (!cut.view.top.best_bid || !cut.view.top.best_ask)
      return fail("accepted recommendation has no executable market");
    const contracts::PaperExecutionEvidence execution_evidence{
        profile_.run_id,
        profile_.portfolio_id,
        profile_.account_id,
        profile_.instrument_id,
        profile_.listing_id,
        version(168),
        contracts::RunMode::backtest,
        cut.view.view_id,
        cut.view.lineage,
        cut.run_input_sequence,
        cut.logical_time_nanoseconds,
        execution_expiry,
        quality,
        true,
        tradeable(cut.view),
        version(177),
        version(178),
        profile_.price_scale,
        profile_.exposure_scale,
        contracts::DecimalScale::from_exponent(6).value(),
        cut.view.top.best_bid->price.units(),
        cut.view.top.best_ask->price.units(),
        profile_.price_tick_units,
        1000};
    const auto intent_id = converted_id<contracts::ExecutableOrderIntentId>(
        decision.decision_id());
    auto intent = intents_.create(intent_id, decision, *reservations_,
                                  reserved.reservation()->reservation_id(),
                                  execution_evidence, risk_cut);
    if (!intent.accepted())
      return fail("paper intent authority rejected active reservation");
    ++result_.intents;
    result_.last_intent_id = intent.intent->facts().intent_id;

    const contracts::PaperBrokerPolicy broker_policy{
        execution_evidence.broker_model_version, 0, 5, 1, 2};
    auto fill =
        broker_.submit(*intent.intent, execution_evidence, broker_policy);
    if (!fill.accepted())
      return fail("paper broker rejected executable intent");
    ++result_.fills;
    result_.last_fill_id = fill.fill->facts().fill_id;

    auto posted = ledger_->post(*fill.fill);
    if (!posted.accepted() || !posted.settlement)
      return fail("ledger rejected paper fill");
    const auto transaction_count = ledger_->transactions().size();
    const auto retried = ledger_->post(*fill.fill);
    if (retried != posted ||
        ledger_->transactions().size() != transaction_count)
      return fail("ledger exact retry was not idempotent");
    result_.ledger_idempotent = true;
    ++result_.ledger_transactions;
    result_.last_transaction_id = posted.transaction_id;
    auto settlement = *posted.settlement;
    if (options_.corrupt_settlement_evidence_for_test)
      settlement.ledger_checksum = {};
    if (!settlement_matches_ledger_tail(settlement, *ledger_, *fill.fill))
      return fail("ledger settlement did not match owned ledger tail");
    const auto &tail = ledger_->transactions().back();
    result_.chain_linked =
        decision.target_position_id() == target.target_position_id() &&
        reserved.reservation()->decision().decision_id() ==
            decision.decision_id() &&
        intent.intent->facts().target_position_id ==
            target.target_position_id() &&
        intent.intent->facts().risk_decision_id == decision.decision_id() &&
        intent.intent->facts().reservation_id ==
            reserved.reservation()->reservation_id() &&
        fill.fill->facts().intent == intent.intent->facts() &&
        tail.source_fill == fill.fill->facts() &&
        settlement.transaction_id == *posted.transaction_id;
    if (!result_.chain_linked)
      return fail("paper authority chain linkage failed");
    auto reconciled = reservations_->reconcile(
        reserved.reservation()->reservation_id(), settlement);
    if (!reconciled.accepted())
      return fail("reservation reconciliation rejected ledger settlement");
    const auto projected = reservations_->snapshot(risk_cut);
    if (projected.worst_case_exposure_before_target_units() !=
        ledger_->position_units())
      return fail("reservation capacity did not reconcile to ledger position");
    result_.reservation_capacity_reconciled = true;
    return true;
  }

  bool finalize(const MarketReplayResult &market_result) {
    if (!market_result.final_view || !market_result.final_bundle)
      return fail("market replay completed without a final accepted view");
    const auto mark_price = midpoint(*market_result.final_view);
    if (!mark_price)
      return fail("final accepted view has no valid mark");
    const auto quality = tradeable(*market_result.final_view)
                             ? valid_quality()
                             : unavailable_quality();
    const contracts::PositionMark mark{
        profile_.run_id,
        profile_.portfolio_id,
        profile_.account_id,
        profile_.instrument_id,
        profile_.listing_id,
        version(168),
        contracts::RunMode::backtest,
        market_result.final_view->view_id,
        market_result.final_view->lineage,
        profile_.price_definition,
        *mark_price,
        profile_.price_scale,
        profile_.exposure_scale,
        contracts::DecimalScale::from_exponent(6).value(),
        quality,
        market_result.final_bundle->run_input_sequence,
        market_result.final_bundle->logical_time_nanoseconds,
        version(179)};
    const accounting::ValuationPolicy valuation_policy{
        ledger_policy(profile_), version(180), profile_.price_definition,
        profile_.price_scale,    version(179), 1000000000};
    const auto valued = accounting::value_position(
        ledger_->transactions(), mark, valuation_policy,
        {market_result.final_bundle->run_input_sequence,
         market_result.final_bundle->logical_time_nanoseconds});
    if (!valued.available()) {
      result_.valuation_status = PaperValuationStatus::Unavailable;
      return fail("ledger-derived valuation unavailable");
    }
    result_.ledger_balanced =
        std::all_of(ledger_->transactions().begin(),
                    ledger_->transactions().end(), balanced);
    if (!result_.ledger_balanced)
      return fail("ledger transaction balance invariant failed");
    result_.position_units = valued.position->position_units;
    result_.realized_gross_units = valued.position->realized_gross_units;
    result_.unrealized_gross_units = *valued.unrealized_gross_units;
    result_.fees_units = valued.position->fees_units;
    result_.total_net_units = *valued.total_net_units;
    result_.valuation_status = PaperValuationStatus::Available;
    if (valued.position->closing_events != 0) {
      result_.profitable_closing_events =
          valued.position->profitable_gross_closing_events;
      result_.closing_events = valued.position->closing_events;
    }
    result_.ledger_checksum = ledger_->checksum();
    result_.final_mark_view_id = mark.source_view_id;
    result_.mark_policy_version = mark.mark_policy_version;
    result_.valuation_policy_version =
        valuation_policy.valuation_policy_version;
    return true;
  }

private:
  bool fail(std::string message) {
    if (result_.error.empty())
      result_.error = std::move(message);
    return false;
  }

  MarketReplayProfile profile_;
  PaperReplayOptions options_;
  PaperReplayResult &result_;
  std::unique_ptr<risk::ReservationAuthority> reservations_;
  execution::PaperIntentAuthority intents_;
  paper::PaperBroker broker_;
  std::unique_ptr<accounting::LedgerAuthority> ledger_;
};

void append_u64(std::vector<std::byte> &bytes, std::uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

void append_optional_amount(
    std::vector<std::byte> &bytes,
    const std::optional<contracts::AmountUnits> &value) {
  append_u64(bytes, value.has_value());
  if (value)
    append_u64(bytes, static_cast<std::uint64_t>(*value));
}

template <typename Id>
void append_optional_id(std::vector<std::byte> &bytes,
                        const std::optional<Id> &value) {
  append_u64(bytes, value.has_value());
  if (value) {
    for (const auto byte : value->bytes())
      bytes.push_back(static_cast<std::byte>(byte));
  }
}

void finalize_checksum(PaperReplayResult &result) {
  std::vector<std::byte> bytes;
  bytes.reserve(256);
  for (const auto byte : result.market.semantic_checksum.bytes)
    bytes.push_back(static_cast<std::byte>(byte));
  for (const auto byte : result.ledger_checksum.bytes)
    bytes.push_back(static_cast<std::byte>(byte));
  for (const auto value :
       {result.targets, result.no_change_targets, result.risk_decisions,
        result.risk_unavailable, result.reservations, result.intents,
        result.fills, result.ledger_transactions})
    append_u64(bytes, value);
  append_u64(bytes, result.ledger_balanced);
  append_u64(bytes, result.ledger_idempotent);
  append_u64(bytes, result.reservation_capacity_reconciled);
  append_u64(bytes, result.chain_linked);
  append_u64(bytes, static_cast<std::uint64_t>(result.valuation_status));
  append_optional_amount(bytes, result.position_units);
  append_optional_amount(bytes, result.realized_gross_units);
  append_optional_amount(bytes, result.unrealized_gross_units);
  append_optional_amount(bytes, result.fees_units);
  append_optional_amount(bytes, result.total_net_units);
  append_optional_id(bytes, result.last_target_id);
  append_optional_id(bytes, result.last_risk_decision_id);
  append_optional_id(bytes, result.last_reservation_id);
  append_optional_id(bytes, result.last_intent_id);
  append_optional_id(bytes, result.last_fill_id);
  append_optional_id(bytes, result.last_transaction_id);
  append_optional_id(bytes, result.final_mark_view_id);
  if (result.mark_policy_version) {
    append_optional_id(
        bytes, std::optional{result.mark_policy_version->definition_id()});
    append_u64(bytes, result.mark_policy_version->version());
  } else {
    append_optional_id(bytes, std::optional<contracts::DefinitionId>{});
  }
  if (result.valuation_policy_version) {
    append_optional_id(
        bytes, std::optional{result.valuation_policy_version->definition_id()});
    append_u64(bytes, result.valuation_policy_version->version());
  } else {
    append_optional_id(bytes, std::optional<contracts::DefinitionId>{});
  }
  result.semantic_checksum = contracts::sha256(bytes);
}

template <typename ByteRange> std::string hex(const ByteRange &bytes) {
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const auto byte : bytes)
    output << std::setw(2) << static_cast<unsigned>(byte);
  return output.str();
}

std::string json_escape(std::string_view value) {
  std::string result;
  for (const auto character : value) {
    if (character == '"' || character == '\\')
      result.push_back('\\');
    result.push_back(character);
  }
  return result;
}

std::vector<std::byte> bytes(std::string_view value) {
  const auto *begin = reinterpret_cast<const std::byte *>(value.data());
  return {begin, begin + value.size()};
}

sdk::SourceCaptureContext fixture_context() {
  return {.adapter_id = "chronos.bybit.public-market-data",
          .adapter_version = "m6.7-fixture",
          .build_version = "m6.7-fixture",
          .venue = "bybit",
          .environment = sdk::EnvironmentClass::Test,
          .market = sdk::MarketClass::LinearPerpetual,
          .endpoint = sdk::EndpointClass::PublicMarketData,
          .trust_class = sdk::SourceTrustClass::PublicUnauthenticated,
          .capture_session_id = seeded_id<sdk::CaptureSessionId>(201),
          .runtime_id = seeded_id<contracts::RuntimeId>(202),
          .connection_id = seeded_id<sdk::SourceConnectionId>(203),
          .subscription_id = seeded_id<sdk::SourceSubscriptionId>(204),
          .capture_partition_id = seeded_id<sdk::CapturePartitionId>(205),
          .framing_version = "websocket-rfc6455-v1",
          .static_configuration_version = "m6.7-synthetic-v1",
          .capability_manifest_version = "bybit-v5-v1",
          .schema_policy_version = "bybit-v5-public-v1",
          .data_classification = sdk::DataClassification::PublicMarketData,
          .access_restriction = sdk::AccessRestriction::ChronosInternal,
          .maximum_retained_payload_bytes = 1U << 20U,
          .maximum_source_events = 16};
}

constexpr std::array<std::string_view, 2> kFixtureFrames{
    R"({"topic":"orderbook.50.BTCUSDT","type":"snapshot","ts":1672304486869,"data":{"s":"BTCUSDT","b":[["42000.00","2.000"]],"a":[["42000.50","0.100"]],"u":18521288,"seq":7961638724},"cts":1672304486868})",
    R"({"topic":"orderbook.50.BTCUSDT","type":"delta","ts":1672304486870,"data":{"s":"BTCUSDT","b":[["42000.00","0"],["41999.50","0.100"]],"a":[["42000.50","0"],["42000.00","2.000"]],"u":18521289,"seq":7961638725},"cts":1672304486869})"};
} // namespace

PaperReplayResult run_paper_replay(const std::filesystem::path &dataset,
                                   PaperReplayOptions options) {
  PaperReplayResult result;
  std::unique_ptr<PaperReplaySession> session;
  result.market = run_market_replay(dataset, [&](const MarketReplayCut &cut) {
    if (!session)
      session =
          std::make_unique<PaperReplaySession>(cut.profile, options, result);
    return session->accept(cut);
  });
  if (!result.market.completed) {
    if (result.error.empty())
      result.error = result.market.error;
    finalize_checksum(result);
    return result;
  }
  if (!session) {
    result.error = "market replay produced no recommendation cuts";
    finalize_checksum(result);
    return result;
  }
  if (!session->finalize(result.market)) {
    finalize_checksum(result);
    return result;
  }
  result.completed = true;
  finalize_checksum(result);
  return result;
}

bool write_synthetic_paper_replay_fixture(const std::filesystem::path &dataset,
                                          std::string &error) {
  if (std::filesystem::exists(dataset) ||
      std::filesystem::exists(dataset.string() + ".partial")) {
    error = "fixture destination already exists";
    return false;
  }
  const auto context = fixture_context();
  auto recorder = sdk::SourceCaptureRecorder::create(context);
  auto writer = adapter::CaptureDatasetWriter::create(dataset, context);
  if (!recorder || !writer) {
    error = "fixture recorder or dataset writer creation failed";
    return false;
  }
  for (std::size_t index = 0; index < kFixtureFrames.size(); ++index) {
    const auto payload = bytes(kFixtureFrames[index]);
    auto captured = recorder->capture(
        {.source_event_id = seeded_id<contracts::SourceEventId>(
             static_cast<std::uint8_t>(210 + index)),
         .chronos_receive_time =
             contracts::TimePoint::from(
                 static_cast<std::int64_t>(1000000 * (index + 1)),
                 seeded_id<contracts::ClockDomainId>(211),
                 contracts::ClockClass::monotonic, 1)
                 .value(),
         .raw_payload = payload,
         .framing_protocol = sdk::FramingProtocol::WebSocket,
         .frame_kind = sdk::SourceFrameKind::Text,
         .framing_status = sdk::FramingStatus::Complete,
         .integrity_status = sdk::CaptureIntegrityStatus::Complete,
         .content_encoding = sdk::ContentEncoding::Utf8Text,
         .compression_disposition =
             sdk::CompressionDisposition::NotCompressed});
    if (!captured.ok() ||
        writer->append(*captured.event) != adapter::DatasetFailure::None) {
      error = "fixture capture append failed";
      return false;
    }
  }
  const auto sealed = writer->seal();
  if (!sealed.manifest) {
    error = "fixture dataset seal failed";
    return false;
  }
  return true;
}

std::string paper_replay_json(const PaperReplayResult &result) {
  const auto optional_id = [](const auto &value) {
    return value ? "\"" + value->to_string() + "\"" : std::string("null");
  };
  const auto version_json = [&](const auto &value) {
    if (!value)
      return std::string("null");
    return "{\"definition_id\":\"" + value->definition_id().to_string() +
           "\",\"version\":" + std::to_string(value->version()) + "}";
  };
  const auto optional_amount = [](const auto &value) {
    return value ? std::to_string(*value) : std::string("null");
  };
  const auto valuation_status = [&] {
    switch (result.valuation_status) {
    case PaperValuationStatus::Available:
      return "available";
    case PaperValuationStatus::Unavailable:
      return "unavailable";
    case PaperValuationStatus::NotEvaluated:
      return "not_evaluated";
    }
    return "not_evaluated";
  };
  std::ostringstream output;
  output << "{\"schema\":\"chronos.paper-replay-summary.v1\""
         << ",\"accounting_model\":\"quantity/quote paper accounting\""
         << ",\"input_class\":\""
         << (result.market.profile && result.market.profile->synthetic_fixture
                 ? "synthetic_fixture"
                 : "verified_capture_dataset")
         << "\""
         << ",\"completed\":" << (result.completed ? "true" : "false")
         << ",\"error\":\"" << json_escape(result.error) << "\""
         << ",\"capture_records\":" << result.market.capture_records
         << ",\"run_inputs\":" << result.market.run_inputs
         << ",\"recommendations\":" << result.market.recommendations
         << ",\"targets\":" << result.targets
         << ",\"risk_decisions\":" << result.risk_decisions
         << ",\"risk_unavailable\":" << result.risk_unavailable
         << ",\"reservations\":" << result.reservations
         << ",\"intents\":" << result.intents << ",\"fills\":" << result.fills
         << ",\"ledger_transactions\":" << result.ledger_transactions
         << ",\"ledger_balanced\":"
         << (result.ledger_balanced ? "true" : "false")
         << ",\"ledger_idempotent\":"
         << (result.ledger_idempotent ? "true" : "false")
         << ",\"reservation_capacity_reconciled\":"
         << (result.reservation_capacity_reconciled ? "true" : "false")
         << ",\"chain_linked\":" << (result.chain_linked ? "true" : "false")
         << ",\"valuation_status\":\"" << valuation_status() << "\""
         << ",\"position_units\":" << optional_amount(result.position_units)
         << ",\"realized_gross_units\":"
         << optional_amount(result.realized_gross_units)
         << ",\"unrealized_gross_units\":"
         << optional_amount(result.unrealized_gross_units)
         << ",\"fees_units\":" << optional_amount(result.fees_units)
         << ",\"total_net_units\":" << optional_amount(result.total_net_units)
         << ",\"hit_rate_numerator\":";
  if (result.profitable_closing_events)
    output << *result.profitable_closing_events;
  else
    output << "null";
  output << ",\"hit_rate_denominator\":";
  if (result.closing_events)
    output << *result.closing_events;
  else
    output << "null";
  output
      << ",\"ledger_checksum\":\"" << hex(result.ledger_checksum.bytes) << "\""
      << ",\"last_target_id\":" << optional_id(result.last_target_id)
      << ",\"last_risk_decision_id\":"
      << optional_id(result.last_risk_decision_id)
      << ",\"last_reservation_id\":" << optional_id(result.last_reservation_id)
      << ",\"last_intent_id\":" << optional_id(result.last_intent_id)
      << ",\"last_fill_id\":" << optional_id(result.last_fill_id)
      << ",\"last_transaction_id\":" << optional_id(result.last_transaction_id)
      << ",\"final_mark_view_id\":" << optional_id(result.final_mark_view_id)
      << ",\"mark_policy_version\":" << version_json(result.mark_policy_version)
      << ",\"valuation_policy_version\":"
      << version_json(result.valuation_policy_version)
      << ",\"semantic_checksum\":\"" << hex(result.semantic_checksum.bytes)
      << "\"}";
  return output.str();
}

} // namespace chronos::applications::replay_runner
