#include "chronos/core/reference_data/reference_data.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <utility>

namespace chronos::core::reference_data {
namespace {

struct ParsedDecimal final {
  contracts::AmountUnits units{};
  std::uint8_t exponent{};
};

std::optional<ParsedDecimal> parse_decimal(std::string_view value) {
  if (value.empty() || value.front() == '-' || value.front() == '+') {
    return std::nullopt;
  }
  const auto point = value.find('.');
  if (point != std::string_view::npos &&
      value.find('.', point + 1) != std::string_view::npos) {
    return std::nullopt;
  }
  const auto integer = value.substr(0, point);
  auto fraction = point == std::string_view::npos ? std::string_view{}
                                                  : value.substr(point + 1);
  if (integer.empty() ||
      (point != std::string_view::npos && fraction.empty()) ||
      fraction.size() > contracts::DecimalScale::kMaxExponent) {
    return std::nullopt;
  }
  while (!fraction.empty() && fraction.back() == '0') {
    fraction.remove_suffix(1);
  }
  std::string digits;
  digits.reserve(integer.size() + fraction.size());
  digits.append(integer);
  digits.append(fraction);
  contracts::AmountUnits units{};
  const auto [end, error] =
      std::from_chars(digits.data(), digits.data() + digits.size(), units);
  if (error != std::errc{} || end != digits.data() + digits.size()) {
    return std::nullopt;
  }
  return ParsedDecimal{units, static_cast<std::uint8_t>(fraction.size())};
}

std::optional<contracts::AmountUnits> scale_to(const ParsedDecimal &value,
                                               std::uint8_t target_exponent) {
  if (value.exponent > target_exponent) {
    return std::nullopt;
  }
  auto units = value.units;
  for (auto exponent = value.exponent; exponent < target_exponent; ++exponent) {
    if (units > std::numeric_limits<contracts::AmountUnits>::max() / 10) {
      return std::nullopt;
    }
    units *= 10;
  }
  return units;
}

bool valid_token(std::string_view value) {
  if (value.empty() || value.size() > 32) {
    return false;
  }
  for (const char character : value) {
    const bool valid = (character >= 'A' && character <= 'Z') ||
                       (character >= 'a' && character <= 'z') ||
                       (character >= '0' && character <= '9') ||
                       character == '-' || character == '_';
    if (!valid) {
      return false;
    }
  }
  return true;
}

void append_u64(std::vector<std::byte> &output, std::uint64_t value) {
  for (std::size_t index = 0; index < sizeof(value); ++index)
    output.push_back(static_cast<std::byte>(value >> (index * 8U)));
}

void append_string(std::vector<std::byte> &output, std::string_view value) {
  append_u64(output, static_cast<std::uint64_t>(value.size()));
  if (value.empty())
    return;
  const auto *begin = reinterpret_cast<const std::byte *>(value.data());
  output.insert(output.end(), begin, begin + value.size());
}

void append_version(std::vector<std::byte> &output,
                    const contracts::VersionRef &version) {
  const auto definition_id = version.definition_id();
  for (const auto byte : definition_id.bytes())
    output.push_back(static_cast<std::byte>(byte));
  append_u64(output, version.version());
}

contracts::Sha256Digest
lineage_checksum(std::uint64_t lineage_version_number,
                 std::string_view lineage_schema_version,
                 std::string_view semantic_key_policy_version,
                 std::string_view effective_basis_policy_version,
                 std::string_view selection_policy_version,
                 const std::vector<ReferenceSnapshot> &allowed_snapshots) {
  std::vector<contracts::VersionRef> snapshot_versions;
  snapshot_versions.reserve(allowed_snapshots.size());
  for (const auto &snapshot : allowed_snapshots)
    snapshot_versions.push_back(snapshot.version());
  std::sort(snapshot_versions.begin(), snapshot_versions.end());

  std::vector<std::byte> canonical;
  append_string(canonical, "chronos-reference-configuration-lineage-v1");
  append_u64(canonical, lineage_version_number);
  append_string(canonical, lineage_schema_version);
  append_string(canonical, semantic_key_policy_version);
  append_string(canonical, effective_basis_policy_version);
  append_string(canonical, selection_policy_version);
  append_u64(canonical, static_cast<std::uint64_t>(snapshot_versions.size()));
  for (const auto &version : snapshot_versions)
    append_version(canonical, version);
  return contracts::sha256(canonical);
}

bool valid(ListingStatus status) {
  return status == ListingStatus::Active || status == ListingStatus::Inactive;
}

bool valid(ProductClass product_class) {
  return product_class == ProductClass::Spot ||
         product_class == ProductClass::LinearPerpetual;
}

bool valid(VenueEnvironment environment) {
  return environment == VenueEnvironment::Test ||
         environment == VenueEnvironment::Production;
}

bool contained_by(const EffectiveInterval &inner,
                  const EffectiveInterval &outer) {
  if (inner.partition_id() != outer.partition_id() ||
      inner.first() < outer.first()) {
    return false;
  }
  if (!outer.last_exclusive().has_value()) {
    return true;
  }
  return inner.last_exclusive().has_value() &&
         *inner.last_exclusive() <= *outer.last_exclusive();
}

} // namespace

EffectiveInterval::EffectiveInterval(
    contracts::CapturePartitionId partition_id, std::uint64_t first,
    std::optional<std::uint64_t> last_exclusive) noexcept
    : partition_id_(partition_id), first_(first),
      last_exclusive_(last_exclusive) {}

std::optional<EffectiveInterval> EffectiveInterval::from_capture_sequence(
    contracts::CapturePartitionId partition_id, std::uint64_t first,
    std::optional<std::uint64_t> last_exclusive) {
  if (first == 0 || (last_exclusive.has_value() && *last_exclusive <= first)) {
    return std::nullopt;
  }
  return EffectiveInterval(partition_id, first, last_exclusive);
}

bool EffectiveInterval::contains(
    contracts::CapturePartitionId partition_id,
    std::uint64_t capture_sequence) const noexcept {
  return partition_id == partition_id_ && capture_sequence >= first_ &&
         (!last_exclusive_.has_value() || capture_sequence < *last_exclusive_);
}

contracts::CapturePartitionId EffectiveInterval::partition_id() const noexcept {
  return partition_id_;
}

std::uint64_t EffectiveInterval::first() const noexcept { return first_; }

std::optional<std::uint64_t>
EffectiveInterval::last_exclusive() const noexcept {
  return last_exclusive_;
}

DecimalIncrement::DecimalIncrement(contracts::AmountUnits decimal_units,
                                   contracts::DecimalScale scale) noexcept
    : decimal_units_(decimal_units), scale_(scale) {}

std::optional<DecimalIncrement>
DecimalIncrement::parse(std::string_view decimal) {
  const auto parsed = parse_decimal(decimal);
  if (!parsed.has_value() || parsed->units <= 0) {
    return std::nullopt;
  }
  const auto scale = contracts::DecimalScale::from_exponent(parsed->exponent);
  if (!scale.has_value()) {
    return std::nullopt;
  }
  return DecimalIncrement(parsed->units, *scale);
}

std::optional<contracts::AmountUnits>
DecimalIncrement::parse_multiple(std::string_view decimal) const {
  const auto parsed = parse_decimal(decimal);
  if (!parsed.has_value()) {
    return std::nullopt;
  }
  const auto scaled = scale_to(*parsed, scale_.exponent());
  if (!scaled.has_value() || *scaled % decimal_units_ != 0) {
    return std::nullopt;
  }
  return *scaled / decimal_units_;
}

contracts::AmountUnits DecimalIncrement::decimal_units() const noexcept {
  return decimal_units_;
}

contracts::DecimalScale DecimalIncrement::scale() const noexcept {
  return scale_;
}

std::optional<contracts::Price>
ListingDefinition::parse_price(std::string_view decimal) const {
  const auto units = price_tick.parse_multiple(decimal);
  if (!units.has_value()) {
    return std::nullopt;
  }
  return contracts::Price::from_units(*units, version);
}

std::optional<contracts::Quantity>
ListingDefinition::parse_quantity(std::string_view decimal) const {
  const auto units = quantity_step.parse_multiple(decimal);
  if (!units.has_value()) {
    return std::nullopt;
  }
  return contracts::Quantity::from_units(*units, version);
}

ReferenceSnapshot::ReferenceSnapshot(contracts::VersionRef snapshot_version,
                                     CanonicalInstrumentDefinition instrument,
                                     ListingDefinition listing)
    : snapshot_version_(snapshot_version), instrument_(std::move(instrument)),
      listing_(std::move(listing)) {}

std::optional<ReferenceSnapshot>
ReferenceSnapshot::create(contracts::VersionRef snapshot_version,
                          CanonicalInstrumentDefinition instrument,
                          ListingDefinition listing) {
  if (listing.instrument_id != instrument.instrument_id ||
      !valid(instrument.product_class) || !valid(listing.environment) ||
      !valid(listing.status) ||
      snapshot_version.definition_id() == instrument.version.definition_id() ||
      snapshot_version.definition_id() == listing.version.definition_id() ||
      instrument.version.definition_id() == listing.version.definition_id() ||
      !valid_token(instrument.base_asset) ||
      !valid_token(instrument.quote_asset) ||
      instrument.base_asset == instrument.quote_asset ||
      !valid_token(listing.venue) || !valid_token(listing.source_symbol) ||
      !contained_by(listing.effective_interval,
                    instrument.effective_interval)) {
    return std::nullopt;
  }
  return ReferenceSnapshot(snapshot_version, std::move(instrument),
                           std::move(listing));
}

const contracts::VersionRef &ReferenceSnapshot::version() const noexcept {
  return snapshot_version_;
}

const CanonicalInstrumentDefinition &
ReferenceSnapshot::instrument() const noexcept {
  return instrument_;
}

const ListingDefinition &ReferenceSnapshot::listing() const noexcept {
  return listing_;
}

const ListingDefinition *
ReferenceSnapshot::resolve(std::string_view venue, VenueEnvironment environment,
                           ProductClass product_class,
                           std::string_view source_symbol,
                           contracts::CapturePartitionId partition_id,
                           std::uint64_t capture_sequence) const noexcept {
  if (listing_.status != ListingStatus::Active || listing_.venue != venue ||
      listing_.environment != environment ||
      instrument_.product_class != product_class ||
      listing_.source_symbol != source_symbol ||
      !listing_.effective_interval.contains(partition_id, capture_sequence) ||
      !instrument_.effective_interval.contains(partition_id,
                                               capture_sequence)) {
    return nullptr;
  }
  return &listing_;
}

ReferenceConfigurationLineage::ReferenceConfigurationLineage(
    contracts::VersionRef lineage_version,
    contracts::Sha256Digest semantic_checksum,
    std::string lineage_schema_version, std::string semantic_key_policy_version,
    std::string effective_basis_policy_version,
    std::string selection_policy_version,
    std::vector<ReferenceSnapshot> allowed_snapshots)
    : lineage_version_(lineage_version), semantic_checksum_(semantic_checksum),
      lineage_schema_version_(std::move(lineage_schema_version)),
      semantic_key_policy_version_(std::move(semantic_key_policy_version)),
      effective_basis_policy_version_(
          std::move(effective_basis_policy_version)),
      selection_policy_version_(std::move(selection_policy_version)),
      allowed_snapshots_(std::move(allowed_snapshots)) {}

std::optional<ReferenceConfigurationLineage>
ReferenceConfigurationLineage::create(
    std::uint64_t lineage_version_number, std::string lineage_schema_version,
    std::string semantic_key_policy_version,
    std::string effective_basis_policy_version,
    std::string selection_policy_version,
    std::vector<ReferenceSnapshot> allowed_snapshots) {
  if (lineage_version_number == 0 || !valid_token(lineage_schema_version) ||
      !valid_token(semantic_key_policy_version) ||
      !valid_token(effective_basis_policy_version) ||
      !valid_token(selection_policy_version) || allowed_snapshots.empty()) {
    return std::nullopt;
  }
  const auto checksum = lineage_checksum(
      lineage_version_number, lineage_schema_version,
      semantic_key_policy_version, effective_basis_policy_version,
      selection_policy_version, allowed_snapshots);
  contracts::DefinitionId::bytes_type identity_bytes{};
  std::copy_n(checksum.bytes.begin(), identity_bytes.size(),
              identity_bytes.begin());
  const auto definition_id =
      contracts::DefinitionId::from_bytes(identity_bytes);
  if (!definition_id.has_value())
    return std::nullopt;
  const auto lineage_version =
      contracts::VersionRef::from(*definition_id, lineage_version_number);
  if (!lineage_version.has_value())
    return std::nullopt;
  for (std::size_t index = 0; index < allowed_snapshots.size(); ++index) {
    const auto &snapshot = allowed_snapshots[index];
    if (lineage_version->definition_id() ==
            snapshot.version().definition_id() ||
        lineage_version->definition_id() ==
            snapshot.instrument().version.definition_id() ||
        lineage_version->definition_id() ==
            snapshot.listing().version.definition_id()) {
      return std::nullopt;
    }
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (allowed_snapshots[prior].version() == snapshot.version()) {
        return std::nullopt;
      }
    }
  }
  return ReferenceConfigurationLineage(
      *lineage_version, checksum, std::move(lineage_schema_version),
      std::move(semantic_key_policy_version),
      std::move(effective_basis_policy_version),
      std::move(selection_policy_version), std::move(allowed_snapshots));
}

const contracts::Sha256Digest &
ReferenceConfigurationLineage::semantic_checksum() const noexcept {
  return semantic_checksum_;
}

const contracts::VersionRef &
ReferenceConfigurationLineage::version() const noexcept {
  return lineage_version_;
}

std::string_view
ReferenceConfigurationLineage::lineage_schema_version() const noexcept {
  return lineage_schema_version_;
}

std::string_view
ReferenceConfigurationLineage::semantic_key_policy_version() const noexcept {
  return semantic_key_policy_version_;
}

std::string_view
ReferenceConfigurationLineage::effective_basis_policy_version() const noexcept {
  return effective_basis_policy_version_;
}

std::string_view
ReferenceConfigurationLineage::selection_policy_version() const noexcept {
  return selection_policy_version_;
}

ReferenceSelectionResult ReferenceConfigurationLineage::select(
    std::string_view venue, VenueEnvironment environment,
    ProductClass product_class, std::string_view source_symbol,
    contracts::CapturePartitionId partition_id,
    std::uint64_t capture_sequence) const noexcept {
  ReferenceSelectionResult result{.failure =
                                      ReferenceSelectionFailure::Missing};
  for (const auto &snapshot : allowed_snapshots_) {
    const auto *listing =
        snapshot.resolve(venue, environment, product_class, source_symbol,
                         partition_id, capture_sequence);
    if (listing == nullptr) {
      continue;
    }
    if (result.snapshot != nullptr) {
      return {.failure = ReferenceSelectionFailure::Ambiguous};
    }
    result = {.snapshot = &snapshot,
              .listing = listing,
              .failure = ReferenceSelectionFailure::None};
  }
  return result;
}

} // namespace chronos::core::reference_data
