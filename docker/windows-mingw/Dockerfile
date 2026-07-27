# syntax=docker/dockerfile:1.7

FROM ubuntu:24.04 AS builder

ARG BUILD_JOBS=4

ENV DEBIAN_FRONTEND=noninteractive \
    VCPKG_ROOT=/opt/vcpkg \
    VCPKG_INSTALLED_DIR=/deps/vcpkg_installed \
    BUILD_JOBS=${BUILD_JOBS}

WORKDIR /deps

RUN --mount=type=cache,target=/var/cache/apt \
    --mount=type=cache,target=/var/lib/apt \
    apt-get update && apt-get install -y --no-install-recommends \
        autoconf \
        autoconf-archive \
        automake \
        bzip2 \
        libbz2-dev \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        binutils-mingw-w64-x86-64 \
        file \
        g++-mingw-w64-x86-64 \
        gcc-mingw-w64-x86-64 \
        git \
        libtool \
        libltdl-dev \
        mingw-w64-tools \
        nasm \
        ninja-build \
        pkg-config \
        patch \
        python3 \
        python3-venv \
        unzip \
        xz-utils \
        yasm \
        zip \
    && rm -rf /var/lib/apt/lists/*

COPY vcpkg.json /deps/vcpkg.json

RUN --mount=type=cache,target=/root/.cache/vcpkg \
    baseline="$(python3 -c 'import json, pathlib; print(json.loads(pathlib.Path("/deps/vcpkg.json").read_text())["builtin-baseline"])')" \
    && git init /opt/vcpkg \
    && cd /opt/vcpkg \
    && git remote add origin https://github.com/microsoft/vcpkg.git \
    && git fetch --depth 1 origin "$baseline" \
    && git checkout --detach FETCH_HEAD \
    && git fetch --unshallow --tags origin \
    && ./bootstrap-vcpkg.sh -disableMetrics

RUN --mount=type=cache,target=/root/.cache/vcpkg \
    VCPKG_ROOT=/opt/vcpkg \
    /opt/vcpkg/vcpkg install --triplet x64-mingw-dynamic --host-triplet x64-linux

COPY . /src
WORKDIR /src

RUN cmake --preset windows-mingw-x64
RUN cmake --build --preset windows-mingw-x64 --parallel "${BUILD_JOBS}"

RUN set -eux; \
    exe_path="$(find build/windows-mingw-x64 -type f -name 'opendis2.exe' -print -quit)"; \
    exe_count="$(find build/windows-mingw-x64 -type f -name 'opendis2.exe' | wc -l | tr -d ' ')"; \
    [ -n "$exe_path" ]; \
    [ "$exe_count" = 1 ]; \
    mkdir -p /out; \
    cp "$exe_path" /out/opendis2.exe; \
    file_output="$(file /out/opendis2.exe)"; \
    objdump_output="$(x86_64-w64-mingw32-objdump -f /out/opendis2.exe)"; \
    printf '%s\n%s\n' "$file_output" "$objdump_output" > /out/pe-info.txt; \
    printf '%s\n' "$file_output" | grep -Eq 'PE32\+ executable.*x86-64|x86-64.*PE32\+'; \
    printf '%s\n' "$objdump_output" | grep -Eq 'pei-x86-64'; \
    ! printf '%s\n' "$file_output" | grep -Eq 'ELF|Mach-O|PE32 executable|ARM|Aarch64|unrecognized'; \
    test -s /out/opendis2.exe; \
    test -s /out/pe-info.txt

FROM scratch AS artifact
COPY --from=builder /out/ /
