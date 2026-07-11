"""Build and test every C++ profile promised by M0.2."""

import subprocess
from pathlib import Path

PRESETS = ("debug", "release", "benchmark", "sanitize")


def run(root: Path, *command: str) -> None:
    print("==> " + " ".join(command), flush=True)
    subprocess.run(command, cwd=root, check=True)


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    for preset in PRESETS:
        run(root, "cmake", "--preset", preset, "--fresh")
        run(root, "cmake", "--build", "--preset", preset)
        run(root, "ctest", "--preset", preset)

    multi_dir = "build-multi-config"
    run(root, "cmake", "-S", ".", "-B", multi_dir, "-G", "Ninja Multi-Config", "--fresh")
    for profile in ("Debug", "Release", "Benchmark"):
        run(root, "cmake", "--build", multi_dir, "--config", profile)
        run(root, "ctest", "--test-dir", multi_dir, "-C", profile, "--output-on-failure")

    print("All M0.2 C++ profiles passed.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
