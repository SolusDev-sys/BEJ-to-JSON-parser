cmake_minimum_required(VERSION 3.23)

project(BEJ-to-JSON LANGUAGES C CXX)

add_executable(BEJ-to-JSON
    main.c
    decode.c
)

target_include_directories(BEJ-to-JSON PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# -----------------------------------------------------------------------------
# Testing and GoogleTest
# -----------------------------------------------------------------------------
include(FetchContent)

# Fix CMP0135 warning about DOWNLOAD_EXTRACT_TIMESTAMP
cmake_policy(SET CMP0135 NEW)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)

# Prevent GoogleTest from forcing its own compiler flags
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(decode_tests
    test/test.cpp  # <-- create this file with GTest unit tests
    decode.c
)

target_include_directories(decode_tests PRIVATE
    //${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(decode_tests
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(decode_tests)

set_target_properties(BEJ-to-JSON decode_tests PROPERTIES
    C_STANDARD 11
    CXX_STANDARD 17
)

if (MSVC)
    target_compile_options(BEJ-to-JSON PRIVATE /W4)
    target_compile_options(decode_tests PRIVATE /W4)
else()
    target_compile_options(BEJ-to-JSON PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(decode_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
