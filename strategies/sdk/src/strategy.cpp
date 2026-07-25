#include "chronos/strategies/sdk/strategy.hpp"

#include <algorithm>
#include <utility>

namespace chronos::strategies::sdk {

bool DeterministicOperationBudget::charge(std::uint64_t operations) noexcept {
  if (operations > available_operations_ - charged_operations_)
    return false;
  charged_operations_ += operations;
  return true;
}

StrategyOutput::StrategyOutput(
    std::span<std::optional<ExplanationFactor>> factor_storage) noexcept
    : factor_storage_(factor_storage) {
  for (auto &factor : factor_storage_)
    factor.reset();
}

bool StrategyOutput::append_factor(ExplanationFactor factor) noexcept {
  if (terminal_ || factor_count_ == factor_storage_.size() ||
      factor.rank != factor_count_ + 1)
    return false;
  factor_storage_[factor_count_++].emplace(std::move(factor));
  return true;
}

bool StrategyOutput::emit_signal(SignalDraft signal) noexcept {
  if (terminal_ || signal.horizon_nanoseconds <= 0 ||
      signal.strength.units <= 0 ||
      signal.strength.units > signal.strength.scale.denominator())
    return false;
  terminal_ = std::move(signal);
  return true;
}

bool StrategyOutput::abstain(AbstentionDraft abstention) noexcept {
  if (terminal_)
    return false;
  terminal_ = abstention;
  return true;
}

bool validate_descriptor(const StrategyDescriptor &descriptor) noexcept {
  const auto &limits = descriptor.resource_limits;
  if (descriptor.required_features.empty() ||
      descriptor.required_features.size() > limits.maximum_features ||
      descriptor.parameter_schema.size() > limits.maximum_parameters ||
      limits.operations_per_evaluation == 0 || limits.maximum_features == 0 ||
      limits.maximum_explanation_factors == 0 ||
      limits.maximum_working_bytes == 0)
    return false;

  const auto duplicate_feature = std::any_of(
      descriptor.required_features.begin(), descriptor.required_features.end(),
      [&](const auto &current) {
        return std::count_if(descriptor.required_features.begin(),
                             descriptor.required_features.end(),
                             [&](const auto &candidate) {
                               return candidate.kind == current.kind;
                             }) != 1;
      });
  if (duplicate_feature)
    return false;

  return std::none_of(
      descriptor.parameter_schema.begin(), descriptor.parameter_schema.end(),
      [&](const auto &current) {
        return std::count_if(descriptor.parameter_schema.begin(),
                             descriptor.parameter_schema.end(),
                             [&](const auto &candidate) {
                               return candidate.parameter_id ==
                                      current.parameter_id;
                             }) != 1;
      });
}

bool logical_deadline_exceeded(const LogicalCut &cut) noexcept {
  return cut.logical_deadline_nanoseconds &&
         cut.logical_time_nanoseconds > *cut.logical_deadline_nanoseconds;
}

} // namespace chronos::strategies::sdk
