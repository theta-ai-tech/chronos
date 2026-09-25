"""Paper simulation depends only on neutral contracts."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_broker_contracts_only() -> None:
    owner = ROOT / "adapters/paper"
    cmake = " ".join((owner / "CMakeLists.txt").read_text().split())
    assert cmake == (
        "add_library(chronos_paper_broker STATIC src/paper_broker.cpp) "
        "target_include_directories(chronos_paper_broker PUBLIC "
        "${CMAKE_CURRENT_SOURCE_DIR}/include) "
        "target_link_libraries(chronos_paper_broker PUBLIC chronos_contracts chronos_options "
        "PRIVATE chronos_warnings)"
    )
    for path in owner.rglob("*"):
        if path.suffix not in {".hpp", ".cpp"}:
            continue
        for line in path.read_text().splitlines():
            if line.startswith('#include "'):
                assert line.split('"')[1] in {
                    "chronos/contracts/paper_execution.hpp",
                    "chronos/adapters/paper/paper_broker.hpp",
                }
