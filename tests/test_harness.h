// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cmath>
#include <cstdio>

// The check-and-count the four suites in this directory share.
//
// Deliberately not a test framework: these suites build in the same CMake
// project as a Windows game plugin, and taking a dependency on one to assert on
// a few dozen floats would cost more than it returns. What is here is the part
// every suite had written for itself - a counter, a failure line naming the
// file and line, and an exit code - kept in one place so a suite added later
// reports the same way as the ones already here.
namespace hol_test {

inline int g_checks = 0;
inline int g_failures = 0;

inline void Record(bool ok, const char* what, const char* file, int line) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::printf("FAIL %s:%d  %s\n", file, line, what);
}

inline void RecordNear(double actual, double expected, double tolerance,
                       const char* what, const char* file, int line) {
    ++g_checks;
    if (std::fabs(actual - expected) <= tolerance) return;
    ++g_failures;
    std::printf("FAIL %s:%d  %s (expected %.9f, got %.9f)\n",
                file, line, what, expected, actual);
}

// Print the tally and hand main() its exit code.
inline int Report() {
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

}  // namespace hol_test

// The expression itself is the description.
#define CHECK(expr) ::hol_test::Record((expr), #expr, __FILE__, __LINE__)
#define CHECK_NEAR(actual, expected, tolerance) \
    ::hol_test::RecordNear((actual), (expected), (tolerance), \
                           #actual " ~= " #expected, __FILE__, __LINE__)

// Where the expression alone does not say what the number means - a sign
// convention, an axis mapping - the description carries it instead.
#define CHECK_MSG(expr, what) ::hol_test::Record((expr), (what), __FILE__, __LINE__)
#define CHECK_NEAR_MSG(actual, expected, tolerance, what) \
    ::hol_test::RecordNear((actual), (expected), (tolerance), (what), __FILE__, __LINE__)
