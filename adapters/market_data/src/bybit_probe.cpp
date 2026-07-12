#include "chronos/adapters/market_data/bybit_websocket.hpp"
#include "chronos/adapters/market_data/source_capture.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace {
namespace market_data = chronos::adapters::market_data;
namespace sdk = chronos::adapters::sdk;
namespace contracts = chronos::contracts;

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type bytes{};
  bytes.front() = seed;
  return *Id::from_bytes(bytes);
}

class ProbeIdentitySource final
    : public market_data::SourceEventIdentitySource {
public:
  std::optional<contracts::SourceEventId> next() override {
    contracts::SourceEventId::bytes_type bytes{};
    bytes.front() = 1;
    for (std::size_t index = 0; index < sizeof(next_); ++index) {
      bytes[bytes.size() - 1 - index] =
          static_cast<std::uint8_t>(next_ >> (index * 8U));
    }
    ++next_;
    return contracts::SourceEventId::from_bytes(bytes);
  }

private:
  std::uint64_t next_{1};
};

class ProbeConsumer final : public market_data::SourceEventConsumer {
public:
  bool accept(sdk::SourceEvent event) override {
    if (event.capture_sequence() != count_ + 1 ||
        event.payload_digest().coverage !=
            sdk::DigestCoverage::CompletePayload) {
      return false;
    }
    ++count_;
    return true;
  }

  [[nodiscard]] std::uint64_t count() const noexcept { return count_; }

private:
  std::uint64_t count_{};
};

std::string_view as_text(const market_data::WebSocketMessage &message) {
  return {reinterpret_cast<const char *>(message.payload.data()),
          message.payload.size()};
}
} // namespace

int main(int argc, char **argv) {
  using namespace std::chrono_literals;

  const std::string symbol = argc > 1 ? argv[1] : "BTCUSDT";
  const bool production = argc > 2 && std::string_view(argv[2]) == "production";
  market_data::BybitSubscription subscription{
      .environment = production ? sdk::EnvironmentClass::Production
                                : sdk::EnvironmentClass::Test,
      .market = sdk::MarketClass::LinearPerpetual,
      .symbol = symbol,
      .order_book_depth = 50};

  auto recorder = sdk::SourceCaptureRecorder::create(
      {.adapter_id = "chronos.bybit.public-market-data",
       .adapter_version = "m2.3",
       .build_version = "probe",
       .venue = "bybit",
       .environment = subscription.environment,
       .endpoint = sdk::EndpointClass::PublicMarketData,
       .trust_class = sdk::SourceTrustClass::PublicUnauthenticated,
       .capture_session_id = id<sdk::CaptureSessionId>(1),
       .runtime_id = id<contracts::RuntimeId>(2),
       .connection_id = id<sdk::SourceConnectionId>(3),
       .subscription_id = id<sdk::SourceSubscriptionId>(4),
       .capture_partition_id = id<sdk::CapturePartitionId>(5),
       .framing_version = "websocket-rfc6455-v1",
       .static_configuration_version = "probe-v1",
       .capability_manifest_version = "adapter-sdk-v1",
       .schema_policy_version = "bybit-v5-public-v1",
       .data_classification = sdk::DataClassification::PublicMarketData,
       .access_restriction = sdk::AccessRestriction::ChronosInternal,
       .maximum_retained_payload_bytes = 1U << 20U,
       .maximum_source_events = 1000});
  if (!recorder.has_value()) {
    std::cerr << "capture recorder configuration failed\n";
    return 1;
  }
  ProbeIdentitySource identities;
  ProbeConsumer consumer;
  market_data::WebSocketSourceCapture capture(std::move(*recorder),
                                              id<contracts::ClockDomainId>(6),
                                              identities, consumer);

  market_data::BybitWebSocketSession session(
      market_data::make_curl_websocket_transport(),
      [&capture](const market_data::WebSocketMessage &message) {
        return capture.capture(message);
      });
  const auto connected =
      session.connect_and_subscribe(subscription, 10s, 1U << 20U);
  if (!connected.ok()) {
    std::cerr << "connect/subscribe failed: " << connected.detail << '\n';
    return 1;
  }
  const auto heartbeat = session.send_heartbeat(5s);
  if (!heartbeat.ok()) {
    std::cerr << "heartbeat failed: " << heartbeat.detail << '\n';
    return 1;
  }

  bool saw_book = false;
  bool saw_trade = false;
  for (int index = 0; index < 100 && !(saw_book && saw_trade); ++index) {
    const auto received = session.receive(10s);
    if (!received.ok()) {
      std::cerr << "receive failed: " << received.detail << '\n';
      return 1;
    }
    if (received.value.kind != market_data::WebSocketMessageKind::Text) {
      continue;
    }
    const auto text = as_text(received.value);
    saw_book = saw_book ||
               text.find("\"topic\":\"orderbook.") != std::string_view::npos;
    saw_trade = saw_trade ||
                text.find("\"topic\":\"publicTrade.") != std::string_view::npos;
    std::cout << "text frame bytes=" << text.size()
              << " book=" << (saw_book ? "yes" : "no")
              << " trade=" << (saw_trade ? "yes" : "no") << '\n';
  }
  session.close();
  if (!saw_book || !saw_trade) {
    std::cerr << "did not observe both required topics\n";
    return 1;
  }
  if (consumer.count() < 3) {
    std::cerr << "expected captured acknowledgement, book, and trade events\n";
    return 1;
  }
  std::cout << "captured source events=" << consumer.count() << '\n';
  return 0;
}
