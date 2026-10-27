# ---------------------------------------------------------------------
# Every test case carries its suite's label.
#
#     cmake -D CTEST=<ctest> -D BUILD_DIR=<build> -D "BINARIES=<a;b;...>"
#           -P check_labels.cmake
#
# For each TEST_SUITE a binary reports, the number of its test cases
# (doctest's own count) must equal the number of tests that
#
#     ctest -N -L ^<suite>$
#
# lists. A name doctest_discover_tests mishandles -- a comma once dropped
# the label, an unbalanced '[' drops the add_test -- shows up here as a
# shortfall, instead of as a test that `ctest -L` silently skips.
#
# Counts are summed across binaries, so two binaries may share a suite.
# ---------------------------------------------------------------------

# The N in doctest's "... test cases passing the current filters: N".
# Read from the summary line, not by splitting the listing, so a '[' in a
# test name cannot unbalance the list.
function(count_cases exe filter out_var)
  execute_process(
    COMMAND "${exe}" ${filter} --list-test-cases
    OUTPUT_VARIABLE output
    RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${exe} --list-test-cases failed:\n${output}")
  endif()
  string(REGEX MATCH "unskipped test cases passing the current filters: ([0-9]+)"
      _ "${output}")
  set(${out_var} ${CMAKE_MATCH_1} PARENT_SCOPE)
endfunction()

# Suite name -> expected count, as suite_<i> and expected_<i>.
set(suites)
foreach(exe IN LISTS BINARIES)
  execute_process(
    COMMAND "${exe}" --list-test-suites
    OUTPUT_VARIABLE output
    RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${exe} --list-test-suites failed:\n${output}")
  endif()
  string(REPLACE "\n" ";" lines "${output}")
  foreach(suite IN LISTS lines)
    if(suite STREQUAL "" OR suite MATCHES "^(=+|\\[doctest\\] .*)$")
      continue()
    endif()
    # doctest splits filters on commas, the same trap as in discovery.
    string(REPLACE "," "\\," filter "${suite}")
    count_cases("${exe}" "--test-suite=${filter}" count)
    list(FIND suites "${suite}" index)
    if(index EQUAL -1)
      list(LENGTH suites index)
      list(APPEND suites "${suite}")
      set(expected_${index} 0)
    endif()
    math(EXPR expected_${index} "${expected_${index}} + ${count}")
  endforeach()
endforeach()

set(failures "")
list(LENGTH suites suite_count)
math(EXPR last "${suite_count} - 1")
foreach(index RANGE ${last})
  list(GET suites ${index} suite)
  # -L is a regex; anchor it, so bsdf/metal does not also count
  # bsdf/metal-rough.
  string(REGEX REPLACE "([][.+*?^$(){}|\\\\])" "\\\\\\1" pattern "${suite}")
  execute_process(
    COMMAND "${CTEST}" --test-dir "${BUILD_DIR}" -N -L "^${pattern}$"
    OUTPUT_VARIABLE output
    RESULT_VARIABLE result)
  string(REGEX MATCH "Total Tests: ([0-9]+)" _ "${output}")
  set(labelled "${CMAKE_MATCH_1}")
  if(NOT result EQUAL 0 OR NOT labelled EQUAL expected_${index})
    string(APPEND failures
        "\n  ${suite}: ${expected_${index}} test cases, "
        "ctest -L lists ${labelled}")
  endif()
endforeach()

if(failures)
  message(FATAL_ERROR "Test cases missing their suite label:${failures}")
endif()
message(STATUS "ctest labels: all ${suite_count} suites complete")
