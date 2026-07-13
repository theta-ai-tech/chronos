#include "chronos/adapters/market_data/bybit_book_decoder.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <string>
#include <string_view>
#include <utility>

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
  std::string key;
  std::string scalar;
  std::vector<JsonValue> array;
  std::vector<JsonValue> object;
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
      for (const auto &existing : result.object) {
        if (existing.key == *key) {
          failure_ = JsonFailure::DuplicateMember;
          return std::nullopt;
        }
      }
      auto value = parse_value(depth + 1);
      if (!value.has_value()) {
        return std::nullopt;
      }
      value->key = std::move(*key);
      result.object.push_back(std::move(*value));
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
  for (const auto &value : object.object) {
    if (value.key == key) {
      return &value;
    }
  }
  return nullptr;
}

void append_json_string(std::string_view value, std::string &output) {
  constexpr std::array<char, 16> hex{'0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  output.push_back('"');
  for (const auto character : value) {
    const auto byte = static_cast<unsigned char>(character);
    switch (character) {
    case '"':
      output += "\\\"";
      break;
    case '\\':
      output += "\\\\";
      break;
    case '\b':
      output += "\\b";
      break;
    case '\f':
      output += "\\f";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      if (byte < 0x20U) {
        output += "\\u00";
        output.push_back(hex[byte >> 4U]);
        output.push_back(hex[byte & 0x0FU]);
      } else {
        output.push_back(character);
      }
      break;
    }
  }
  output.push_back('"');
}

void append_canonical_json(const JsonValue &value, std::string &output) {
  switch (value.kind) {
  case JsonKind::Null:
  case JsonKind::Boolean:
  case JsonKind::Number:
    output += value.scalar;
    return;
  case JsonKind::String:
    append_json_string(value.scalar, output);
    return;
  case JsonKind::Array:
    output.push_back('[');
    for (std::size_t index = 0; index < value.array.size(); ++index) {
      if (index != 0) {
        output.push_back(',');
      }
      append_canonical_json(value.array[index], output);
    }
    output.push_back(']');
    return;
  case JsonKind::Object: {
    std::vector<const JsonValue *> members;
    members.reserve(value.object.size());
    for (const auto &child : value.object) {
      members.push_back(&child);
    }
    std::sort(members.begin(), members.end(),
              [](const auto *left, const auto *right) {
                return left->key < right->key;
              });
    output.push_back('{');
    for (std::size_t index = 0; index < members.size(); ++index) {
      if (index != 0) {
        output.push_back(',');
      }
      append_json_string(members[index]->key, output);
      output.push_back(':');
      append_canonical_json(*members[index], output);
    }
    output.push_back('}');
    return;
  }
  }
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
      append_canonical_json(value, extension.canonical_json);
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
  return manifest.format_version == "chronos-source-capture-v2" &&
         manifest.venue == "bybit" && manifest.record_count != 0 &&
         manifest.adapter_id == "chronos.bybit.public-market-data" &&
         manifest.schema_policy_version == "bybit-v5-public-v1" &&
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
