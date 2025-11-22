#!/bin/bash
set -e

PORT0=${1:-"enp0s3"}
PORT1=${2:-"enp0s4"}

echo 1024 > /proc/sys/vm/nr_hugepages

mkdir -p /mnt/forwarder
mount -t hugetlbfs nodev /mnt/forwarder

modprobe uio
modprobe uio_pci_generic

# Find PCI addresses for eth0 and eth1
PORT0_PCI=$(ethtool -i ${PORT0} | grep 'bus-info' | awk '{print $2}')
PORT1_PCI=$(ethtool -i ${PORT1} | grep 'bus-info' | awk '{print $2}')

echo "PORT0 = ${PORT0} -> ${PORT0_PCI}"
echo "PORT1 = ${PORT1} -> ${PORT1_PCI}"

dpdk-devbind.py --bind=uio_pci_generic $PORT0_PCI $PORT1_PCI
dpdk-devbind.py --status

