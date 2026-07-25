#pragma once

#include "chronos/contracts/digest.hpp"
#include "chronos/contracts/event_envelope.hpp"
#include "chronos/contracts/state_lineage.hpp"
#include "chronos/core/dispatch/run_input_dispatcher.hpp"
#include "chronos/core/market_state/l2_book.hpp"
#include "chronos/core/market_state/listing_aux_state.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace chronos::core::market_state {

enum class ViewPublicationState : std::uint8_t {
  NotPublished,
  PublicationInProgress,
  PublishedToFeatureBoundary,
  FeatureConsumerAccepted,
  PublicationFailedRetryable,
  PublicationFailedTerminal,
};

enum class ListingViewFailure : std::uint8_t {
  None,
  WrongRun,
  WrongListing,
  IncompleteLineage,
  CursorMismatch,
  BookStateMismatch,
  RunInputMismatch,
  InvalidSelectionEvidence,
  InvalidLineageTransition,
  ContradictorySelection,
  SourceCutChanged,
  IdentityDerivationFailed,
  PriorViewPending,
  WrongView,
  WrongBoundary,
  InvalidPublicationTransition,
  PublicationHistoryExhausted,
  AcceptedViewHistoryExhausted,
};

struct ListingViewPublisherConfig final {
  contracts::RunId run_id;
  contracts::ListingId listing_id;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  contracts::VersionRef reference_snapshot_version;
  contracts::VersionRef listing_definition_version;
  contracts::VersionRef reference_configuration_lineage_version;
  std::vector<contracts::StreamId> required_streams;
  contracts::StateLineage initial_lineage;
  contracts::StreamId book_stream_id;
  contracts::StreamId trade_stream_id;
  contracts::StreamId trade_continuity_stream_id;
  contracts::StreamId reference_stream_id;
  contracts::StreamId market_control_stream_id;
  contracts::StreamId run_control_stream_id;
  contracts::StreamId run_timer_stream_id;
  contracts::ConsumerBoundaryId feature_boundary_id;
  dispatch::RunInputDispatcherConfig dispatcher_config;
  contracts::VersionRef merge_policy_version;
  std::uint64_t initial_configuration_epoch{};
  std::optional<std::uint64_t> initial_effective_control_position;
  contracts::VersionRef view_schema_version;
  contracts::VersionRef capability_version;
  contracts::VersionRef transition_policy_version;
  contracts::VersionRef arithmetic_version;
  contracts::VersionRef canonicalization_version;
  contracts::VersionRef bundle_schema_version;
  contracts::VersionRef identity_policy_version;
  std::size_t maximum_publication_transitions{};
  std::size_t maximum_retained_views{};
};

struct ListingViewCutInput final {
  contracts::RunInputSelectionId selection_id;
  dispatch::RunInputSelectionRecord dispatch_selection;
  dispatch::RunInputCandidate dispatch_candidate;
  contracts::EventId selected_event_id;
  std::string selected_event_type;
  contracts::EventPosition selected_event_position;
  contracts::Sha256Digest input_semantic_checksum;
  contracts::Sha256Digest selection_semantic_checksum;
  contracts::VersionRef merge_policy_version;
  std::uint64_t configuration_epoch{};
  std::optional<std::uint64_t> effective_control_position;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  contracts::VersionRef reference_snapshot_version;
  contracts::VersionRef listing_definition_version;
  contracts::VersionRef reference_configuration_lineage_version;
  contracts::StateLineage lineage;
  std::optional<dispatch::AcceptedControlOutcome> accepted_control_outcome;

  bool operator==(const ListingViewCutInput &) const = default;
};

struct ListingStateView final {
  contracts::StateViewId view_id;
  contracts::RunId run_id;
  contracts::ListingId listing_id;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  contracts::VersionRef reference_snapshot_version;
  contracts::VersionRef listing_definition_version;
  contracts::VersionRef reference_configuration_lineage_version;
  contracts::RunInputSelectionId causing_selection_id;
  contracts::EventId causing_event_id;
  std::string causing_event_type;
  contracts::EventPosition causing_event_position;
  contracts::Sha256Digest input_semantic_checksum;
  contracts::Sha256Digest selection_semantic_checksum;
  contracts::VersionRef merge_policy_version;
  std::uint64_t configuration_epoch{};
  std::optional<std::uint64_t> effective_control_position;
  std::optional<contracts::EventId> active_control_outcome_id;
  std::optional<contracts::Sha256Digest>
      active_control_selection_semantic_checksum;
  contracts::StateLineage lineage;
  std::uint64_t l2_transition_sequence{};
  std::vector<L2Level> bids;
  std::vector<L2Level> asks;
  L2TopOfBook top;
  std::vector<RecentTrade> recent_trades;
  ListingQualityState quality;
  std::optional<L2InputEvidence> last_book_input;
  std::optional<contracts::StateViewId> prior_view_id;
  contracts::VersionRef view_schema_version;
  contracts::VersionRef capability_version;
  contracts::VersionRef transition_policy_version;
  contracts::VersionRef arithmetic_version;
  contracts::VersionRef canonicalization_version;
  contracts::Sha256Digest semantic_checksum;

  bool operator==(const ListingStateView &) const = default;
};

struct StateViewBundle final {
  contracts::StateViewId bundle_id;
  contracts::RunId run_id;
  std::uint64_t run_input_sequence{};
  contracts::RunInputSelectionId causing_selection_id;
  contracts::EventId causing_event_id;
  std::vector<std::pair<contracts::ListingId, contracts::StateViewId>>
      listing_views;
  contracts::ListingId listing_id;
  contracts::CanonicalInstrumentId canonical_instrument_id;
  contracts::StateViewId listing_view_id;
  contracts::StreamCursor run_control_cursor;
  contracts::StreamCursor run_timer_cursor;
  contracts::StreamCursor reference_cursor;
  std::int64_t logical_time_nanoseconds{};
  contracts::Sha256Digest selection_semantic_checksum;
  contracts::VersionRef merge_policy_version;
  std::uint64_t configuration_epoch{};
  std::optional<std::uint64_t> effective_control_position;
  std::optional<contracts::EventId> active_control_outcome_id;
  std::optional<contracts::Sha256Digest>
      active_control_selection_semantic_checksum;
  std::optional<contracts::StateViewId> prior_bundle_id;
  contracts::VersionRef view_schema_version;
  contracts::VersionRef bundle_schema_version;
  contracts::VersionRef registry_snapshot_version;
  contracts::VersionRef reference_snapshot_version;
  contracts::VersionRef listing_definition_version;
  contracts::VersionRef reference_configuration_lineage_version;
  contracts::VersionRef arithmetic_version;
  contracts::VersionRef canonicalization_version;
  contracts::VersionRef identity_policy_version;
  contracts::Sha256Digest semantic_checksum;

  bool operator==(const StateViewBundle &) const = default;
};

class AcceptedFeatureCut final {
public:
  [[nodiscard]] const ListingStateView &view() const noexcept { return *view_; }
  [[nodiscard]] const StateViewBundle &bundle() const noexcept {
    return *bundle_;
  }
  [[nodiscard]] const std::optional<dispatch::AcceptedControlOutcome> &
  accepted_control_outcome() const noexcept {
    return accepted_control_outcome_;
  }

private:
  AcceptedFeatureCut(std::shared_ptr<const ListingStateView> view,
                     std::shared_ptr<const StateViewBundle> bundle,
                     std::optional<dispatch::AcceptedControlOutcome> control)
      : view_(std::move(view)), bundle_(std::move(bundle)),
        accepted_control_outcome_(std::move(control)) {}

  std::shared_ptr<const ListingStateView> view_;
  std::shared_ptr<const StateViewBundle> bundle_;
  std::optional<dispatch::AcceptedControlOutcome> accepted_control_outcome_;

  friend class ListingViewPublisher;
};

struct ViewPublicationTransition final {
  contracts::StateViewId view_id;
  contracts::StateViewId bundle_id;
  contracts::PublicationAttemptId attempt_id;
  contracts::ConsumerBoundaryId boundary_id;
  std::uint64_t attempt_number{};
  ViewPublicationState from{ViewPublicationState::NotPublished};
  ViewPublicationState to{ViewPublicationState::PublicationInProgress};

  bool operator==(const ViewPublicationTransition &) const = default;
};

struct ListingViewResult final {
  ListingViewFailure failure{ListingViewFailure::None};
  std::shared_ptr<const ListingStateView> view;
  std::shared_ptr<const StateViewBundle> bundle;

  [[nodiscard]] bool ok() const noexcept {
    return failure == ListingViewFailure::None;
  }
};

class ListingViewPublisher final {
public:
  [[nodiscard]] static std::optional<ListingViewPublisher>
  create(ListingViewPublisherConfig config);

  ListingViewPublisher(ListingViewPublisher &&) noexcept;
  ListingViewPublisher &operator=(ListingViewPublisher &&) noexcept;
  ~ListingViewPublisher();

  [[nodiscard]] ListingViewResult accept_cut(const ListingViewCutInput &input,
                                             const L2Book &book,
                                             const ListingAuxState &auxiliary);
  [[nodiscard]] ListingViewFailure
  transition_publication(const ViewPublicationTransition &transition);

  [[nodiscard]] std::shared_ptr<const ListingStateView>
  accepted_view() const noexcept;
  [[nodiscard]] std::shared_ptr<const ListingStateView>
  published_view() const noexcept;
  [[nodiscard]] std::shared_ptr<const StateViewBundle>
  accepted_bundle() const noexcept;
  [[nodiscard]] std::shared_ptr<const StateViewBundle>
  published_bundle() const noexcept;
  [[nodiscard]] std::optional<AcceptedFeatureCut>
  accepted_feature_cut() const noexcept;
  [[nodiscard]] ViewPublicationState publication_state() const noexcept;
  [[nodiscard]] std::span<const ViewPublicationTransition>
  publication_history() const noexcept;
  [[nodiscard]] std::size_t publication_storage_capacity() const noexcept;

private:
  struct State;
  explicit ListingViewPublisher(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
};

} // namespace chronos::core::market_state
