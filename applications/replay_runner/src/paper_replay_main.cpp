#include "chronos/applications/replay_runner/paper_replay.hpp"

#include <iostream>
#include <string>

int main(int argc, char **argv) {
  namespace app = chronos::applications::replay_runner;
  if (argc == 3 && std::string(argv[1]) == "--generate-fixture") {
    std::string error;
    if (!app::write_synthetic_paper_replay_fixture(argv[2], error)) {
      std::cerr << error << '\n';
      return 1;
    }
    return 0;
  }
  if (argc != 2) {
    std::cerr << "usage: chronos_paper_replay DATASET_DIRECTORY\n"
                 "       chronos_paper_replay --generate-fixture DIRECTORY\n";
    return 2;
  }
  const auto result = app::run_paper_replay(argv[1]);
  std::cout << app::paper_replay_json(result) << '\n';
  return result.completed ? 0 : 1;
}
