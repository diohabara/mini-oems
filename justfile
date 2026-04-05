dev_image := "mini-oems-dev"
prod_image := "mini-oems"
workdir := justfile_directory()

default:
    @just --list

# Build the dev container image once.
dev:
    podman build --target dev -t {{dev_image}} .

# Build the production image.
build:
    podman build -t {{prod_image}} .

# Run the production image.
run:
    podman run --rm {{prod_image}}

# Run an arbitrary shell command inside the dev container.
# Usage: just sh "cmake -B build -G Ninja"
sh cmd: dev
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} sh -c "{{cmd}}"

# ----- Configuration & tests -----

configure: dev
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} \
        sh -c "cmake -B build -G Ninja"

test: configure
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} \
        sh -c "cmake --build build && ctest --test-dir build --output-on-failure"

unit-test: configure
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} \
        sh -c "cmake --build build --target unit_tests && build/unit_tests"

integration-test: configure
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} \
        sh -c "cmake --build build --target integration_tests && build/integration_tests"

system-test: configure
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} \
        sh -c "cmake --build build --target system_tests && build/system_tests"

# ----- Coverage (requires lcov in the dev image) -----

coverage: dev
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} sh -c '\
        rm -rf build-cov && \
        cmake -B build-cov -G Ninja -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug && \
        cmake --build build-cov && \
        ctest --test-dir build-cov --output-on-failure && \
        lcov --capture --directory build-cov --output-file build-cov/coverage.info \
            --ignore-errors mismatch,gcov,source,unused,negative,inconsistent \
            --exclude "*/_deps/*" --exclude "*/test/*" --exclude "*/third_party/*" \
            --exclude "/usr/*" --exclude "*/nix/store/*" && \
        lcov --list build-cov/coverage.info && \
        genhtml build-cov/coverage.info --output-directory build-cov/coverage-html \
            --ignore-errors source,unused'

# ----- Benchmarks (opt-in) -----

bench: dev
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} sh -c '\
        cmake -B build-bench -G Ninja -DENABLE_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release && \
        cmake --build build-bench --target benchmarks && \
        build-bench/benchmarks'

# ----- Docs (requires doxygen in the dev image) -----

docs: dev
    rm -rf build/docs/en build/docs/ja build/docs/html build/docs/index.html
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} sh -c '\
        ./scripts/run-localized-doxygen.sh en build/docs && \
        ./scripts/run-localized-doxygen.sh ja build/docs'
    ./scripts/write-docs-i18n-map.sh build/docs/en build/docs/ja
    ./scripts/open-docs.sh build/docs/ja/index.html

# ----- OpenAPI codegen (spec-first) -----

# Regenerate src/api-gen/ from docs/openapi.yaml via openapi-generator.
api-gen: dev
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} sh -c '\
        rm -rf src/api-gen && \
        openapi-generator-cli generate \
            -i docs/openapi.yaml \
            -g cpp-oatpp-server \
            -o src/api-gen \
            --additional-properties=packageName=oems_api && \
        rm -rf src/api-gen/impl \
               src/api-gen/main-api-server.cpp \
               src/api-gen/CMakeLists.txt \
               src/api-gen/README.md && \
        sed -i -e "s/const OrderStatus \&/const org::openapitools::server::model::OrderStatus \&/g" \
               -e "s/QUERY(OrderStatus,/QUERY(org::openapitools::server::model::OrderStatus,/g" \
               src/api-gen/api/OrdersApi.hpp'

# Verify generated code matches committed files (CI use).
api-gen-check: api-gen
    git diff --exit-code src/api-gen

# Preview docs/openapi.yaml in a local Swagger UI (http://localhost:8081).
# Uses --network=host since rootless podman may lack the rootlessport helper.
openapi-preview:
    podman run --rm --network=host \
        -e PORT=8081 \
        -e SWAGGER_JSON=/spec/openapi.yaml \
        -v {{workdir}}/docs:/spec \
        docker.io/swaggerapi/swagger-ui:latest

# Validate openapi.yaml with the generator's built-in validator.
openapi-validate: dev
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} \
        openapi-generator-cli validate -i docs/openapi.yaml

# ----- Format & lint -----

# Format all first-party .cc/.h under src/, test/, bench/ (excludes third_party).
fmt: dev
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} sh -c '\
        find src test bench \( -name "*.cc" -o -name "*.h" \) -print 2>/dev/null \
          | xargs -r clang-format -i'

# Run clang-tidy (read-only) on all first-party sources.  Excludes
# third_party/ and the generated api-gen/.
lint: configure
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} sh -c '\
        find src -name "*.cc" \
             -not -path "*/third_party/*" -not -path "*/api-gen/*" -print \
          | xargs -r clang-tidy -p build --quiet'

# Same as `lint` but apply auto-fixes clang-tidy can produce.
lint-fix: configure
    podman run --rm -v {{workdir}}:/app -w /app {{dev_image}} sh -c '\
        find src -name "*.cc" \
             -not -path "*/third_party/*" -not -path "*/api-gen/*" -print \
          | xargs -r clang-tidy -p build --quiet --fix --fix-errors'

check: fmt lint test
