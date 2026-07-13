#include "chronos/contracts/serialization.hpp"

#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace chronos::contracts {
namespace {

constexpr std::array<std::uint8_t, 5> kMagic{'C', 'H', 'R', '1', 2};

[[nodiscard]] bool valid_utf8(std::span<const std::uint8_t> bytes) noexcept {
  std::size_t index = 0;
  while (index < bytes.size()) {
    const auto first = bytes[index++];
    if (first <= 0x7F) {
      continue;
    }
    std::size_t continuation_count = 0;
    std::uint32_t codepoint = 0;
    if (first >= 0xC2 && first <= 0xDF) {
      continuation_count = 1;
      codepoint = first & 0x1FU;
    } else if (first >= 0xE0 && first <= 0xEF) {
      continuation_count = 2;
      codepoint = first & 0x0FU;
    } else if (first >= 0xF0 && first <= 0xF4) {
      continuation_count = 3;
      codepoint = first & 0x07U;
    } else {
      return false;
    }
    if (continuation_count > bytes.size() - index) {
      return false;
    }
    for (std::size_t offset = 0; offset < continuation_count; ++offset) {
      const auto continuation = bytes[index++];
      if ((continuation & 0xC0U) != 0x80U) {
        return false;
      }
      codepoint = (codepoint << 6U) | (continuation & 0x3FU);
    }
    const bool overlong = (continuation_count == 2 && codepoint < 0x800U) ||
                          (continuation_count == 3 && codepoint < 0x10000U);
    if (overlong || (codepoint >= 0xD800U && codepoint <= 0xDFFFU) ||
        codepoint > 0x10FFFFU) {
      return false;
    }
  }
  return true;
}

class Writer final {
public:
  void u8(std::uint8_t value) { bytes_.push_back(value); }
  void u32(std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
      u8(static_cast<std::uint8_t>(value >> shift));
    }
  }
  void u64(std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8) {
      u8(static_cast<std::uint8_t>(value >> shift));
    }
  }
  void i64(std::int64_t value) { u64(std::bit_cast<std::uint64_t>(value)); }
  void raw(std::span<const std::uint8_t> value) {
    bytes_.insert(bytes_.end(), value.begin(), value.end());
  }
  void blob(std::span<const std::uint8_t> value) {
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
      throw std::length_error("conformance blob exceeds uint32 length");
    }
    u32(static_cast<std::uint32_t>(value.size()));
    raw(value);
  }
  void text(std::string_view value) {
    blob({reinterpret_cast<const std::uint8_t *>(value.data()), value.size()});
  }
  template <typename Id> void id(const Id &value) { raw(value.bytes()); }
  template <typename Value, typename Encode>
  void optional(const std::optional<Value> &value, Encode encode) {
    u8(value.has_value() ? 1 : 0);
    if (value.has_value()) {
      encode(*value);
    }
  }
  template <typename Value, typename Encode>
  void vector(const std::vector<Value> &values, Encode encode) {
    if (values.size() > std::numeric_limits<std::uint32_t>::max()) {
      throw std::length_error("conformance vector exceeds uint32 length");
    }
    u32(static_cast<std::uint32_t>(values.size()));
    for (const auto &value : values) {
      encode(value);
    }
  }
  [[nodiscard]] std::vector<std::uint8_t> finish() && {
    if (bytes_.size() > kMaxConformanceFrameBytes) {
      throw std::length_error("conformance frame exceeds boundary limit");
    }
    return std::move(bytes_);
  }

private:
  std::vector<std::uint8_t> bytes_;
};

class Reader final {
public:
  explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

  std::uint8_t u8() {
    require(1);
    return bytes_[position_++];
  }
  bool boolean() {
    const auto value = u8();
    if (value > 1) {
      throw std::runtime_error("noncanonical boolean");
    }
    return value == 1;
  }
  std::uint32_t u32() {
    std::uint32_t value = 0;
    for (unsigned shift = 0; shift < 32; shift += 8) {
      value |= static_cast<std::uint32_t>(u8()) << shift;
    }
    return value;
  }
  std::uint64_t u64() {
    std::uint64_t value = 0;
    for (unsigned shift = 0; shift < 64; shift += 8) {
      value |= static_cast<std::uint64_t>(u8()) << shift;
    }
    return value;
  }
  std::int64_t i64() { return std::bit_cast<std::int64_t>(u64()); }
  std::vector<std::uint8_t> blob() {
    const auto size = u32();
    require(size);
    const auto contents = bytes_.subspan(position_, size);
    std::vector<std::uint8_t> result(contents.begin(), contents.end());
    position_ += size;
    return result;
  }
  std::string text() {
    const auto value = blob();
    if (!valid_utf8(value)) {
      throw std::runtime_error("invalid UTF-8 in conformance frame");
    }
    return {reinterpret_cast<const char *>(value.data()), value.size()};
  }
  template <typename Id> Id id() {
    typename Id::bytes_type bytes{};
    require(bytes.size());
    std::memcpy(bytes.data(), bytes_.data() + position_, bytes.size());
    position_ += bytes.size();
    const auto result = Id::from_bytes(bytes);
    if (!result.has_value()) {
      throw std::runtime_error("nil identity in conformance frame");
    }
    return *result;
  }
  template <typename Decode> auto optional(Decode decode) {
    using Value = decltype(decode());
    const auto present = u8();
    if (present > 1) {
      throw std::runtime_error("noncanonical optional flag");
    }
    return present == 1 ? std::optional<Value>{decode()}
                        : std::optional<Value>{};
  }
  template <typename Decode> auto vector(Decode decode) {
    using Value = decltype(decode());
    const auto count = u32();
    if (count > remaining()) {
      throw std::runtime_error("impossible conformance vector count");
    }
    std::vector<Value> values;
    values.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
      values.push_back(decode());
    }
    return values;
  }
  [[nodiscard]] bool done() const noexcept {
    return position_ == bytes_.size();
  }
  [[nodiscard]] std::size_t remaining() const noexcept {
    return bytes_.size() - position_;
  }

private:
  void require(std::size_t count) const {
    if (count > bytes_.size() - position_) {
      throw std::runtime_error("truncated conformance frame");
    }
  }
  std::span<const std::uint8_t> bytes_;
  std::size_t position_ = 0;
};

void write_version(Writer &writer, const VersionRef &value) {
  writer.id(value.definition_id());
  writer.u64(value.version());
}

VersionRef read_version(Reader &reader) {
  const auto definition_id = reader.id<DefinitionId>();
  const auto version = reader.u64();
  const auto result = VersionRef::from(definition_id, version);
  if (!result.has_value()) {
    throw std::runtime_error("invalid version reference");
  }
  return *result;
}

void write_time(Writer &writer, const TimePoint &value) {
  writer.i64(value.nanoseconds());
  writer.id(value.clock_domain_id());
  writer.u8(static_cast<std::uint8_t>(value.clock_class()));
  writer.u32(value.precision_nanoseconds());
}

TimePoint read_time(Reader &reader) {
  const auto nanoseconds = reader.i64();
  const auto clock_domain_id = reader.id<ClockDomainId>();
  const auto clock_class = static_cast<ClockClass>(reader.u8());
  const auto precision_nanoseconds = reader.u32();
  const auto result = TimePoint::from(nanoseconds, clock_domain_id, clock_class,
                                      precision_nanoseconds);
  if (!result.has_value()) {
    throw std::runtime_error("invalid time point");
  }
  return *result;
}

void write_cursor(Writer &writer, const StreamCursor &value) {
  writer.id(value.stream_id());
  writer.u64(value.stream_epoch());
  writer.optional(value.last_consumed_sequence(),
                  [&](std::uint64_t sequence) { writer.u64(sequence); });
}

StreamCursor read_cursor(Reader &reader) {
  const auto stream_id = reader.id<StreamId>();
  const auto epoch = reader.u64();
  const auto sequence = reader.optional([&] { return reader.u64(); });
  const auto result =
      sequence.has_value()
          ? StreamCursor::at_sequence(stream_id, epoch, *sequence)
          : StreamCursor::at_origin(stream_id, epoch);
  if (!result.has_value()) {
    throw std::runtime_error("invalid stream cursor");
  }
  return *result;
}

void write_lineage(Writer &writer, const StateLineage &value) {
  writer.id(value.run_id());
  writer.u64(value.run_input_sequence());
  writer.u32(static_cast<std::uint32_t>(value.cursors().size()));
  for (const auto &cursor : value.cursors()) {
    write_cursor(writer, cursor);
  }
}

StateLineage read_lineage(Reader &reader) {
  const auto run_id = reader.id<RunId>();
  const auto run_input_sequence = reader.u64();
  const auto count = reader.u32();
  if (count > reader.remaining()) {
    throw std::runtime_error("impossible lineage cursor count");
  }
  std::vector<StreamCursor> cursors;
  std::vector<StreamId> required;
  cursors.reserve(count);
  required.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    auto cursor = read_cursor(reader);
    required.push_back(cursor.stream_id());
    cursors.push_back(std::move(cursor));
  }
  const auto result =
      StateLineage::from(run_id, run_input_sequence, required, cursors);
  if (!result.has_value()) {
    throw std::runtime_error("invalid state lineage");
  }
  return *result;
}

void write_registration(Writer &writer, const EventTypeRegistration &value) {
  writer.text(value.event_type);
  writer.id(value.semantic_owner);
  writer.u32(value.envelope_version);
  write_version(writer, value.schema_version);
  writer.vector(value.authorized_producers,
                [&](const ProducerId &id) { writer.id(id); });
  writer.vector(value.allowed_acceptance_classes, [&](AcceptanceClass item) {
    writer.u8(static_cast<std::uint8_t>(item));
  });
  writer.vector(value.permitted_modes, [&](RunMode item) {
    writer.u8(static_cast<std::uint8_t>(item));
  });
  writer.u8(value.root_observation ? 1 : 0);
  writer.u8(static_cast<std::uint8_t>(value.run_scope));
  writer.u8(static_cast<std::uint8_t>(value.event_position));
  writer.u8(value.run_input_eligible ? 1 : 0);
  writer.u8(static_cast<std::uint8_t>(value.source_event));
  writer.u8(static_cast<std::uint8_t>(value.subjects));
  writer.u8(static_cast<std::uint8_t>(value.mode));
  writer.u8(static_cast<std::uint8_t>(value.effective_position));
  writer.u8(static_cast<std::uint8_t>(value.integrity));
  writer.u8(static_cast<std::uint8_t>(value.receive_time));
}

EventTypeRegistration read_registration(Reader &reader) {
  EventTypeRegistration value{
      .event_type = reader.text(),
      .semantic_owner = reader.id<AuthorityId>(),
      .envelope_version = reader.u32(),
      .schema_version = read_version(reader),
      .authorized_producers =
          reader.vector([&] { return reader.id<ProducerId>(); }),
      .allowed_acceptance_classes = reader.vector(
          [&] { return static_cast<AcceptanceClass>(reader.u8()); }),
      .permitted_modes =
          reader.vector([&] { return static_cast<RunMode>(reader.u8()); }),
      .root_observation = reader.boolean(),
      .run_scope = static_cast<Applicability>(reader.u8()),
      .event_position = static_cast<Applicability>(reader.u8()),
      .run_input_eligible = reader.boolean(),
      .source_event = static_cast<Applicability>(reader.u8()),
      .subjects = static_cast<Applicability>(reader.u8()),
      .mode = static_cast<Applicability>(reader.u8()),
      .effective_position = static_cast<EffectivePositionPolicy>(reader.u8()),
      .integrity = static_cast<Applicability>(reader.u8()),
      .receive_time = static_cast<Applicability>(reader.u8()),
  };
  return value;
}

void write_position(Writer &writer, const EventPosition &value) {
  writer.id(value.stream_id());
  writer.u64(value.stream_epoch());
  writer.u64(value.stream_sequence());
}

EventPosition read_position(Reader &reader) {
  const auto stream_id = reader.id<StreamId>();
  const auto stream_epoch = reader.u64();
  const auto stream_sequence = reader.u64();
  const auto result =
      EventPosition::from(stream_id, stream_epoch, stream_sequence);
  if (!result.has_value()) {
    throw std::runtime_error("invalid event position");
  }
  return *result;
}

void write_cause(Writer &writer, const CausationRef &value) {
  writer.u8(static_cast<std::uint8_t>(value.index()));
  std::visit([&](const auto &id) { writer.id(id); }, value);
}

CausationRef read_cause(Reader &reader) {
  switch (reader.u8()) {
  case 0:
    return reader.id<CommandId>();
  case 1:
    return reader.id<EventId>();
  case 2:
    return reader.id<StateViewId>();
  case 3:
    return reader.id<DecisionId>();
  default:
    throw std::runtime_error("invalid causation tag");
  }
}

void write_subject(Writer &writer, const SubjectRef &value) {
  writer.u8(static_cast<std::uint8_t>(value.index()));
  std::visit([&](const auto &id) { writer.id(id); }, value);
}

SubjectRef read_subject(Reader &reader) {
  switch (reader.u8()) {
  case 0:
    return reader.id<CanonicalInstrumentId>();
  case 1:
    return reader.id<ListingId>();
  default:
    throw std::runtime_error("invalid subject tag");
  }
}

void write_envelope(Writer &writer, const EventEnvelope &value) {
  writer.id(value.event_id());
  writer.text(value.event_type());
  writer.u32(value.envelope_version());
  write_version(writer, value.schema_version());
  writer.id(value.semantic_owner());
  writer.id(value.producer().component_id);
  write_version(writer, value.producer().implementation_version);
  writer.id(value.producer().runtime_incarnation_id);
  writer.u8(static_cast<std::uint8_t>(value.acceptance_class()));
  writer.optional(value.run_id(), [&](const RunId &id) { writer.id(id); });
  writer.optional(value.mode(), [&](RunMode mode) {
    writer.u8(static_cast<std::uint8_t>(mode));
  });
  writer.optional(value.event_position(), [&](const EventPosition &position) {
    write_position(writer, position);
  });
  writer.optional(value.run_input_sequence(),
                  [&](std::uint64_t sequence) { writer.u64(sequence); });
  writer.optional(value.effective_position(),
                  [&](std::uint64_t position) { writer.u64(position); });
  writer.optional(value.state_lineage(), [&](const StateLineage &lineage) {
    write_lineage(writer, lineage);
  });
  writer.optional(value.source_event_id(),
                  [&](const SourceEventId &id) { writer.id(id); });
  writer.optional(value.causation_refs(), [&](const auto &refs) {
    writer.vector(refs,
                  [&](const CausationRef &ref) { write_cause(writer, ref); });
  });
  writer.optional(value.correlation_refs(), [&](const auto &refs) {
    writer.vector(refs, [&](const CorrelationId &id) { writer.id(id); });
  });
  writer.optional(value.subject_refs(), [&](const auto &refs) {
    writer.vector(refs,
                  [&](const SubjectRef &ref) { write_subject(writer, ref); });
  });
  writer.optional(value.source_event_time(),
                  [&](const TimePoint &time) { write_time(writer, time); });
  writer.optional(value.chronos_receive_time(),
                  [&](const TimePoint &time) { write_time(writer, time); });
  write_time(writer, value.accept_time());
  writer.optional(value.recoverability_handoff_time(),
                  [&](const TimePoint &time) { write_time(writer, time); });
  writer.optional(value.record_time(),
                  [&](const TimePoint &time) { write_time(writer, time); });
  writer.u8(static_cast<std::uint8_t>(value.quality().status()));
  writer.u32(value.quality().reason_code());
  writer.blob(value.payload());
  writer.optional(value.integrity(),
                  [&](const IntegrityId &id) { writer.id(id); });
}

EventEnvelope read_envelope(Reader &reader,
                            const EventTypeRegistration &registration) {
  const auto event_id = reader.id<EventId>();
  const auto event_type = reader.text();
  const auto envelope_version = reader.u32();
  const auto schema_version = read_version(reader);
  const auto semantic_owner = reader.id<AuthorityId>();
  const ProducerRef producer{reader.id<ProducerId>(), read_version(reader),
                             reader.id<RuntimeId>()};
  const auto acceptance_class = static_cast<AcceptanceClass>(reader.u8());
  auto run_id = reader.optional([&] { return reader.id<RunId>(); });
  auto mode =
      reader.optional([&] { return static_cast<RunMode>(reader.u8()); });
  auto event_position = reader.optional([&] { return read_position(reader); });
  auto run_input = reader.optional([&] { return reader.u64(); });
  auto effective = reader.optional([&] { return reader.u64(); });
  auto lineage = reader.optional([&] { return read_lineage(reader); });
  auto source_event =
      reader.optional([&] { return reader.id<SourceEventId>(); });
  auto causes = reader.optional(
      [&] { return reader.vector([&] { return read_cause(reader); }); });
  auto correlations = reader.optional([&] {
    return reader.vector([&] { return reader.id<CorrelationId>(); });
  });
  auto subjects = reader.optional(
      [&] { return reader.vector([&] { return read_subject(reader); }); });
  auto source_time = reader.optional([&] { return read_time(reader); });
  auto receive_time = reader.optional([&] { return read_time(reader); });
  auto accept_time = read_time(reader);
  auto handoff_time = reader.optional([&] { return read_time(reader); });
  auto record_time = reader.optional([&] { return read_time(reader); });
  const auto quality_status = static_cast<QualityStatus>(reader.u8());
  const auto quality_reason = reader.u32();
  const auto quality = DataQuality::from(quality_status, quality_reason);
  if (!quality.has_value()) {
    throw std::runtime_error("invalid data quality");
  }
  auto payload = reader.blob();
  auto integrity = reader.optional([&] { return reader.id<IntegrityId>(); });
  EventEnvelopeDraft draft{
      .event_id = event_id,
      .event_type = std::move(event_type),
      .envelope_version = envelope_version,
      .schema_version = schema_version,
      .semantic_owner = semantic_owner,
      .producer = producer,
      .acceptance_class = acceptance_class,
      .run_id = std::move(run_id),
      .mode = std::move(mode),
      .event_position = std::move(event_position),
      .run_input_sequence = std::move(run_input),
      .effective_position = std::move(effective),
      .state_lineage = std::move(lineage),
      .source_event_id = std::move(source_event),
      .causation_refs = std::move(causes),
      .correlation_refs = std::move(correlations),
      .subject_refs = std::move(subjects),
      .source_event_time = std::move(source_time),
      .chronos_receive_time = std::move(receive_time),
      .accept_time = accept_time,
      .recoverability_handoff_time = std::move(handoff_time),
      .record_time = std::move(record_time),
      .quality = *quality,
      .payload = std::move(payload),
      .integrity = std::move(integrity),
  };
  auto result = EventEnvelope::from(registration, std::move(draft));
  if (!result.has_value()) {
    throw std::runtime_error("invalid event envelope");
  }
  return std::move(*result);
}

} // namespace

std::vector<std::uint8_t>
encode_conformance_frame(const ConformanceFrame &frame) {
  Writer writer;
  writer.raw(kMagic);
  writer.u8(frame.decimal_scale.exponent());
  writer.i64(frame.price.units());
  write_version(writer, frame.price.definition_ref());
  writer.i64(frame.quantity.units());
  write_version(writer, frame.quantity.definition_ref());
  writer.i64(frame.money.units());
  write_version(writer, frame.money.definition_ref());
  write_registration(writer, frame.registration);
  write_envelope(writer, frame.envelope);
  return std::move(writer).finish();
}

std::optional<ConformanceFrame>
decode_conformance_frame(std::span<const std::uint8_t> bytes) noexcept {
  if (bytes.size() > kMaxConformanceFrameBytes) {
    return std::nullopt;
  }
  try {
    Reader reader(bytes);
    for (const auto expected : kMagic) {
      if (reader.u8() != expected) {
        return std::nullopt;
      }
    }
    const auto scale = DecimalScale::from_exponent(reader.u8());
    const auto price_units = reader.i64();
    const auto price_definition = read_version(reader);
    const auto quantity_units = reader.i64();
    const auto quantity_definition = read_version(reader);
    const auto money_units = reader.i64();
    const auto money_definition = read_version(reader);
    const auto price = Price::from_units(price_units, price_definition);
    const auto quantity =
        Quantity::from_units(quantity_units, quantity_definition);
    const auto money = Money::from_units(money_units, money_definition);
    if (!scale.has_value() || !price.has_value() || !quantity.has_value() ||
        !money.has_value()) {
      return std::nullopt;
    }
    auto registration = read_registration(reader);
    auto envelope = read_envelope(reader, registration);
    if (!reader.done()) {
      return std::nullopt;
    }
    return ConformanceFrame{*scale,
                            *price,
                            *quantity,
                            *money,
                            std::move(registration),
                            std::move(envelope)};
  } catch (...) {
    return std::nullopt;
  }
}

} // namespace chronos::contracts
