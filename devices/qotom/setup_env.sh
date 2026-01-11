#!/bin/bash
set -e

echo 2048 > /proc/sys/vm/nr_hugepages

mkdir -p /mnt/forwarder
mount -t hugetlbfs nodev /mnt/forwarder

mkdir -p /mnt/trex
mount -t hugetlbfs nodev /mnt/trex

modprobe vfio
modprobe vfio-pci

# Find PCI addresses for Ethernet port and bind to DPDK
interfaces=("eno1" "eno2" "eno3" "eno4" "enp4s0" "enp5s0" "enp6s0" "enp7s0")
pci_addresses=()
for interface in "${interfaces[@]}"; do
    pci_address=$(ethtool -i ${interface} | grep 'bus-info' | awk '{print $2}')

    if [ -n "$pci_address" ]; then
        echo "${interface} -> ${pci_address}"
        pci_addresses+=("$pci_address")
    else
        echo "Failed to find PCI address for ${interface}"
    fi
done

# Bind all found PCI addresses to DPDK
if [ ${#pci_addresses[@]} -gt 0 ]; then
    dpdk-devbind.py --bind=vfio-pci "${pci_addresses[@]}"
else
    echo "No PCI addresses found to bind."
fi

