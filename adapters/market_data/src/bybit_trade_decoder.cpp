#include "chronos/adapters/market_data/bybit_trade_decoder.hpp"

#include "bybit_decode_support.hpp"

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace chronos::adapters::market_data {
namespace {

namespace trade = chronos::normalization::market_data;
using detail::JsonFailure;
using detail::JsonKind;
using detail::JsonValue;
using trade::TradeNormalizationFailure;

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

void collect_extensions(const JsonValue &object,
                        std::initializer_list<std::string_view> known_members,
                        std::vector<trade::SourceExtension> &extensions) {
  for (const auto &value : object.object) {
    const auto known =
        std::find(known_members.begin(), known_members.end(), value.key);
    if (known == known_members.end()) {
      extensions.push_back(
          {.name = value.key, .canonical_json = detail::canonical_json(value)});
    }
  }
}

TradeNormalizationFailure decode_member(const JsonValue &value,
                                        std::string_view topic,
                                        std::string_view expected_symbol,
                                        std::uint64_t system_timestamp,
                                        std::uint32_t member_index,
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
  collect_extensions(value,
                     {"T", "s", "S", "v", "p", "L", "i", "BT", "RPI", "seq"},
                     output.extensions);
  return TradeNormalizationFailure::None;
}

} // namespace

BybitTradeDecodeResult
decode_bybit_v5_trades(const CaptureDatasetManifest &manifest,
                       const CaptureDatasetRecord &record,
                       const BybitTradeDecodeLimits &limits) {
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
  collect_extensions(*root, {"topic", "type", "ts", "data"},
                     message.envelope_extensions);
  message.members.reserve(data->array.size());
  for (std::size_t index = 0; index < data->array.size(); ++index) {
    trade::SourceTradeAssertions assertions;
    const auto failure = decode_member(
        data->array[index], *topic, *expected_symbol, *system_timestamp,
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

  return {
      .decoded =
          trade::BoundDecodedTradeMessage{
              .message = std::move(message),
              .source_lineage =
                  trade::SourceCaptureLineage{
                      .source_event_id = record.source_event_id,
                      .capture_session_id = manifest.capture_session_id,
                      .runtime_id = manifest.runtime_id,
                      .connection_id = manifest.connection_id,
                      .subscription_id = manifest.subscription_id,
                      .capture_partition_id = manifest.capture_partition_id,
                      .capture_sequence = record.capture_sequence,
                      .chronos_receive_time = record.chronos_receive_time,
                      .payload_digest = record.payload_digest,
                  },
          },
      .failure = TradeNormalizationFailure::None,
  };
}

} // namespace chronos::adapters::market_data
