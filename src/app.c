#include "app.h"

#include <stdio.h>
#include <unistd.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>

const unsigned ETH_DESC_PER_QUEUE = 1024;

static int validate_ports_and_queues(const struct application *app)
{
    struct rte_eth_dev_info info0, info1;

    if (!rte_eth_dev_is_valid_port(app->cfg.port0)
        || !rte_eth_dev_is_valid_port(app->cfg.port1)) {
        fprintf(stderr, "Invalid port ids: %u, %u\n",
                app->cfg.port0, app->cfg.port1);
        return -1;
    }

    if (rte_eth_dev_info_get(app->cfg.port0, &info0)
        || rte_eth_dev_info_get(app->cfg.port1, &info1)) {
        fprintf(stderr, "Cant get the port info ids %u,%u\n",
                app->cfg.port0, app->cfg.port1);
        return -2;
    }

    if ((app->cfg.queues > info0.max_rx_queues)
        || (app->cfg.queues > info0.max_tx_queues)) {
        fprintf(stderr,
                "Port %u supports max RX %u TX %u queues, requested %u\n",
                app->cfg.port0, info0.max_rx_queues, info0.max_tx_queues,
                app->cfg.queues);
        return -3;
    }

    if ((app->cfg.queues > info1.max_rx_queues)
        || (app->cfg.queues > info1.max_tx_queues)) {
        fprintf(stderr,
                "Port %u supports max RX %u TX %u queues, requested %u\n",
                app->cfg.port1, info1.max_rx_queues, info1.max_tx_queues,
                app->cfg.queues);
        return -4;
    }
    return 0;
}

static unsigned get_enabled_lcores(void)
{
    unsigned cnt = 0, lc = 0;
    RTE_LCORE_FOREACH_WORKER(lc) {
        if (rte_lcore_is_enabled(lc)) {
            ++cnt;
        }
    }
    return cnt;
}

static unsigned collect_worker_lcores(int socket, unsigned *out)
{
    unsigned n = 0, lc = 0;

    /* same socket first */
    RTE_LCORE_FOREACH_WORKER(lc) {
        if (!rte_lcore_is_enabled(lc) || (rte_lcore_to_socket_id(lc) != socket)) {
            continue;
        }
        if (n < RTE_MAX_LCORE) {
            out[n++] = lc;
        }
    }

    /* any remaining socket */
    RTE_LCORE_FOREACH_WORKER(lc) {
        if (!rte_lcore_is_enabled(lc) || (rte_lcore_to_socket_id(lc) == socket)) {
            continue;
        }
        if (n < RTE_MAX_LCORE) {
            out[n++] = lc;
        }
    }
    return n;
}

static int wait_link_up(uint16_t port, unsigned timeout_ms)
{
    const unsigned DELAY_MS = 100;
    const unsigned CHECK_NUM = (timeout_ms + DELAY_MS - 1) / DELAY_MS;

    for (unsigned i=0;i<CHECK_NUM;++i) {
        struct rte_eth_link link;
        memset(&link, 0, sizeof(link));
        int retval = rte_eth_link_get_nowait(port, &link);
        if (retval != 0) {
            return -1;
        }
        if (link.link_status) {
            return 0;
        }

        usleep(DELAY_MS * 1000);
    }
    fprintf(stderr, "Warning: port %u link down\n", port);
    return -1;
}

int simple_pipeline_init(struct application *app)
{
    if (app->cfg.mode != MODE_SIMPLE) {
        fprintf(stderr, "no-simple configuration\n");
        return -1;
    }

    if (app->cfg.queues == 0) {
        fprintf(stderr, "queues must be > 0\n");
        return -1;
    }

    if (validate_ports_and_queues(app) != 0) {
        return -1;
    }

    unsigned enabled_workers = get_enabled_lcores();
    if (enabled_workers < app->cfg.queues) {
        fprintf(stderr,
                "Not enough worker lcores: have %u, need %u (queues)\n",
                enabled_workers, app->cfg.queues);
        return -1;
    }

    int sock0 = rte_eth_dev_socket_id(app->cfg.port0);
    int sock1 = rte_eth_dev_socket_id(app->cfg.port1);
    if (sock0 >= 0 && sock1 >= 0 && sock0 != sock1) {
        fprintf(stderr,
                "Warning: ports on different NUMA nodes (%d vs %d).\n",
                sock0, sock1);
    }

    int socket = (sock0 >= 0) ? sock0 : 0;

    /* Build ordered worker list. Index i => queue i */
    memset(app->state.workers, 0, sizeof(app->state.workers));
    app->state.nb_workers = 0;

    unsigned ordered[RTE_MAX_LCORE];
    unsigned n_ordered = collect_worker_lcores(socket, ordered);
    for (unsigned i=0;i<app->cfg.queues;++i) {
        app->state.workers[i] = ordered[i];
        ++app->state.nb_workers;
    }

    printf("Simple pipeline mode:\n");
    printf("\tports: %u <-> %u\n", app->cfg.port0, app->cfg.port1);
    printf("\tqueues per port: %u\n", app->cfg.queues);
    printf("\tworkers:\n");
    for (unsigned i=0;i<app->state.nb_workers;++i) {
        unsigned lc = app->state.workers[i];
        printf("\t\tqueue %u -> lcore %u (socket %d)\n",
               i, lc, rte_lcore_to_socket_id(lc));
    }
    printf("\tstats lcore: %u\n", rte_get_main_lcore());
    return 0;
}

int multicore_mode_init(struct application *app)
{
    if (app->cfg.mode != MODE_MULTICORE) {
        fprintf(stderr, "non-multicore configuration\n");
        return -1;
    }

    if (app->cfg.queues == 0 || app->cfg.queues > MAX_QUEUES) {
        fprintf(stderr, "queues must be 1..%u\n", MAX_QUEUES);
        return -2;
    }

    if (!rte_eth_dev_is_valid_port(app->cfg.port0)
        || !rte_eth_dev_is_valid_port(app->cfg.port1)) {
        fprintf(stderr, "Invalid port ids %u,%u\n",
                app->cfg.port0, app->cfg.port1);
        return -3;
    }

    struct rte_eth_dev_info info0, info1;
    if (rte_eth_dev_info_get(app->cfg.port0, &info0)
        || rte_eth_dev_info_get(app->cfg.port1, &info1)) {
        fprintf(stderr, "Cant get the port info ids %u,%u\n",
                app->cfg.port0, app->cfg.port1);
        return -4;
    }

    if (app->cfg.queues > info0.max_rx_queues
        || app->cfg.queues > info0.max_tx_queues
        || app->cfg.queues > info1.max_rx_queues
        || app->cfg.queues > info1.max_tx_queues) {
        fprintf(stderr, "Requested queues exceed port capabilities\n");
        return -1;
    }

    const unsigned need = app->cfg.queues * 3; /* RX + worker + TX */
    unsigned have = get_enabled_lcores();
    if (have < need) {
        fprintf(stderr,
                "Not enough worker lcores for multicore mode: have %u, need %u (3*queues)\n",
                have, need);
        return -1;
    }

    int sock0 = rte_eth_dev_socket_id(app->cfg.port0);
    int sock1 = rte_eth_dev_socket_id(app->cfg.port1);
    if (sock0 >= 0 && sock1 >= 0 && sock0 != sock1) {
        fprintf(stderr,
                "Warning: ports on different NUMA nodes (%d vs %d).\n",
                sock0, sock1);
    }

    int socket = (sock0 >= 0) ? sock0 : 0;
    unsigned ordered[RTE_MAX_LCORE];
    unsigned n_ordered = collect_worker_lcores(socket, ordered);
    if (n_ordered < need) {
        return -1;
    }

    memset(app->state.rx_lcores, 0, sizeof(app->state.rx_lcores));
    memset(app->state.workers,   0, sizeof(app->state.workers));
    memset(app->state.tx_lcores, 0, sizeof(app->state.tx_lcores));

    app->state.nb_rx = 0;
    app->state.nb_tx = 0;
    app->state.nb_workers = 0;

    unsigned idx = 0;
    for (uint16_t q=0;q<app->cfg.queues;++q) {
        app->state.rx_lcores[app->state.nb_rx++] = ordered[idx++];
    }
    for (uint16_t q=0;q<app->cfg.queues;++q) {
        app->state.workers[app->state.nb_workers++] = ordered[idx++];
    }
    for (uint16_t q=0;q<app->cfg.queues;++q) {
        app->state.tx_lcores[app->state.nb_tx++] = ordered[idx++];
    }

    printf("Multicore mode mapping:\n");
    printf("\tports: %u <-> %u\n", app->cfg.port0, app->cfg.port1);
    printf("\tqueues per port: %u\n", app->cfg.queues);
    for (uint16_t q=0;q<app->cfg.queues;++q) {
        printf("\t\tq%u: RX lcore %u, Worker lcore %u, TX lcore %u\n",
               q,
               app->state.rx_lcores[q],
               app->state.workers[q],
               app->state.tx_lcores[q]);
    }
    printf("\tstats lcore: %u\n", rte_get_main_lcore());
    return 0;
}

int application_init(struct application *app)
{
    int retval = 0;

    if (!rte_eth_dev_is_valid_port(app->cfg.port0)
        || !rte_eth_dev_is_valid_port(app->cfg.port1)) {
        fprintf(stderr, "Invalid ports %u,%u\n", app->cfg.port0, app->cfg.port1);
        return -1;
    }

    uint16_t nb_ports = rte_eth_dev_count_avail();
    if (nb_ports < 2) {
        fprintf(stderr, "Need at least 2 DPDK ports, have %u\n", nb_ports);
        return -1;
    }

    int sock0 = rte_eth_dev_socket_id(app->cfg.port0);
    unsigned socket0 = (sock0 >= 0) ? (unsigned)sock0 : 0;
    const unsigned nb_mbuf = app->cfg.queues * MAX_BURST * 1024;

    app->state.pool = rte_pktmbuf_pool_create("mbuf_pool",
                                              nb_mbuf,
                                              256,
                                              0,
                                              RTE_MBUF_DEFAULT_BUF_SIZE,
                                              socket0);
    if (!app->state.pool) {
        fprintf(stderr, "mempool create failed\n");
        retval = -1;
        goto fail;
    }

    struct rte_eth_conf eth;
    memset(&eth, 0, sizeof(eth));
    
    /* Enable RSS for multiple queues to distribute traffic */
    if (app->cfg.queues > 1) {
        eth.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
        eth.rx_adv_conf.rss_conf.rss_key = NULL;  /* Use default RSS key */
        eth.rx_adv_conf.rss_conf.rss_hf = RTE_ETH_RSS_IP | RTE_ETH_RSS_TCP | RTE_ETH_RSS_UDP;
    } else {
        eth.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
    }
    
    eth.txmode.mq_mode = RTE_ETH_MQ_TX_NONE;

    uint16_t ports[2] = { app->cfg.port0, app->cfg.port1 };
    for (int i=0;i<2;++i) {
        uint16_t port = ports[i];

        retval = rte_eth_dev_configure(port, app->cfg.queues, app->cfg.queues, &eth);
        if (retval < 0) {
            fprintf(stderr, "rte_eth_dev_configure(port %u) failed\n", port);
            retval = -1;
            goto fail;
        }

        for (uint16_t q=0;q<app->cfg.queues;++q) {
            retval = rte_eth_rx_queue_setup(port, q,
                                            ETH_DESC_PER_QUEUE,
                                            rte_eth_dev_socket_id(port),
                                            NULL,
                                            app->state.pool);
            if (retval < 0) {
                fprintf(stderr, "rx_queue_setup(port %u q %u) failed\n", port, q);
                retval = -1;
                goto fail;
            }

            retval = rte_eth_tx_queue_setup(port, q,
                                            ETH_DESC_PER_QUEUE,
                                            rte_eth_dev_socket_id(port),
                                            NULL);
            if (retval < 0) {
                fprintf(stderr, "tx_queue_setup(port %u q %u) failed\n", port, q);
                retval = -1;
                goto fail;
            }
        }

        retval = rte_eth_dev_start(port);
        if (retval < 0) {
            fprintf(stderr, "rte_eth_dev_start(port %u) failed\n", port);
            retval = -1;
            goto fail;
        }

        if (app->cfg.promisc) {
            rte_eth_promiscuous_enable(port);
        }

        wait_link_up(port, 3 * 1000);
    }
    return 0;

fail:
    application_cleanup(app);
    return retval;
}

void application_cleanup(struct application *app)
{
    /* Multicore mode rings */
    multicore_ctx_cleanup(&app->state.multicore);

    uint16_t ports[2] = { app->cfg.port0, app->cfg.port1 };
    for (int i=0;i<2;++i) {
        uint16_t port = ports[i];
        if (!rte_eth_dev_is_valid_port(port)) {
            continue;
        }

        rte_eth_dev_stop(port);
        rte_eth_dev_close(port);
    }

    if (app->state.pool) {
        rte_mempool_free(app->state.pool);
        app->state.pool = NULL;
    }
}

