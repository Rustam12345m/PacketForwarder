#!/bin/bash
set -x

LCORES="6,7,8,9"
NIC_PCI_ADDR1="pci:0000:04:00.0"
NIC_PCI_ADDR2="pci:0000:05:00.0"

./dpdk_forwarder -l $LCORES \
    -a $NIC_PCI_ADDR1 \
    -a $NIC_PCI_ADDR2 \
    --huge-dir=/mnt/forwarder/ \
    --file-prefix=forwarder \
    -- $@
