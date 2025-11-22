# DPDK Packet Forwarder

A simple DPDK-based packet forwarder for forwarding IP packets from one port to another.

## Requirements

- **DPDK**: Data Plane Development Kit (tested with Debian 13 packages)
- **CMake**: Version 3.12 or higher
- **CMocka**: For unit testing
- **Docker/Podman**: For containerized building

## Building

```bash
# Local build
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Container build + packaging

```bash
# Build+package (defaults to: --mode release --device qemu)
./ci/build.sh

# Explicit build matrix entry
./ci/build.sh --mode debug --device qemu
./ci/build.sh --mode release --device qotom

# Open an interactive shell in the container
./ci/build.sh --shell
# (inside container) ./ci/build_inside_container.sh --mode debug --device qemu
```

Build artifacts are written to `artifacts/`:

- `debug_qemu.zip`, `release_qemu.zip`
- `debug_qotom.zip`, `release_qotom.zip`

Each zip contains the `dpdk_forwarder` binary + `devices/<device>/*.sh`.

CI:

- **Every commit/PR**: workflow uploads the zips as **GitHub Actions artifacts**.
- **Merges to `main`**: the same zips are also published as **GitHub Release assets**.

## Running

### Pipeline Modes

**Simple mode** (default):
- Single-threaded packet processing per queue
- Each worker thread handles both RX and TX for a queue
- Steps: RX burst → process packets → TX burst
- Lower latency, simpler pipeline

**Multicore mode**:
- Multi-threaded pipeline with dedicated RX/Worker/TX threads
- Threads communicate via lock-free rings
- Steps: RX threads → rings → Worker threads (process) → rings → TX threads
- Better CPU utilization for high-throughput scenarios

### Basic Usage

```bash
# Run the forwarder with DPDK EAL parameters
sudo ./build/src/dpdk_forwarder -l 0-1 -n 4 -- [app args]
```

App args (from `src/cli.c`, defaults in parentheses):

- **`--mode simple|multicore`**: pipeline mode (**simple**)
- **`--ports A,B`**: DPDK port ids (**0,1**)
- **`--queues N`**: queues per port (**1**)
- **`--stat-sec N`**: stats print period in seconds (**1**)
- **`--promisc`**: enable promiscuous mode (**off**)
- **`--drop-non-ip`**: drop non-IP packets (**on**)
- **`--rewrite-mac[=aa:bb:cc:dd:ee:ff]`**: rewrite SMAC only (no arg), or SMAC+DMAC (with arg) (**off**)
- **`--rate-limit-pps N`**: rate limit in PPS (**off**)

## License

GPL-3

