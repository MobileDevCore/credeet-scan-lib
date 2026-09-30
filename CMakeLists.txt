cmake_minimum_required(VERSION 3.16)
project(scanmatch VERSION 0.11.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Production C library: Minimal, optimized, zero external OCR/AI dependencies
add_library(scanmatch SHARED
    src/scanmatch.c
    src/text.c
    src/parser.c
    src/ffi.c
)

target_compile_definitions(scanmatch PRIVATE SCANMATCH_BUILD)
target_include_directories(scanmatch PUBLIC include)
target_link_libraries(scanmatch PRIVATE m)

set_target_properties(scanmatch PROPERTIES
    OUTPUT_NAME "scanmatch"
    PREFIX "lib"
)

# On Windows/MinGW, export symbols
if(WIN32)
    set_target_properties(scanmatch PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
    if(MINGW)
        target_link_options(scanmatch PRIVATE -Wl,--export-all-symbols)
    endif()
endif()

# CLI utility
add_executable(scanmatch_cli cli/scanmatch_cli.c)
target_link_libraries(scanmatch_cli PRIVATE scanmatch)
target_include_directories(scanmatch_cli PRIVATE include)

# Tests
add_executable(test_similarity tests/test_similarity.c)
target_link_libraries(test_similarity PRIVATE scanmatch)
target_include_directories(test_similarity PRIVATE include)

add_executable(test_parser tests/test_parser.c)
target_link_libraries(test_parser PRIVATE scanmatch)
target_include_directories(test_parser PRIVATE include)

add_executable(test_ffi tests/test_ffi.c)
target_link_libraries(test_ffi PRIVATE scanmatch)
target_include_directories(test_ffi PRIVATE include)

add_executable(test_matching tests/test_matching.c)
target_link_libraries(test_matching PRIVATE scanmatch)
target_include_directories(test_matching PRIVATE include)

add_executable(test_gujarati tests/test_gujarati.c)
target_link_libraries(test_gujarati PRIVATE scanmatch)
target_include_directories(test_gujarati PRIVATE include)

add_executable(test_catalog_gujarati tests/test_catalog_gujarati.c)
target_link_libraries(test_catalog_gujarati PRIVATE scanmatch)
target_include_directories(test_catalog_gujarati PRIVATE include)

add_executable(test_comprehensive tests/test_comprehensive.c)
target_link_libraries(test_comprehensive PRIVATE scanmatch)
target_include_directories(test_comprehensive PRIVATE include)

enable_testing()
add_test(NAME similarity COMMAND test_similarity)
add_test(NAME parser COMMAND test_parser)
add_test(NAME ffi COMMAND test_ffi)
add_test(NAME matching COMMAND test_matching)
add_test(NAME gujarati COMMAND test_gujarati)
add_test(NAME catalog_gujarati COMMAND test_catalog_gujarati)
add_test(NAME comprehensive COMMAND test_comprehensive)
