#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_ring.h>
#include <rte_mempool.h>

#define MAX_BURST       64
#define MAX_QUEUES      32

enum APP_MODE
{
    MODE_SIMPLE = 0,
    MODE_MULTICORE = 1
};

enum MAC_SUBST_MODE
{
    MAC_REWRITE_NONE = 0,
    MAC_REWRITE_SRC = 1,
    MAC_REWRITE_SRC_DST = 2
};

struct statistics
{
    uint64_t rx_pkts;
    uint64_t rx_bytes;

    uint64_t tx_pkts;
    uint64_t tx_bytes;

    uint64_t drop_non_ip;
    uint64_t drop_tx;
    uint64_t drop_rate_limit;
} __rte_cache_aligned;

struct app_config
{
    enum APP_MODE mode;

    uint16_t port0;
    uint16_t port1;

    uint16_t queues;

    uint32_t stat_sec;

    uint8_t promisc;
    uint8_t drop_non_ip;

    enum MAC_SUBST_MODE mac_mode;
    uint8_t dmac[6];

    uint8_t rate_limit;
    uint64_t rate_limit_pps;
};

struct multicore_ctx
{
    uint16_t queues;
    struct rte_ring* ring_rx0[MAX_QUEUES]; /* RX from port0 */
    struct rte_ring* ring_rx1[MAX_QUEUES];
    struct rte_ring* ring_tx0[MAX_QUEUES]; /* TX to port0 */
    struct rte_ring* ring_tx1[MAX_QUEUES];
};

struct app_state
{
    /* Multicore-mode context */
    struct multicore_ctx multicore;

    unsigned rx_lcores[RTE_MAX_LCORE];
    unsigned nb_rx;

    unsigned tx_lcores[RTE_MAX_LCORE];
    unsigned nb_tx;

    unsigned workers[RTE_MAX_LCORE];
    unsigned nb_workers;

    struct rte_mempool *pool;

    struct statistics stats[RTE_MAX_LCORE];
};

struct application
{
    /* Configuration */
    struct app_config cfg;

    /* State */
    struct app_state state;
};

extern volatile uint8_t g_quit;

int application_init(struct application *app);
void application_cleanup(struct application *app);
int application_parse_args(int argc, char **argv, struct application *app);

int simple_pipeline_init(struct application *app);
int multicore_mode_init(struct application *app);

int run_simple_pipeline(struct application *app);
int run_multicore_mode(struct application *app);

int multicore_ctx_init(struct multicore_ctx *ctx, uint16_t queues);
void multicore_ctx_cleanup(struct multicore_ctx *ctx);

int init_signals(void);
int run_stats_loop(void *arg);

#endif

