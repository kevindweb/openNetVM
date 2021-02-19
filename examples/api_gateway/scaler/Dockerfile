# https://docs.docker.com/develop/develop-images/multistage-build/
# multi-stage builds to reduce container size
FROM sdnfv/openNetVM:latest as builder
WORKDIR /container/
COPY . .
RUN make clean && make

FROM alpine:latest
WORKDIR /container/
COPY --from=builder /container/cont_nf/build/app/cont_nf .
CMD ["./cont_nf"]