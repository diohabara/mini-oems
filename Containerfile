FROM docker.io/library/debian:bookworm-slim AS dev

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl xz-utils git ca-certificates \
    && curl --proto '=https' --tlsv1.2 -sSf -L \
       https://install.determinate.systems/nix \
       | sh -s -- install linux --no-confirm --init none \
    && rm -rf /var/lib/apt/lists/*

ENV PATH="/nix/var/nix/profiles/default/bin:$PATH"

RUN nix profile install \
    nixpkgs#cmake \
    nixpkgs#ninja \
    nixpkgs#gcc14 \
    nixpkgs#clang-tools

WORKDIR /app

FROM dev AS build

COPY . .
RUN cmake -B build -G Ninja \
    && cmake --build build \
    && ctest --test-dir build --output-on-failure

ENTRYPOINT ["/app/build/mini-oems"]
