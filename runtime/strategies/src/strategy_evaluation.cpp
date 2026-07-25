#include "chronos/runtime/strategies/strategy_evaluation.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <type_traits>
#include <vector>

namespace chronos::runtime::strategies {
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

void append_digest(std::vector<std::byte> &output,
                   const contracts::Sha256Digest &value) {
  for (const auto byte : value.bytes)
    output.push_back(static_cast<std::byte>(byte));
}

void append_cursor(std::vector<std::byte> &output,
                   const contracts::StreamCursor &value) {
  append_id(output, value.stream_id());
  append_integer(output, value.stream_epoch());
  append_integer<std::uint8_t>(output,
                               value.last_consumed_sequence().has_value());
  if (value.last_consumed_sequence())
    append_integer(output, *value.last_consumed_sequence());
}

template <typename Id>
Id id_from_digest(const contracts::Sha256Digest &checksum) {
  typename Id::bytes_type bytes{};
  std::copy_n(checksum.bytes.begin(), bytes.size(), bytes.begin());
  return *Id::from_bytes(bytes);
}

contracts::Sha256Digest evaluation_key(
    const chronos::strategies::sdk::AcceptedStrategyInvocation &invocation) {
  std::vector<std::byte> canonical;
  canonical.reserve(512);
  constexpr std::string_view domain = "chronos.strategy-evaluation-key.v1";
  for (const auto character : domain)
    canonical.push_back(static_cast<std::byte>(character));
  append_id(canonical, invocation.run_id());
  append_id(canonical, invocation.strategy_instance_id());
  append_id(canonical, invocation.listing_id());
  append_id(canonical, invocation.canonical_instrument_id());
  append_digest(canonical, invocation.definition_digest());
  append_digest(canonical, invocation.activation_checksum());
  append_id(canonical,
            invocation.recommendation_policy_version().definition_id());
  append_integer(canonical,
                 invocation.recommendation_policy_version().version());
  append_digest(canonical, invocation.recommendation_policy_checksum());
  append_digest(canonical, invocation.selection_semantic_checksum());
  append_cursor(canonical, invocation.run_control_cursor());
  const auto &cut = invocation.cut();
  append_integer(canonical, cut.run_input_sequence);
  append_integer(canonical, cut.logical_time_nanoseconds);
  append_cursor(canonical, cut.run_timer_cursor);
  append_integer(canonical, cut.configuration_epoch);
  append_integer<std::uint8_t>(canonical,
                               cut.effective_control_position.has_value());
  if (cut.effective_control_position)
    append_integer(canonical, *cut.effective_control_position);
  const auto &features = invocation.feature_cut().evaluations();
  append_integer(canonical, static_cast<std::uint64_t>(features.size()));
  for (const auto &feature : features) {
    append_id(canonical, feature.evaluation_id);
    append_digest(canonical, feature.semantic_checksum);
  }
  const auto parameters = invocation.parameters();
  append_integer(canonical, static_cast<std::uint64_t>(parameters.size()));
  for (const auto &parameter : parameters) {
    append_id(canonical, parameter.parameter_id);
    append_id(canonical, parameter.definition_version.definition_id());
    append_integer(canonical, parameter.definition_version.version());
    append_integer(canonical, parameter.units);
    append_integer(canonical, parameter.scale.exponent());
  }
  return contracts::sha256(canonical);
}

bool valid_signal(const chronos::strategies::sdk::SignalDraft &signal) {
  return signal.strength.units > 0 && signal.horizon_nanoseconds > 0 &&
         signal.horizon_nanoseconds <=
             chronos::strategies::sdk::kMaximumSignalHorizonNanoseconds;
}

bool valid_factors(
    std::span<const std::optional<chronos::strategies::sdk::ExplanationFactor>>
        candidates,
    std::size_t count) {
  if (count == 0 || count > candidates.size())
    return false;
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    if (candidates[index].has_value() != (index < count))
      return false;
    if (index < count &&
        candidates[index]->rank != static_cast<std::uint32_t>(index + 1))
      return false;
  }
  return true;
}

StrategyEvaluationFailure
failure_for(chronos::strategies::sdk::StrategyExecutionStatus status) {
  return status == chronos::strategies::sdk::StrategyExecutionStatus::
                       ContractViolation
             ? StrategyEvaluationFailure::ContractViolation
             : StrategyEvaluationFailure::OperationalInterruption;
}

} // namespace

StrategyEvaluationResult StrategyEvaluationAuthority::evaluate(
    const chronos::strategies::sdk::AcceptedStrategyDefinition &definition,
    const chronos::strategies::sdk::AcceptedStrategyInvocation &invocation) {
  const auto key = evaluation_key(invocation);
  const auto evaluation_id =
      id_from_digest<contracts::StrategyEvaluationId>(key);
  std::array<std::byte, chronos::strategies::sdk::kInterpreterWorkingBytes>
      workspace{};
  std::array<std::optional<chronos::strategies::sdk::ExplanationFactor>,
             chronos::strategies::sdk::kThresholdProgramFactors>
      factor_storage{};
  const auto host = chronos::strategies::sdk::StrategyHost::evaluate(
      definition, invocation, workspace, factor_storage);
  StrategyEvaluationResult result{
      .failure = failure_for(host.status),
      .execution_status = host.status,
      .charged_operations = host.charged_operations,
      .accepted_evaluation_key = key,
      .accepted_evaluation_id = evaluation_id,
  };
  if (!host.completed())
    return result;
  if (!valid_factors(factor_storage, host.factor_count)) {
    result.failure = StrategyEvaluationFailure::InvalidTerminal;
    return result;
  }

  StrategyEvaluationTerminal terminal = StrategyAbstention{};
  if (const auto *signal =
          std::get_if<chronos::strategies::sdk::SignalDraft>(&*host.terminal)) {
    if (!valid_signal(*signal)) {
      result.failure = StrategyEvaluationFailure::InvalidTerminal;
      return result;
    }
    std::vector<std::byte> signal_identity;
    constexpr std::string_view domain = "chronos.strategy-signal.v1";
    for (const auto character : domain)
      signal_identity.push_back(static_cast<std::byte>(character));
    append_digest(signal_identity, key);
    append_enum(signal_identity, signal->direction);
    append_integer(signal_identity, signal->strength.units);
    append_integer(signal_identity, signal->strength.scale.exponent());
    append_integer(signal_identity, signal->horizon_nanoseconds);
    terminal = StrategySignal(id_from_digest<contracts::StrategySignalId>(
                                  contracts::sha256(signal_identity)),
                              evaluation_id, *signal);
  } else {
    terminal = StrategyAbstention{
        .reason =
            std::get<chronos::strategies::sdk::AbstentionDraft>(*host.terminal)
                .reason};
  }

  std::vector<contracts::FeatureEvaluationId> feature_ids;
  const auto &features = invocation.feature_cut().evaluations();
  feature_ids.reserve(features.size());
  for (const auto &feature : features)
    feature_ids.push_back(feature.evaluation_id);
  std::vector<chronos::strategies::sdk::ExplanationFactor> factors;
  factors.reserve(host.factor_count);
  for (std::size_t index = 0; index < host.factor_count; ++index)
    factors.push_back(*factor_storage[index]);

  result.failure = StrategyEvaluationFailure::None;
  result.evaluation = StrategyEvaluation(
      evaluation_id, key, invocation.run_id(),
      invocation.strategy_instance_id(), invocation.listing_id(),
      invocation.canonical_instrument_id(), invocation.definition_digest(),
      invocation.activation_checksum(),
      invocation.recommendation_policy_version(),
      invocation.recommendation_policy_checksum(),
      invocation.cut().run_input_sequence,
      invocation.cut().logical_time_nanoseconds, std::move(feature_ids),
      std::move(terminal), std::move(factors));
  return result;
}

} // namespace chronos::runtime::strategies
