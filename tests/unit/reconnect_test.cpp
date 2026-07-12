#include "chronos/adapters/market_data/reconnect.hpp"

#include "microtest.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

namespace {
namespace market_data = chronos::adapters::market_data;
namespace sdk = chronos::adapters::sdk;
using namespace std::chrono_literals;

class FakeSession final : public market_data::BybitSession {
public:
  FakeSession(market_data::TransportFailure failure, bool capture_enabled)
      : failure_(failure), capture_enabled_(capture_enabled) {}

  market_data::TransportResult<bool>
  connect_and_subscribe(const market_data::BybitSubscription &,
                        std::chrono::milliseconds timeout, std::size_t,
                        const std::function<bool()> &cancelled = {}) override {
    ++connect_calls;
    timeouts.push_back(timeout);
    if (cancelled && cancelled()) {
      return {.failure = market_data::TransportFailure::Closed};
    }
    return failure_ == market_data::TransportFailure::None
               ? market_data::TransportResult<bool>{.value = true}
               : market_data::TransportResult<bool>{.failure = failure_,
                                                    .detail = "injected"};
  }

  market_data::TransportResult<market_data::WebSocketMessage>
  receive(std::chrono::milliseconds) override {
    return {.failure = market_data::TransportFailure::Timeout};
  }

  market_data::TransportResult<std::size_t>
  send_heartbeat(std::chrono::milliseconds) override {
    return {.value = 13};
  }

  bool capture_enabled() const noexcept override { return capture_enabled_; }

  void close() noexcept override { closed = true; }

  int connect_calls{};
  std::vector<std::chrono::milliseconds> timeouts;
  bool closed{};

private:
  market_data::TransportFailure failure_;
  bool capture_enabled_{};
};

class FakeFactory final : public market_data::BybitSessionFactory {
public:
  std::unique_ptr<market_data::BybitSession>
  create(const Attempt &attempt) override {
    ++created;
    attempts.push_back(attempt);
    if (outcomes.empty()) {
      return {};
    }
    const auto outcome = outcomes.front();
    outcomes.pop_front();
    return std::make_unique<FakeSession>(outcome, capture_enabled);
  }

  std::deque<market_data::TransportFailure> outcomes;
  std::vector<Attempt> attempts;
  int created{};
  bool capture_enabled{true};
};

class FakeScheduler final : public market_data::RecoveryScheduler {
public:
  std::chrono::steady_clock::time_point now() const override { return now_; }
  void sleep_for(std::chrono::milliseconds delay) override {
    sleeps.push_back(delay);
    now_ += delay;
    if (cancel_on_sleep_)
      cancelled_ = true;
  }
  bool cancelled() const noexcept override { return cancelled_; }

  std::chrono::steady_clock::time_point now_{};
  std::vector<std::chrono::milliseconds> sleeps;
  bool cancelled_{};
  bool cancel_on_sleep_{};
};

class ZeroJitter final : public market_data::JitterSource {
public:
  std::uint64_t next() noexcept override { return 0; }
};

class SequentialCaptureSessions final
    : public market_data::CaptureSessionIdentitySource {
public:
  std::optional<sdk::CaptureSessionId> next() override {
    sdk::CaptureSessionId::bytes_type bytes{};
    bytes.front() = next_++;
    return sdk::CaptureSessionId::from_bytes(bytes);
  }

private:
  std::uint8_t next_{1};
};

class ConstantCaptureSession final
    : public market_data::CaptureSessionIdentitySource {
public:
  std::optional<sdk::CaptureSessionId> next() override {
    sdk::CaptureSessionId::bytes_type bytes{};
    bytes.front() = 9;
    return sdk::CaptureSessionId::from_bytes(bytes);
  }
};

market_data::BybitSubscription subscription() {
  return {.environment = sdk::EnvironmentClass::Test,
          .market = sdk::MarketClass::LinearPerpetual,
          .symbol = "BTCUSDT",
          .order_book_depth = 50};
}

market_data::RecoveryPolicy policy() {
  return {.retry = {.maximum_attempts = 3,
                    .initial_delay = 100ms,
                    .maximum_delay = 1s},
          .maximum_elapsed = 10s,
          .connection_timeout = 2s,
          .maximum_message_bytes = 1U << 20U};
}
} // namespace

TEST_CASE("initial connection opens one source-session epoch") {
  FakeFactory factory;
  factory.outcomes = {market_data::TransportFailure::None};
  FakeScheduler scheduler;
  ZeroJitter jitter;
  SequentialCaptureSessions identities;
  auto controller = market_data::BybitReconnectController::create(
      subscription(), policy(), factory, scheduler, jitter, identities);
  CHECK(controller.has_value());
  const auto record = controller->start();
  CHECK(record.disposition ==
        market_data::RecoveryDisposition::ConnectedInitialOrigin);
  CHECK(record.new_source_session_epoch == 1);
  CHECK(controller->health().source_session_epoch == 1);
  CHECK(controller->health().at(sdk::HealthScope::Transport) ==
        sdk::HealthState::Healthy);
  CHECK(controller->health().at(sdk::HealthScope::SourceSession) ==
        sdk::HealthState::Starting);
  CHECK(controller->health().at(sdk::HealthScope::Continuity) ==
        sdk::HealthState::Starting);
  CHECK(!controller->health().continuity_proven);
}

TEST_CASE("unproven reconnect always establishes a new source epoch") {
  FakeFactory factory;
  factory.outcomes = {market_data::TransportFailure::None,
                      market_data::TransportFailure::None};
  FakeScheduler scheduler;
  ZeroJitter jitter;
  SequentialCaptureSessions identities;
  auto controller = market_data::BybitReconnectController::create(
      subscription(), policy(), factory, scheduler, jitter, identities);
  CHECK(controller->start().new_source_session_epoch == 1);
  const auto recovered =
      controller->recover(market_data::RecoveryCause::TransportClosed);
  CHECK(recovered.disposition ==
        market_data::RecoveryDisposition::RecoveredNewSourceEpoch);
  CHECK(recovered.previous_source_session_epoch == 1);
  CHECK(recovered.new_source_session_epoch == 2);
  CHECK(recovered.proof == market_data::ContinuityProof::UnprovenNewOrigin);
  CHECK(controller->health().at(sdk::HealthScope::Continuity) ==
        sdk::HealthState::Gapped);
  CHECK(!controller->health().continuity_proven);
  CHECK(factory.created == 2);
  CHECK(factory.attempts[0].capture_session_id !=
        factory.attempts[1].capture_session_id);
  CHECK(factory.attempts[0].proposed_source_session_epoch == 1);
  CHECK(factory.attempts[1].proposed_source_session_epoch == 2);
  CHECK(!factory.attempts[0].recovery);
  CHECK(factory.attempts[1].recovery);
}

TEST_CASE("retryable failures use bounded jittered exponential backoff") {
  FakeFactory factory;
  factory.outcomes = {market_data::TransportFailure::Timeout,
                      market_data::TransportFailure::Connect,
                      market_data::TransportFailure::None};
  FakeScheduler scheduler;
  ZeroJitter jitter;
  SequentialCaptureSessions identities;
  auto controller = market_data::BybitReconnectController::create(
      subscription(), policy(), factory, scheduler, jitter, identities);
  const auto record = controller->start();
  CHECK(record.disposition ==
        market_data::RecoveryDisposition::ConnectedInitialOrigin);
  CHECK(record.attempts == 3);
  CHECK(scheduler.sleeps.size() == 2);
  CHECK(scheduler.sleeps[0] == 50ms);
  CHECK(scheduler.sleeps[1] == 100ms);
  CHECK(factory.attempts[0].capture_session_id !=
        factory.attempts[1].capture_session_id);
  CHECK(factory.attempts[1].capture_session_id !=
        factory.attempts[2].capture_session_id);
  CHECK(factory.attempts[0].proposed_source_session_epoch == 1);
  CHECK(factory.attempts[2].proposed_source_session_epoch == 1);
}

TEST_CASE("permanent subscription failure does not enter retry loop") {
  FakeFactory factory;
  factory.outcomes = {market_data::TransportFailure::SubscriptionRejected,
                      market_data::TransportFailure::None};
  FakeScheduler scheduler;
  ZeroJitter jitter;
  SequentialCaptureSessions identities;
  auto controller = market_data::BybitReconnectController::create(
      subscription(), policy(), factory, scheduler, jitter, identities);
  const auto record = controller->start();
  CHECK(record.disposition ==
        market_data::RecoveryDisposition::PermanentFailure);
  CHECK(record.attempts == 1);
  CHECK(factory.created == 1);
  CHECK(scheduler.sleeps.empty());
}

TEST_CASE("attempt, elapsed, and cancellation bounds are explicit") {
  FakeFactory attempts_factory;
  attempts_factory.outcomes = {market_data::TransportFailure::Timeout,
                               market_data::TransportFailure::Timeout,
                               market_data::TransportFailure::Timeout};
  FakeScheduler attempts_scheduler;
  ZeroJitter jitter;
  SequentialCaptureSessions identities;
  auto attempts = market_data::BybitReconnectController::create(
      subscription(), policy(), attempts_factory, attempts_scheduler, jitter,
      identities);
  CHECK(attempts->start().disposition ==
        market_data::RecoveryDisposition::AttemptsExhausted);
  CHECK(attempts->health().connection == sdk::ConnectionState::Failed);
  CHECK(!attempts->health().capture_session_id.has_value());

  auto short_policy = policy();
  short_policy.maximum_elapsed = 40ms;
  short_policy.connection_timeout = 20ms;
  FakeFactory elapsed_factory;
  elapsed_factory.outcomes = {market_data::TransportFailure::Timeout,
                              market_data::TransportFailure::None};
  FakeScheduler elapsed_scheduler;
  auto elapsed = market_data::BybitReconnectController::create(
      subscription(), short_policy, elapsed_factory, elapsed_scheduler, jitter,
      identities);
  CHECK(elapsed->start().disposition ==
        market_data::RecoveryDisposition::ElapsedTimeExhausted);
  CHECK(elapsed_factory.created == 1);

  FakeFactory cancelled_factory;
  cancelled_factory.outcomes = {market_data::TransportFailure::None};
  FakeScheduler cancelled_scheduler;
  cancelled_scheduler.cancelled_ = true;
  auto cancelled = market_data::BybitReconnectController::create(
      subscription(), policy(), cancelled_factory, cancelled_scheduler, jitter,
      identities);
  CHECK(cancelled->start().disposition ==
        market_data::RecoveryDisposition::Cancelled);
  CHECK(cancelled_factory.created == 0);

  FakeFactory backoff_factory;
  backoff_factory.outcomes = {market_data::TransportFailure::Timeout,
                              market_data::TransportFailure::None};
  FakeScheduler backoff_scheduler;
  backoff_scheduler.cancel_on_sleep_ = true;
  auto backoff_cancelled = market_data::BybitReconnectController::create(
      subscription(), policy(), backoff_factory, backoff_scheduler, jitter,
      identities);
  CHECK(backoff_cancelled->start().disposition ==
        market_data::RecoveryDisposition::Cancelled);
  CHECK(backoff_factory.created == 1);
}

TEST_CASE("capture health is never inferred from transport readiness") {
  FakeFactory factory;
  factory.capture_enabled = false;
  factory.outcomes = {market_data::TransportFailure::None};
  FakeScheduler scheduler;
  ZeroJitter jitter;
  SequentialCaptureSessions identities;
  auto controller = market_data::BybitReconnectController::create(
      subscription(), policy(), factory, scheduler, jitter, identities);
  CHECK(controller->start().disposition ==
        market_data::RecoveryDisposition::ConnectedInitialOrigin);
  CHECK(controller->health().at(sdk::HealthScope::Transport) ==
        sdk::HealthState::Healthy);
  CHECK(controller->health().at(sdk::HealthScope::Capture) ==
        sdk::HealthState::Unknown);
}

TEST_CASE("capture-session identity collision fails closed") {
  FakeFactory factory;
  factory.outcomes = {market_data::TransportFailure::Timeout,
                      market_data::TransportFailure::None};
  FakeScheduler scheduler;
  ZeroJitter jitter;
  ConstantCaptureSession identities;
  auto controller = market_data::BybitReconnectController::create(
      subscription(), policy(), factory, scheduler, jitter, identities);
  const auto result = controller->start();
  CHECK(result.disposition ==
        market_data::RecoveryDisposition::PermanentFailure);
  CHECK(result.last_failure ==
        market_data::TransportFailure::InvalidConfiguration);
  CHECK(factory.created == 1);
}
