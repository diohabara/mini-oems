# mini-oems

A minimal Order and Execution Management System in modern C++ — FIX protocol, price-time priority matching, and TWAP/VWAP execution algorithms.

## Prerequisites

- [Podman](https://podman.io/)
- [just](https://github.com/casey/just)

## Commands

All operations go through `just`, which delegates to Podman containers.
Nix manages dependencies inside the container; CMake builds the project.

```bash
just dev               # Build the dev image (cached after first run)
just configure         # cmake configure
just test              # Run all tests (unit + integration + system)
just unit-test         # Run unit tests only
just integration-test  # Run integration tests only
just system-test       # Run end-to-end HTTP system tests
just coverage          # gcov + lcov HTML report under build-cov/coverage-html/
just bench             # Google Benchmark suite under build-bench/
just docs              # Generate EN/JA Doxygen HTML and open the Japanese docs entry page
just api-gen           # Regenerate src/api-gen/ from docs/openapi.yaml
just api-gen-check     # Fail if src/api-gen/ is stale vs committed (CI)
just openapi-validate  # Validate docs/openapi.yaml
just openapi-preview   # Preview openapi.yaml via Swagger UI at :8081
just fmt               # Format code (clang-format)
just lint              # Static analysis (clang-tidy)
just check             # fmt + lint + test
just build             # Build the production image
just run               # Run in container
```

## API (spec-first)

The HTTP API is specified in [`docs/openapi.yaml`](docs/openapi.yaml) (OpenAPI 3.0.3)
— the **single source of truth**. Server-side C++ code (DTOs + abstract
controllers using [oatpp](https://oatpp.io)) is regenerated from it via
[openapi-generator](https://openapi-generator.tech) and lives under
`src/api-gen/`. Human-written glue in `src/core/api/oems_controllers.{hpp,cpp}`
bridges the generated abstract API to the domain layer.

To add or change an endpoint:

1. Edit `docs/openapi.yaml`
2. Run `just api-gen`
3. Compiler errors point to missing/changed controller methods — implement them
4. `just test` to verify

## License

Apache-2.0 — see [LICENSE](LICENSE).

## Architecture

See [docs/architecture.md](docs/architecture.md) for the full v1 design.
The implementation follows the recommended build order:

| Module              | Path                          | Purpose                                    |
|---------------------|-------------------------------|--------------------------------------------|
| Foundation types    | `src/core/types/`             | OrderId, Price, Symbol, Result<T>, errors |
| Matching engine     | `src/core/matching/`          | Price-time-priority order book             |
| Risk manager        | `src/core/risk/`              | Pre-trade controls                         |
| Order manager       | `src/core/order/`             | Lifecycle, events, coordination            |
| Persistence         | `src/core/persistence/`       | SQLite-backed durable state                |
| HTTP/JSON API       | `src/core/api/` + `src/api-gen/` | oatpp server, spec-first (openapi.yaml)  |
| FIX gateway         | `src/core/fix/`               | FIX4 session + application translation     |
| Execution algos     | `src/core/algo/`              | TWAP and VWAP slice generation             |
| Market data         | `src/core/market_data/`       | BBO snapshots, reference prices            |
| Server              | `src/main.cc`                 | Assembles modules, runs HTTP server        |
| CLI                 | `src/cli/main.cc`             | Developer command-line client              |

## CLI usage

Once the server is running (`./mini-oems 8080 oems.db`):

```bash
oems-cli server-status
oems-cli new-order --symbol AAPL --side buy --type limit --price 10000 --qty 100
oems-cli show-book --symbol AAPL
oems-cli show-orders --status Accepted
oems-cli cancel-order --order-id 1
oems-cli show-trades
```
