FROM alpine:latest
WORKDIR /app
COPY . .
RUN apk add --update make gcc libc-dev
RUN make
VOLUME /world
CMD ./netmud small.db /world/netmud.dump 4201 /world/netmud.log
