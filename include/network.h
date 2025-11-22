#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <string.h>

#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_byteorder.h>

/* ========== Ethernet Layer ========== */
#define ETH_TYPE_VLAN   0x8100
#define ETH_TYPE_QINQ   0x88A8
#define ETH_TYPE_IPV4   0x0800
#define ETH_TYPE_IPV6   0x86DD

static inline struct rte_ether_hdr* eth_hdr(const struct rte_mbuf *m)
{
    return rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
}

/* Returns EtherType in host order and sets l2_len (incl VLAN) */
static inline uint16_t get_l2_type_and_len(const struct rte_mbuf *m, uint16_t *l2_len)
{
    const uint8_t *p = rte_pktmbuf_mtod(m, const uint8_t *);
    uint16_t offset = sizeof(struct rte_ether_hdr);
    uint16_t type = rte_be_to_cpu_16(*(const uint16_t *)(p + 12));

    if ((type == ETH_TYPE_VLAN) || (type == ETH_TYPE_QINQ)) {
        if (rte_pktmbuf_pkt_len(m) >= offset + 4) {
            type = rte_be_to_cpu_16(*(const uint16_t *)(p + offset + 2));
            offset += 4;

            if (((type == ETH_TYPE_VLAN) || (type == ETH_TYPE_QINQ))
                && (rte_pktmbuf_pkt_len(m) >= offset + 4)) {
                type = rte_be_to_cpu_16(*(const uint16_t *)(p + offset + 2));
                offset += 4;
            }
        }
    }

    *l2_len = offset;
    return type;
}

static inline int is_ip_packet(uint16_t et)
{
    return (et == ETH_TYPE_IPV4) || (et == ETH_TYPE_IPV6);
}

static inline void set_src_mac(struct rte_mbuf *m, const uint8_t mac[6])
{
    struct rte_ether_hdr *h = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    rte_ether_addr_copy((const struct rte_ether_addr *)mac, &h->src_addr);
}

static inline void set_dst_mac(struct rte_mbuf *m, const uint8_t mac[6])
{
    struct rte_ether_hdr *h = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    rte_ether_addr_copy((const struct rte_ether_addr *)mac, &h->dst_addr);
}

static inline void rewrite_macs(struct rte_mbuf *m, const uint8_t smac[6], const uint8_t dmac[6])
{
    struct rte_ether_hdr *h = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    rte_ether_addr_copy((const struct rte_ether_addr *)smac, &h->src_addr);
    rte_ether_addr_copy((const struct rte_ether_addr *)dmac, &h->dst_addr);
}

/* ========== IP Layer ========== */
static inline struct rte_ipv4_hdr* ipv4_hdr(const struct rte_mbuf *m, uint16_t l2_len)
{
    return rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, l2_len);
}

static inline struct rte_ipv6_hdr* ipv6_hdr(const struct rte_mbuf *m, uint16_t l2_len)
{
    return rte_pktmbuf_mtod_offset(m, struct rte_ipv6_hdr *, l2_len);
}

static inline int parse_ipv4_min(const struct rte_mbuf *m, uint16_t l2_len,
                                 uint32_t *src, uint32_t *dst, uint8_t *proto)
{
    if (rte_pktmbuf_pkt_len(m) < l2_len + sizeof(struct rte_ipv4_hdr)) {
        return -1;
    }

    struct rte_ipv4_hdr *h = ipv4_hdr(m, l2_len);
    if ((h->version_ihl >> 4) != 4) {
        return -1;
    }

    uint8_t ihl = (h->version_ihl & 0x0F) * 4;
    if ((ihl < sizeof(struct rte_ipv4_hdr))
        || (rte_pktmbuf_pkt_len(m) < l2_len + ihl)) {
        return -1;
    }

    *src = rte_be_to_cpu_32(h->src_addr);
    *dst = rte_be_to_cpu_32(h->dst_addr);
    *proto = h->next_proto_id;
    return 0;
}

static inline int parse_ipv6_min(const struct rte_mbuf *m, uint16_t l2_len,
                                 const uint8_t **src, const uint8_t **dst, uint8_t *proto)
{
    if (rte_pktmbuf_pkt_len(m) < l2_len + sizeof(struct rte_ipv6_hdr)) {
        return -1;
    }

    struct rte_ipv6_hdr *h = ipv6_hdr(m, l2_len);
    if (((rte_be_to_cpu_32(h->vtc_flow) >> 28) & 0xF) != 6) {
        return -1;
    }

    /*
    *src = h->src_addr;
    *dst = h->dst_addr;
    *proto = h->proto;
    */
    return 0;
}

#endif

