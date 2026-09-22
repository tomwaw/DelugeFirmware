# Repository Guidelines

## Project Structure & Module Organization

Deluge Community Firmware is a bare-metal C/C++ firmware project for the Synthstrom Deluge. Core firmware lives in `src/deluge`, with platform and third-party support in `src/RZA1`, `src/fatfs`, `src/NE10`, `src/RTT`, `src/dsp_ng`, and `src/lib`. Shared external libraries are under `lib`, Pure Data support is in `pd`, and user-facing/reference material is in `docs`, `assets`, and `website`. Build helpers and DBT subcommands are implemented in `scripts/tasks`. Tests live in `tests/unit`, `tests/spec`, `tests/32bit_unit_tests`, `tests/qemu`, and `tests/integration`.

## Build, Test, and Development Commands

- `./dbt configure` prepares the CMake build tree and toolchain.
- `./dbt build debug`, `./dbt build release`, or `./dbt build relwithdebinfo` builds firmware variants.
- `make debug`, `make release`, and `make clean` are thin wrappers around DBT.
- `./dbt test` configures, builds, and runs local CTest-based tests in `build/tests`.
- `./dbt format -g` formats staged C/C++ files; `./dbt format -c src tests` checks formatting.
- `cd website && bun run dev` serves the Astro docs site; `bun run check` runs Astro, ESLint, and Prettier checks.

## Coding Style & Naming Conventions

C and C++ use `.clang-format` based on LLVM: tabs for indentation, 4-column tab width, 120-column limit, left pointer alignment, and case-sensitive include sorting. `.editorconfig` enforces LF endings, UTF-8, final newlines, and trimmed trailing whitespace for C/C++ except selected driver files. Python code is checked with Ruff in CI and pre-commit. Follow nearby naming patterns; test files commonly use `*_tests.cpp`, `*_spec.cpp`, and Python `test_*.py`.

## Testing Guidelines

Unit tests use CppUTest, specs use CppSpec, QEMU tests cover ARMv7 behavior, and USB MIDI integration tests use pytest. Add focused regression tests with behavior changes, especially for shared model, DSP, UI, MIDI, storage, or memory code. Run `./dbt test` before opening a PR when local dependencies allow it; use the direct CMake workflow from `.github/workflows/tests.yml` if debugging CI parity.

## Commit & Pull Request Guidelines

Recent history uses concise, descriptive commit subjects, sometimes with conventional prefixes such as `build(deps): ...`. Prefer an imperative summary that names the affected behavior, for example `Fix MIDI device definition browser selection`. Pull requests should describe the change, link issues or discussions, list test results, and note hardware/UI impact such as OLED vs 7-segment behavior. Include logs for build/test failures and screenshots or short recordings for visible UI changes.
