# Reproducible toolchain for building Adafruit nRF52 / OTAFIX bootloaders.
# Host only needs Docker — avoids broken Homebrew arm-none-eabi-gcc (missing newlib).
#
# Build image (once):
#   docker build -t vk-otafix-build .
#
# Build a board (from repo root, submodules already initialized):
#   docker run --rm -v "$PWD":/src -w /src vk-otafix-build make BOARD=wismesh_tag all
#
# UF2 lands on the host at:
#   _build/build-<board>/update-<board>_bootloader-*_nosd.uf2

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      gcc-arm-none-eabi \
      binutils-arm-none-eabi \
      libnewlib-arm-none-eabi \
      python3 \
      python3-pip \
      python3-setuptools \
      git \
      ca-certificates \
    && pip3 install --break-system-packages --no-cache-dir adafruit-nrfutil intelhex \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
# Default: show help. Pass make args via docker run.
CMD ["make", "BOARD=wismesh_tag", "all"]
