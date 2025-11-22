
1. QEMU VM:

    sudo apt install vim mc rsync sudo

    sudo nano /etc/network/interfaces
    auto eth0
    iface eth0 inet static
        address 192.168.21.2
        netmask 255.255.255.0
        gateway 192.168.21.1
        dns-nameservers 8.8.8.8 8.8.4.4
    sudo systemctl restart networking

    sudo apt install openssh-server
    sudo vim /etc/ssh/sshd_config
    PermitRootLogin yes
    sudo systemctl restart ssh

    sudo vim /etc/sudoers
    user ALL=(ALL:ALL) ALL

    sudo sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT="/GRUB_CMDLINE_LINUX_DEFAULT="intel_iommu=on iommu=pt /' /etc/default/grub
    sudo update-grub
    sudo reboot

2. DPDK:

    sudo apt install dpdk

3. Host:

    sudo apt install -y python3-scapy

