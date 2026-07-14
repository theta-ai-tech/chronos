#include "chronos/adapters/market_data/bybit_book_decoder.hpp"

#include "bybit_decode_support.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace chronos::adapters::market_data {

class BybitBookDecoderAccess final {
public:
  static normalization::market_data::DecodedBookEnrichment
  bind(normalization::market_data::DecodedBookMessage message,
       normalization::market_data::SourceCaptureLineage source_lineage,
       normalization::market_data::SourceDecodeEvidence decode_evidence) {
    return {std::move(message), std::move(source_lineage),
            std::move(decode_evidence)};
  }
};

namespace {

namespace book = chronos::normalization::market_data;
using book::BookNormalizationFailure;
using detail::JsonFailure;
using detail::JsonKind;
using detail::JsonValue;

constexpr std::string_view kDecoderVersion = "bybit-book-decoder-v2";
constexpr std::string_view kSourceSchemaVersion = "bybit-v5-orderbook-v1";
constexpr std::string_view kRegistryVersion = "bybit-v5-public-registry-v1";
constexpr std::string_view kCanonicalizationVersion =
    "chronos-source-enrichment-v1";

void append_u64(std::vector<std::byte> &output, std::uint64_t value) {
  for (std::size_t index = 0; index < sizeof(value); ++index) {
    output.push_back(static_cast<std::byte>(value >> (index * 8U)));
  }
}

void append_string(std::vector<std::byte> &output, std::string_view value) {
  append_u64(output, static_cast<std::uint64_t>(value.size()));
  if (value.empty())
    return;
  const auto *begin = reinterpret_cast<const std::byte *>(value.data());
  output.insert(output.end(), begin, begin + value.size());
}

template <typename Id>
void append_id(std::vector<std::byte> &output, const Id &value) {
  for (const auto byte : value.bytes()) {
    output.push_back(static_cast<std::byte>(byte));
  }
}

std::optional<book::SourceDecodeEvidence>
make_decode_evidence(const book::DecodedBookMessage &message,
                     const book::SourceCaptureLineage &lineage) {
  std::vector<std::byte> semantic;
  semantic.reserve(512 + message.bids.size() * 32 + message.asks.size() * 32);
  append_string(semantic, kDecoderVersion);
  append_string(semantic, kSourceSchemaVersion);
  append_string(semantic, kRegistryVersion);
  append_string(semantic, kCanonicalizationVersion);
  append_string(semantic, lineage.dataset_id);
  append_id(semantic, lineage.source_event_id);
  append_u64(semantic, lineage.capture_sequence);
  append_u64(semantic, static_cast<std::uint64_t>(message.assertions.kind));
  append_string(semantic, message.assertions.venue);
  append_u64(semantic,
             static_cast<std::uint64_t>(message.assertions.environment));
  append_u64(semantic,
             static_cast<std::uint64_t>(message.assertions.product_class));
  append_string(semantic, message.assertions.topic);
  append_string(semantic, message.assertions.source_symbol);
  append_u64(semantic, message.assertions.depth);
  append_u64(semantic, message.assertions.sequence);
  append_u64(semantic,
             static_cast<std::uint64_t>(message.assertions.sequence_scope));
  append_u64(semantic, message.assertions.update_id);
  append_u64(semantic,
             static_cast<std::uint64_t>(message.assertions.update_id_scope));
  append_u64(semantic, message.assertions.system_timestamp_milliseconds);
  append_u64(semantic, message.assertions.matching_timestamp_milliseconds);
  const auto append_levels = [&semantic](const auto &levels) {
    append_u64(semantic, static_cast<std::uint64_t>(levels.size()));
    for (const auto &level : levels) {
      append_string(semantic, level.price_decimal);
      append_string(semantic, level.quantity_decimal);
    }
  };
  append_levels(message.bids);
  append_levels(message.asks);
  append_u64(semantic, static_cast<std::uint64_t>(message.extensions.size()));
  for (const auto &extension : message.extensions) {
    append_string(semantic, extension.json_pointer);
    append_string(semantic, extension.canonical_json);
  }
  const auto checksum =
      sdk::sha256(semantic, sdk::DigestCoverage::CompletePayload);
  contracts::SourceDecodeEnrichmentId::bytes_type identity_bytes{};
  std::copy_n(checksum.bytes.begin(), identity_bytes.size(),
              identity_bytes.begin());
  const auto identity =
      contracts::SourceDecodeEnrichmentId::from_bytes(identity_bytes);
  if (!identity.has_value()) {
    return std::nullopt;
  }
  return book::SourceDecodeEvidence{
      .source_decode_enrichment_id = *identity,
      .source_member_index = 0,
      .decoder_version = std::string(kDecoderVersion),
      .source_schema_version = std::string(kSourceSchemaVersion),
      .registry_version = std::string(kRegistryVersion),
      .canonicalization_version = std::string(kCanonicalizationVersion),
      .semantic_checksum = checksum,
  };
}

template <std::size_t Size>
bool known_member(std::string_view key,
                  const std::array<std::string_view, Size> &known) {
  return std::find(known.begin(), known.end(), key) != known.end();
}

std::string json_pointer_token(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (const auto character : value) {
    if (character == '~') {
      result += "~0";
    } else if (character == '/') {
      result += "~1";
    } else {
      result.push_back(character);
    }
  }
  return result;
}

BookNormalizationFailure
collect_extensions(const JsonValue &root, const JsonValue &data,
                   std::size_t maximum_bytes,
                   std::vector<book::SourceExtensionField> &output) {
  constexpr std::array<std::string_view, 5> root_members{"topic", "type", "ts",
                                                         "data", "cts"};
  constexpr std::array<std::string_view, 5> data_members{"s", "b", "a", "u",
                                                         "seq"};
  std::size_t bytes{};
  const auto collect = [&output, &bytes, maximum_bytes](const JsonValue &object,
                                                        std::string_view prefix,
                                                        const auto &known) {
    for (const auto &value : object.object) {
      if (known_member(value.key, known)) {
        continue;
      }
      book::SourceExtensionField extension{
          .json_pointer = std::string(prefix) + json_pointer_token(value.key)};
      extension.canonical_json = detail::canonical_json(value);
      bytes += extension.json_pointer.size() + extension.canonical_json.size();
      if (bytes > maximum_bytes) {
        return false;
      }
      output.push_back(std::move(extension));
    }
    return true;
  };
  if (!collect(root, "/", root_members) ||
      !collect(data, "/data/", data_members)) {
    return BookNormalizationFailure::ResourceLimitExceeded;
  }
  std::sort(output.begin(), output.end(),
            [](const auto &left, const auto &right) {
              return left.json_pointer < right.json_pointer;
            });
  for (std::size_t index = 1; index < output.size(); ++index) {
    if (output[index - 1].json_pointer == output[index].json_pointer) {
      return BookNormalizationFailure::AmbiguousDuplicate;
    }
  }
  return BookNormalizationFailure::None;
}

BookNormalizationFailure
parse_levels(const JsonValue *value, std::size_t maximum_levels,
             std::vector<book::SourceBookLevel> &output) {
  if (value == nullptr || value->kind != JsonKind::Array) {
    return BookNormalizationFailure::SchemaViolation;
  }
  if (value->array.size() > maximum_levels) {
    return BookNormalizationFailure::ResourceLimitExceeded;
  }
  output.reserve(value->array.size());
  for (const auto &level : value->array) {
    if (level.kind != JsonKind::Array || level.array.size() != 2 ||
        level.array[0].kind != JsonKind::String ||
        level.array[1].kind != JsonKind::String) {
      return BookNormalizationFailure::SchemaViolation;
    }
    output.push_back({.price_decimal = level.array[0].scalar,
                      .quantity_decimal = level.array[1].scalar});
  }
  return BookNormalizationFailure::None;
}

struct TopicParts final {
  std::uint32_t depth{};
  std::string_view symbol;
};

std::optional<TopicParts> parse_topic(std::string_view topic) {
  constexpr std::string_view prefix = "orderbook.";
  if (!topic.starts_with(prefix)) {
    return std::nullopt;
  }
  const auto separator = topic.find('.', prefix.size());
  if (separator == std::string_view::npos || separator == prefix.size() ||
      separator + 1 >= topic.size()) {
    return std::nullopt;
  }
  std::uint32_t depth{};
  const auto depth_text =
      topic.substr(prefix.size(), separator - prefix.size());
  const auto [end, error] = std::from_chars(
      depth_text.data(), depth_text.data() + depth_text.size(), depth);
  if (error != std::errc{} || end != depth_text.data() + depth_text.size() ||
      (depth != 1 && depth != 50 && depth != 200 && depth != 1000)) {
    return std::nullopt;
  }
  return TopicParts{.depth = depth, .symbol = topic.substr(separator + 1)};
}

BookNormalizationFailure parser_failure(JsonFailure failure) {
  switch (failure) {
  case JsonFailure::ResourceLimit:
    return BookNormalizationFailure::ResourceLimitExceeded;
  case JsonFailure::DuplicateMember:
    return BookNormalizationFailure::AmbiguousDuplicate;
  case JsonFailure::None:
  case JsonFailure::Malformed:
    return BookNormalizationFailure::MalformedPayload;
  }
  return BookNormalizationFailure::MalformedPayload;
}

std::optional<book::SourceProductClass>
source_product_class(sdk::MarketClass market) {
  switch (market) {
  case sdk::MarketClass::Spot:
    return book::SourceProductClass::Spot;
  case sdk::MarketClass::LinearPerpetual:
    return book::SourceProductClass::LinearPerpetual;
  case sdk::MarketClass::InversePerpetual:
    return std::nullopt;
  }
  return std::nullopt;
}

} // namespace

BybitBookDecodeResult
decode_bybit_v5_book(const DatasetReadResult &dataset, std::size_t record_index,
                     const BybitBookDecodeLimits &limits) {
  if (!dataset.ok() || record_index >= dataset.records().size()) {
    return {.failure = BookNormalizationFailure::IntegrityIneligible};
  }
  const auto &manifest = dataset.manifest().value();
  const auto &record = dataset.records()[record_index];
  const auto product_class = source_product_class(manifest.market);
  if (!product_class.has_value()) {
    return {.failure = BookNormalizationFailure::UnsupportedMessage};
  }
  if (record.raw_payload.size() > limits.maximum_payload_bytes) {
    return {.failure = BookNormalizationFailure::ResourceLimitExceeded};
  }
  if (!detail::capture_record_eligible(manifest, record,
                                       limits.maximum_payload_bytes)) {
    return {.failure = BookNormalizationFailure::IntegrityIneligible};
  }

  const std::string_view payload{
      reinterpret_cast<const char *>(record.raw_payload.data()),
      record.raw_payload.size()};
  if (!detail::valid_utf8(payload)) {
    return {.failure = BookNormalizationFailure::MalformedPayload};
  }
  const detail::JsonLimits json_limits{
      .maximum_json_depth = limits.maximum_json_depth,
      .maximum_json_nodes = limits.maximum_json_nodes,
      .maximum_object_members = limits.maximum_object_members,
      .maximum_string_bytes = limits.maximum_string_bytes,
      .maximum_number_bytes = limits.maximum_number_bytes,
  };
  detail::BoundedJsonParser parser(payload, json_limits);
  const auto root = parser.parse();
  if (!root.has_value()) {
    return {.failure = parser_failure(parser.failure())};
  }
  if (root->kind != JsonKind::Object) {
    return {.failure = BookNormalizationFailure::SchemaViolation};
  }

  const auto topic = detail::string_value(detail::member(*root, "topic"));
  const auto type = detail::string_value(detail::member(*root, "type"));
  const auto system_timestamp =
      detail::positive_integer(detail::member(*root, "ts"));
  const auto *data = detail::member(*root, "data");
  if (!topic.has_value() || !type.has_value() ||
      !system_timestamp.has_value() || data == nullptr ||
      data->kind != JsonKind::Object) {
    return {.failure = BookNormalizationFailure::SchemaViolation};
  }

  book::SourceBookKind kind{};
  if (*type == "snapshot") {
    kind = book::SourceBookKind::Snapshot;
  } else if (*type == "delta") {
    kind = book::SourceBookKind::Delta;
  } else {
    return {.failure = BookNormalizationFailure::UnsupportedMessage};
  }

  const auto topic_parts = parse_topic(*topic);
  const auto symbol = detail::string_value(detail::member(*data, "s"));
  if (!topic_parts.has_value() || topic_parts->depth != limits.expected_depth ||
      !symbol.has_value() || topic_parts->symbol != *symbol) {
    return {.failure = BookNormalizationFailure::WrongTopicOrSymbol};
  }

  const auto update_id = detail::positive_integer(detail::member(*data, "u"));
  const auto sequence = detail::positive_integer(detail::member(*data, "seq"));
  const auto matching_timestamp =
      detail::positive_integer(detail::member(*root, "cts"));
  if (!update_id.has_value() || !sequence.has_value() ||
      !matching_timestamp.has_value()) {
    return {.failure = BookNormalizationFailure::InvalidNumeric};
  }

  book::DecodedBookMessage message{
      .assertions = {
          .kind = kind,
          .venue = manifest.venue,
          .environment = manifest.environment,
          .product_class = *product_class,
          .topic = std::string(*topic),
          .source_symbol = std::string(*symbol),
          .depth = topic_parts->depth,
          .sequence = *sequence,
          .sequence_scope = sdk::SourceSequenceScope::VenueCrossSequence,
          .update_id = *update_id,
          .update_id_scope = sdk::SourceSequenceScope::ListingChannel,
          .system_timestamp_milliseconds = *system_timestamp,
          .matching_timestamp_milliseconds = *matching_timestamp,
          .timestamp_unit = book::SourceTimestampUnit::Milliseconds}};
  auto failure = parse_levels(detail::member(*data, "b"),
                              limits.maximum_levels_per_side, message.bids);
  if (failure == BookNormalizationFailure::None) {
    failure = parse_levels(detail::member(*data, "a"),
                           limits.maximum_levels_per_side, message.asks);
  }
  if (failure != BookNormalizationFailure::None) {
    return {.failure = failure};
  }
  failure = collect_extensions(*root, *data, limits.maximum_extension_bytes,
                               message.extensions);
  if (failure != BookNormalizationFailure::None) {
    return {.failure = failure};
  }

  book::SourceCaptureLineage source_lineage{
      .dataset_format_version = manifest.format_version,
      .dataset_id = manifest.dataset_id,
      .records_sha256 = manifest.records_sha256,
      .dataset_record_index = static_cast<std::uint64_t>(record_index),
      .source_event_id = record.source_event_id,
      .capture_session_id = manifest.capture_session_id,
      .runtime_id = manifest.runtime_id,
      .connection_id = manifest.connection_id,
      .subscription_id = manifest.subscription_id,
      .capture_partition_id = manifest.capture_partition_id,
      .capture_sequence = record.capture_sequence,
      .chronos_receive_time = record.chronos_receive_time,
      .payload_digest = record.payload_digest,
      .adapter_version = manifest.adapter_version,
      .build_version = manifest.build_version,
      .framing_version = manifest.framing_version,
      .static_configuration_version = manifest.static_configuration_version,
      .capability_manifest_version = manifest.capability_manifest_version,
      .schema_policy_version = manifest.schema_policy_version,
  };
  auto decode_evidence = make_decode_evidence(message, source_lineage);
  if (!decode_evidence.has_value()) {
    return {.failure = BookNormalizationFailure::SchemaViolation};
  }
  return {
      .enrichment = BybitBookDecoderAccess::bind(std::move(message),
                                                 std::move(source_lineage),
                                                 std::move(*decode_evidence)),
      .failure = BookNormalizationFailure::None,
  };
}

} // namespace chronos::adapters::market_data
