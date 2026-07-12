#pragma once

#include "chronos/adapters/market_data/websocket_transport.hpp"
#include "chronos/adapters/sdk/source_event.hpp"

namespace chronos::adapters::market_data {

class SourceEventIdentitySource {
public:
  virtual ~SourceEventIdentitySource() = default;
  [[nodiscard]] virtual std::optional<contracts::SourceEventId> next() = 0;
};

class SourceEventConsumer {
public:
  virtual ~SourceEventConsumer() = default;
  [[nodiscard]] virtual bool accept(const sdk::SourceEvent &event) = 0;
};

[[nodiscard]] sdk::CaptureResult
capture_websocket_message(sdk::SourceCaptureRecorder &recorder,
                          contracts::SourceEventId source_event_id,
                          contracts::ClockDomainId monotonic_clock_domain_id,
                          const WebSocketMessage &message);

class WebSocketSourceCapture final {
public:
  WebSocketSourceCapture(sdk::SourceCaptureRecorder recorder,
                         contracts::ClockDomainId monotonic_clock_domain_id,
                         SourceEventIdentitySource &identity_source,
                         SourceEventConsumer &consumer);

  [[nodiscard]] bool capture(const WebSocketMessage &message);
  [[nodiscard]] bool retry_pending();
  [[nodiscard]] bool has_pending() const noexcept;
  [[nodiscard]] sdk::CaptureFailure last_failure() const noexcept;

private:
  sdk::SourceCaptureRecorder recorder_;
  contracts::ClockDomainId monotonic_clock_domain_id_;
  SourceEventIdentitySource &identity_source_;
  SourceEventConsumer &consumer_;
  std::optional<sdk::SourceEvent> pending_event_;
  sdk::CaptureFailure last_failure_{sdk::CaptureFailure::None};
};

} // namespace chronos::adapters::market_data
