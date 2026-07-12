"""Build Python artifacts and verify their contents are intentional."""

import subprocess
import tarfile
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

EXPECTED_SDIST_PATHS = {
    ".gitignore",
    "README.md",
    "contracts/conformance/fixtures/m1-full-frame.hex",
    "PKG-INFO",
    "pyproject.toml",
    "python/chronos/__init__.py",
    "python/chronos/boundary.py",
    "python/chronos/contracts.py",
    "python/chronos/event_envelope.py",
    "python/chronos/value_objects.py",
    "python/chronos/state_lineage.py",
    "python/chronos/serialization.py",
    "tests/python/test_boundary.py",
    "tests/python/test_bootstrap.py",
    "tests/python/test_contracts_fixed_point.py",
    "tests/python/test_event_envelope.py",
    "tests/python/test_value_objects.py",
    "tests/python/test_state_lineage.py",
    "tests/python/test_serialization.py",
    "tests/python/test_package.py",
    "tools/development/bootstrap_m0.py",
}
EXPECTED_WHEEL_PACKAGE_PATHS = {
    "chronos/__init__.py",
    "chronos/boundary.py",
    "chronos/contracts.py",
    "chronos/event_envelope.py",
    "chronos/value_objects.py",
    "chronos/state_lineage.py",
    "chronos/serialization.py",
}
EXPECTED_DIST_INFO_FILES = {"METADATA", "RECORD", "WHEEL"}


def require_exact_paths(actual: set[str], expected: set[str], artifact: Path) -> None:
    missing = sorted(expected - actual)
    unexpected = sorted(actual - expected)
    if missing or unexpected:
        detail = []
        if missing:
            detail.append(f"missing: {', '.join(missing)}")
        if unexpected:
            detail.append(f"unexpected: {', '.join(unexpected)}")
        raise RuntimeError(f"{artifact.name} content mismatch ({'; '.join(detail)})")


def normalized_sdist_paths(paths: set[str], artifact: Path) -> set[str]:
    roots = {PurePosixPath(path).parts[0] for path in paths}
    if len(roots) != 1:
        raise RuntimeError(f"{artifact.name} must contain exactly one archive root")
    return {str(PurePosixPath(*PurePosixPath(path).parts[1:])) for path in paths}


def verify_wheel_paths(paths: set[str], artifact: Path) -> None:
    package_paths = {path for path in paths if ".dist-info/" not in path}
    require_exact_paths(package_paths, EXPECTED_WHEEL_PACKAGE_PATHS, artifact)

    metadata_paths = {PurePosixPath(path) for path in paths if ".dist-info/" in path}
    metadata_roots = {path.parent for path in metadata_paths}
    if len(metadata_roots) != 1 or not next(iter(metadata_roots)).name.endswith(".dist-info"):
        raise RuntimeError(f"{artifact.name} must contain exactly one dist-info directory")
    metadata_names = {path.name for path in metadata_paths}
    require_exact_paths(metadata_names, EXPECTED_DIST_INFO_FILES, artifact)


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="chronos-python-dist-") as tmp:
        output = Path(tmp)
        subprocess.run(("uv", "build", "--out-dir", str(output)), cwd=root, check=True)
        sdist = next(output.glob("*.tar.gz"))
        wheel = next(output.glob("*.whl"))

        with tarfile.open(sdist, "r:gz") as archive:
            members = archive.getmembers()
            non_files = sorted(member.name for member in members if not member.isfile())
            if non_files:
                raise RuntimeError(
                    f"{sdist.name} contains non-regular archive members: {', '.join(non_files)}"
                )
            sdist_paths = {member.name for member in members}
        with zipfile.ZipFile(wheel) as archive:
            wheel_paths = set(archive.namelist())

        require_exact_paths(normalized_sdist_paths(sdist_paths, sdist), EXPECTED_SDIST_PATHS, sdist)
        verify_wheel_paths(wheel_paths, wheel)

    print("Python sdist and wheel contents are clean.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
