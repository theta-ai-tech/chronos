#include "chronos/contracts/event_envelope.hpp"

#include "microtest.hpp"

#include <array>

using namespace chronos::contracts;

namespace {
template <typename Id>
Id id(std::string_view value = "018f1f6e-7d3a-7c4b-8a91-0123456789a1") {
  return Id::parse(value).value();
}

VersionRef version(std::string_view value, std::uint64_t number) {
  return VersionRef::from(id<DefinitionId>(value), number).value();
}

TimePoint time_point(std::int64_t value) {
  return TimePoint::from(
             value, id<ClockDomainId>("018f1f6e-7d3a-7c4b-8a91-0123456789a2"),
             ClockClass::chronos_wall, 1)
      .value();
}

EventEnvelopeDraft valid_draft() {
  return EventEnvelopeDraft{
      .event_id = id<EventId>(),
      .event_type = "source.capture.payload_captured",
      .envelope_version = 1,
      .schema_version = version("018f1f6e-7d3a-7c4b-8a91-0123456789a3", 2),
      .semantic_owner = id<AuthorityId>("018f1f6e-7d3a-7c4b-8a91-0123456789a6"),
      .producer =
          ProducerRef{id<ProducerId>("018f1f6e-7d3a-7c4b-8a91-0123456789a4"),
                      version("018f1f6e-7d3a-7c4b-8a91-0123456789a5", 3),
                      id<RuntimeId>("018f1f6e-7d3a-7c4b-8a91-0123456789a7")},
      .acceptance_class = AcceptanceClass::accepted_observation,
      .run_id = std::nullopt,
      .mode = std::nullopt,
      .event_position = std::nullopt,
      .run_input_sequence = std::nullopt,
      .effective_position = std::nullopt,
      .state_lineage = std::nullopt,
      .source_event_id = std::nullopt,
      .causation_refs = std::nullopt,
      .correlation_refs = std::nullopt,
      .subject_refs = std::nullopt,
      .source_event_time = std::nullopt,
      .chronos_receive_time = std::nullopt,
      .accept_time = time_point(10),
      .recoverability_handoff_time = std::nullopt,
      .record_time = std::nullopt,
      .quality = DataQuality::from(QualityStatus::valid, 0).value(),
      .payload = {0x01, 0x02},
      .integrity = std::nullopt,
  };
}

EventTypeRegistration registration() {
  return EventTypeRegistration{
      .event_type = "source.capture.payload_captured",
      .semantic_owner = id<AuthorityId>("018f1f6e-7d3a-7c4b-8a91-0123456789a6"),
      .envelope_version = 1,
      .schema_version = version("018f1f6e-7d3a-7c4b-8a91-0123456789a3", 2),
      .authorized_producers = {id<ProducerId>(
          "018f1f6e-7d3a-7c4b-8a91-0123456789a4")},
      .root_observation = true,
      .run_scope = Applicability::optional,
      .event_position = Applicability::optional,
      .run_input_eligible = false,
      .source_event = Applicability::optional,
      .subjects = Applicability::optional,
      .mode = Applicability::optional,
      .effective_position = EffectivePositionPolicy::optional,
      .integrity = Applicability::optional,
      .receive_time = Applicability::optional,
  };
}
} // namespace

TEST_CASE("reserved event taxonomy accepts only canonical names") {
  CHECK(kReservedEventNamespaces.size() == 22);
  CHECK(event_namespace("market.book.snapshot_applied") == "market.book");
  CHECK(event_namespace("execution.fill.accepted") == "execution.fill");
  CHECK(is_valid_event_type("source.capture.payload_captured"));
  CHECK(is_valid_event_type("run.input.selection.event_selected"));
  CHECK(!is_valid_event_type("market.book.snapshot_applied"));
  CHECK(!is_valid_event_type("unknown.payload_captured"));
  CHECK(!is_valid_event_type("market.book..snapshot"));
  CHECK(!is_valid_event_type("market.book.Snapshot"));
}

TEST_CASE("minimum envelope preserves genuinely absent fields") {
  const auto envelope =
      EventEnvelope::from(registration(), valid_draft()).value();
  CHECK(!envelope.run_id().has_value());
  CHECK(!envelope.state_lineage().has_value());
  CHECK(!envelope.source_event_time().has_value());
  CHECK(!envelope.record_time().has_value());
  CHECK(!envelope.causation_refs().has_value());
  CHECK(envelope.payload() == std::vector<std::uint8_t>({0x01, 0x02}));
}

TEST_CASE("envelope rejects invalid taxonomy versions and fake empty refs") {
  auto draft = valid_draft();
  draft.event_type = "unknown.fact";
  CHECK(!EventEnvelope::from(registration(), std::move(draft)).has_value());
  draft = valid_draft();
  draft.envelope_version = 0;
  CHECK(!EventEnvelope::from(registration(), std::move(draft)).has_value());
  draft = valid_draft();
  draft.causation_refs = std::vector<CausationRef>{};
  CHECK(!EventEnvelope::from(registration(), std::move(draft)).has_value());
}

TEST_CASE("registry enforces owner and required stable semantics") {
  auto draft = valid_draft();
  draft.semantic_owner =
      id<AuthorityId>("018f1f6e-7d3a-7c4b-8a91-0123456789af");
  CHECK(!EventEnvelope::from(registration(), std::move(draft)).has_value());

  draft = valid_draft();
  auto derived = registration();
  derived.root_observation = false;
  CHECK(!EventEnvelope::from(derived, std::move(draft)).has_value());

  CHECK(!EventPosition::from(id<StreamId>(), 0, 1).has_value());

  draft = valid_draft();
  draft.schema_version = version("018f1f6e-7d3a-7c4b-8a91-0123456789ae", 9);
  CHECK(!EventEnvelope::from(registration(), std::move(draft)).has_value());

  draft = valid_draft();
  draft.producer.component_id =
      id<ProducerId>("018f1f6e-7d3a-7c4b-8a91-0123456789ae");
  CHECK(!EventEnvelope::from(registration(), std::move(draft)).has_value());
}

TEST_CASE("envelope validates causation and typed subjects") {
  auto draft = valid_draft();
  draft.causation_refs = std::vector<CausationRef>{id<EventId>()};
  CHECK(!EventEnvelope::from(registration(), std::move(draft)).has_value());

  draft = valid_draft();
  const auto cause = id<EventId>("018f1f6e-7d3a-7c4b-8a91-0123456789af");
  draft.causation_refs = std::vector<CausationRef>{cause, cause};
  CHECK(!EventEnvelope::from(registration(), std::move(draft)).has_value());

  draft = valid_draft();
  draft.subject_refs =
      std::vector<SubjectRef>{id<CanonicalInstrumentId>(), id<ListingId>()};
  auto subject_registration = registration();
  subject_registration.subjects = Applicability::required;
  CHECK(
      EventEnvelope::from(subject_registration, std::move(draft)).has_value());

  draft = valid_draft();
  draft.subject_refs = std::vector<SubjectRef>{id<ListingId>()};
  CHECK(EventEnvelope::from(registration(), std::move(draft)).has_value());
}

TEST_CASE("effective position follows acceptance disposition") {
  auto policy = registration();
  policy.run_scope = Applicability::required;
  policy.effective_position = EffectivePositionPolicy::accepted_transition_only;
  auto draft = valid_draft();
  draft.run_id = id<RunId>();
  draft.acceptance_class = AcceptanceClass::accepted_transition;
  draft.effective_position = 12;
  const auto accepted = EventEnvelope::from(policy, draft).value();
  CHECK(accepted.acceptance_class() == AcceptanceClass::accepted_transition);
  CHECK(accepted.effective_position() == 12);
  CHECK(!accepted.mode().has_value());
  CHECK(!accepted.correlation_refs().has_value());
  CHECK(!accepted.recoverability_handoff_time().has_value());
  CHECK(!accepted.integrity().has_value());

  draft.acceptance_class = AcceptanceClass::accepted_rejection;
  CHECK(!EventEnvelope::from(policy, draft).has_value());
  draft.effective_position = std::nullopt;
  CHECK(EventEnvelope::from(policy, std::move(draft)).has_value());
}

TEST_CASE("causation references preserve their identity kind") {
  auto draft = valid_draft();
  draft.causation_refs = std::vector<CausationRef>{
      id<CommandId>("018f1f6e-7d3a-7c4b-8a91-0123456789ab"),
      id<StateViewId>("018f1f6e-7d3a-7c4b-8a91-0123456789ac"),
      id<DecisionId>("018f1f6e-7d3a-7c4b-8a91-0123456789ad")};
  CHECK(EventEnvelope::from(registration(), std::move(draft)).has_value());
}

TEST_CASE("lineage fields agree instead of inventing positions") {
  const auto stream = id<StreamId>();
  const std::array required{stream};
  const std::array cursors{StreamCursor::at_sequence(stream, 1, 4).value()};
  const auto run = id<RunId>();
  const auto lineage = StateLineage::from(run, 7, required, cursors).value();

  auto draft = valid_draft();
  draft.run_id = run;
  draft.event_position = EventPosition::from(stream, 1, 4);
  draft.run_input_sequence = 7;
  draft.state_lineage = lineage;
  auto run_registration = registration();
  run_registration.run_scope = Applicability::required;
  run_registration.event_position = Applicability::required;
  run_registration.run_input_eligible = true;
  CHECK(EventEnvelope::from(run_registration, draft).has_value());
  draft.run_input_sequence = 8;
  CHECK(!EventEnvelope::from(run_registration, std::move(draft)).has_value());

  draft = valid_draft();
  draft.run_input_sequence = 3;
  CHECK(!EventEnvelope::from(registration(), std::move(draft)).has_value());
}
