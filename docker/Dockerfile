# so2-decomp dev image. Build: docker build -t so2-decomp docker/   (no project files copied in)
# Run:   .\docker\run.ps1 <cmd>   (project mounted at /work; tools/maspsx used from the mount)
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      binutils-mipsel-linux-gnu build-essential make ninja-build git curl ca-certificates \
      python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*
RUN pip3 install --no-cache-dir splat64

# Prebuilt static old-gcc PSX compilers (decompals/old-gcc release 0.17), mipsel target.
ARG OLDGCC=https://github.com/decompals/old-gcc/releases/download/0.17
RUN for v in 2.6.0-psx 2.7.2-psx 2.7.2-cdk 2.8.0-psx 2.8.1-psx; do \
      mkdir -p /opt/gcc-$v && curl -fsSL $OLDGCC/gcc-$v.tar.gz | tar xz -C /opt/gcc-$v; \
    done
# ASPSX not needed: maspsx (mounted at /work/tools/maspsx) feeds GNU as instead.
WORKDIR /work
