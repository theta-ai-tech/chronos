#include "chronos/adapters/market_data/capture_dataset.hpp"

#include <filesystem>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  const auto result = chronos::adapters::market_data::read_capture_dataset(
      std::filesystem::path(argv[1]));
  if (!result.ok()) {
    std::cerr << "dataset read failed: " << static_cast<int>(result.failure())
              << '\n';
    return 1;
  }
  std::cout << "dataset_id=" << result.manifest().value().dataset_id
            << " records=" << result.records().size() << '\n';
  return 0;
}
