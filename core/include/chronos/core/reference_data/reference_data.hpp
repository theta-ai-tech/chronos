#pragma once

#include "chronos/contracts/fixed_point.hpp"
#include "chronos/contracts/value_objects.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace chronos::core::reference_data {

enum class ListingStatus : std::uint8_t { Active, Inactive };
enum class ProductClass : std::uint8_t { Spot };
struct EffectiveDomainIdTag;
using EffectiveDomainId = contracts::OpaqueId<EffectiveDomainIdTag>;

class EffectiveInterval final {
public:
  [[nodiscard]] static std::optional<EffectiveInterval>
  from_capture_sequence(EffectiveDomainId domain_id, std::uint64_t first,
                        std::optional<std::uint64_t> last_exclusive);
  [[nodiscard]] bool contains(EffectiveDomainId domain_id,
                              std::uint64_t capture_sequence) const noexcept;
  [[nodiscard]] EffectiveDomainId domain_id() const noexcept;
  [[nodiscard]] std::uint64_t first() const noexcept;
  [[nodiscard]] std::optional<std::uint64_t> last_exclusive() const noexcept;

private:
  EffectiveInterval(EffectiveDomainId domain_id, std::uint64_t first,
                    std::optional<std::uint64_t> last_exclusive) noexcept;
  EffectiveDomainId domain_id_;
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
  std::string source_symbol;
  ListingStatus status{ListingStatus::Active};
  DecimalIncrement price_tick;
  DecimalIncrement quantity_step;
  std::uint64_t amount_definition_ref{};
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
  resolve(std::string_view venue, std::string_view source_symbol,
          EffectiveDomainId domain_id,
          std::uint64_t capture_sequence) const noexcept;

private:
  ReferenceSnapshot(contracts::VersionRef snapshot_version,
                    CanonicalInstrumentDefinition instrument,
                    ListingDefinition listing);
  contracts::VersionRef snapshot_version_;
  CanonicalInstrumentDefinition instrument_;
  ListingDefinition listing_;
};

} // namespace chronos::core::reference_data
