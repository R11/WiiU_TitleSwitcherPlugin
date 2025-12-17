# Dockerfile for running unit tests
# Uses Ubuntu with GCC and Google Test

FROM ubuntu:22.04

# Install build tools and Google Test
RUN apt-get update && apt-get install -y \
    g++ \
    make \
    libgtest-dev \
    cmake \
    && rm -rf /var/lib/apt/lists/*

# Build and install Google Test (Ubuntu provides source, not binaries)
RUN cd /usr/src/gtest && \
    cmake . && \
    make && \
    cp lib/*.a /usr/lib/

WORKDIR /tests
