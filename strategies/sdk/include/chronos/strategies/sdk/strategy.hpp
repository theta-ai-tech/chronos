#pragma once

#include "chronos/contracts/digest.hpp"
#include "chronos/contracts/fixed_point.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace chronos::strategies::sdk {

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

enum class StrategyFeatureDisposition : std::uint8_t {
  ValidObservation,
  Unavailable,
};

enum class StrategyFeatureUnavailableReason : std::uint8_t {
  BookStarting,
  BookRecovering,
  BookGapped,
  BookInvalid,
  BookUnavailable,
  BookClosed,
  BookStale,
  BookFreshnessUnknown,
  UnsupportedBookShape,
  TopNotProven,
  TopUnavailable,
  InvalidQuantity,
  DefinitionMismatch,
  ArithmeticOverflow,
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
  // Native strategies use one declared fixed charge per invocation. The host
  // charges it before entering strategy code, so fuel cannot be ignored.
  std::uint64_t operations_per_evaluation{};
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

using StrategyFeatureValue = std::variant<ScaledRatio, contracts::Price>;

struct StrategyFeatureProvenance final {
  contracts::RunId run_id;
  contracts::ListingId listing_id;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  std::uint64_t run_input_sequence{};
  std::int64_t logical_time_nanoseconds{};
  std::uint64_t configuration_epoch{};
  std::optional<std::uint64_t> effective_control_position;
  contracts::VersionRef feature_definition_version;
  contracts::Sha256Digest input_view_semantic_checksum;
  contracts::Sha256Digest input_bundle_semantic_checksum;

  bool operator==(const StrategyFeatureProvenance &) const = default;
};

struct StrategyFeature final {
  contracts::FeatureEvaluationId evaluation_id;
  StrategyFeatureKind kind{StrategyFeatureKind::OrderBookImbalance};
  StrategyFeatureDisposition disposition{
      StrategyFeatureDisposition::Unavailable};
  StrategyFeatureProvenance provenance;
  std::optional<StrategyFeatureValue> value;
  std::optional<StrategyFeatureUnavailableReason> unavailable_reason;
  contracts::Sha256Digest semantic_checksum;

  bool operator==(const StrategyFeature &) const = default;
};

class AcceptedStrategyInvocation final {
public:
  [[nodiscard]] const contracts::RunId &run_id() const noexcept {
    return run_id_;
  }
  [[nodiscard]] const contracts::StrategyInstanceId &
  strategy_instance_id() const noexcept {
    return strategy_instance_id_;
  }
  [[nodiscard]] const contracts::ListingId &listing_id() const noexcept {
    return listing_id_;
  }
  [[nodiscard]] const contracts::CanonicalInstrumentId &
  canonical_instrument_id() const noexcept {
    return canonical_instrument_id_;
  }
  [[nodiscard]] std::span<const StrategyFeature> features() const noexcept {
    return features_;
  }
  [[nodiscard]] std::span<const StrategyParameter> parameters() const noexcept {
    return parameters_;
  }
  [[nodiscard]] const LogicalCut &cut() const noexcept { return cut_; }

private:
  AcceptedStrategyInvocation(
      contracts::RunId run_id,
      contracts::StrategyInstanceId strategy_instance_id,
      contracts::ListingId listing_id,
      contracts::CanonicalInstrumentId canonical_instrument_id,
      std::vector<StrategyFeature> features,
      std::vector<StrategyParameter> parameters, LogicalCut cut)
      : run_id_(run_id), strategy_instance_id_(strategy_instance_id),
        listing_id_(listing_id),
        canonical_instrument_id_(canonical_instrument_id),
        features_(std::move(features)), parameters_(std::move(parameters)),
        cut_(std::move(cut)) {}

  contracts::RunId run_id_;
  contracts::StrategyInstanceId strategy_instance_id_;
  contracts::ListingId listing_id_;
  contracts::CanonicalInstrumentId canonical_instrument_id_;
  std::vector<StrategyFeature> features_;
  std::vector<StrategyParameter> parameters_;
  LogicalCut cut_;

  friend class StrategyHost;
};

class DeterministicOperationBudget final {
public:
  explicit constexpr DeterministicOperationBudget(
      std::uint64_t available_operations) noexcept
      : available_operations_(available_operations) {}

  [[nodiscard]] bool charge(std::uint64_t operations) noexcept;
  [[nodiscard]] constexpr std::uint64_t available_operations() const noexcept {
    return available_operations_;
  }
  [[nodiscard]] constexpr std::uint64_t charged_operations() const noexcept {
    return charged_operations_;
  }

private:
  std::uint64_t available_operations_{};
  std::uint64_t charged_operations_{};
};

class StrategyWorkspace final {
public:
  [[nodiscard]] std::span<std::byte> bytes() const noexcept { return bytes_; }

private:
  explicit StrategyWorkspace(std::span<std::byte> bytes) noexcept
      : bytes_(bytes) {}

  std::span<std::byte> bytes_;

  friend class StrategyHost;
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

class StrategyOutput final {
public:
  explicit StrategyOutput(
      std::span<std::optional<ExplanationFactor>> factor_storage) noexcept;

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
  evaluate(const AcceptedStrategyInvocation &invocation,
           StrategyWorkspace &workspace,
           StrategyOutput &output) const noexcept = 0;
};

[[nodiscard]] bool
validate_descriptor(const StrategyDescriptor &descriptor) noexcept;
[[nodiscard]] bool logical_deadline_exceeded(const LogicalCut &cut) noexcept;

} // namespace chronos::strategies::sdk
