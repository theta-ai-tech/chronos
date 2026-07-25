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

contracts::Sha256Digest
derive_activation_checksum(const StrategyRuntimeConfig &config) noexcept {
  CanonicalActivation canonical;
  canonical.append("chronos.strategy-activation.v1");
  canonical.append_id(config.run_id);
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
  canonical.append_id(config.activation_control_outcome_id);
  canonical.append_integer(config.active_configuration_epoch);
  canonical.append_integer(
      static_cast<std::uint8_t>(config.effective_control_position.has_value()));
  if (config.effective_control_position)
    canonical.append_integer(*config.effective_control_position);
  canonical.append_id(config.run_control_stream_id);
  canonical.append_integer(config.run_control_stream_epoch);
  canonical.append_id(config.run_timer_stream_id);
  canonical.append_integer(config.run_timer_stream_epoch);
  canonical.append_integer(config.maximum_operations);
  canonical.append_integer(static_cast<std::uint8_t>(
      config.logical_deadline_nanoseconds.has_value()));
  if (config.logical_deadline_nanoseconds)
    canonical.append_integer(*config.logical_deadline_nanoseconds);
  return contracts::sha256(
      std::span<const std::byte>(canonical.bytes).first(canonical.size));
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

std::optional<StrategyRuntime>
StrategyRuntime::activate(StrategyRuntimeConfig config) noexcept {
  const auto limits = config.definition.descriptor().resource_limits;
  if (!valid_parameter(config) || config.active_configuration_epoch == 0 ||
      config.run_control_stream_epoch == 0 ||
      config.run_timer_stream_epoch == 0 || config.maximum_operations == 0 ||
      config.maximum_operations > limits.maximum_operations ||
      config.run_control_stream_id == config.run_timer_stream_id)
    return std::nullopt;
  const auto checksum = derive_activation_checksum(config);
  return StrategyRuntime(std::move(config), checksum);
}

std::optional<chronos::strategies::sdk::AcceptedStrategyInvocation>
StrategyRuntime::admit(const core::features::AcceptedFeatureEvaluationCut
                           &feature_cut) const noexcept {
  const auto &evaluations = feature_cut.evaluations();
  if (evaluations.empty() ||
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
  if (first->run_id != config_.run_id ||
      first->listing_id != config_.listing_id ||
      first->canonical_instrument_id != config_.canonical_instrument_id ||
      first->configuration_epoch != config_.active_configuration_epoch ||
      first->effective_control_position != config_.effective_control_position ||
      first->run_control_cursor.stream_id() != config_.run_control_stream_id ||
      first->run_control_cursor.stream_epoch() !=
          config_.run_control_stream_epoch ||
      first->run_timer_cursor.stream_id() != config_.run_timer_stream_id ||
      first->run_timer_cursor.stream_epoch() != config_.run_timer_stream_epoch)
    return std::nullopt;

  const auto descriptor = config_.definition.descriptor();
  return chronos::strategies::sdk::AcceptedStrategyInvocation(
      config_.run_id, config_.strategy_instance_id, config_.listing_id,
      config_.canonical_instrument_id, descriptor.definition_version,
      descriptor.implementation_version, descriptor.arithmetic_version,
      descriptor.explanation_policy_version,
      config_.definition.definition_digest(),
      config_.activation_control_outcome_id, activation_checksum_,
      first->run_control_cursor, first->selection_semantic_checksum,
      config_.maximum_operations, feature_cut, config_.parameter,
      {
          .run_input_sequence = first->run_input_sequence,
          .logical_time_nanoseconds = first->logical_time_nanoseconds,
          .run_timer_cursor = first->run_timer_cursor,
          .configuration_epoch = first->configuration_epoch,
          .effective_control_position = first->effective_control_position,
          .logical_deadline_nanoseconds = config_.logical_deadline_nanoseconds,
      });
}

} // namespace chronos::runtime::strategies
