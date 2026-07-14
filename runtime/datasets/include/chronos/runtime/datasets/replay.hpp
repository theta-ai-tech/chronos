#pragma once

#include "chronos/adapters/market_data/capture_dataset.hpp"
#include "chronos/contracts/digest.hpp"
#include "chronos/contracts/value_objects.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace chronos::runtime::datasets {

enum class ReplayClass : std::uint8_t {
  FaithfulCaptureOrder,
  NormalizedFact,
};

enum class ReplayFailure : std::uint8_t {
  None,
  InvalidManifest,
  DatasetIneligible,
  DatasetMismatch,
  DispatchRejected,
};

struct ReplayVersionPins final {
  std::string provider_version;
  std::string merge_policy_version;
  std::string schema_registry_version;
  std::string canonicalization_version;
  std::optional<std::string> normalizer_version;
  std::optional<contracts::VersionRef> reference_lineage_version;

  bool operator==(const ReplayVersionPins &) const = default;
};

class ReplayRunManifest final {
public:
  [[nodiscard]] static std::optional<ReplayRunManifest>
  create(contracts::RunId run_id, ReplayClass replay_class,
         contracts::Sha256Digest dataset_identity, ReplayVersionPins pins);

  [[nodiscard]] const contracts::Sha256Digest &identity() const noexcept;
  [[nodiscard]] contracts::RunId run_id() const noexcept;
  [[nodiscard]] ReplayClass replay_class() const noexcept;
  [[nodiscard]] const contracts::Sha256Digest &
  dataset_identity() const noexcept;
  [[nodiscard]] const ReplayVersionPins &pins() const noexcept;

private:
  ReplayRunManifest(contracts::Sha256Digest identity, contracts::RunId run_id,
                    ReplayClass replay_class,
                    contracts::Sha256Digest dataset_identity,
                    ReplayVersionPins pins);

  contracts::Sha256Digest identity_;
  contracts::RunId run_id_;
  ReplayClass replay_class_;
  contracts::Sha256Digest dataset_identity_;
  ReplayVersionPins pins_;
};

struct NormalizedFactRecord final {
  std::uint64_t normalized_position{};
  std::string event_type;
  std::vector<std::byte> semantic_payload;
  contracts::Sha256Digest semantic_checksum;

  bool operator==(const NormalizedFactRecord &) const = default;
};

class NormalizedFactDataset final {
public:
  [[nodiscard]] static std::optional<NormalizedFactDataset>
  create(std::vector<NormalizedFactRecord> records);

  [[nodiscard]] const contracts::Sha256Digest &identity() const noexcept;
  [[nodiscard]] const std::vector<NormalizedFactRecord> &
  records() const noexcept;

private:
  NormalizedFactDataset(contracts::Sha256Digest identity,
                        std::vector<NormalizedFactRecord> records);

  contracts::Sha256Digest identity_;
  std::vector<NormalizedFactRecord> records_;
};

struct ReplayDispatchInput final {
  ReplayClass replay_class{ReplayClass::FaithfulCaptureOrder};
  std::uint64_t replay_ordinal{};
  std::string_view event_type;
  std::span<const std::byte> semantic_payload;
  contracts::Sha256Digest semantic_checksum;
  std::optional<contracts::SourceEventId> source_event_id;
  std::optional<std::uint64_t> capture_sequence;
  std::optional<std::uint64_t> normalized_position;
};

class ReplayDispatchSink {
public:
  virtual ~ReplayDispatchSink() = default;
  [[nodiscard]] virtual bool accept(const ReplayDispatchInput &input) = 0;
};

struct ReplayResult final {
  std::uint64_t dispatched_count{};
  ReplayFailure failure{ReplayFailure::None};

  [[nodiscard]] bool ok() const noexcept {
    return failure == ReplayFailure::None;
  }
};

[[nodiscard]] ReplayResult
replay_capture_order(const ReplayRunManifest &manifest,
                     const adapters::market_data::DatasetReadResult &dataset,
                     ReplayDispatchSink &sink);

[[nodiscard]] ReplayResult
replay_normalized_facts(const ReplayRunManifest &manifest,
                        const NormalizedFactDataset &dataset,
                        ReplayDispatchSink &sink);

} // namespace chronos::runtime::datasets
