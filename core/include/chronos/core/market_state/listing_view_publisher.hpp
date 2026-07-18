#pragma once

#include "chronos/contracts/digest.hpp"
#include "chronos/contracts/state_lineage.hpp"
#include "chronos/core/market_state/l2_book.hpp"
#include "chronos/core/market_state/listing_aux_state.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
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
  SourceCutChanged,
  IdentityDerivationFailed,
  PriorViewPending,
  WrongView,
  WrongBoundary,
  InvalidPublicationTransition,
  PublicationHistoryExhausted,
};

struct ListingViewPublisherConfig final {
  contracts::RunId run_id;
  contracts::ListingId listing_id;
  std::vector<contracts::StreamId> required_streams;
  contracts::StreamId book_stream_id;
  contracts::StreamId trade_stream_id;
  contracts::StreamId trade_continuity_stream_id;
  contracts::StreamId reference_stream_id;
  contracts::StreamId market_control_stream_id;
  contracts::StreamId run_control_stream_id;
  contracts::StreamId run_timer_stream_id;
  contracts::ConsumerBoundaryId feature_boundary_id;
  contracts::VersionRef view_schema_version;
  contracts::VersionRef capability_version;
  contracts::VersionRef transition_policy_version;
  contracts::VersionRef arithmetic_version;
  contracts::VersionRef canonicalization_version;
  std::size_t maximum_publication_transitions{};
};

struct ListingViewCutInput final {
  contracts::RunInputSelectionId selection_id;
  contracts::EventId selected_event_id;
  contracts::StateLineage lineage;
};

struct ListingStateView final {
  contracts::StateViewId view_id;
  contracts::RunId run_id;
  contracts::ListingId listing_id;
  contracts::RunInputSelectionId causing_selection_id;
  contracts::EventId causing_event_id;
  contracts::StateLineage lineage;
  std::uint64_t l2_transition_sequence{};
  std::vector<L2Level> bids;
  std::vector<L2Level> asks;
  L2TopOfBook top;
  std::vector<RecentTrade> recent_trades;
  ListingQualityState quality;
  std::optional<contracts::StateViewId> prior_view_id;
  contracts::VersionRef view_schema_version;
  contracts::VersionRef capability_version;
  contracts::VersionRef transition_policy_version;
  contracts::VersionRef arithmetic_version;
  contracts::VersionRef canonicalization_version;
  contracts::Sha256Digest semantic_checksum;

  bool operator==(const ListingStateView &) const = default;
};

struct ViewPublicationTransition final {
  contracts::StateViewId view_id;
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
