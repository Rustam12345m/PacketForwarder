#include "app.h"

#include <inttypes.h>
#include <unistd.h>
#include <time.h>

static void sum_all(const struct application *app, struct statistics *out)
{
    struct statistics total = {0};

    switch (app->cfg.mode) {
    case MODE_SIMPLE: {
        for (unsigned i=0;i<app->state.nb_workers;++i) {
            unsigned lc = app->state.workers[i];
            const struct statistics *s = &app->state.stats[lc];

            total.rx_pkts += s->rx_pkts;
            total.rx_bytes += s->rx_bytes;
            total.tx_pkts += s->tx_pkts;
            total.tx_bytes += s->tx_bytes;
            total.drop_non_ip += s->drop_non_ip;
            total.drop_tx += s->drop_tx;
            total.drop_rate_limit += s->drop_rate_limit;
        }
        break;
    }
    case MODE_MULTICORE: {
        for (unsigned i=0;i<app->state.nb_rx;++i) {
            unsigned lc = app->state.rx_lcores[i];
            const struct statistics *s = &app->state.stats[lc];
            total.rx_pkts += s->rx_pkts;
            total.rx_bytes += s->rx_bytes;
            total.drop_tx += s->drop_tx;
        }
        
        for (unsigned i=0;i<app->state.nb_workers;++i) {
            unsigned lc = app->state.workers[i];
            const struct statistics *s = &app->state.stats[lc];
            total.drop_non_ip += s->drop_non_ip;
            total.drop_rate_limit += s->drop_rate_limit;
            total.drop_tx += s->drop_tx;
        }
        
        for (unsigned i=0;i<app->state.nb_tx;++i) {
            unsigned lc = app->state.tx_lcores[i];
            const struct statistics *s = &app->state.stats[lc];
            total.tx_pkts += s->tx_pkts;
            total.tx_bytes += s->tx_bytes;
            total.drop_tx += s->drop_tx;
        }
        break;
    }
    }
    *out = total;
}

static void print_line(double dt_sec,
                       const struct statistics *cur,
                       const struct statistics *prev)
{
    uint64_t drx_pkts = cur->rx_pkts - prev->rx_pkts;
    uint64_t dtx_pkts = cur->tx_pkts - prev->tx_pkts;
    uint64_t drx_bytes = cur->rx_bytes - prev->rx_bytes;
    uint64_t dtx_bytes = cur->tx_bytes - prev->tx_bytes;

    double rx_pps = dt_sec > 0 ? (double)drx_pkts / dt_sec : 0.0;
    double tx_pps = dt_sec > 0 ? (double)dtx_pkts / dt_sec : 0.0;
    double rx_bps = dt_sec > 0 ? (8.0 * (double)drx_bytes) / dt_sec : 0.0;
    double tx_bps = dt_sec > 0 ? (8.0 * (double)dtx_bytes) / dt_sec : 0.0;

    printf(
        "RX %" PRIu64 " pkts  TX %" PRIu64 " pkts  "
        "DROP(non-ip %" PRIu64 ", tx %" PRIu64 ", rate %" PRIu64 ")  "
        "RXpps %.0f  TXpps %.0f  RXbps %.0f  TXbps %.0f\n",
        cur->rx_pkts, cur->tx_pkts,
        cur->drop_non_ip, cur->drop_tx, cur->drop_rate_limit,
        rx_pps, tx_pps, rx_bps, tx_bps
    );
}

int run_stats_loop(void *arg)
{
    const struct application *app = (const struct application *)arg;
    const unsigned period_sec = app->cfg.stat_sec;
    const unsigned sleep_us = (period_sec * 1000000) / 10;

    struct statistics prev = {0}, cur = {0};
    time_t last_print = time(NULL);
    sum_all(app, &prev);

    while (!g_quit) {
        usleep(sleep_us);

        time_t now = time(NULL);
        double elapsed = difftime(now, last_print);

        if (elapsed >= (double)period_sec) {
            sum_all(app, &cur);
            print_line(elapsed, &cur, &prev);

            prev = cur;
            last_print = now;
        }
    }

    sum_all(app, &cur);
    printf("Final: RX %" PRIu64 " TX %" PRIu64 " DROP %" PRIu64 "\n",
           cur.rx_pkts, cur.tx_pkts,
           cur.drop_non_ip + cur.drop_tx + cur.drop_rate_limit);
    return 0;
}

