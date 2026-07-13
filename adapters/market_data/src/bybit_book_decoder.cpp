#include "chronos/adapters/market_data/bybit_book_decoder.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace chronos::adapters::market_data {
namespace {

namespace book = chronos::normalization::market_data;
using book::BookNormalizationFailure;

enum class JsonKind : std::uint8_t {
  Null,
  Boolean,
  Number,
  String,
  Array,
  Object,
};

struct JsonValue final {
  JsonKind kind{JsonKind::Null};
  std::string scalar;
  std::vector<JsonValue> array;
  std::vector<std::pair<std::string, JsonValue>> object;
};

enum class JsonFailure : std::uint8_t {
  None,
  Malformed,
  ResourceLimit,
  DuplicateMember,
};

class BoundedJsonParser final {
public:
  BoundedJsonParser(std::string_view input, const BybitBookDecodeLimits &limits)
      : input_(input), limits_(limits) {}

  std::optional<JsonValue> parse() {
    auto value = parse_value(0);
    whitespace();
    if (!value.has_value() || position_ != input_.size()) {
      if (failure_ == JsonFailure::None) {
        failure_ = JsonFailure::Malformed;
      }
      return std::nullopt;
    }
    return value;
  }

  [[nodiscard]] JsonFailure failure() const noexcept { return failure_; }

private:
  void whitespace() noexcept {
    while (position_ < input_.size() &&
           (input_[position_] == ' ' || input_[position_] == '\n' ||
            input_[position_] == '\r' || input_[position_] == '\t')) {
      ++position_;
    }
  }

  bool consume(char expected) noexcept {
    whitespace();
    if (position_ >= input_.size() || input_[position_] != expected) {
      return false;
    }
    ++position_;
    return true;
  }

  bool count_node() noexcept {
    if (++nodes_ > limits_.maximum_json_nodes) {
      failure_ = JsonFailure::ResourceLimit;
      return false;
    }
    return true;
  }

  std::optional<JsonValue> parse_value(std::size_t depth) {
    whitespace();
    if (failure_ != JsonFailure::None || depth > limits_.maximum_json_depth ||
        position_ >= input_.size()) {
      if (failure_ == JsonFailure::None) {
        failure_ = depth > limits_.maximum_json_depth
                       ? JsonFailure::ResourceLimit
                       : JsonFailure::Malformed;
      }
      return std::nullopt;
    }
    if (!count_node()) {
      return std::nullopt;
    }
    switch (input_[position_]) {
    case '{':
      return parse_object(depth);
    case '[':
      return parse_array(depth);
    case '"': {
      auto value = parse_string();
      if (!value.has_value()) {
        return std::nullopt;
      }
      return JsonValue{.kind = JsonKind::String, .scalar = std::move(*value)};
    }
    case 't':
      return parse_literal("true", JsonKind::Boolean);
    case 'f':
      return parse_literal("false", JsonKind::Boolean);
    case 'n':
      return parse_literal("null", JsonKind::Null);
    default:
      return parse_number();
    }
  }

  std::optional<JsonValue> parse_object(std::size_t depth) {
    JsonValue result{.kind = JsonKind::Object};
    ++position_;
    whitespace();
    if (consume('}')) {
      return result;
    }
    while (true) {
      if (result.object.size() >= limits_.maximum_object_members) {
        failure_ = JsonFailure::ResourceLimit;
        return std::nullopt;
      }
      auto key = parse_string();
      if (!key.has_value() || !consume(':')) {
        failure_ = JsonFailure::Malformed;
        return std::nullopt;
      }
      for (const auto &[existing, unused] : result.object) {
        (void)unused;
        if (existing == *key) {
          failure_ = JsonFailure::DuplicateMember;
          return std::nullopt;
        }
      }
      auto value = parse_value(depth + 1);
      if (!value.has_value()) {
        return std::nullopt;
      }
      result.object.emplace_back(std::move(*key), std::move(*value));
      if (consume('}')) {
        return result;
      }
      if (!consume(',')) {
        failure_ = JsonFailure::Malformed;
        return std::nullopt;
      }
    }
  }

  std::optional<JsonValue> parse_array(std::size_t depth) {
    JsonValue result{.kind = JsonKind::Array};
    ++position_;
    whitespace();
    if (consume(']')) {
      return result;
    }
    while (true) {
      if (result.array.size() >= limits_.maximum_json_nodes) {
        failure_ = JsonFailure::ResourceLimit;
        return std::nullopt;
      }
      auto value = parse_value(depth + 1);
      if (!value.has_value()) {
        return std::nullopt;
      }
      result.array.push_back(std::move(*value));
      if (consume(']')) {
        return result;
      }
      if (!consume(',')) {
        failure_ = JsonFailure::Malformed;
        return std::nullopt;
      }
    }
  }

  static std::optional<std::uint16_t> hex_quad(std::string_view input) {
    if (input.size() < 4) {
      return std::nullopt;
    }
    std::uint16_t value = 0;
    for (std::size_t index = 0; index < 4; ++index) {
      value = static_cast<std::uint16_t>(value << 4U);
      const char character = input[index];
      if (character >= '0' && character <= '9') {
        value = static_cast<std::uint16_t>(value + character - '0');
      } else if (character >= 'a' && character <= 'f') {
        value = static_cast<std::uint16_t>(value + character - 'a' + 10);
      } else if (character >= 'A' && character <= 'F') {
        value = static_cast<std::uint16_t>(value + character - 'A' + 10);
      } else {
        return std::nullopt;
      }
    }
    return value;
  }

  bool append_code_point(std::string &output, std::uint32_t code_point) {
    if (code_point <= 0x7FU) {
      output.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7FFU) {
      output.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
      output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else if (code_point <= 0xFFFFU) {
      output.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
      output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else {
      output.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
      output.push_back(
          static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    }
    if (output.size() > limits_.maximum_string_bytes) {
      failure_ = JsonFailure::ResourceLimit;
      return false;
    }
    return true;
  }

  std::optional<std::string> parse_string() {
    whitespace();
    if (position_ >= input_.size() || input_[position_++] != '"') {
      return std::nullopt;
    }
    std::string result;
    while (position_ < input_.size()) {
      const auto character = static_cast<unsigned char>(input_[position_++]);
      if (character == '"') {
        return result;
      }
      if (character < 0x20U) {
        return std::nullopt;
      }
      if (character != '\\') {
        result.push_back(static_cast<char>(character));
      } else {
        if (position_ >= input_.size()) {
          return std::nullopt;
        }
        const char escaped = input_[position_++];
        switch (escaped) {
        case '"':
        case '\\':
        case '/':
          result.push_back(escaped);
          break;
        case 'b':
          result.push_back('\b');
          break;
        case 'f':
          result.push_back('\f');
          break;
        case 'n':
          result.push_back('\n');
          break;
        case 'r':
          result.push_back('\r');
          break;
        case 't':
          result.push_back('\t');
          break;
        case 'u': {
          const auto high = hex_quad(input_.substr(position_));
          if (!high.has_value()) {
            return std::nullopt;
          }
          position_ += 4;
          std::uint32_t code_point = *high;
          if (*high >= 0xD800U && *high <= 0xDBFFU) {
            if (input_.substr(position_, 2) != "\\u") {
              return std::nullopt;
            }
            position_ += 2;
            const auto low = hex_quad(input_.substr(position_));
            if (!low.has_value() || *low < 0xDC00U || *low > 0xDFFFU) {
              return std::nullopt;
            }
            position_ += 4;
            code_point =
                0x10000U +
                ((static_cast<std::uint32_t>(*high) - 0xD800U) << 10U) +
                (static_cast<std::uint32_t>(*low) - 0xDC00U);
          } else if (*high >= 0xDC00U && *high <= 0xDFFFU) {
            return std::nullopt;
          }
          if (!append_code_point(result, code_point)) {
            return std::nullopt;
          }
          break;
        }
        default:
          return std::nullopt;
        }
      }
      if (result.size() > limits_.maximum_string_bytes) {
        failure_ = JsonFailure::ResourceLimit;
        return std::nullopt;
      }
    }
    return std::nullopt;
  }

  std::optional<JsonValue> parse_literal(std::string_view literal,
                                         JsonKind kind) {
    if (input_.substr(position_, literal.size()) != literal) {
      failure_ = JsonFailure::Malformed;
      return std::nullopt;
    }
    position_ += literal.size();
    return JsonValue{.kind = kind, .scalar = std::string(literal)};
  }

  std::optional<JsonValue> parse_number() {
    const auto start = position_;
    if (position_ < input_.size() && input_[position_] == '-') {
      ++position_;
    }
    if (position_ >= input_.size()) {
      failure_ = JsonFailure::Malformed;
      return std::nullopt;
    }
    if (input_[position_] == '0') {
      ++position_;
    } else if (input_[position_] >= '1' && input_[position_] <= '9') {
      while (position_ < input_.size() && input_[position_] >= '0' &&
             input_[position_] <= '9') {
        ++position_;
      }
    } else {
      failure_ = JsonFailure::Malformed;
      return std::nullopt;
    }
    if (position_ < input_.size() && input_[position_] == '.') {
      ++position_;
      const auto fraction = position_;
      while (position_ < input_.size() && input_[position_] >= '0' &&
             input_[position_] <= '9') {
        ++position_;
      }
      if (position_ == fraction) {
        failure_ = JsonFailure::Malformed;
        return std::nullopt;
      }
    }
    if (position_ < input_.size() &&
        (input_[position_] == 'e' || input_[position_] == 'E')) {
      ++position_;
      if (position_ < input_.size() &&
          (input_[position_] == '+' || input_[position_] == '-')) {
        ++position_;
      }
      const auto exponent = position_;
      while (position_ < input_.size() && input_[position_] >= '0' &&
             input_[position_] <= '9') {
        ++position_;
      }
      if (position_ == exponent) {
        failure_ = JsonFailure::Malformed;
        return std::nullopt;
      }
    }
    const auto token = input_.substr(start, position_ - start);
    if (token.size() > limits_.maximum_number_bytes) {
      failure_ = JsonFailure::ResourceLimit;
      return std::nullopt;
    }
    return JsonValue{.kind = JsonKind::Number, .scalar = std::string(token)};
  }

  std::string_view input_;
  const BybitBookDecodeLimits &limits_;
  std::size_t position_{};
  std::size_t nodes_{};
  JsonFailure failure_{JsonFailure::None};
};

const JsonValue *member(const JsonValue &object, std::string_view key) {
  if (object.kind != JsonKind::Object) {
    return nullptr;
  }
  for (const auto &[name, value] : object.object) {
    if (name == key) {
      return &value;
    }
  }
  return nullptr;
}

std::optional<std::uint64_t> positive_integer(const JsonValue *value) {
  if (value == nullptr || value->kind != JsonKind::Number ||
      value->scalar.empty() || value->scalar.front() == '-' ||
      value->scalar.find_first_not_of("0123456789") != std::string::npos) {
    return std::nullopt;
  }
  std::uint64_t result{};
  const auto [end, error] =
      std::from_chars(value->scalar.data(),
                      value->scalar.data() + value->scalar.size(), result);
  if (error != std::errc{} ||
      end != value->scalar.data() + value->scalar.size() || result == 0) {
    return std::nullopt;
  }
  return result;
}

std::optional<std::string_view> string_value(const JsonValue *value) {
  if (value == nullptr || value->kind != JsonKind::String) {
    return std::nullopt;
  }
  return value->scalar;
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

bool integrity_eligible(const CaptureDatasetManifest &manifest,
                        const CaptureDatasetRecord &record,
                        const BybitBookDecodeLimits &limits) {
  return manifest.venue == "bybit" && manifest.record_count != 0 &&
         manifest.adapter_id == "chronos.bybit.public-market-data" &&
         manifest.endpoint == sdk::EndpointClass::PublicMarketData &&
         manifest.data_classification ==
             sdk::DataClassification::PublicMarketData &&
         manifest.dataset_class == "raw_source_capture" &&
         manifest.maximum_retained_payload_bytes != 0 &&
         record.capture_sequence >= manifest.first_capture_sequence &&
         record.capture_sequence <= manifest.last_capture_sequence &&
         record.raw_payload.size() <= manifest.maximum_retained_payload_bytes &&
         record.raw_payload.size() <= limits.maximum_payload_bytes &&
         record.raw_payload.size() == record.original_payload_size &&
         record.payload_digest.coverage ==
             sdk::DigestCoverage::CompletePayload &&
         record.payload_digest ==
             sdk::sha256(record.raw_payload,
                         sdk::DigestCoverage::CompletePayload) &&
         record.framing_protocol == sdk::FramingProtocol::WebSocket &&
         record.frame_kind == sdk::SourceFrameKind::Text &&
         record.framing_status == sdk::FramingStatus::Complete &&
         record.integrity_status == sdk::CaptureIntegrityStatus::Complete &&
         record.parse_status == sdk::ParseStatus::NotAttempted &&
         record.content_encoding == sdk::ContentEncoding::Utf8Text &&
         record.compression_disposition !=
             sdk::CompressionDisposition::CompressedOpaque;
}

bool valid_utf8(std::string_view value) {
  std::size_t index = 0;
  while (index < value.size()) {
    const auto lead = static_cast<unsigned char>(value[index++]);
    if (lead <= 0x7FU) {
      continue;
    }
    std::size_t continuation_count{};
    std::uint32_t code_point{};
    if (lead >= 0xC2U && lead <= 0xDFU) {
      continuation_count = 1;
      code_point = lead & 0x1FU;
    } else if (lead >= 0xE0U && lead <= 0xEFU) {
      continuation_count = 2;
      code_point = lead & 0x0FU;
    } else if (lead >= 0xF0U && lead <= 0xF4U) {
      continuation_count = 3;
      code_point = lead & 0x07U;
    } else {
      return false;
    }
    if (index + continuation_count > value.size()) {
      return false;
    }
    for (std::size_t offset = 0; offset < continuation_count; ++offset) {
      const auto continuation = static_cast<unsigned char>(value[index++]);
      if ((continuation & 0xC0U) != 0x80U) {
        return false;
      }
      code_point = (code_point << 6U) | (continuation & 0x3FU);
    }
    if ((continuation_count == 2 && code_point < 0x800U) ||
        (continuation_count == 3 && code_point < 0x10000U) ||
        code_point > 0x10FFFFU ||
        (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
      return false;
    }
  }
  return true;
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

} // namespace

BybitBookDecodeResult
decode_bybit_v5_book(const CaptureDatasetManifest &manifest,
                     const CaptureDatasetRecord &record,
                     const BybitBookDecodeLimits &limits) {
  if (record.raw_payload.size() > limits.maximum_payload_bytes) {
    return {.failure = BookNormalizationFailure::ResourceLimitExceeded};
  }
  if (!integrity_eligible(manifest, record, limits)) {
    return {.failure = BookNormalizationFailure::IntegrityIneligible};
  }

  const std::string_view payload{
      reinterpret_cast<const char *>(record.raw_payload.data()),
      record.raw_payload.size()};
  if (!valid_utf8(payload)) {
    return {.failure = BookNormalizationFailure::MalformedPayload};
  }
  BoundedJsonParser parser(payload, limits);
  const auto root = parser.parse();
  if (!root.has_value()) {
    return {.failure = parser_failure(parser.failure())};
  }
  if (root->kind != JsonKind::Object) {
    return {.failure = BookNormalizationFailure::SchemaViolation};
  }

  const auto topic = string_value(member(*root, "topic"));
  const auto type = string_value(member(*root, "type"));
  const auto system_timestamp = positive_integer(member(*root, "ts"));
  const auto *data = member(*root, "data");
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
  const auto symbol = string_value(member(*data, "s"));
  if (!topic_parts.has_value() || topic_parts->depth != limits.expected_depth ||
      !symbol.has_value() || topic_parts->symbol != *symbol) {
    return {.failure = BookNormalizationFailure::WrongTopicOrSymbol};
  }

  const auto update_id = positive_integer(member(*data, "u"));
  const auto sequence = positive_integer(member(*data, "seq"));
  const auto matching_timestamp = positive_integer(member(*root, "cts"));
  if (!update_id.has_value() || !sequence.has_value() ||
      !matching_timestamp.has_value()) {
    return {.failure = BookNormalizationFailure::InvalidNumeric};
  }

  book::DecodedBookMessage message{
      .assertions = {
          .kind = kind,
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
  auto failure = parse_levels(member(*data, "b"),
                              limits.maximum_levels_per_side, message.bids);
  if (failure == BookNormalizationFailure::None) {
    failure = parse_levels(member(*data, "a"), limits.maximum_levels_per_side,
                           message.asks);
  }
  if (failure != BookNormalizationFailure::None) {
    return {.failure = failure};
  }

  return {
      .message = std::move(message),
      .source_lineage =
          book::SourceCaptureLineage{
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
      .failure = BookNormalizationFailure::None,
  };
}

} // namespace chronos::adapters::market_data
