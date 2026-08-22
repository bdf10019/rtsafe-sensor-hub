cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rtsafe_zephyr_tests)

target_include_directories(app PRIVATE ../../../../include)
target_sources(app PRIVATE
    src/test_core.c
    ../../../../src/core/crc16.c
    ../../../../src/core/filters.c
    ../../../../src/core/pipeline.c
    ../../../../src/core/protocol.c
    ../../../../src/core/safety.c
)

