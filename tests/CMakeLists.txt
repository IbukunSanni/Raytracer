# ---------------------------------------------------------------------
# Tests
#
#     cmake --build build && ctest --test-dir build --output-on-failure
#
# Two binaries, split by what a failure would mean:
#
#   bsdf_test    a material is wrong. No scene, no image, no renderer --
#                it links Material.cpp and nothing else, so it can run
#                before the integrator exists and cannot be broken by it.
#
#   render_test  a picture is wrong. Runs the raytracer as a subprocess
#                and reads the PNG back, so it covers the path loop, the
#                transform stack and the image writer.
#
# doctest_discover_tests runs each binary once at build time and registers
# every test case it finds as its own CTest entry. Nothing lists test names
# twice, ctest runs them in parallel, and a failure names one case.
# ---------------------------------------------------------------------

include(${CMAKE_SOURCE_DIR}/third_party/doctest/cmake/doctest.cmake)

# Shared by both binaries: the doctest-generated main(), the include paths,
# and the warning level.
add_library(test_main STATIC support/main.cpp)
target_include_directories(test_main SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/third_party)

add_library(test_support INTERFACE)
target_link_libraries(test_support INTERFACE test_main)
target_include_directories(test_support INTERFACE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR})

function(add_doctest_binary name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE test_support)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W3)
    else()
        target_compile_options(${name} PRIVATE -Wall)
    endif()

    # Scene paths inside the tests are written relative to the repo root,
    # the same way you would type them on the command line.
    # ADD_LABELS turns each TEST_SUITE into a ctest label, so a whole area
    # can be run on its own: ctest -L "bsdf/metal".
    doctest_discover_tests(${name}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        ADD_LABELS 1)
endfunction()

# ---------------------------------------------------------------------
add_doctest_binary(bsdf_test
    bsdf_test.cpp
    ${CMAKE_SOURCE_DIR}/src/scene/Material.cpp)

# ---------------------------------------------------------------------
add_doctest_binary(render_test render_test.cpp)
target_link_libraries(render_test PRIVATE lodepng)

# The renderer under test, by absolute path, so the tests never guess at
# a build layout and never pick up a stale binary from somewhere else.
target_compile_definitions(render_test PRIVATE
    RAYTRACER_EXE="$<TARGET_FILE:raytracer>")
add_dependencies(render_test raytracer)
