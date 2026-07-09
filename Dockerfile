FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY mycelium-cpp/ .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN adduser --disabled-password --gecos "" mycelium
USER mycelium

COPY --from=builder /src/build/mycelium /usr/local/bin/mycelium

EXPOSE 18028
EXPOSE 8080

ENTRYPOINT ["mycelium"]
CMD ["--help"]
