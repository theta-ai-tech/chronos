#include "chronos/adapters/market_data/bybit_book_decoder.hpp"
#include "chronos/adapters/market_data/bybit_trade_decoder.hpp"
#include "chronos/normalization/market_data/book_normalizer.hpp"
#include "chronos/normalization/market_data/trade_normalizer.hpp"
#include "chronos/runtime/datasets/replay.hpp"

#include "microtest.hpp"

#include <algorithm>
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

std::string golden_digest() {
  const auto path = std::filesystem::path(CHRONOS_SOURCE_DIR) / "tests" /
                    "replay" / "fixtures" / "m3-normalized-stream.sha256";
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

} // namespace

TEST_CASE("captured session rerun is byte-for-semantics identical") {
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
  CHECK(normalized.identity().hex() == golden_digest());
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
}
