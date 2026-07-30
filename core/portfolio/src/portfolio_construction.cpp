#include "chronos/core/portfolio/portfolio_construction.hpp"

#include "chronos/contracts/digest.hpp"

#include <algorithm>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>

namespace chronos::core::portfolio {
namespace {

template <typename Id>
void append_id(std::vector<std::byte> &output, const Id &value) {
  for (const auto byte : value.bytes())
    output.push_back(static_cast<std::byte>(byte));
}

template <typename Integer>
void append_integer(std::vector<std::byte> &output, Integer value) {
  using Unsigned = std::make_unsigned_t<Integer>;
  const auto converted = static_cast<Unsigned>(value);
  for (std::size_t index = 0; index < sizeof(Integer); ++index) {
    const auto shift = (sizeof(Integer) - index - 1) * 8U;
    output.push_back(
        static_cast<std::byte>((converted >> shift) & Unsigned{0xFF}));
  }
}

template <typename Enum>
void append_enum(std::vector<std::byte> &output, Enum value) {
  append_integer(output, static_cast<std::underlying_type_t<Enum>>(value));
}

void append_domain(std::vector<std::byte> &output, std::string_view domain) {
  for (const auto character : domain)
    output.push_back(static_cast<std::byte>(character));
}

void append_version(std::vector<std::byte> &output,
                    contracts::VersionRef value) {
  append_id(output, value.definition_id());
  append_integer(output, value.version());
}

void append_key(std::vector<std::byte> &output, const TargetKey &key) {
  append_id(output, key.portfolio_id());
  append_id(output, key.account_id());
  append_id(output, key.canonical_instrument_id());
  append_id(output, key.listing_id());
  append_version(output, key.target_policy_version());
}

void append_snapshot(std::vector<std::byte> &output,
                     const PortfolioStateSnapshot &snapshot) {
  append_id(output, snapshot.snapshot_id());
  append_id(output, snapshot.run_id());
  append_id(output, snapshot.portfolio_id());
  append_id(output, snapshot.account_id());
  append_id(output, snapshot.canonical_instrument_id());
  append_id(output, snapshot.listing_id());
  append_integer(output, snapshot.current_exposure_units());
  append_integer(output, snapshot.exposure_scale().exponent());
  append_integer(output, snapshot.run_input_sequence());
  append_integer(output, snapshot.logical_time_nanoseconds());
  append_integer(output, snapshot.configuration_epoch());
  append_enum(output, snapshot.disposition());
  append_integer<std::uint8_t>(output, snapshot.paper_transition_assumption());
}

void append_policy(std::vector<std::byte> &output,
                   const PortfolioConstructionPolicy &policy) {
  append_version(output, policy.target_schema_version());
  append_version(output, policy.sizing_policy_version());
  append_version(output, policy.aggregation_policy_version());
  append_version(output, policy.authority_version());
  append_key(output, policy.target_key());
  append_id(output, policy.run_id());
  auto assignments = std::vector<contracts::StrategyInstanceId>(
      policy.assigned_strategy_ids().begin(),
      policy.assigned_strategy_ids().end());
  std::sort(assignments.begin(), assignments.end());
  append_integer(output, static_cast<std::uint64_t>(assignments.size()));
  for (const auto assignment : assignments)
    append_id(output, assignment);
  append_integer(output, policy.exposure_scale().exponent());
  append_integer(output, static_cast<std::uint64_t>(
                             policy.maximum_selected_recommendations()));
  append_integer(output,
                 policy.recommendation_maximum_logical_age_nanoseconds());
  append_integer(output, policy.target_validity_duration_nanoseconds());
}

void append_cut(std::vector<std::byte> &output,
                const PortfolioConstructionCut &cut) {
  append_integer(output, cut.run_input_sequence());
  append_integer(output, cut.logical_time_nanoseconds());
}

struct CanonicalEvidence final {
  std::vector<contracts::TradeRecommendationId> source_recommendation_ids;
  std::vector<contracts::StrategySignalId> source_signal_ids;
  std::vector<contracts::TradeRecommendationId> excluded_recommendation_ids;
  std::vector<contracts::StrategySignalId> excluded_signal_ids;
};

void append_evidence(std::vector<std::byte> &output,
                     const CanonicalEvidence &evidence) {
  append_integer(output, static_cast<std::uint64_t>(
                             evidence.source_recommendation_ids.size()));
  for (const auto value : evidence.source_recommendation_ids)
    append_id(output, value);
  append_integer(output,
                 static_cast<std::uint64_t>(evidence.source_signal_ids.size()));
  for (const auto value : evidence.source_signal_ids)
    append_id(output, value);
  append_integer(output, static_cast<std::uint64_t>(
                             evidence.excluded_recommendation_ids.size()));
  for (const auto value : evidence.excluded_recommendation_ids)
    append_id(output, value);
  append_integer(
      output, static_cast<std::uint64_t>(evidence.excluded_signal_ids.size()));
  for (const auto value : evidence.excluded_signal_ids)
    append_id(output, value);
}

void append_context(std::vector<std::byte> &output,
                    const PortfolioStateSnapshot &snapshot,
                    const PortfolioConstructionPolicy &policy,
                    const PortfolioConstructionCut &cut,
                    const CanonicalEvidence &evidence,
                    std::size_t selected_count) {
  append_policy(output, policy);
  append_snapshot(output, snapshot);
  append_cut(output, cut);
  append_integer(output, static_cast<std::uint64_t>(selected_count));
  append_evidence(output, evidence);
}

template <typename Id>
Id id_from_canonical(const std::vector<std::byte> &canonical) {
  const auto digest = contracts::sha256(canonical);
  typename Id::bytes_type bytes{};
  std::copy_n(digest.bytes.begin(), bytes.size(), bytes.begin());
  return *Id::from_bytes(bytes);
}

bool valid_policy(const PortfolioConstructionPolicy &policy) {
  const auto assignments = policy.assigned_strategy_ids();
  if (policy.maximum_selected_recommendations() == 0 ||
      policy.maximum_selected_recommendations() >
          PortfolioConstructionAuthority::kMaximumRecommendations ||
      assignments.empty() ||
      assignments.size() >
          PortfolioConstructionAuthority::kMaximumAssignedStrategies ||
      policy.recommendation_maximum_logical_age_nanoseconds() < 0 ||
      policy.target_validity_duration_nanoseconds() <= 0)
    return false;
  for (const auto assignment : assignments) {
    if (std::count(assignments.begin(), assignments.end(), assignment) != 1)
      return false;
  }
  return true;
}

std::optional<PortfolioConstructionRejectionReason>
snapshot_rejection(const PortfolioStateSnapshot &snapshot,
                   const PortfolioConstructionPolicy &policy,
                   const PortfolioConstructionCut &cut) {
  if (snapshot.configuration_epoch() == 0 ||
      !snapshot.paper_transition_assumption() ||
      snapshot.exposure_scale() != policy.exposure_scale())
    return PortfolioConstructionRejectionReason::InvalidSnapshot;
  if (snapshot.run_id() != policy.run_id() ||
      snapshot.portfolio_id() != policy.target_key().portfolio_id() ||
      snapshot.account_id() != policy.target_key().account_id() ||
      snapshot.canonical_instrument_id() !=
          policy.target_key().canonical_instrument_id() ||
      snapshot.listing_id() != policy.target_key().listing_id())
    return PortfolioConstructionRejectionReason::SnapshotScopeMismatch;
  if (snapshot.run_input_sequence() > cut.run_input_sequence() ||
      snapshot.logical_time_nanoseconds() > cut.logical_time_nanoseconds())
    return PortfolioConstructionRejectionReason::FutureSnapshot;
  switch (snapshot.disposition()) {
  case PortfolioSnapshotDisposition::FreshComplete:
    return std::nullopt;
  case PortfolioSnapshotDisposition::Stale:
    return PortfolioConstructionRejectionReason::StaleSnapshot;
  case PortfolioSnapshotDisposition::Incomplete:
    return PortfolioConstructionRejectionReason::IncompleteSnapshot;
  case PortfolioSnapshotDisposition::Recovering:
    return PortfolioConstructionRejectionReason::InvalidSnapshot;
  }
  return PortfolioConstructionRejectionReason::InvalidSnapshot;
}

std::vector<const recommendation::TradeRecommendation *>
canonical_recommendations(
    std::span<const recommendation::TradeRecommendation> recommendations) {
  std::vector<const recommendation::TradeRecommendation *> ordered;
  ordered.reserve(recommendations.size());
  for (const auto &value : recommendations)
    ordered.push_back(&value);
  std::sort(ordered.begin(), ordered.end(),
            [](const auto *left, const auto *right) {
              return left->recommendation_id() < right->recommendation_id();
            });
  return ordered;
}

CanonicalEvidence canonical_partitioned_evidence(
    const std::vector<const recommendation::TradeRecommendation *> &ordered) {
  CanonicalEvidence evidence;
  evidence.source_recommendation_ids.reserve(ordered.size());
  evidence.source_signal_ids.reserve(ordered.size());
  evidence.excluded_recommendation_ids.reserve(ordered.size());
  evidence.excluded_signal_ids.reserve(ordered.size());
  for (const auto *value : ordered) {
    if (value->hold()) {
      evidence.excluded_recommendation_ids.push_back(
          value->recommendation_id());
      evidence.excluded_signal_ids.push_back(value->signal_id());
    } else {
      evidence.source_recommendation_ids.push_back(value->recommendation_id());
      evidence.source_signal_ids.push_back(value->signal_id());
    }
  }
  return evidence;
}

contracts::PortfolioConstructionOutcomeId
rejection_id(PortfolioConstructionRejectionReason reason,
             const PortfolioStateSnapshot &snapshot,
             const PortfolioConstructionPolicy &policy,
             const PortfolioConstructionCut &cut,
             const CanonicalEvidence &evidence, std::size_t selected_count) {
  std::vector<std::byte> canonical;
  canonical.reserve(2048);
  append_domain(canonical, "chronos.portfolio-construction-rejected.v1");
  append_enum(canonical, reason);
  append_context(canonical, snapshot, policy, cut, evidence, selected_count);
  return id_from_canonical<contracts::PortfolioConstructionOutcomeId>(
      canonical);
}

contracts::PortfolioConstructionOutcomeId
no_change_id(PortfolioNoChangeReason reason, contracts::AmountUnits desired,
             contracts::AmountUnits current,
             const PortfolioStateSnapshot &snapshot,
             const PortfolioConstructionPolicy &policy,
             const PortfolioConstructionCut &cut,
             const CanonicalEvidence &evidence, std::size_t selected_count) {
  std::vector<std::byte> canonical;
  canonical.reserve(2048);
  append_domain(canonical, "chronos.portfolio-no-change.v1");
  append_enum(canonical, reason);
  append_integer(canonical, desired);
  append_integer(canonical, current);
  append_context(canonical, snapshot, policy, cut, evidence, selected_count);
  return id_from_canonical<contracts::PortfolioConstructionOutcomeId>(
      canonical);
}

contracts::TargetPositionId
target_id(contracts::AmountUnits desired, contracts::AmountUnits current,
          contracts::AmountUnits delta, std::int64_t valid_until,
          const PortfolioStateSnapshot &snapshot,
          const PortfolioConstructionPolicy &policy,
          const PortfolioConstructionCut &cut,
          const CanonicalEvidence &evidence, std::size_t selected_count) {
  std::vector<std::byte> canonical;
  canonical.reserve(2048);
  append_domain(canonical, "chronos.target-position.v1");
  append_integer(canonical, desired);
  append_integer(canonical, current);
  append_integer(canonical, delta);
  append_integer(canonical, valid_until);
  append_context(canonical, snapshot, policy, cut, evidence, selected_count);
  return id_from_canonical<contracts::TargetPositionId>(canonical);
}

contracts::PortfolioConstructionOutcomeId
target_outcome_id(contracts::TargetPositionId target_position_id,
                  const PortfolioStateSnapshot &snapshot,
                  const PortfolioConstructionPolicy &policy,
                  const PortfolioConstructionCut &cut,
                  const CanonicalEvidence &evidence,
                  std::size_t selected_count) {
  std::vector<std::byte> canonical;
  canonical.reserve(2048);
  append_domain(canonical, "chronos.portfolio-target-constructed.v1");
  append_id(canonical, target_position_id);
  append_context(canonical, snapshot, policy, cut, evidence, selected_count);
  return id_from_canonical<contracts::PortfolioConstructionOutcomeId>(
      canonical);
}

} // namespace

PortfolioConstructionResult PortfolioConstructionAuthority::construct(
    std::span<const recommendation::TradeRecommendation> recommendations,
    const PortfolioStateSnapshot &snapshot,
    const PortfolioConstructionPolicy &policy,
    const PortfolioConstructionCut &cut) {
  if (!valid_policy(policy))
    return {.failure = PortfolioConstructionFailure::InvalidPolicy};

  const auto selected_count = recommendations.size();
  const auto ordered = canonical_recommendations(recommendations);
  const auto evidence = canonical_partitioned_evidence(ordered);

  const auto rejected = [&](PortfolioConstructionRejectionReason reason) {
    const auto identity =
        rejection_id(reason, snapshot, policy, cut, evidence, selected_count);
    PortfolioConstructionResult result;
    result.terminal.emplace(PortfolioConstructionRejected(
        identity, reason, policy.target_key(), policy.run_id(), snapshot,
        policy, cut, evidence.source_recommendation_ids,
        evidence.source_signal_ids, evidence.excluded_recommendation_ids,
        evidence.excluded_signal_ids));
    return result;
  };

  if (const auto reason = snapshot_rejection(snapshot, policy, cut))
    return rejected(*reason);
  if (selected_count > policy.maximum_selected_recommendations() ||
      selected_count > kMaximumRecommendations)
    return rejected(
        PortfolioConstructionRejectionReason::RecommendationCapacityExceeded);

  for (std::size_t index = 0; index < ordered.size(); ++index) {
    const auto &candidate = *ordered[index];
    for (std::size_t other = index + 1; other < ordered.size(); ++other) {
      if (candidate.recommendation_id() == ordered[other]->recommendation_id())
        return rejected(
            PortfolioConstructionRejectionReason::DuplicateRecommendationId);
      if (candidate.signal_id() == ordered[other]->signal_id())
        return rejected(
            PortfolioConstructionRejectionReason::DuplicateSignalId);
    }
  }

  std::size_t actionable_count{};
  __int128 aggregate_exposure_units{};
  for (const auto *candidate_pointer : ordered) {
    const auto &candidate = *candidate_pointer;
    if (candidate.run_id() != policy.run_id() ||
        candidate.listing_id() != policy.target_key().listing_id() ||
        candidate.canonical_instrument_id() !=
            policy.target_key().canonical_instrument_id())
      return rejected(
          PortfolioConstructionRejectionReason::RecommendationScopeMismatch);
    if (std::find(policy.assigned_strategy_ids().begin(),
                  policy.assigned_strategy_ids().end(),
                  candidate.strategy_instance_id()) ==
        policy.assigned_strategy_ids().end())
      return rejected(PortfolioConstructionRejectionReason::UnassignedStrategy);
    if (candidate.exposure_scale() != policy.exposure_scale())
      return rejected(
          PortfolioConstructionRejectionReason::IncompatibleExposureScale);
    if (candidate.issue_run_input_sequence() > cut.run_input_sequence() ||
        candidate.issue_logical_time_nanoseconds() >
            cut.logical_time_nanoseconds())
      return rejected(
          PortfolioConstructionRejectionReason::FutureIssuedRecommendation);

    std::int64_t logical_age{};
    std::int64_t expires_at{};
    if (__builtin_sub_overflow(cut.logical_time_nanoseconds(),
                               candidate.issue_logical_time_nanoseconds(),
                               &logical_age) ||
        __builtin_add_overflow(candidate.issue_logical_time_nanoseconds(),
                               candidate.horizon_nanoseconds(), &expires_at))
      return rejected(PortfolioConstructionRejectionReason::ArithmeticOverflow);
    if (logical_age > policy.recommendation_maximum_logical_age_nanoseconds() ||
        cut.logical_time_nanoseconds() > expires_at)
      return rejected(
          PortfolioConstructionRejectionReason::ExpiredRecommendation);

    if (candidate.hold()) {
      continue;
    }
    ++actionable_count;
    const auto units =
        std::get<recommendation::ActionableRecommendation>(candidate.outcome())
            .indicative_exposure_units;
    __int128 contribution = units;
    switch (candidate.direction()) {
    case chronos::strategies::sdk::StrategyDirection::Positive:
      break;
    case chronos::strategies::sdk::StrategyDirection::Negative:
      contribution = -contribution;
      break;
    }
    aggregate_exposure_units += contribution;
  }

  const auto no_change = [&](PortfolioNoChangeReason reason,
                             contracts::AmountUnits desired) {
    const auto identity =
        no_change_id(reason, desired, snapshot.current_exposure_units(),
                     snapshot, policy, cut, evidence, selected_count);
    PortfolioConstructionResult result;
    result.terminal.emplace(PortfolioNoChange(
        identity, reason, policy.target_key(), policy.run_id(), desired,
        snapshot.current_exposure_units(), policy.exposure_scale(), snapshot,
        policy, cut, evidence.source_recommendation_ids,
        evidence.source_signal_ids, evidence.excluded_recommendation_ids,
        evidence.excluded_signal_ids));
    return result;
  };

  if (actionable_count == 0)
    return no_change(PortfolioNoChangeReason::NoActionableRecommendations,
                     snapshot.current_exposure_units());
  if (aggregate_exposure_units == 0)
    return no_change(PortfolioNoChangeReason::ContributionsCancelled,
                     snapshot.current_exposure_units());
  if (aggregate_exposure_units <
          std::numeric_limits<contracts::AmountUnits>::min() ||
      aggregate_exposure_units >
          std::numeric_limits<contracts::AmountUnits>::max())
    return rejected(PortfolioConstructionRejectionReason::ArithmeticOverflow);
  const auto desired_exposure_units =
      static_cast<contracts::AmountUnits>(aggregate_exposure_units);

  contracts::AmountUnits delta{};
  if (__builtin_sub_overflow(desired_exposure_units,
                             snapshot.current_exposure_units(), &delta))
    return rejected(PortfolioConstructionRejectionReason::ArithmeticOverflow);
  if (delta == 0)
    return no_change(PortfolioNoChangeReason::AlreadyAtDesiredExposure,
                     desired_exposure_units);

  std::int64_t valid_until{};
  if (__builtin_add_overflow(cut.logical_time_nanoseconds(),
                             policy.target_validity_duration_nanoseconds(),
                             &valid_until))
    return rejected(PortfolioConstructionRejectionReason::ArithmeticOverflow);
  const auto identity = target_id(
      desired_exposure_units, snapshot.current_exposure_units(), delta,
      valid_until, snapshot, policy, cut, evidence, selected_count);
  const auto outcome_identity = target_outcome_id(
      identity, snapshot, policy, cut, evidence, selected_count);
  PortfolioConstructionResult result;
  result.terminal.emplace(TargetPosition(
      identity, outcome_identity, policy.target_key(), policy.run_id(),
      desired_exposure_units, snapshot.current_exposure_units(), delta,
      policy.exposure_scale(), snapshot, policy, cut, valid_until,
      evidence.source_recommendation_ids, evidence.source_signal_ids,
      evidence.excluded_recommendation_ids, evidence.excluded_signal_ids));
  return result;
}

} // namespace chronos::core::portfolio
