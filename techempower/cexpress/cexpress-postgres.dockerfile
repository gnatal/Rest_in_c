FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential libpq-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY engine engine
COPY src src
COPY Makefile .
RUN make

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends libpq5 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /app/tfb-cexpress /usr/local/bin/tfb-cexpress
ENV CEXPRESS_DB=1
EXPOSE 8080
# Handlers no longer block on Postgres (each worker keeps many requests' queries in flight on one pipelined
# connection), so one worker per core. CEXPRESS_WORKERS overrides it for a sweep.
CMD ["sh", "-c", "CEXPRESS_WORKERS=${CEXPRESS_WORKERS:-$(nproc)} exec tfb-cexpress"]
