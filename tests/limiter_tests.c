#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <string.h>

/* Simulated limiter without DPDK dependencies */
struct limiter_sim
{
    uint64_t rate_pps;
    uint64_t tokens;
    uint64_t last_tsc;
    uint64_t tsc_hz;
    uint64_t burst_cap;
};

static inline void limiter_sim_init(struct limiter_sim *l, uint64_t rate_pps, uint64_t tsc_hz, uint64_t initial_tsc)
{
    l->rate_pps = rate_pps;
    l->tokens = rate_pps;
    l->last_tsc = initial_tsc;
    l->tsc_hz = tsc_hz;
    l->burst_cap = rate_pps * 2;
}

static inline uint16_t limiter_sim_allow(struct limiter_sim *l, uint64_t now_tsc, uint16_t want)
{
    uint64_t dt = now_tsc - l->last_tsc;
    if (dt) {
        uint64_t add = (l->rate_pps * dt) / l->tsc_hz;
        if (add) {
            uint64_t t = l->tokens + add;
            l->tokens = (t > l->burst_cap) ? l->burst_cap : t;
            l->last_tsc = now_tsc;
        }
    }
    if (l->tokens == 0) {
        return 0;
    }

    uint16_t allow = (l->tokens < want) ? (uint16_t)l->tokens : want;
    l->tokens -= allow;
    return allow;
}

/* Test constants - simulating 1 GHz TSC frequency */
#define TSC_HZ 1000000000ULL
#define TSC_START 1000000000ULL

/* Helper: calculate TSC for time offset in seconds */
static inline uint64_t tsc_after_seconds(double seconds)
{
    return TSC_START + (uint64_t)(seconds * TSC_HZ);
}

/* Helper: calculate TSC for time offset in milliseconds */
static inline uint64_t tsc_after_ms(uint64_t ms)
{
    return TSC_START + (ms * TSC_HZ / 1000);
}

/* ========== Limiter Tests ========== */

/* Test: Basic usage workflow */
static void test_limiter_basic_usage(void **state)
{
    (void) state;

    struct limiter_sim lim;
    uint64_t rate = 1000;  /* 1000 packets per second */

    /* Initialize limiter at time 0 */
    limiter_sim_init(&lim, rate, TSC_HZ, TSC_START);

    /* Step 1: Forward initial burst of 500 packets */
    uint16_t allowed = limiter_sim_allow(&lim, TSC_START, 500);
    assert_int_equal(allowed, 500);
    assert_int_equal(lim.tokens, 500);

    /* Step 2: Try to forward another 600 packets immediately - only 500 available */
    allowed = limiter_sim_allow(&lim, TSC_START, 600);
    assert_int_equal(allowed, 500);
    assert_int_equal(lim.tokens, 0);

    /* Step 3: Try to forward more - should be blocked */
    allowed = limiter_sim_allow(&lim, TSC_START, 100);
    assert_int_equal(allowed, 0);

    /* Step 4: Wait 0.5 seconds - should get 500 new tokens */
    uint64_t half_sec_later = tsc_after_seconds(0.5);
    allowed = limiter_sim_allow(&lim, half_sec_later, 600);
    assert_int_equal(allowed, 500);  /* Only 500 tokens refilled */
    assert_int_equal(lim.tokens, 0);

    /* Step 5: Wait 1 full second - should get 1000 new tokens */
    uint64_t one_sec_later = tsc_after_seconds(1.5);
    allowed = limiter_sim_allow(&lim, one_sec_later, 1000);
    assert_int_equal(allowed, 1000);
    assert_int_equal(lim.tokens, 0);

    /* Step 6: Wait 5 seconds - should cap at burst_cap (2000) */
    uint64_t five_sec_later = tsc_after_seconds(6.5);
    limiter_sim_allow(&lim, five_sec_later, 0);  /* Trigger refill */
    assert_int_equal(lim.tokens, 2000);  /* Capped at burst_cap */
}

/* Test: Initialization sets correct values */
static void test_limiter_init(void **state)
{
    (void) state;

    struct limiter_sim lim;
    uint64_t rate = 1000;

    limiter_sim_init(&lim, rate, TSC_HZ, TSC_START);

    assert_int_equal(lim.rate_pps, rate);
    assert_int_equal(lim.tokens, rate);
    assert_int_equal(lim.burst_cap, rate * 2);
    assert_int_equal(lim.tsc_hz, TSC_HZ);
    assert_int_equal(lim.last_tsc, TSC_START);
}

/* Test: Initial tokens allow immediate packets */
static void test_limiter_initial_tokens(void **state)
{
    (void) state;

    struct limiter_sim lim;
    uint64_t rate = 100;
    limiter_sim_init(&lim, rate, TSC_HZ, TSC_START);

    /* Should allow up to 'rate' packets immediately */
    uint16_t allowed = limiter_sim_allow(&lim, TSC_START, 50);
    assert_int_equal(allowed, 50);
    assert_int_equal(lim.tokens, 50);

    /* Should allow remaining 50 */
    allowed = limiter_sim_allow(&lim, TSC_START, 50);
    assert_int_equal(allowed, 50);
    assert_int_equal(lim.tokens, 0);
}

/* Test: Limiter blocks when tokens exhausted */
static void test_limiter_exhausted(void **state)
{
    (void) state;

    struct limiter_sim lim;
    uint64_t rate = 100;
    limiter_sim_init(&lim, rate, TSC_HZ, TSC_START);

    /* Exhaust all tokens */
    limiter_sim_allow(&lim, TSC_START, 100);
    assert_int_equal(lim.tokens, 0);

    /* Should not allow more packets */
    uint16_t allowed = limiter_sim_allow(&lim, TSC_START, 10);
    assert_int_equal(allowed, 0);
}

/* Test: Token refill over time */
static void test_limiter_refill(void **state)
{
    (void) state;

    struct limiter_sim lim;
    uint64_t rate = 1000;
    limiter_sim_init(&lim, rate, TSC_HZ, TSC_START);

    /* Exhaust all tokens */
    limiter_sim_allow(&lim, TSC_START, 1000);
    assert_int_equal(lim.tokens, 0);

    /* Wait 0.5 seconds - should get 500 tokens */
    uint64_t half_sec = tsc_after_seconds(0.5);
    uint16_t allowed = limiter_sim_allow(&lim, half_sec, 600);
    assert_int_equal(allowed, 500);

    /* Wait another 0.25 seconds - should get 250 tokens */
    uint64_t quarter_sec = tsc_after_seconds(0.75);
    allowed = limiter_sim_allow(&lim, quarter_sec, 300);
    assert_int_equal(allowed, 250);

    /* Wait 1 full second from last - should get 1000 tokens */
    uint64_t one_sec = tsc_after_seconds(1.75);
    allowed = limiter_sim_allow(&lim, one_sec, 1500);
    assert_int_equal(allowed, 1000);
}

/* Test: Burst capacity limits token accumulation */
static void test_limiter_burst_cap(void **state)
{
    (void) state;

    struct limiter_sim lim;
    uint64_t rate = 1000;
    limiter_sim_init(&lim, rate, TSC_HZ, TSC_START);

    /* Use half the tokens */
    limiter_sim_allow(&lim, TSC_START, 500);
    assert_int_equal(lim.tokens, 500);

    /* Wait 10 seconds (would generate 10000 tokens, but cap is 2000) */
    uint64_t ten_sec = tsc_after_seconds(10.0);
    limiter_sim_allow(&lim, ten_sec, 0);

    /* Should be capped at burst_cap */
    assert_int_equal(lim.tokens, rate * 2);
}

/* Test: Partial allowance when tokens < want */
static void test_limiter_partial_allow(void **state)
{
    (void) state;

    struct limiter_sim lim;
    uint64_t rate = 100;
    limiter_sim_init(&lim, rate, TSC_HZ, TSC_START);

    /* Use 60 tokens, leaving 40 */
    limiter_sim_allow(&lim, TSC_START, 60);
    assert_int_equal(lim.tokens, 40);

    /* Request 50, should get 40 */
    uint16_t allowed = limiter_sim_allow(&lim, TSC_START, 50);
    assert_int_equal(allowed, 40);
    assert_int_equal(lim.tokens, 0);
}

/* Test: Zero rate edge case */
static void test_limiter_zero_rate(void **state)
{
    (void) state;

    struct limiter_sim lim;
    limiter_sim_init(&lim, 0, TSC_HZ, TSC_START);

    assert_int_equal(lim.rate_pps, 0);
    assert_int_equal(lim.tokens, 0);
    assert_int_equal(lim.burst_cap, 0);

    uint16_t allowed = limiter_sim_allow(&lim, TSC_START, 10);
    assert_int_equal(allowed, 0);
}

/* Test: Very high rate */
static void test_limiter_high_rate(void **state)
{
    (void) state;

    struct limiter_sim lim;
    uint64_t rate = 10000000;  /* 10 million pps */
    limiter_sim_init(&lim, rate, TSC_HZ, TSC_START);

    assert_int_equal(lim.rate_pps, rate);
    assert_int_equal(lim.tokens, rate);
    assert_int_equal(lim.burst_cap, rate * 2);

    uint16_t allowed = limiter_sim_allow(&lim, TSC_START, 1000);
    assert_int_equal(allowed, 1000);
}

/* Test: No refill when no time elapsed */
static void test_limiter_no_time_elapsed(void **state)
{
    (void) state;

    struct limiter_sim lim;
    uint64_t rate = 1000;
    limiter_sim_init(&lim, rate, TSC_HZ, TSC_START);

    /* Use some tokens */
    limiter_sim_allow(&lim, TSC_START, 500);
    assert_int_equal(lim.tokens, 500);

    /* Call again with same timestamp - no refill */
    uint16_t allowed = limiter_sim_allow(&lim, TSC_START, 100);
    assert_int_equal(allowed, 100);
    assert_int_equal(lim.tokens, 400);
}

/* Test: Precise millisecond timing */
static void test_limiter_millisecond_precision(void **state)
{
    (void) state;

    struct limiter_sim lim;
    uint64_t rate = 1000;  /* 1 packet per ms */
    limiter_sim_init(&lim, rate, TSC_HZ, TSC_START);

    /* Exhaust tokens */
    limiter_sim_allow(&lim, TSC_START, 1000);
    assert_int_equal(lim.tokens, 0);

    /* Wait exactly 10ms - should get 10 tokens */
    uint64_t ten_ms = tsc_after_ms(10);
    uint16_t allowed = limiter_sim_allow(&lim, ten_ms, 20);
    assert_int_equal(allowed, 10);

    /* Wait another 50ms - should get 50 tokens */
    uint64_t sixty_ms = tsc_after_ms(60);
    allowed = limiter_sim_allow(&lim, sixty_ms, 100);
    assert_int_equal(allowed, 50);
}

/* Test: Multiple consecutive calls */
static void test_limiter_consecutive_calls(void **state)
{
    (void) state;

    struct limiter_sim lim;
    uint64_t rate = 1000;
    limiter_sim_init(&lim, rate, TSC_HZ, TSC_START);

    uint64_t total_allowed = 0;

    /* Make 10 calls requesting 100 each */
    for (int i = 0; i < 10; i++) {
        uint16_t allowed = limiter_sim_allow(&lim, TSC_START, 100);
        total_allowed += allowed;
    }

    /* Should have allowed exactly 1000 total */
    assert_int_equal(total_allowed, 1000);
    assert_int_equal(lim.tokens, 0);
}

/* Test: Gradual token consumption and refill */
static void test_limiter_gradual_usage(void **state)
{
    (void) state;

    struct limiter_sim lim;
    uint64_t rate = 100;  /* 100 pps = 10 tokens per 100ms */
    limiter_sim_init(&lim, rate, TSC_HZ, TSC_START);

    /* Consume 10 packets every 100ms for 1 second */
    /* At 100 pps, we should refill exactly what we consume */
    for (int i = 0; i < 10; i++) {
        uint64_t time = tsc_after_ms(i * 100);
        uint16_t allowed = limiter_sim_allow(&lim, time, 10);
        assert_int_equal(allowed, 10);
    }

    /* After 1 second, we consumed 100 and refilled 90 (9 intervals of 100ms) */
    /* Initial 100 - consumed 100 + refilled 90 = 90 tokens remaining */
    assert_int_equal(lim.tokens, 90);

    /* Wait another 100ms to refill the last 10 */
    uint64_t final_time = tsc_after_ms(1000);
    limiter_sim_allow(&lim, final_time, 0);
    assert_int_equal(lim.tokens, 100);
}

const struct CMUnitTest limiter_tests[] = {
    cmocka_unit_test(test_limiter_basic_usage),
    cmocka_unit_test(test_limiter_init),
    cmocka_unit_test(test_limiter_initial_tokens),
    cmocka_unit_test(test_limiter_exhausted),
    cmocka_unit_test(test_limiter_refill),
    cmocka_unit_test(test_limiter_burst_cap),
    cmocka_unit_test(test_limiter_partial_allow),
    cmocka_unit_test(test_limiter_zero_rate),
    cmocka_unit_test(test_limiter_high_rate),
    cmocka_unit_test(test_limiter_no_time_elapsed),
    cmocka_unit_test(test_limiter_millisecond_precision),
    cmocka_unit_test(test_limiter_consecutive_calls),
    cmocka_unit_test(test_limiter_gradual_usage),
};

/* Public function to run all limiter tests */
int run_limiter_tests(void)
{
    return cmocka_run_group_tests(limiter_tests, NULL, NULL);
}

