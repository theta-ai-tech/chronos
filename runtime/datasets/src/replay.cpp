#include "chronos/runtime/datasets/replay.hpp"

#include <algorithm>
#include <utility>

namespace chronos::runtime::datasets {
namespace {

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

void append_digest(std::vector<std::byte> &output,
                   const contracts::Sha256Digest &digest) {
  for (const auto byte : digest.bytes)
    output.push_back(static_cast<std::byte>(byte));
}

void append_version(std::vector<std::byte> &output,
                    const contracts::VersionRef &version) {
  const auto definition_id = version.definition_id();
  append_id(output, definition_id);
  append_u64(output, version.version());
}

bool valid_version(std::string_view value) {
  return !value.empty() && value.size() <= 64 &&
         std::all_of(value.begin(), value.end(), [](char character) {
           return (character >= 'a' && character <= 'z') ||
                  (character >= 'A' && character <= 'Z') ||
                  (character >= '0' && character <= '9') || character == '.' ||
                  character == '-' || character == '_';
         });
}

bool known(ReplayClass value) {
  return value == ReplayClass::FaithfulCaptureOrder ||
         value == ReplayClass::NormalizedFact;
}

bool nonzero(const contracts::Sha256Digest &digest) {
  return std::any_of(digest.bytes.begin(), digest.bytes.end(),
                     [](std::uint8_t byte) { return byte != 0; });
}

std::optional<std::uint8_t> hex_nibble(char character) {
  if (character >= '0' && character <= '9')
    return static_cast<std::uint8_t>(character - '0');
  if (character >= 'a' && character <= 'f')
    return static_cast<std::uint8_t>(character - 'a' + 10);
  return std::nullopt;
}

std::optional<contracts::Sha256Digest> parse_digest(std::string_view value) {
  if (value.size() != 64)
    return std::nullopt;
  contracts::Sha256Digest digest;
  for (std::size_t index = 0; index < digest.bytes.size(); ++index) {
    const auto high = hex_nibble(value[index * 2]);
    const auto low = hex_nibble(value[index * 2 + 1]);
    if (!high.has_value() || !low.has_value())
      return std::nullopt;
    digest.bytes[index] = static_cast<std::uint8_t>((*high << 4U) | *low);
  }
  return digest;
}

contracts::Sha256Digest
manifest_identity(contracts::RunId run_id, ReplayClass replay_class,
                  const contracts::Sha256Digest &dataset_identity,
                  const ReplayVersionPins &pins) {
  std::vector<std::byte> canonical;
  append_string(canonical, "chronos-replay-run-manifest-v1");
  append_id(canonical, run_id);
  append_u64(canonical, static_cast<std::uint64_t>(replay_class));
  append_digest(canonical, dataset_identity);
  append_string(canonical, pins.provider_version);
  append_string(canonical, pins.merge_policy_version);
  append_string(canonical, pins.schema_registry_version);
  append_string(canonical, pins.canonicalization_version);
  append_u64(canonical, pins.normalizer_version.has_value() ? 1U : 0U);
  if (pins.normalizer_version.has_value())
    append_string(canonical, *pins.normalizer_version);
  append_u64(canonical, pins.reference_lineage_version.has_value() ? 1U : 0U);
  if (pins.reference_lineage_version.has_value())
    append_version(canonical, *pins.reference_lineage_version);
  append_u64(canonical,
             pins.expected_normalized_dataset_identity.has_value() ? 1U : 0U);
  if (pins.expected_normalized_dataset_identity.has_value())
    append_digest(canonical, *pins.expected_normalized_dataset_identity);
  return contracts::sha256(canonical);
}

contracts::Sha256Digest
normalized_dataset_identity(const std::vector<NormalizedFactRecord> &records) {
  std::vector<std::byte> canonical;
  append_string(canonical, "chronos-normalized-fact-dataset-v1");
  append_u64(canonical, static_cast<std::uint64_t>(records.size()));
  for (const auto &record : records) {
    append_u64(canonical, record.normalized_position);
    append_id(canonical, record.normalized_stream_id);
    append_u64(canonical, record.normalized_stream_epoch);
    append_digest(canonical, record.source_dataset_identity);
    append_id(canonical, record.source_event_id);
    append_id(canonical, record.source_decode_enrichment_id);
    append_id(canonical, record.acceptance_evidence_id);
    append_string(canonical, record.normalizer_version);
    append_version(canonical, record.reference_lineage_version);
    append_string(canonical, record.event_type);
    append_u64(canonical,
               static_cast<std::uint64_t>(record.semantic_payload.size()));
    canonical.insert(canonical.end(), record.semantic_payload.begin(),
                     record.semantic_payload.end());
    append_digest(canonical, record.semantic_checksum);
  }
  return contracts::sha256(canonical);
}

contracts::Sha256Digest
source_checksum(const adapters::sdk::PayloadDigest &digest) {
  return {.bytes = digest.bytes};
}

} // namespace

ReplayRunManifest::ReplayRunManifest(contracts::Sha256Digest identity,
                                     contracts::RunId run_id,
                                     ReplayClass replay_class,
                                     contracts::Sha256Digest dataset_identity,
                                     ReplayVersionPins pins)
    : identity_(identity), run_id_(run_id), replay_class_(replay_class),
      dataset_identity_(dataset_identity), pins_(std::move(pins)) {}

std::optional<ReplayRunManifest>
ReplayRunManifest::create(contracts::RunId run_id, ReplayClass replay_class,
                          contracts::Sha256Digest dataset_identity,
                          ReplayVersionPins pins) {
  if (!known(replay_class) || !nonzero(dataset_identity) ||
      !valid_version(pins.provider_version) ||
      !valid_version(pins.merge_policy_version) ||
      !valid_version(pins.schema_registry_version) ||
      !valid_version(pins.canonicalization_version)) {
    return std::nullopt;
  }
  const auto faithful = replay_class == ReplayClass::FaithfulCaptureOrder;
  if (faithful != pins.normalizer_version.has_value() ||
      faithful != pins.reference_lineage_version.has_value() ||
      faithful != pins.expected_normalized_dataset_identity.has_value() ||
      (pins.normalizer_version.has_value() &&
       !valid_version(*pins.normalizer_version)) ||
      (pins.expected_normalized_dataset_identity.has_value() &&
       !nonzero(*pins.expected_normalized_dataset_identity))) {
    return std::nullopt;
  }
  const auto identity =
      manifest_identity(run_id, replay_class, dataset_identity, pins);
  return ReplayRunManifest(identity, run_id, replay_class, dataset_identity,
                           std::move(pins));
}

const contracts::Sha256Digest &ReplayRunManifest::identity() const noexcept {
  return identity_;
}

contracts::RunId ReplayRunManifest::run_id() const noexcept { return run_id_; }

ReplayClass ReplayRunManifest::replay_class() const noexcept {
  return replay_class_;
}

const contracts::Sha256Digest &
ReplayRunManifest::dataset_identity() const noexcept {
  return dataset_identity_;
}

const ReplayVersionPins &ReplayRunManifest::pins() const noexcept {
  return pins_;
}

NormalizedFactDataset::NormalizedFactDataset(
    contracts::Sha256Digest identity, std::vector<NormalizedFactRecord> records)
    : identity_(identity), records_(std::move(records)) {}

std::optional<NormalizedFactDataset>
NormalizedFactDataset::create(std::vector<NormalizedFactRecord> records,
                              const NormalizedFactDatasetLimits &limits) {
  if (records.empty() || records.size() > limits.maximum_records ||
      limits.maximum_records == 0 || limits.maximum_payload_bytes == 0 ||
      limits.maximum_total_payload_bytes == 0)
    return std::nullopt;
  std::size_t total_payload_bytes{};
  for (std::size_t index = 0; index < records.size(); ++index) {
    const auto &record = records[index];
    if (record.normalized_position != index + 1 ||
        record.normalized_stream_epoch == 0 ||
        !nonzero(record.source_dataset_identity) ||
        !valid_version(record.normalizer_version) ||
        !valid_version(record.event_type) || record.semantic_payload.empty() ||
        record.semantic_payload.size() > limits.maximum_payload_bytes ||
        record.semantic_payload.size() >
            limits.maximum_total_payload_bytes - total_payload_bytes ||
        record.semantic_checksum !=
            contracts::sha256(record.semantic_payload)) {
      return std::nullopt;
    }
    total_payload_bytes += record.semantic_payload.size();
  }
  const auto identity = normalized_dataset_identity(records);
  return NormalizedFactDataset(identity, std::move(records));
}

const contracts::Sha256Digest &
NormalizedFactDataset::identity() const noexcept {
  return identity_;
}

const std::vector<NormalizedFactRecord> &
NormalizedFactDataset::records() const noexcept {
  return records_;
}

ReplayResult
replay_capture_order(const ReplayRunManifest &manifest,
                     const adapters::market_data::DatasetReadResult &dataset,
                     ReplayDispatchSink &sink) {
  if (manifest.replay_class() != ReplayClass::FaithfulCaptureOrder)
    return {.failure = ReplayFailure::InvalidManifest};
  if (!dataset.ok())
    return {.failure = ReplayFailure::DatasetIneligible};
  const auto identity = parse_digest(dataset.manifest()->dataset_id);
  if (!identity.has_value() || *identity != manifest.dataset_identity())
    return {.failure = ReplayFailure::DatasetMismatch};

  ReplayResult result;
  for (const auto &record : dataset.records()) {
    const ReplayDispatchInput input{
        .replay_class = ReplayClass::FaithfulCaptureOrder,
        .replay_ordinal = result.dispatched_count + 1,
        .event_type = "source.capture",
        .semantic_payload = record.raw_payload,
        .semantic_checksum = source_checksum(record.payload_digest),
        .source_event_id = record.source_event_id,
        .capture_sequence = record.capture_sequence,
        .source_dataset_identity = manifest.dataset_identity(),
    };
    if (!sink.accept(input))
      return {.dispatched_count = result.dispatched_count,
              .failure = ReplayFailure::DispatchRejected};
    ++result.dispatched_count;
  }
  const auto reconstructed = sink.completed_normalized_dataset_identity();
  if (!reconstructed.has_value() ||
      *reconstructed != *manifest.pins().expected_normalized_dataset_identity) {
    return {.dispatched_count = result.dispatched_count,
            .failure = ReplayFailure::SemanticMismatch};
  }
  return result;
}

ReplayResult replay_normalized_facts(const ReplayRunManifest &manifest,
                                     const NormalizedFactDataset &dataset,
                                     ReplayDispatchSink &sink) {
  if (manifest.replay_class() != ReplayClass::NormalizedFact)
    return {.failure = ReplayFailure::InvalidManifest};
  if (dataset.identity() != manifest.dataset_identity())
    return {.failure = ReplayFailure::DatasetMismatch};

  ReplayResult result;
  for (const auto &record : dataset.records()) {
    const ReplayDispatchInput input{
        .replay_class = ReplayClass::NormalizedFact,
        .replay_ordinal = result.dispatched_count + 1,
        .event_type = record.event_type,
        .semantic_payload = record.semantic_payload,
        .semantic_checksum = record.semantic_checksum,
        .source_event_id = record.source_event_id,
        .normalized_position = record.normalized_position,
        .normalized_stream_id = record.normalized_stream_id,
        .normalized_stream_epoch = record.normalized_stream_epoch,
        .source_dataset_identity = record.source_dataset_identity,
        .source_decode_enrichment_id = record.source_decode_enrichment_id,
        .acceptance_evidence_id = record.acceptance_evidence_id,
        .normalizer_version = record.normalizer_version,
        .reference_lineage_version = record.reference_lineage_version,
    };
    if (!sink.accept(input))
      return {.dispatched_count = result.dispatched_count,
              .failure = ReplayFailure::DispatchRejected};
    ++result.dispatched_count;
  }
  return result;
}

} // namespace chronos::runtime::datasets
