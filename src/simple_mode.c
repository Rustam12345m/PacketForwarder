#include "app.h"
#include "forwarder.h"

#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_prefetch.h>
#include <rte_launch.h>

struct worker_args
{
    struct forwarder_ctx fctx;
    uint16_t port0;
    uint16_t port1;
    uint16_t qid;
};

static inline void process_direction(uint16_t in_port, uint16_t out_port,
                                     uint16_t qid,
                                     struct forwarder_ctx *fctx,
                                     struct statistics *stat)
{
    struct rte_mbuf *bufs[MAX_BURST];
    uint16_t num_rx = rte_eth_rx_burst(in_port, qid, bufs, MAX_BURST);
    if (num_rx == 0) {
        return;
    }
    stat->rx_pkts += num_rx;

    // rx static
    uint64_t rx_bytes = 0;
    for (uint16_t i=0;i<num_rx;++i) {
        rte_prefetch0(rte_pktmbuf_mtod(bufs[i], void *));
        rx_bytes += rte_pktmbuf_pkt_len(bufs[i]);
    }
    stat->rx_bytes += rx_bytes;

    // packet pipeline
    uint16_t num_tx_req = forwarder(bufs, num_rx, fctx, out_port);
    if (num_tx_req == 0) {
        return;
    }

    uint16_t num_tx = rte_eth_tx_burst(out_port, qid, bufs, num_tx_req);
    stat->tx_pkts += num_tx;

    // tx static
    uint64_t tx_bytes = 0;
    for (uint16_t i=0;i<num_tx;++i) {
        tx_bytes += rte_pktmbuf_pkt_len(bufs[i]);
    }
    stat->tx_bytes += tx_bytes;

    if (num_tx < num_tx_req) {
        stat->drop_tx += (uint64_t)(num_tx_req - num_tx);
        for (uint16_t i=num_tx;i<num_tx_req;++i) {
            rte_pktmbuf_free(bufs[i]);
        }
    }
}

static int lcore_worker_thread(void *worker)
{
    struct worker_args *arg = (struct worker_args *)worker;
    struct statistics *stat = arg->fctx.stats;
    memset(stat, 0, sizeof(struct statistics));

    while (!g_quit) {
        process_direction(arg->port0, arg->port1, arg->qid, &arg->fctx, stat);
        process_direction(arg->port1, arg->port0, arg->qid, &arg->fctx, stat);
    }
    return 0;
}

int run_simple_pipeline(struct application *app)
{
    struct rte_ether_addr mac0, mac1;
    rte_eth_macaddr_get(app->cfg.port0, &mac0);
    rte_eth_macaddr_get(app->cfg.port1, &mac1);

    /* launch worker threads */
    struct worker_args arg_list[RTE_MAX_LCORE];
    for (unsigned i=0;i<app->state.nb_workers;++i) {
        unsigned lc = app->state.workers[i];

        struct worker_args *arg = &arg_list[lc];
        arg->port0 = app->cfg.port0;
        arg->port1 = app->cfg.port1;
        arg->qid   = i;

        arg->fctx.drop_non_ip = app->cfg.drop_non_ip;
        arg->fctx.port0 = app->cfg.port0;
        arg->fctx.mac_mode = app->cfg.mac_mode;
        if (app->cfg.mac_mode == MAC_REWRITE_SRC_DST) {
            memcpy(arg->fctx.dmac, app->cfg.dmac, 6);
        }

        memcpy(arg->fctx.smac0, mac0.addr_bytes, 6);
        memcpy(arg->fctx.smac1, mac1.addr_bytes, 6);

        arg->fctx.rate_limit = app->cfg.rate_limit;
        arg->fctx.rate_limit_pps = app->cfg.rate_limit_pps;
        arg->fctx.stats = &app->state.stats[lc];

        /* run worker thread on lcore */
        rte_eal_remote_launch(lcore_worker_thread, arg, lc);
    }

    /* control/statistics */
    run_stats_loop((void *)app);

    /* wait worker threads */
    for (unsigned i=0;i<app->state.nb_workers;++i) {
        unsigned lc = app->state.workers[i];
        rte_eal_wait_lcore(lc);
    }
    return 0;
}

