#!/bin/bash
set -e
#./setup_env.sh enp4s0 enp5s0

PORT0=${1:-"eth0"}
PORT1=${2:-"eth1"}

echo 1024 > /proc/sys/vm/nr_hugepages

mkdir -p /mnt/forwarder
mount -t hugetlbfs nodev /mnt/forwarder

modprobe vfio
modprobe vfio-pci

# Find PCI addresses for eth0 and eth1
PORT0_PCI=$(ethtool -i ${PORT0} | grep 'bus-info' | awk '{print $2}')
PORT1_PCI=$(ethtool -i ${PORT1} | grep 'bus-info' | awk '{print $2}')

echo "PORT0 = ${PORT0} -> ${PORT0_PCI}"
echo "PORT1 = ${PORT1} -> ${PORT1_PCI}"

dpdk-devbind.py --bind=vfio-pci $PORT0_PCI $PORT1_PCI
dpdk-devbind.py --status

