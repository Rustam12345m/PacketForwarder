#ifndef FORWARDER_H
#define FORWARDER_H

#include "app.h"
#include "network.h"
#include "limiter.h"

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_branch_prediction.h>
#include <rte_prefetch.h>

struct forwarder_ctx
{
    uint8_t drop_non_ip;
    uint16_t port0;

    enum MAC_SUBST_MODE mac_mode;
    uint8_t dmac[6];
    uint8_t smac0[6];
    uint8_t smac1[6];

    uint8_t rate_limit;
    uint64_t rate_limit_pps;
    struct limiter lim;

    struct statistics *stats;
};

static inline const uint8_t* select_smac(const struct forwarder_ctx *ctx, uint16_t out_port)
{
    return (out_port == ctx->port0) ? ctx->smac0 : ctx->smac1;
}

static inline uint16_t forwarder(struct rte_mbuf **bufs, uint16_t num,
                                 struct forwarder_ctx *ctx, uint16_t out_port)
{
    const uint8_t do_filter = ctx->drop_non_ip;
    const uint8_t do_rate = ctx->rate_limit;

    const enum MAC_SUBST_MODE mac_mode = ctx->mac_mode;
    const uint8_t do_mac = (mac_mode != MAC_REWRITE_NONE);
    const uint8_t *smac = do_mac ? select_smac(ctx, out_port) : NULL;

    struct statistics *stats = ctx->stats;

    uint16_t keep = 0;
    for (uint16_t i=0;i<num;++i) {
        struct rte_mbuf *buf = bufs[i];

        if (i + 1 < num) {
            rte_prefetch0(rte_pktmbuf_mtod(bufs[i + 1], void *));
        }

        if (do_filter) {
            uint16_t l2_len = 0;
            uint16_t l2_type = get_l2_type_and_len(buf, &l2_len);
            (void)l2_len;

            if (!is_ip_packet(l2_type)) {
                ++stats->drop_non_ip;
                rte_pktmbuf_free(buf);
                continue;
            }
        }

        if (do_mac) {
            if (mac_mode == MAC_REWRITE_SRC_DST) {
                rewrite_macs(buf, smac, ctx->dmac);
            } else {
                set_src_mac(buf, smac);
            }
        }

        bufs[keep++] = buf;
    }
    if (keep == 0) {
        return 0;
    }

    if (do_rate) {
        if (ctx->lim.rate_pps == 0) {
            limiter_init(&ctx->lim, ctx->rate_limit_pps);
        }

        uint16_t allow = limiter_allow(&ctx->lim, rte_rdtsc(), keep);
        if (allow < keep) {
            stats->drop_rate_limit += (uint64_t)(keep - allow);

            for (uint16_t i=allow;i<keep;++i) {
                rte_pktmbuf_free(bufs[i]);
            }
            keep = allow;
        }
    }
    return keep;
}

#endif

