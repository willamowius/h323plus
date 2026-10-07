// Minimal, dependency-free unit test framework for the OpenH264 H323Plus
// plugin. No external test library is assumed (the surrounding project
// has none), so this stays intentionally small: a handful of assertion
// macros, a self-registering test list, and a runner that prints a
// pass/fail summary and returns a process exit code suitable for `make
// check`. Not a general-purpose framework - just enough to make the
// plugin's regression tests self-contained and easy to run in CI.
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

namespace testfw {

struct TestCase {
  const char * name;
  std::function<void()> fn;
};

inline std::vector<TestCase> & Registry() {
  static std::vector<TestCase> reg;
  return reg;
}

struct Registrar {
  Registrar(const char * name, std::function<void()> fn) {
    Registry().push_back({name, fn});
  }
};

// Thrown by the ASSERT_* macros on failure; caught by the runner so one
// failing assertion fails just that test, not the whole binary.
struct AssertionFailure {
  std::string message;
};

inline int & FailCounter() { static int n = 0; return n; }

inline int RunAll() {
  int total = 0, failed = 0;
  for (auto & tc : Registry()) {
    total++;
    printf("[ RUN      ] %s\n", tc.name);
    try {
      tc.fn();
      printf("[       OK ] %s\n", tc.name);
    }
    catch (const AssertionFailure & f) {
      failed++;
      printf("[  FAILED  ] %s: %s\n", tc.name, f.message.c_str());
    }
    catch (const std::exception & e) {
      failed++;
      printf("[  FAILED  ] %s: unexpected exception: %s\n", tc.name, e.what());
    }
    catch (...) {
      failed++;
      printf("[  FAILED  ] %s: unexpected unknown exception\n", tc.name);
    }
  }
  printf("----\n%d test%s, %d failed\n", total, total == 1 ? "" : "s", failed);
  return failed == 0 ? 0 : 1;
}

} // namespace testfw

#define TEST(suite, name) \
  static void suite##_##name##_fn(); \
  static testfw::Registrar suite##_##name##_reg(#suite "." #name, suite##_##name##_fn); \
  static void suite##_##name##_fn()

#define TESTFW_FAIL(msg) \
  do { \
    char buf[512]; \
    snprintf(buf, sizeof(buf), "%s:%d: %s", __FILE__, __LINE__, (msg)); \
    throw testfw::AssertionFailure{buf}; \
  } while (0)

#define ASSERT_TRUE(cond) \
  do { if (!(cond)) TESTFW_FAIL("ASSERT_TRUE(" #cond ") failed"); } while (0)

#define ASSERT_FALSE(cond) \
  do { if (cond) TESTFW_FAIL("ASSERT_FALSE(" #cond ") failed"); } while (0)

#define ASSERT_EQ(a, b) \
  do { \
    if (!((a) == (b))) { \
      char buf[512]; \
      snprintf(buf, sizeof(buf), "%s:%d: ASSERT_EQ(" #a ", " #b ") failed", __FILE__, __LINE__); \
      throw testfw::AssertionFailure{buf}; \
    } \
  } while (0)

#define ASSERT_NE(a, b) \
  do { if ((a) == (b)) TESTFW_FAIL("ASSERT_NE(" #a ", " #b ") failed"); } while (0)

#endif // TEST_FRAMEWORK_H
