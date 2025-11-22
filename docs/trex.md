How to use TRex with DPDK-forwarder

1. Prerequisites:

    sudo apt update
    sudo apt install -y \
        python3 \
        python3-yaml \
        pciutils \
        ethtool \
        libnuma1 \
        libpcap0.8 \
        wget

2. Download binaries:

    cd /opt
    sudo wget https://trex-tgn.cisco.com/trex/release/latest -O trex.tar.gz
    sudo tar -xzf trex.tar.gz
    sudo mv trex-* trex
    sudo chown -R $USER:$USER trex

3. Check:

    cat /proc/cpuinfo | grep -E 'sse4_2|avx'

4. Bind ports:

    cd /opt/trex
    sudo ./dpdk_setup_ports.py -s

    sudo ./dpdk_setup_ports.py -t
    sudo ./dpdk_setup_ports.py -c

5. Run:

    cd /opt/trex
    sudo ./t-rex-64 -i \
        --cfg none \
        -a 0000:04:00.0 \
        -a 0000:05:00.0 \
        -l 1,2,3

