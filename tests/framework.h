#pragma once
#include <cstdio>
#include <cstdlib>

// Minimal test runner. No external deps, ASan-transparent.
//
// Usage:
//   RUN(fn)         — set context name and call fn()
//   CHECK(expr)     — non-fatal assertion
//   CHECK_EQ(a, b)  — equality shorthand
//   CHECK_NULL(p)   — expects nullptr
//   CHECK_NOTNULL(p)
//   REQUIRE(expr)   — fatal (exit 1 on failure)
//   CAPY_TEST_MAIN()— print summary, return 1 if any CHECK failed

static int g_pass = 0;
static int g_fail = 0;
static const char *g_test = "";

#define RUN(fn) do { g_test = #fn; fn(); } while (0)

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "  FAIL  [%s] %s:%d  %s\n", \
                    g_test, __FILE__, __LINE__, #expr); \
            g_fail++; \
        } else { \
            g_pass++; \
        } \
    } while (0)

#define CHECK_EQ(a, b)   CHECK((a) == (b))
#define CHECK_NULL(p)    CHECK((p) == nullptr)
#define CHECK_NOTNULL(p) CHECK((p) != nullptr)

#define REQUIRE(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "  FATAL [%s] %s:%d  %s\n", \
                    g_test, __FILE__, __LINE__, #expr); \
            std::exit(1); \
        } \
    } while (0)

#define CAPY_TEST_MAIN() \
    do { \
        int total = g_pass + g_fail; \
        if (g_fail == 0) \
            fprintf(stdout, "OK  %d/%d passed\n", g_pass, total); \
        else \
            fprintf(stdout, "FAIL  %d/%d passed, %d failed\n", \
                    g_pass, total, g_fail); \
        return g_fail > 0 ? 1 : 0; \
    } while (0)
