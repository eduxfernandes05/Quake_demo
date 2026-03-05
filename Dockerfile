FROM ubuntu:24.04 AS builder
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake pkg-config \
        libgl-dev libegl-dev libgbm-dev && \
    rm -rf /var/lib/apt/lists/*
COPY . /src
WORKDIR /src
RUN cmake -B build -DHEADLESS=ON -DNOASM=ON -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel "$(nproc)"

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \
        libgl1-mesa-dri libegl1 libgbm1 curl && \
    rm -rf /var/lib/apt/lists/*
RUN useradd -r -s /usr/sbin/nologin quake
COPY --from=builder /src/build/WinQuake/quake-dedicated /usr/local/bin/quake-worker
USER quake
EXPOSE 8080
HEALTHCHECK --interval=10s --timeout=3s --retries=3 \
    CMD curl -sf http://localhost:8080/healthz || exit 1
ENTRYPOINT ["quake-worker"]
