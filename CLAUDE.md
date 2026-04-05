# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Mini OEMS — a minimal Order Management / Execution Management System in modern C++23. Accepts order flow over FIX, applies risk checks, matches with price-time priority, and runs TWAP/VWAP execution algorithms. See `docs/architecture.md` for the canonical v1 system design and `docs/fix-latest-guide.md` for the FIX protocol stance (FIX Latest app layer, FIX4/FIXT sessions, FIXS/TLS transport, with FIX 4.2/4.4 interop).

## Workflow

**Always work in a git worktree under `.worktrees/`** — do not modify the main checkout directly. This keeps the primary working tree clean, lets multiple tasks/PRs run in parallel, and prevents half-finished work from blocking a quick fix on another branch.

```bash
# Start a new task on a fresh branch
git worktree add .worktrees/<short-task-name> -b <type>/<short-task-name>
cd .worktrees/<short-task-name>

# ... edit, build, test, commit, push, open PR ...

# After the PR is merged, remove the worktree
cd /home/jio/repo/mini-oems
git worktree remove .worktrees/<short-task-name>
git branch -d <type>/<short-task-name>
```

- Branch name prefix follows Conventional Commits: `feat/...`, `fix/...`, `refactor/...`, `docs/...`, `test/...`, `chore/...`, `perf/...`, `ci/...`, `build/...`.
- `.worktrees/` is already in `.gitignore`.
- One worktree per PR. If a worktree accumulates unrelated changes, split them into separate worktrees before opening the PR.

## Commands

All commands run inside a Podman dev container (`docker.io/nixos/nix:latest` + `nix profile install` for cmake/ninja/gcc14/clang-tools/lcov/doxygen) via `just`:

```bash
just dev               # Build the dev image (cached after first run)
just configure         # cmake -B build -G Ninja
just test              # Build + run all tests (unit + integration + system)
just unit-test         # Only unit tests
just integration-test  # Only integration tests
just system-test       # End-to-end HTTP system tests
just coverage          # gcov + lcov HTML report under build-cov/coverage-html/
just bench             # Google Benchmark under build-bench/
just docs              # Generate EN/JA Doxygen HTML and open the Japanese docs entry page
just fmt               # clang-format -i on all src/test/bench .cc/.h
just lint              # clang-tidy (requires configured build/)
just check             # fmt + lint + test
just sh "<command>"    # Run an arbitrary shell command inside the dev container
just build             # Build the production image
just run               # Run the prod container
```

Running a single test (inside the dev container, after a build):
```bash
ctest --test-dir build --output-on-failure -R <TestNameRegex>
# or directly
./build/unit_tests --gtest_filter=SuiteName.TestName
```

Optional CMake flags (set by `just coverage`/`just bench`): `-DENABLE_COVERAGE=ON`, `-DENABLE_BENCHMARKS=ON`.

Build output conventions: main build in `build/`, coverage in `build-cov/`, benchmarks in `build-bench/` (kept separate so each is a clean, reproducible artifact).

## Architecture

The codebase is organized by component under `src/core/`, mirrored by test directories under `test/unit/` (and `test/integration/`, `test/system/`). Components map directly to the architecture diagram in `docs/architecture.md`:

- `types/` — shared primitives (errors, core value types); header-only
- `fix/` — FIX gateway: session management, message encode/decode, counterparty-specific quirks isolated at this boundary
- `order/` — Order Manager: validation and lifecycle (New → PendingNew → Acked → PartiallyFilled → Filled/Canceled/Rejected)
- `risk/` — pre-trade risk controls; gates orders before they reach the book
- `matching/` — price-time priority matching engine / order book
- `market_data/` — market data ingest and distribution (isolated from order path due to volume asymmetry)
- `algo/` — execution algorithms (TWAP, VWAP) that consume fills and emit child orders back through the Order Manager
- `persistence/` — audit/replay of orders and state transitions
- `api/` — external API surface
- `cli/` — local developer CLI (the primary user in v1)

Flow: **FIX Gateway → Order Manager → Risk Manager → Matching Engine**, with **Market Data Handler → Matching Engine** for reference prices, and **Matching Engine → Execution Algorithms → Order Manager** closing the child-order loop.

### HTTP API — spec-first via openapi-generator

The HTTP/JSON API is NOT hand-written. `docs/openapi.yaml` (OpenAPI 3.0.3) is
the single source of truth. `just api-gen` runs openapi-generator-cli with the
`cpp-oatpp-server` generator to produce `src/api-gen/{api,model}/` — abstract
API controller classes and DTO classes. Our adapters in
`src/core/api/oems_controllers.{hpp,cpp}` subclass the generated abstract APIs
and bridge them to the domain layer. The oatpp server is started in
`src/main.cc`.

To evolve the API: edit `docs/openapi.yaml`, run `just api-gen`, then
implement/update the controller methods — the compiler tells you exactly
what's missing.

`src/api-gen/` is committed. CI runs `just api-gen-check` to ensure the
committed output matches what the spec would regenerate.

### Build structure

- Single `CMakeLists.txt` at root. Each module compiles into its own static library (`oems_matching`, `oems_risk`, `oems_order`, `oems_persistence`, `oems_fix`, `oems_algo`, `oems_market_data`, `oems_api`) with explicit dependency edges. Shared value types live in `oems_types` (INTERFACE library with `src/` on its include path).
- `oems_api_gen` (INTERFACE) exposes `src/api-gen/` include paths + links `oatpp::oatpp`.
- `oems_api` (static) contains `OemsControllers` that implement the generated abstract APIs.
- Test sources are collected by `file(GLOB_RECURSE ...)` across `test/unit`, `test/integration`, `test/system` with `CONFIGURE_DEPENDS`, so new test files are picked up without editing CMake. Each tier compiles into its own executable (`unit_tests`, `integration_tests`, `system_tests`) and is registered with `gtest_discover_tests`.
- GoogleTest v1.15.2 and (optionally) Google Benchmark v1.9.1 are pulled via `FetchContent`.
- SQLite is vendored as `third_party/sqlite/sqlite3.{c,h}` (amalgamation) and built as the `oems_sqlite` static library with warnings suppressed.
- C++23 required (uses `std::expected`, `std::print`, `std::format`, concepts). Toolchain is gcc14 + clang-tools from nixpkgs inside the container.

### Error handling convention

All fallible functions return `Result<T> = std::expected<T, OemsError>` (defined in `src/core/types/error.h`). No exceptions are thrown from domain code. Callers check `.has_value()` and branch on `.error()`. This applies uniformly across matching, risk, order, persistence, FIX, and API layers.

### Money representation

Prices and notionals are `int64_t` **integer minor units** — no floating point. The interpretation of "minor unit" depends on the currency:

- **JPY (TSE target market): 1 minor unit = 1 yen.** There are no sub-yen units on TSE, so `Price = 10000` means 10,000 JPY, not 100.00 JPY.
- For reference: USD would use 1 minor unit = 1 cent (= 0.01 USD), but v1 targets TSE exclusively.

Notional and price-band arithmetic uses `__int128` to avoid overflow on attacker-supplied values.

### Per-symbol reference data (TSE)

TSE-specific rules (lot size 売買単位, tick bands 呼値の刻み, daily price limits 値幅制限) live in `SymbolConfig` (`src/core/types/instrument.h`) and are installed on the Risk Manager via `SetSymbolConfig(symbol, config)`. Unconfigured symbols fall through every TSE-specific check unchanged.

### Design constraints worth preserving

- Keep the matching path simple and deterministic.
- Isolate counterparty-specific FIX differences inside `fix/` — do not leak them into `order/` or `matching/`.
- Every external order and internal state transition should be auditable/replayable.
- Market data handling stays isolated from the order path (volume asymmetry).
