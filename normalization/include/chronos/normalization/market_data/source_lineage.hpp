#pragma once

#include "chronos/adapters/sdk/source_event.hpp"
#include "chronos/contracts/value_objects.hpp"

#include <cstdint>
#include <optional>

namespace chronos::normalization::market_data {

enum class SourceTimestampUnit : std::uint8_t { Milliseconds };

struct SourceCaptureLineage final {
  contracts::SourceEventId source_event_id;
  adapters::sdk::CaptureSessionId capture_session_id;
  contracts::RuntimeId runtime_id;
  std::optional<adapters::sdk::SourceConnectionId> connection_id;
  std::optional<adapters::sdk::SourceSubscriptionId> subscription_id;
  contracts::CapturePartitionId capture_partition_id;
  std::uint64_t capture_sequence{};
  contracts::TimePoint chronos_receive_time;
  adapters::sdk::PayloadDigest payload_digest;

  bool operator==(const SourceCaptureLineage &) const = default;
};

} // namespace chronos::normalization::market_data
