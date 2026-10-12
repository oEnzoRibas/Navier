cmake_minimum_required(VERSION 3.24)
project(Navier VERSION 0.1.0 LANGUAGES CXX)

option(NAVIER_BUILD_TESTS "Build GoogleTest unit tests" ON)
option(NAVIER_BUILD_EXAMPLES "Build synthetic checkpoint demo" ON)
option(NAVIER_BUILD_BENCHMARKS "Build index benchmark" ON)
option(NAVIER_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(NAVIER_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(NAVIER_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(cmake/CompilerOptions.cmake)

add_library(navier src/storage/checkpoint_store.cpp)
add_library(navier::navier ALIAS navier)
target_compile_features(navier PUBLIC cxx_std_23)
target_include_directories(navier PUBLIC "${PROJECT_SOURCE_DIR}/include")
navier_configure_target(navier)

if(NAVIER_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723) # v1.17.0
        FetchContent_MakeAvailable(googletest)
        navier_sanitize_target(gtest)
        navier_sanitize_target(gtest_main)
    endif()
    # A prebuilt MSVC GTest may lack ASan STL annotations; preserve ABI compatibility.
    if(GTest_FOUND AND MSVC AND NAVIER_ENABLE_ASAN)
        target_compile_definitions(navier PUBLIC _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
    endif()
    add_executable(navier_tests tests/bplus_tree_test.cpp tests/checkpoint_store_test.cpp)
    target_link_libraries(navier_tests PRIVATE navier GTest::gtest_main)
    navier_configure_target(navier_tests)
    include(GoogleTest)
    gtest_discover_tests(navier_tests)
endif()

if(NAVIER_BUILD_EXAMPLES)
    add_executable(navier_demo examples/synthetic_checkpoint_demo.cpp)
    target_link_libraries(navier_demo PRIVATE navier)
    navier_configure_target(navier_demo)
endif()
if(NAVIER_BUILD_BENCHMARKS)
    add_executable(navier_benchmark benchmarks/index_benchmark.cpp)
    target_link_libraries(navier_benchmark PRIVATE navier)
    navier_configure_target(navier_benchmark)
endif()

option(NAVIER_BUILD_SIMULATIONS "Build the external heat/fluid 2D example producers" ON)
if(NAVIER_BUILD_SIMULATIONS)
    foreach(
    simulation
    heat2d
    fluid2d
    cavity2d
    shear2d
    fluid3d
    electromag2d
    electromag3d
    lorenz3d
    wave3d
)
        add_executable(navier_${simulation} examples/${simulation}_demo.cpp)
        target_link_libraries(navier_${simulation} PRIVATE navier)
        navier_configure_target(navier_${simulation})
    endforeach()
    if(NAVIER_BUILD_TESTS)
        target_sources(navier_tests PRIVATE tests/simulation2d_test.cpp tests/cavity2d_test.cpp tests/advanced_fluid_test.cpp)
    endif()
endif()

option(NAVIER_BUILD_CLI "Build the Navier checkpoint explorer executable" ON)
if(NAVIER_BUILD_CLI)
    add_executable(navier_cli tools/navier_cli.cpp)
    set_target_properties(navier_cli PROPERTIES OUTPUT_NAME navier)
    target_link_libraries(navier_cli PRIVATE navier)
    navier_configure_target(navier_cli)
    if(NAVIER_BUILD_TESTS AND NAVIER_BUILD_EXAMPLES)
        add_test(NAME NavierCLI.Commands
            COMMAND ${CMAKE_COMMAND}
                -DCLI=$<TARGET_FILE:navier_cli>
                -DDEMO=$<TARGET_FILE:navier_demo>
                -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-test/$<CONFIG>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.cmake)
    endif()
endif()

option(NAVIER_BUILD_VIEWER "Build the offline animated GUI exporter" ON)
if(NAVIER_BUILD_VIEWER)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/viewer/index.html" NAVIER_VIEWER_HTML)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/viewer/volume3d.js" NAVIER_VOLUME_JS)
    string(REPLACE "<script src=\"volume3d.js\"></script>"
        "<script>\n${NAVIER_VOLUME_JS}\n</script>" NAVIER_VIEWER_HTML "${NAVIER_VIEWER_HTML}")
    string(LENGTH "${NAVIER_VIEWER_HTML}" html_length)
    set(html_offset 0)
    set(NAVIER_VIEWER_CHUNKS "")
    while(html_offset LESS html_length)
        math(EXPR remaining "${html_length} - ${html_offset}")
        if(remaining GREATER 4000)
            set(chunk_length 4000)
        else()
            set(chunk_length ${remaining})
        endif()
        string(SUBSTRING "${NAVIER_VIEWER_HTML}" ${html_offset} ${chunk_length} chunk)
        string(FIND "${chunk}" "\n" last_newline REVERSE)
        if(last_newline GREATER 0 AND remaining GREATER 4000)
            math(EXPR chunk_length "${last_newline} + 1")
            string(SUBSTRING "${NAVIER_VIEWER_HTML}" ${html_offset} ${chunk_length} chunk)
        endif()
        string(APPEND NAVIER_VIEWER_CHUNKS "R\"NAVIERHTML(${chunk})NAVIERHTML\",\n")
        math(EXPR html_offset "${html_offset} + ${chunk_length}")
    endwhile()
    configure_file(viewer/viewer_template.hpp.in generated/navier/viewer_html.hpp @ONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS viewer/index.html viewer/volume3d.js)
    add_executable(navier_viewer tools/navier_viewer.cpp)
    target_include_directories(navier_viewer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_link_libraries(navier_viewer PRIVATE navier)
    navier_configure_target(navier_viewer)
    if(NAVIER_BUILD_TESTS AND NAVIER_BUILD_EXAMPLES)
        add_test(NAME NavierViewer.Export
            COMMAND ${CMAKE_COMMAND}
                -DVIEWER=$<TARGET_FILE:navier_viewer>
                -DDEMO=$<TARGET_FILE:navier_demo>
                -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/viewer-test/$<CONFIG>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/viewer_test.cmake)
    endif()
endif()
