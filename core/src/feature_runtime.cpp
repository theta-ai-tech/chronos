#include "chronos/core/features/feature_runtime.hpp"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace chronos::core::features {
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

void append_version(std::vector<std::byte> &output,
                    const contracts::VersionRef &value) {
  append_id(output, value.definition_id());
  append_integer(output, value.version());
}

void append_cursor(std::vector<std::byte> &output,
                   const contracts::StreamCursor &cursor) {
  append_id(output, cursor.stream_id());
  append_integer(output, cursor.stream_epoch());
  append_integer<std::uint8_t>(
      output, cursor.last_consumed_sequence().has_value() ? 1 : 0);
  if (cursor.last_consumed_sequence())
    append_integer(output, *cursor.last_consumed_sequence());
}

void append_optional_u64(std::vector<std::byte> &output,
                         const std::optional<std::uint64_t> &value) {
  append_integer<std::uint8_t>(output, value.has_value() ? 1 : 0);
  if (value)
    append_integer(output, *value);
}

void append_provenance(std::vector<std::byte> &output,
                       const FeatureProvenance &value) {
  append_id(output, value.run_id);
  append_id(output, value.listing_id);
  append_id(output, value.bundle_id);
  append_id(output, value.listing_view_id);
  append_integer(output, value.run_input_sequence);
  append_id(output, value.causing_selection_id);
  append_id(output, value.causing_event_id);
  append_integer(output, value.logical_time_nanoseconds);
  append_integer(output, value.configuration_epoch);
  append_optional_u64(output, value.effective_control_position);
  append_id(output, value.lineage.run_id());
  append_integer(output, value.lineage.run_input_sequence());
  append_integer(output,
                 static_cast<std::uint64_t>(value.lineage.cursors().size()));
  for (const auto &cursor : value.lineage.cursors())
    append_cursor(output, cursor);
  append_id(output, value.canonical_instrument_id);
  append_version(output, value.reference_snapshot_version);
  append_version(output, value.listing_definition_version);
  append_version(output, value.reference_configuration_lineage_version);
  append_version(output, value.input_view_schema_version);
  append_version(output, value.input_view_capability_version);
  append_version(output, value.input_bundle_schema_version);
  append_version(output, value.input_arithmetic_version);
  append_version(output, value.input_canonicalization_version);
  append_version(output, value.input_identity_policy_version);
  append_version(output, value.input_merge_policy_version);
  append_version(output, value.input_registry_snapshot_version);
  append_version(output, value.feature_definition_version);
  append_version(output, value.implementation_version);
  append_version(output, value.feature_arithmetic_version);
  append_version(output, value.canonicalization_version);
  append_version(output, value.identity_policy_version);
  append_digest(output, value.input_view_semantic_checksum);
  append_digest(output, value.input_bundle_semantic_checksum);
}

template <typename Id>
Id id_from_digest(const contracts::Sha256Digest &checksum) {
  typename Id::bytes_type bytes{};
  std::copy_n(checksum.bytes.begin(), bytes.size(), bytes.begin());
  return Id::from_bytes(bytes).value();
}

contracts::VersionRef definition_version(const FeatureRuntimeConfig &config,
                                         FeatureKind kind) {
  switch (kind) {
  case FeatureKind::OrderBookImbalance:
    return config.imbalance_definition_version;
  case FeatureKind::Microprice:
    return config.microprice_definition_version;
  case FeatureKind::Spread:
    return config.spread_definition_version;
  }
  return config.imbalance_definition_version;
}

FeatureProvenance provenance(const FeatureRuntimeConfig &config,
                             FeatureKind kind,
                             const market_state::StateViewBundle &bundle,
                             const market_state::ListingStateView &view) {
  return {
      .run_id = view.run_id,
      .listing_id = view.listing_id,
      .bundle_id = bundle.bundle_id,
      .listing_view_id = view.view_id,
      .run_input_sequence = bundle.run_input_sequence,
      .causing_selection_id = bundle.causing_selection_id,
      .causing_event_id = bundle.causing_event_id,
      .logical_time_nanoseconds = bundle.logical_time_nanoseconds,
      .configuration_epoch = bundle.configuration_epoch,
      .effective_control_position = bundle.effective_control_position,
      .lineage = view.lineage,
      .canonical_instrument_id = view.canonical_instrument_id,
      .reference_snapshot_version = view.reference_snapshot_version,
      .listing_definition_version = view.listing_definition_version,
      .reference_configuration_lineage_version =
          view.reference_configuration_lineage_version,
      .input_view_schema_version = view.view_schema_version,
      .input_view_capability_version = view.capability_version,
      .input_bundle_schema_version = bundle.bundle_schema_version,
      .input_arithmetic_version = view.arithmetic_version,
      .input_canonicalization_version = view.canonicalization_version,
      .input_identity_policy_version = bundle.identity_policy_version,
      .input_merge_policy_version = bundle.merge_policy_version,
      .input_registry_snapshot_version = bundle.registry_snapshot_version,
      .feature_definition_version = definition_version(config, kind),
      .implementation_version = config.implementation_version,
      .feature_arithmetic_version = config.feature_arithmetic_version,
      .canonicalization_version = config.canonicalization_version,
      .identity_policy_version = config.identity_policy_version,
      .input_view_semantic_checksum = view.semantic_checksum,
      .input_bundle_semantic_checksum = bundle.semantic_checksum,
  };
}

FeatureObservation observation(FeatureKind kind, FeatureProvenance provenance,
                               FeatureValue value) {
  std::vector<std::byte> canonical;
  canonical.reserve(512);
  constexpr std::string_view domain = "chronos.feature-observation.v1";
  for (const auto character : domain)
    canonical.push_back(static_cast<std::byte>(character));
  append_enum(canonical, kind);
  append_provenance(canonical, provenance);
  append_integer(canonical, static_cast<std::uint8_t>(value.index()));
  if (const auto *ratio = std::get_if<ScaledRatio>(&value)) {
    append_integer(canonical, ratio->units);
    append_integer(canonical, ratio->scale.exponent());
  } else {
    const auto &price = std::get<contracts::Price>(value);
    append_integer(canonical, price.units());
    append_version(canonical, price.definition_ref());
  }
  const auto checksum = contracts::sha256(canonical);
  return {
      .observation_id =
          id_from_digest<contracts::FeatureObservationId>(checksum),
      .kind = kind,
      .provenance = std::move(provenance),
      .value = std::move(value),
      .semantic_checksum = checksum,
  };
}

FeatureUnavailable unavailable(FeatureKind kind, FeatureProvenance provenance,
                               FeatureUnavailableReason reason) {
  std::vector<std::byte> canonical;
  canonical.reserve(480);
  constexpr std::string_view domain = "chronos.feature-unavailable.v1";
  for (const auto character : domain)
    canonical.push_back(static_cast<std::byte>(character));
  append_enum(canonical, kind);
  append_provenance(canonical, provenance);
  append_enum(canonical, reason);
  const auto checksum = contracts::sha256(canonical);
  return {
      .unavailable_id =
          id_from_digest<contracts::FeatureUnavailableId>(checksum),
      .kind = kind,
      .provenance = std::move(provenance),
      .reason = reason,
      .semantic_checksum = checksum,
  };
}

FeatureEvaluation evaluate_observation(FeatureObservation value) {
  std::vector<std::byte> canonical;
  canonical.reserve(80);
  constexpr std::string_view domain = "chronos.feature-evaluation.v1";
  for (const auto character : domain)
    canonical.push_back(static_cast<std::byte>(character));
  append_enum(canonical, value.kind);
  append_enum(canonical, FeatureDisposition::ValidObservation);
  append_id(canonical, value.observation_id);
  append_digest(canonical, value.semantic_checksum);
  const auto checksum = contracts::sha256(canonical);
  return {
      .evaluation_id = id_from_digest<contracts::FeatureEvaluationId>(checksum),
      .kind = value.kind,
      .disposition = FeatureDisposition::ValidObservation,
      .observation = std::move(value),
      .unavailable = std::nullopt,
      .semantic_checksum = checksum,
  };
}

FeatureEvaluation evaluate_unavailable(FeatureUnavailable value) {
  std::vector<std::byte> canonical;
  canonical.reserve(80);
  constexpr std::string_view domain = "chronos.feature-evaluation.v1";
  for (const auto character : domain)
    canonical.push_back(static_cast<std::byte>(character));
  append_enum(canonical, value.kind);
  append_enum(canonical, FeatureDisposition::Unavailable);
  append_id(canonical, value.unavailable_id);
  append_digest(canonical, value.semantic_checksum);
  const auto checksum = contracts::sha256(canonical);
  return {
      .evaluation_id = id_from_digest<contracts::FeatureEvaluationId>(checksum),
      .kind = value.kind,
      .disposition = FeatureDisposition::Unavailable,
      .observation = std::nullopt,
      .unavailable = std::move(value),
      .semantic_checksum = checksum,
  };
}

FeatureEvaluation
unavailable_evaluation(const FeatureRuntimeConfig &config, FeatureKind kind,
                       const market_state::StateViewBundle &bundle,
                       const market_state::ListingStateView &view,
                       FeatureUnavailableReason reason) {
  return evaluate_unavailable(
      unavailable(kind, provenance(config, kind, bundle, view), reason));
}

std::optional<FeatureUnavailableReason>
book_unavailable_reason(const market_state::ListingStateView &view) {
  using market_state::BookSynchronization;
  switch (view.quality.book_synchronization) {
  case BookSynchronization::Synchronized:
    break;
  case BookSynchronization::Starting:
    return FeatureUnavailableReason::BookStarting;
  case BookSynchronization::Recovering:
    return FeatureUnavailableReason::BookRecovering;
  case BookSynchronization::Gapped:
    return FeatureUnavailableReason::BookGapped;
  case BookSynchronization::Invalid:
    return FeatureUnavailableReason::BookInvalid;
  case BookSynchronization::Unavailable:
    return FeatureUnavailableReason::BookUnavailable;
  case BookSynchronization::Closed:
    return FeatureUnavailableReason::BookClosed;
  }

  using market_state::FreshnessStatus;
  switch (view.quality.book_freshness) {
  case FreshnessStatus::Fresh:
    break;
  case FreshnessStatus::Stale:
    return FeatureUnavailableReason::BookStale;
  case FreshnessStatus::Unknown:
    return FeatureUnavailableReason::BookFreshnessUnknown;
  case FreshnessStatus::Closed:
    return FeatureUnavailableReason::BookClosed;
  }

  const auto proven = [](market_state::L2SideCompleteness completeness) {
    return completeness == market_state::L2SideCompleteness::Complete ||
           completeness ==
               market_state::L2SideCompleteness::BoundedWithProvenTop;
  };
  if (!proven(view.top.bid_completeness) ||
      !proven(view.top.ask_completeness)) {
    return FeatureUnavailableReason::TopNotProven;
  }
  if (view.top.shape != market_state::L2BookShape::Normal &&
      view.top.shape != market_state::L2BookShape::Locked) {
    return FeatureUnavailableReason::UnsupportedBookShape;
  }
  if (!view.top.best_bid || !view.top.best_ask || !view.top.spread ||
      view.bids.empty() || view.asks.empty()) {
    return FeatureUnavailableReason::TopUnavailable;
  }
  return std::nullopt;
}

std::optional<FeatureUnavailableReason>
price_definition_reason(const market_state::ListingStateView &view) {
  const auto &bid = *view.top.best_bid;
  const auto &ask = *view.top.best_ask;
  if (bid.price.definition_ref() != ask.price.definition_ref() ||
      view.top.spread->definition_ref() != bid.price.definition_ref()) {
    return FeatureUnavailableReason::DefinitionMismatch;
  }
  return std::nullopt;
}

std::optional<FeatureUnavailableReason>
quantity_reason(const market_state::ListingStateView &view) {
  const auto &bid = *view.top.best_bid;
  const auto &ask = *view.top.best_ask;
  if (bid.quantity.definition_ref() != ask.quantity.definition_ref())
    return FeatureUnavailableReason::DefinitionMismatch;
  if (bid.quantity.units() <= 0 || ask.quantity.units() <= 0)
    return FeatureUnavailableReason::InvalidQuantity;
  contracts::AmountUnits total{};
  if (__builtin_add_overflow(bid.quantity.units(), ask.quantity.units(),
                             &total) ||
      total <= 0) {
    return FeatureUnavailableReason::ArithmeticOverflow;
  }
  return std::nullopt;
}

FeatureRuntimeFailure validate_cut(const FeatureRuntimeConfig &config,
                                   const market_state::StateViewBundle &bundle,
                                   const market_state::ListingStateView &view) {
  if (bundle.run_id != config.run_id || view.run_id != config.run_id)
    return FeatureRuntimeFailure::WrongRun;
  if (view.listing_id != config.listing_id ||
      bundle.listing_id != config.listing_id)
    return FeatureRuntimeFailure::WrongListing;
  const auto member =
      std::find(bundle.listing_views.begin(), bundle.listing_views.end(),
                std::pair{view.listing_id, view.view_id});
  if (member == bundle.listing_views.end() ||
      bundle.listing_view_id != view.view_id)
    return FeatureRuntimeFailure::BundleMembershipMismatch;
  if (bundle.run_input_sequence != view.lineage.run_input_sequence() ||
      bundle.causing_selection_id != view.causing_selection_id ||
      bundle.causing_event_id != view.causing_event_id ||
      bundle.selection_semantic_checksum != view.selection_semantic_checksum ||
      bundle.merge_policy_version != view.merge_policy_version ||
      bundle.configuration_epoch != view.configuration_epoch ||
      bundle.effective_control_position != view.effective_control_position ||
      bundle.logical_time_nanoseconds !=
          view.quality.logical_time_nanoseconds ||
      view.bids.empty() != !view.top.best_bid.has_value() ||
      view.asks.empty() != !view.top.best_ask.has_value() ||
      (view.top.best_bid && view.bids.front() != *view.top.best_bid) ||
      (view.top.best_ask && view.asks.front() != *view.top.best_ask)) {
    return FeatureRuntimeFailure::CutMismatch;
  }
  if (view.top.best_bid && view.top.best_ask && view.top.spread) {
    const auto derived_spread =
        view.top.best_ask->price.checked_subtract(view.top.best_bid->price);
    if (!derived_spread || *derived_spread != *view.top.spread)
      return FeatureRuntimeFailure::CutMismatch;
  }
  if (view.view_schema_version != config.required_view_schema_version ||
      view.capability_version != config.required_view_capability_version ||
      bundle.view_schema_version != config.required_view_schema_version ||
      bundle.bundle_schema_version != config.required_bundle_schema_version)
    return FeatureRuntimeFailure::IncompatibleSchema;
  if (view.arithmetic_version != config.required_input_arithmetic_version ||
      bundle.arithmetic_version != config.required_input_arithmetic_version)
    return FeatureRuntimeFailure::IncompatibleArithmetic;
  if (view.canonicalization_version !=
          config.required_input_canonicalization_version ||
      bundle.canonicalization_version !=
          config.required_input_canonicalization_version)
    return FeatureRuntimeFailure::IncompatibleCanonicalization;
  if (bundle.identity_policy_version !=
      config.required_input_identity_policy_version)
    return FeatureRuntimeFailure::IncompatibleIdentityPolicy;
  if (bundle.canonical_instrument_id != view.canonical_instrument_id ||
      bundle.reference_snapshot_version != view.reference_snapshot_version ||
      bundle.listing_definition_version != view.listing_definition_version ||
      bundle.reference_configuration_lineage_version !=
          view.reference_configuration_lineage_version)
    return FeatureRuntimeFailure::IncompatibleReference;
  return FeatureRuntimeFailure::None;
}

} // namespace

FeatureRuntime::FeatureRuntime(FeatureRuntimeConfig config)
    : config_(std::move(config)) {}

FeatureRuntimeResult
FeatureRuntime::evaluate(const market_state::AcceptedFeatureCut &cut) const {
  const auto &bundle = cut.bundle();
  const auto &view = cut.view();
  FeatureRuntimeResult result;
  result.failure = validate_cut(config_, bundle, view);
  if (!result.ok())
    return result;
  result.evaluations.reserve(3);

  const auto book_reason = book_unavailable_reason(view);
  if (book_reason) {
    for (const auto kind : {FeatureKind::OrderBookImbalance,
                            FeatureKind::Microprice, FeatureKind::Spread}) {
      result.evaluations.push_back(
          unavailable_evaluation(config_, kind, bundle, view, *book_reason));
    }
    return result;
  }

  const auto price_reason = price_definition_reason(view);
  const auto quantity_failure = quantity_reason(view);
  const auto &bid = *view.top.best_bid;
  const auto &ask = *view.top.best_ask;

  if (quantity_failure) {
    result.evaluations.push_back(
        unavailable_evaluation(config_, FeatureKind::OrderBookImbalance, bundle,
                               view, *quantity_failure));
  } else {
    contracts::AmountUnits difference{};
    contracts::AmountUnits total{};
    const auto overflow =
        __builtin_sub_overflow(bid.quantity.units(), ask.quantity.units(),
                               &difference) ||
        __builtin_add_overflow(bid.quantity.units(), ask.quantity.units(),
                               &total);
    const auto units = overflow
                           ? std::nullopt
                           : contracts::checked_multiply_divide(
                                 difference, 1'000'000, total,
                                 contracts::RoundingMode::nearest_ties_to_even);
    if (!units) {
      result.evaluations.push_back(unavailable_evaluation(
          config_, FeatureKind::OrderBookImbalance, bundle, view,
          FeatureUnavailableReason::ArithmeticOverflow));
    } else {
      result.evaluations.push_back(evaluate_observation(observation(
          FeatureKind::OrderBookImbalance,
          provenance(config_, FeatureKind::OrderBookImbalance, bundle, view),
          ScaledRatio{.units = *units,
                      .scale = *contracts::DecimalScale::from_exponent(
                          kImbalanceScaleExponent)})));
    }
  }

  if (price_reason || quantity_failure) {
    result.evaluations.push_back(unavailable_evaluation(
        config_, FeatureKind::Microprice, bundle, view,
        price_reason ? *price_reason : *quantity_failure));
  } else {
    contracts::AmountUnits total{};
    const auto total_overflow = __builtin_add_overflow(
        bid.quantity.units(), ask.quantity.units(), &total);
    const auto weighted_units =
        total_overflow ? std::nullopt
                       : contracts::checked_weighted_average(
                             ask.price.units(), bid.quantity.units(),
                             bid.price.units(), ask.quantity.units(), total,
                             contracts::RoundingMode::nearest_ties_to_even);
    const auto weighted = weighted_units
                              ? contracts::Price::from_units(
                                    *weighted_units, bid.price.definition_ref())
                              : std::nullopt;
    if (!weighted) {
      result.evaluations.push_back(
          unavailable_evaluation(config_, FeatureKind::Microprice, bundle, view,
                                 FeatureUnavailableReason::ArithmeticOverflow));
    } else {
      result.evaluations.push_back(evaluate_observation(observation(
          FeatureKind::Microprice,
          provenance(config_, FeatureKind::Microprice, bundle, view),
          *weighted)));
    }
  }

  if (price_reason) {
    result.evaluations.push_back(unavailable_evaluation(
        config_, FeatureKind::Spread, bundle, view, *price_reason));
  } else {
    const auto spread = ask.price.checked_subtract(bid.price);
    if (!spread || spread->units() < 0) {
      result.evaluations.push_back(
          unavailable_evaluation(config_, FeatureKind::Spread, bundle, view,
                                 FeatureUnavailableReason::ArithmeticOverflow));
    } else {
      result.evaluations.push_back(evaluate_observation(observation(
          FeatureKind::Spread,
          provenance(config_, FeatureKind::Spread, bundle, view), *spread)));
    }
  }
  return result;
}

const FeatureRuntimeConfig &FeatureRuntime::config() const noexcept {
  return config_;
}

} // namespace chronos::core::features
