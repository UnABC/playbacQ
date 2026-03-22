# FROM gcc:15.2 AS builder
# ENV DEBIAN_FRONTEND=noninteractive
# RUN apt-get update && apt-get install -y \
# 	git \
# 	cmake \
# 	uuid-dev \
# 	zlib1g-dev \
# 	libssl-dev \
# 	libjsoncpp-dev \
# 	libzstd-dev \
# 	libhiredis-dev \
# 	libboost-dev \
# 	libboost-system-dev \
# 	libboost-filesystem-dev \
# 	default-libmysqlclient-dev \
# 	libcurl4-openssl-dev \
# 	ffmpeg \
# 	&& rm -rf /var/lib/apt/lists/*
# RUN git clone https://github.com/sewenew/redis-plus-plus.git \
# 	&& cd redis-plus-plus \
# 	&& mkdir build && cd build \
# 	&& cmake .. && make -j$(nproc) && make install
# RUN git clone https://github.com/drogonframework/drogon \
# 	&& cd drogon \
# 	&& git submodule update --init \
# 	&& mkdir build && cd build && cmake .. && make -j$(nproc) && make install
# RUN git clone --recurse-submodules --depth 1 https://github.com/aws/aws-sdk-cpp.git \
# 	&& cd aws-sdk-cpp \
# 	&& mkdir build && cd build \
# 	&& cmake .. -DBUILD_ONLY="s3" -DBUILD_SHARED_LIBS=OFF -DENABLE_TESTING=OFF -DCMAKE_BUILD_TYPE=Release \
# 	&& make -j$(nproc) && make install

FROM unabc/playbacq-base:latest AS builder

# Register system libraries in builder
RUN ldconfig

ARG BUILD_JOBS=1

WORKDIR /app
COPY CMakeLists.txt /app/
COPY *.cpp /app/
COPY test/ /app/test/
COPY models/ /app/models/
COPY controllers/ /app/controllers/
COPY filters/ /app/filters/
COPY plugins/ /app/plugins/
COPY worker/ /app/worker/
COPY docker/ /app/docker/
COPY config.json config.yaml /app/
RUN mkdir build && cd build && cmake .. && make -j${BUILD_JOBS}

FROM gcc:15.2
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
	ca-certificates \
	libcurl4 \
	ffmpeg \
	libhiredis1.1.0 \
	&& rm -rf /var/lib/apt/lists/*

# Copy all built libraries from builder
COPY --from=builder /usr/local/lib /usr/local/lib

# Copy necessary system libraries from builder
COPY --from=builder /usr/lib/x86_64-linux-gnu/libjsoncpp.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libbrotli*.so* /usr/lib/x86_64-linux-gnu/

# Register libraries in runtime cache
RUN ldconfig

WORKDIR /app

# Copy both executables and make them available
COPY --from=builder /app/build/playbacq /usr/local/bin/playbacq
COPY --from=builder /app/build/playbacq_worker /usr/local/bin/playbacq_worker
RUN chmod +x /usr/local/bin/playbacq /usr/local/bin/playbacq_worker

EXPOSE 8080

CMD ["/usr/local/bin/playbacq"]