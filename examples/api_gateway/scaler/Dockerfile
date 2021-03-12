# https://docs.docker.com/develop/develop-images/multistage-build/
# multi-stage builds to reduce container size
# run from openNetVM root: `sudo docker build -t <tagname>:latest .`

FROM ubuntu:20.04 as builder
WORKDIR /container/
# COPY dpdk ./dpdk
# COPY examples/api_gateway/scaler/cont_nf .
COPY cont_nf .

# Set dpdk-specific env vars
# ARG RTE_TARGET=x86_64-native-linuxapp-gcc
# ARG RTE_SDK=/container/dpdk

# Make sure ubuntu:20.04 doesn't hang
RUN DEBIAN_FRONTEND=noninteractive \
    ln -fs /usr/share/zoneinfo/America/New_York /etc/localtime

RUN apt-get update -y && \
    apt-get install -y --no-install-recommends \
        build-essential \
        gcc && \
    rm -rf /var/lib/apt/lists/*

# compile application
RUN gcc -o cont_nf cont_nf.c -static

# multi-stage part - move binary to separate container
FROM alpine:latest
WORKDIR /container/
COPY --from=builder /container/cont_nf .
CMD ["./cont_nf"]