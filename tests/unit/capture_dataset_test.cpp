#include "chronos/adapters/market_data/capture_dataset.hpp"

#include "microtest.hpp"

#include <filesystem>
#include <fstream>
#include <string_view>

namespace {
namespace market_data = chronos::adapters::market_data;
namespace sdk = chronos::adapters::sdk;
namespace contracts = chronos::contracts;

template <typename Id> Id id(std::uint8_t seed) {
  typename Id::bytes_type bytes{};
  bytes.front() = seed;
  return *Id::from_bytes(bytes);
}

std::vector<std::byte> bytes(std::string_view value) {
  const auto *begin = reinterpret_cast<const std::byte *>(value.data());
  return {begin, begin + value.size()};
}

sdk::SourceCaptureContext context() {
  return {.adapter_id = "chronos.bybit.public-market-data",
          .adapter_version = "m2.5",
          .build_version = "test",
          .venue = "bybit",
          .environment = sdk::EnvironmentClass::Test,
          .endpoint = sdk::EndpointClass::PublicMarketData,
          .trust_class = sdk::SourceTrustClass::PublicUnauthenticated,
          .capture_session_id = id<sdk::CaptureSessionId>(1),
          .runtime_id = id<contracts::RuntimeId>(2),
          .connection_id = id<sdk::SourceConnectionId>(3),
          .subscription_id = id<sdk::SourceSubscriptionId>(4),
          .capture_partition_id = id<sdk::CapturePartitionId>(5),
          .framing_version = "websocket-rfc6455-v1",
          .static_configuration_version = "capture-test-v1",
          .capability_manifest_version = "adapter-sdk-v1",
          .schema_policy_version = "bybit-v5-public-v1",
          .data_classification = sdk::DataClassification::PublicMarketData,
          .access_restriction = sdk::AccessRestriction::ChronosInternal,
          .maximum_retained_payload_bytes = 4096,
          .maximum_source_events = 10};
}

sdk::SourceCaptureInput input(std::uint8_t seed,
                              std::span<const std::byte> payload) {
  return {.source_event_id = id<contracts::SourceEventId>(seed),
          .chronos_receive_time = *contracts::TimePoint::from(
              100 + seed, id<contracts::ClockDomainId>(6),
              contracts::ClockClass::monotonic, 1),
          .raw_payload = payload,
          .framing_protocol = sdk::FramingProtocol::WebSocket,
          .frame_kind = sdk::SourceFrameKind::Text,
          .framing_status = sdk::FramingStatus::Complete,
          .integrity_status = sdk::CaptureIntegrityStatus::Complete,
          .content_encoding = sdk::ContentEncoding::Utf8Text,
          .compression_disposition =
              sdk::CompressionDisposition::NotCompressed};
}

std::filesystem::path temporary_dataset(std::string_view name) {
  auto path =
      std::filesystem::temp_directory_path() / ("chronos-" + std::string(name));
  std::filesystem::remove_all(path);
  std::filesystem::remove_all(path.string() + ".partial");
  return path;
}

class RecordingPersistence final : public market_data::DatasetPersistence {
public:
  bool sync_file(const std::filesystem::path &) override {
    calls.push_back("file");
    return calls.size() != fail_at;
  }
  bool sync_directory(const std::filesystem::path &) override {
    calls.push_back("directory");
    return calls.size() != fail_at;
  }
  bool publish_directory(const std::filesystem::path &staging,
                         const std::filesystem::path &destination) override {
    calls.push_back("publish");
    if (calls.size() == fail_at)
      return false;
    std::error_code error;
    std::filesystem::rename(staging, destination, error);
    return !error;
  }

  std::size_t fail_at{};
  std::vector<std::string> calls;
};
} // namespace

TEST_CASE("capture dataset seals and rereads deterministically") {
  const auto path = temporary_dataset("dataset-roundtrip");
  auto recorder = sdk::SourceCaptureRecorder::create(context());
  auto writer = market_data::CaptureDatasetWriter::create(path, context());
  CHECK(writer.has_value());
  const auto one = bytes("one");
  const auto two = bytes("two");
  const auto first = recorder->capture(input(10, one));
  const auto second = recorder->capture(input(11, two));
  CHECK(writer->append(*first.event) == market_data::DatasetFailure::None);
  CHECK(writer->append(*second.event) == market_data::DatasetFailure::None);
  const auto sealed = writer->seal();
  CHECK(sealed.manifest.has_value());
  CHECK(sealed.manifest->record_count == 2);
  const auto read_once = market_data::read_capture_dataset(path);
  const auto read_twice = market_data::read_capture_dataset(path);
  CHECK(read_once.ok());
  CHECK(read_twice.ok());
  CHECK(read_once.manifest->dataset_id == sealed.manifest->dataset_id);
  CHECK(read_once.records == read_twice.records);
  CHECK(read_once.records[0].raw_payload == one);
  CHECK(read_once.records[1].capture_sequence == 2);
  std::filesystem::remove_all(path);
}

TEST_CASE("capture dataset rejects sequence gaps and corruption") {
  const auto path = temporary_dataset("dataset-corrupt");
  auto recorder = sdk::SourceCaptureRecorder::create(context());
  auto writer = market_data::CaptureDatasetWriter::create(path, context());
  const auto payload = bytes("evidence");
  const auto first = recorder->capture(input(20, payload));
  const auto second = recorder->capture(input(21, payload));
  CHECK(writer->append(*second.event) ==
        market_data::DatasetFailure::SequenceMismatch);
  CHECK(writer->append(*first.event) == market_data::DatasetFailure::None);
  CHECK(writer->seal().manifest.has_value());
  std::fstream records(path / "records.bin",
                       std::ios::in | std::ios::out | std::ios::binary);
  records.seekp(-1, std::ios::end);
  records.put('x');
  records.close();
  CHECK(market_data::read_capture_dataset(path).failure ==
        market_data::DatasetFailure::IntegrityMismatch);
  std::filesystem::remove_all(path);
}

TEST_CASE("capture dataset publication follows the crash-safe sync order") {
  const auto path = temporary_dataset("dataset-publication");
  auto recorder = sdk::SourceCaptureRecorder::create(context());
  const auto payload = bytes("durable evidence");
  const auto event = recorder->capture(input(30, payload));
  RecordingPersistence persistence;
  auto writer =
      market_data::CaptureDatasetWriter::create(path, context(), &persistence);
  CHECK(writer->append(*event.event) == market_data::DatasetFailure::None);
  CHECK(writer->seal().manifest.has_value());
  const std::vector<std::string> expected_calls{"file", "file", "directory",
                                                "publish", "directory"};
  CHECK(persistence.calls == expected_calls);
  std::filesystem::remove_all(path);

  for (std::size_t fail_at = 1; fail_at <= 4; ++fail_at) {
    const auto failed_path = temporary_dataset("dataset-publication-fail-" +
                                               std::to_string(fail_at));
    RecordingPersistence failing;
    failing.fail_at = fail_at;
    auto failed_writer = market_data::CaptureDatasetWriter::create(
        failed_path, context(), &failing);
    CHECK(failed_writer->append(*event.event) ==
          market_data::DatasetFailure::None);
    CHECK(failed_writer->seal().failure == market_data::DatasetFailure::Io);
    CHECK(!std::filesystem::exists(failed_path));
  }
}
