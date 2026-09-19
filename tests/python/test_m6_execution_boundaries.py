"""Narrow execution consumer: no changes to the risk/portfolio mint allowlists."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def violations(root: Path) -> list[str]:
    owner = root / "core/execution_planning"
    expected = (
        "add_library(chronos_execution STATIC src/paper_intent.cpp) "
        "target_include_directories(chronos_execution PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include) "
        "target_link_libraries(chronos_execution PUBLIC chronos_reservation chronos_contracts "
        "PRIVATE chronos_options chronos_warnings)"
    )
    errors = []
    if " ".join((owner / "CMakeLists.txt").read_text().split()) != expected:
        errors.append("execution target shape")
    sources = {p.relative_to(owner).as_posix() for p in owner.rglob("*") if p.suffix == ".cpp"}
    if sources != {"src/paper_intent.cpp"}:
        errors.append("execution sources")
    for path in owner.rglob("*"):
        if path.suffix not in {".cpp", ".hpp"}:
            continue
        for line in path.read_text().splitlines():
            if not line.strip().startswith("#include") or '"' not in line:
                continue
            include = line.split('"')[1]
            if include not in {
                "chronos/contracts/paper_execution.hpp",
                "chronos/core/risk/reservation.hpp",
                "chronos/core/execution_planning/paper_intent.hpp",
            }:
                errors.append("execution include")
    neutral = root / "contracts/include/chronos/contracts/paper_execution.hpp"
    for line in neutral.read_text().splitlines():
        if line.strip().startswith("#include") and '"' in line:
            if not line.split('"')[1].startswith("chronos/contracts/"):
                errors.append("nonneutral paper contract")
    return errors


def test_execution_consumer_has_exact_dependency_boundary() -> None:
    assert violations(ROOT) == []


def test_execution_rejects_broker_or_ledger_dependency(tmp_path: Path) -> None:
    import shutil

    shutil.copytree(ROOT / "core/execution_planning", tmp_path / "core/execution_planning")
    target = tmp_path / "contracts/include/chronos/contracts"
    target.mkdir(parents=True)
    shutil.copy(ROOT / "contracts/include/chronos/contracts/paper_execution.hpp", target)
    owner = tmp_path / "core/execution_planning/CMakeLists.txt"
    owner.write_text(
        owner.read_text() + "\ntarget_link_libraries(chronos_execution PRIVATE chronos_ledger)\n"
    )
    assert "execution target shape" in violations(tmp_path)
    header = target / "paper_execution.hpp"
    header.write_text(header.read_text() + '#include "chronos/core/risk/reservation.hpp"\n')
    assert "nonneutral paper contract" in violations(tmp_path)
