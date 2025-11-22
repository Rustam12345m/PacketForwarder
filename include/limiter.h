#ifndef LIMITER_H
#define LIMITER_H

#include <stdint.h>
#include <rte_cycles.h>

struct limiter
{
    uint64_t rate_pps;
    uint64_t tokens;
    uint64_t last_tsc;
    uint64_t tsc_hz;
    uint64_t burst_cap;
};

static inline void limiter_init(struct limiter *l, uint64_t rate_pps)
{
    l->rate_pps = rate_pps;
    l->tokens = rate_pps;
    l->last_tsc = rte_rdtsc();
    l->tsc_hz = rte_get_tsc_hz();
    l->burst_cap = rate_pps * 2;
}

static inline uint16_t limiter_allow(struct limiter *l, uint64_t now_tsc, uint16_t want)
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

#endif

