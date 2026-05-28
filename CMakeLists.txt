cmake_minimum_required(VERSION 3.16)
project(drs_sync C)

set(CMAKE_C_STANDARD 11)

if(NOT DEFINED PLATFORM)
    set(PLATFORM "host")
endif()

add_compile_definitions(PLATFORM_${PLATFORM})

set(WARN_FLAGS -Wall -Wextra)

# Source files (shared between host and arm)
set(DRS_SOURCES
    src/clock.c
    src/protocol.c
    src/sync.c
    src/election.c
    src/net.c
    src/gpio.c
    src/telemetry.c
    src/calibrate.c
)

# Main binary
add_executable(drs_sync src/main.c ${DRS_SOURCES})
target_include_directories(drs_sync PRIVATE src)
target_compile_options(drs_sync PRIVATE ${WARN_FLAGS})
if(NOT PLATFORM STREQUAL "host")
    target_link_libraries(drs_sync PRIVATE pthread)
endif()

# Test binary
set(TEST_SOURCES
    tests/main_test.c
    tests/test_protocol.c
    tests/test_clock.c
    tests/test_sync.c
    tests/test_election.c
    ${DRS_SOURCES}
)

add_executable(drs_tests ${TEST_SOURCES})
target_include_directories(drs_tests PRIVATE src tests)
target_compile_options(drs_tests PRIVATE ${WARN_FLAGS})
if(NOT PLATFORM STREQUAL "host")
    target_link_libraries(drs_tests PRIVATE pthread)
endif()
