#include "chronos/adapters/market_data/bybit_trade_decoder.hpp"

#include "bybit_decode_support.hpp"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace chronos::adapters::market_data {

class BybitTradeDecoderAccess final {
public:
  static normalization::market_data::DecodedTradeEnrichment
  bind(normalization::market_data::DecodedTradeMessage message,
       normalization::market_data::SourceCaptureLineage source_lineage,
       std::vector<normalization::market_data::SourceDecodeEvidence>
           decode_evidence) {
    return {std::move(message), std::move(source_lineage),
            std::move(decode_evidence)};
  }
};

namespace {

namespace trade = chronos::normalization::market_data;
using detail::JsonFailure;
using detail::JsonKind;
using detail::JsonValue;
using trade::TradeNormalizationFailure;

constexpr std::string_view kDecoderVersion = "bybit-trade-decoder-v1";
constexpr std::string_view kSourceSchemaVersion = "bybit-v5-public-trade-v1";
constexpr std::string_view kRegistryVersion = "bybit-v5-public-registry-v1";
constexpr std::string_view kCanonicalizationVersion =
    "chronos-source-enrichment-v1";

void append_u64(std::vector<std::byte> &output, std::uint64_t value) {
  for (std::size_t index = 0; index < sizeof(value); ++index) {
    output.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xFFU));
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
  for (const auto byte : value.bytes())
    output.push_back(static_cast<std::byte>(byte));
}

TradeNormalizationFailure parser_failure(JsonFailure failure) {
  switch (failure) {
  case JsonFailure::ResourceLimit:
    return TradeNormalizationFailure::ResourceLimitExceeded;
  case JsonFailure::DuplicateMember:
    return TradeNormalizationFailure::AmbiguousDuplicate;
  case JsonFailure::None:
  case JsonFailure::Malformed:
    return TradeNormalizationFailure::MalformedPayload;
  }
  return TradeNormalizationFailure::MalformedPayload;
}

std::optional<std::string_view> topic_symbol(std::string_view topic) {
  constexpr std::string_view prefix = "publicTrade.";
  if (!topic.starts_with(prefix) || topic.size() == prefix.size()) {
    return std::nullopt;
  }
  const auto symbol = topic.substr(prefix.size());
  if (symbol.find('.') != std::string_view::npos) {
    return std::nullopt;
  }
  return symbol;
}

bool valid_tick_direction(std::string_view value) {
  return value == "PlusTick" || value == "ZeroPlusTick" ||
         value == "MinusTick" || value == "ZeroMinusTick";
}

std::string pointer_token(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (const auto character : value) {
    if (character == '~')
      result += "~0";
    else if (character == '/')
      result += "~1";
    else
      result.push_back(character);
  }
  return result;
}

void collect_extensions(const JsonValue &object,
                        std::initializer_list<std::string_view> known_members,
                        std::string_view pointer_prefix,
                        std::vector<trade::SourceExtensionField> &extensions) {
  for (const auto &value : object.object) {
    const auto known =
        std::find(known_members.begin(), known_members.end(), value.key);
    if (known == known_members.end()) {
      extensions.push_back({
          .json_pointer =
              std::string(pointer_prefix) + "/" + pointer_token(value.key),
          .canonical_json = detail::canonical_json(value),
      });
    }
  }
  std::sort(extensions.begin(), extensions.end(),
            [](const auto &left, const auto &right) {
              return left.json_pointer < right.json_pointer;
            });
}

TradeNormalizationFailure
decode_member(const JsonValue &value, std::string_view topic,
              std::string_view expected_symbol, std::string_view venue,
              sdk::EnvironmentClass environment,
              trade::SourceProductClass product_class,
              std::uint64_t system_timestamp, std::uint32_t member_index,
              trade::SourceTradeAssertions &output) {
  if (value.kind != JsonKind::Object) {
    return TradeNormalizationFailure::SchemaViolation;
  }

  const auto trade_timestamp =
      detail::positive_integer(detail::member(value, "T"));
  const auto symbol = detail::string_value(detail::member(value, "s"));
  const auto side = detail::string_value(detail::member(value, "S"));
  const auto quantity = detail::string_value(detail::member(value, "v"));
  const auto price = detail::string_value(detail::member(value, "p"));
  const auto tick_direction = detail::string_value(detail::member(value, "L"));
  const auto trade_id = detail::string_value(detail::member(value, "i"));
  const auto block_trade = detail::boolean_value(detail::member(value, "BT"));

  if (!trade_timestamp.has_value()) {
    return TradeNormalizationFailure::InvalidTime;
  }
  if (!symbol.has_value() || *symbol != expected_symbol) {
    return TradeNormalizationFailure::WrongTopicOrSymbol;
  }
  if (!side.has_value()) {
    return TradeNormalizationFailure::InvalidSide;
  }
  if (!quantity.has_value() || !price.has_value()) {
    return TradeNormalizationFailure::InvalidNumeric;
  }
  if (!trade_id.has_value() || trade_id->empty()) {
    return TradeNormalizationFailure::InvalidTradeId;
  }
  if (!tick_direction.has_value() || !valid_tick_direction(*tick_direction) ||
      !block_trade.has_value()) {
    return TradeNormalizationFailure::SchemaViolation;
  }

  std::optional<bool> rpi_trade;
  if (detail::member(value, "RPI") != nullptr) {
    rpi_trade = detail::boolean_value(detail::member(value, "RPI"));
    if (!rpi_trade.has_value()) {
      return TradeNormalizationFailure::SchemaViolation;
    }
  }
  std::optional<std::uint64_t> sequence;
  if (detail::member(value, "seq") != nullptr) {
    sequence = detail::positive_integer(detail::member(value, "seq"));
    if (!sequence.has_value()) {
      return TradeNormalizationFailure::InvalidNumeric;
    }
  }

  output = {
      .venue = std::string(venue),
      .environment = environment,
      .product_class = product_class,
      .topic = std::string(topic),
      .source_symbol = std::string(*symbol),
      .source_trade_id = std::string(*trade_id),
      .source_side = std::string(*side),
      .price_decimal = std::string(*price),
      .quantity_decimal = std::string(*quantity),
      .tick_direction = std::string(*tick_direction),
      .block_trade = *block_trade,
      .rpi_trade = rpi_trade,
      .sequence = sequence,
      .sequence_scope = sdk::SourceSequenceScope::VenueCrossSequence,
      .system_timestamp_milliseconds = system_timestamp,
      .trade_timestamp_milliseconds = *trade_timestamp,
      .timestamp_unit = trade::SourceTimestampUnit::Milliseconds,
      .member_index = member_index,
  };
  collect_extensions(
      value, {"T", "s", "S", "v", "p", "L", "i", "BT", "RPI", "seq"},
      "/data/" + std::to_string(member_index), output.extensions);
  return TradeNormalizationFailure::None;
}

std::optional<trade::SourceProductClass>
source_product_class(sdk::MarketClass market) {
  switch (market) {
  case sdk::MarketClass::Spot:
    return trade::SourceProductClass::Spot;
  case sdk::MarketClass::LinearPerpetual:
    return trade::SourceProductClass::LinearPerpetual;
  case sdk::MarketClass::InversePerpetual:
    return std::nullopt;
  }
  return std::nullopt;
}

trade::SourceCaptureLineage
source_lineage(const CaptureDatasetManifest &manifest,
               const CaptureDatasetRecord &record, std::size_t record_index) {
  return {
      .dataset_format_version = manifest.format_version,
      .dataset_id = manifest.dataset_id,
      .records_sha256 = manifest.records_sha256,
      .dataset_record_index = record_index,
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
}

std::optional<trade::SourceDecodeEvidence> decode_evidence(
    const trade::SourceTradeAssertions &member,
    const std::vector<trade::SourceExtensionField> &envelope_extensions,
    const trade::SourceCaptureLineage &lineage) {
  std::vector<std::byte> semantic;
  append_string(semantic, kDecoderVersion);
  append_string(semantic, kSourceSchemaVersion);
  append_string(semantic, kRegistryVersion);
  append_string(semantic, kCanonicalizationVersion);
  append_string(semantic, lineage.dataset_id);
  append_id(semantic, lineage.source_event_id);
  append_u64(semantic, lineage.capture_sequence);
  append_u64(semantic, member.member_index);
  append_string(semantic, member.venue);
  append_u64(semantic, static_cast<std::uint64_t>(member.environment));
  append_u64(semantic, static_cast<std::uint64_t>(member.product_class));
  append_string(semantic, member.topic);
  append_string(semantic, member.source_symbol);
  append_string(semantic, member.source_trade_id);
  append_string(semantic, member.source_side);
  append_string(semantic, member.price_decimal);
  append_string(semantic, member.quantity_decimal);
  append_string(semantic, member.tick_direction);
  append_u64(semantic, member.block_trade ? 1U : 0U);
  append_u64(semantic, member.rpi_trade.has_value() ? 1U : 0U);
  if (member.rpi_trade.has_value())
    append_u64(semantic, *member.rpi_trade ? 1U : 0U);
  append_u64(semantic, member.sequence.has_value() ? 1U : 0U);
  if (member.sequence.has_value())
    append_u64(semantic, *member.sequence);
  append_u64(semantic, member.system_timestamp_milliseconds);
  append_u64(semantic, member.trade_timestamp_milliseconds);
  const auto append_extensions = [&semantic](const auto &extensions) {
    append_u64(semantic, static_cast<std::uint64_t>(extensions.size()));
    for (const auto &extension : extensions) {
      append_string(semantic, extension.json_pointer);
      append_string(semantic, extension.canonical_json);
    }
  };
  append_extensions(envelope_extensions);
  append_extensions(member.extensions);

  const auto checksum =
      sdk::sha256(semantic, sdk::DigestCoverage::CompletePayload);
  contracts::SourceDecodeEnrichmentId::bytes_type identity_bytes{};
  std::copy_n(checksum.bytes.begin(), identity_bytes.size(),
              identity_bytes.begin());
  const auto identity =
      contracts::SourceDecodeEnrichmentId::from_bytes(identity_bytes);
  if (!identity.has_value())
    return std::nullopt;
  return trade::SourceDecodeEvidence{
      .source_decode_enrichment_id = *identity,
      .source_member_index = member.member_index,
      .decoder_version = std::string(kDecoderVersion),
      .source_schema_version = std::string(kSourceSchemaVersion),
      .registry_version = std::string(kRegistryVersion),
      .canonicalization_version = std::string(kCanonicalizationVersion),
      .semantic_checksum = checksum,
  };
}

bool add_bounded(std::size_t &total, std::size_t value, std::size_t maximum) {
  if (value > maximum || total > maximum - value)
    return false;
  total += value;
  return true;
}

std::size_t
extension_bytes(const std::vector<trade::SourceExtensionField> &extensions) {
  std::size_t total{};
  for (const auto &extension : extensions)
    total += extension.json_pointer.size() + extension.canonical_json.size();
  return total;
}

bool normalized_output_within_limit(const trade::DecodedTradeMessage &message,
                                    std::size_t maximum) {
  constexpr std::size_t fixed_fact_bytes = 512;
  const auto envelope_bytes = extension_bytes(message.envelope_extensions);
  std::size_t total{};
  for (const auto &member : message.members) {
    std::size_t member_bytes = fixed_fact_bytes;
    const std::array strings{std::string_view(member.venue),
                             std::string_view(member.topic),
                             std::string_view(member.source_symbol),
                             std::string_view(member.source_trade_id),
                             std::string_view(member.source_side),
                             std::string_view(member.price_decimal),
                             std::string_view(member.quantity_decimal),
                             std::string_view(member.tick_direction)};
    for (const auto value : strings) {
      if (!add_bounded(member_bytes, value.size(), maximum))
        return false;
    }
    if (!add_bounded(member_bytes, extension_bytes(member.extensions),
                     maximum) ||
        !add_bounded(member_bytes, envelope_bytes, maximum) ||
        !add_bounded(total, member_bytes, maximum)) {
      return false;
    }
  }
  return true;
}

} // namespace

BybitTradeDecodeResult
decode_bybit_v5_trades(const DatasetReadResult &dataset,
                       std::size_t record_index,
                       const BybitTradeDecodeLimits &limits) {
  if (!dataset.ok() || record_index >= dataset.records().size()) {
    return {.failure = TradeNormalizationFailure::IntegrityIneligible};
  }
  const auto &manifest = dataset.manifest().value();
  const auto &record = dataset.records()[record_index];
  if (manifest.format_version != "chronos-source-capture-v2") {
    return {.failure = TradeNormalizationFailure::IntegrityIneligible};
  }
  const auto product_class = source_product_class(manifest.market);
  if (!product_class.has_value()) {
    return {.failure = TradeNormalizationFailure::UnsupportedMessage};
  }
  if (record.raw_payload.size() > limits.maximum_payload_bytes) {
    return {.failure = TradeNormalizationFailure::ResourceLimitExceeded};
  }
  if (!detail::capture_record_eligible(manifest, record,
                                       limits.maximum_payload_bytes)) {
    return {.failure = TradeNormalizationFailure::IntegrityIneligible};
  }

  const std::string_view payload{
      reinterpret_cast<const char *>(record.raw_payload.data()),
      record.raw_payload.size()};
  if (!detail::valid_utf8(payload)) {
    return {.failure = TradeNormalizationFailure::MalformedPayload};
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
    return {.failure = TradeNormalizationFailure::SchemaViolation};
  }

  const auto topic = detail::string_value(detail::member(*root, "topic"));
  const auto type = detail::string_value(detail::member(*root, "type"));
  const auto system_timestamp =
      detail::positive_integer(detail::member(*root, "ts"));
  const auto *data = detail::member(*root, "data");
  if (!topic.has_value() || data == nullptr || data->kind != JsonKind::Array) {
    return {.failure = TradeNormalizationFailure::SchemaViolation};
  }
  if (!type.has_value() || *type != "snapshot") {
    return {.failure = TradeNormalizationFailure::UnsupportedMessage};
  }
  if (!system_timestamp.has_value()) {
    return {.failure = TradeNormalizationFailure::InvalidTime};
  }
  const auto expected_symbol = topic_symbol(*topic);
  if (!expected_symbol.has_value()) {
    return {.failure = TradeNormalizationFailure::WrongTopicOrSymbol};
  }
  if (data->array.empty()) {
    return {.failure = TradeNormalizationFailure::SchemaViolation};
  }
  if (data->array.size() > limits.maximum_trades_per_message ||
      data->array.size() >
          static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    return {.failure = TradeNormalizationFailure::ResourceLimitExceeded};
  }

  trade::DecodedTradeMessage message;
  collect_extensions(*root, {"topic", "type", "ts", "data"}, "",
                     message.envelope_extensions);
  message.members.reserve(data->array.size());
  for (std::size_t index = 0; index < data->array.size(); ++index) {
    trade::SourceTradeAssertions assertions;
    const auto failure = decode_member(
        data->array[index], *topic, *expected_symbol, manifest.venue,
        manifest.environment, *product_class, *system_timestamp,
        static_cast<std::uint32_t>(index), assertions);
    if (failure != TradeNormalizationFailure::None) {
      return {.failure = failure};
    }
    const auto duplicate = std::find_if(
        message.members.begin(), message.members.end(), [&](const auto &prior) {
          return prior.source_trade_id == assertions.source_trade_id;
        });
    if (duplicate != message.members.end()) {
      return {.failure = TradeNormalizationFailure::AmbiguousDuplicate};
    }
    message.members.push_back(std::move(assertions));
  }

  if (!normalized_output_within_limit(message,
                                      limits.maximum_normalized_output_bytes)) {
    return {.failure = TradeNormalizationFailure::ResourceLimitExceeded};
  }

  auto lineage = source_lineage(manifest, record, record_index);
  std::vector<trade::SourceDecodeEvidence> evidence;
  evidence.reserve(message.members.size());
  for (const auto &member : message.members) {
    auto member_evidence =
        decode_evidence(member, message.envelope_extensions, lineage);
    if (!member_evidence.has_value()) {
      return {.failure = TradeNormalizationFailure::SchemaViolation};
    }
    evidence.push_back(std::move(*member_evidence));
  }

  return {
      .enrichment = BybitTradeDecoderAccess::bind(
          std::move(message), std::move(lineage), std::move(evidence)),
      .failure = TradeNormalizationFailure::None,
  };
}

} // namespace chronos::adapters::market_data
