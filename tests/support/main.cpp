// The entry point for every test binary in this directory.
//
// doctest generates main() from this one define, and having it in its own
// translation unit keeps the 300KB header out of every rebuild of an
// actual test file.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
