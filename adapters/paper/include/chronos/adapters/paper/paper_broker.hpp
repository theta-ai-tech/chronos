#pragma once
#include "chronos/contracts/paper_execution.hpp"
#include <vector>
namespace chronos::adapters::paper {
// Single-writer D0 synchronous full-fill model. Latency changes timestamps
// only; the fill uses the supplied cut, never an implied future book.
class PaperBroker final {
public:
  PaperBroker() = default;
  PaperBroker(const PaperBroker &) = delete;
  PaperBroker &operator=(const PaperBroker &) = delete;
  [[nodiscard]] contracts::PaperBrokerResult
  submit(const contracts::PaperIntent &intent,
         const contracts::PaperExecutionEvidence &market,
         const contracts::PaperBrokerPolicy &policy);

private:
  struct Record {
    contracts::PaperIntent intent;
    contracts::PaperExecutionEvidence market;
    contracts::PaperBrokerPolicy policy;
    contracts::PaperBrokerResult result;
  };
  std::vector<Record> records_;
};
} // namespace chronos::adapters::paper
