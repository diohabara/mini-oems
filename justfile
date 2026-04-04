dev_image := "mini-oems-dev"
prod_image := "mini-oems"

default:
    @just --list

dev:
    podman build --target dev -t {{dev_image}} .

_run +cmd: dev
    podman run --rm \
        -v {{justfile_directory()}}:/app \
        -w /app \
        {{dev_image}} \
        {{cmd}}

build:
    podman build -t {{prod_image}} .

run:
    podman run --rm {{prod_image}}

test:
    just _run sh -c "cmake -B build -G Ninja && ctest --test-dir build --output-on-failure"

fmt:
    just _run clang-format -i src/*.cc test/*.cc

lint:
    just _run clang-tidy src/*.cc -- -std=c++23

check: fmt lint test
