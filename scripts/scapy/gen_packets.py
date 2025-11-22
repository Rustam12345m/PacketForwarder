from scapy.all import *
import random

def send_l2_packets(iface, count=10, interval=1, pkt_size=60, src_mac=None, dst_mac=None):
    """Send raw Ethernet frames with a specified size."""
    if not src_mac:
        src_mac = str(RandMAC())
    if not dst_mac:
        dst_mac = str(RandMAC())

    payload_size = pkt_size - 14  # Subtract Ethernet header size
    payload = bytes(random.getrandbits(8) for _ in range(payload_size))

    pkt = Ether(src=src_mac, dst=dst_mac) / payload
    sendp(pkt, iface=iface, count=count, inter=interval)

def send_udp_packets(iface, count=10, interval=1, pkt_size=60, src_mac=None, dst_mac=None, src_ip=None, dst_ip=None):
    """Send UDP packets over IP with a specified size."""
    if not src_mac:
        src_mac = str(RandMAC())
    if not dst_mac:
        dst_mac = str(RandMAC())

    if not src_ip:
        src_ip = str(RandIP())
    if not dst_ip:
        dst_ip = str(RandIP())

    headers_size = 14 + 20 + 8  # Ethernet + IP + UDP header sizes
    payload_size = pkt_size - headers_size
    payload = bytes(random.getrandbits(8) for _ in range(payload_size))

    pkt = Ether(src=src_mac, dst=dst_mac) / IP(src=src_ip, dst=dst_ip) / UDP(sport=1234, dport=5678) / payload
    sendp(pkt, iface=iface, count=count, inter=interval)

if __name__ == "__main__":
    iface = "vm_tap1"

    # Send L2 packets with specified size
    send_l2_packets(iface=iface, count=10, interval=1, pkt_size=100)

    # Send UDP packets with specified size
    send_udp_packets(iface=iface, count=10, interval=1, pkt_size=100)

