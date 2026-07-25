#include "chronos/runtime/strategies/strategy_runtime.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <type_traits>
#include <utility>

namespace chronos::runtime::strategies {
namespace {

struct CanonicalActivation final {
  std::array<std::byte, 512> bytes{};
  std::size_t size{};

  void append(std::string_view value) noexcept {
    for (const auto character : value)
      bytes[size++] = static_cast<std::byte>(character);
  }

  template <typename Id> void append_id(const Id &value) noexcept {
    for (const auto byte : value.bytes())
      bytes[size++] = static_cast<std::byte>(byte);
  }

  void append_digest(const contracts::Sha256Digest &value) noexcept {
    for (const auto byte : value.bytes)
      bytes[size++] = static_cast<std::byte>(byte);
  }

  template <typename Integer> void append_integer(Integer value) noexcept {
    using Unsigned = std::make_unsigned_t<Integer>;
    const auto converted = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
      const auto shift = (sizeof(Integer) - index - 1) * 8U;
      bytes[size++] =
          static_cast<std::byte>((converted >> shift) & Unsigned{0xFF});
    }
  }

  void append_version(const contracts::VersionRef &value) noexcept {
    append_id(value.definition_id());
    append_integer(value.version());
  }
};

std::vector<std::byte>
canonical_activation_config(const StrategyRuntimeConfig &config) {
  CanonicalActivation canonical;
  canonical.append("chronos.strategy-activation.v1");
  canonical.append_id(config.strategy_instance_id);
  canonical.append_id(config.listing_id);
  canonical.append_id(config.canonical_instrument_id);
  canonical.append_digest(config.definition.definition_digest());
  canonical.append_integer(
      static_cast<std::uint8_t>(config.parameter.has_value()));
  if (config.parameter) {
    canonical.append_id(config.parameter->parameter_id);
    canonical.append_version(config.parameter->definition_version);
    canonical.append_integer(config.parameter->units);
    canonical.append_integer(config.parameter->scale.exponent());
  }
  canonical.append_version(config.recommendation_policy.policy_version);
  canonical.append_digest(
      contracts::recommendation_policy_checksum(config.recommendation_policy));
  canonical.append_id(config.run_control_stream_id);
  canonical.append_integer(config.run_control_stream_epoch);
  canonical.append_id(config.run_timer_stream_id);
  canonical.append_integer(config.run_timer_stream_epoch);
  canonical.append_integer(config.maximum_operations);
  canonical.append_integer(static_cast<std::uint8_t>(
      config.logical_deadline_offset_nanoseconds.has_value()));
  if (config.logical_deadline_offset_nanoseconds)
    canonical.append_integer(*config.logical_deadline_offset_nanoseconds);
  return {canonical.bytes.begin(), canonical.bytes.begin() + canonical.size};
}

contracts::Sha256Digest derive_activation_checksum(
    const StrategyRuntimeConfig &config,
    const core::dispatch::AcceptedControlOutcome &control) noexcept {
  auto bytes = canonical_activation_config(config);
  CanonicalActivation proof;
  proof.append("chronos.strategy-accepted-control.v1");
  const auto &reservation = control.reservation();
  proof.append_id(reservation.run_id);
  proof.append_id(reservation.control_stream_id);
  proof.append_integer(reservation.control_stream_epoch);
  proof.append_id(reservation.control_outcome_id);
  proof.append_integer(reservation.control_sequence);
  proof.append_integer(reservation.effective_position);
  proof.append_integer(reservation.prior_configuration_epoch);
  proof.append_integer(reservation.new_configuration_epoch);
  proof.append_id(control.selection_id());
  proof.append_digest(control.selection_semantic_checksum());
  proof.append_id(control.accepted_control_cursor().stream_id());
  proof.append_integer(control.accepted_control_cursor().stream_epoch());
  proof.append_integer(
      *control.accepted_control_cursor().last_consumed_sequence());
  bytes.insert(bytes.end(), proof.bytes.begin(),
               proof.bytes.begin() + proof.size);
  return contracts::sha256(bytes);
}

const core::features::FeatureProvenance *
provenance(const core::features::FeatureEvaluation &evaluation) noexcept {
  if (evaluation.disposition ==
      core::features::FeatureDisposition::ValidObservation) {
    if (!evaluation.observation || evaluation.unavailable)
      return nullptr;
    return &evaluation.observation->provenance;
  }
  if (evaluation.observation || !evaluation.unavailable)
    return nullptr;
  return &evaluation.unavailable->provenance;
}

bool same_admission_cut(
    const core::features::FeatureProvenance &left,
    const core::features::FeatureProvenance &right) noexcept {
  return left.run_id == right.run_id && left.listing_id == right.listing_id &&
         left.canonical_instrument_id == right.canonical_instrument_id &&
         left.run_input_sequence == right.run_input_sequence &&
         left.logical_time_nanoseconds == right.logical_time_nanoseconds &&
         left.configuration_epoch == right.configuration_epoch &&
         left.effective_control_position == right.effective_control_position &&
         left.active_control_outcome_id == right.active_control_outcome_id &&
         left.active_control_selection_semantic_checksum ==
             right.active_control_selection_semantic_checksum &&
         left.run_control_cursor == right.run_control_cursor &&
         left.run_timer_cursor == right.run_timer_cursor &&
         left.selection_semantic_checksum ==
             right.selection_semantic_checksum &&
         left.lineage == right.lineage;
}

bool valid_parameter(const StrategyRuntimeConfig &config) noexcept {
  if (!config.parameter)
    return true;
  const auto schema = config.definition.descriptor().parameter_schema;
  return schema.size() == 1 &&
         config.parameter->parameter_id == schema[0].parameter_id &&
         config.parameter->definition_version == schema[0].definition_version &&
         config.parameter->scale == schema[0].scale;
}

} // namespace

std::vector<std::byte>
encode_strategy_activation_control(const StrategyRuntimeConfig &config) {
  return canonical_activation_config(config);
}

std::optional<StrategyRuntime> StrategyRuntime::activate(
    StrategyRuntimeConfig config,
    const core::dispatch::AcceptedControlOutcome &control) noexcept {
  const auto limits = config.definition.descriptor().resource_limits;
  const auto signal_scale =
      config.definition.descriptor().parameter_schema.front().scale;
  const auto &reservation = control.reservation();
  if (!valid_parameter(config) ||
      !contracts::valid_recommendation_policy(config.recommendation_policy) ||
      config.recommendation_policy.scale != signal_scale ||
      config.run_control_stream_epoch == 0 ||
      config.run_timer_stream_epoch == 0 || config.maximum_operations == 0 ||
      config.maximum_operations > limits.maximum_operations ||
      config.run_control_stream_id == config.run_timer_stream_id ||
      (config.logical_deadline_offset_nanoseconds &&
       *config.logical_deadline_offset_nanoseconds < 0) ||
      reservation.behavior_payload != canonical_activation_config(config) ||
      reservation.behavior_checksum !=
          contracts::sha256(reservation.behavior_payload) ||
      control.accepted_control_cursor().stream_id() !=
          reservation.control_stream_id ||
      control.accepted_control_cursor().stream_epoch() !=
          reservation.control_stream_epoch ||
      !control.accepted_control_cursor().last_consumed_sequence() ||
      *control.accepted_control_cursor().last_consumed_sequence() <
          reservation.control_sequence)
    return std::nullopt;
  const auto checksum = derive_activation_checksum(config, control);
  return StrategyRuntime(std::move(config), control, checksum);
}

std::optional<chronos::strategies::sdk::AcceptedStrategyInvocation>
StrategyRuntime::admit(const core::features::AcceptedFeatureEvaluationCut
                           &feature_cut) const noexcept {
  const auto &evaluations = feature_cut.evaluations();
  if (!feature_cut.accepted_control_outcome() ||
      *feature_cut.accepted_control_outcome() != control_ ||
      evaluations.empty() ||
      evaluations.size() >
          chronos::strategies::sdk::kMaximumAcceptedFeatureEvaluations)
    return std::nullopt;
  const auto *first = provenance(evaluations.front());
  if (!first || first->lineage.cursors().size() >
                    chronos::strategies::sdk::kMaximumAcceptedLineageCursors)
    return std::nullopt;
  for (const auto &evaluation : evaluations) {
    const auto *current = provenance(evaluation);
    if (!current || !same_admission_cut(*first, *current))
      return std::nullopt;
  }
  const auto &reservation = control_.reservation();
  if (first->run_id != reservation.run_id ||
      first->listing_id != config_.listing_id ||
      first->canonical_instrument_id != config_.canonical_instrument_id ||
      first->configuration_epoch != reservation.new_configuration_epoch ||
      first->effective_control_position !=
          std::optional(reservation.effective_position) ||
      first->active_control_outcome_id !=
          std::optional(reservation.control_outcome_id) ||
      first->active_control_selection_semantic_checksum !=
          std::optional(control_.selection_semantic_checksum()) ||
      first->run_input_sequence < reservation.effective_position ||
      first->run_control_cursor.last_consumed_sequence() !=
          std::optional(reservation.control_sequence) ||
      first->run_control_cursor.stream_id() != config_.run_control_stream_id ||
      first->run_control_cursor.stream_epoch() !=
          config_.run_control_stream_epoch ||
      first->run_timer_cursor.stream_id() != config_.run_timer_stream_id ||
      first->run_timer_cursor.stream_epoch() != config_.run_timer_stream_epoch)
    return std::nullopt;

  std::optional<std::int64_t> deadline;
  if (config_.logical_deadline_offset_nanoseconds) {
    std::int64_t value{};
    if (__builtin_add_overflow(first->logical_time_nanoseconds,
                               *config_.logical_deadline_offset_nanoseconds,
                               &value))
      return std::nullopt;
    deadline = value;
  }
  const auto descriptor = config_.definition.descriptor();
  return chronos::strategies::sdk::AcceptedStrategyInvocation(
      reservation.run_id, config_.strategy_instance_id, config_.listing_id,
      config_.canonical_instrument_id, descriptor.definition_version,
      descriptor.implementation_version, descriptor.arithmetic_version,
      descriptor.explanation_policy_version,
      config_.recommendation_policy.policy_version,
      contracts::recommendation_policy_checksum(config_.recommendation_policy),
      config_.definition.definition_digest(), reservation.control_outcome_id,
      activation_checksum_, first->run_control_cursor,
      first->selection_semantic_checksum, config_.maximum_operations,
      feature_cut, config_.parameter,
      {
          .run_input_sequence = first->run_input_sequence,
          .logical_time_nanoseconds = first->logical_time_nanoseconds,
          .run_timer_cursor = first->run_timer_cursor,
          .configuration_epoch = first->configuration_epoch,
          .effective_control_position = first->effective_control_position,
          .logical_deadline_nanoseconds = deadline,
      });
}

} // namespace chronos::runtime::strategies
