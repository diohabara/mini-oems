cmake_minimum_required(VERSION 3.28)
project(mini-oems LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Disable CMake's C++ module scanning: GCC 14's `-fdeps-format=p1689r5`,
# `-fmodule-mapper=...`, `-fmodules-ts` flags are unknown to clang-tidy and
# would break static analysis on the exported compile_commands.json.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ENABLE_COVERAGE "Build with gcov coverage instrumentation" OFF)
option(ENABLE_BENCHMARKS "Build Google Benchmark targets" OFF)

if(ENABLE_COVERAGE)
  add_compile_options(--coverage -fprofile-arcs -ftest-coverage -O0 -g)
  add_link_options(--coverage)
endif()

# ---------------------------------------------------------------------------
# Third-party: GoogleTest
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------------
# Third-party: Google Benchmark (optional)
# ---------------------------------------------------------------------------
if(ENABLE_BENCHMARKS)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
      googlebenchmark
      GIT_REPOSITORY https://github.com/google/benchmark.git
      GIT_TAG v1.9.1
  )
  FetchContent_MakeAvailable(googlebenchmark)
endif()

# ---------------------------------------------------------------------------
# Core library: types (header-only)
# ---------------------------------------------------------------------------
add_library(oems_types INTERFACE)
target_include_directories(oems_types INTERFACE ${CMAKE_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
# Core library: matching
# ---------------------------------------------------------------------------
add_library(oems_matching
    src/core/matching/order_book.cc
    src/core/matching/matching_engine.cc
)
target_link_libraries(oems_matching PUBLIC oems_types)

# ---------------------------------------------------------------------------
# Core library: risk
# ---------------------------------------------------------------------------
add_library(oems_risk
    src/core/risk/risk_manager.cc
)
target_link_libraries(oems_risk PUBLIC oems_types)

# ---------------------------------------------------------------------------
# Core library: order
# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# Third-party: vendored SQLite
# ---------------------------------------------------------------------------
add_library(oems_sqlite STATIC third_party/sqlite/sqlite3.c)
target_include_directories(oems_sqlite PUBLIC third_party/sqlite)
set_target_properties(oems_sqlite PROPERTIES C_STANDARD 11)
target_compile_options(oems_sqlite PRIVATE -w)
target_compile_definitions(oems_sqlite PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_RTREE=0
    SQLITE_OMIT_DEPRECATED
)

# ---------------------------------------------------------------------------
# Core library: persistence
# Uses order/OrderEvent/Fill types through headers only (via oems_types include
# path). Does not link against oems_order to avoid a link-time cycle, since
# oems_order now calls into oems_persistence at runtime.
# ---------------------------------------------------------------------------
add_library(oems_persistence
    src/core/persistence/database.cc
)
target_link_libraries(oems_persistence PUBLIC oems_types oems_matching oems_sqlite)

# ---------------------------------------------------------------------------
# Core library: order
# ---------------------------------------------------------------------------
add_library(oems_order
    src/core/order/order_manager.cc
)
target_link_libraries(oems_order PUBLIC oems_types oems_matching oems_risk oems_persistence)

# ---------------------------------------------------------------------------
# Core library: algo
# ---------------------------------------------------------------------------
add_library(oems_algo
    src/core/algo/algo_engine.cc
    src/core/algo/twap.cc
    src/core/algo/vwap.cc
)
target_link_libraries(oems_algo PUBLIC oems_types oems_order)

# ---------------------------------------------------------------------------
# Core library: market_data
# ---------------------------------------------------------------------------
add_library(oems_market_data
    src/core/market_data/market_data_handler.cc
)
target_link_libraries(oems_market_data PUBLIC oems_types oems_risk)

# ---------------------------------------------------------------------------
# Core library: fix
# ---------------------------------------------------------------------------
add_library(oems_fix
    src/core/fix/fix_message.cc
    src/core/fix/fix_session.cc
    src/core/fix/fix_gateway.cc
)
target_link_libraries(oems_fix PUBLIC oems_types oems_order oems_matching oems_risk)

# ---------------------------------------------------------------------------
# OpenAPI-generated DTOs + abstract APIs (src/api-gen/ is managed by
# `just api-gen` from docs/openapi.yaml via openapi-generator-cli).
# ---------------------------------------------------------------------------
# nixpkgs' oatpp Config file has a broken absolute-path concatenation
# (oatpp_LIBRARIES_DIRS ends up as "/prefix//nix/store/..."), so we bypass
# the Config and include Targets.cmake directly.  The exported oatpp::oatpp
# target carries valid INTERFACE_INCLUDE_DIRECTORIES already.
find_file(OATPP_TARGETS_CMAKE oatppTargets.cmake
    PATHS /nix/var/nix/profiles/default/lib/cmake /usr/lib/cmake /usr/local/lib/cmake
    PATH_SUFFIXES oatpp-1.3.0
    NO_DEFAULT_PATH REQUIRED)
include(${OATPP_TARGETS_CMAKE})

add_library(oems_api_gen INTERFACE)
target_include_directories(oems_api_gen INTERFACE
    ${CMAKE_SOURCE_DIR}/src/api-gen
    ${CMAKE_SOURCE_DIR}/src/api-gen/api
    ${CMAKE_SOURCE_DIR}/src/api-gen/model
)
target_link_libraries(oems_api_gen INTERFACE oatpp::oatpp)

# ---------------------------------------------------------------------------
# Core library: API controller implementations (bridges generated code to
# the domain layer).  No hand-written HTTP stack.
# ---------------------------------------------------------------------------
add_library(oems_api
    src/core/api/oems_controllers.cpp
)
target_link_libraries(oems_api PUBLIC
    oems_types oems_order oems_matching oems_api_gen oatpp::oatpp)

# ---------------------------------------------------------------------------
# Executables
# ---------------------------------------------------------------------------
add_executable(mini-oems src/main.cc)
target_link_libraries(mini-oems PRIVATE
    oems_types oems_order oems_matching oems_risk
    oems_persistence oems_api oems_market_data oems_algo oems_fix
)

add_executable(oems-cli src/cli/main.cc)
target_link_libraries(oems-cli PRIVATE oems_types)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()

# Collect test sources by glob so incremental phases can add files without
# editing CMakeLists.
file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/test/unit/*.cc")
file(GLOB_RECURSE INTEGRATION_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/test/integration/*.cc")
file(GLOB_RECURSE SYSTEM_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/test/system/*.cc")

if(UNIT_TEST_SOURCES)
  add_executable(unit_tests ${UNIT_TEST_SOURCES})
  target_link_libraries(unit_tests PRIVATE GTest::gtest_main oems_types oems_matching oems_risk oems_order oems_persistence oems_algo oems_market_data oems_fix oems_api)
endif()

if(INTEGRATION_TEST_SOURCES)
  add_executable(integration_tests ${INTEGRATION_TEST_SOURCES})
  target_link_libraries(integration_tests PRIVATE
      GTest::gtest_main oems_types oems_matching oems_risk oems_order
      oems_persistence oems_algo oems_market_data)
endif()

if(SYSTEM_TEST_SOURCES)
  add_executable(system_tests ${SYSTEM_TEST_SOURCES})
  target_link_libraries(system_tests PRIVATE
      GTest::gtest_main oems_types oems_matching oems_risk oems_order oems_api)
endif()

include(GoogleTest)
if(TARGET unit_tests)
  gtest_discover_tests(unit_tests)
endif()
if(TARGET integration_tests)
  gtest_discover_tests(integration_tests)
endif()
if(TARGET system_tests)
  gtest_discover_tests(system_tests)
endif()

# Legacy sanity test kept for continuity.
add_executable(sanity_test test/sanity_test.cc)
target_link_libraries(sanity_test GTest::gtest_main)
gtest_discover_tests(sanity_test)

# ---------------------------------------------------------------------------
# Benchmarks (opt-in via -DENABLE_BENCHMARKS=ON)
# ---------------------------------------------------------------------------
if(ENABLE_BENCHMARKS)
  file(GLOB BENCHMARK_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/bench/*.cc")
  if(BENCHMARK_SOURCES)
    add_executable(benchmarks ${BENCHMARK_SOURCES})
    target_link_libraries(benchmarks PRIVATE
        benchmark::benchmark benchmark::benchmark_main
        oems_matching oems_risk oems_order)
  endif()
endif()
