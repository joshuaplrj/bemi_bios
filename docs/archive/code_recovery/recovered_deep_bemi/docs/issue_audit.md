# Bemi BIOS Issue Audit

Confirmed mistakes and breakages found in the current tree.

## Test Discovery
- `bemi_bios/pro-tes/tests` is not pytest-discoverable as written.
- Evidence: `pytest -q` in `bemi_bios/pro-tes/tests` reports `no tests ran in 0.03s`.
- The only pytest-style test function found is `bemi_bios/pro-tes/tests/integration/BootFreeDOS.py:10`, but the filename does not match pytest discovery rules.

## Missing QEMU Guest Artifact
- `bemi_bios/pro-tes/tests/qemu/conftest.py:11` and `:28` default `--guest` to `tests/qemu/QemuRoundTrip.efi`.
- `glob` under `bemi_bios/pro-tes` found no `QemuRoundTrip.efi` file.
- The source and INF exist, but the built EFI payload is absent: `bemi_bios/pro-tes/tests/qemu/QemuRoundTrip.c`, `bemi_bios/pro-tes/tests/qemu/QemuRoundTrip.inf`.

## Firmware Path Inconsistencies
- `bemi_bios/pro-tes/deploy/Makefile:23` copies from `Build/BemiBios/RELEASE_GCC5/X64/BemiBiosCore.efi`.
- Other defaults use `Build/RELEASE_GCC5/X64/BemiBiosCore.efi`, including `bemi_bios/pro-tes/tests/qemu/conftest.py:10`, `bemi_bios/pro-tes/tests/qemu/QemuTest.py:46`, and `bemi_bios/pro-tes/tests/integration/StressTest.py:52`.
- `bemi_bios/pro-tes/tests/QemuTest.py:32` uses `Build/BemiBios/RELEASE_GCC5/X64/BemiBiosCore.efi`.
- `bemi_bios/pro-tes/deploy/usb/BemiUsbInstaller.ps1:4` uses `Build\RELEASE_GCC5\X64\BemiBiosCore.efi`.

## Linux-Only Deployment Assumptions
- `bemi_bios/pro-tes/deploy/Makefile` depends on Linux tools and paths: `find`, `mkdir -p`, `cp`, `rm -rf`, `nproc`, `dd`, `/tmp/*`, `/usr/share/ovmf/OVMF*.fd`.
- `bemi_bios/pro-tes/scripts/qemu_test.sh` hardcodes `/usr/share/ovmf/OVMF*.fd`, `/tmp/*`, `dd`, and `accel=kvm`.
- These scripts are not portable to Windows without additional handling.

## Rust Warnings
- `cargo check --all-features` in `bemi_bios/pro-tes/dbt` reproduces warnings.
- `lib.rs` has ambiguous glob re-exports for `MAX_CODE_SIZE` from both `codegen::*` and `ir::*`.
- `x8088/decoder.rs:222` uses `0xD0..0xD3`, which triggers `non_contiguous_range_endpoints` and likely should be inclusive.
- Unused imports and variables remain in `x8088/state.rs`, `x8088/bios.rs`, `x8088/bench.rs`, `x8088/decoder.rs`, and `x8088/executor.rs`.

## Documentation Mismatch
- `bemi_bios/docs/README.md:116` says `python bemi_constants.py --version v1.3`.
- `bemi_constants.py` has no CLI parser or `__main__` entry point, and running it with `--version v1.3` produces no output.
- The README also references scripts outside `bemi_bios` (`compare_all_three.py`, `run_all_tests.py`) without clearly stating they live in the repo root.

## Missing Python Dependency Manifest
- No `requirements.txt`, `pyproject.toml`, `Pipfile`, or `environment.yml` was found under `vemi`.
- The code imports third-party packages such as `pandas`, `numpy`, and `matplotlib` across benchmark scripts.
- Fresh environments will not know what to install without external instructions.

## Path-Hack Imports
- Several scripts modify `sys.path` to import shared modules from the repo root.
- Examples: `bemi_bios/rob_density_benchmark.py:35`, `bemi_bios/rob_dbt_benchmarks.py:18`, `bemi_bios/bios_prototype.py:7`, `vemi/tests/*.py`, and `compare_all_three.py:9`.
- This works locally but is brittle for packaging and test execution.
