FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends g++ cmake make libboost-system-dev libcpp-httplib-dev libsqlite3-dev librabbitmq-dev libhiredis-dev libprotobuf-dev protobuf-compiler nlohmann-json3-dev zlib1g-dev libssl-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY backend/proto/ proto/
COPY backend/pipeline/ pipeline/
RUN cmake -S pipeline -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j2 && ctest --test-dir build --output-on-failure
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends libboost-system1.74.0 libcpp-httplib0.11 libsqlite3-0 librabbitmq4 libhiredis0.14 libprotobuf32 zlib1g libssl3 ca-certificates curl && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/build/douyin-pipeline /usr/local/bin/
RUN mkdir /data
EXPOSE 8080 8081
CMD ["douyin-pipeline"]
