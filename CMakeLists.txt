cmake_minimum_required(VERSION 3.24)

project(rtsafe_sensor_hub VERSION 1.0.0 LANGUAGES C CXX)

option(RTSAFE_BUILD_TESTS "Build host-side automated tests" ON)
option(RTSAFE_BUILD_SIMULATOR "Build the deterministic sensor simulator" ON)
option(RTSAFE_ENABLE_STRICT_WARNINGS "Treat compiler warnings as errors" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(rtsafe_core OBJECT
    src/core/crc16.c
    src/core/filters.c
    src/core/pipeline.c
    src/core/protocol.c
    src/core/safety.c
)

target_include_directories(rtsafe_core PUBLIC include)

if(MSVC)
    target_compile_options(rtsafe_core PRIVATE /W4)
    if(RTSAFE_ENABLE_STRICT_WARNINGS)
        target_compile_options(rtsafe_core PRIVATE /WX)
    endif()
else()
    target_compile_options(rtsafe_core PRIVATE -Wall -Wextra -Wpedantic)
    if(RTSAFE_ENABLE_STRICT_WARNINGS)
        target_compile_options(rtsafe_core PRIVATE -Werror)
    endif()
    if(NOT WIN32)
        target_link_libraries(rtsafe_core PUBLIC m)
    endif()
endif()

if(RTSAFE_BUILD_SIMULATOR)
    add_executable(rtsafe_simulator src/simulator/main.cpp)
    target_link_libraries(rtsafe_simulator PRIVATE rtsafe_core)
    set_target_properties(rtsafe_simulator PROPERTIES OUTPUT_NAME "rtsafe-simulator")
endif()

add_executable(rtsafe_supervisor src/supervisor/main.cpp)
target_link_libraries(rtsafe_supervisor PRIVATE rtsafe_core)
set_target_properties(rtsafe_supervisor PROPERTIES OUTPUT_NAME "rtsafe-supervisor")

if(NOT MSVC)
    foreach(target_name IN ITEMS rtsafe_simulator rtsafe_supervisor)
        if(TARGET ${target_name})
            target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
            if(RTSAFE_ENABLE_STRICT_WARNINGS)
                target_compile_options(${target_name} PRIVATE -Werror)
            endif()
        endif()
    endforeach()
endif()

install(TARGETS rtsafe_supervisor RUNTIME DESTINATION bin)
install(FILES LICENSE_INFORMATION.md DESTINATION share/licenses/rtsafe-sensor-hub)

if(RTSAFE_BUILD_TESTS)
    enable_testing()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_executable(rtsafe_unit_tests tests/unit_tests.cpp)
    target_link_libraries(rtsafe_unit_tests PRIVATE rtsafe_core)
    if(NOT MSVC)
        target_compile_options(rtsafe_unit_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME unit COMMAND rtsafe_unit_tests)
    add_test(
        NAME integration
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/integration_test.py
            --simulator $<TARGET_FILE:rtsafe_simulator>
            --supervisor $<TARGET_FILE:rtsafe_supervisor>
            --work-dir ${CMAKE_BINARY_DIR}/integration-output
    )
    add_test(
        NAME public_safety
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/check_public_safety.py
            ${CMAKE_SOURCE_DIR}
    )
    add_test(
        NAME metadata
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/validate_metadata.py
            ${CMAKE_SOURCE_DIR}
    )
endif()
