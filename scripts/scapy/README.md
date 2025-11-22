### Scapy Basics

1. **Installation**

   To install Scapy using pip, run the following command in your terminal:

   ```bash
   sudo pip install scapy
   ```

2. **Starting Scapy**

   Start Scapy in interactive mode by opening a Python shell with root privileges:

   ```bash
   sudo scapy
   ```

   Alternatively, you can use Scapy in your scripts by importing it:

   ```python
   from scapy.all import *
   ```

3. **Creating Packets**

   Basic packet creation using Scapy involves specifying layers and fields:

   - **Ethernet Frame**: `Ether()`
   - **IP Packet**: `IP(dst="1.2.3.4")`
   - **TCP Packet**: `TCP(dport=80, flags="S")`

4. **Combining Layers**

   Combine packet layers using `/`:

   ```python
   packet = Ether() / IP(dst="192.168.1.1") / TCP(dport=80)
   ```

5. **Sending Packets**

   Use `send()`, `sendp()`, or `sr()` functions:

   - **Sending at layer 3**: 
     ```python
     send(IP(dst="192.168.1.1")/ICMP())
     ```

   - **Sending at layer 2 (Ethernet layer)**:
     ```python
     sendp(Ether()/IP(dst="192.168.1.1")/ICMP())
     ```

   - **Sending and receiving**:
     ```python
     ans, unans = sr(IP(dst="192.168.1.1")/TCP(dport=80, flags="S"))
     ```

6. **Sniffing Packets**

   Capture live packets using:

   ```python
   packets = sniff(count=10)
   ```

   - **Filter with BPF syntax**:
     ```python
     packets = sniff(filter="tcp and port 80", count=10)
     ```

7. **Packet Analysis**

   - **Show packet summary**:
     ```python
     packet.summary()
     ```

   - **Detailed view**:
     ```python
     packet.show()
     ```

   - **Hex dump**:
     ```python
     hexdump(packet)
     ```

8. **Modifying Packets**

   Modify fields directly on a packet, e.g., change the destination IP:

   ```python
   packet[IP].dst = "10.0.0.1"
   ```

9. **Saving and Reading Packets**

   - **Save packets to a file**:
     ```python
     wrpcap('packets.pcap', packets)
     ```

   - **Read packets from a file**:
     ```python
     packets = rdpcap('packets.pcap')
     ```

10. **Advanced Features**

    - **Traceroute**:
      ```python
      traceroute(["www.google.com", "www.yahoo.com"])
      ```

    - **ARP Spoofing**:
      ```python
      send(ARP(op=2, pdst="192.168.1.10", psrc="192.168.1.1"), loop=1, inter=0.1)
      ```

