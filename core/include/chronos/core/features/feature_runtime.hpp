#pragma once

#include "chronos/contracts/digest.hpp"
#include "chronos/contracts/fixed_point.hpp"
#include "chronos/contracts/state_lineage.hpp"
#include "chronos/core/market_state/listing_view_publisher.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace chronos::core::features {

inline constexpr bool kDiagnosticFeatureObservationsActivated = false;

enum class FeatureKind : std::uint8_t {
  OrderBookImbalance,
  Microprice,
  Spread,
};

enum class FeatureDisposition : std::uint8_t {
  ValidObservation,
  Unavailable,
};

enum class FeatureUnavailableReason : std::uint8_t {
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

enum class FeatureRuntimeFailure : std::uint8_t {
  None,
  WrongRun,
  WrongListing,
  BundleMembershipMismatch,
  CutMismatch,
  IncompatibleSchema,
  IncompatibleArithmetic,
  IncompatibleCanonicalization,
  IncompatibleIdentityPolicy,
  IncompatibleReference,
};

struct ScaledRatio final {
  contracts::AmountUnits units{};
  contracts::DecimalScale scale;

  bool operator==(const ScaledRatio &) const = default;
};

using FeatureValue = std::variant<ScaledRatio, contracts::Price>;

struct FeatureRuntimeConfig final {
  contracts::RunId run_id;
  contracts::ListingId listing_id;
  contracts::VersionRef imbalance_definition_version;
  contracts::VersionRef microprice_definition_version;
  contracts::VersionRef spread_definition_version;
  contracts::VersionRef implementation_version;
  contracts::VersionRef required_view_schema_version;
  contracts::VersionRef required_view_capability_version;
  contracts::VersionRef required_bundle_schema_version;
  contracts::VersionRef required_input_arithmetic_version;
  contracts::VersionRef required_input_canonicalization_version;
  contracts::VersionRef required_input_identity_policy_version;
  contracts::VersionRef feature_arithmetic_version;
  contracts::VersionRef canonicalization_version;
  contracts::VersionRef identity_policy_version;
};

struct FeatureProvenance final {
  contracts::RunId run_id;
  contracts::ListingId listing_id;
  contracts::StateViewId bundle_id;
  contracts::StateViewId listing_view_id;
  std::uint64_t run_input_sequence{};
  contracts::RunInputSelectionId causing_selection_id;
  contracts::EventId causing_event_id;
  std::int64_t logical_time_nanoseconds{};
  std::uint64_t configuration_epoch{};
  std::optional<std::uint64_t> effective_control_position;
  std::optional<contracts::EventId> active_control_outcome_id;
  std::optional<contracts::Sha256Digest>
      active_control_selection_semantic_checksum;
  contracts::StreamCursor run_control_cursor;
  contracts::StreamCursor run_timer_cursor;
  contracts::Sha256Digest selection_semantic_checksum;
  contracts::StateLineage lineage;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  contracts::VersionRef reference_snapshot_version;
  contracts::VersionRef listing_definition_version;
  contracts::VersionRef reference_configuration_lineage_version;
  contracts::VersionRef input_view_schema_version;
  contracts::VersionRef input_view_capability_version;
  contracts::VersionRef input_bundle_schema_version;
  contracts::VersionRef input_arithmetic_version;
  contracts::VersionRef input_canonicalization_version;
  contracts::VersionRef input_identity_policy_version;
  contracts::VersionRef input_merge_policy_version;
  contracts::VersionRef input_registry_snapshot_version;
  contracts::VersionRef feature_definition_version;
  contracts::VersionRef implementation_version;
  contracts::VersionRef feature_arithmetic_version;
  contracts::VersionRef canonicalization_version;
  contracts::VersionRef identity_policy_version;
  contracts::Sha256Digest input_view_semantic_checksum;
  contracts::Sha256Digest input_bundle_semantic_checksum;

  bool operator==(const FeatureProvenance &) const = default;
};

struct FeatureObservation final {
  contracts::FeatureObservationId observation_id;
  FeatureKind kind{FeatureKind::OrderBookImbalance};
  FeatureProvenance provenance;
  FeatureValue value;
  contracts::Sha256Digest semantic_checksum;

  bool operator==(const FeatureObservation &) const = default;
};

struct FeatureUnavailable final {
  contracts::FeatureUnavailableId unavailable_id;
  FeatureKind kind{FeatureKind::OrderBookImbalance};
  FeatureProvenance provenance;
  FeatureUnavailableReason reason{FeatureUnavailableReason::BookUnavailable};
  contracts::Sha256Digest semantic_checksum;

  bool operator==(const FeatureUnavailable &) const = default;
};

struct FeatureEvaluation final {
  contracts::FeatureEvaluationId evaluation_id;
  FeatureKind kind{FeatureKind::OrderBookImbalance};
  FeatureDisposition disposition{FeatureDisposition::Unavailable};
  std::optional<FeatureObservation> observation;
  std::optional<FeatureUnavailable> unavailable;
  contracts::Sha256Digest semantic_checksum;

  bool operator==(const FeatureEvaluation &) const = default;
};

class AcceptedFeatureEvaluationCut final {
public:
  [[nodiscard]] const std::vector<FeatureEvaluation> &
  evaluations() const noexcept {
    return *evaluations_;
  }
  [[nodiscard]] const std::optional<dispatch::AcceptedControlOutcome> &
  accepted_control_outcome() const noexcept {
    return accepted_control_outcome_;
  }

  bool operator==(const AcceptedFeatureEvaluationCut &other) const noexcept {
    return evaluations() == other.evaluations() &&
           accepted_control_outcome_ == other.accepted_control_outcome_;
  }

private:
  explicit AcceptedFeatureEvaluationCut(
      std::shared_ptr<const std::vector<FeatureEvaluation>> evaluations,
      std::optional<dispatch::AcceptedControlOutcome> control)
      : evaluations_(std::move(evaluations)),
        accepted_control_outcome_(std::move(control)) {}

  std::shared_ptr<const std::vector<FeatureEvaluation>> evaluations_;
  std::optional<dispatch::AcceptedControlOutcome> accepted_control_outcome_;

  friend class FeatureRuntime;
};

class FeatureRuntimeResult final {
public:
  FeatureRuntimeFailure failure{FeatureRuntimeFailure::None};

  [[nodiscard]] bool ok() const noexcept {
    return failure == FeatureRuntimeFailure::None && accepted_cut_.has_value();
  }
  [[nodiscard]] const std::vector<FeatureEvaluation> &
  evaluations() const noexcept {
    static const std::vector<FeatureEvaluation> empty;
    return accepted_cut_ ? accepted_cut_->evaluations() : empty;
  }
  [[nodiscard]] const AcceptedFeatureEvaluationCut *
  accepted_cut() const noexcept {
    return accepted_cut_ ? &*accepted_cut_ : nullptr;
  }

  bool operator==(const FeatureRuntimeResult &) const = default;

private:
  std::optional<AcceptedFeatureEvaluationCut> accepted_cut_;

  friend class FeatureRuntime;
};

class FeatureRuntime final {
public:
  static constexpr std::uint8_t kImbalanceScaleExponent = 6;

  explicit FeatureRuntime(FeatureRuntimeConfig config);

  [[nodiscard]] FeatureRuntimeResult
  evaluate(const market_state::AcceptedFeatureCut &cut) const;

  [[nodiscard]] const FeatureRuntimeConfig &config() const noexcept;
  [[nodiscard]] static constexpr std::string_view
  feature_name(FeatureKind kind) noexcept {
    switch (kind) {
    case FeatureKind::OrderBookImbalance:
      return "order-book-imbalance-top-v1";
    case FeatureKind::Microprice:
      return "microprice-top-quantity-weighted-v1";
    case FeatureKind::Spread:
      return "spread-top-exact-v1";
    }
    return "unknown";
  }

private:
  FeatureRuntimeConfig config_;
};

} // namespace chronos::core::features
