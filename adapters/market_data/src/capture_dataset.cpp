#include "chronos/adapters/market_data/capture_dataset.hpp"

#include <array>
#include <charconv>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <unistd.h>

namespace chronos::adapters::market_data {
namespace {
constexpr std::string_view kFormat = "chronos-source-capture-v1";
constexpr std::uint64_t kMaximumRecordBytes = 8U * 1024U * 1024U;
constexpr std::uint64_t kMaximumDatasetBytes = 64U * 1024U * 1024U;
constexpr std::uint64_t kMaximumManifestBytes = 16U * 1024U;
constexpr std::uint64_t kMaximumRecords = 1'000'000U;
constexpr std::array<std::byte, 8> kMagic{
    std::byte{'C'}, std::byte{'H'}, std::byte{'R'}, std::byte{'S'},
    std::byte{'R'}, std::byte{'C'}, std::byte{'0'}, std::byte{'1'}};

template <typename T> void put_unsigned(std::vector<std::byte> &out, T value) {
  for (std::size_t index = 0; index < sizeof(T); ++index)
    out.push_back(static_cast<std::byte>(value >> (index * 8U)));
}

template <typename T>
std::optional<T> take_unsigned(std::span<const std::byte> bytes,
                               std::size_t &offset) {
  if (bytes.size() - std::min(bytes.size(), offset) < sizeof(T))
    return std::nullopt;
  T value{};
  for (std::size_t index = 0; index < sizeof(T); ++index)
    value |= static_cast<T>(std::to_integer<std::uint8_t>(bytes[offset++]))
             << (index * 8U);
  return value;
}

template <typename Id> void put_id(std::vector<std::byte> &out, Id id) {
  for (const auto byte : id.bytes())
    out.push_back(static_cast<std::byte>(byte));
}

template <typename Id>
std::optional<Id> take_id(std::span<const std::byte> bytes,
                          std::size_t &offset) {
  if (bytes.size() - std::min(bytes.size(), offset) < 16)
    return std::nullopt;
  typename Id::bytes_type value{};
  for (auto &byte : value)
    byte = std::to_integer<std::uint8_t>(bytes[offset++]);
  return Id::from_bytes(value);
}

std::string hex(const sdk::PayloadDigest &digest) { return digest.hex(); }

std::optional<std::array<std::uint8_t, 32>> parse_hex(std::string_view value) {
  if (value.size() != 64)
    return std::nullopt;
  std::array<std::uint8_t, 32> result{};
  for (std::size_t index = 0; index < result.size(); ++index) {
    const auto nibble = [](char character) -> std::optional<std::uint8_t> {
      if (character >= '0' && character <= '9')
        return character - '0';
      if (character >= 'a' && character <= 'f')
        return character - 'a' + 10;
      return std::nullopt;
    };
    const auto high = nibble(value[index * 2]);
    const auto low = nibble(value[index * 2 + 1]);
    if (!high || !low)
      return std::nullopt;
    result[index] = static_cast<std::uint8_t>((*high << 4U) | *low);
  }
  return result;
}

std::vector<std::byte> encode_record(const sdk::SourceEvent &event) {
  std::vector<std::byte> out;
  put_id(out, event.source_event_id());
  put_unsigned(out, event.capture_sequence());
  put_unsigned(out, static_cast<std::uint64_t>(
                        event.chronos_receive_time().nanoseconds()));
  put_id(out, event.chronos_receive_time().clock_domain_id());
  put_unsigned(out, static_cast<std::uint8_t>(
                        event.chronos_receive_time().clock_class()));
  put_unsigned(out, event.chronos_receive_time().precision_nanoseconds());
  put_unsigned(out, static_cast<std::uint64_t>(event.original_payload_size()));
  put_unsigned(out, static_cast<std::uint8_t>(event.payload_digest().coverage));
  for (const auto byte : event.payload_digest().bytes)
    out.push_back(static_cast<std::byte>(byte));
  put_unsigned(out, static_cast<std::uint8_t>(event.framing_protocol()));
  put_unsigned(out, static_cast<std::uint8_t>(event.frame_kind()));
  put_unsigned(out, static_cast<std::uint8_t>(event.framing_status()));
  put_unsigned(out, static_cast<std::uint8_t>(event.integrity_status()));
  put_unsigned(out, static_cast<std::uint8_t>(event.parse_status()));
  put_unsigned(out, static_cast<std::uint8_t>(event.content_encoding()));
  put_unsigned(out, static_cast<std::uint8_t>(event.compression_disposition()));
  put_unsigned(out, static_cast<std::uint8_t>(event.fragmented()));
  put_unsigned(out, static_cast<std::uint64_t>(event.raw_payload().size()));
  out.insert(out.end(), event.raw_payload().begin(), event.raw_payload().end());
  return out;
}

std::string optional_id(const auto &value) {
  return value ? value->to_string() : "none";
}

std::string manifest_body(const CaptureDatasetManifest &manifest) {
  std::ostringstream out;
  out << "format=" << manifest.format_version << '\n'
      << "records_sha256=" << manifest.records_sha256 << '\n'
      << "capture_session_id=" << manifest.capture_session_id.to_string()
      << '\n'
      << "capture_partition_id=" << manifest.capture_partition_id.to_string()
      << '\n'
      << "runtime_id=" << manifest.runtime_id.to_string() << '\n'
      << "connection_id=" << optional_id(manifest.connection_id) << '\n'
      << "subscription_id=" << optional_id(manifest.subscription_id) << '\n'
      << "adapter_id=" << manifest.adapter_id << '\n'
      << "adapter_version=" << manifest.adapter_version << '\n'
      << "build_version=" << manifest.build_version << '\n'
      << "venue=" << manifest.venue << '\n'
      << "environment=" << static_cast<unsigned>(manifest.environment) << '\n'
      << "endpoint=" << static_cast<unsigned>(manifest.endpoint) << '\n'
      << "trust_class=" << static_cast<unsigned>(manifest.trust_class) << '\n'
      << "framing_version=" << manifest.framing_version << '\n'
      << "static_configuration_version="
      << manifest.static_configuration_version << '\n'
      << "capability_manifest_version=" << manifest.capability_manifest_version
      << '\n'
      << "schema_policy_version=" << manifest.schema_policy_version << '\n'
      << "data_classification="
      << static_cast<unsigned>(manifest.data_classification) << '\n'
      << "access_restriction="
      << static_cast<unsigned>(manifest.access_restriction) << '\n'
      << "dataset_class=" << manifest.dataset_class << '\n'
      << "replay_admissible="
      << static_cast<unsigned>(manifest.replay_admissible) << '\n'
      << "records_bytes=" << manifest.records_bytes << '\n'
      << "maximum_retained_payload_bytes="
      << manifest.maximum_retained_payload_bytes << '\n'
      << "maximum_source_events=" << manifest.maximum_source_events << '\n'
      << "record_count=" << manifest.record_count << '\n'
      << "first_capture_sequence=" << manifest.first_capture_sequence << '\n'
      << "last_capture_sequence=" << manifest.last_capture_sequence << '\n';
  return out.str();
}

bool safe_manifest_value(std::string_view value) {
  return !value.empty() &&
         value.find_first_of("\r\n=") == std::string_view::npos;
}

class PosixDatasetPersistence final : public DatasetPersistence {
public:
  bool sync_file(const std::filesystem::path &path) override {
    const auto descriptor = open(path.c_str(), O_RDONLY);
    if (descriptor < 0)
      return false;
    const auto result = fsync(descriptor) == 0;
    close(descriptor);
    return result;
  }
  bool sync_directory(const std::filesystem::path &path) override {
    const auto descriptor = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (descriptor < 0)
      return false;
    const auto result = fsync(descriptor) == 0;
    close(descriptor);
    return result;
  }
  bool publish_directory(const std::filesystem::path &staging,
                         const std::filesystem::path &destination) override {
    std::error_code error;
    std::filesystem::rename(staging, destination, error);
    return !error;
  }
};

DatasetPersistence &default_persistence() {
  static PosixDatasetPersistence persistence;
  return persistence;
}
} // namespace

struct CaptureDatasetWriter::State final {
  State(sdk::SourceCaptureContext value, DatasetPersistence &persistence_value)
      : context(std::move(value)), persistence(persistence_value) {}

  std::filesystem::path destination;
  std::filesystem::path partial;
  sdk::SourceCaptureContext context;
  DatasetPersistence &persistence;
  std::ofstream records;
  std::uint64_t count{};
  std::uint64_t first{};
  std::uint64_t last{};
  bool sealed{};
};

CaptureDatasetWriter::CaptureDatasetWriter(std::unique_ptr<State> state)
    : state_(std::move(state)) {}
CaptureDatasetWriter::CaptureDatasetWriter(CaptureDatasetWriter &&) noexcept =
    default;
CaptureDatasetWriter &
CaptureDatasetWriter::operator=(CaptureDatasetWriter &&) noexcept = default;
CaptureDatasetWriter::~CaptureDatasetWriter() {
  if (state_ && !state_->sealed) {
    state_->records.close();
    std::error_code error;
    std::filesystem::remove_all(state_->partial, error);
  }
}

std::optional<CaptureDatasetWriter>
CaptureDatasetWriter::create(std::filesystem::path destination,
                             const sdk::SourceCaptureContext &context,
                             DatasetPersistence *persistence) {
  if (destination.empty() || std::filesystem::exists(destination) ||
      !safe_manifest_value(context.adapter_id) ||
      !safe_manifest_value(context.adapter_version) ||
      !safe_manifest_value(context.build_version) ||
      !safe_manifest_value(context.venue) ||
      !safe_manifest_value(context.static_configuration_version) ||
      !safe_manifest_value(context.capability_manifest_version) ||
      !safe_manifest_value(context.schema_policy_version) ||
      context.maximum_retained_payload_bytes == 0 ||
      context.maximum_retained_payload_bytes > kMaximumRecordBytes ||
      context.maximum_source_events == 0 ||
      context.maximum_source_events > kMaximumRecords)
    return std::nullopt;
  auto state = std::make_unique<State>(
      context, persistence ? *persistence : default_persistence());
  state->destination = std::move(destination);
  state->partial = state->destination;
  state->partial += ".partial";
  if (std::filesystem::exists(state->partial))
    return std::nullopt;
  std::error_code error;
  std::filesystem::create_directories(state->partial, error);
  if (error)
    return std::nullopt;
  state->records.open(state->partial / "records.bin", std::ios::binary);
  if (!state->records)
    return std::nullopt;
  state->records.write(reinterpret_cast<const char *>(kMagic.data()),
                       static_cast<std::streamsize>(kMagic.size()));
  return CaptureDatasetWriter(std::move(state));
}

DatasetFailure CaptureDatasetWriter::append(const sdk::SourceEvent &event) {
  if (!state_ || state_->sealed)
    return DatasetFailure::AlreadySealed;
  if (event.capture_session_id() != state_->context.capture_session_id ||
      event.capture_partition_id() != state_->context.capture_partition_id ||
      event.context().adapter_id != state_->context.adapter_id ||
      event.context().adapter_version != state_->context.adapter_version ||
      event.context().build_version != state_->context.build_version ||
      event.context().venue != state_->context.venue ||
      event.context().environment != state_->context.environment ||
      event.context().endpoint != state_->context.endpoint ||
      event.context().trust_class != state_->context.trust_class ||
      event.runtime_id() != state_->context.runtime_id ||
      event.connection_id() != state_->context.connection_id ||
      event.subscription_id() != state_->context.subscription_id ||
      event.context().framing_version != state_->context.framing_version ||
      event.context().static_configuration_version !=
          state_->context.static_configuration_version ||
      event.context().capability_manifest_version !=
          state_->context.capability_manifest_version ||
      event.context().schema_policy_version !=
          state_->context.schema_policy_version ||
      event.context().data_classification !=
          state_->context.data_classification ||
      event.context().access_restriction !=
          state_->context.access_restriction ||
      event.context().maximum_retained_payload_bytes !=
          state_->context.maximum_retained_payload_bytes ||
      event.context().maximum_source_events !=
          state_->context.maximum_source_events ||
      event.capture_sequence() != state_->last + 1)
    return DatasetFailure::SequenceMismatch;
  auto record = encode_record(event);
  if (record.size() > kMaximumRecordBytes ||
      record.size() > std::numeric_limits<std::uint32_t>::max())
    return DatasetFailure::InvalidRecord;
  std::vector<std::byte> framed;
  put_unsigned(framed, static_cast<std::uint32_t>(record.size()));
  framed.insert(framed.end(), record.begin(), record.end());
  state_->records.write(reinterpret_cast<const char *>(framed.data()),
                        static_cast<std::streamsize>(framed.size()));
  if (!state_->records)
    return DatasetFailure::Io;
  if (state_->count == 0)
    state_->first = event.capture_sequence();
  state_->last = event.capture_sequence();
  ++state_->count;
  return DatasetFailure::None;
}

DatasetSealResult CaptureDatasetWriter::seal() {
  if (!state_ || state_->sealed)
    return {.failure = DatasetFailure::AlreadySealed};
  state_->records.flush();
  state_->records.close();
  if (state_->count == 0 || !state_->records)
    return {.failure = state_->count == 0 ? DatasetFailure::InvalidRecord
                                          : DatasetFailure::Io};
  std::error_code size_error;
  const auto records_size =
      std::filesystem::file_size(state_->partial / "records.bin", size_error);
  if (size_error || records_size > kMaximumDatasetBytes ||
      state_->count > kMaximumRecords)
    return {.failure = DatasetFailure::InvalidRecord};
  if (!state_->persistence.sync_file(state_->partial / "records.bin"))
    return {.failure = DatasetFailure::Io};
  std::ifstream records_input(state_->partial / "records.bin",
                              std::ios::binary);
  std::vector<char> record_chars(
      (std::istreambuf_iterator<char>(records_input)), {});
  if (records_input.bad())
    return {.failure = DatasetFailure::Io};
  std::vector<std::byte> record_bytes(record_chars.size());
  for (std::size_t index = 0; index < record_chars.size(); ++index)
    record_bytes[index] =
        static_cast<std::byte>(static_cast<unsigned char>(record_chars[index]));
  const auto records_digest =
      sdk::sha256(record_bytes, sdk::DigestCoverage::CompletePayload);
  CaptureDatasetManifest manifest{
      .format_version = std::string(kFormat),
      .records_sha256 = hex(records_digest),
      .capture_session_id = state_->context.capture_session_id,
      .capture_partition_id = state_->context.capture_partition_id,
      .runtime_id = state_->context.runtime_id,
      .connection_id = state_->context.connection_id,
      .subscription_id = state_->context.subscription_id,
      .adapter_id = state_->context.adapter_id,
      .adapter_version = state_->context.adapter_version,
      .build_version = state_->context.build_version,
      .venue = state_->context.venue,
      .environment = state_->context.environment,
      .endpoint = state_->context.endpoint,
      .trust_class = state_->context.trust_class,
      .framing_version = state_->context.framing_version,
      .static_configuration_version =
          state_->context.static_configuration_version,
      .capability_manifest_version =
          state_->context.capability_manifest_version,
      .schema_policy_version = state_->context.schema_policy_version,
      .data_classification = state_->context.data_classification,
      .access_restriction = state_->context.access_restriction,
      .dataset_class = "raw_source_capture",
      .replay_admissible = false,
      .records_bytes = records_size,
      .maximum_retained_payload_bytes =
          state_->context.maximum_retained_payload_bytes,
      .maximum_source_events = state_->context.maximum_source_events,
      .record_count = state_->count,
      .first_capture_sequence = state_->first,
      .last_capture_sequence = state_->last};
  const auto body = manifest_body(manifest);
  const auto body_bytes = std::span<const std::byte>(
      reinterpret_cast<const std::byte *>(body.data()), body.size());
  manifest.dataset_id =
      sdk::sha256(body_bytes, sdk::DigestCoverage::CompletePayload).hex();
  std::ofstream manifest_file(state_->partial / "manifest.txt",
                              std::ios::binary);
  manifest_file << "dataset_id=" << manifest.dataset_id << '\n' << body;
  manifest_file.flush();
  manifest_file.close();
  if (!manifest_file)
    return {.failure = DatasetFailure::Io};
  if (!state_->persistence.sync_file(state_->partial / "manifest.txt") ||
      !state_->persistence.sync_directory(state_->partial) ||
      !state_->persistence.publish_directory(state_->partial,
                                             state_->destination))
    return {.failure = DatasetFailure::Io};
  auto parent = state_->destination.parent_path();
  if (parent.empty())
    parent = ".";
  if (!state_->persistence.sync_directory(parent)) {
    state_->sealed = true;
    return {.manifest = std::move(manifest),
            .failure = DatasetFailure::PublicationUnconfirmed};
  }
  state_->sealed = true;
  return {.manifest = std::move(manifest)};
}

DatasetReadResult read_capture_dataset(const std::filesystem::path &directory) {
  DatasetReadResult result;
  std::error_code manifest_size_error;
  const auto manifest_size = std::filesystem::file_size(
      directory / "manifest.txt", manifest_size_error);
  if (manifest_size_error || manifest_size > kMaximumManifestBytes)
    return {.failure = DatasetFailure::InvalidManifest};
  std::ifstream manifest_file(directory / "manifest.txt", std::ios::binary);
  std::map<std::string, std::string> values;
  std::string line;
  while (std::getline(manifest_file, line)) {
    const auto separator = line.find('=');
    if (separator == std::string::npos || separator == 0 ||
        !values.emplace(line.substr(0, separator), line.substr(separator + 1))
             .second)
      return {.failure = DatasetFailure::InvalidManifest};
  }
  const std::array required{"dataset_id",
                            "format",
                            "records_sha256",
                            "capture_session_id",
                            "capture_partition_id",
                            "runtime_id",
                            "connection_id",
                            "subscription_id",
                            "adapter_id",
                            "adapter_version",
                            "build_version",
                            "venue",
                            "environment",
                            "endpoint",
                            "trust_class",
                            "framing_version",
                            "static_configuration_version",
                            "capability_manifest_version",
                            "schema_policy_version",
                            "data_classification",
                            "access_restriction",
                            "dataset_class",
                            "replay_admissible",
                            "records_bytes",
                            "maximum_retained_payload_bytes",
                            "maximum_source_events",
                            "record_count",
                            "first_capture_sequence",
                            "last_capture_sequence"};
  if (!manifest_file.eof() || values.size() != required.size())
    return {.failure = DatasetFailure::InvalidManifest};
  for (const auto *key : required)
    if (!values.contains(key))
      return {.failure = DatasetFailure::InvalidManifest};
  const auto session =
      sdk::CaptureSessionId::parse(values["capture_session_id"]);
  const auto partition =
      sdk::CapturePartitionId::parse(values["capture_partition_id"]);
  const auto runtime = contracts::RuntimeId::parse(values["runtime_id"]);
  const auto parse_u64 =
      [&values](const char *key) -> std::optional<std::uint64_t> {
    std::uint64_t value{};
    const auto &text = values[key];
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
               ? std::optional(value)
               : std::nullopt;
  };
  const auto count = parse_u64("record_count");
  const auto first = parse_u64("first_capture_sequence");
  const auto last = parse_u64("last_capture_sequence");
  const auto environment = parse_u64("environment");
  const auto endpoint = parse_u64("endpoint");
  const auto trust_class = parse_u64("trust_class");
  const auto classification = parse_u64("data_classification");
  const auto restriction = parse_u64("access_restriction");
  const auto replay_admissible = parse_u64("replay_admissible");
  const auto records_bytes = parse_u64("records_bytes");
  const auto maximum_payload = parse_u64("maximum_retained_payload_bytes");
  const auto maximum_events = parse_u64("maximum_source_events");
  const auto parse_optional_connection =
      [&values]() -> std::optional<std::optional<sdk::SourceConnectionId>> {
    if (values["connection_id"] == "none")
      return std::optional<sdk::SourceConnectionId>{};
    const auto value = sdk::SourceConnectionId::parse(values["connection_id"]);
    return value ? std::optional<std::optional<sdk::SourceConnectionId>>(*value)
                 : std::nullopt;
  };
  const auto parse_optional_subscription =
      [&values]() -> std::optional<std::optional<sdk::SourceSubscriptionId>> {
    if (values["subscription_id"] == "none")
      return std::optional<sdk::SourceSubscriptionId>{};
    const auto value =
        sdk::SourceSubscriptionId::parse(values["subscription_id"]);
    return value
               ? std::optional<std::optional<sdk::SourceSubscriptionId>>(*value)
               : std::nullopt;
  };
  const auto connection = parse_optional_connection();
  const auto subscription = parse_optional_subscription();
  if (!session || !partition || !runtime || !connection || !subscription ||
      !count || !first || !last || !environment || !endpoint || !trust_class ||
      !classification || !restriction || !replay_admissible || !records_bytes ||
      !maximum_payload || !maximum_events || *maximum_payload == 0 ||
      *maximum_payload > kMaximumRecordBytes || *maximum_events == 0 ||
      *maximum_events > kMaximumRecords || *count == 0 ||
      *count > kMaximumRecords ||
      *environment >
          static_cast<std::uint64_t>(sdk::EnvironmentClass::Production) ||
      *endpoint >
          static_cast<std::uint64_t>(sdk::EndpointClass::PublicMarketData) ||
      *trust_class > static_cast<std::uint64_t>(
                         sdk::SourceTrustClass::PublicUnauthenticated) ||
      *classification > static_cast<std::uint64_t>(
                            sdk::DataClassification::PublicMarketData) ||
      *restriction >
          static_cast<std::uint64_t>(sdk::AccessRestriction::ChronosInternal) ||
      values["format"] != kFormat || !parse_hex(values["dataset_id"]) ||
      !parse_hex(values["records_sha256"]) ||
      values["dataset_class"] != "raw_source_capture" ||
      *replay_admissible != 0)
    return {.failure = DatasetFailure::InvalidManifest};
  CaptureDatasetManifest manifest{
      .format_version = values["format"],
      .dataset_id = values["dataset_id"],
      .records_sha256 = values["records_sha256"],
      .capture_session_id = *session,
      .capture_partition_id = *partition,
      .runtime_id = *runtime,
      .connection_id = *connection,
      .subscription_id = *subscription,
      .adapter_id = values["adapter_id"],
      .adapter_version = values["adapter_version"],
      .build_version = values["build_version"],
      .venue = values["venue"],
      .environment = static_cast<sdk::EnvironmentClass>(*environment),
      .endpoint = static_cast<sdk::EndpointClass>(*endpoint),
      .trust_class = static_cast<sdk::SourceTrustClass>(*trust_class),
      .framing_version = values["framing_version"],
      .static_configuration_version = values["static_configuration_version"],
      .capability_manifest_version = values["capability_manifest_version"],
      .schema_policy_version = values["schema_policy_version"],
      .data_classification =
          static_cast<sdk::DataClassification>(*classification),
      .access_restriction = static_cast<sdk::AccessRestriction>(*restriction),
      .dataset_class = values["dataset_class"],
      .replay_admissible = false,
      .records_bytes = *records_bytes,
      .maximum_retained_payload_bytes = *maximum_payload,
      .maximum_source_events = *maximum_events,
      .record_count = *count,
      .first_capture_sequence = *first,
      .last_capture_sequence = *last};
  const auto canonical_manifest = manifest_body(manifest);
  if (sdk::sha256(
          std::span<const std::byte>(
              reinterpret_cast<const std::byte *>(canonical_manifest.data()),
              canonical_manifest.size()),
          sdk::DigestCoverage::CompletePayload)
          .hex() != manifest.dataset_id)
    return {.failure = DatasetFailure::IntegrityMismatch};
  std::ifstream records_file(directory / "records.bin", std::ios::binary);
  std::error_code size_error;
  const auto file_size =
      std::filesystem::file_size(directory / "records.bin", size_error);
  if (size_error || file_size > kMaximumDatasetBytes ||
      file_size != manifest.records_bytes)
    return {.failure = DatasetFailure::InvalidRecord};
  std::vector<char> chars((std::istreambuf_iterator<char>(records_file)), {});
  if (records_file.bad())
    return {.failure = DatasetFailure::Io};
  std::vector<std::byte> bytes(chars.size());
  for (std::size_t i = 0; i < chars.size(); ++i)
    bytes[i] = static_cast<std::byte>(static_cast<unsigned char>(chars[i]));
  if (sdk::sha256(bytes, sdk::DigestCoverage::CompletePayload).hex() !=
      manifest.records_sha256)
    return {.failure = DatasetFailure::IntegrityMismatch};
  if (bytes.size() < kMagic.size() ||
      !std::equal(kMagic.begin(), kMagic.end(), bytes.begin()))
    return {.failure = DatasetFailure::InvalidRecord};
  std::size_t offset = kMagic.size();
  while (offset < bytes.size()) {
    const auto size = take_unsigned<std::uint32_t>(bytes, offset);
    if (!size || *size > kMaximumRecordBytes ||
        bytes.size() - std::min(bytes.size(), offset) < *size)
      return {.failure = DatasetFailure::InvalidRecord};
    const auto record_bytes =
        std::span<const std::byte>(bytes).subspan(offset, *size);
    offset += *size;
    std::size_t cursor{};
    const auto event_id =
        take_id<contracts::SourceEventId>(record_bytes, cursor);
    const auto sequence = take_unsigned<std::uint64_t>(record_bytes, cursor);
    const auto time_bits = take_unsigned<std::uint64_t>(record_bytes, cursor);
    const auto clock_id =
        take_id<contracts::ClockDomainId>(record_bytes, cursor);
    const auto clock_class = take_unsigned<std::uint8_t>(record_bytes, cursor);
    const auto precision = take_unsigned<std::uint32_t>(record_bytes, cursor);
    const auto original_size =
        take_unsigned<std::uint64_t>(record_bytes, cursor);
    const auto coverage = take_unsigned<std::uint8_t>(record_bytes, cursor);
    if (!event_id || !sequence || !time_bits || !clock_id || !clock_class ||
        !precision || !original_size || !coverage ||
        record_bytes.size() - std::min(record_bytes.size(), cursor) < 32)
      return {.failure = DatasetFailure::InvalidRecord};
    sdk::PayloadDigest digest{.coverage =
                                  static_cast<sdk::DigestCoverage>(*coverage)};
    for (auto &byte : digest.bytes)
      byte = std::to_integer<std::uint8_t>(record_bytes[cursor++]);
    const auto framing_protocol =
        take_unsigned<std::uint8_t>(record_bytes, cursor);
    const auto frame_kind = take_unsigned<std::uint8_t>(record_bytes, cursor);
    const auto framing_status =
        take_unsigned<std::uint8_t>(record_bytes, cursor);
    const auto integrity = take_unsigned<std::uint8_t>(record_bytes, cursor);
    const auto parse_status = take_unsigned<std::uint8_t>(record_bytes, cursor);
    const auto encoding = take_unsigned<std::uint8_t>(record_bytes, cursor);
    const auto compression = take_unsigned<std::uint8_t>(record_bytes, cursor);
    const auto fragmented = take_unsigned<std::uint8_t>(record_bytes, cursor);
    const auto payload_size =
        take_unsigned<std::uint64_t>(record_bytes, cursor);
    if (!framing_protocol ||
        *framing_protocol >
            static_cast<std::uint8_t>(sdk::FramingProtocol::WebSocket) ||
        !frame_kind ||
        *frame_kind >
            static_cast<std::uint8_t>(sdk::SourceFrameKind::Unknown) ||
        !framing_status ||
        *framing_status >
            static_cast<std::uint8_t>(sdk::FramingStatus::Unsupported) ||
        !integrity ||
        *integrity > static_cast<std::uint8_t>(
                         sdk::CaptureIntegrityStatus::ResourceLimitExceeded) ||
        !parse_status ||
        *parse_status >
            static_cast<std::uint8_t>(sdk::ParseStatus::Unavailable) ||
        !encoding ||
        *encoding >
            static_cast<std::uint8_t>(sdk::ContentEncoding::OpaqueBinary) ||
        !compression ||
        *compression > static_cast<std::uint8_t>(
                           sdk::CompressionDisposition::CompressedOpaque) ||
        !fragmented || *fragmented > 1 ||
        *coverage >
            static_cast<std::uint8_t>(sdk::DigestCoverage::RetainedPrefix) ||
        !payload_size ||
        *payload_size >
            record_bytes.size() - std::min(record_bytes.size(), cursor) ||
        cursor + *payload_size != record_bytes.size())
      return {.failure = DatasetFailure::InvalidRecord};
    const auto time = contracts::TimePoint::from(
        static_cast<std::int64_t>(*time_bits), *clock_id,
        static_cast<contracts::ClockClass>(*clock_class), *precision);
    if (!time)
      return {.failure = DatasetFailure::InvalidRecord};
    std::vector<std::byte> payload(record_bytes.begin() +
                                       static_cast<std::ptrdiff_t>(cursor),
                                   record_bytes.end());
    if (*original_size < payload.size() ||
        (digest.coverage == sdk::DigestCoverage::CompletePayload &&
         *original_size != payload.size()) ||
        sdk::sha256(payload, digest.coverage) != digest)
      return {.failure = DatasetFailure::IntegrityMismatch};
    result.records.push_back(
        {.source_event_id = *event_id,
         .capture_sequence = *sequence,
         .chronos_receive_time = *time,
         .raw_payload = std::move(payload),
         .original_payload_size = *original_size,
         .payload_digest = digest,
         .framing_protocol =
             static_cast<sdk::FramingProtocol>(*framing_protocol),
         .frame_kind = static_cast<sdk::SourceFrameKind>(*frame_kind),
         .framing_status = static_cast<sdk::FramingStatus>(*framing_status),
         .integrity_status =
             static_cast<sdk::CaptureIntegrityStatus>(*integrity),
         .parse_status = static_cast<sdk::ParseStatus>(*parse_status),
         .content_encoding = static_cast<sdk::ContentEncoding>(*encoding),
         .compression_disposition =
             static_cast<sdk::CompressionDisposition>(*compression),
         .fragmented = *fragmented != 0});
  }
  if (result.records.size() != manifest.record_count ||
      result.records.front().capture_sequence !=
          manifest.first_capture_sequence ||
      result.records.back().capture_sequence != manifest.last_capture_sequence)
    return {.failure = DatasetFailure::SequenceMismatch};
  for (std::size_t index = 0; index < result.records.size(); ++index)
    if (result.records[index].capture_sequence !=
        manifest.first_capture_sequence + index)
      return {.failure = DatasetFailure::SequenceMismatch};
  result.manifest = std::move(manifest);
  return result;
}

} // namespace chronos::adapters::market_data
