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

    Fix cgi:

    cd /opt/trex/
    cat > cgi.py <<'PY'
from html import escape as _escape

def escape(s, quote=False):
    return _escape(s, quote=quote)

def parse_header(line):
    parts = [p.strip() for p in line.split(';') if p.strip()]
    key = parts[0].lower() if parts else ''
    params = {}
    for p in parts[1:]:
        if '=' in p:
            k, v = p.split('=', 1)
            params[k.strip().lower()] = v.strip().strip('"')
        else:
            params[p.lower()] = ''
    return key, params
PY

4. Bind ports:

    cd /opt/trex
    sudo ./dpdk_setup_ports.py -s

    sudo ./dpdk_setup_ports.py -t
    sudo ./dpdk_setup_ports.py -c

5. Run:

    # Dataplane:
    cd /opt/trex
    ./t-rex-64 -i --cfg ../trex_cfg.yaml --no-scapy-server

    # Console
    reset
    start -f stl/udp_1pkt_simple.py -p 0 -m 1mpps

    # Statistics
    stats -a -p
    tui
