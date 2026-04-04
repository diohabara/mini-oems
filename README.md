# mini-oems

A minimal Order and Execution Management System in modern C++ — FIX protocol, price-time priority matching, and TWAP/VWAP execution algorithms.

## Prerequisites

- [Podman](https://podman.io/)
- [just](https://github.com/casey/just)

## Commands

All operations go through `just`, which delegates to Podman containers.
Nix manages dependencies inside the container; CMake builds the project.

```bash
just build  # Build production image
just run    # Run in container
just test   # Run tests
just fmt    # Format code (clang-format)
just lint   # Static analysis (clang-tidy)
just check  # fmt + lint + test
```
