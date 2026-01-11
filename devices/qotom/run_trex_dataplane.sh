#!/bin/bash
set -e

cd /opt/trex/
./t-rex-64 \
    -i --stl \
    --mbuf-factor 0.5 \
    --cfg /opt/forwarder/trex_config.yaml \
    --no-scapy-server \
    --prefix trex

