#!/bin/bash
set -euo pipefail

LCORES="0,1,2,3"
NIC_PCI_ADDR1="pci:0000:00:03.0"
NIC_PCI_ADDR2="pci:0000:00:04.0"

./dpdk_forwarder -l $LCORES \
    -a $NIC_PCI_ADDR1 \
    -a $NIC_PCI_ADDR2 \
    --huge-dir=/mnt/forwarder/ \
    --file-prefix=forwarder \
    -- \
    --mode=simple \
    --ports=0,1 \
    --queues=1 \
    --rewrite-mac=10:11:12:13:14:15 \
    --rate-limit-pps=10 \
    --stat-sec=1 \
    --promisc \
    -- $@

