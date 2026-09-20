#include "chronos/adapters/market_data/bybit_book_decoder.hpp"
#include "chronos/adapters/market_data/bybit_trade_decoder.hpp"
#include "chronos/core/dispatch/run_input_dispatcher.hpp"
#include "chronos/core/market_state/listing_view_publisher.hpp"
#include "chronos/normalization/market_data/book_normalizer.hpp"
#include "chronos/normalization/market_data/trade_normalizer.hpp"
#include "chronos/runtime/datasets/replay.hpp"

#include "bybit_decode_support.hpp"
#include "chronos/applications/replay_runner/market_replay.hpp"
#include "chronos/core/features/feature_runtime.hpp"
#include "chronos/runtime/strategies/strategy_evaluation.hpp"
#include "chronos/runtime/strategies/strategy_runtime.hpp"
#include "chronos/strategies/generated/chronos_reference_strategies.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace chronos::applications::replay_runner {
namespace {
namespace adapter = chronos::adapters::market_data;
namespace sdk = chronos::adapters::sdk;
namespace contracts = chronos::contracts;
namespace dispatch = chronos::core::dispatch;
namespace state = chronos::core::market_state;
namespace reference = chronos::core::reference_data;
namespace market = chronos::normalization::market_data;
namespace replay = chronos::runtime::datasets;

enum class SourceTopicClass : std::uint8_t { Other, Relevant, InvalidJson };

SourceTopicClass classify_source_topic(std::string_view payload) {
  const adapter::detail::JsonLimits limits{.maximum_json_depth = 16,
                                           .maximum_json_nodes = 16384,
                                           .maximum_object_members = 64,
                                           .maximum_string_bytes = 128,
                                           .maximum_number_bytes = 32};
  adapter::detail::BoundedJsonParser parser(payload, limits);
  const auto root = parser.parse();
  if (!root || root->kind != adapter::detail::JsonKind::Object)
    return SourceTopicClass::InvalidJson;
  const auto topic =
      adapter::detail::string_value(adapter::detail::member(*root, "topic"));
  if (topic &&
      (topic->starts_with("orderbook.") || topic->starts_with("publicTrade.")))
    return SourceTopicClass::Relevant;
  return SourceTopicClass::Other;
}

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type value{};
  value.front() = seed;
  return Id::from_bytes(value).value();
}

contracts::VersionRef version(std::uint8_t seed, std::uint64_t number) {
  return contracts::VersionRef::from(id<contracts::DefinitionId>(seed), number)
      .value();
}

std::vector<std::byte> bytes(std::string_view value) {
  const auto *begin = reinterpret_cast<const std::byte *>(value.data());
  return {begin, begin + value.size()};
}

std::uint8_t hex_nibble(char value) {
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  return static_cast<std::uint8_t>(value - 'a' + 10);
}

contracts::Sha256Digest digest(std::string_view value) {
  contracts::Sha256Digest result;
  for (std::size_t index = 0; index < result.bytes.size(); ++index) {
    result.bytes[index] =
        static_cast<std::uint8_t>((hex_nibble(value[index * 2]) << 4U) |
                                  hex_nibble(value[index * 2 + 1]));
  }
  return result;
}

namespace features = chronos::core::features;
namespace strategy = chronos::runtime::strategies;
namespace strategy_sdk = chronos::strategies::sdk;
namespace recommendation = chronos::core::recommendation;
reference::ReferenceConfigurationLineage
reference_lineage(const adapter::DatasetReadResult &dataset) {
  const auto interval = reference::EffectiveInterval::from_capture_sequence(
                            dataset.manifest()->capture_partition_id,
                            dataset.manifest()->first_capture_sequence,
                            dataset.manifest()->last_capture_sequence + 1)
                            .value();
  auto snapshot =
      reference::ReferenceSnapshot::create(
          version(7, 2),
          {.instrument_id = id<contracts::CanonicalInstrumentId>(8),
           .version = version(9, 3),
           .base_asset = "BTC",
           .quote_asset = "USDT",
           .product_class = reference::ProductClass::LinearPerpetual,
           .effective_interval = interval},
          {.listing_id = id<contracts::ListingId>(10),
           .instrument_id = id<contracts::CanonicalInstrumentId>(8),
           .version = version(11, 4),
           .venue = "bybit",
           .environment = dataset.manifest()->environment ==
                                  sdk::EnvironmentClass::Production
                              ? reference::VenueEnvironment::Production
                              : reference::VenueEnvironment::Test,
           .source_symbol = "BTCUSDT",
           .status = reference::ListingStatus::Active,
           .price_tick = reference::DecimalIncrement::parse("0.10").value(),
           .quantity_step = reference::DecimalIncrement::parse("0.001").value(),
           .effective_interval = interval})
          .value();
  return reference::ReferenceConfigurationLineage::create(
             1, "reference-lineage-v1", "bybit-semantic-key-v1",
             "capture-sequence-v1", "exact-single-match-v1",
             {std::move(snapshot)})
      .value();
}

class SemanticWriter final {
public:
  void u64(std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index)
      output_.push_back(
          static_cast<std::byte>((value >> (index * 8U)) & 0xFFU));
  }

  void i64(std::int64_t value) { u64(static_cast<std::uint64_t>(value)); }
  void boolean(bool value) { u64(value ? 1U : 0U); }

  template <typename Enum> void enumeration(Enum value) {
    u64(static_cast<std::uint64_t>(value));
  }

  void string(std::string_view value) {
    u64(static_cast<std::uint64_t>(value.size()));
    const auto data = bytes(value);
    output_.insert(output_.end(), data.begin(), data.end());
  }

  template <typename Id> void identifier(const Id &value) {
    for (const auto byte : value.bytes())
      output_.push_back(static_cast<std::byte>(byte));
  }

  template <typename Id> void optional_id(const std::optional<Id> &value) {
    boolean(value.has_value());
    if (value.has_value())
      identifier(*value);
  }

  void version(const contracts::VersionRef &value) {
    identifier(value.definition_id());
    u64(value.version());
  }

  void digest(const contracts::Sha256Digest &value) {
    for (const auto byte : value.bytes)
      output_.push_back(static_cast<std::byte>(byte));
  }

  void payload_digest(const sdk::PayloadDigest &value) {
    for (const auto byte : value.bytes)
      output_.push_back(static_cast<std::byte>(byte));
    enumeration(value.coverage);
  }

  void time(const contracts::TimePoint &value) {
    i64(value.nanoseconds());
    identifier(value.clock_domain_id());
    enumeration(value.clock_class());
    u64(value.precision_nanoseconds());
  }

  void optional_bool(const std::optional<bool> &value) {
    boolean(value.has_value());
    if (value.has_value())
      boolean(*value);
  }

  void optional_u64(const std::optional<std::uint64_t> &value) {
    boolean(value.has_value());
    if (value.has_value())
      u64(*value);
  }

  template <typename Fixed> void fixed(const Fixed &value) {
    i64(value.units());
    version(value.definition_ref());
  }

  [[nodiscard]] std::vector<std::byte> take() { return std::move(output_); }

private:
  std::vector<std::byte> output_;
};

void append_extensions(
    SemanticWriter &output,
    const std::vector<market::SourceExtensionField> &values) {
  output.u64(static_cast<std::uint64_t>(values.size()));
  for (const auto &value : values) {
    output.string(value.json_pointer);
    output.string(value.canonical_json);
  }
}

void append_source_lineage(SemanticWriter &output,
                           const market::SourceCaptureLineage &value) {
  output.string(value.dataset_format_version);
  output.string(value.dataset_id);
  output.string(value.records_sha256);
  output.u64(value.dataset_record_index);
  output.identifier(value.source_event_id);
  output.identifier(value.capture_session_id);
  output.identifier(value.runtime_id);
  output.optional_id(value.connection_id);
  output.optional_id(value.subscription_id);
  output.identifier(value.capture_partition_id);
  output.u64(value.capture_sequence);
  output.time(value.chronos_receive_time);
  output.payload_digest(value.payload_digest);
  output.string(value.adapter_version);
  output.string(value.build_version);
  output.string(value.framing_version);
  output.string(value.static_configuration_version);
  output.string(value.capability_manifest_version);
  output.string(value.schema_policy_version);
}

void append_decode_evidence(SemanticWriter &output,
                            const market::SourceDecodeEvidence &value) {
  output.identifier(value.source_decode_enrichment_id);
  output.u64(value.source_member_index);
  output.string(value.decoder_version);
  output.string(value.source_schema_version);
  output.string(value.registry_version);
  output.string(value.canonicalization_version);
  output.payload_digest(value.semantic_checksum);
}

void append_reference_selection(
    SemanticWriter &output, const market::ReferenceSelectionEvidence &value) {
  output.version(value.reference_configuration_lineage_version);
  output.string(value.lineage_schema_version);
  output.string(value.semantic_key_policy_version);
  output.string(value.effective_basis_policy_version);
  output.string(value.selection_policy_version);
  output.string(value.semantic_key.venue);
  output.enumeration(value.semantic_key.environment);
  output.enumeration(value.semantic_key.product_class);
  output.string(value.semantic_key.source_listing_key);
  output.identifier(value.effective_capture_partition_id);
  output.u64(value.effective_capture_sequence);
}

void append_fact_header(SemanticWriter &output, std::uint64_t position,
                        std::string_view event_type,
                        const contracts::CanonicalInstrumentId &instrument_id,
                        const contracts::ListingId &listing_id,
                        const contracts::VersionRef &snapshot_version,
                        const contracts::VersionRef &instrument_version,
                        const contracts::VersionRef &listing_version,
                        const market::ReferenceSelectionEvidence &selection,
                        const market::SourceCaptureLineage &lineage,
                        const market::SourceDecodeEvidence &decode) {
  output.string("chronos-normalized-semantic-v1");
  output.u64(position);
  output.string(event_type);
  output.identifier(instrument_id);
  output.identifier(listing_id);
  output.version(snapshot_version);
  output.version(instrument_version);
  output.version(listing_version);
  append_reference_selection(output, selection);
  append_source_lineage(output, lineage);
  append_decode_evidence(output, decode);
}

std::vector<std::byte> encode_book(std::uint64_t position,
                                   const market::NormalizedBookFact &fact) {
  SemanticWriter output;
  append_fact_header(output, position, fact.event_type(),
                     fact.canonical_instrument_id, fact.listing_id,
                     fact.reference_snapshot_version, fact.instrument_version,
                     fact.listing_version, fact.reference_selection,
                     fact.source_lineage, fact.source_decode_evidence);
  const auto &assertions = fact.source_assertions;
  output.enumeration(assertions.kind);
  output.string(assertions.venue);
  output.enumeration(assertions.environment);
  output.enumeration(assertions.product_class);
  output.string(assertions.topic);
  output.string(assertions.source_symbol);
  output.u64(assertions.depth);
  output.u64(assertions.sequence);
  output.enumeration(assertions.sequence_scope);
  output.u64(assertions.update_id);
  output.enumeration(assertions.update_id_scope);
  output.u64(assertions.system_timestamp_milliseconds);
  output.u64(assertions.matching_timestamp_milliseconds);
  output.enumeration(assertions.timestamp_unit);
  append_extensions(output, fact.source_extensions);
  output.time(fact.source_event_time);
  output.string(fact.decoder_version);
  output.string(fact.source_schema_version);
  output.string(fact.normalizer_version);
  if (const auto *snapshot =
          std::get_if<market::BookSnapshotObservation>(&fact.payload)) {
    output.u64(0);
    const auto levels = [&](const std::vector<market::BookLevel> &values) {
      output.u64(static_cast<std::uint64_t>(values.size()));
      for (const auto &value : values) {
        output.fixed(value.price);
        output.fixed(value.quantity);
      }
    };
    levels(snapshot->bids);
    levels(snapshot->asks);
  } else {
    output.u64(1);
    const auto changes =
        [&](const std::vector<market::BookLevelChange> &values) {
          output.u64(static_cast<std::uint64_t>(values.size()));
          for (const auto &value : values) {
            output.fixed(value.price);
            output.fixed(value.quantity);
            output.enumeration(value.operation);
          }
        };
    const auto &delta = std::get<market::BookDeltaObservation>(fact.payload);
    changes(delta.bid_changes);
    changes(delta.ask_changes);
  }
  return output.take();
}

std::vector<std::byte> encode_trade(std::uint64_t position,
                                    const market::NormalizedTradeFact &fact) {
  SemanticWriter output;
  append_fact_header(output, position, fact.event_type(),
                     fact.canonical_instrument_id, fact.listing_id,
                     fact.reference_snapshot_version, fact.instrument_version,
                     fact.listing_version, fact.reference_selection,
                     fact.source_lineage, fact.source_decode_evidence);
  append_extensions(output, fact.source_message_extensions);
  const auto &assertions = fact.source_assertions;
  output.string(assertions.venue);
  output.enumeration(assertions.environment);
  output.enumeration(assertions.product_class);
  output.string(assertions.topic);
  output.string(assertions.source_symbol);
  output.string(assertions.source_trade_id);
  output.string(assertions.source_side);
  output.string(assertions.price_decimal);
  output.string(assertions.quantity_decimal);
  output.string(assertions.tick_direction);
  output.boolean(assertions.block_trade);
  output.optional_bool(assertions.rpi_trade);
  output.optional_u64(assertions.sequence);
  output.enumeration(assertions.sequence_scope);
  output.u64(assertions.system_timestamp_milliseconds);
  output.u64(assertions.trade_timestamp_milliseconds);
  output.enumeration(assertions.timestamp_unit);
  output.u64(assertions.member_index);
  append_extensions(output, assertions.extensions);
  output.time(fact.source_event_time);
  output.enumeration(fact.aggressor_side);
  output.fixed(fact.price);
  output.fixed(fact.quantity);
  output.string(fact.decoder_version);
  output.string(fact.source_schema_version);
  output.string(fact.normalizer_version);
  return output.take();
}

using Fact =
    std::variant<market::NormalizedBookFact, market::NormalizedTradeFact>;

template <typename Id> Id digest_id(const contracts::Sha256Digest &checksum) {
  typename Id::bytes_type value{};
  std::copy_n(checksum.bytes.begin(), value.size(), value.begin());
  return Id::from_bytes(value).value();
}

contracts::StreamCursor origin(std::uint8_t stream_seed) {
  return contracts::StreamCursor::at_origin(
             id<contracts::StreamId>(stream_seed), 1)
      .value();
}

contracts::StreamCursor cursor(std::uint8_t stream_seed,
                               std::uint64_t sequence) {
  return contracts::StreamCursor::at_sequence(
             id<contracts::StreamId>(stream_seed), 1, sequence)
      .value();
}

// In-memory append journal. Every successful persistence callback retains the
// exact committed fact; no filesystem/durability or cross-process recovery
// claim.
class InMemoryDispatchPersistence final
    : public dispatch::RunInputSelectionPersistence {
public:
  std::vector<dispatch::ControlBoundaryReservation> reservations, visible;
  std::vector<
      std::pair<dispatch::RunInputSelectionRecord, dispatch::RunInputCandidate>>
      selections;
  std::vector<dispatch::PublicationTransition> transitions;
  dispatch::RunInputRecoveryLoad
  load_recovery_state(const dispatch::RunInputDispatcherConfig &) override {
    return {.success = selections.empty() && reservations.empty()};
  }
  bool commit_control_reservation(
      const dispatch::ControlBoundaryReservation &r) override {
    reservations.push_back(r);
    return true;
  }
  bool commit_control_visibility(
      const dispatch::ControlBoundaryReservation &r) override {
    if (std::find(reservations.begin(), reservations.end(), r) ==
        reservations.end())
      return false;
    visible.push_back(r);
    return true;
  }
  bool commit_selection(const dispatch::RunInputSelectionRecord &r,
                        const dispatch::RunInputCandidate &c) override {
    if (r.run_input_sequence != selections.size() + 1)
      return false;
    selections.emplace_back(r, c);
    return true;
  }
  bool commit_publication_transition(
      const dispatch::PublicationTransition &t) override {
    if (selections.empty() ||
        t.selection_id != selections.back().first.selection_id)
      return false;
    transitions.push_back(t);
    return true;
  }
};
class ReplayEligibilityRegistry final
    : public dispatch::RunInputEligibilityRegistry {
public:
  explicit ReplayEligibilityRegistry(contracts::VersionRef version)
      : version_(version) {}

  bool is_run_input_eligible(
      std::string_view event_type,
      contracts::VersionRef registry_snapshot_version) const override {
    return registry_snapshot_version == version_ &&
           (event_type == "market.book.observation.snapshot" ||
            event_type == "market.book.observation.delta" ||
            event_type == "market.control.source.observed" ||
            event_type == "market.trade.observation.unadmitted" ||
            event_type == "market.trade.continuity.synchronized" ||
            event_type == "market.trade.observation.executed");
  }

private:
  contracts::VersionRef version_;
};

class ReplayDispatchConsumer final : public dispatch::RunInputConsumer {
public:
  contracts::ConsumerBoundaryId boundary_id() const noexcept override {
    return id<contracts::ConsumerBoundaryId>(51);
  }

  dispatch::ConsumerDisposition
  accept(const dispatch::RunInputSelectionRecord &r,
         const dispatch::RunInputCandidate &c,
         contracts::PublicationAttemptId) override {
    received.emplace_back(r, c);
    return dispatch::ConsumerDisposition::Accepted;
  }
  std::vector<
      std::pair<dispatch::RunInputSelectionRecord, dispatch::RunInputCandidate>>
      received;
};

recommendation::RecommendationPolicy recommendation_policy() {
  return {.policy_version = version(110, 1),
          .schema_version = version(111, 1),
          .authority_version = version(112, 1),
          .minimum_actionable_strength = 300000,
          .maximum_indicative_exposure = 750000,
          .scale = *contracts::DecimalScale::from_exponent(6)};
}
strategy::StrategyRuntimeConfig strategy_config() {
  const auto definition = chronos::strategies::generated::
                              accepted_chronos_reference_strategies_definition()
                                  .value();
  const auto parameter = definition.descriptor().parameter_schema[0];
  return {.strategy_instance_id = id<contracts::StrategyInstanceId>(98),
          .listing_id = id<contracts::ListingId>(10),
          .canonical_instrument_id = id<contracts::CanonicalInstrumentId>(8),
          .definition = definition,
          .parameter =
              strategy_sdk::StrategyParameter{
                  .parameter_id = parameter.parameter_id,
                  .definition_version = parameter.definition_version,
                  .units = 250000,
                  .scale = parameter.scale},
          .recommendation_policy = recommendation_policy(),
          .run_control_stream_id = id<contracts::StreamId>(46),
          .run_control_stream_epoch = 1,
          .run_timer_stream_id = id<contracts::StreamId>(47),
          .run_timer_stream_epoch = 1,
          .maximum_operations = strategy_sdk::kMaximumEvaluationOperations};
}
features::FeatureRuntimeConfig feature_config() {
  return {.run_id = id<contracts::RunId>(30),
          .listing_id = id<contracts::ListingId>(10),
          .imbalance_definition_version = strategy_config()
                                              .definition.descriptor()
                                              .required_features[0]
                                              .definition_version,
          .microprice_definition_version = version(101, 1),
          .spread_definition_version = version(102, 1),
          .implementation_version = version(103, 1),
          .required_view_schema_version = version(64, 1),
          .required_view_capability_version = version(65, 1),
          .required_bundle_schema_version = version(69, 1),
          .required_input_arithmetic_version = version(67, 1),
          .required_input_canonicalization_version = version(68, 1),
          .required_input_identity_policy_version = version(70, 1),
          .feature_arithmetic_version = version(104, 1),
          .canonicalization_version = version(105, 1),
          .identity_policy_version = version(106, 1)};
}
class MarketStatePipeline final {
public:
  MarketStatePipeline(const market::NormalizedBookFact &initial,
                      contracts::RunId run_id, MarketReplayResult &result,
                      const MarketReplayCallback &callback)
      : result_(result), callback_(callback), registry_(version(61, 1)),
        dispatcher_config_(dispatcher_config(run_id)),
        dispatcher_(*dispatch::RunInputDispatcher::create(
            dispatcher_config_, persistence_, registry_)),
        book_(*state::L2Book::create({
            .listing_id = initial.listing_id,
            .price_definition = price_definition(initial),
            .quantity_definition = quantity_definition(initial),
            .maximum_levels_per_side = 1000,
            .maximum_changes_per_delta = 1000,
        })),
        auxiliary_(*state::ListingAuxState::create({
            .listing_id = initial.listing_id,
            .price_definition = price_definition(initial),
            .quantity_definition = quantity_definition(initial),
            .trade_stream_id = id<contracts::StreamId>(41),
            .trade_stream_epoch = 1,
            .trade_continuity_stream_id = id<contracts::StreamId>(42),
            .trade_continuity_stream_epoch = 1,
            .book_stream_id = id<contracts::StreamId>(43),
            .book_stream_epoch = 1,
            .initial_logical_time_nanoseconds = 0,
            .book_freshness_deadline_nanoseconds = 1000000000,
            .trade_freshness_deadline_nanoseconds = 1000000000,
            .freshness_policy_version = version(62, 1),
            .trade_window_policy_version = version(63, 1),
            .accepted_source_clock_domain =
                initial.source_event_time.clock_domain_id(),
            .accepted_source_clock_class =
                initial.source_event_time.clock_class(),
            .required_source_time_quality = state::SourceTimeQuality::Exact,
            .correction_policy = state::TradeCorrectionPolicy::Reject,
            .trade_window_policy = state::TradeWindowPolicy::AcceptedCount,
            .recent_trade_capacity = 8,
        })),
        publisher_(*state::ListingViewPublisher::create(
            publisher_config(run_id, initial, dispatcher_config_))),
        canonical_instrument_id_(initial.canonical_instrument_id),
        reference_snapshot_version_(initial.reference_snapshot_version),
        listing_definition_version_(initial.listing_version),
        reference_configuration_lineage_version_(
            initial.reference_selection
                .reference_configuration_lineage_version) {}

  bool accept_fact(const Fact &fact, std::int64_t logical_time,
                   std::uint64_t capture_sequence) {
    logical_time_ = logical_time;
    capture_sequence_ = capture_sequence;
    auto payload = std::visit(
        [&](const auto &f) {
          if constexpr (std::is_same_v<std::decay_t<decltype(f)>,
                                       market::NormalizedBookFact>)
            return encode_book(dispatcher_.current_run_input_sequence() + 1, f);
          else
            return encode_trade(dispatcher_.current_run_input_sequence() + 1,
                                f);
        },
        fact);
    const auto type =
        std::holds_alternative<market::NormalizedTradeFact>(fact)
            ? std::string("market.trade.observation.unadmitted")
            : std::string(
                  std::get<market::NormalizedBookFact>(fact).event_type());
    auto candidate = make_candidate(type, std::move(payload));
    prepare_activation();
    return dispatch_fact(candidate, fact);
  }
  bool accept_other(std::vector<std::byte> payload, std::int64_t time,
                    std::uint64_t capture) {
    logical_time_ = time;
    capture_sequence_ = capture;
    prepare_activation();
    auto candidate =
        make_candidate("market.control.source.observed", std::move(payload));
    auto dispatched = dispatcher_.dispatch(candidate, consumer_);
    if (!dispatched.ok())
      throw std::runtime_error(
          "source dispatch failure: " +
          std::to_string(static_cast<int>(dispatched.failure)));
    accept_control(dispatched);
    if (!auxiliary_
             .apply_quality_input(
                 {.event_id = candidate.event_id,
                  .input_semantic_checksum = candidate.semantic_checksum,
                  .listing_id = book_.listing_id(),
                  .kind = state::ListingQualityInputKind::SourceObservation,
                  .run_input_sequence =
                      dispatched.selection->run_input_sequence,
                  .logical_time_nanoseconds = time})
             .ok())
      return false;
    const auto seq = next_source_sequence_++;
    return publish(
        candidate, *dispatched.selection,
        contracts::EventPosition::from(id<contracts::StreamId>(45), 1, seq)
            .value(),
        45, seq);
  }
  bool finalize() { return acceptance_.finalize(emitted_signals_); }

private:
  void prepare_activation() {
    if (!activated_) {
      const auto config = strategy_config();
      const auto payload_control =
          strategy::encode_strategy_activation_control(config);
      dispatch::ControlBoundaryReservation reservation{
          .run_id = dispatcher_config_.run_id,
          .control_stream_id = id<contracts::StreamId>(46),
          .control_stream_epoch = 1,
          .control_outcome_id = id<contracts::EventId>(99),
          .control_sequence = 1,
          .effective_position = dispatcher_.current_run_input_sequence() + 1,
          .prior_configuration_epoch = 1,
          .new_configuration_epoch = 2,
          .behavior_payload = payload_control,
          .behavior_checksum = contracts::sha256(payload_control)};
      if (!dispatcher_.reserve_control_boundary(reservation) ||
          !dispatcher_.make_control_visible(reservation))
        throw std::runtime_error("activation reservation rejected");
    }
  }
  dispatch::RunInputCandidate make_candidate(std::string type,
                                             std::vector<std::byte> payload) {
    const auto checksum = contracts::sha256(payload);
    return {.event_id = digest_id<contracts::EventId>(checksum),
            .event_type = std::move(type),
            .event_position = contracts::EventPosition::from(
                                  dispatcher_config_.input_stream_id, 1,
                                  dispatcher_.current_run_input_sequence() + 1)
                                  .value(),
            .semantic_payload = std::move(payload),
            .semantic_checksum = checksum};
  }

  static const market::BookSnapshotObservation &
  initial_snapshot(const market::NormalizedBookFact &initial) {
    return std::get<market::BookSnapshotObservation>(initial.payload);
  }

  static contracts::VersionRef
  price_definition(const market::NormalizedBookFact &initial) {
    return initial_snapshot(initial).bids.front().price.definition_ref();
  }

  static contracts::VersionRef
  quantity_definition(const market::NormalizedBookFact &initial) {
    return initial_snapshot(initial).bids.front().quantity.definition_ref();
  }

  static dispatch::RunInputDispatcherConfig
  dispatcher_config(contracts::RunId run_id) {
    return {
        .run_id = run_id,
        .input_stream_id = id<contracts::StreamId>(31),
        .input_stream_epoch = 1,
        .control_stream_id = id<contracts::StreamId>(46),
        .control_stream_epoch = 1,
        .consumer_boundary_id = id<contracts::ConsumerBoundaryId>(51),
        .merge_policy_version = version(60, 1),
        .registry_snapshot_version = version(61, 1),
        .initial_configuration_epoch = 1,
        .maximum_payload_bytes = 1U << 20U,
    };
  }

  static std::vector<contracts::StreamId> required_streams() {
    return {id<contracts::StreamId>(41), id<contracts::StreamId>(42),
            id<contracts::StreamId>(43), id<contracts::StreamId>(44),
            id<contracts::StreamId>(45), id<contracts::StreamId>(46),
            id<contracts::StreamId>(47)};
  }

  static contracts::StateLineage initial_lineage(contracts::RunId run_id) {
    const auto required = required_streams();
    const std::array cursors = {origin(41), origin(42), origin(43), origin(44),
                                origin(45), origin(46), origin(47)};
    return contracts::StateLineage::from(run_id, 0, required, cursors).value();
  }

  static state::ListingViewPublisherConfig publisher_config(
      contracts::RunId run_id, const market::NormalizedBookFact &initial,
      const dispatch::RunInputDispatcherConfig &dispatcher_config) {
    return {
        .run_id = run_id,
        .listing_id = initial.listing_id,
        .canonical_instrument_id = initial.canonical_instrument_id,
        .reference_snapshot_version = initial.reference_snapshot_version,
        .listing_definition_version = initial.listing_version,
        .reference_configuration_lineage_version =
            initial.reference_selection.reference_configuration_lineage_version,
        .required_streams = required_streams(),
        .initial_lineage = initial_lineage(run_id),
        .book_stream_id = id<contracts::StreamId>(43),
        .trade_stream_id = id<contracts::StreamId>(41),
        .trade_continuity_stream_id = id<contracts::StreamId>(42),
        .reference_stream_id = id<contracts::StreamId>(44),
        .market_control_stream_id = id<contracts::StreamId>(45),
        .run_control_stream_id = id<contracts::StreamId>(46),
        .run_timer_stream_id = id<contracts::StreamId>(47),
        .feature_boundary_id = id<contracts::ConsumerBoundaryId>(50),
        .dispatcher_config = dispatcher_config,
        .merge_policy_version = version(60, 1),
        .initial_configuration_epoch = 1,
        .view_schema_version = version(64, 1),
        .capability_version = version(65, 1),
        .transition_policy_version = version(66, 1),
        .arithmetic_version = version(67, 1),
        .canonicalization_version = version(68, 1),
        .bundle_schema_version = version(69, 1),
        .identity_policy_version = version(70, 1),
        .maximum_publication_transitions = 12288,
        .maximum_retained_views = 4096,
    };
  }

  void accept_control(const dispatch::DispatchResult &dispatched) {
    result_.run_inputs = dispatched.selection->run_input_sequence;
    if (!dispatched.accepted_control_outcomes.empty()) {
      current_control_ = dispatched.accepted_control_outcomes.front();
      runtime_ = strategy::StrategyRuntime::activate(strategy_config(),
                                                     *current_control_);
      if (!runtime_)
        throw std::runtime_error("strategy activation failed");
      activated_ = true;
      lineage_cursors_[5] = current_control_->accepted_control_cursor();
      effective_control_position_ =
          current_control_->reservation().effective_position;
    }
  }
  bool dispatch_fact(const dispatch::RunInputCandidate &candidate,
                     const Fact &fact) {
    const auto dispatched = dispatcher_.dispatch(candidate, consumer_);
    if (!dispatched.ok())
      return false;
    const auto run_sequence = dispatched.selection->run_input_sequence;
    accept_control(dispatched);
    if (const auto *book = std::get_if<market::NormalizedBookFact>(&fact)) {
      const auto role_sequence = next_book_sequence_++;
      if (!apply_book(*book, candidate, run_sequence, role_sequence))
        return false;
      const auto role_position =
          contracts::EventPosition::from(id<contracts::StreamId>(43), 1,
                                         role_sequence)
              .value();
      return publish(candidate, *dispatched.selection, role_position, 43,
                     role_sequence);
    }
    const auto seq = next_trade_sequence_++;
    if (!auxiliary_
             .apply_quality_input(
                 {.event_id = candidate.event_id,
                  .input_semantic_checksum = candidate.semantic_checksum,
                  .listing_id = book_.listing_id(),
                  .kind = state::ListingQualityInputKind::
                      TradeObservationUnadmitted,
                  .run_input_sequence = run_sequence,
                  .logical_time_nanoseconds = logical_time_,
                  .event_cursor = cursor(41, seq)})
             .ok())
      return false;
    return publish(
        candidate, *dispatched.selection,
        contracts::EventPosition::from(id<contracts::StreamId>(41), 1, seq)
            .value(),
        41, seq);
  }

  bool apply_book(const market::NormalizedBookFact &fact,
                  const dispatch::RunInputCandidate &candidate,
                  std::uint64_t run_sequence, std::uint64_t role_sequence) {
    const bool snapshot =
        std::holds_alternative<market::BookSnapshotObservation>(fact.payload);
    if (!snapshot && (!last_update_id_ ||
                      fact.source_assertions.update_id <= *last_update_id_ ||
                      fact.source_assertions.sequence <= *last_cross_sequence_))
      throw std::runtime_error(
          "book delta lacks a preceding snapshot or ordered update evidence");
    last_update_id_ = fact.source_assertions.update_id;
    last_cross_sequence_ = fact.source_assertions.sequence;
    const state::L2InputEvidence evidence{
        .event_id = candidate.event_id,
        .semantic_checksum = candidate.semantic_checksum,
        .kind = std::holds_alternative<market::BookSnapshotObservation>(
                    fact.payload)
                    ? state::L2InputKind::Snapshot
                    : state::L2InputKind::Delta,
    };
    if (const auto *snapshot_payload =
            std::get_if<market::BookSnapshotObservation>(&fact.payload)) {
      std::vector<state::L2Level> bids;
      std::vector<state::L2Level> asks;
      for (const auto &level : snapshot_payload->bids)
        bids.push_back({level.price, level.quantity});
      for (const auto &level : snapshot_payload->asks)
        asks.push_back({level.price, level.quantity});
      if (!book_
               .apply_snapshot({
                   .listing_id = fact.listing_id,
                   .input_evidence = evidence,
                   .bids = std::move(bids),
                   .asks = std::move(asks),
                   .bid_completeness =
                       state::L2SideCompleteness::BoundedWithProvenTop,
                   .ask_completeness =
                       state::L2SideCompleteness::BoundedWithProvenTop,
               })
               .ok()) {
        return false;
      }
      return auxiliary_
          .apply_quality_input(
              {
                  .event_id = candidate.event_id,
                  .input_semantic_checksum = candidate.semantic_checksum,
                  .listing_id = fact.listing_id,
                  .kind = state::ListingQualityInputKind::BookSynchronized,
                  .run_input_sequence = run_sequence,
                  .logical_time_nanoseconds = logical_time(run_sequence),
                  .book_proof =
                      state::BookSynchronizationProof{
                          .snapshot_event_id = candidate.event_id,
                          .snapshot_cursor = cursor(43, role_sequence),
                          .applied_through_cursor = cursor(43, role_sequence),
                          .l2_transition_sequence = book_.transition_sequence(),
                          .bridge_complete = true,
                          .reference_compatible = true,
                      },
              },
              &book_)
          .ok();
    }
    const auto &delta = std::get<market::BookDeltaObservation>(fact.payload);
    const auto changes = [](const auto &source) {
      std::vector<state::L2Change> result;
      for (const auto &change : source) {
        result.push_back({
            .price = change.price,
            .quantity = change.quantity,
            .operation = change.operation == market::BookLevelOperation::Delete
                             ? state::L2Operation::Delete
                             : state::L2Operation::SetAbsolute,
        });
      }
      return result;
    };
    if (!book_
             .apply_delta({
                 .listing_id = fact.listing_id,
                 .input_evidence = evidence,
                 .bid_changes = changes(delta.bid_changes),
                 .ask_changes = changes(delta.ask_changes),
             })
             .ok()) {
      return false;
    }
    auto proof = *auxiliary_.quality().last_book_proof;
    proof.applied_through_cursor = cursor(43, role_sequence);
    proof.l2_transition_sequence = book_.transition_sequence();
    return auxiliary_
        .apply_quality_input(
            {
                .event_id = candidate.event_id,
                .input_semantic_checksum = candidate.semantic_checksum,
                .listing_id = fact.listing_id,
                .kind = state::ListingQualityInputKind::BookEvidenceObserved,
                .run_input_sequence = run_sequence,
                .logical_time_nanoseconds = logical_time(run_sequence),
                .book_proof = proof,
            },
            &book_)
        .ok();
  }

  bool publish(const dispatch::RunInputCandidate &candidate,
               const dispatch::RunInputSelectionRecord &selection,
               contracts::EventPosition role_position,
               std::uint8_t role_stream_seed, std::uint64_t role_sequence) {
    for (auto &entry : lineage_cursors_) {
      if (entry.stream_id() == id<contracts::StreamId>(role_stream_seed))
        entry = cursor(role_stream_seed, role_sequence);
    }
    const auto lineage =
        contracts::StateLineage::from(dispatcher_config_.run_id,
                                      selection.run_input_sequence,
                                      required_streams(), lineage_cursors_)
            .value();
    const auto accepted = publisher_.accept_cut(
        {
            .selection_id = selection.selection_id,
            .dispatch_selection = selection,
            .dispatch_candidate = candidate,
            .selected_event_id = candidate.event_id,
            .selected_event_type = candidate.event_type,
            .selected_event_position = role_position,
            .input_semantic_checksum = candidate.semantic_checksum,
            .selection_semantic_checksum =
                selection.selection_semantic_checksum,
            .merge_policy_version = selection.merge_policy_version,
            .configuration_epoch = selection.active_configuration_epoch,
            .effective_control_position = effective_control_position_,
            .canonical_instrument_id = canonical_instrument_id_,
            .reference_snapshot_version = reference_snapshot_version_,
            .listing_definition_version = listing_definition_version_,
            .reference_configuration_lineage_version =
                reference_configuration_lineage_version_,
            .lineage = lineage,
            .accepted_control_outcome = current_control_,
        },
        book_, auxiliary_);
    if (!accepted.ok() || !accepted.view || !accepted.bundle)
      throw std::runtime_error(
          "view admission failed (4096 cut bound): " +
          std::to_string(static_cast<int>(accepted.failure)));
    const auto attempt = digest_id<contracts::PublicationAttemptId>(
        accepted.bundle->semantic_checksum);
    const auto transition = [&](state::ViewPublicationState from,
                                state::ViewPublicationState to) {
      return publisher_.transition_publication({
                 .view_id = accepted.view->view_id,
                 .bundle_id = accepted.bundle->bundle_id,
                 .attempt_id = attempt,
                 .boundary_id = id<contracts::ConsumerBoundaryId>(50),
                 .attempt_number = 1,
                 .from = from,
                 .to = to,
             }) == state::ListingViewFailure::None;
    };
    if (!transition(state::ViewPublicationState::NotPublished,
                    state::ViewPublicationState::PublicationInProgress) ||
        !transition(state::ViewPublicationState::PublicationInProgress,
                    state::ViewPublicationState::PublishedToFeatureBoundary) ||
        !transition(state::ViewPublicationState::PublishedToFeatureBoundary,
                    state::ViewPublicationState::FeatureConsumerAccepted)) {
      return false;
    }
    ++result_.published_views;
    result_.final_view = *accepted.view;
    result_.final_bundle = *accepted.bundle;
    const auto feature_cut = publisher_.accepted_feature_cut();
    if (!feature_cut)
      throw std::runtime_error(
          "immutable feature publication was not accepted");
    const auto computed =
        features::FeatureRuntime(feature_config()).evaluate(*feature_cut);
    if (!computed.accepted_cut())
      throw std::runtime_error(
          "feature evaluation failed: " +
          std::to_string(static_cast<int>(computed.failure)));
    const auto invocation = runtime_->admit(*computed.accepted_cut());
    if (!invocation)
      throw std::runtime_error("strategy admission failed");
    const auto evaluated = strategy::StrategyEvaluationAuthority::evaluate(
        strategy_config().definition, *invocation);
    if (!evaluated.completed())
      throw std::runtime_error("strategy evaluation failed");
    ++result_.evaluations;
    if (evaluated.evaluation->abstained()) {
      ++result_.abstentions;
      return true;
    }
    const auto recommended = recommendation::RecommendationAuthority::recommend(
        *evaluated.evaluation, recommendation_policy());
    if (!recommended.completed())
      throw std::runtime_error("recommendation creation failed");
    const auto accepted_recommendation =
        acceptance_.accept(*recommended.recommendation);
    if (!accepted_recommendation.accepted())
      throw std::runtime_error(
          "recommendation acceptance failed or 4096 bound exceeded");
    emitted_signals_.push_back(
        accepted_recommendation.recommendation->signal_id());
    ++result_.recommendations;
    if (accepted_recommendation.recommendation->hold())
      ++result_.holds;
    for (auto byte :
         accepted_recommendation.recommendation->recommendation_id().bytes())
      semantic_output_.push_back(static_cast<std::byte>(byte));
    for (auto byte : accepted.view->semantic_checksum.bytes)
      semantic_output_.push_back(static_cast<std::byte>(byte));
    result_.semantic_checksum = contracts::sha256(semantic_output_);
    if (callback_ &&
        !callback_({*result_.profile, *accepted_recommendation.recommendation,
                    *accepted.view, *accepted.bundle, capture_sequence_,
                    selection.run_input_sequence, logical_time_}))
      throw std::runtime_error("downstream callback rejected market cut");
    return true;
  }

  std::int64_t logical_time(std::uint64_t) const { return logical_time_; }

  MarketReplayResult &result_;
  const MarketReplayCallback &callback_;
  std::int64_t logical_time_{};
  std::uint64_t capture_sequence_{};
  std::optional<std::uint64_t> last_update_id_, last_cross_sequence_,
      effective_control_position_;
  bool activated_{};
  std::optional<dispatch::AcceptedControlOutcome> current_control_;
  std::optional<strategy::StrategyRuntime> runtime_;
  recommendation::RecommendationAcceptanceAuthority acceptance_ =
      *recommendation::RecommendationAcceptanceAuthority::create(4096);
  std::vector<contracts::StrategySignalId> emitted_signals_;
  std::vector<std::byte> semantic_output_;
  InMemoryDispatchPersistence persistence_;
  ReplayEligibilityRegistry registry_;
  ReplayDispatchConsumer consumer_;
  dispatch::RunInputDispatcherConfig dispatcher_config_;
  dispatch::RunInputDispatcher dispatcher_;
  state::L2Book book_;
  state::ListingAuxState auxiliary_;
  state::ListingViewPublisher publisher_;
  std::array<contracts::StreamCursor, 7> lineage_cursors_{
      origin(41), origin(42), origin(43), origin(44),
      origin(45), origin(46), origin(47)};
  contracts::CanonicalInstrumentId canonical_instrument_id_;
  contracts::VersionRef reference_snapshot_version_;
  contracts::VersionRef listing_definition_version_;
  contracts::VersionRef reference_configuration_lineage_version_;
  std::uint64_t next_book_sequence_{};
  std::uint64_t next_trade_sequence_{}, next_source_sequence_{};
};

template <typename T>
void append_normalized(std::vector<replay::NormalizedFactRecord> &records,
                       const T &fact, contracts::Sha256Digest source) {
  const auto position = static_cast<std::uint64_t>(records.size() + 1);
  auto payload = [&] {
    if constexpr (std::is_same_v<T, market::NormalizedBookFact>)
      return encode_book(position, fact);
    else
      return encode_trade(position, fact);
  }();
  const auto checksum = contracts::sha256(payload);
  records.push_back(
      {.normalized_position = position,
       .normalized_stream_id = id<contracts::StreamId>(31),
       .normalized_stream_epoch = 1,
       .source_dataset_identity = source,
       .source_event_id = fact.source_lineage.source_event_id,
       .source_decode_enrichment_id =
           fact.source_decode_evidence.source_decode_enrichment_id,
       .acceptance_evidence_id = digest_id<contracts::IntegrityId>(checksum),
       .normalizer_version = fact.normalizer_version,
       .reference_lineage_version =
           fact.reference_selection.reference_configuration_lineage_version,
       .event_type = std::string(fact.event_type()),
       .semantic_payload = std::move(payload),
       .semantic_checksum = checksum});
}
contracts::Sha256Digest
expected_normalized_identity(const adapter::DatasetReadResult &dataset) {
  const auto lineage = reference_lineage(dataset);
  std::vector<replay::NormalizedFactRecord> records;
  const auto source = digest(dataset.manifest()->dataset_id);
  for (std::size_t i = 0; i < dataset.records().size(); ++i) {
    const auto book = adapter::decode_bybit_v5_book(dataset, i);
    if (book.ok()) {
      const auto normalized = market::normalize_book(
          *book.enrichment, lineage, id<contracts::ClockDomainId>(12),
          {.normalizer_version = "chronos-book-normalizer-v1"});
      if (!normalized.ok())
        throw std::runtime_error("book normalization preflight failed");
      append_normalized(records, *normalized.fact, source);
      continue;
    }
    const auto trades = adapter::decode_bybit_v5_trades(dataset, i);
    if (trades.ok()) {
      const auto normalized = market::normalize_trades(
          *trades.enrichment, lineage, id<contracts::ClockDomainId>(12),
          {.normalizer_version = "chronos-trade-normalizer-v1"});
      if (!normalized.ok())
        throw std::runtime_error("trade normalization preflight failed");
      for (const auto &fact : normalized.facts)
        append_normalized(records, fact, source);
    }
  }
  const auto normalized =
      replay::NormalizedFactDataset::create(std::move(records));
  if (!normalized)
    throw std::runtime_error("normalized dataset validation failed");
  return normalized->identity();
}
class CaptureReplaySink final : public replay::ReplayDispatchSink {
public:
  CaptureReplaySink(const adapter::DatasetReadResult &dataset,
                    MarketReplayResult &result,
                    const MarketReplayCallback &callback)
      : dataset_(dataset), result_(result), callback_(callback),
        lineage_(reference_lineage(dataset)) {
    // Decode only to acquire the fixed reference definitions. No book levels or
    // future observations are applied before their capture-order selection.
    for (std::size_t i = 0; i < dataset.records().size(); ++i) {
      const auto decoded = adapter::decode_bybit_v5_book(dataset, i);
      if (!decoded.ok())
        continue;
      const auto normalized = market::normalize_book(
          *decoded.enrichment, lineage_, id<contracts::ClockDomainId>(12),
          {.normalizer_version = "chronos-book-normalizer-v1"});
      if (!normalized.ok())
        throw std::runtime_error("initial reference normalization failed");
      if (!std::holds_alternative<market::BookSnapshotObservation>(
              normalized.fact->payload))
        throw std::runtime_error("initial book event must be snapshot");
      const auto &snapshot =
          std::get<market::BookSnapshotObservation>(normalized.fact->payload);
      if (snapshot.bids.empty() || snapshot.asks.empty())
        throw std::runtime_error("initial snapshot lacks both sides");
      result_.profile = MarketReplayProfile{
          .profile_version = version(150, 1),
          .run_id = id<contracts::RunId>(30),
          .portfolio_id = id<contracts::PortfolioId>(140),
          .account_id = id<contracts::AccountId>(141),
          .listing_id = normalized.fact->listing_id,
          .instrument_id = normalized.fact->canonical_instrument_id,
          .price_definition = snapshot.bids.front().price.definition_ref(),
          .quantity_definition =
              snapshot.bids.front().quantity.definition_ref(),
          .price_scale = *contracts::DecimalScale::from_exponent(1),
          .quantity_scale = *contracts::DecimalScale::from_exponent(3),
          .exposure_scale = *contracts::DecimalScale::from_exponent(6),
          .price_tick_units = 1,
          .quantity_step_units = 1};
      pipeline_ = std::make_unique<MarketStatePipeline>(
          *normalized.fact, id<contracts::RunId>(30), result_, callback_);
      break;
    }
    if (!pipeline_)
      throw std::runtime_error("no initial snapshot available");
  }
  bool accept(const replay::ReplayDispatchInput &input) override {
    if (input.replay_class != replay::ReplayClass::FaithfulCaptureOrder ||
        input.replay_ordinal == 0 ||
        input.replay_ordinal > dataset_.records().size())
      return false;
    const auto index = static_cast<std::size_t>(input.replay_ordinal - 1);
    const auto &record = dataset_.records()[index];
    if (record.chronos_receive_time.clock_class() !=
        contracts::ClockClass::monotonic)
      throw std::runtime_error("capture receive clock is not monotonic");
    if (index &&
        (record.chronos_receive_time.clock_domain_id() !=
             dataset_.records()
                 .front()
                 .chronos_receive_time.clock_domain_id() ||
         record.chronos_receive_time.nanoseconds() <
             dataset_.records()[index - 1].chronos_receive_time.nanoseconds()))
      throw std::runtime_error(
          "capture receive timestamps regress or change clock domain");
    std::int64_t logical{};
    if (__builtin_sub_overflow(
            record.chronos_receive_time.nanoseconds(),
            dataset_.records().front().chronos_receive_time.nanoseconds(),
            &logical))
      throw std::runtime_error("capture logical time overflow");
    ++result_.capture_records;
    const auto book = adapter::decode_bybit_v5_book(dataset_, index);
    if (book.ok()) {
      const auto normalized = market::normalize_book(
          *book.enrichment, lineage_, id<contracts::ClockDomainId>(12),
          {.normalizer_version = "chronos-book-normalizer-v1"});
      if (!normalized.ok())
        throw std::runtime_error(
            "book normalization failed at capture " +
            std::to_string(record.capture_sequence) + " failure " +
            std::to_string(static_cast<int>(normalized.failure)));
      ++result_.normalized_book_facts;
      append_normalized(normalized_records_, *normalized.fact,
                        digest(dataset_.manifest()->dataset_id));
      if (!pipeline_->accept_fact(*normalized.fact, logical,
                                  record.capture_sequence))
        throw std::runtime_error("book cut rejected at capture " +
                                 std::to_string(record.capture_sequence));
      return true;
    }
    const auto trades = adapter::decode_bybit_v5_trades(dataset_, index);
    if (trades.ok()) {
      const auto normalized = market::normalize_trades(
          *trades.enrichment, lineage_, id<contracts::ClockDomainId>(12),
          {.normalizer_version = "chronos-trade-normalizer-v1"});
      if (!normalized.ok())
        throw std::runtime_error("trade normalization failed at capture " +
                                 std::to_string(record.capture_sequence));
      for (const auto &fact : normalized.facts) {
        append_normalized(normalized_records_, fact,
                          digest(dataset_.manifest()->dataset_id));
        ++result_.normalized_trade_facts;
        ++result_.trade_observations_without_continuity;
        if (!pipeline_->accept_fact(fact, logical, record.capture_sequence))
          return false;
      }
      return true;
    }
    const std::string raw(
        reinterpret_cast<const char *>(record.raw_payload.data()),
        record.raw_payload.size());
    const auto topic_class = classify_source_topic(raw);
    if (topic_class == SourceTopicClass::Relevant ||
        topic_class == SourceTopicClass::InvalidJson ||
        book.failure == market::BookNormalizationFailure::MalformedPayload)
      throw std::runtime_error(
          "malformed or unsupported relevant source data at capture " +
          std::to_string(record.capture_sequence));
    const bool control = record.frame_kind != sdk::SourceFrameKind::Text ||
                         raw.find("\"op\"") != std::string::npos;
    if (control)
      ++result_.control_frames;
    else
      ++result_.unsupported_frames;
    SemanticWriter payload;
    payload.string("chronos-m6-source-observation-v1");
    payload.u64(record.capture_sequence);
    payload.identifier(record.source_event_id);
    payload.string(raw);
    return pipeline_->accept_other(payload.take(), logical,
                                   record.capture_sequence);
  }
  std::optional<contracts::Sha256Digest>
  completed_normalized_dataset_identity() const override {
    const auto data =
        replay::NormalizedFactDataset::create(normalized_records_);
    return data ? std::optional(data->identity()) : std::nullopt;
  }
  bool finalize() {
    if (!pipeline_)
      throw std::runtime_error(
          "capture contains no accepted initial book snapshot");
    result_.recommendation_cardinality_proven = pipeline_->finalize();
    return result_.recommendation_cardinality_proven;
  }

private:
  std::vector<replay::NormalizedFactRecord> normalized_records_;
  const adapter::DatasetReadResult &dataset_;
  MarketReplayResult &result_;
  const MarketReplayCallback &callback_;
  reference::ReferenceConfigurationLineage lineage_;
  std::unique_ptr<MarketStatePipeline> pipeline_;
};
} // namespace

MarketReplayResult run_market_replay(const std::filesystem::path &path,
                                     const MarketReplayCallback &callback) {
  MarketReplayResult result;
  try {
    const auto dataset = adapter::read_capture_dataset(path);
    if (!dataset.ok())
      throw std::runtime_error(
          "capture dataset verification failed: " +
          std::to_string(static_cast<int>(dataset.failure())));
    if (dataset.records().empty() || dataset.records().size() > 100000 ||
        dataset.manifest()->last_capture_sequence ==
            std::numeric_limits<std::uint64_t>::max())
      throw std::runtime_error(
          "capture outside fixed profile record bounds (1..100000)");
    if (dataset.manifest()->venue != "bybit" ||
        dataset.manifest()->market != sdk::MarketClass::LinearPerpetual)
      throw std::runtime_error(
          "fixed profile requires Bybit BTCUSDT linear perpetual source data");
    const auto lineage = reference_lineage(dataset);
    result.manifest = replay::ReplayRunManifest::create(
        id<contracts::RunId>(30), replay::ReplayClass::FaithfulCaptureOrder,
        digest(dataset.manifest()->dataset_id),
        {.provider_version = "capture-order-provider-v1",
         .merge_policy_version = "single-stream-capture-order-v1",
         .schema_registry_version = "bybit-v5-public-registry-v1",
         .canonicalization_version = "m6-market-replay-canonical-v1",
         .normalizer_version = "chronos-market-normalizers-v1",
         .reference_lineage_version = lineage.version(),
         .expected_normalized_dataset_identity =
             expected_normalized_identity(dataset)});
    if (!result.manifest)
      throw std::runtime_error("invalid replay manifest");
    CaptureReplaySink sink(dataset, result, callback);
    const auto replayed =
        replay::replay_capture_order(*result.manifest, dataset, sink);
    if (!replayed.ok())
      throw std::runtime_error(
          "capture-order replay rejected: " +
          std::to_string(static_cast<int>(replayed.failure)));
    if (!sink.finalize())
      throw std::runtime_error("recommendation cardinality was not proven");
    SemanticWriter summary;
    summary.string("chronos-m6-market-replay-summary-v1");
    summary.digest(result.manifest->identity());
    summary.digest(result.semantic_checksum);
    summary.digest(result.final_view->semantic_checksum);
    summary.u64(result.capture_records);
    summary.u64(result.run_inputs);
    summary.u64(result.normalized_book_facts);
    summary.u64(result.normalized_trade_facts);
    summary.u64(result.control_frames);
    summary.u64(result.unsupported_frames);
    summary.u64(result.evaluations);
    summary.u64(result.abstentions);
    summary.u64(result.recommendations);
    summary.u64(result.holds);
    result.semantic_checksum = contracts::sha256(summary.take());
    result.completed = true;
  } catch (const std::exception &error) {
    result.error = error.what();
  }
  return result;
}
} // namespace chronos::applications::replay_runner
