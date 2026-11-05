# Render builds this image from the repository root.
FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates git cmake g++ make libasio-dev libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt server.cpp ./
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_FULLY_DISCONNECTED=OFF \
    && cmake --build build --config Release --parallel 2

FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libsqlite3-0 \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /src/build/visa_vista_api /app/visa_vista_api
RUN mkdir -p /var/data/uploads && useradd --system --uid 10001 appuser \
    && chown -R appuser:appuser /var/data /app
ENV VV_DATA_DIR=/var/data
USER appuser
EXPOSE 8080
CMD ["/app/visa_vista_api"]
