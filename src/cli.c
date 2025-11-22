#include "app.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <getopt.h>

static void set_defaults(struct application *app)
{
    memset(app, 0, sizeof(*app));
    struct app_config *cfg = &app->cfg;
    cfg->mode = MODE_SIMPLE;
    cfg->port0 = 0;
    cfg->port1 = 1;
    cfg->queues = 1;
    cfg->stat_sec = 1;
    cfg->promisc = 0;
    cfg->drop_non_ip = 1;
    cfg->mac_mode = MAC_REWRITE_NONE;
    cfg->rate_limit = 0;
    cfg->rate_limit_pps = 0;
}

static int parse_u16(const char *s, uint16_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (errno || end == s || *end != '\0' || v > 0xFFFF) {
        return -1;
    }
    *out = (uint16_t)v;
    return 0;
}

static int parse_u32(const char *s, uint32_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (errno || end == s || *end != '\0' || v > 0xFFFFFFFFUL) {
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

static int parse_u64(const char *s, uint64_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno || end == s || *end != '\0') {
        return -1;
    }
    *out = (uint64_t)v;
    return 0;
}

static int parse_mode(const char *s, enum APP_MODE *m)
{
    if (!strcmp(s, "simple") || !strcmp(s, "0")) {
        *m = MODE_SIMPLE;
        return 0;
    }
    if (!strcmp(s, "multicore") || !strcmp(s, "1")) {
        *m = MODE_MULTICORE;
        return 0;
    }
    return -1;
}

static int parse_mac(const char *s, uint8_t mac[6])
{
    unsigned v[6] = {0};
    if (sscanf(s, "%x:%x:%x:%x:%x:%x",
               &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
        return -1;
    }
    for (int i=0;i<6;++i) {
        if (v[i] > 0xFF) {
            return -1;
        }
        mac[i] = (uint8_t)v[i];
    }
    return 0;
}

int application_parse_args(int argc, char **argv, struct application *app)
{
    set_defaults(app);

    struct app_config *cfg = &app->cfg;
    const struct option opt_list[] = {
        { "mode", required_argument, 0, 'm' },
        { "ports", required_argument, 0, 'p' },
        { "queues", required_argument, 0, 'q' },
        { "stat-sec", required_argument, 0, 's' },
        { "promisc", no_argument, 0, 'P' },
        { "drop-non-ip", no_argument, 0, 'd' },
        { "rewrite-mac", optional_argument, 0, 'r' },
        { "rate-limit-pps", required_argument, 0, 'R' },
        { 0, 0, 0, 0 }
    };

    int opt = 0, idx = 0;
    while ((opt = getopt_long(argc, argv, "m:p:q:s:Pdr::R:",
                              opt_list, &idx)) != -1) {
        switch (opt) {
        case 'm': {
            if (parse_mode(optarg, &cfg->mode) != 0) {
                fprintf(stderr, "bad --mode\n");
                return -1;
            }
            break;
        }
        case 'p': {
            uint16_t a, b;
            if (sscanf(optarg, "%hu,%hu", &a, &b) != 2) {
                fprintf(stderr, "bad --ports (use A,B)\n");
                return -1;
            }
            cfg->port0 = a;
            cfg->port1 = b;
            break;
        }
        case 'q': {
            if (parse_u16(optarg, &cfg->queues) != 0 || cfg->queues == 0) {
                fprintf(stderr, "bad --queues\n");
                return -1;
            }
            break;
        }
        case 's': {
            if (parse_u32(optarg, &cfg->stat_sec) != 0 ||
                cfg->stat_sec == 0) {
                fprintf(stderr, "bad --stat-sec\n");
                return -1;
            }
            break;
        }
        case 'P': {
            cfg->promisc = 1;
            break;
        }
        case 'd': {
            cfg->drop_non_ip = 1;
            break;
        }
        case 'r': {
            if (optarg) {
                if (parse_mac(optarg, cfg->dmac) != 0) {
                    fprintf(stderr, "bad --rewrite-mac=aa:bb:cc:dd:ee:ff\n");
                    return -1;
                }
                cfg->mac_mode = MAC_REWRITE_SRC_DST;

                printf("Rewrite dmac: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       cfg->dmac[0], cfg->dmac[1], cfg->dmac[2],
                       cfg->dmac[3], cfg->dmac[4], cfg->dmac[5]);
            } else {
                cfg->mac_mode = MAC_REWRITE_SRC;
                printf("Rewrite smac only\n");
            }
            break;
        }
        case 'R': {
            cfg->rate_limit = 1;
            if (parse_u64(optarg, &cfg->rate_limit_pps) != 0
                || cfg->rate_limit_pps == 0) {
                fprintf(stderr, "bad --rate-limit-pps\n");
                return -1;
            }

            printf("Rate limit to %" PRIu64 " PPS\n", cfg->rate_limit_pps);
            break;
        }
        default: {
            fprintf(stderr, "unknown option\n");
            return -1;
        }
        }
    }
    return 0;
}

