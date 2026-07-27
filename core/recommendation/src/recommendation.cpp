#include "chronos/core/recommendation/recommendation.hpp"

#include <algorithm>
#include <string_view>
#include <type_traits>
#include <vector>

namespace chronos::core::recommendation {
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

void append_version(std::vector<std::byte> &output,
                    const contracts::VersionRef &value) {
  append_id(output, value.definition_id());
  append_integer(output, value.version());
}

template <typename Id>
Id id_from_digest(const contracts::Sha256Digest &checksum) {
  typename Id::bytes_type bytes{};
  std::copy_n(checksum.bytes.begin(), bytes.size(), bytes.begin());
  return *Id::from_bytes(bytes);
}

bool valid_factors(
    std::span<const chronos::strategies::sdk::ExplanationFactor> factors) {
  if (factors.empty())
    return false;
  for (std::size_t index = 0; index < factors.size(); ++index) {
    if (factors[index].rank != static_cast<std::uint32_t>(index + 1))
      return false;
  }
  return true;
}

contracts::TradeRecommendationId
recommendation_id(const runtime::strategies::StrategySignal &signal,
                  const RecommendationPolicy &policy,
                  const RecommendationOutcome &outcome) {
  std::vector<std::byte> canonical;
  canonical.reserve(160);
  constexpr std::string_view domain = "chronos.trade-recommendation.v1";
  for (const auto character : domain)
    canonical.push_back(static_cast<std::byte>(character));
  append_id(canonical, signal.signal_id());
  append_id(canonical, signal.evaluation_id());
  append_version(canonical, policy.policy_version);
  append_version(canonical, policy.schema_version);
  append_version(canonical, policy.authority_version);
  append_integer(canonical, policy.minimum_actionable_strength);
  append_integer(canonical, policy.maximum_indicative_exposure);
  append_integer(canonical, policy.scale.exponent());
  append_integer(canonical, static_cast<std::uint8_t>(outcome.index()));
  if (const auto *action = std::get_if<ActionableRecommendation>(&outcome)) {
    append_integer(canonical, action->indicative_exposure_units);
  } else {
    const auto &hold = std::get<HoldRecommendation>(outcome);
    append_enum(canonical, hold.reason);
    append_integer(canonical, hold.indicative_exposure_units);
  }
  return id_from_digest<contracts::TradeRecommendationId>(
      contracts::sha256(canonical));
}

} // namespace

RecommendationResult RecommendationAuthority::recommend(
    const runtime::strategies::StrategyEvaluation &evaluation,
    const RecommendationPolicy &policy) {
  if (!contracts::valid_recommendation_policy(policy))
    return {.failure = RecommendationFailure::InvalidPolicy};
  if (evaluation.recommendation_policy_version() != policy.policy_version ||
      evaluation.recommendation_policy_checksum() !=
          recommendation_policy_checksum(policy))
    return {.failure = RecommendationFailure::InvalidPolicy};
  if (evaluation.abstained())
    return {.failure = RecommendationFailure::AbstainedEvaluation};

  const auto &signal =
      std::get<runtime::strategies::StrategySignal>(evaluation.terminal());
  const auto &draft = signal.draft();
  if (draft.strength.units <= 0 || draft.strength.scale != policy.scale ||
      draft.horizon_nanoseconds <= 0 || !valid_factors(evaluation.factors()))
    return {.failure = RecommendationFailure::InvalidSignal};

  RecommendationOutcome outcome = HoldRecommendation{};
  if (draft.strength.units >= policy.minimum_actionable_strength) {
    outcome = ActionableRecommendation{
        .indicative_exposure_units =
            std::min(draft.strength.units, policy.maximum_indicative_exposure),
    };
  } else {
    outcome = HoldRecommendation{
        .reason = RecommendationHoldReason::BelowActionThreshold,
        .indicative_exposure_units = 0,
    };
  }
  std::vector<chronos::strategies::sdk::ExplanationFactor> factors(
      evaluation.factors().begin(), evaluation.factors().end());
  const auto identity = recommendation_id(signal, policy, outcome);
  RecommendationResult result;
  result.recommendation = TradeRecommendation(
      identity, signal.signal_id(), evaluation.evaluation_id(), policy,
      draft.direction, draft.horizon_nanoseconds,
      evaluation.run_input_sequence(), evaluation.logical_time_nanoseconds(),
      std::move(outcome), std::move(factors));
  return result;
}

std::optional<RecommendationAcceptanceAuthority>
RecommendationAcceptanceAuthority::create(std::size_t maximum_recommendations) {
  if (maximum_recommendations == 0 ||
      maximum_recommendations > kMaximumAcceptedRecommendations)
    return std::nullopt;
  return RecommendationAcceptanceAuthority(maximum_recommendations);
}

RecommendationAcceptanceResult RecommendationAcceptanceAuthority::accept(
    const TradeRecommendation &candidate) {
  const auto existing =
      std::find_if(accepted_.begin(), accepted_.end(), [&](const auto &value) {
        return value.signal_id() == candidate.signal_id();
      });
  if (existing != accepted_.end()) {
    if (*existing != candidate)
      return {.failure =
                  RecommendationAcceptanceFailure::ConflictingRecommendation};
    return {
        .disposition =
            RecommendationAcceptanceDisposition::DeduplicatedExisting,
        .recommendation = *existing,
    };
  }
  if (accepted_.size() == maximum_recommendations_)
    return {.failure = RecommendationAcceptanceFailure::CapacityExceeded};
  accepted_.push_back(candidate);
  return {
      .disposition = RecommendationAcceptanceDisposition::AcceptedNew,
      .recommendation = accepted_.back(),
  };
}

} // namespace chronos::core::recommendation
