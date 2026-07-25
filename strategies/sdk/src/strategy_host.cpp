#include "chronos/strategies/sdk/strategy_host.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace chronos::strategies::sdk {
namespace {

std::optional<StrategyFeatureKind>
map_kind(core::features::FeatureKind kind) noexcept {
  using Source = core::features::FeatureKind;
  switch (kind) {
  case Source::OrderBookImbalance:
    return StrategyFeatureKind::OrderBookImbalance;
  case Source::Microprice:
    return StrategyFeatureKind::Microprice;
  case Source::Spread:
    return StrategyFeatureKind::Spread;
  }
  return std::nullopt;
}

std::optional<StrategyFeatureUnavailableReason>
map_reason(core::features::FeatureUnavailableReason reason) noexcept {
  using Source = core::features::FeatureUnavailableReason;
  using Target = StrategyFeatureUnavailableReason;
  switch (reason) {
  case Source::BookStarting:
    return Target::BookStarting;
  case Source::BookRecovering:
    return Target::BookRecovering;
  case Source::BookGapped:
    return Target::BookGapped;
  case Source::BookInvalid:
    return Target::BookInvalid;
  case Source::BookUnavailable:
    return Target::BookUnavailable;
  case Source::BookClosed:
    return Target::BookClosed;
  case Source::BookStale:
    return Target::BookStale;
  case Source::BookFreshnessUnknown:
    return Target::BookFreshnessUnknown;
  case Source::UnsupportedBookShape:
    return Target::UnsupportedBookShape;
  case Source::TopNotProven:
    return Target::TopNotProven;
  case Source::TopUnavailable:
    return Target::TopUnavailable;
  case Source::InvalidQuantity:
    return Target::InvalidQuantity;
  case Source::DefinitionMismatch:
    return Target::DefinitionMismatch;
  case Source::ArithmeticOverflow:
    return Target::ArithmeticOverflow;
  }
  return std::nullopt;
}

bool matches_cut(const core::features::FeatureProvenance &provenance,
                 const StrategyInvocationRequest &request) noexcept {
  return provenance.run_id == request.run_id &&
         provenance.listing_id == request.listing_id &&
         provenance.canonical_instrument_id ==
             request.canonical_instrument_id &&
         provenance.run_input_sequence == request.cut.run_input_sequence &&
         provenance.logical_time_nanoseconds ==
             request.cut.logical_time_nanoseconds &&
         provenance.configuration_epoch == request.cut.configuration_epoch &&
         provenance.effective_control_position ==
             request.cut.effective_control_position;
}

std::optional<StrategyFeature>
map_feature(const core::features::FeatureEvaluation &evaluation,
            const StrategyInvocationRequest &request) {
  const auto kind = map_kind(evaluation.kind);
  if (!kind)
    return std::nullopt;

  const core::features::FeatureProvenance *source_provenance = nullptr;
  std::optional<StrategyFeatureValue> value;
  std::optional<StrategyFeatureUnavailableReason> unavailable_reason;
  StrategyFeatureDisposition disposition{};

  if (evaluation.disposition ==
      core::features::FeatureDisposition::ValidObservation) {
    if (!evaluation.observation || evaluation.unavailable ||
        evaluation.observation->kind != evaluation.kind)
      return std::nullopt;
    source_provenance = &evaluation.observation->provenance;
    disposition = StrategyFeatureDisposition::ValidObservation;
    if (const auto *ratio = std::get_if<core::features::ScaledRatio>(
            &evaluation.observation->value)) {
      value = ScaledRatio{.units = ratio->units, .scale = ratio->scale};
    } else {
      value = std::get<contracts::Price>(evaluation.observation->value);
    }
  } else {
    if (evaluation.observation || !evaluation.unavailable ||
        evaluation.unavailable->kind != evaluation.kind)
      return std::nullopt;
    source_provenance = &evaluation.unavailable->provenance;
    disposition = StrategyFeatureDisposition::Unavailable;
    unavailable_reason = map_reason(evaluation.unavailable->reason);
    if (!unavailable_reason)
      return std::nullopt;
  }

  if (!matches_cut(*source_provenance, request))
    return std::nullopt;

  return StrategyFeature{
      .evaluation_id = evaluation.evaluation_id,
      .kind = *kind,
      .disposition = disposition,
      .provenance =
          {
              .run_id = source_provenance->run_id,
              .listing_id = source_provenance->listing_id,
              .canonical_instrument_id =
                  source_provenance->canonical_instrument_id,
              .run_input_sequence = source_provenance->run_input_sequence,
              .logical_time_nanoseconds =
                  source_provenance->logical_time_nanoseconds,
              .configuration_epoch = source_provenance->configuration_epoch,
              .effective_control_position =
                  source_provenance->effective_control_position,
              .feature_definition_version =
                  source_provenance->feature_definition_version,
              .input_view_semantic_checksum =
                  source_provenance->input_view_semantic_checksum,
              .input_bundle_semantic_checksum =
                  source_provenance->input_bundle_semantic_checksum,
          },
      .value = std::move(value),
      .unavailable_reason = unavailable_reason,
      .semantic_checksum = evaluation.semantic_checksum,
  };
}

bool validate_parameters(
    const StrategyDescriptor &descriptor,
    std::span<const StrategyParameter> parameters) noexcept {
  if (parameters.size() > descriptor.resource_limits.maximum_parameters)
    return false;
  return std::all_of(
      parameters.begin(), parameters.end(), [&](const auto &value) {
        return std::count_if(
                   descriptor.parameter_schema.begin(),
                   descriptor.parameter_schema.end(), [&](const auto &schema) {
                     return value.parameter_id == schema.parameter_id &&
                            value.definition_version ==
                                schema.definition_version &&
                            value.scale == schema.scale;
                   }) == 1;
      });
}

bool validate_factors(const StrategyDescriptor &descriptor,
                      const StrategyOutput &output,
                      const TerminalDraft &terminal) noexcept {
  const auto abstained = std::holds_alternative<AbstentionDraft>(terminal);
  const auto direction = abstained ? std::optional<StrategyDirection>{}
                                   : std::get<SignalDraft>(terminal).direction;
  for (std::size_t index = 0; index < output.factor_count(); ++index) {
    const auto &factor = output.factor(index);
    if (factor.ranking_policy_version !=
            descriptor.explanation_policy_version ||
        (factor.source == ExplanationSource::Feature &&
         !factor.causal_feature_evaluation_id) ||
        (abstained && factor.role != ExplanationRole::ExplainsAbstention) ||
        (!abstained && direction == StrategyDirection::Positive &&
         factor.role == ExplanationRole::SupportsNegative) ||
        (!abstained && direction == StrategyDirection::Negative &&
         factor.role == ExplanationRole::SupportsPositive))
      return false;
  }
  return true;
}

StrategyHostResult failure(StrategyExecutionStatus status,
                           std::uint64_t charged) noexcept {
  return {.status = status, .charged_operations = charged};
}

} // namespace

StrategyHostResult StrategyHost::evaluate(
    const Strategy &strategy, const StrategyInvocationRequest &request,
    DeterministicOperationBudget &budget,
    std::span<std::byte> workspace_storage,
    std::span<std::optional<ExplanationFactor>> factor_storage) {
  const auto &descriptor = strategy.descriptor();
  if (!validate_descriptor(descriptor) ||
      !validate_parameters(descriptor, request.parameters))
    return failure(StrategyExecutionStatus::ContractViolation,
                   budget.charged_operations());

  if (logical_deadline_exceeded(request.cut))
    return failure(StrategyExecutionStatus::LogicalDeadlineExceeded,
                   budget.charged_operations());

  if (!budget.charge(descriptor.resource_limits.operations_per_evaluation))
    return failure(StrategyExecutionStatus::DeterministicBudgetExhausted,
                   budget.charged_operations());

  if (workspace_storage.size() <
      descriptor.resource_limits.maximum_working_bytes)
    return failure(StrategyExecutionStatus::InsufficientWorkspace,
                   budget.charged_operations());
  if (factor_storage.size() <
      descriptor.resource_limits.maximum_explanation_factors)
    return failure(StrategyExecutionStatus::OutputCapacityExceeded,
                   budget.charged_operations());

  std::vector<StrategyFeature> mapped_features;
  mapped_features.reserve(request.feature_cut.evaluations().size());
  for (const auto &source : request.feature_cut.evaluations()) {
    auto mapped = map_feature(source, request);
    if (!mapped)
      return failure(StrategyExecutionStatus::ContractViolation,
                     budget.charged_operations());
    const auto duplicate = std::find_if(
        mapped_features.begin(), mapped_features.end(),
        [&](const auto &prior) { return prior.kind == mapped->kind; });
    if (duplicate != mapped_features.end())
      return failure(StrategyExecutionStatus::ContractViolation,
                     budget.charged_operations());
    mapped_features.push_back(std::move(*mapped));
  }

  std::vector<StrategyFeature> selected_features;
  selected_features.reserve(descriptor.required_features.size());
  for (const auto &dependency : descriptor.required_features) {
    const auto source =
        std::find_if(mapped_features.begin(), mapped_features.end(),
                     [&](const auto &candidate) {
                       return candidate.kind == dependency.kind;
                     });
    if (source == mapped_features.end())
      continue;
    selected_features.push_back(*source);
  }

  if (selected_features.size() > descriptor.resource_limits.maximum_features)
    return failure(StrategyExecutionStatus::ContractViolation,
                   budget.charged_operations());

  std::fill_n(workspace_storage.begin(),
              descriptor.resource_limits.maximum_working_bytes, std::byte{});
  StrategyWorkspace workspace(workspace_storage.first(
      descriptor.resource_limits.maximum_working_bytes));
  StrategyOutput output(factor_storage.first(
      descriptor.resource_limits.maximum_explanation_factors));
  AcceptedStrategyInvocation invocation(
      request.run_id, request.strategy_instance_id, request.listing_id,
      request.canonical_instrument_id, std::move(selected_features),
      std::vector<StrategyParameter>(request.parameters.begin(),
                                     request.parameters.end()),
      request.cut);

  const auto status = strategy.evaluate(invocation, workspace, output);
  if (status != StrategyExecutionStatus::Completed) {
    if (output.terminal() || output.factor_count() != 0)
      return failure(StrategyExecutionStatus::ContractViolation,
                     budget.charged_operations());
    return failure(status, budget.charged_operations());
  }
  if (!output.terminal() ||
      !validate_factors(descriptor, output, *output.terminal()))
    return failure(StrategyExecutionStatus::ContractViolation,
                   budget.charged_operations());

  return {
      .status = StrategyExecutionStatus::Completed,
      .charged_operations = budget.charged_operations(),
      .factor_count = output.factor_count(),
      .terminal = *output.terminal(),
  };
}

} // namespace chronos::strategies::sdk
