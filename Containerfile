FROM docker.io/nixos/nix:latest AS dev

# Enable flakes for `nix profile install nixpkgs#...` syntax.
RUN mkdir -p /etc/nix && \
    echo "experimental-features = nix-command flakes" >> /etc/nix/nix.conf

# Core toolchain + HTTP framework + codegen tooling.
RUN nix profile install \
    nixpkgs#cmake \
    nixpkgs#ninja \
    nixpkgs#gcc14 \
    nixpkgs#clang-tools \
    nixpkgs#lcov \
    nixpkgs#doxygen \
    nixpkgs#oatpp \
    nixpkgs#nlohmann_json \
    nixpkgs#openapi-generator-cli \
    nixpkgs#jre_minimal \
    nixpkgs#pkg-config \
    nixpkgs#gnused

# Put gcov (from gcc-14) on PATH.  `/usr/local/bin` is not on the nixos/nix
# PATH, so symlink into the default nix profile bin which is.
RUN ln -sf $(find /nix/store -name gcov -path '*14.3*' -type f | head -1) \
        /nix/var/nix/profiles/default/bin/gcov

WORKDIR /app

FROM dev AS build

COPY . .
RUN cmake -B build -G Ninja \
    && cmake --build build \
    && ctest --test-dir build --output-on-failure

ENTRYPOINT ["/app/build/mini-oems"]
