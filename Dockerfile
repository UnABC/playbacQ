FROM gcc:15.2 AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
	cmake \
	uuid-dev \
	zlib1g-dev \
	libssl-dev \
	libjsoncpp-dev \
	default-libmysqlclient-dev \
	&& rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY CMakeLists.txt /app/
COPY src/ /app/src/
RUN mkdir build && cd build && cmake .. && make -j$(nproc)

FROM debian:bookworm-slim AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
	libssl3 \
	libjsoncpp25 \
	libmariadb3 \
	ca-certificates \
	&& rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /app/build/ /app/
COPY --from=builder /app/config.json /app/
EXPOSE 8080
CMD ["./server"]
