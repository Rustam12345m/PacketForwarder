#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

/* External test suite runners */
extern int run_limiter_tests(void);

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    int result = 0;

    /* Run all test suites */
    result |= run_limiter_tests();

    return result;
}


