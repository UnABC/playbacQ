FROM gcc:15.2 AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
	git \
	cmake \
	uuid-dev \
	zlib1g-dev \
	libssl-dev \
	libjsoncpp-dev \
	libzstd-dev \
	default-libmysqlclient-dev \
	libcurl4-openssl-dev \
	&& rm -rf /var/lib/apt/lists/*
RUN git clone https://github.com/drogonframework/drogon \
	&& cd drogon \
	&& git submodule update --init \
	&& mkdir build && cd build && cmake .. && make -j$(nproc) && make install
RUN git clone --recurse-submodules --depth 1 https://github.com/aws/aws-sdk-cpp.git \
	&& cd aws-sdk-cpp \
	&& mkdir build && cd build \
	&& cmake .. -DBUILD_ONLY="s3" -DBUILD_SHARED_LIBS=OFF -DENABLE_TESTING=OFF -DCMAKE_BUILD_TYPE=Release \
	&& make -j$(nproc) && make install
WORKDIR /app
COPY CMakeLists.txt /app/
COPY src/ /app/src/
COPY test/ /app/test/
COPY models/ /app/models/
RUN mkdir build && cd build && cmake .. && make -j$(nproc)

FROM gcc:15.2
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
	ca-certificates \
	libcurl4 \
	&& rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /app/build/playbacq /app/playbacq
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /usr/lib/x86_64-linux-gnu/libjsoncpp.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libbrotli*.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libpq.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libsqlite3.so* /usr/lib/x86_64-linux-gnu/
RUN ldconfig
EXPOSE 8080
WORKDIR /app
COPY --from=builder /app/build/playbacq /usr/local/bin/playbacq
RUN chmod +x /usr/local/bin/playbacq && ldconfig
CMD ["/usr/local/bin/playbacq"]