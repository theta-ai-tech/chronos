#include "chronos/adapters/market_data/reconnect.hpp"

#include <algorithm>
#include <limits>
#include <thread>
#include <utility>

namespace chronos::adapters::market_data {

bool RecoveryPolicy::valid() const noexcept {
  return retry.valid() && maximum_elapsed.count() > 0 &&
         connection_timeout.count() > 0 &&
         connection_timeout <= maximum_elapsed && maximum_message_bytes > 0;
}

sdk::HealthState
SourceSessionHealth::at(sdk::HealthScope scope) const noexcept {
  const auto index = static_cast<std::size_t>(scope);
  return index < scopes.size() ? scopes[index] : sdk::HealthState::Unknown;
}

bool retryable(TransportFailure failure) noexcept {
  switch (failure) {
  case TransportFailure::Resolve:
  case TransportFailure::Connect:
  case TransportFailure::Send:
  case TransportFailure::Receive:
  case TransportFailure::Timeout:
  case TransportFailure::Closed:
  case TransportFailure::MessageTooLarge:
  case TransportFailure::UnsupportedFrame:
    return true;
  case TransportFailure::None:
  case TransportFailure::InvalidConfiguration:
  case TransportFailure::Tls:
  case TransportFailure::Upgrade:
  case TransportFailure::Protocol:
  case TransportFailure::SubscriptionRejected:
  case TransportFailure::CaptureHandoff:
    return false;
  }
  return false;
}

BybitWebSocketSessionFactory::BybitWebSocketSessionFactory(
    ObserverFactory observer_factory)
    : observer_factory_(std::move(observer_factory)) {}

std::unique_ptr<BybitSession>
BybitWebSocketSessionFactory::create(const Attempt &attempt) {
  auto observer = observer_factory_ ? observer_factory_(attempt)
                                    : BybitWebSocketSession::MessageObserver{};
  return std::make_unique<BybitWebSocketSession>(
      make_curl_websocket_transport(), std::move(observer));
}

std::chrono::steady_clock::time_point SteadyRecoveryScheduler::now() const {
  return std::chrono::steady_clock::now();
}

void SteadyRecoveryScheduler::sleep_for(std::chrono::milliseconds delay) {
  constexpr auto polling_interval = std::chrono::milliseconds(25);
  while (delay.count() > 0 && !cancelled()) {
    const auto slice = std::min(delay, polling_interval);
    std::this_thread::sleep_for(slice);
    delay -= slice;
  }
}

bool SteadyRecoveryScheduler::cancelled() const noexcept {
  return cancelled_.load(std::memory_order_relaxed);
}

void SteadyRecoveryScheduler::cancel() noexcept {
  cancelled_.store(true, std::memory_order_relaxed);
}

RandomJitterSource::RandomJitterSource() : generator_(std::random_device{}()) {}

std::uint64_t RandomJitterSource::next() noexcept { return generator_(); }

RandomCaptureSessionIdentitySource::RandomCaptureSessionIdentitySource()
    : generator_(std::random_device{}()) {}

std::optional<sdk::CaptureSessionId>
RandomCaptureSessionIdentitySource::next() {
  sdk::CaptureSessionId::bytes_type bytes{};
  for (std::size_t block = 0; block < 2; ++block) {
    const auto random = generator_();
    for (std::size_t index = 0; index < 8; ++index) {
      bytes[block * 8 + index] =
          static_cast<std::uint8_t>(random >> (index * 8U));
    }
  }
  bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0FU) | 0x40U);
  bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3FU) | 0x80U);
  return sdk::CaptureSessionId::from_bytes(bytes);
}

std::optional<BybitReconnectController> BybitReconnectController::create(
    BybitSubscription subscription, RecoveryPolicy policy,
    BybitSessionFactory &factory, RecoveryScheduler &scheduler,
    JitterSource &jitter,
    CaptureSessionIdentitySource &capture_session_identities) {
  if (!valid_bybit_subscription(subscription) || !policy.valid()) {
    return std::nullopt;
  }
  return BybitReconnectController(std::move(subscription), policy, factory,
                                  scheduler, jitter,
                                  capture_session_identities);
}

BybitReconnectController::BybitReconnectController(
    BybitSubscription subscription, RecoveryPolicy policy,
    BybitSessionFactory &factory, RecoveryScheduler &scheduler,
    JitterSource &jitter,
    CaptureSessionIdentitySource &capture_session_identities)
    : subscription_(std::move(subscription)), policy_(policy),
      factory_(factory), scheduler_(scheduler), jitter_(jitter),
      capture_session_identities_(capture_session_identities) {
  set_all_health(sdk::HealthState::Unknown);
}

RecoveryRecord BybitReconnectController::start() {
  return connect_with_policy(RecoveryCause::OperatorRestart, true);
}

RecoveryRecord BybitReconnectController::recover(RecoveryCause cause) {
  const auto previous_epoch = health_.source_session_epoch;
  if (session_) {
    session_->close();
    session_.reset();
  }
  health_.capture_session_id.reset();
  health_.connection = sdk::ConnectionState::ReconnectWait;
  set_all_health(sdk::HealthState::Recovering);
  health_.scopes[static_cast<std::size_t>(sdk::HealthScope::Continuity)] =
      sdk::HealthState::Gapped;
  health_.continuity_proven = false;
  auto record = connect_with_policy(cause, false);
  if (previous_epoch != 0) {
    record.previous_source_session_epoch = previous_epoch;
  }
  return record;
}

RecoveryRecord
BybitReconnectController::connect_with_policy(RecoveryCause cause,
                                              bool initial) {
  const auto started = scheduler_.now();
  const auto deadline = started + policy_.maximum_elapsed;
  TransportFailure last_failure = TransportFailure::None;
  for (std::uint32_t attempt = 1; attempt <= policy_.retry.maximum_attempts;
       ++attempt) {
    if (scheduler_.cancelled()) {
      health_.connection = sdk::ConnectionState::Closed;
      set_all_health(sdk::HealthState::Unknown);
      health_.capture_session_id.reset();
      return {.disposition = RecoveryDisposition::Cancelled,
              .cause = cause,
              .attempts = attempt - 1,
              .last_failure = last_failure};
    }
    if (next_attempt_id_ == std::numeric_limits<std::uint64_t>::max() ||
        health_.source_session_epoch ==
            std::numeric_limits<std::uint64_t>::max()) {
      health_.connection = sdk::ConnectionState::Failed;
      set_all_health(sdk::HealthState::Failed);
      return {.disposition = RecoveryDisposition::PermanentFailure,
              .cause = cause,
              .attempts = attempt,
              .last_failure = TransportFailure::Protocol};
    }
    const auto capture_session_id = capture_session_identities_.next();
    if (!capture_session_id.has_value() ||
        issued_capture_session_ids_.contains(*capture_session_id)) {
      health_.connection = sdk::ConnectionState::Failed;
      set_all_health(sdk::HealthState::Failed);
      return {.disposition = RecoveryDisposition::PermanentFailure,
              .cause = cause,
              .attempts = attempt,
              .last_failure = TransportFailure::InvalidConfiguration};
    }
    issued_capture_session_ids_.insert(*capture_session_id);
    const BybitSessionFactory::Attempt session_attempt{
        .attempt_id = next_attempt_id_++,
        .proposed_source_session_epoch = health_.source_session_epoch + 1,
        .capture_session_id = *capture_session_id,
        .recovery = !initial};
    session_ = factory_.create(session_attempt);
    if (!session_) {
      health_.connection = sdk::ConnectionState::Failed;
      set_all_health(sdk::HealthState::Failed);
      return {.disposition = RecoveryDisposition::PermanentFailure,
              .cause = cause,
              .attempts = attempt,
              .last_failure = TransportFailure::InvalidConfiguration};
    }
    health_.connection = sdk::ConnectionState::Connecting;
    set_all_health(sdk::HealthState::Starting);
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                              scheduler_.now());
    if (remaining.count() <= 0) {
      session_.reset();
      health_.connection = sdk::ConnectionState::Failed;
      set_all_health(sdk::HealthState::Failed);
      health_.capture_session_id.reset();
      return {.disposition = RecoveryDisposition::ElapsedTimeExhausted,
              .cause = cause,
              .attempts = attempt - 1,
              .last_failure = last_failure};
    }
    auto connected = session_->connect_and_subscribe(
        subscription_, std::min(policy_.connection_timeout, remaining),
        policy_.maximum_message_bytes,
        [this] { return scheduler_.cancelled(); });
    if (scheduler_.cancelled()) {
      session_->close();
      session_.reset();
      health_.connection = sdk::ConnectionState::Closed;
      set_all_health(sdk::HealthState::Unknown);
      health_.capture_session_id.reset();
      return {.disposition = RecoveryDisposition::Cancelled,
              .cause = cause,
              .attempts = attempt,
              .last_failure = connected.failure};
    }
    if (connected.ok()) {
      ++health_.source_session_epoch;
      health_.capture_session_id = *capture_session_id;
      // An acknowledgement proves transport/subscription setup, not source
      // readiness. M3 promotes the session after qualifying channel evidence.
      health_.connection = sdk::ConnectionState::Connecting;
      set_all_health(sdk::HealthState::Starting);
      health_.scopes[static_cast<std::size_t>(sdk::HealthScope::Transport)] =
          sdk::HealthState::Healthy;
      health_.scopes[static_cast<std::size_t>(sdk::HealthScope::Subscription)] =
          sdk::HealthState::Starting;
      health_.scopes[static_cast<std::size_t>(sdk::HealthScope::Capture)] =
          session_->capture_enabled() ? sdk::HealthState::Healthy
                                      : sdk::HealthState::Unknown;
      health_.scopes[static_cast<std::size_t>(sdk::HealthScope::Continuity)] =
          initial ? sdk::HealthState::Starting : sdk::HealthState::Gapped;
      health_.continuity_proven = false;
      return {.disposition = initial
                                 ? RecoveryDisposition::ConnectedInitialOrigin
                                 : RecoveryDisposition::RecoveredNewSourceEpoch,
              .cause = cause,
              .proof = initial ? ContinuityProof::InitialOrigin
                               : ContinuityProof::UnprovenNewOrigin,
              .attempts = attempt,
              .new_source_session_epoch = health_.source_session_epoch,
              .new_capture_session_id = *capture_session_id,
              .last_failure = TransportFailure::None};
    }

    last_failure = connected.failure;
    session_->close();
    session_.reset();
    health_.connection = sdk::ConnectionState::ReconnectWait;
    set_all_health(sdk::HealthState::Degraded);
    health_.scopes[static_cast<std::size_t>(sdk::HealthScope::Continuity)] =
        sdk::HealthState::Gapped;
    if (!retryable(last_failure)) {
      health_.connection = sdk::ConnectionState::Failed;
      set_all_health(sdk::HealthState::Failed);
      return {.disposition = RecoveryDisposition::PermanentFailure,
              .cause = cause,
              .attempts = attempt,
              .last_failure = last_failure};
    }
    if (attempt == policy_.retry.maximum_attempts) {
      health_.connection = sdk::ConnectionState::Failed;
      set_all_health(sdk::HealthState::Failed);
      health_.capture_session_id.reset();
      return {.disposition = RecoveryDisposition::AttemptsExhausted,
              .cause = cause,
              .attempts = attempt,
              .last_failure = last_failure};
    }
    const auto delay = backoff(attempt);
    if (scheduler_.now() - started + delay > policy_.maximum_elapsed) {
      health_.connection = sdk::ConnectionState::Failed;
      set_all_health(sdk::HealthState::Failed);
      health_.capture_session_id.reset();
      return {.disposition = RecoveryDisposition::ElapsedTimeExhausted,
              .cause = cause,
              .attempts = attempt,
              .last_failure = last_failure};
    }
    scheduler_.sleep_for(delay);
    if (scheduler_.cancelled()) {
      health_.connection = sdk::ConnectionState::Closed;
      set_all_health(sdk::HealthState::Unknown);
      health_.capture_session_id.reset();
      return {.disposition = RecoveryDisposition::Cancelled,
              .cause = cause,
              .attempts = attempt,
              .last_failure = last_failure};
    }
  }
  return {.disposition = RecoveryDisposition::AttemptsExhausted,
          .cause = cause,
          .attempts = policy_.retry.maximum_attempts,
          .last_failure = last_failure};
}

std::chrono::milliseconds
BybitReconnectController::backoff(std::uint32_t attempt) noexcept {
  auto delay = policy_.retry.initial_delay;
  for (std::uint32_t index = 1; index < attempt; ++index) {
    if (delay >= policy_.retry.maximum_delay / 2) {
      delay = policy_.retry.maximum_delay;
      break;
    }
    delay *= 2;
  }
  delay = std::min(delay, policy_.retry.maximum_delay);
  const auto lower = delay.count() / 2;
  const auto range = delay.count() - lower + 1;
  const auto jitter = static_cast<std::int64_t>(
      jitter_.next() % static_cast<std::uint64_t>(range));
  return std::chrono::milliseconds(lower + jitter);
}

void BybitReconnectController::stop() noexcept {
  if (session_) {
    session_->close();
    session_.reset();
  }
  health_.connection = sdk::ConnectionState::Closed;
  set_all_health(sdk::HealthState::Unknown);
  health_.continuity_proven = false;
  health_.capture_session_id.reset();
}

const SourceSessionHealth &BybitReconnectController::health() const noexcept {
  return health_;
}

BybitSession *BybitReconnectController::session() noexcept {
  return session_.get();
}

void BybitReconnectController::set_all_health(sdk::HealthState state) noexcept {
  health_.scopes.fill(state);
}

} // namespace chronos::adapters::market_data
