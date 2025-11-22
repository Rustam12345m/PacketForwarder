#include "app.h"
#include "forwarder.h"

#include <rte_ring.h>
#include <rte_launch.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_pause.h>

const unsigned RING_SIZE = 4096;

struct lcore_arg
{
    struct multicore_ctx *ctx;
    struct statistics *stats;
    struct forwarder_ctx fctx;
    uint16_t port0;
    uint16_t port1;
    uint16_t qid;
};

static inline void rx_process_port(uint16_t pid, uint16_t qid,
                                   struct rte_ring *ring,
                                   struct statistics *stats)
{
    struct rte_mbuf *bufs[MAX_BURST];
    uint16_t num = rte_eth_rx_burst(pid, qid, bufs, MAX_BURST);
    if (num) {
        stats->rx_pkts += num;
        for (uint16_t i = 0; i < num; ++i) {
            stats->rx_bytes += rte_pktmbuf_pkt_len(bufs[i]);
        }

        uint16_t enq = rte_ring_sp_enqueue_burst(ring,
                                                 (void **)bufs,
                                                 num,
                                                 NULL);
        if (enq < num) {
            stats->drop_tx += num - enq;
            for (uint16_t i = enq; i < num; ++i) {
                rte_pktmbuf_free(bufs[i]);
            }
        }
    }
}

static inline void tx_process_port(uint16_t pid, uint16_t qid,
                                   struct rte_ring *ring,
                                   struct statistics *stats)
{
    struct rte_mbuf *bufs[MAX_BURST];
    uint16_t num = rte_ring_sc_dequeue_burst(ring,
                                             (void **)bufs,
                                             MAX_BURST,
                                             NULL);
    if (num) {
        uint16_t sent = rte_eth_tx_burst(pid, qid, bufs, num);
        stats->tx_pkts += sent;
        for (uint16_t i = 0; i < sent; ++i) {
            stats->tx_bytes += rte_pktmbuf_pkt_len(bufs[i]);
        }

        if (sent < num) {
            stats->drop_tx += (uint64_t)(num - sent);
            for (uint16_t i = sent; i < num; ++i) {
                rte_pktmbuf_free(bufs[i]);
            }
        }
    }
}

static inline uint16_t process_direction(struct rte_ring *rx,
                                         struct rte_ring *tx,
                                         uint16_t out_port,
                                         struct forwarder_ctx *fctx,
                                         struct statistics *stats)
{
    struct rte_mbuf *bufs[MAX_BURST];
    uint16_t num = rte_ring_sc_dequeue_burst(rx,
                                             (void **)bufs,
                                             MAX_BURST,
                                             NULL);
    if (num > 0) {
        uint16_t num_tx_req = forwarder(bufs, num, fctx, out_port);
        if (num_tx_req > 0) {
            uint16_t enq = rte_ring_sp_enqueue_burst(tx, (void **)bufs, num_tx_req, NULL);
            if (enq < num_tx_req) {
                stats->drop_tx += (num_tx_req - enq);
                for (uint16_t i = enq; i < num_tx_req; ++i) {
                    rte_pktmbuf_free(bufs[i]);
                }
            }
        }
    }
    return num;
}

static int rx_lcore_thread(void *worker)
{
    struct lcore_arg *arg = (struct lcore_arg *)worker;
    struct multicore_ctx *ctx = arg->ctx;
    struct statistics *stats = arg->stats;
    memset(stats, 0, sizeof(struct statistics));

    while (!g_quit) {
        rx_process_port(arg->port0, arg->qid, ctx->ring_rx0[arg->qid], stats);

        rx_process_port(arg->port1, arg->qid, ctx->ring_rx1[arg->qid], stats);

        rte_pause();
    }
    return 0;
}

static int tx_lcore_thread(void *worker)
{
    struct lcore_arg *arg = (struct lcore_arg *)worker;
    struct multicore_ctx *ctx = arg->ctx;
    struct statistics *stats = arg->stats;
    memset(stats, 0, sizeof(struct statistics));

    while (!g_quit) {
        tx_process_port(arg->port0, arg->qid, ctx->ring_tx0[arg->qid], stats);

        tx_process_port(arg->port1, arg->qid, ctx->ring_tx1[arg->qid], stats);

        rte_pause();
    }
    return 0;
}

static int worker_lcore_thread(void *worker)
{
    struct lcore_arg *arg = (struct lcore_arg *)worker;
    struct multicore_ctx *ctx = arg->ctx;
    struct statistics *stats = arg->stats;
    memset(stats, 0, sizeof(struct statistics));

    while (!g_quit) {
        /* Process packets from port0 -> to port1 */
        uint16_t n0 = process_direction(ctx->ring_rx0[arg->qid],
                                        ctx->ring_tx1[arg->qid],
                                        arg->port1,
                                        &arg->fctx,
                                        stats);

        /* Process packets from port1 -> to port0 */
        uint16_t n1 = process_direction(ctx->ring_rx1[arg->qid],
                                        ctx->ring_tx0[arg->qid],
                                        arg->port0,
                                        &arg->fctx,
                                        stats);

        if (n0 == 0 && n1 == 0) {
            rte_pause();
        }
    }
    return 0;
}

int multicore_ctx_init(struct multicore_ctx *ctx, uint16_t queues)
{
    if (!ctx || queues == 0 || queues > MAX_QUEUES) {
        return -1;
    }

    multicore_ctx_cleanup(ctx);

    ctx->queues = queues;
    for (uint16_t q=0;q<queues;++q) {
        char name[64];
        snprintf(name, sizeof(name), "ring_rx0_%u", q);
        ctx->ring_rx0[q] = rte_ring_create(name, RING_SIZE,
                                           rte_socket_id(),
                                           RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (!ctx->ring_rx0[q]) {
            return -1;
        }

        snprintf(name, sizeof(name), "ring_rx1_%u", q);
        ctx->ring_rx1[q] = rte_ring_create(name, RING_SIZE,
                                           rte_socket_id(),
                                           RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (!ctx->ring_rx1[q]) {
            return -2;
        }

        snprintf(name, sizeof(name), "ring_tx0_%u", q);
        ctx->ring_tx0[q] = rte_ring_create(name, RING_SIZE,
                                           rte_socket_id(),
                                           RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (!ctx->ring_tx0[q]) {
            return -3;
        }

        snprintf(name, sizeof(name), "ring_tx1_%u", q);
        ctx->ring_tx1[q] = rte_ring_create(name, RING_SIZE,
                                           rte_socket_id(),
                                           RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (!ctx->ring_tx1[q]) {
            return -4;
        }
    }
    return 0;
}

void multicore_ctx_cleanup(struct multicore_ctx *ctx)
{
    for (uint16_t q=0;q<MAX_QUEUES;++q) {
        if (ctx->ring_rx0[q]) {
            rte_ring_free(ctx->ring_rx0[q]);
            ctx->ring_rx0[q] = NULL;
        }
        if (ctx->ring_rx1[q]) {
            rte_ring_free(ctx->ring_rx1[q]);
            ctx->ring_rx1[q] = NULL;
        }
        if (ctx->ring_tx0[q]) {
            rte_ring_free(ctx->ring_tx0[q]);
            ctx->ring_tx0[q] = NULL;
        }
        if (ctx->ring_tx1[q]) {
            rte_ring_free(ctx->ring_tx1[q]);
            ctx->ring_tx1[q] = NULL;
        }
    }
    ctx->queues = 0;
}

int run_multicore_mode(struct application *app)
{
    if (multicore_ctx_init(&app->state.multicore, app->cfg.queues) != 0) {
        fprintf(stderr, "ring create failed\n");
        return -1;
    }

    /* MAC addresses */
    struct rte_ether_addr mac0, mac1;
    rte_eth_macaddr_get(app->cfg.port0, &mac0);
    rte_eth_macaddr_get(app->cfg.port1, &mac1);

    struct lcore_arg args[RTE_MAX_LCORE];
    memset(&args, 0, sizeof(args));

    /* launch Worker threads */
    for (unsigned i=0;i<app->state.nb_workers;++i) {
        unsigned lc = app->state.workers[i];
        uint16_t qid = (uint16_t)i;

        struct lcore_arg *a = &args[lc];
        a->ctx = &app->state.multicore;
        a->stats = &app->state.stats[lc];
        a->port0 = app->cfg.port0;
        a->port1 = app->cfg.port1;
        a->qid   = qid;

        a->fctx.drop_non_ip = app->cfg.drop_non_ip;
        a->fctx.port0 = app->cfg.port0;
        a->fctx.mac_mode = app->cfg.mac_mode;
        if (app->cfg.mac_mode == MAC_REWRITE_SRC_DST) {
            memcpy(a->fctx.dmac, app->cfg.dmac, 6);
        }

        memcpy(a->fctx.smac0, mac0.addr_bytes, 6);
        memcpy(a->fctx.smac1, mac1.addr_bytes, 6);

        a->fctx.rate_limit = app->cfg.rate_limit;
        a->fctx.rate_limit_pps = app->cfg.rate_limit_pps;
        a->fctx.stats = &app->state.stats[lc];

        rte_eal_remote_launch(worker_lcore_thread, a, lc);
    }

    /* launch TX threads */
    for (unsigned i=0;i<app->state.nb_tx;++i) {
        unsigned lc = app->state.tx_lcores[i];
        uint16_t qid = (uint16_t)i;

        struct lcore_arg *a = &args[lc];
        a->ctx = &app->state.multicore;
        a->stats = &app->state.stats[lc];
        a->port0 = app->cfg.port0;
        a->port1 = app->cfg.port1;
        a->qid   = qid;

        rte_eal_remote_launch(tx_lcore_thread, a, lc);
    }

    /* launch RX threads */
    for (unsigned i=0;i<app->state.nb_rx;++i) {
        unsigned lc = app->state.rx_lcores[i];
        uint16_t qid = (uint16_t)i;

        struct lcore_arg *a = &args[lc];
        a->ctx = &app->state.multicore;
        a->stats = &app->state.stats[lc];
        a->port0 = app->cfg.port0;
        a->port1 = app->cfg.port1;
        a->qid   = qid;

        rte_eal_remote_launch(rx_lcore_thread, a, lc);
    }

    /* control/statistics */
    run_stats_loop((void *)app);

    /* wait threads */
    for (unsigned i=0;i<app->state.nb_rx;++i) {
        rte_eal_wait_lcore(app->state.rx_lcores[i]);
    }
    for (unsigned i=0;i<app->state.nb_workers;++i) {
        rte_eal_wait_lcore(app->state.workers[i]);
    }
    for (unsigned i=0;i<app->state.nb_tx;++i) {
        rte_eal_wait_lcore(app->state.tx_lcores[i]);
    }
    return 0;
}

