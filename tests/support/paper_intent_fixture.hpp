#pragma once
#include "chronos/core/execution_planning/paper_intent.hpp"
#include "paper_risk_fixture.hpp"
#include <array>
namespace chronos::test_support::paper {
inline contracts::PaperExecutionEvidence execution_evidence() {
  const std::array streams{id<contracts::StreamId>(150)};
  const std::array cursors{
      *contracts::StreamCursor::at_sequence(streams[0], 1, 5)};
  const auto key = target_key();
  return {id<contracts::RunId>(30),
          key.portfolio_id(),
          key.account_id(),
          key.canonical_instrument_id(),
          key.listing_id(),
          version(120),
          contracts::RunMode::backtest,
          id<contracts::StateViewId>(151),
          *contracts::StateLineage::from(id<contracts::RunId>(30), 5, streams,
                                         cursors),
          5,
          120,
          160,
          valid_quality(),
          true,
          true,
          version(152),
          version(153),
          exposure_scale(),
          exposure_scale(),
          exposure_scale(),
          100,
          101,
          1,
          1};
}
inline risk::ReservationPolicy reservation_policy() {
  return {risk_policy(EvaluationSpec{}), version(120),
          id<contracts::StreamId>(121), 1, 100};
}
} // namespace chronos::test_support::paper
