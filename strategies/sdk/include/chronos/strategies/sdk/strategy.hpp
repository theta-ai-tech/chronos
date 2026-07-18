#pragma once

#include "chronos/contracts/digest.hpp"
#include "chronos/contracts/fixed_point.hpp"
#include "chronos/core/features/feature_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

namespace chronos::strategies::sdk {

enum class StrategyScope : std::uint8_t {
  SingleListing,
};

enum class StrategyFamily : std::uint8_t {
  OrderBookImbalance,
  MicropriceSpread,
  ShortHorizonMomentum,
};

enum class StrategyDirection : std::uint8_t {
  Positive,
  Negative,
};

enum class ExplanationSource : std::uint8_t {
  Feature,
  MarketStateLineage,
  ControlConfiguration,
  LogicalTimer,
  StrategyParameter,
  DiagnosticStatus,
};

enum class ExplanationRole : std::uint8_t {
  SupportsPositive,
  SupportsNegative,
  ExplainsAbstention,
};

enum class StrategyAbstentionReason : std::uint8_t {
  MissingDeclaredFeature,
  NonValidFeature,
  IncompatibleFeature,
  MissingParameter,
  InvalidParameter,
  LogicalDeadlineExceeded,
  DeterministicBudgetExhausted,
  InsufficientWarmup,
};

enum class StrategyExecutionStatus : std::uint8_t {
  Completed,
  DeterministicBudgetExhausted,
  OutputCapacityExceeded,
  ContractViolation,
};

struct FeatureDependency final {
  core::features::FeatureKind kind{
      core::features::FeatureKind::OrderBookImbalance};
  contracts::VersionRef definition_version;

  bool operator==(const FeatureDependency &) const = default;
};

struct StrategyParameterSchema final {
  contracts::DefinitionId parameter_id;
  contracts::VersionRef definition_version;
  contracts::DecimalScale scale;

  bool operator==(const StrategyParameterSchema &) const = default;
};

struct StrategyParameter final {
  contracts::DefinitionId parameter_id;
  contracts::VersionRef definition_version;
  contracts::AmountUnits units{};
  contracts::DecimalScale scale;

  bool operator==(const StrategyParameter &) const = default;
};

struct StrategyResourceLimits final {
  std::uint64_t maximum_operations{};
  std::size_t maximum_features{};
  std::size_t maximum_parameters{};
  std::size_t maximum_explanation_factors{};
  std::size_t maximum_working_bytes{};

  bool operator==(const StrategyResourceLimits &) const = default;
};

struct StrategyDescriptor final {
  contracts::VersionRef definition_version;
  contracts::VersionRef implementation_version;
  StrategyScope scope{StrategyScope::SingleListing};
  StrategyFamily family{StrategyFamily::OrderBookImbalance};
  std::span<const FeatureDependency> required_features;
  std::span<const StrategyParameterSchema> parameter_schema;
  contracts::VersionRef arithmetic_version;
  contracts::VersionRef explanation_policy_version;
  StrategyResourceLimits resource_limits;
};

struct LogicalCut final {
  std::uint64_t run_input_sequence{};
  std::int64_t logical_time_nanoseconds{};
  contracts::StreamCursor run_timer_cursor;
  std::uint64_t configuration_epoch{};
  std::optional<std::uint64_t> effective_control_position;
  std::optional<std::int64_t> logical_deadline_nanoseconds;

  bool operator==(const LogicalCut &) const = default;
};

struct StrategyInvocation final {
  contracts::RunId run_id;
  contracts::StrategyInstanceId strategy_instance_id;
  contracts::ListingId listing_id;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  std::span<const core::features::FeatureEvaluation> features;
  std::span<const StrategyParameter> parameters;
  LogicalCut cut;
};

class DeterministicOperationBudget final {
public:
  explicit constexpr DeterministicOperationBudget(
      std::uint64_t maximum_operations) noexcept
      : maximum_operations_(maximum_operations) {}

  [[nodiscard]] bool consume(std::uint64_t operations = 1) noexcept;
  [[nodiscard]] constexpr std::uint64_t maximum_operations() const noexcept {
    return maximum_operations_;
  }
  [[nodiscard]] constexpr std::uint64_t consumed_operations() const noexcept {
    return consumed_operations_;
  }
  [[nodiscard]] constexpr std::uint64_t remaining_operations() const noexcept {
    return maximum_operations_ - consumed_operations_;
  }
  [[nodiscard]] constexpr bool exhausted() const noexcept {
    return consumed_operations_ == maximum_operations_;
  }

private:
  std::uint64_t maximum_operations_{};
  std::uint64_t consumed_operations_{};
};

struct ExplanationFactor final {
  contracts::DefinitionId factor_id;
  std::uint32_t rank{};
  ExplanationSource source{ExplanationSource::Feature};
  ExplanationRole role{ExplanationRole::SupportsPositive};
  contracts::AmountUnits observed_units{};
  contracts::DecimalScale observed_scale;
  contracts::AmountUnits signed_contribution_units{};
  contracts::DecimalScale contribution_scale;
  std::optional<contracts::FeatureEvaluationId> causal_feature_evaluation_id;
  contracts::VersionRef ranking_policy_version;

  bool operator==(const ExplanationFactor &) const = default;
};

struct SignalDraft final {
  StrategyDirection direction{StrategyDirection::Positive};
  core::features::ScaledRatio strength;
  std::int64_t horizon_nanoseconds{};
  std::optional<contracts::Price> reference_price;

  bool operator==(const SignalDraft &) const = default;
};

struct AbstentionDraft final {
  StrategyAbstentionReason reason{
      StrategyAbstentionReason::MissingDeclaredFeature};

  bool operator==(const AbstentionDraft &) const = default;
};

using TerminalDraft = std::variant<SignalDraft, AbstentionDraft>;

class StrategyOutput final {
public:
  explicit StrategyOutput(
      std::span<std::optional<ExplanationFactor>> factor_storage) noexcept
      : factor_storage_(factor_storage) {}

  [[nodiscard]] bool append_factor(ExplanationFactor factor) noexcept;
  [[nodiscard]] bool emit_signal(SignalDraft signal) noexcept;
  [[nodiscard]] bool abstain(AbstentionDraft abstention) noexcept;

  [[nodiscard]] const std::optional<TerminalDraft> &terminal() const noexcept {
    return terminal_;
  }
  [[nodiscard]] std::size_t factor_count() const noexcept {
    return factor_count_;
  }
  [[nodiscard]] const ExplanationFactor &
  factor(std::size_t index) const noexcept {
    return *factor_storage_[index];
  }

private:
  std::span<std::optional<ExplanationFactor>> factor_storage_;
  std::size_t factor_count_{};
  std::optional<TerminalDraft> terminal_;
};

class Strategy {
public:
  virtual ~Strategy() = default;

  [[nodiscard]] virtual const StrategyDescriptor &
  descriptor() const noexcept = 0;
  [[nodiscard]] virtual StrategyExecutionStatus
  evaluate(const StrategyInvocation &invocation,
           DeterministicOperationBudget &budget,
           StrategyOutput &output) const noexcept = 0;
};

[[nodiscard]] bool
validate_descriptor(const StrategyDescriptor &descriptor) noexcept;
[[nodiscard]] bool logical_deadline_exceeded(const LogicalCut &cut) noexcept;

} // namespace chronos::strategies::sdk
