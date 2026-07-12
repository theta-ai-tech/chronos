#pragma once

#include "chronos/adapters/market_data/bybit_websocket.hpp"
#include "chronos/adapters/sdk/adapter.hpp"
#include "chronos/adapters/sdk/source_event.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <set>

namespace chronos::adapters::market_data {

enum class RecoveryCause : std::uint8_t {
  TransportClosed,
  HeartbeatTimeout,
  FramingFailure,
  ResourceIsolation,
  OperatorRestart,
};

enum class RecoveryDisposition : std::uint8_t {
  ConnectedInitialOrigin,
  RecoveredNewSourceEpoch,
  PermanentFailure,
  AttemptsExhausted,
  ElapsedTimeExhausted,
  Cancelled,
};

enum class ContinuityProof : std::uint8_t { InitialOrigin, UnprovenNewOrigin };

struct RecoveryPolicy final {
  sdk::RetryPolicy retry;
  std::chrono::milliseconds maximum_elapsed{};
  std::chrono::milliseconds connection_timeout{};
  std::size_t maximum_message_bytes{};

  [[nodiscard]] bool valid() const noexcept;
};

struct SourceSessionHealth final {
  sdk::ConnectionState connection{sdk::ConnectionState::Configured};
  std::array<sdk::HealthState, 5> scopes{};
  std::uint64_t source_session_epoch{};
  std::optional<sdk::CaptureSessionId> capture_session_id;
  bool continuity_proven{};

  [[nodiscard]] sdk::HealthState at(sdk::HealthScope scope) const noexcept;
};

struct RecoveryRecord final {
  RecoveryDisposition disposition{RecoveryDisposition::PermanentFailure};
  RecoveryCause cause{RecoveryCause::TransportClosed};
  ContinuityProof proof{ContinuityProof::UnprovenNewOrigin};
  std::uint32_t attempts{};
  std::optional<std::uint64_t> previous_source_session_epoch;
  std::optional<std::uint64_t> new_source_session_epoch;
  std::optional<sdk::CaptureSessionId> new_capture_session_id;
  TransportFailure last_failure{TransportFailure::None};
};

class BybitSessionFactory {
public:
  struct Attempt final {
    std::uint64_t attempt_id{};
    std::uint64_t proposed_source_session_epoch{};
    sdk::CaptureSessionId capture_session_id;
    bool recovery{};
  };

  virtual ~BybitSessionFactory() = default;
  [[nodiscard]] virtual std::unique_ptr<BybitSession>
  create(const Attempt &attempt) = 0;
};

class CaptureSessionIdentitySource {
public:
  virtual ~CaptureSessionIdentitySource() = default;
  [[nodiscard]] virtual std::optional<sdk::CaptureSessionId> next() = 0;
};

class RecoveryScheduler {
public:
  virtual ~RecoveryScheduler() = default;
  [[nodiscard]] virtual std::chrono::steady_clock::time_point now() const = 0;
  virtual void sleep_for(std::chrono::milliseconds delay) = 0;
  [[nodiscard]] virtual bool cancelled() const noexcept = 0;
};

class JitterSource {
public:
  virtual ~JitterSource() = default;
  [[nodiscard]] virtual std::uint64_t next() noexcept = 0;
};

class BybitWebSocketSessionFactory final : public BybitSessionFactory {
public:
  using ObserverFactory =
      std::function<BybitWebSocketSession::MessageObserver(const Attempt &)>;
  explicit BybitWebSocketSessionFactory(ObserverFactory observer_factory = {});
  [[nodiscard]] std::unique_ptr<BybitSession>
  create(const Attempt &attempt) override;

private:
  ObserverFactory observer_factory_;
};

class SteadyRecoveryScheduler final : public RecoveryScheduler {
public:
  [[nodiscard]] std::chrono::steady_clock::time_point now() const override;
  void sleep_for(std::chrono::milliseconds delay) override;
  [[nodiscard]] bool cancelled() const noexcept override;
  void cancel() noexcept;

private:
  std::atomic_bool cancelled_{};
};

class RandomJitterSource final : public JitterSource {
public:
  RandomJitterSource();
  [[nodiscard]] std::uint64_t next() noexcept override;

private:
  std::mt19937_64 generator_;
};

class RandomCaptureSessionIdentitySource final
    : public CaptureSessionIdentitySource {
public:
  RandomCaptureSessionIdentitySource();
  [[nodiscard]] std::optional<sdk::CaptureSessionId> next() override;

private:
  std::mt19937_64 generator_;
};

class BybitReconnectController final {
public:
  [[nodiscard]] static std::optional<BybitReconnectController>
  create(BybitSubscription subscription, RecoveryPolicy policy,
         BybitSessionFactory &factory, RecoveryScheduler &scheduler,
         JitterSource &jitter,
         CaptureSessionIdentitySource &capture_session_identities);

  [[nodiscard]] RecoveryRecord start();
  [[nodiscard]] RecoveryRecord recover(RecoveryCause cause);
  void stop() noexcept;

  [[nodiscard]] const SourceSessionHealth &health() const noexcept;
  [[nodiscard]] BybitSession *session() noexcept;

private:
  BybitReconnectController(
      BybitSubscription subscription, RecoveryPolicy policy,
      BybitSessionFactory &factory, RecoveryScheduler &scheduler,
      JitterSource &jitter,
      CaptureSessionIdentitySource &capture_session_identities);

  [[nodiscard]] RecoveryRecord connect_with_policy(RecoveryCause cause,
                                                   bool initial);
  [[nodiscard]] std::chrono::milliseconds
  backoff(std::uint32_t attempt) noexcept;
  void set_all_health(sdk::HealthState state) noexcept;

  BybitSubscription subscription_;
  RecoveryPolicy policy_;
  BybitSessionFactory &factory_;
  RecoveryScheduler &scheduler_;
  JitterSource &jitter_;
  CaptureSessionIdentitySource &capture_session_identities_;
  std::unique_ptr<BybitSession> session_;
  SourceSessionHealth health_;
  std::uint64_t next_attempt_id_{1};
  std::set<sdk::CaptureSessionId> issued_capture_session_ids_;
};

[[nodiscard]] bool retryable(TransportFailure failure) noexcept;

} // namespace chronos::adapters::market_data
