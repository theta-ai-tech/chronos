#pragma once

#include "chronos/contracts/digest.hpp"
#include "chronos/contracts/fixed_point.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

namespace chronos::strategies::sdk {

inline constexpr std::size_t kThresholdProgramInstructions = 8;
inline constexpr std::size_t kThresholdProgramFactors = 2;
inline constexpr std::size_t kMaximumAcceptedFeatureEvaluations = 8;
inline constexpr std::size_t kInterpreterWorkingBytes = 128;
inline constexpr std::uint64_t kAdmissionOperations = 8;
inline constexpr std::uint64_t kMaximumEvaluationOperations =
    kAdmissionOperations + kThresholdProgramInstructions;

enum class StrategyScope : std::uint8_t { SingleListing };

enum class StrategyFamily : std::uint8_t {
  OrderBookImbalance,
  MicropriceSpread,
  ShortHorizonMomentum,
};

enum class StrategyFeatureKind : std::uint8_t {
  OrderBookImbalance,
  Microprice,
  Spread,
};

enum class StrategyDirection : std::uint8_t { Positive, Negative };

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
  NoDirectionalSignal,
};

enum class StrategyExecutionStatus : std::uint8_t {
  Completed,
  LogicalDeadlineExceeded,
  DeterministicBudgetExhausted,
  InsufficientWorkspace,
  OutputCapacityExceeded,
  ContractViolation,
};

struct FeatureDependency final {
  StrategyFeatureKind kind{StrategyFeatureKind::OrderBookImbalance};
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

struct ScaledRatio final {
  contracts::AmountUnits units{};
  contracts::DecimalScale scale;

  bool operator==(const ScaledRatio &) const = default;
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
  ScaledRatio strength;
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

enum class StrategyOpcode : std::uint8_t {
  LoadFeature,
  RequireValidScaledRatio,
  LoadParameter,
  RequirePositiveParameter,
  CompareAbsoluteFeatureAtLeastParameter,
  AppendFeatureFactor,
  AppendParameterFactor,
  FinishDirectionalThreshold,
};

struct StrategyInstruction final {
  StrategyOpcode opcode{StrategyOpcode::LoadFeature};
  std::uint16_t operand{};

  bool operator==(const StrategyInstruction &) const = default;
};

struct ProgramFactorDefinition final {
  contracts::DefinitionId factor_id;
  ExplanationSource source{ExplanationSource::Feature};

  bool operator==(const ProgramFactorDefinition &) const = default;
};

struct StrategyProgram final {
  std::span<const StrategyInstruction> instructions;
  std::span<const ProgramFactorDefinition> factors;
  std::int64_t signal_horizon_nanoseconds{};
};

struct StrategyDefinition final {
  StrategyDescriptor descriptor;
  StrategyProgram program;
};

class AcceptedStrategyDefinition final {
public:
  AcceptedStrategyDefinition(const AcceptedStrategyDefinition &) = default;
  AcceptedStrategyDefinition(AcceptedStrategyDefinition &&) = default;
  AcceptedStrategyDefinition &
  operator=(const AcceptedStrategyDefinition &) = default;
  AcceptedStrategyDefinition &
  operator=(AcceptedStrategyDefinition &&) = default;

  [[nodiscard]] static std::optional<AcceptedStrategyDefinition>
  accept(const StrategyDefinition &candidate) noexcept;

  [[nodiscard]] StrategyDescriptor descriptor() const noexcept {
    return {
        .definition_version = definition_version_,
        .implementation_version = implementation_version_,
        .scope = scope_,
        .family = family_,
        .required_features = required_features_,
        .parameter_schema = parameter_schema_,
        .arithmetic_version = arithmetic_version_,
        .explanation_policy_version = explanation_policy_version_,
        .resource_limits = resource_limits_,
    };
  }
  [[nodiscard]] StrategyProgram program() const noexcept {
    return {
        .instructions = instructions_,
        .factors = factors_,
        .signal_horizon_nanoseconds = signal_horizon_nanoseconds_,
    };
  }

private:
  AcceptedStrategyDefinition(
      const StrategyDescriptor &descriptor, FeatureDependency required_feature,
      StrategyParameterSchema parameter_schema,
      std::array<StrategyInstruction, kThresholdProgramInstructions>
          instructions,
      std::array<ProgramFactorDefinition, kThresholdProgramFactors> factors,
      std::int64_t signal_horizon_nanoseconds)
      : definition_version_(descriptor.definition_version),
        implementation_version_(descriptor.implementation_version),
        scope_(descriptor.scope), family_(descriptor.family),
        required_features_{required_feature},
        parameter_schema_{parameter_schema},
        arithmetic_version_(descriptor.arithmetic_version),
        explanation_policy_version_(descriptor.explanation_policy_version),
        resource_limits_(descriptor.resource_limits),
        instructions_(instructions), factors_(factors),
        signal_horizon_nanoseconds_(signal_horizon_nanoseconds) {}

  contracts::VersionRef definition_version_;
  contracts::VersionRef implementation_version_;
  StrategyScope scope_;
  StrategyFamily family_;
  std::array<FeatureDependency, 1> required_features_;
  std::array<StrategyParameterSchema, 1> parameter_schema_;
  contracts::VersionRef arithmetic_version_;
  contracts::VersionRef explanation_policy_version_;
  StrategyResourceLimits resource_limits_;
  std::array<StrategyInstruction, kThresholdProgramInstructions> instructions_;
  std::array<ProgramFactorDefinition, kThresholdProgramFactors> factors_;
  std::int64_t signal_horizon_nanoseconds_{};
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

private:
  std::uint64_t maximum_operations_{};
  std::uint64_t consumed_operations_{};
};

[[nodiscard]] bool
validate_descriptor(const StrategyDescriptor &descriptor) noexcept;
[[nodiscard]] bool
validate_definition(const StrategyDefinition &definition) noexcept;
[[nodiscard]] bool logical_deadline_exceeded(const LogicalCut &cut) noexcept;

} // namespace chronos::strategies::sdk
