#pragma once

#include "chronos/contracts/digest.hpp"
#include "chronos/contracts/fixed_point.hpp"
#include "chronos/contracts/value_objects.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chronos::core::reference_data {

enum class ListingStatus : std::uint8_t { Active, Inactive };
enum class ProductClass : std::uint8_t { Spot, LinearPerpetual };
enum class VenueEnvironment : std::uint8_t { Test, Production };

class EffectiveInterval final {
public:
  [[nodiscard]] static std::optional<EffectiveInterval>
  from_capture_sequence(contracts::CapturePartitionId partition_id,
                        std::uint64_t first,
                        std::optional<std::uint64_t> last_exclusive);
  [[nodiscard]] bool contains(contracts::CapturePartitionId partition_id,
                              std::uint64_t capture_sequence) const noexcept;
  [[nodiscard]] contracts::CapturePartitionId partition_id() const noexcept;
  [[nodiscard]] std::uint64_t first() const noexcept;
  [[nodiscard]] std::optional<std::uint64_t> last_exclusive() const noexcept;

private:
  EffectiveInterval(contracts::CapturePartitionId partition_id,
                    std::uint64_t first,
                    std::optional<std::uint64_t> last_exclusive) noexcept;
  contracts::CapturePartitionId partition_id_;
  std::uint64_t first_;
  std::optional<std::uint64_t> last_exclusive_;
};

class DecimalIncrement final {
public:
  [[nodiscard]] static std::optional<DecimalIncrement>
  parse(std::string_view decimal);
  [[nodiscard]] std::optional<contracts::AmountUnits>
  parse_multiple(std::string_view decimal) const;
  [[nodiscard]] contracts::AmountUnits decimal_units() const noexcept;
  [[nodiscard]] contracts::DecimalScale scale() const noexcept;

private:
  DecimalIncrement(contracts::AmountUnits decimal_units,
                   contracts::DecimalScale scale) noexcept;
  contracts::AmountUnits decimal_units_;
  contracts::DecimalScale scale_;
};

struct CanonicalInstrumentDefinition final {
  contracts::CanonicalInstrumentId instrument_id;
  contracts::VersionRef version;
  std::string base_asset;
  std::string quote_asset;
  ProductClass product_class{ProductClass::Spot};
  EffectiveInterval effective_interval;
};

struct ListingDefinition final {
  contracts::ListingId listing_id;
  contracts::CanonicalInstrumentId instrument_id;
  contracts::VersionRef version;
  std::string venue;
  VenueEnvironment environment{VenueEnvironment::Test};
  std::string source_symbol;
  ListingStatus status{ListingStatus::Active};
  DecimalIncrement price_tick;
  DecimalIncrement quantity_step;
  EffectiveInterval effective_interval;

  [[nodiscard]] std::optional<contracts::Price>
  parse_price(std::string_view decimal) const;
  [[nodiscard]] std::optional<contracts::Quantity>
  parse_quantity(std::string_view decimal) const;
};

class ReferenceSnapshot final {
public:
  [[nodiscard]] static std::optional<ReferenceSnapshot>
  create(contracts::VersionRef snapshot_version,
         CanonicalInstrumentDefinition instrument, ListingDefinition listing);

  [[nodiscard]] const contracts::VersionRef &version() const noexcept;
  [[nodiscard]] const CanonicalInstrumentDefinition &
  instrument() const noexcept;
  [[nodiscard]] const ListingDefinition &listing() const noexcept;
  [[nodiscard]] const ListingDefinition *
  resolve(std::string_view venue, VenueEnvironment environment,
          ProductClass product_class, std::string_view source_symbol,
          contracts::CapturePartitionId partition_id,
          std::uint64_t capture_sequence) const noexcept;

private:
  ReferenceSnapshot(contracts::VersionRef snapshot_version,
                    CanonicalInstrumentDefinition instrument,
                    ListingDefinition listing);
  contracts::VersionRef snapshot_version_;
  CanonicalInstrumentDefinition instrument_;
  ListingDefinition listing_;
};

enum class ReferenceSelectionFailure : std::uint8_t {
  None,
  Missing,
  Ambiguous,
};

struct ReferenceSelectionResult final {
  const ReferenceSnapshot *snapshot{};
  const ListingDefinition *listing{};
  ReferenceSelectionFailure failure{ReferenceSelectionFailure::None};

  [[nodiscard]] bool ok() const noexcept {
    return snapshot != nullptr && listing != nullptr &&
           failure == ReferenceSelectionFailure::None;
  }
};

class ReferenceConfigurationLineage final {
public:
  [[nodiscard]] static std::optional<ReferenceConfigurationLineage>
  create(std::uint64_t lineage_version_number,
         std::string lineage_schema_version,
         std::string semantic_key_policy_version,
         std::string effective_basis_policy_version,
         std::string selection_policy_version,
         std::vector<ReferenceSnapshot> allowed_snapshots);

  [[nodiscard]] const contracts::VersionRef &version() const noexcept;
  [[nodiscard]] const contracts::Sha256Digest &
  semantic_checksum() const noexcept;
  [[nodiscard]] std::string_view lineage_schema_version() const noexcept;
  [[nodiscard]] std::string_view semantic_key_policy_version() const noexcept;
  [[nodiscard]] std::string_view
  effective_basis_policy_version() const noexcept;
  [[nodiscard]] std::string_view selection_policy_version() const noexcept;
  [[nodiscard]] ReferenceSelectionResult
  select(std::string_view venue, VenueEnvironment environment,
         ProductClass product_class, std::string_view source_symbol,
         contracts::CapturePartitionId partition_id,
         std::uint64_t capture_sequence) const noexcept;

private:
  ReferenceConfigurationLineage(
      contracts::VersionRef lineage_version,
      contracts::Sha256Digest semantic_checksum,
      std::string lineage_schema_version,
      std::string semantic_key_policy_version,
      std::string effective_basis_policy_version,
      std::string selection_policy_version,
      std::vector<ReferenceSnapshot> allowed_snapshots);

  contracts::VersionRef lineage_version_;
  contracts::Sha256Digest semantic_checksum_;
  std::string lineage_schema_version_;
  std::string semantic_key_policy_version_;
  std::string effective_basis_policy_version_;
  std::string selection_policy_version_;
  std::vector<ReferenceSnapshot> allowed_snapshots_;
};

} // namespace chronos::core::reference_data
