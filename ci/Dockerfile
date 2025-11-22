# Build: podman build -t dpdk_debian13 -f Dockerfile
# Run: podman run -it --rm -v $(pwd):/repos/ --userns=keep-id dpdk_debian13 /bin/bash

FROM debian:13

RUN apt-get update && \
    apt-get -y --no-install-recommends install \
        mc vim sudo bash wget \
        build-essential \
        coreutils git gcc make cppcheck \
        meson ninja-build cmake \
        zip \
        pkg-config pciutils elfutils \
        python3 python3-pip python3-pyelftools \
        libdpdk-dev libcmocka-dev \
        linux-libc-dev libbsd-dev \
        libatomic1 libpcap-dev libnuma-dev \
        libxdp-dev libbpf-dev libelf-dev \
        openssh-client sshpass \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Create a new user 'builder' and add to the sudo group
RUN useradd -m -s /bin/bash builder \
    && echo "builder:builder" | chpasswd \
    && usermod -aG sudo builder \
    && echo "builder ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/builder

# Set working directory
WORKDIR /home/builder
USER builder

CMD ["/bin/bash"]

