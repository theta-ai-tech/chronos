#include "chronos/adapters/market_data/bybit_book_decoder.hpp"
#include "chronos/adapters/market_data/bybit_trade_decoder.hpp"
#include "chronos/core/dispatch/run_input_dispatcher.hpp"
#include "chronos/core/market_state/listing_view_publisher.hpp"
#include "chronos/normalization/market_data/book_normalizer.hpp"
#include "chronos/normalization/market_data/trade_normalizer.hpp"
#include "chronos/runtime/datasets/replay.hpp"

#include "microtest.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
namespace adapter = chronos::adapters::market_data;
namespace sdk = chronos::adapters::sdk;
namespace contracts = chronos::contracts;
namespace dispatch = chronos::core::dispatch;
namespace state = chronos::core::market_state;
namespace reference = chronos::core::reference_data;
namespace market = chronos::normalization::market_data;
namespace replay = chronos::runtime::datasets;

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

constexpr std::string_view kSnapshot = R"({
  "data":{"a":[["42000.50","0.200"],["42000.30","0.100"]],
          "seq":7961638724,"s":"BTCUSDT",
          "b":[["42000.00","1.000"],["42000.20","0.500"]],"u":18521288},
  "cts":1672304486868,"ts":1672304486869,"type":"snapshot",
  "topic":"orderbook.50.BTCUSDT"
})";

constexpr std::string_view kDelta = R"({
  "topic":"orderbook.50.BTCUSDT","type":"delta","ts":1672304486870,
  "data":{"s":"BTCUSDT","b":[["42000.00","0"],["42000.20","0.750"]],
          "a":[["42000.30","0.300"]],"u":18521289,"seq":7961638725},
  "cts":1672304486869
})";

constexpr std::string_view kTrades = R"({
  "topic":"publicTrade.BTCUSDT","type":"snapshot","ts":1672304486868,
  "data":[
    {"T":1672304486867,"s":"BTCUSDT","S":"Sell","v":"0.002",
     "p":"16578.60","L":"MinusTick","i":"trade-b","BT":false,
     "RPI":true,"seq":1783284618},
    {"T":1672304486865,"s":"BTCUSDT","S":"Buy","v":"0.001",
     "p":"16578.50","L":"PlusTick","i":"trade-a","BT":true}
  ]
})";

sdk::SourceCaptureContext capture_context() {
  return {
      .adapter_id = "chronos.bybit.public-market-data",
      .adapter_version = "m2.5",
      .build_version = "m3.5-golden",
      .venue = "bybit",
      .environment = sdk::EnvironmentClass::Test,
      .market = sdk::MarketClass::LinearPerpetual,
      .endpoint = sdk::EndpointClass::PublicMarketData,
      .trust_class = sdk::SourceTrustClass::PublicUnauthenticated,
      .capture_session_id = id<sdk::CaptureSessionId>(1),
      .runtime_id = id<contracts::RuntimeId>(2),
      .connection_id = id<sdk::SourceConnectionId>(3),
      .subscription_id = id<sdk::SourceSubscriptionId>(4),
      .capture_partition_id = id<sdk::CapturePartitionId>(5),
      .framing_version = "websocket-rfc6455-v1",
      .static_configuration_version = "m3.5-golden-v1",
      .capability_manifest_version = "bybit-v5-v1",
      .schema_policy_version = "bybit-v5-public-v1",
      .data_classification = sdk::DataClassification::PublicMarketData,
      .access_restriction = sdk::AccessRestriction::ChronosInternal,
      .maximum_retained_payload_bytes = 1U << 20U,
      .maximum_source_events = 3,
  };
}

adapter::DatasetReadResult captured_session() {
  static std::uint64_t fixture_sequence{};
  const auto path =
      std::filesystem::temp_directory_path() /
      ("chronos-m3.5-golden-" + std::to_string(++fixture_sequence));
  std::filesystem::remove_all(path);
  std::filesystem::remove_all(path.string() + ".partial");
  const auto context = capture_context();
  auto recorder = sdk::SourceCaptureRecorder::create(context).value();
  auto writer = adapter::CaptureDatasetWriter::create(path, context).value();
  const std::vector<std::string_view> payloads{kSnapshot, kDelta, kTrades};
  for (std::size_t index = 0; index < payloads.size(); ++index) {
    const auto payload = bytes(payloads[index]);
    const auto captured = recorder.capture({
        .source_event_id =
            id<contracts::SourceEventId>(static_cast<std::uint8_t>(20 + index)),
        .chronos_receive_time =
            contracts::TimePoint::from(static_cast<std::int64_t>(900 + index),
                                       id<contracts::ClockDomainId>(6),
                                       contracts::ClockClass::monotonic, 1)
                .value(),
        .raw_payload = payload,
        .framing_protocol = sdk::FramingProtocol::WebSocket,
        .frame_kind = sdk::SourceFrameKind::Text,
        .framing_status = sdk::FramingStatus::Complete,
        .integrity_status = sdk::CaptureIntegrityStatus::Complete,
        .content_encoding = sdk::ContentEncoding::Utf8Text,
        .compression_disposition = sdk::CompressionDisposition::NotCompressed,
    });
    if (!captured.ok() ||
        writer.append(*captured.event) != adapter::DatasetFailure::None) {
      throw std::runtime_error("failed to capture golden source event");
    }
  }
  if (!writer.seal().manifest.has_value())
    throw std::runtime_error("failed to seal golden capture dataset");
  auto result = adapter::read_capture_dataset(path);
  std::filesystem::remove_all(path);
  return result;
}

reference::ReferenceConfigurationLineage reference_lineage() {
  const auto interval = reference::EffectiveInterval::from_capture_sequence(
                            id<contracts::CapturePartitionId>(5), 1, 4)
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
           .environment = reference::VenueEnvironment::Test,
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

struct NormalizedRun final {
  std::vector<Fact> facts;
  std::vector<replay::NormalizedFactRecord> records;
};

NormalizedRun normalize_session(const adapter::DatasetReadResult &dataset) {
  if (!dataset.ok() || dataset.records().size() != 3)
    throw std::runtime_error("golden capture dataset is not verified");
  const auto lineage = reference_lineage();
  const auto clock = id<contracts::ClockDomainId>(12);
  const auto stream_id = id<contracts::StreamId>(31);
  const auto source_dataset_identity = digest(dataset.manifest()->dataset_id);
  NormalizedRun run;
  for (std::size_t index = 0; index < 2; ++index) {
    const auto decoded = adapter::decode_bybit_v5_book(dataset, index);
    if (!decoded.ok())
      throw std::runtime_error("golden book decode failed");
    auto result = market::normalize_book(
        *decoded.enrichment, lineage, clock,
        {.normalizer_version = "chronos-book-normalizer-v1"});
    if (!result.ok())
      throw std::runtime_error("golden book normalization failed");
    const auto position = static_cast<std::uint64_t>(run.facts.size() + 1);
    auto payload = encode_book(position, *result.fact);
    run.records.push_back(
        {.normalized_position = position,
         .normalized_stream_id = stream_id,
         .normalized_stream_epoch = 1,
         .source_dataset_identity = source_dataset_identity,
         .source_event_id = result.fact->source_lineage.source_event_id,
         .source_decode_enrichment_id =
             result.fact->source_decode_evidence.source_decode_enrichment_id,
         .acceptance_evidence_id = id<contracts::IntegrityId>(32),
         .normalizer_version = result.fact->normalizer_version,
         .reference_lineage_version =
             result.fact->reference_selection
                 .reference_configuration_lineage_version,
         .event_type = std::string(result.fact->event_type()),
         .semantic_payload = payload,
         .semantic_checksum = contracts::sha256(payload)});
    run.facts.emplace_back(std::move(*result.fact));
  }
  const auto decoded = adapter::decode_bybit_v5_trades(dataset, 2);
  if (!decoded.ok())
    throw std::runtime_error("golden trade decode failed");
  auto result = market::normalize_trades(
      *decoded.enrichment, lineage, clock,
      {.normalizer_version = "chronos-trade-normalizer-v1"});
  if (!result.ok())
    throw std::runtime_error("golden trade normalization failed");
  for (auto &fact : result.facts) {
    const auto position = static_cast<std::uint64_t>(run.facts.size() + 1);
    auto payload = encode_trade(position, fact);
    run.records.push_back(
        {.normalized_position = position,
         .normalized_stream_id = stream_id,
         .normalized_stream_epoch = 1,
         .source_dataset_identity = source_dataset_identity,
         .source_event_id = fact.source_lineage.source_event_id,
         .source_decode_enrichment_id =
             fact.source_decode_evidence.source_decode_enrichment_id,
         .acceptance_evidence_id = id<contracts::IntegrityId>(32),
         .normalizer_version = fact.normalizer_version,
         .reference_lineage_version =
             fact.reference_selection.reference_configuration_lineage_version,
         .event_type = std::string(fact.event_type()),
         .semantic_payload = payload,
         .semantic_checksum = contracts::sha256(payload)});
    run.facts.emplace_back(std::move(fact));
  }
  return run;
}

std::vector<std::byte>
concatenate(const std::vector<replay::NormalizedFactRecord> &records) {
  std::vector<std::byte> output;
  for (const auto &record : records)
    output.insert(output.end(), record.semantic_payload.begin(),
                  record.semantic_payload.end());
  return output;
}

std::string golden_digest(std::string_view fixture_name) {
  const auto path = std::filesystem::path(CHRONOS_SOURCE_DIR) / "tests" /
                    "replay" / "fixtures" / fixture_name;
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("normalized stream golden is missing");
  std::string contents{std::istreambuf_iterator<char>(input), {}};
  if (!contents.empty() && contents.back() == '\n')
    contents.pop_back();
  return contents;
}

class ByteSink final : public replay::ReplayDispatchSink {
public:
  explicit ByteSink(const std::vector<replay::NormalizedFactRecord> &records)
      : records_(records) {}

  bool accept(const replay::ReplayDispatchInput &input) override {
    if (input.replay_ordinal == 0 || input.replay_ordinal > records_.size())
      return false;
    const auto &expected = records_[input.replay_ordinal - 1];
    if (input.replay_class != replay::ReplayClass::NormalizedFact ||
        input.replay_ordinal != expected.normalized_position ||
        input.event_type != expected.event_type ||
        input.semantic_payload.size() != expected.semantic_payload.size() ||
        !std::equal(input.semantic_payload.begin(),
                    input.semantic_payload.end(),
                    expected.semantic_payload.begin()) ||
        input.semantic_checksum != expected.semantic_checksum ||
        input.source_event_id != expected.source_event_id ||
        input.capture_sequence.has_value() ||
        input.normalized_position != expected.normalized_position ||
        input.normalized_stream_id != expected.normalized_stream_id ||
        input.normalized_stream_epoch != expected.normalized_stream_epoch ||
        input.source_dataset_identity != expected.source_dataset_identity ||
        input.source_decode_enrichment_id !=
            expected.source_decode_enrichment_id ||
        input.acceptance_evidence_id != expected.acceptance_evidence_id ||
        input.normalizer_version != expected.normalizer_version ||
        input.reference_lineage_version != expected.reference_lineage_version)
      return false;
    output.insert(output.end(), input.semantic_payload.begin(),
                  input.semantic_payload.end());
    return true;
  }

  std::vector<std::byte> output;

private:
  const std::vector<replay::NormalizedFactRecord> &records_;
};

class FaithfulNormalizationSink final : public replay::ReplayDispatchSink {
public:
  FaithfulNormalizationSink(const adapter::DatasetReadResult &dataset,
                            replay::ReplayVersionPins pins)
      : dataset_(dataset), pins_(std::move(pins)) {}

  bool accept(const replay::ReplayDispatchInput &input) override {
    if (input.replay_class != replay::ReplayClass::FaithfulCaptureOrder ||
        input.replay_ordinal != accepted_ + 1 ||
        input.replay_ordinal >
            static_cast<std::uint64_t>(dataset_.records().size())) {
      return false;
    }
    const auto &source = dataset_.records()[input.replay_ordinal - 1];
    if (input.semantic_payload.size() != source.raw_payload.size() ||
        !std::equal(input.semantic_payload.begin(),
                    input.semantic_payload.end(), source.raw_payload.begin()) ||
        input.source_event_id != source.source_event_id ||
        input.capture_sequence != source.capture_sequence) {
      return false;
    }
    ++accepted_;
    return true;
  }

  std::optional<contracts::Sha256Digest>
  completed_normalized_dataset_identity() const override {
    const auto lineage = reference_lineage();
    if (accepted_ != dataset_.records().size() ||
        pins_.provider_version != "capture-order-provider-v1" ||
        pins_.merge_policy_version != "single-stream-capture-order-v1" ||
        pins_.schema_registry_version != "bybit-v5-public-registry-v1" ||
        pins_.canonicalization_version != "m3-normalized-golden-v1" ||
        pins_.normalizer_version != "chronos-market-normalizers-v1" ||
        pins_.reference_lineage_version != lineage.version() ||
        !pins_.expected_normalized_dataset_identity.has_value())
      return std::nullopt;
    const auto normalized = replay::NormalizedFactDataset::create(
        normalize_session(dataset_).records);
    if (!normalized.has_value())
      return std::nullopt;
    return normalized->identity();
  }

private:
  const adapter::DatasetReadResult &dataset_;
  replay::ReplayVersionPins pins_;
  std::size_t accepted_{};
};

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

class GoldenDispatchPersistence final
    : public dispatch::RunInputSelectionPersistence {
public:
  dispatch::RunInputRecoveryLoad
  load_recovery_state(const dispatch::RunInputDispatcherConfig &) override {
    return {.success = true};
  }

  bool commit_control_reservation(
      const dispatch::ControlBoundaryReservation &) override {
    return true;
  }

  bool commit_control_visibility(
      const dispatch::ControlBoundaryReservation &) override {
    return true;
  }

  bool commit_selection(const dispatch::RunInputSelectionRecord &,
                        const dispatch::RunInputCandidate &) override {
    return true;
  }

  bool commit_publication_transition(
      const dispatch::PublicationTransition &) override {
    return true;
  }
};

class GoldenEligibilityRegistry final
    : public dispatch::RunInputEligibilityRegistry {
public:
  explicit GoldenEligibilityRegistry(contracts::VersionRef version)
      : version_(version) {}

  bool is_run_input_eligible(
      std::string_view event_type,
      contracts::VersionRef registry_snapshot_version) const override {
    return registry_snapshot_version == version_ &&
           (event_type == "market.book.observation.snapshot" ||
            event_type == "market.book.observation.delta" ||
            event_type == "market.trade.continuity.synchronized" ||
            event_type == "market.trade.observation.executed");
  }

private:
  contracts::VersionRef version_;
};

class GoldenDispatchConsumer final : public dispatch::RunInputConsumer {
public:
  contracts::ConsumerBoundaryId boundary_id() const noexcept override {
    return id<contracts::ConsumerBoundaryId>(51);
  }

  dispatch::ConsumerDisposition
  accept(const dispatch::RunInputSelectionRecord &,
         const dispatch::RunInputCandidate &,
         contracts::PublicationAttemptId) override {
    return dispatch::ConsumerDisposition::Accepted;
  }
};

struct ViewReplayResult final {
  std::vector<state::ListingStateView> views;
  std::vector<state::StateViewBundle> bundles;

  bool operator==(const ViewReplayResult &) const = default;
};

class MarketStateReplaySink final : public replay::ReplayDispatchSink {
public:
  MarketStateReplaySink(const std::vector<Fact> &facts,
                        const market::NormalizedBookFact &initial,
                        contracts::RunId run_id)
      : facts_(facts), registry_(version(61, 1)),
        dispatcher_config_(dispatcher_config(run_id)),
        dispatcher_(*dispatch::RunInputDispatcher::create(
            dispatcher_config_, persistence_, registry_)),
        book_(*state::L2Book::create({
            .listing_id = initial.listing_id,
            .price_definition = price_definition(initial),
            .quantity_definition = quantity_definition(initial),
            .maximum_levels_per_side = 64,
            .maximum_changes_per_delta = 64,
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
            .initial_logical_time_nanoseconds = 1000,
            .book_freshness_deadline_nanoseconds = 100,
            .trade_freshness_deadline_nanoseconds = 100,
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

  bool accept(const replay::ReplayDispatchInput &input) override {
    if (input.replay_class != replay::ReplayClass::NormalizedFact ||
        input.replay_ordinal == 0 || input.replay_ordinal > facts_.size()) {
      return false;
    }
    const auto index = static_cast<std::size_t>(input.replay_ordinal - 1);
    if (std::holds_alternative<market::NormalizedTradeFact>(facts_[index]) &&
        !trade_continuity_ready_ && !dispatch_trade_continuity()) {
      return false;
    }
    dispatch::RunInputCandidate candidate{
        .event_id = digest_id<contracts::EventId>(input.semantic_checksum),
        .event_type = std::string(input.event_type),
        .event_position = contracts::EventPosition::from(
                              dispatcher_config_.input_stream_id,
                              dispatcher_config_.input_stream_epoch,
                              dispatcher_.current_run_input_sequence() + 1)
                              .value(),
        .semantic_payload = std::vector<std::byte>(
            input.semantic_payload.begin(), input.semantic_payload.end()),
        .semantic_checksum = input.semantic_checksum,
    };
    return dispatch_fact(candidate, facts_[index]);
  }

  [[nodiscard]] const ViewReplayResult &result() const noexcept {
    return result_;
  }

private:
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
        .control_stream_id = id<contracts::StreamId>(32),
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
        .maximum_publication_transitions = 15,
        .maximum_retained_views = 5,
    };
  }

  bool dispatch_trade_continuity() {
    auto payload = bytes("m4.6-trade-continuity-v1");
    dispatch::RunInputCandidate candidate{
        .event_id = id<contracts::EventId>(71),
        .event_type = "market.trade.continuity.synchronized",
        .event_position = contracts::EventPosition::from(
                              dispatcher_config_.input_stream_id, 1,
                              dispatcher_.current_run_input_sequence() + 1)
                              .value(),
        .semantic_payload = payload,
        .semantic_checksum = contracts::sha256(payload),
    };
    const auto dispatched = dispatcher_.dispatch(candidate, consumer_);
    if (!dispatched.ok())
      return false;
    const auto role_position =
        contracts::EventPosition::from(id<contracts::StreamId>(42), 1, 0)
            .value();
    const state::TradeContinuityProof proof{
        .boundary_event_id = candidate.event_id,
        .boundary_cursor = cursor(42, 0),
        .prior_trade_cursor = origin(41),
        .recovered_trade_cursor = origin(41),
        .fidelity = state::TradeFidelity::Lossless,
    };
    if (!auxiliary_
             .apply_quality_input({
                 .event_id = candidate.event_id,
                 .input_semantic_checksum = candidate.semantic_checksum,
                 .listing_id = book_.listing_id(),
                 .kind = state::ListingQualityInputKind::TradeSynchronized,
                 .run_input_sequence = dispatched.selection->run_input_sequence,
                 .logical_time_nanoseconds =
                     logical_time(dispatched.selection->run_input_sequence),
                 .trade_proof = proof,
             })
             .ok()) {
      return false;
    }
    trade_continuity_ready_ =
        publish(candidate, *dispatched.selection, role_position, 42, 0);
    return trade_continuity_ready_;
  }

  bool dispatch_fact(const dispatch::RunInputCandidate &candidate,
                     const Fact &fact) {
    const auto dispatched = dispatcher_.dispatch(candidate, consumer_);
    if (!dispatched.ok())
      return false;
    const auto run_sequence = dispatched.selection->run_input_sequence;
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
    const auto &trade = std::get<market::NormalizedTradeFact>(fact);
    const auto role_sequence = next_trade_sequence_++;
    if (!apply_trade(trade, candidate, run_sequence, role_sequence))
      return false;
    const auto role_position =
        contracts::EventPosition::from(id<contracts::StreamId>(41), 1,
                                       role_sequence)
            .value();
    return publish(candidate, *dispatched.selection, role_position, 41,
                   role_sequence);
  }

  bool apply_book(const market::NormalizedBookFact &fact,
                  const dispatch::RunInputCandidate &candidate,
                  std::uint64_t run_sequence, std::uint64_t role_sequence) {
    const state::L2InputEvidence evidence{
        .event_id = candidate.event_id,
        .semantic_checksum = candidate.semantic_checksum,
        .kind = role_sequence == 0 ? state::L2InputKind::Snapshot
                                   : state::L2InputKind::Delta,
    };
    if (const auto *snapshot =
            std::get_if<market::BookSnapshotObservation>(&fact.payload)) {
      std::vector<state::L2Level> bids;
      std::vector<state::L2Level> asks;
      for (const auto &level : snapshot->bids)
        bids.push_back({level.price, level.quantity});
      for (const auto &level : snapshot->asks)
        asks.push_back({level.price, level.quantity});
      if (!book_
               .apply_snapshot({
                   .listing_id = fact.listing_id,
                   .input_evidence = evidence,
                   .bids = std::move(bids),
                   .asks = std::move(asks),
                   .bid_completeness = state::L2SideCompleteness::Complete,
                   .ask_completeness = state::L2SideCompleteness::Complete,
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

  bool apply_trade(const market::NormalizedTradeFact &fact,
                   const dispatch::RunInputCandidate &candidate,
                   std::uint64_t run_sequence, std::uint64_t role_sequence) {
    return auxiliary_
        .apply_trade({
            .event_id = candidate.event_id,
            .input_semantic_checksum = candidate.semantic_checksum,
            .source_event_id = fact.source_lineage.source_event_id,
            .listing_id = fact.listing_id,
            .cursor = cursor(41, role_sequence),
            .source_event_time = fact.source_event_time,
            .source_time_quality = state::SourceTimeQuality::Exact,
            .fidelity = state::TradeFidelity::Lossless,
            .price = fact.price,
            .quantity = fact.quantity,
            .aggressor_side = fact.aggressor_side == market::AggressorSide::Buy
                                  ? state::TradeAggressorSide::Buy
                                  : state::TradeAggressorSide::Sell,
            .run_input_sequence = run_sequence,
            .logical_time_nanoseconds = logical_time(run_sequence),
        })
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
            .canonical_instrument_id = canonical_instrument_id_,
            .reference_snapshot_version = reference_snapshot_version_,
            .listing_definition_version = listing_definition_version_,
            .reference_configuration_lineage_version =
                reference_configuration_lineage_version_,
            .lineage = lineage,
        },
        book_, auxiliary_);
    if (!accepted.ok() || !accepted.view || !accepted.bundle)
      return false;
    const auto attempt = id<contracts::PublicationAttemptId>(
        static_cast<std::uint8_t>(80 + selection.run_input_sequence));
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
    result_.views.push_back(*accepted.view);
    result_.bundles.push_back(*accepted.bundle);
    return true;
  }

  static std::int64_t logical_time(std::uint64_t run_sequence) {
    return static_cast<std::int64_t>(1000 + run_sequence);
  }

  const std::vector<Fact> &facts_;
  GoldenDispatchPersistence persistence_;
  GoldenEligibilityRegistry registry_;
  GoldenDispatchConsumer consumer_;
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
  std::uint64_t next_trade_sequence_{};
  bool trade_continuity_ready_{};
  ViewReplayResult result_;
};

ViewReplayResult
replay_market_state(const NormalizedRun &run,
                    const replay::NormalizedFactDataset &data,
                    const replay::ReplayRunManifest &manifest) {
  const auto &initial = std::get<market::NormalizedBookFact>(run.facts.front());
  MarketStateReplaySink sink(run.facts, initial, manifest.run_id());
  const auto replayed = replay::replay_normalized_facts(manifest, data, sink);
  if (!replayed.ok())
    throw std::runtime_error("market-state replay failed");
  return sink.result();
}

contracts::Sha256Digest view_replay_digest(const ViewReplayResult &result) {
  std::vector<std::byte> bytes;
  bytes.reserve(result.views.size() * 96);
  for (std::size_t index = 0; index < result.views.size(); ++index) {
    for (const auto byte : result.views[index].view_id.bytes())
      bytes.push_back(static_cast<std::byte>(byte));
    for (const auto byte : result.views[index].semantic_checksum.bytes)
      bytes.push_back(static_cast<std::byte>(byte));
    for (const auto byte : result.bundles[index].bundle_id.bytes())
      bytes.push_back(static_cast<std::byte>(byte));
    for (const auto byte : result.bundles[index].semantic_checksum.bytes)
      bytes.push_back(static_cast<std::byte>(byte));
  }
  return contracts::sha256(bytes);
}

} // namespace

TEST_CASE("captured session rerun preserves normalized bytes and views") {
  const auto dataset = captured_session();
  CHECK(dataset.ok());
  const auto first = normalize_session(dataset);
  const auto second = normalize_session(dataset);
  CHECK(first.facts == second.facts);
  CHECK(first.records == second.records);
  CHECK(first.records.size() == 4);
  const auto semantic_stream = concatenate(first.records);
  CHECK(semantic_stream == concatenate(second.records));

  const auto normalized =
      replay::NormalizedFactDataset::create(first.records).value();
  CHECK(normalized.identity().hex() ==
        golden_digest("m3-normalized-stream.sha256"));
  const replay::ReplayVersionPins faithful_pins{
      .provider_version = "capture-order-provider-v1",
      .merge_policy_version = "single-stream-capture-order-v1",
      .schema_registry_version = "bybit-v5-public-registry-v1",
      .canonicalization_version = "m3-normalized-golden-v1",
      .normalizer_version = "chronos-market-normalizers-v1",
      .reference_lineage_version = reference_lineage().version(),
      .expected_normalized_dataset_identity = normalized.identity(),
  };
  const auto faithful_manifest =
      replay::ReplayRunManifest::create(
          id<contracts::RunId>(29), replay::ReplayClass::FaithfulCaptureOrder,
          digest(dataset.manifest()->dataset_id), faithful_pins)
          .value();
  FaithfulNormalizationSink faithful_sink(dataset, faithful_pins);
  const auto faithful =
      replay::replay_capture_order(faithful_manifest, dataset, faithful_sink);
  CHECK(faithful.ok());
  CHECK(faithful.dispatched_count == 3);

  auto wrong_pins = faithful_pins;
  wrong_pins.normalizer_version = "other-normalizer-v1";
  const auto wrong_manifest =
      replay::ReplayRunManifest::create(
          id<contracts::RunId>(28), replay::ReplayClass::FaithfulCaptureOrder,
          digest(dataset.manifest()->dataset_id), wrong_pins)
          .value();
  FaithfulNormalizationSink wrong_sink(dataset, wrong_pins);
  CHECK(replay::replay_capture_order(wrong_manifest, dataset, wrong_sink)
            .failure == replay::ReplayFailure::SemanticMismatch);

  const replay::ReplayVersionPins pins{
      .provider_version = "normalized-fact-provider-v1",
      .merge_policy_version = "single-stream-normalized-order-v1",
      .schema_registry_version = "bybit-v5-public-registry-v1",
      .canonicalization_version = "m3-normalized-golden-v1",
  };
  const auto manifest =
      replay::ReplayRunManifest::create(id<contracts::RunId>(30),
                                        replay::ReplayClass::NormalizedFact,
                                        normalized.identity(), pins)
          .value();
  ByteSink sink(normalized.records());
  const auto replayed =
      replay::replay_normalized_facts(manifest, normalized, sink);
  CHECK(replayed.ok());
  CHECK(replayed.dispatched_count == 4);
  CHECK(sink.output == semantic_stream);

  const auto first_views = replay_market_state(first, normalized, manifest);
  const auto second_views = replay_market_state(second, normalized, manifest);
  CHECK(first_views == second_views);
  CHECK(first_views.views.size() == 5);
  CHECK(first_views.bundles.size() == 5);
  for (std::size_t index = 0; index < first_views.views.size(); ++index) {
    CHECK(first_views.bundles[index].listing_view_id ==
          first_views.views[index].view_id);
    CHECK(first_views.bundles[index].run_input_sequence == index + 1);
    if (index > 0) {
      CHECK(first_views.views[index].prior_view_id ==
            first_views.views[index - 1].view_id);
      CHECK(first_views.bundles[index].prior_bundle_id ==
            first_views.bundles[index - 1].bundle_id);
    }
  }
  const auto &final_view = first_views.views.back();
  CHECK(final_view.bids.size() == 1);
  CHECK(final_view.bids[0].price.units() == 420002);
  CHECK(final_view.bids[0].quantity.units() == 750);
  CHECK(final_view.asks.size() == 2);
  CHECK(final_view.asks[0].price.units() == 420003);
  CHECK(final_view.asks[0].quantity.units() == 300);
  CHECK(final_view.top.shape == state::L2BookShape::Normal);
  CHECK(final_view.top.spread.has_value());
  if (final_view.top.spread)
    CHECK(final_view.top.spread->units() == 1);
  CHECK(final_view.recent_trades.size() == 2);
  CHECK(final_view.recent_trades[0].price.units() == 165786);
  CHECK(final_view.recent_trades[0].quantity.units() == 2);
  CHECK(final_view.recent_trades[1].price.units() == 165785);
  CHECK(final_view.recent_trades[1].quantity.units() == 1);
  CHECK(final_view.quality.book_synchronization ==
        state::BookSynchronization::Synchronized);
  CHECK(final_view.quality.trade_continuity ==
        state::TradeContinuity::Continuous);
  CHECK(view_replay_digest(first_views).hex() ==
        golden_digest("m4-market-state-views.sha256"));
}
