"""Build Python artifacts and verify their contents are intentional."""

import subprocess
import tarfile
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

FORBIDDEN_PARTS = {".claude", ".git", ".venv", "build", "dist", "__pycache__"}
REQUIRED_SDIST_SUFFIXES = {
    "README.md",
    "pyproject.toml",
    "python/chronos/__init__.py",
    "python/chronos/boundary.py",
}
REQUIRED_WHEEL_SUFFIXES = {
    "chronos/__init__.py",
    "chronos/boundary.py",
}


def reject_unexpected(paths: set[str], artifact: Path) -> None:
    failures = []
    for path in paths:
        parts = PurePosixPath(path).parts
        if FORBIDDEN_PARTS.intersection(parts) or parts[-1] == "journal.html":
            failures.append(path)
        if any(part.endswith((".o", ".a", ".so", ".dylib", ".dll")) for part in parts):
            failures.append(path)
    if failures:
        joined = "\n- ".join(sorted(set(failures)))
        raise RuntimeError(f"{artifact.name} contains forbidden paths:\n- {joined}")


def require_suffixes(paths: set[str], artifact: Path, required: set[str]) -> None:
    missing = [suffix for suffix in sorted(required) if not any(p.endswith(suffix) for p in paths)]
    if missing:
        raise RuntimeError(f"{artifact.name} is missing required content: {', '.join(missing)}")


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="chronos-python-dist-") as tmp:
        output = Path(tmp)
        subprocess.run(("uv", "build", "--out-dir", str(output)), cwd=root, check=True)
        sdist = next(output.glob("*.tar.gz"))
        wheel = next(output.glob("*.whl"))

        with tarfile.open(sdist, "r:gz") as archive:
            sdist_paths = {member.name for member in archive.getmembers()}
        with zipfile.ZipFile(wheel) as archive:
            wheel_paths = set(archive.namelist())

        reject_unexpected(sdist_paths, sdist)
        reject_unexpected(wheel_paths, wheel)
        require_suffixes(sdist_paths, sdist, REQUIRED_SDIST_SUFFIXES)
        require_suffixes(wheel_paths, wheel, REQUIRED_WHEEL_SUFFIXES)

    print("Python sdist and wheel contents are clean.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
